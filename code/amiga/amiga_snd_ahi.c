/*
===========================================================================
Copyright (C) 1999-2005 Id Software, Inc.

This file is part of Quake III Arena source code.

Quake III Arena source code is free software; you can redistribute it
and/or modify it under the terms of the GNU General Public License as
published by the Free Software Foundation; either version 2 of the License,
or (at your option) any later version.

Quake III Arena source code is distributed in the hope that it will be
useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with Foobar; if not, write to the Free Software
Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
===========================================================================
*/

/*
** amiga_snd_ahi.c -- AHI sound backend (2026-10-08)
**
** Hardware-confirmed in v295, through Paula and a USB sound card.
**
** Revived from the parked amiga_snd_ahi.c.test, since deleted (AHI snd by Jarmo
** Laakkonen and Hans-Joerg Frieden, via Cowcat's Quake 2 port). Selected
** by s_driver "ahi" through the dispatcher at the end of amiga_snd.c,
** which falls back to Paula when AHISnd_Init() fails.
**
** Changes from the parked version:
**   - always 16 bit stereo (AHIST_S16S): 8 bit gains nothing under AHI,
**     and a stale archived sndbits 8 from the Paula era would otherwise
**     silently select it
**   - buffer rounded up to a power of two: snd_mix.c wraps positions
**     with (dma.samples - 1) as a mask, and the parked code's buffer
**     length (MaxPlaySamples * speed / mixfreq, used as bytes) was not
**   - every init step can be logged to PROGDIR:ahi_init.log (s_ahilog 1, default 0),
**     opened and closed per line so it survives a hang -- the parked
**     version was suspected of a startup hang inside S_Init that was
**     never localized
**   - full cleanup on every failure path, since S_Base_Init does not
**     call SNDDMA_Shutdown when SNDDMA_Init fails
**   - dma.splitchannels cleared: AHI takes interleaved stereo
*/

#include <stdio.h>

#include "../client/client.h"
#include "../client/snd_local.h"

#undef NULL

#pragma pack(push,2)

#include <exec/exec.h>
#include <devices/ahi.h>
#include <utility/hooks.h>
#include <proto/exec.h>
#include <proto/ahi.h>

#pragma pack(pop)

#ifndef NULL
#define NULL ((void *)0)
#endif

/* AHIET_CHANNELINFO hook, called by AHI with a0 = hook, a1 = the
   AHIEffChannelInfo. Stores the channel's play offset (in sample frames)
   in hook->h_Data. Hand-assembled 68k, kept from the parked version:

	movem.l	a2,-(sp)	48e7 0020
	movea.l	a0,a2		2448
	move.l	12(a1),d0	2029 000c	ahieci_Offset[0]
	move.l	d0,16(a2)	2540 0010	h_Data
	moveq	#0,d0		7000
	movea.l	(sp)+,a2	245f
	rts			4e75
*/
static const unsigned short ahi_callback[] = {
	0x48e7, 0x0020, 0x2448, 0x2029, 0x000c, 0x2540, 0x0010, 0x7000, 0x245f, 0x4e75
};

#pragma pack(push,2)

struct ChannelInfo
{
	struct AHIEffChannelInfo cinfo;
	ULONG x[1];
};

#pragma pack(pop)

#define AHI_MINBUFFERSIZE	(64 * 1024)	/* bytes: 16384 frames, ~0.74 s at 22050 Hz */

struct Library *AHIBase = NULL;
static struct MsgPort *AHImp = NULL;
static struct AHIRequest *AHIio = NULL;
static BYTE AHIDevice = -1;
static struct AHIAudioCtrl *actrl = NULL;
static ULONG sound_loaded = 0;
static ULONG effect_set = 0;
static ULONG playing = 0;
static struct ChannelInfo info;
static UBYTE *ahibuf = NULL;

static struct Hook EffHook;

static cvar_t *s_ahilog;


static void AHI_Log(const char *fmt, ...)
{
	va_list	ap;
	char	buf[256];
	FILE	*f;

	va_start(ap, fmt);
	Q_vsnprintf(buf, sizeof(buf), fmt, ap);
	va_end(ap);

	Com_Printf("%s", buf);

	if (!s_ahilog || !s_ahilog->integer)
		return;

	/* reopened per line so the file is complete even if the next call hangs */
	if ( (f = fopen("PROGDIR:ahi_init.log", "a")) )
	{
		fputs(buf, f);
		fclose(f);
	}
}


void AHISnd_Shutdown(void)
{
	if (effect_set)
	{
		info.cinfo.ahie_Effect = AHIET_CHANNELINFO | AHIET_CANCEL;
		AHI_SetEffect(&info, actrl);
		effect_set = 0;
	}

	if (playing)
	{
		AHI_ControlAudio(actrl, AHIC_Play, FALSE, TAG_END);
		playing = 0;
	}

	if (sound_loaded)
	{
		AHI_UnloadSound(0, actrl);
		sound_loaded = 0;
	}

	if (actrl)
	{
		AHI_FreeAudio(actrl);
		actrl = NULL;
	}

	if (ahibuf)
	{
		FreeVec(ahibuf);
		ahibuf = NULL;
	}

	if (AHIDevice == 0)
	{
		CloseDevice((struct IORequest *)AHIio);
		AHIDevice = -1;
		AHIBase = NULL;
	}

	if (AHIio)
	{
		DeleteIORequest((struct IORequest *)AHIio);
		AHIio = NULL;
	}

	if (AHImp)
	{
		DeleteMsgPort(AHImp);
		AHImp = NULL;
	}
}


static qboolean AHISnd_Fail(const char *msg)
{
	AHI_Log("ERROR: %s\n", msg);
	AHISnd_Shutdown();
	return qfalse;
}


