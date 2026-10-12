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
 * name:		be_ai_char_p1a.c
 *
 * desc:		bot characters -- permanent split of the original
 * be_ai_char.c, part 1a of 4 (p1a/p1b1/p1b2/p2). BotLoadCharacterFromFile
 * (be_ai_char_p1b1.c) is a confirmed real GCC -O3 codegen bug for m68k
 * (see project_minigl_v3d_ioq3_o3_bots_missing memory) that silently
 * breaks SP bot spawning; it's pinned at -O0 while everything else here
 * stays -O3. This file: BotCharacterFromHandle through
 * BotDefaultCharacteristics (small housekeeping/free functions), plus
 * the botcharacters[] array definition.
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

bot_character_t *botcharacters[MAX_CLIENTS + 1];

//========================================================================
//
// Parameter:			-
// Returns:				-
// Changes Globals:		-
//========================================================================
bot_character_t *BotCharacterFromHandle(int handle)
{
	if (handle <= 0 || handle > MAX_CLIENTS)
	{
		botimport.Print(PRT_FATAL, "character handle %d out of range\n", handle);
		return NULL;
	} //end if
	if (!botcharacters[handle])
	{
		botimport.Print(PRT_FATAL, "invalid character %d\n", handle);
		return NULL;
	} //end if
	return botcharacters[handle];
} //end of the function BotCharacterFromHandle
//===========================================================================
//
// Parameter:			-
// Returns:				-
// Changes Globals:		-
//===========================================================================
void BotDumpCharacter(bot_character_t *ch)
{
	int i;

	Log_Write("%s\n", ch->filename);
	Log_Write("skill %.1f\n", ch->skill);
	Log_Write("{\n");
	for (i = 0; i < MAX_CHARACTERISTICS; i++)
	{
		switch(ch->c[i].type)
		{
			case CT_INTEGER: Log_Write(" %4d %d\n", i, ch->c[i].value.integer); break;
			case CT_FLOAT: Log_Write(" %4d %f\n", i, ch->c[i].value._float); break;
			case CT_STRING: Log_Write(" %4d %s\n", i, ch->c[i].value.string); break;
		} //end case
	} //end for
	Log_Write("}\n");
} //end of the function BotDumpCharacter
//========================================================================
//
// Parameter:			-
// Returns:				-
// Changes Globals:		-
//========================================================================
void BotFreeCharacterStrings(bot_character_t *ch)
{
	int i;

	for (i = 0; i < MAX_CHARACTERISTICS; i++)
	{
		if (ch->c[i].type == CT_STRING)
		{
			FreeMemory(ch->c[i].value.string);
		} //end if
	} //end for
} //end of the function BotFreeCharacterStrings
//========================================================================
//
// Parameter:			-
// Returns:				-
// Changes Globals:		-
//========================================================================
void BotFreeCharacter2(int handle)
{
	if (handle <= 0 || handle > MAX_CLIENTS)
	{
		botimport.Print(PRT_FATAL, "character handle %d out of range\n", handle);
		return;
	} //end if
	if (!botcharacters[handle])
	{
		botimport.Print(PRT_FATAL, "invalid character %d\n", handle);
		return;
	} //end if
	BotFreeCharacterStrings(botcharacters[handle]);
	FreeMemory(botcharacters[handle]);
	botcharacters[handle] = NULL;
} //end of the function BotFreeCharacter2
//========================================================================
//
// Parameter:			-
// Returns:				-
// Changes Globals:		-
//========================================================================
void BotFreeCharacter(int handle)
{
	if (!LibVarGetValue("bot_reloadcharacters")) return;
	BotFreeCharacter2(handle);
} //end of the function BotFreeCharacter
//===========================================================================
//
// Parameter:			-
// Returns:				-
// Changes Globals:		-
//===========================================================================
void BotDefaultCharacteristics(bot_character_t *ch, bot_character_t *defaultch)
{
	int i;

	for (i = 0; i < MAX_CHARACTERISTICS; i++)
	{
		if (ch->c[i].type) continue;
		//
		if (defaultch->c[i].type == CT_FLOAT)
		{
			ch->c[i].type = CT_FLOAT;
			ch->c[i].value._float = defaultch->c[i].value._float;
		} //end if
		else if (defaultch->c[i].type == CT_INTEGER)
		{
			ch->c[i].type = CT_INTEGER;
			ch->c[i].value.integer = defaultch->c[i].value.integer;
		} //end else if
		else if (defaultch->c[i].type == CT_STRING)
		{
			ch->c[i].type = CT_STRING;
			ch->c[i].value.string = (char *) GetMemory(strlen(defaultch->c[i].value.string)+1);
			strcpy(ch->c[i].value.string, defaultch->c[i].value.string);
		} //end else if
	} //end for
} //end of the function BotDefaultCharacteristics
