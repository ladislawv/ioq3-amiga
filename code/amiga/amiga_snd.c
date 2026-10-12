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
** amiga_snd.c -- Paula sound driver (2026-08-18) and the sound backend
** dispatcher (2026-10-08)
**
** AHI lives in amiga_snd_ahi.c and is the default backend since v295
** (hardware-confirmed through Paula and a USB sound card); this file's
** Paula code is the fallback. In 2026-08 AHI was parked as the suspect
** for a startup hang inside S_Init; the reworked backend has not shown it.
**
** Ported from D:\v3d_driver\glquake\snd_paula.c (Frank Wille, modified
** by Matt Hey), glquake's real default sound path (AHI there is
** opt-in via -ahi, Paula is the proven default) -- including the
** hardware-confirmed `volatile` fix on aud_start_time (see
** project_minigl_v3d_aud_start_time_volatile_bug.md): without it, GCC
** is free to hoist the busy-wait's read out of the loop and spin
** forever regardless of whether the interrupt ever fires.
*/

#include "../client/client.h"
#include "../client/snd_local.h"

#undef NULL

#pragma pack(push,2)

#include <exec/exec.h>
#include <exec/memory.h>
#include <exec/tasks.h>
#include <exec/interrupts.h>
#include <exec/libraries.h>
#include <devices/audio.h>
#include <devices/timer.h>
#include <hardware/custom.h>
#include <hardware/cia.h>
#include <hardware/intbits.h>
#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/timer.h>
#include <clib/alib_protos.h>
#include <graphics/gfxbase.h>

#pragma pack(pop)

#define NSAMPLES 0x8000

#define CLOCK_PAL	3546895
#define CLOCK_NTSC	3579545

/* custom chip base address */
static volatile struct Custom *custom = (struct Custom *)0xdff000;

/* CIA A base address */
static volatile struct CIA *cia_a = (struct CIA *)0xbfe001;

static UBYTE *dmabuf = NULL;
static UWORD period;
static BYTE audio_dev = -1;
static float speed;
static struct MsgPort *audioport1 = NULL, *audioport2 = NULL;
static struct IOAudio *audio1 = NULL, *audio2 = NULL;
static struct Interrupt AudioInt;
static struct Interrupt *OldInt = NULL;
static UWORD OldINTENA = 0;
static UWORD aud_intflag;
static UBYTE saved_filter;

static long sysclock = CLOCK_PAL;

/* GetSysTime()'s direct-LVO call needs a real, properly-opened
   timer.device base in a global named exactly TimerBase (confirmed by
   linking: an undefined-reference error, not a FindName-returns-NULL
   runtime issue -- nothing else in this program exports that symbol
   with external linkage, amiga_shared.c's own copy is file-static).
   Opened here the same way glquake's sys_amiga.c does it (real
   CreateMsgPort/CreateIORequest/OpenDevice(TIMERNAME,...) sequence,
   not just a FindName lookup of an already-resident device) since
   that's this project's own proven-on-this-hardware mechanism. */
struct Device *TimerBase = NULL;
static struct MsgPort *timerport = NULL;
static struct timerequest *timerio = NULL;
static int firstTimeForPaula = 0;

static struct
{
	struct Device	*TimerBase;
	int		*FirstTime2;

	/* volatile: the audio interrupt handler (snd_int68k_gas.s's
	   AudioIntCode) writes *aud_start_time asynchronously, invisible
	   to the compiler's own data-flow analysis -- see
	   project_minigl_v3d_aud_start_time_volatile_bug.md. Without
	   volatile, GCC is free to hoist SNDDMA_Init's busy-wait read out
	   of the loop entirely, an infinite loop regardless of the
	   interrupt. Confirmed exactly this happened in glquake under GCC;
	   applying the fix here from the start rather than re-discovering
	   it. */
	volatile double *aud_start_time;

} IntData;

