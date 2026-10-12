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

/* q_shared1.c -- split from q_shared.c (originally one file) to work
 * around a real GCC -O3 codegen bug: compiling more than ~7 of these
 * functions together in one -O3 translation unit produced a binary that
 * hung on real hardware, hardware-confirmed via bisection (2026-08-23).
 * Splitting into several smaller files avoids it; see
 * project_minigl_v3d memory for the investigation. */

float Com_Clamp( float min, float max, float value )
{
	if ( value < min )
	{
		return min;
	}

	if ( value > max )
	{
		return max;
	}

	return value;
}


char *COM_SkipPath (char *pathname)
{
	char	*last;
	
	last = pathname;

	while (*pathname)
	{
		if (*pathname=='/')
			last = pathname+1;

		pathname++;
	}

	return last;
}


const char *COM_GetExtension( const char *name )
{
	const char *dot = strrchr(name, '.'), *slash;

	if (dot && (!(slash = strrchr(name, '/')) || slash < dot))
		return dot + 1;

	else
		return "";
}


void COM_StripExtension( const char *in, char *out, int destsize )
{
	const char *dot = strrchr(in, '.'), *slash;

	if (dot && (!(slash = strrchr(in, '/')) || slash < dot))
		destsize = (destsize < dot-in+1 ? destsize : dot-in+1);

	if ( in == out && destsize > 1 )
		out[destsize-1] = '\0';

	else
		Q_strncpyz(out, in, destsize);
}


void COM_DefaultExtension( char *path, int maxSize, const char *extension )
{
	const char *dot = strrchr(path, '.'), *slash;

	if (dot && (!(slash = strrchr(path, '/')) || slash < dot))
		return;

	else
		Q_strcat(path, maxSize, extension);
}


short ShortSwap (short l)
{
	byte	b1,b2;

	b1 = l&255;
	b2 = (l>>8)&255;

	return (b1<<8) + b2;
}


short ShortNoSwap (short l)
{
	return l;
}


