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
 * name:		be_ai_char_p2.c
 *
 * desc:		bot characters -- permanent split of the original
 * be_ai_char.c, part 2 of 4 (p1a/p1b1/p1b2/p2). See be_ai_char_p1b1.c
 * for the full backstory (BotLoadCharacterFromFile, in p1b1, is the
 * one real -O3 codegen bug this split exists to isolate and pin at
 * -O0). This file: BotLoadCharacterSkill through BotShutdownCharacters
 * (the public-facing wrapper/characteristic-accessor functions).
 * Compiles at normal -O3.
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

// defined in be_ai_char_p1.c, shared across the split
extern bot_character_t *botcharacters[MAX_CLIENTS + 1];

// defined in be_ai_char_p1.c, used here
bot_character_t *BotCharacterFromHandle(int handle);
void BotDumpCharacter(bot_character_t *ch);
void BotFreeCharacter2(int handle);
void BotDefaultCharacteristics(bot_character_t *ch, bot_character_t *defaultch);
int BotFindCachedCharacter(char *charfile, float skill);
int BotLoadCachedCharacter(char *charfile, float skill, int reload);

//===========================================================================
//
// Parameter:			-
// Returns:				-
// Changes Globals:		-
//===========================================================================
int BotLoadCharacterSkill(char *charfile, float skill)
{
	int ch, defaultch;

	defaultch = BotLoadCachedCharacter(DEFAULT_CHARACTER, skill, qfalse);
	ch = BotLoadCachedCharacter(charfile, skill, LibVarGetValue("bot_reloadcharacters"));

	if (defaultch && ch)
	{
		BotDefaultCharacteristics(botcharacters[ch], botcharacters[defaultch]);
	} //end if

	return ch;
} //end of the function BotLoadCharacterSkill
//===========================================================================
//
// Parameter:			-
// Returns:				-
// Changes Globals:		-
//===========================================================================
int BotInterpolateCharacters(int handle1, int handle2, float desiredskill)
{
	bot_character_t *ch1, *ch2, *out;
	int i, handle;
	float scale;

	ch1 = BotCharacterFromHandle(handle1);
	ch2 = BotCharacterFromHandle(handle2);
	if (!ch1 || !ch2)
		return 0;
	//find a free spot for a character
	for (handle = 1; handle <= MAX_CLIENTS; handle++)
	{
		if (!botcharacters[handle]) break;
	} //end for
	if (handle > MAX_CLIENTS) return 0;
	out = (bot_character_t *) GetClearedMemory(sizeof(bot_character_t) +
					MAX_CHARACTERISTICS * sizeof(bot_characteristic_t));
	out->skill = desiredskill;
	strcpy(out->filename, ch1->filename);
	botcharacters[handle] = out;

	scale = (float) (desiredskill - ch1->skill) / (ch2->skill - ch1->skill);
	for (i = 0; i < MAX_CHARACTERISTICS; i++)
	{
		//
		if (ch1->c[i].type == CT_FLOAT && ch2->c[i].type == CT_FLOAT)
		{
			out->c[i].type = CT_FLOAT;
			out->c[i].value._float = ch1->c[i].value._float +
								(ch2->c[i].value._float - ch1->c[i].value._float) * scale;
		} //end if
		else if (ch1->c[i].type == CT_INTEGER)
		{
			out->c[i].type = CT_INTEGER;
			out->c[i].value.integer = ch1->c[i].value.integer;
		} //end else if
		else if (ch1->c[i].type == CT_STRING)
		{
			out->c[i].type = CT_STRING;
			out->c[i].value.string = (char *) GetMemory(strlen(ch1->c[i].value.string)+1);
			strcpy(out->c[i].value.string, ch1->c[i].value.string);
		} //end else if
	} //end for
	return handle;
} //end of the function BotInterpolateCharacters
//===========================================================================
//
// Parameter:			-
// Returns:				-
// Changes Globals:		-
//===========================================================================
int BotLoadCharacter(char *charfile, float skill)
{
	int firstskill, secondskill, handle;

	//make sure the skill is in the valid range
	if (skill < 1.0) skill = 1.0;
	else if (skill > 5.0) skill = 5.0;
	//skill 1, 4 and 5 should be available in the character files
	if (skill == 1.0 || skill == 4.0 || skill == 5.0)
	{
		return BotLoadCharacterSkill(charfile, skill);
	} //end if
	//check if there's a cached skill
	handle = BotFindCachedCharacter(charfile, skill);
	if (handle)
	{
		botimport.Print(PRT_MESSAGE, "loaded cached skill %f from %s\n", skill, charfile);
		return handle;
	} //end if
	if (skill < 4.0)
	{
		//load skill 1 and 4
		firstskill = BotLoadCharacterSkill(charfile, 1);
		if (!firstskill) return 0;
		secondskill = BotLoadCharacterSkill(charfile, 4);
		if (!secondskill) return firstskill;
	} //end if
	else
	{
		//load skill 4 and 5
		firstskill = BotLoadCharacterSkill(charfile, 4);
		if (!firstskill) return 0;
		secondskill = BotLoadCharacterSkill(charfile, 5);
		if (!secondskill) return firstskill;
	} //end else
	//interpolate between the two skills
	handle = BotInterpolateCharacters(firstskill, secondskill, skill);
	if (!handle) return 0;
	//write the character to the log file
	BotDumpCharacter(botcharacters[handle]);
	//
	return handle;
} //end of the function BotLoadCharacter
//===========================================================================
//
// Parameter:			-
// Returns:				-
// Changes Globals:		-
//===========================================================================
int CheckCharacteristicIndex(int character, int index)
{
	bot_character_t *ch;

	ch = BotCharacterFromHandle(character);
	if (!ch) return qfalse;
	if (index < 0 || index >= MAX_CHARACTERISTICS)
	{
		botimport.Print(PRT_ERROR, "characteristic %d does not exist\n", index);
		return qfalse;
	} //end if
	if (!ch->c[index].type)
	{
		botimport.Print(PRT_ERROR, "characteristic %d is not initialized\n", index);
		return qfalse;
	} //end if
	return qtrue;
} //end of the function CheckCharacteristicIndex
//===========================================================================
//
// Parameter:			-
// Returns:				-
// Changes Globals:		-
//===========================================================================
float Characteristic_Float(int character, int index)
{
	bot_character_t *ch;

	ch = BotCharacterFromHandle(character);
	if (!ch) return 0;
	//check if the index is in range
	if (!CheckCharacteristicIndex(character, index)) return 0;
	//an integer will be converted to a float
	if (ch->c[index].type == CT_INTEGER)
	{
		return (float) ch->c[index].value.integer;
	} //end if
	//floats are just returned
	else if (ch->c[index].type == CT_FLOAT)
	{
		return ch->c[index].value._float;
	} //end else if
	//cannot convert a string pointer to a float
	else
	{
		botimport.Print(PRT_ERROR, "characteristic %d is not a float\n", index);
		return 0;
	} //end else if
//	return 0;
} //end of the function Characteristic_Float
//===========================================================================
//
// Parameter:				-
// Returns:					-
// Changes Globals:		-
//===========================================================================
float Characteristic_BFloat(int character, int index, float min, float max)
{
	float value;
	bot_character_t *ch;

	ch = BotCharacterFromHandle(character);
	if (!ch) return 0;
	if (min > max)
	{
		botimport.Print(PRT_ERROR, "cannot bound characteristic %d between %f and %f\n", index, min, max);
		return 0;
	} //end if
	value = Characteristic_Float(character, index);
	if (value < min) return min;
	if (value > max) return max;
	return value;
} //end of the function Characteristic_BFloat
//===========================================================================
//
// Parameter:			-
// Returns:				-
// Changes Globals:		-
//===========================================================================
int Characteristic_Integer(int character, int index)
{
	bot_character_t *ch;

	ch = BotCharacterFromHandle(character);
	if (!ch) return 0;
	//check if the index is in range
	if (!CheckCharacteristicIndex(character, index)) return 0;
	//an integer will just be returned
	if (ch->c[index].type == CT_INTEGER)
	{
		return ch->c[index].value.integer;
	} //end if
	//floats are casted to integers
	else if (ch->c[index].type == CT_FLOAT)
	{
		return (int) ch->c[index].value._float;
	} //end else if
	else
	{
		botimport.Print(PRT_ERROR, "characteristic %d is not an integer\n", index);
		return 0;
	} //end else if
//	return 0;
} //end of the function Characteristic_Integer
//===========================================================================
//
// Parameter:			-
// Returns:				-
// Changes Globals:		-
//===========================================================================
int Characteristic_BInteger(int character, int index, int min, int max)
{
	int value;
	bot_character_t *ch;

	ch = BotCharacterFromHandle(character);
	if (!ch) return 0;
	if (min > max)
	{
		botimport.Print(PRT_ERROR, "cannot bound characteristic %d between %d and %d\n", index, min, max);
		return 0;
	} //end if
	value = Characteristic_Integer(character, index);
	if (value < min) return min;
	if (value > max) return max;
	return value;
} //end of the function Characteristic_BInteger
//===========================================================================
//
// Parameter:			-
// Returns:				-
// Changes Globals:		-
//===========================================================================
void Characteristic_String(int character, int index, char *buf, int size)
{
	bot_character_t *ch;

	ch = BotCharacterFromHandle(character);
	if (!ch) return;
	//check if the index is in range
	if (!CheckCharacteristicIndex(character, index)) return;
	//an integer will be converted to a float
	if (ch->c[index].type == CT_STRING)
	{
		Q_strncpyz( buf, ch->c[index].value.string, size );
	} //end if
	else
	{
		botimport.Print(PRT_ERROR, "characteristic %d is not a string\n", index);
	} //end else if
} //end of the function Characteristic_String
//===========================================================================
//
// Parameter:			-
// Returns:				-
// Changes Globals:		-
//===========================================================================
void BotShutdownCharacters(void)
{
	int handle;

	for (handle = 1; handle <= MAX_CLIENTS; handle++)
	{
		if (botcharacters[handle])
		{
			BotFreeCharacter2(handle);
		} //end if
	} //end for
} //end of the function BotShutdownCharacters