cvar_t *sndbits;
cvar_t *sndspeed;
cvar_t *sndchannels;
ULONG samplepos;

extern void AudioIntCode(void);

void (*audintptr)(void) = AudioIntCode;


static double Paula_FloatTime(void)
{
	struct timeval tv;

	GetSysTime(&tv);

	return (double)(tv.tv_secs - firstTimeForPaula) + ((double)tv.tv_micro / 1000000.0);
}


static void Paula_Shutdown(void)
{
	if (OldINTENA)
	{
		cia_a->ciapra &= ~CIAF_LED;
		cia_a->ciapra |= saved_filter;
		custom->intena = aud_intflag;
		custom->intena = INTF_SETCLR | OldINTENA;
	}

	if (OldInt)
		SetIntVector(7, OldInt);

	if (audio_dev == 0)
	{
		if (!CheckIO(&audio1->ioa_Request))
		{
			AbortIO(&audio1->ioa_Request);
			WaitPort(audioport1);
			while (GetMsg(audioport1));
		}

		if (!CheckIO(&audio2->ioa_Request))
		{
			AbortIO(&audio2->ioa_Request);
			WaitPort(audioport2);
			while (GetMsg(audioport2));
		}

		CloseDevice(&audio1->ioa_Request);
	}

	if (audio2)
		FreeVec(audio2);

	if (audio1)
		DeleteIORequest(&audio1->ioa_Request);

	if (audioport2)
		DeleteMsgPort(audioport2);

	if (audioport1)
		DeleteMsgPort(audioport1);

	if (IntData.aud_start_time)
		FreeVec((void *)(IntData.aud_start_time));

	if (dmabuf)
		FreeVec(dmabuf);

	if (TimerBase)
	{
		if (!CheckIO((struct IORequest *)timerio))
		{
			AbortIO((struct IORequest *)timerio);
			WaitIO((struct IORequest *)timerio);
		}

		CloseDevice((struct IORequest *)timerio);
		DeleteIORequest((struct IORequest *)timerio);
		DeleteMsgPort(timerport);
		TimerBase = NULL;
		timerio = NULL;
		timerport = NULL;
	}

	dmabuf = NULL;
	audio_dev = -1;
	OldInt = NULL;
	OldINTENA = 0;
	audioport1 = audioport2 = NULL;
	audio1 = NULL;
	audio2 = NULL;
}


