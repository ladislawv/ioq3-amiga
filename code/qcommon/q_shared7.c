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

/* q_shared7.c -- split from q_shared.c (originally one file) to work
 * around a real GCC -O3 codegen bug: compiling more than ~7 of these
 * functions together in one -O3 translation unit produced a binary that
 * hung on real hardware, hardware-confirmed via bisection (2026-08-23).
 * Splitting into several smaller files avoids it; see
 * project_minigl_v3d memory for the investigation. */

char *Q_CleanStr( char *string )
{
	char	*d;
	char	*s;
	int	c;

	s = string;
	d = string;

	while ((c = *s) != 0 )
	{
		if ( Q_IsColorString( s ) )
		{
			s++;
		}
		
		else if ( c >= 0x20 && c <= 0x7E )
		{
			*d++ = c;
		}

		s++;
	}

	*d = '\0';

	return string;
}


int Q_CountChar(const char *string, char tocount)
{
	int count;
	
	for(count = 0; *string; string++)
	{
		if(*string == tocount)
			count++;
	}
	
	return count;
}


int QDECL Com_sprintf(char *dest, int size, const char *fmt, ...)
{
	int		len;
	va_list		argptr;

	va_start (argptr,fmt);
	len = Q_vsnprintf(dest, size, fmt, argptr);
	va_end (argptr);

	if(len >= size)
		Com_Printf("Com_sprintf: Output length %d too short, require %d bytes.\n", size, len + 1);
	
	return len;
}


char *QDECL va( char *format, ... )
{
	va_list		argptr;
	static char	string[2][32000]; // in case va is called by nested functions
	static int	index = 0;
	char		*buf;

	buf = string[ index ];
	index ^= 1;

	va_start (argptr, format);
	vsprintf (buf, format, argptr);
	va_end (argptr);

	return buf;
}


void Com_TruncateLongString( char *buffer, const char *s )
{
	int length = strlen( s );

	if( length <= TRUNCATE_LENGTH )
		Q_strncpyz( buffer, s, TRUNCATE_LENGTH );

	else
	{
		Q_strncpyz( buffer, s, ( TRUNCATE_LENGTH / 2 ) - 3 );
		Q_strcat( buffer, TRUNCATE_LENGTH, " ... " );
		Q_strcat( buffer, TRUNCATE_LENGTH, s + length - ( TRUNCATE_LENGTH / 2 ) + 3 );
	}
}


char *Info_ValueForKey( const char *s, const char *key )
{
	char		pkey[BIG_INFO_KEY];
	static char	value[2][BIG_INFO_VALUE];	// use two buffers so compares
							// work without stomping on each other
	static int	valueindex = 0;
	char		*o;
	
	if ( !s || !key )
	{
		return "";
	}

	if ( strlen( s ) >= BIG_INFO_STRING )
	{
		Com_Error( ERR_DROP, "Info_ValueForKey: oversize infostring" );
	}

	valueindex ^= 1;

	if (*s == '\\')
		s++;

	while (1)
	{
		o = pkey;

		while (*s != '\\')
		{
			if (!*s)
				return "";

			*o++ = *s++;
		}

		*o = 0;
		s++;

		o = value[valueindex];

		while (*s != '\\' && *s)
		{
			*o++ = *s++;
		}

		*o = 0;

		if (!Q_stricmp (key, pkey) )
			return value[valueindex];

		if (!*s)
			break;

		s++;
	}

	return "";
}


void Info_NextPair( const char **head, char *key, char *value )
{
	char		*o;
	const char	*s;

	s = *head;

	if ( *s == '\\' )
	{
		s++;
	}

	key[0] = 0;
	value[0] = 0;

	o = key;

	while ( *s != '\\' )
	{
		if ( !*s )
		{
			*o = 0;
			*head = s;
			return;
		}

		*o++ = *s++;
	}

	*o = 0;
	s++;

	o = value;

	while ( *s != '\\' && *s )
	{
		*o++ = *s++;
	}

	*o = 0;

	*head = s;
}


