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

/* q_shared3.c -- split from q_shared.c (originally one file) to work
 * around a real GCC -O3 codegen bug: compiling more than ~7 of these
 * functions together in one -O3 translation unit produced a binary that
 * hung on real hardware, hardware-confirmed via bisection (2026-08-23).
 * Splitting into several smaller files avoids it; see
 * project_minigl_v3d memory for the investigation. */

static	char	com_token[MAX_TOKEN_CHARS];
static	char	com_parsename[MAX_TOKEN_CHARS];
static	int	com_lines;
static	int	com_tokenline;


static char *SkipWhitespace( char *data, qboolean *hasNewLines )
{
	int c;

	while( (c = *data) <= ' ')
	{
		if( !c )
		{
			return NULL;
		}

		if( c == '\n' )
		{
			com_lines++;
			*hasNewLines = qtrue;
		}

		data++;
	}

	return data;
}


int COM_GetCurrentParseLine( void )
{
	if ( com_tokenline )
		return com_tokenline;

	return com_lines;
}


char *COM_Parse( char **data_p )
{
	return COM_ParseExt( data_p, qtrue );
}


void COM_ParseError( char *format, ... )
{
	va_list argptr;
	static char string[4096];

	va_start (argptr, format);
	Q_vsnprintf (string, sizeof(string), format, argptr);
	va_end (argptr);

	Com_Printf("ERROR: %s, line %d: %s\n", com_parsename, COM_GetCurrentParseLine(), string);
}


void COM_ParseWarning( char *format, ... )
{
	va_list argptr;
	static char string[4096];

	va_start (argptr, format);
	Q_vsnprintf (string, sizeof(string), format, argptr);
	va_end (argptr);

	Com_Printf("WARNING: %s, line %d: %s\n", com_parsename, COM_GetCurrentParseLine(), string);
}


int COM_Compress( char *data_p )
{
	char		*in, *out;
	int		c;
	qboolean	newline = qfalse, whitespace = qfalse;

	in = out = data_p;

	if (in)
	{
		while ((c = *in) != 0)
		{
			// skip double slash comments
			if ( c == '/' && in[1] == '/' )
			{
				while (*in && *in != '\n')
				{
					in++;
				}
			}

			// skip /* */ comments
			else if ( c == '/' && in[1] == '*' )
			{
				while ( *in && ( *in != '*' || in[1] != '/' ) ) 
					in++;

				if ( *in ) 
					in += 2;
			}

			// record when we hit a newline
			else if ( c == '\n' || c == '\r' )
			{
				newline = qtrue;
				in++;
			}

			// record when we hit whitespace
			else if ( c == ' ' || c == '\t')
			{
				whitespace = qtrue;
				in++;
			} 

			// an actual token
			else
			{
				// if we have a pending newline, emit it (and it counts as whitespace)
				if (newline)
				{
					*out++ = '\n';
					newline = qfalse;
					whitespace = qfalse;
				}

				if (whitespace)
				{
					*out++ = ' ';
					whitespace = qfalse;
				}

				// copy quoted strings unmolested
				if (c == '"')
				{
					*out++ = c;
					in++;

					while (1)
					{
						c = *in;

						if (c && c != '"')
						{
							*out++ = c;
							in++;
						}

						else
						{
							break;
						}
					}

					if (c == '"')
					{
						*out++ = c;
						in++;
					}
				}

				else
				{
					*out = c;
					out++;
					in++;
				}
			}
		}

		*out = 0;
	}

	return out - data_p;
}


char *COM_ParseExt( char **data_p, qboolean allowLineBreaks )
{
	int c = 0, len;
	qboolean hasNewLines = qfalse;
	char *data;

	data = *data_p;
	len = 0;
	com_token[0] = '\0';
	com_tokenline = 0;

	// make sure incoming data is valid
	if ( !data )
	{
		*data_p = NULL;
		return com_token;
	}

	while ( 1 )
	{
		// skip whitespace
		data = SkipWhitespace( data, &hasNewLines );

		if ( !data )
		{
			*data_p = NULL;
			return com_token;
		}

		if ( hasNewLines && !allowLineBreaks )
		{
			*data_p = data;
			return com_token;
		}

		c = *data;

		// skip double slash comments
		if ( c == '/' && data[1] == '/' )
		{
			data += 2;

			while (*data && *data != '\n')
			{
				data++;
			}
		}

		// skip /* */ comments
		else if ( c == '/' && data[1] == '*' ) 
		{
			data += 2;

			while ( *data && ( *data != '*' || data[1] != '/' ) ) 
			{
				if ( *data == '\n' )
				{
					com_lines++;
				}

				data++;
			}

			if ( *data ) 
			{
				data += 2;
			}
		}

		else
		{
			break;
		}
	}

	// token starts on this line
	com_tokenline = com_lines;

	// handle quoted strings
	if ( c == '"' )
	{
		data++;

		while (1)
		{
			c = *data++;

			if ( c == '"' || !c )
			{
				com_token[len] = '\0';
				*data_p = ( char * ) data;
				return com_token;
			}

			if ( c == '\n' )
			{
				com_lines++;
			}

			if ( len < MAX_TOKEN_CHARS - 1 )
			{
				com_token[len] = c;
				len++;
			}
		}
	}

	// parse a regular word
	do
	{
		if ( len < MAX_TOKEN_CHARS - 1 )
		{
			com_token[len] = c;
			len++;
		}

		data++;
		c = *data;

	} while ( c > 32 );

	com_token[len] = '\0';

	*data_p = ( char * ) data;
	return com_token;
}


void COM_MatchToken( char **buf_p, char *match )
{
	char	*token;

	token = COM_Parse( buf_p );

	if ( strcmp( token, match ) )
	{
		Com_Error( ERR_DROP, "MatchToken: %s != %s", token, match );
	}
}