static qboolean Paula_Init(int desired_speed)
{
	struct GfxBase	*GfxBase;
	static const UBYTE channelalloc[2] = { 1 | 2, 4 | 8 };
	int		channelnr;
	struct timeval	tv;

	Com_Printf("Paula sound initialisation\n");
	Com_Printf("...Frequency: %d Hz\n", desired_speed);
	Com_Printf("...Channels: 2 (fixed, Paula hardware)\n");
	Com_Printf("...Bits: 8 (fixed, Paula hardware)\n");

	if ( (GfxBase = (struct GfxBase *)OpenLibrary("graphics.library", 0L)) )
	{
		if (GfxBase->DisplayFlags & REALLY_PAL)
			sysclock = CLOCK_PAL;
		else
			sysclock = CLOCK_NTSC;
	}

	else
	{
		Com_Printf("ERROR: Failed to open graphics.library\n");
		return qfalse;
	}

	period = (UWORD)(sysclock / desired_speed);

	if ( !(dmabuf = AllocVec(NSAMPLES, MEMF_CHIP | MEMF_PUBLIC | MEMF_CLEAR)) )
	{
		Com_Printf("ERROR: Can't allocate Paula DMA buffer\n");
		return qfalse;
	}

	if ( !(IntData.aud_start_time = (double *)AllocVec(sizeof(double), MEMF_CHIP | MEMF_CLEAR)) )
	{
		Com_Printf("ERROR: Failed to allocate 8 bytes of CHIP-RAM buffer\n");
		return qfalse;
	}

	dma.buffer = (unsigned char *)dmabuf;
	dma.channels = 2;
	dma.speed = sysclock / (long)period;
	dma.samplebits = 8;
	dma.samples = NSAMPLES;
	dma.submission_chunk = 1;
	dma.splitchannels = 1;	/* audio2 plays dmabuf (right), audio1 plays dmabuf + NSAMPLES/2 (left) */
	samplepos = 0;
	speed = (float)(dma.speed * dma.channels);

	/* open audio.device */
	if ( (audioport1 = CreateMsgPort()) )
	{
		if ( (audioport2 = CreateMsgPort()) )
		{
			if ( (audio1 = (struct IOAudio *)CreateIORequest(audioport1, sizeof(struct IOAudio))) )
			{
				if ( (audio2 = (struct IOAudio *)AllocVec(sizeof(struct IOAudio), MEMF_PUBLIC)) )
				{
					audio1->ioa_Request.io_Message.mn_Node.ln_Pri = ADALLOC_MAXPREC;
					audio1->ioa_Request.io_Command = ADCMD_ALLOCATE;
					audio1->ioa_Request.io_Flags = ADIOF_NOWAIT;
					audio1->ioa_AllocKey = 0;
					audio1->ioa_Data = (UBYTE *)channelalloc;
					audio1->ioa_Length = sizeof(channelalloc);
					audio_dev = OpenDevice(AUDIONAME, 0, &audio1->ioa_Request, 0);
				}
			}
		}
	}

	if (audio_dev != 0)
	{
		Com_Printf("ERROR: Couldn't open audio.device\n");
		return qfalse;
	}

	/* set up audio io blocks */
	audio1->ioa_Request.io_Command = CMD_WRITE;
	audio1->ioa_Request.io_Flags = ADIOF_PERVOL;
	audio1->ioa_Data = dmabuf + (NSAMPLES >> 1);
	audio1->ioa_Length = NSAMPLES >> 1;
	audio1->ioa_Period = period;
	audio1->ioa_Volume = 64;
	audio1->ioa_Cycles = 0;  /* loop forever */
	*audio2 = *audio1;
	audio2->ioa_Request.io_Message.mn_ReplyPort = audioport2;
	audio1->ioa_Request.io_Unit = (struct Unit *) ((ULONG)audio1->ioa_Request.io_Unit & 9);
	audio2->ioa_Request.io_Unit = (struct Unit *) ((ULONG)audio2->ioa_Request.io_Unit & 6);
	audio2->ioa_Data = dmabuf;

	/* open timer.device (real open, not a FindName lookup -- see the
	   TimerBase comment above) */
	if (!TimerBase)
	{
		if ( (timerport = CreateMsgPort()) )
		{
			if ( (timerio = (struct timerequest *)CreateIORequest(timerport, sizeof(struct timerequest))) )
			{
				if (OpenDevice(TIMERNAME, UNIT_MICROHZ, (struct IORequest *)timerio, 0) == 0)
				{
					TimerBase = (struct Device *)timerio->tr_node.io_Device;
				}

				else
				{
					DeleteIORequest((struct IORequest *)timerio);
					timerio = NULL;
					DeleteMsgPort(timerport);
					timerport = NULL;
				}
			}

			else
			{
				DeleteMsgPort(timerport);
				timerport = NULL;
			}
		}

		if (!TimerBase)
		{
			Com_Printf("ERROR: Can't open timer.device\n");
			return qfalse;
		}
	}

	/* set up audio interrupt */
	IntData.TimerBase = TimerBase;
	IntData.FirstTime2 = &firstTimeForPaula;

	GetSysTime(&tv);
	firstTimeForPaula = tv.tv_secs;

	AudioInt.is_Node.ln_Type = NT_INTERRUPT;
	AudioInt.is_Node.ln_Pri = 0;
	AudioInt.is_Data = &IntData;
	AudioInt.is_Code = (void (*)())audintptr;

	/* start replay of left and right channel */
	switch ((ULONG)audio1->ioa_Request.io_Unit)
	{
		case 1: channelnr = 0; break;
		case 2: channelnr = 1; break;
		case 4: channelnr = 2; break;
		case 8: channelnr = 3; break;
		default: channelnr = 0; break;
	}

	BeginIO(&audio1->ioa_Request);
	BeginIO(&audio2->ioa_Request);

	OldInt = SetIntVector(channelnr + 7, &AudioInt);
	OldINTENA = custom->intenar;
	*IntData.aud_start_time = 0;
	aud_intflag = INTF_AUD0 << channelnr;
	custom->intena = OldINTENA | INTF_SETCLR | INTF_INTEN | aud_intflag;

	while ( !(*IntData.aud_start_time) );

	saved_filter = cia_a->ciapra & CIAF_LED;
	cia_a->ciapra |= CIAF_LED;

	Com_Printf("Paula audio initialized\n");
	Com_Printf("Output: 8bit stereo\n");

	return qtrue;
}


