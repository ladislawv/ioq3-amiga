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
 * name:		be_ai_char_p1b1.c
 *
 * desc:		bot characters -- permanent split of the original
 * be_ai_char.c, part 1b1 of 4 (p1a/p1b1/p1b2/p2). Contains
 * BotLoadCharacterFromFile ALONE (the real character-file parser, uses
 * l_precomp/l_script). This is the ONE function confirmed (12-round
 * hardware bisection, see project_minigl_v3d_ioq3_o3_bots_missing
 * memory) to miscompile under real GCC -O3 for m68k -- it silently
 * breaks BotAISetupClient's trap_BotLoadCharacter call, so SP bots
 * never spawn. Exact miscompiled construct not found (disassembly
 * diffing ruled out the obvious candidates -- float conversion, bounds
 * check, address arithmetic, string handling all checked equivalent);
 * pinning this file at -O0 in the build script is the practical fix
 * for the current toolchain, not a source-level root-cause fix.
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

// defined in be_ai_char_p1a.c, used here
void BotFreeCharacterStrings(bot_character_t *ch);

//===========================================================================
//
// Parameter:			-
// Returns:				-
// Changes Globals:		-
//===========================================================================
bot_character_t *BotLoadCharacterFromFile(char *charfile, int skill)
{
	int indent, index, foundcharacter;
	bot_character_t *ch;
	source_t *source;
	token_t token;

	foundcharacter = qfalse;
	//a bot character is parsed in two phases
	PC_SetBaseFolder(BOTFILESBASEFOLDER);
	source = LoadSourceFile(charfile);
	if (!source)
	{
		botimport.Print(PRT_ERROR, "counldn't load %s\n", charfile);
		return NULL;
	} //end if
	ch = (bot_character_t *) GetClearedMemory(sizeof(bot_character_t) +
					MAX_CHARACTERISTICS * sizeof(bot_characteristic_t));
	strcpy(ch->filename, charfile);
	while(PC_ReadToken(source, &token))
	{
		if (!strcmp(token.string, "skill"))
		{
			if (!PC_ExpectTokenType(source, TT_NUMBER, 0, &token))
			{
				FreeSource(source);
				BotFreeCharacterStrings(ch);
				FreeMemory(ch);
				return NULL;
			} //end if
			if (!PC_ExpectTokenString(source, "{"))
			{
				FreeSource(source);
				BotFreeCharacterStrings(ch);
				FreeMemory(ch);
				return NULL;
			} //end if
			//if it's the correct skill
			if (skill < 0 || token.intvalue == skill)
			{
				foundcharacter = qtrue;
				ch->skill = token.intvalue;
				while(PC_ExpectAnyToken(source, &token))
				{
					if (!strcmp(token.string, "}")) break;
					if (token.type != TT_NUMBER || !(token.subtype & TT_INTEGER))
					{
						SourceError(source, "expected integer index, found %s", token.string);
						FreeSource(source);
						BotFreeCharacterStrings(ch);
						FreeMemory(ch);
						return NULL;
					} //end if
					index = token.intvalue;
					if (index < 0 || index > MAX_CHARACTERISTICS)
					{
						SourceError(source, "characteristic index out of range [0, %d]", MAX_CHARACTERISTICS);
						FreeSource(source);
						BotFreeCharacterStrings(ch);
						FreeMemory(ch);
						return NULL;
					} //end if
					if (ch->c[index].type)
					{
						SourceError(source, "characteristic %d already initialized", index);
						FreeSource(source);
						BotFreeCharacterStrings(ch);
						FreeMemory(ch);
						return NULL;
					} //end if
					if (!PC_ExpectAnyToken(source, &token))
					{
						FreeSource(source);
						BotFreeCharacterStrings(ch);
						FreeMemory(ch);
						return NULL;
					} //end if
					if (token.type == TT_NUMBER)
					{
						if (token.subtype & TT_FLOAT)
						{
							ch->c[index].value._float = token.floatvalue;
							ch->c[index].type = CT_FLOAT;
						} //end if
						else
						{
							ch->c[index].value.integer = token.intvalue;
							ch->c[index].type = CT_INTEGER;
						} //end else
					} //end if
					else if (token.type == TT_STRING)
					{
						StripDoubleQuotes(token.string);
						ch->c[index].value.string = GetMemory(strlen(token.string)+1);
						strcpy(ch->c[index].value.string, token.string);
						ch->c[index].type = CT_STRING;
					} //end else if
					else
					{
						SourceError(source, "expected integer, float or string, found %s", token.string);
						FreeSource(source);
						BotFreeCharacterStrings(ch);
						FreeMemory(ch);
						return NULL;
					} //end else
				} //end if
				break;
			} //end if
			else
			{
				indent = 1;
				while(indent)
				{
					if (!PC_ExpectAnyToken(source, &token))
					{
						FreeSource(source);
						BotFreeCharacterStrings(ch);
						FreeMemory(ch);
						return NULL;
					} //end if
					if (!strcmp(token.string, "{")) indent++;
					else if (!strcmp(token.string, "}")) indent--;
				} //end while
			} //end else
		} //end if
		else
		{
			SourceError(source, "unknown definition %s", token.string);
			FreeSource(source);
			BotFreeCharacterStrings(ch);
			FreeMemory(ch);
			return NULL;
		} //end else
	} //end while
	FreeSource(source);
	//
	if (!foundcharacter)
	{
		BotFreeCharacterStrings(ch);
		FreeMemory(ch);
		return NULL;
	} //end if
	return ch;
} //end of the function BotLoadCharacterFromFile
