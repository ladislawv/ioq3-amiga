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

/*****************************************************************************
 * name:		be_ai_char_p1b2.c
 *
 * desc:		bot characters -- permanent split of the original
 * be_ai_char.c, part 1b2 of 4 (p1a/p1b1/p1b2/p2). See be_ai_char_p1b1.c
 * for the full backstory. This file: BotFindCachedCharacter,
 * BotLoadCachedCharacter -- the cache lookup wrappers around
 * BotLoadCharacterFromFile (p1b1). Compiles at normal -O3.
 *
 *****************************************************************************/

#include "../qcommon/q_shared.h"
#include "l_log.h"
#include "l_memory.h"
#include "l_utils.h"
#include "l_script.h"
#include "l_precomp.h"
#include "l_struct.h"
#include "l_libvar.h"
#include "aasfile.h"
#include "botlib.h"
#include "be_aas.h"
#include "be_aas_funcs.h"
#include "be_interface.h"
#include "be_ai_char.h"

#define MAX_CHARACTERISTICS		80

#define CT_INTEGER				1
#define CT_FLOAT				2
#define CT_STRING				3

#define DEFAULT_CHARACTER		"bots/default_c.c"

//characteristic value
union cvalue
{
	int integer;
	float _float;
	char *string;
};
//a characteristic
typedef struct bot_characteristic_s
{
	char type;						//characteristic type
	union cvalue value;				//characteristic value
} bot_characteristic_t;

//a bot character
typedef struct bot_character_s
{
	char filename[MAX_QPATH];
	float skill;
	bot_characteristic_t c[1];		//variable sized
} bot_character_t;

// defined in be_ai_char_p1a.c, shared across the split
extern bot_character_t *botcharacters[MAX_CLIENTS + 1];

// defined in be_ai_char_p1b1.c, used here
bot_character_t *BotLoadCharacterFromFile(char *charfile, int skill);

//===========================================================================
//
// Parameter:			-
// Returns:				-
// Changes Globals:		-
//===========================================================================
int BotFindCachedCharacter(char *charfile, float skill)
{
	int handle;

	for (handle = 1; handle <= MAX_CLIENTS; handle++)
	{
		if ( !botcharacters[handle] ) continue;
		if ( strcmp( botcharacters[handle]->filename, charfile ) == 0 &&
			(skill < 0 || fabs(botcharacters[handle]->skill - skill) < 0.01) )
		{
			return handle;
		} //end if
	} //end for
	return 0;
} //end of the function BotFindCachedCharacter
//===========================================================================
//
// Parameter:			-
// Returns:				-
// Changes Globals:		-
//===========================================================================
int BotLoadCachedCharacter(char *charfile, float skill, int reload)
{
	int handle, cachedhandle, intskill;
	bot_character_t *ch = NULL;
#ifdef DEBUG
	int starttime;

	starttime = Sys_MilliSeconds();
#endif //DEBUG

	//find a free spot for a character
	for (handle = 1; handle <= MAX_CLIENTS; handle++)
	{
		if (!botcharacters[handle]) break;
	} //end for
	if (handle > MAX_CLIENTS) return 0;
	//try to load a cached character with the given skill
	if (!reload)
	{
		cachedhandle = BotFindCachedCharacter(charfile, skill);
		if (cachedhandle)
		{
			botimport.Print(PRT_MESSAGE, "loaded cached skill %f from %s\n", skill, charfile);
			return cachedhandle;
		} //end if
	} //end else
	//
	intskill = (int) (skill + 0.5);
	//try to load the character with the given skill
	ch = BotLoadCharacterFromFile(charfile, intskill);
	if (ch)
	{
		botcharacters[handle] = ch;
		//
		botimport.Print(PRT_MESSAGE, "loaded skill %d from %s\n", intskill, charfile);
#ifdef DEBUG
		if (botDeveloper)
		{
			botimport.Print(PRT_MESSAGE, "skill %d loaded in %d msec from %s\n", intskill, Sys_MilliSeconds() - starttime, charfile);
		} //end if
#endif //DEBUG
		return handle;
	} //end if
	//
	botimport.Print(PRT_WARNING, "couldn't find skill %d in %s\n", intskill, charfile);
	//
	if (!reload)
	{
		//try to load a cached default character with the given skill
		cachedhandle = BotFindCachedCharacter(DEFAULT_CHARACTER, skill);
		if (cachedhandle)
		{
			botimport.Print(PRT_MESSAGE, "loaded cached default skill %d from %s\n", intskill, charfile);
			return cachedhandle;
		} //end if
	} //end if
	//try to load the default character with the given skill
	ch = BotLoadCharacterFromFile(DEFAULT_CHARACTER, intskill);
	if (ch)
	{
		botcharacters[handle] = ch;
		botimport.Print(PRT_MESSAGE, "loaded default skill %d from %s\n", intskill, charfile);
		return handle;
	} //end if
	//
	if (!reload)
	{
		//try to load a cached character with any skill
		cachedhandle = BotFindCachedCharacter(charfile, -1);
		if (cachedhandle)
		{
			botimport.Print(PRT_MESSAGE, "loaded cached skill %f from %s\n", botcharacters[cachedhandle]->skill, charfile);
			return cachedhandle;
		} //end if
	} //end if
	//try to load a character with any skill
	ch = BotLoadCharacterFromFile(charfile, -1);
	if (ch)
	{
		botcharacters[handle] = ch;
		botimport.Print(PRT_MESSAGE, "loaded skill %f from %s\n", ch->skill, charfile);
		return handle;
	} //end if
	//
	if (!reload)
	{
		//try to load a cached character with any skill
		cachedhandle = BotFindCachedCharacter(DEFAULT_CHARACTER, -1);
		if (cachedhandle)
		{
			botimport.Print(PRT_MESSAGE, "loaded cached default skill %f from %s\n", botcharacters[cachedhandle]->skill, charfile);
			return cachedhandle;
		} //end if
	} //end if
	//try to load a character with any skill
	ch = BotLoadCharacterFromFile(DEFAULT_CHARACTER, -1);
	if (ch)
	{
		botcharacters[handle] = ch;
		botimport.Print(PRT_MESSAGE, "loaded default skill %f from %s\n", ch->skill, charfile);
		return handle;
	} //end if
	//
	botimport.Print(PRT_WARNING, "couldn't load any skill from %s\n", charfile);
	//couldn't load any character
	return 0;
} //end of the function BotLoadCachedCharacter