static int Paula_GetDMAPos(void)
{
	int pos;

	pos = (int)((Paula_FloatTime() - *IntData.aud_start_time) * speed);

	if (pos >= NSAMPLES)
		pos = 0;

	return pos;
}


/*
** Backend dispatcher (2026-10-08). s_driver "ahi" (default) tries AHI
** first and falls back to Paula if AHI fails to initialise; s_driver
** "paula" skips AHI. Takes effect on snd_restart. A hang inside AHI init
** cannot fall back -- start with +set s_driver paula, then set s_ahilog 1
** and PROGDIR:ahi_init.log shows the last AHI step reached.
*/

extern qboolean AHISnd_Init(int speed);
extern void AHISnd_Shutdown(void);
extern int AHISnd_GetDMAPos(void);

static cvar_t *s_driver;
static enum { SND_NONE, SND_PAULA, SND_AHI } snd_backend = SND_NONE;


qboolean SNDDMA_Init(void)
{
	int speed;

	s_driver = Cvar_Get("s_driver", "ahi", CVAR_ARCHIVE);
	sndbits = Cvar_Get("sndbits", "16", CVAR_ARCHIVE);	/* informational: Paula is 8 bit, AHI 16 bit */
	sndspeed = Cvar_Get("sndspeed", "22050", CVAR_ARCHIVE);
	sndchannels = Cvar_Get("sndchannels", "2", CVAR_ARCHIVE);	/* informational: both backends are stereo */

	switch ((int)sndspeed->value)
	{
		case 48000:
		case 44100:
		case 22050:
		case 11025:
			speed = (int)sndspeed->value;
			break;

		default:
			speed = 11025;
			break;
	}

	samplepos = 0;

	if (!Q_stricmp(s_driver->string, "ahi"))
	{
		if (AHISnd_Init(speed))
		{
			snd_backend = SND_AHI;
			return qtrue;
		}

		Com_Printf("AHI failed, falling back to Paula\n");
	}

	if (Paula_Init(speed))
	{
		snd_backend = SND_PAULA;
		return qtrue;
	}

	Paula_Shutdown();
	snd_backend = SND_NONE;
	return qfalse;
}


void SNDDMA_Shutdown(void)
{
	if (snd_backend == SND_AHI)
		AHISnd_Shutdown();

	else if (snd_backend == SND_PAULA)
		Paula_Shutdown();

	snd_backend = SND_NONE;
}


int SNDDMA_GetDMAPos(void)
{
	if (snd_backend == SND_AHI)
		samplepos = AHISnd_GetDMAPos();

	else if (snd_backend == SND_PAULA)
		samplepos = Paula_GetDMAPos();

	return samplepos;
}


void SNDDMA_BeginPainting(void)
{
}


void SNDDMA_Submit(void)
{
}
