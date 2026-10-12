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

/* q_shared4.c -- split from q_shared.c (originally one file) to work
 * around a real GCC -O3 codegen bug: compiling more than ~7 of these
 * functions together in one -O3 translation unit produced a binary that
 * hung on real hardware, hardware-confirmed via bisection (2026-08-23).
 * Splitting into several smaller files avoids it; see
 * project_minigl_v3d memory for the investigation. */

static	char	com_token[MAX_TOKEN_CHARS];
static	char	com_parsename[MAX_TOKEN_CHARS];
static	int	com_lines;
static	int	com_tokenline;


qboolean SkipBracedSection (char **program, int depth)
{
	char	*token;

	do
	{
		token = COM_ParseExt( program, qtrue );

		if( token[1] == 0 )
		{
			if( token[0] == '{' )
			{
				depth++;
			}

			else if( token[0] == '}' )
			{
				depth--;
			}
		}

	} while( depth && *program );

	return (depth == 0);
}


void SkipRestOfLine ( char **data )
{
	char	*p;
	int	c;

	p = *data;

	if ( !*p )
		return;

	while ( (c = *p++) != 0 )
	{
		if ( c == '\n' )
		{
			com_lines++;
			break;
		}
	}

	*data = p;
}


void Parse1DMatrix (char **buf_p, int x, float *m)
{
	char	*token;
	int	i;

	COM_MatchToken( buf_p, "(" );

	for (i = 0 ; i < x ; i++)
	{
		token = COM_Parse(buf_p);
		m[i] = atof(token);
	}

	COM_MatchToken( buf_p, ")" );
}


void Parse2DMatrix (char **buf_p, int y, int x, float *m)
{
	int	i;

	COM_MatchToken( buf_p, "(" );

	for (i = 0 ; i < y ; i++)
	{
		Parse1DMatrix (buf_p, x, m + i * x);
	}

	COM_MatchToken( buf_p, ")" );
}


void Parse3DMatrix (char **buf_p, int z, int y, int x, float *m)
{
	int	i;

	COM_MatchToken( buf_p, "(" );

	for (i = 0 ; i < z ; i++)
	{
		Parse2DMatrix (buf_p, y, x, m + i * x*y);
	}

	COM_MatchToken( buf_p, ")" );
}


int Com_HexStrToInt( const char *str )
{
	if ( !str )
		return -1;

	// check for hex code
	if( str[ 0 ] == '0' && str[ 1 ] == 'x' && str[ 2 ] != '\0')
	{
		int i, n = 0, len = strlen( str );

		for( i = 2; i < len; i++ )
		{
			char digit;

			n *= 16;

			#if defined(AMIGAOS) || defined (__GCC__)
			digit = tolower( (unsigned char) str[ i ] );
			#else
			digit = tolower( str[ i ] );
			#endif

			if( digit >= '0' && digit <= '9' )
				digit -= '0';

			else if( digit >= 'a' && digit <= 'f' )
				digit = digit - 'a' + 10;

			else
				return -1;

			n += digit;
		}

		return n;
	}

	return -1;
}


int Q_isprint( int c )
{
	if ( c >= 0x20 && c <= 0x7E )
		return ( 1 );

	return ( 0 );
}


