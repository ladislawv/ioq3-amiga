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
along with Quake III Arena source code; if not, write to the Free Software
Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
===========================================================================
*/
//
// q_shared.c -- stateless support routines that are included in each code dll
#include "q_shared.h"

/* q_shared2.c -- split from q_shared.c (originally one file) to work
 * around a real GCC -O3 codegen bug: compiling more than ~7 of these
 * functions together in one -O3 translation unit produced a binary that
 * hung on real hardware, hardware-confirmed via bisection (2026-08-23).
 * Splitting into several smaller files avoids it; see
 * project_minigl_v3d memory for the investigation. */

static	char	com_token[MAX_TOKEN_CHARS];
static	char	com_parsename[MAX_TOKEN_CHARS];
static	int	com_lines;
static	int	com_tokenline;


int LongSwap (int l)
{
	#if 1
	byte	b1,b2,b3,b4;

	b1 = l&255;
	b2 = (l>>8)&255;
	b3 = (l>>16)&255;
	b4 = (l>>24)&255;

	return ((int)b1<<24) + ((int)b2<<16) + ((int)b3<<8) + b4;
	#endif

	//return (l<<24) | ((l<<8) & 0xff0000) | ((l>>8) & 0xff00) | (l>>24) ;
}


int LongNoSwap (int l)
{
	return l;
}


qint64 Long64Swap (qint64 ll)
{
	qint64	result;

	result.b0 = ll.b7;
	result.b1 = ll.b6;
	result.b2 = ll.b5;
	result.b3 = ll.b4;
	result.b4 = ll.b3;
	result.b5 = ll.b2;
	result.b6 = ll.b1;
	result.b7 = ll.b0;

	return result;
}


qint64 Long64NoSwap (qint64 ll)
{
	return ll;
}


#define __LittleLong(x) (((uint32_t)(x) << 24 ) | (((uint32_t)(x) & 0xff00) << 8 ) | (((uint32_t)(x) & 0x00ff0000) >> 8 ) | ((uint32_t)(x) >> 24))

float FloatSwap (const float *f)
{
	floatint_t out;

	out.f = *f;
	//out.ui = LongSwap(out.ui);
	out.ui = __LittleLong(out.ui);

	return out.f;
}


float FloatNoSwap (const float *f)
{
	return *f;
}


void COM_BeginParseSession( const char *name )
{
	com_lines = 1;
	com_tokenline = 0;
	Com_sprintf(com_parsename, sizeof(com_parsename), "%s", name);
}