qboolean AHISnd_Init(int speed)
{
	struct AHISampleInfo	sample;
	ULONG			mixfreq = 0, playsamples = 0;
	UBYTE			name[256];
	ULONG			mode = 0;
	int			frames, buflen;

	s_ahilog = Cvar_Get("s_ahilog", "0", CVAR_ARCHIVE);

	name[0] = 0;

	EffHook.h_MinNode.mln_Succ = NULL;
	EffHook.h_MinNode.mln_Pred = NULL;
	EffHook.h_Entry = (void *)ahi_callback;	/* raw 68k code, not a C function */
	EffHook.h_SubEntry = NULL;
	EffHook.h_Data = 0;

	info.cinfo.ahie_Effect = AHIET_CHANNELINFO;
	info.cinfo.ahieci_Func = &EffHook;
	info.cinfo.ahieci_Channels = 1;

	AHI_Log("AHI sound initialisation\n");
	AHI_Log("...Frequency: %d Hz, 16 bit stereo\n", speed);

	AHI_Log("ahi: CreateMsgPort\n");
	if ( !(AHImp = CreateMsgPort()) )
		return AHISnd_Fail("Can't create AHI message port");

	AHI_Log("ahi: CreateIORequest\n");
	if ( !(AHIio = (struct AHIRequest *)CreateIORequest(AHImp, sizeof(struct AHIRequest))) )
		return AHISnd_Fail("Can't create AHI io request");

	AHIio->ahir_Version = 4;

	AHI_Log("ahi: OpenDevice ahi.device v4\n");
	if ( (AHIDevice = OpenDevice(AHINAME, AHI_NO_UNIT, (struct IORequest *)AHIio, 0)) != 0 )
		return AHISnd_Fail("Can't open ahi.device version 4");

	AHIBase = (struct Library *)AHIio->ahir_Std.io_Device;

	AHI_Log("ahi: AHI_AllocAudio\n");
	if ( !(actrl = AHI_AllocAudio(AHIA_AudioID,	AHI_DEFAULT_ID,
					AHIA_MixFreq,	speed,
					AHIA_Channels,	1,
					AHIA_Sounds,	1,
					TAG_END)) )
		return AHISnd_Fail("Can't allocate AHI audio (check AHI prefs: music unit mode)");

	AHI_Log("ahi: AHI_GetAudioAttrs\n");
	AHI_GetAudioAttrs(AHI_INVALID_ID, actrl,
				AHIDB_MaxPlaySamples,	(ULONG)&playsamples,
				AHIDB_BufferLen,	sizeof(name),
				AHIDB_Name,		(ULONG)name,
				AHIDB_AudioID,		(ULONG)&mode,
				TAG_END);

	AHI_Log("ahi: AHI_ControlAudio MixFreq_Query\n");
	AHI_ControlAudio(actrl, AHIC_MixFreq_Query, (ULONG)&mixfreq, TAG_END);

	if (!mixfreq)
		mixfreq = speed;

	/* AHI needs the dynamic sample to hold at least MaxPlaySamples,
	   scaled to our playback rate; snd_mix.c needs a power of two */
	frames = (int)((double)playsamples * speed / mixfreq) + 1;
	buflen = AHI_MINBUFFERSIZE;

	while (buflen < frames * 4)
		buflen <<= 1;

	AHI_Log("ahi: mode \"%s\" (%08lx), mixfreq %lu, maxplaysamples %lu, buffer %d bytes\n",
		name, mode, mixfreq, playsamples, buflen);

	AHI_Log("ahi: AllocVec buffer\n");
	if ( !(ahibuf = AllocVec(buflen, MEMF_ANY | MEMF_PUBLIC | MEMF_CLEAR)) )
		return AHISnd_Fail("Can't allocate AHI dma buffer");

	sample.ahisi_Type = AHIST_S16S;
	sample.ahisi_Address = (APTR)ahibuf;
	sample.ahisi_Length = buflen / AHI_SampleFrameSize(AHIST_S16S);

	AHI_Log("ahi: AHI_LoadSound\n");
	if (AHI_LoadSound(0, AHIST_DYNAMICSAMPLE, &sample, actrl) != AHIE_OK)
		return AHISnd_Fail("Can't load AHI sound");

	sound_loaded = 1;

	AHI_Log("ahi: AHI_ControlAudio Play\n");
	if (AHI_ControlAudio(actrl, AHIC_Play, TRUE, TAG_END) != AHIE_OK)
		return AHISnd_Fail("Can't start AHI playback");

	playing = 1;

	AHI_Log("ahi: AHI_Play\n");
	AHI_Play(actrl,
		AHIP_BeginChannel,	0,
		AHIP_Freq,		speed,
		AHIP_Vol,		0x10000,
		AHIP_Pan,		0x8000,
		AHIP_Sound,		0,
		AHIP_EndChannel,	0,
		TAG_END);

	AHI_Log("ahi: AHI_SetEffect CHANNELINFO\n");
	if (AHI_SetEffect(&info, actrl) != AHIE_OK)
		return AHISnd_Fail("Can't install AHI channel info hook");

	effect_set = 1;

	dma.buffer = (unsigned char *)ahibuf;
	dma.channels = 2;
	dma.speed = speed;
	dma.samplebits = 16;
	dma.samples = buflen / 2;		/* mono samples */
	dma.submission_chunk = 1;
	dma.splitchannels = 0;

	AHI_Log("AHI audio initialized: %s\n", name);
	AHI_Log("AHI snd written by Jarmo Laakkonen and Hans-Joerg Frieden\n");

	return qtrue;
}


int AHISnd_GetDMAPos(void)
{
	/* h_Data = play offset in frames; the core wants mono samples */
	return (int)(ULONG)EffHook.h_Data * dma.channels;
}
