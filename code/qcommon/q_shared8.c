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

/* q_shared8.c -- split from q_shared.c (originally one file) to work
 * around a real GCC -O3 codegen bug: compiling more than ~7 of these
 * functions together in one -O3 translation unit produced a binary that
 * hung on real hardware, hardware-confirmed via bisection (2026-08-23).
 * Splitting into several smaller files avoids it; see
 * project_minigl_v3d memory for the investigation. */

static qboolean Com_CharIsOneOfCharset( char c, char *set )
{
	int i;

	for( i = 0; i < strlen( set ); i++ )
	{
		if( set[ i ] == c )
			return qtrue;
	}

	return qfalse;
}


void Info_RemoveKey( char *s, const char *key )
{
	char	*start;
	char	pkey[MAX_INFO_KEY];
	char	value[MAX_INFO_VALUE];
	char	*o;

	if ( strlen( s ) >= MAX_INFO_STRING )
	{
		Com_Error( ERR_DROP, "Info_RemoveKey: oversize infostring" );
	}

	if (strchr (key, '\\'))
	{
		return;
	}

	while (1)
	{
		start = s;

		if (*s == '\\')
			s++;

		o = pkey;

		while (*s != '\\')
		{
			if (!*s)
				return;

			*o++ = *s++;
		}

		*o = 0;
		s++;

		o = value;

		while (*s != '\\' && *s)
		{
			if (!*s)
				return;

			*o++ = *s++;
		}

		*o = 0;

		if (!strcmp (key, pkey) )
		{
			memmove(start, s, strlen(s) + 1); // remove this part
			return;
		}

		if (!*s)
			return;
	}

}


void Info_RemoveKey_Big( char *s, const char *key )
{
	char	*start;
	char	pkey[BIG_INFO_KEY];
	char	value[BIG_INFO_VALUE];
	char	*o;

	if ( strlen( s ) >= BIG_INFO_STRING )
	{
		Com_Error( ERR_DROP, "Info_RemoveKey_Big: oversize infostring" );
	}

	if (strchr (key, '\\'))
	{
		return;
	}

	while (1)
	{
		start = s;

		if (*s == '\\')
			s++;

		o = pkey;

		while (*s != '\\')
		{
			if (!*s)
				return;

			*o++ = *s++;
		}

		*o = 0;
		s++;

		o = value;

		while (*s != '\\' && *s)
		{
			if (!*s)
				return;

			*o++ = *s++;
		}

		*o = 0;

		if (!strcmp (key, pkey) )
		{
			memmove (start, s, strlen(s) + 1); // remove this part
			return;
		}

		if (!*s)
			return;
	}

}


qboolean Info_Validate( const char *s )
{
	if ( strchr( s, '\"' ) )
	{
		return qfalse;
	}

	if ( strchr( s, ';' ) )
	{
		return qfalse;
	}

	return qtrue;
}


void Info_SetValueForKey( char *s, const char *key, const char *value )
{
	char		newi[MAX_INFO_STRING];
	const char	*blacklist = "\\;\"";

	if ( strlen( s ) >= MAX_INFO_STRING )
	{
		Com_Error( ERR_DROP, "Info_SetValueForKey: oversize infostring" );
	}

	for(; *blacklist; ++blacklist)
	{
		if (strchr (key, *blacklist) || strchr (value, *blacklist))
		{
			Com_Printf (S_COLOR_YELLOW "Can't use keys or values with a '%c': %s = %s\n", *blacklist, key, value);
			return;
		}
	}
	
	Info_RemoveKey (s, key);

	if (!value || !strlen(value))
		return;

	Com_sprintf (newi, sizeof(newi), "\\%s\\%s", key, value);

	if (strlen(newi) + strlen(s) >= MAX_INFO_STRING)
	{
		Com_Printf ("Info string length exceeded\n");
		return;
	}

	strcat (newi, s);
	strcpy (s, newi);
}


void Info_SetValueForKey_Big( char *s, const char *key, const char *value )
{
	char		newi[BIG_INFO_STRING];
	const char	*blacklist = "\\;\"";

	if ( strlen( s ) >= BIG_INFO_STRING )
	{
		Com_Error( ERR_DROP, "Info_SetValueForKey: oversize infostring" );
	}

	for(; *blacklist; ++blacklist)
	{
		if (strchr (key, *blacklist) || strchr (value, *blacklist))
		{
			Com_Printf (S_COLOR_YELLOW "Can't use keys or values with a '%c': %s = %s\n", *blacklist, key, value);
			return;
		}
	}

	Info_RemoveKey_Big (s, key);

	if (!value || !strlen(value))
		return;

	Com_sprintf (newi, sizeof(newi), "\\%s\\%s", key, value);

	if (strlen(newi) + strlen(s) >= BIG_INFO_STRING)
	{
		Com_Printf ("BIG Info string length exceeded\n");
		return;
	}

	strcat (s, newi);
}


char *Com_SkipCharset( char *s, char *sep )
{
	char	*p = s;

	while( p )
	{
		if( Com_CharIsOneOfCharset( *p, sep ) )
			p++;

		else
			break;
	}

	return p;
}


char *Com_SkipTokens( char *s, int numTokens, char *sep )
{
	int	sepCount = 0;
	char	*p = s;

	while( sepCount < numTokens )
	{
		if( Com_CharIsOneOfCharset( *p++, sep ) )
		{
			sepCount++;

			while( Com_CharIsOneOfCharset( *p, sep ) )
				p++;
		}

		else if( *p == '\0' )
			break;
	}

	if( sepCount == numTokens )
		return p;

	else
		return s;
}


