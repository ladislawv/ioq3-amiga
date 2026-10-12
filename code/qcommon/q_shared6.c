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

/* q_shared6.c -- split from q_shared.c (originally one file) to work
 * around a real GCC -O3 codegen bug: compiling more than ~7 of these
 * functions together in one -O3 translation unit produced a binary that
 * hung on real hardware, hardware-confirmed via bisection (2026-08-23).
 * Splitting into several smaller files avoids it; see
 * project_minigl_v3d memory for the investigation. */

int Q_strncmp (const char *s1, const char *s2, int n)
{
	int	c1, c2;
	
	do
	{
		c1 = *s1++;
		c2 = *s2++;

		if (!n--)
		{
			return 0;	// strings are equal until end point
		}
		
		if (c1 != c2)
		{
			return c1 < c2 ? -1 : 1;
		}

	} while (c1);
	
	return 0;	// strings are equal
}


int Q_stricmp ( const char *s1, const char *s2 )
{
	unsigned char	c1, c2;

	if ( s1 == NULL )
	{
		if ( s2 == NULL )
			return 0;

		else
			return -1;
	}

	else if ( s2 == NULL )
		return 1;

	do
	{
		c1 = *s1++;
		c2 = *s2++;

		if (c1 != c2)
		{
			if (c1 <= 'Z' && c1 >= 'A' )
			{
				c1 += ('a' - 'A');
			}

			if (c2 <= 'Z' && c2 >= 'A')
			{
				c2 += ('a' - 'A');
			}

			if (c1 != c2)
			{
				return c1 < c2 ? -1 : 1;
			}
		}

	} while ( c1 != '\0' );

	return 0;
}


char *Q_strlwr( char *s1 )
{
	char	*s;

	s = s1;

	while ( *s )
	{
		#if defined (AMIGAOS) || defined (__GCC__)
		*s = tolower((unsigned char)*s);
		#else
		*s = tolower(*s);
		#endif

		s++;
	}

	return s1;
}


char *Q_strupr( char *s1 )
{
	char	*s;

	s = s1;

	while ( *s )
	{
		#if defined (AMIGAOS) || defined (__GCC__)
		*s = toupper((unsigned char)*s);
		#else
		*s = toupper(*s);
		#endif

		s++;
	}

	return s1;
}


void Q_strcat( char *dest, int size, const char *src )
{
	int	l1;

	l1 = strlen( dest );

	if ( l1 >= size )
	{
		Com_Error( ERR_FATAL, "Q_strcat: already overflowed" );
	}

	Q_strncpyz( dest + l1, src, size - l1 );
}


const char *Q_stristr( const char *s, const char *find )
{
	char	c, sc;
	size_t	len;

	if ( (c = *find++) != 0 )
	{
		if (c >= 'a' && c <= 'z')
		{
			c -= ('a' - 'A');
		}
    
		len = strlen(find);

		do
		{
			do
			{
				if ((sc = *s++) == 0)
					return NULL;

				if (sc >= 'a' && sc <= 'z')
				{
					sc -= ('a' - 'A');
				}

			} while (sc != c);

		} while (Q_stricmpn(s, find, len) != 0);

		s--;
	}

	return s;
}


int Q_PrintStrlen( const char *string )
{
	int		len;
	const char	*p;

	if( !string )
	{
		return 0;
	}

	len = 0;
	p = string;

	while( *p )
	{
		if( Q_IsColorString( p ) )
		{
			p += 2;
			continue;
		}

		p++;
		len++;
	}

	return len;
}


