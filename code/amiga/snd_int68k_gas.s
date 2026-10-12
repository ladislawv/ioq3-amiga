|
| Audio Interrupt routine
|
| setup in amiga_snd.c (ioq3 port of glquake's snd_paula.c)
| written by Frank Wille <frank@phoenix.owl.de>
| modified by Matt Hey <matthey7@gmail.com>
|
| Copied verbatim from D:\v3d_driver\glquake\snd_int68k_gas.s (the
| byte-verified GAS/GCC port of snd_int68k.s) for the ioq3 Paula sound
| driver -- do not edit without re-checking against that original.
|
| Constants verified against the real NDK_39 headers:
|   D:\NDK_39\Include\include_i\devices\timer.i:
|     TV_SECS  = 0  (ULONG)
|     TV_MICRO = 4  (ULONG)
|     TV_SIZE  = 8
|   D:\NDK_39\Include\include_i\hardware\custom.i:
|     intreqr = 0x01e
|     intreq  = 0x09c
|   D:\NDK_39\Include\include_i\hardware\intbits.i:
|     INTF_AUD0 = 1<<7  = 0x080
|     INTF_AUD1 = 1<<8  = 0x100
|     INTF_AUD2 = 1<<9  = 0x200
|     INTF_AUD3 = 1<<10 = 0x400
|     combined  = 0x780
| GetSysTime is a local, self-contained LVO offset (not from an NDK
| header) -- exec.library base call, see the header comment below.

GetSysTime = -66

| See exec.library SetIntVector()
| d0,d1,a0,a1,a5,a6 may be trashed
| d1 = INTENAR & INTREQR (enabled & requested interrupts)
| a0 = -> Amiga custom chip base
| a1 = ->IntData (is_Data of the interrupt structure)
| a6 = ->SysBase (exec.library base)
| IntData offset 0:	*TimerBase
|         offset 4:	*FirstTime2
|         offset 8:	*aud_start_time

	.text
	.balign	4

	.globl	_AudioIntCode
	.globl	AudioIntCode
_AudioIntCode:
AudioIntCode:
	fsave	-(sp)
	fmove.x	fp0,-(sp)
	move.w	(0x01e,a0),d0
	move.l	a1,a5
	and.w	#0x780,d0
	move.l	(a5)+,a6		| TimerBase
	move.w	d0,(0x09c,a0)		| clear audio requests
	lea	(TVstruct,pc),a0
	jsr	(GetSysTime,a6)		| a0 is preserved!
	move.l	(a5)+,a1		| ->FirstTime2
	move.l	(a0)+,d0		| TV_SECS
	fmove.l	(a0),fp0		| TV_MICRO
	move.l	(a5),a5			| ->aud_start_time
	fmul.d	#0f0.000001,fp0		| TV_MICRO/1000000
	sub.l	(a1),d0			| TV_SECS - FirstTime2
	fadd.l	d0,fp0
	fmove.d	fp0,(a5)
	fmove.x	(sp)+,fp0
	frestore (sp)+
	rts


	.balign	8
TVstruct:
	.skip	8,0
