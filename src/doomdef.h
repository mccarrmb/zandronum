// Emacs style mode select	 -*- C++ -*- 
//-----------------------------------------------------------------------------
//
// $Id:$
//
// Copyright (C) 1993-1996 by id Software, Inc.
//
// This source is available for distribution and/or modification
// only under the terms of the DOOM Source Code License as
// published by id Software. All rights reserved.
//
// The source is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// FITNESS FOR A PARTICULAR PURPOSE. See the DOOM Source Code License
// for more details.
//
// DESCRIPTION:
//	Internally used data structures for virtually everything,
//	 key definitions, lots of other stuff.
//
//-----------------------------------------------------------------------------

#ifndef __DOOMDEF_H__
#define __DOOMDEF_H__

#include <stdio.h>
#include <string.h>

#include "farchive.h"

//
// Global parameters/defines.
//

// Game mode handling - identify IWAD version
//	to handle IWAD dependend animations etc.
typedef enum
{
	shareware,		// DOOM 1 shareware, E1, M9
	registered,		// DOOM 1 registered, E3, M27
	commercial,		// DOOM 2 retail, E1 M34
	// DOOM 2 german edition not handled
	retail,			// DOOM 1 retail, E4, M36
	undetermined	// Well, no IWAD found.
  
} GameMode_t;


// Mission packs - might be useful for TC stuff?
typedef enum
{
  doom, 		// DOOM 1
  doom2,		// DOOM 2
  pack_tnt, 	// TNT mission pack
  pack_plut,	// Plutonia pack
  none

} GameMission_t;


// If rangecheck is undefined, most parameter validation debugging code
// will not be compiled
#ifndef NORANGECHECKING
#ifndef RANGECHECK
#define RANGECHECK
#endif
#endif

// The maximum number of players, multiplayer/networking.
// [BC] Changed to 32.
#define MAXPLAYERS		32

// State updates, number of tics / second.
#define TICRATE 		35

// The current state of the game: whether we are
// playing, gazing at the intermission screen,
// the game final animation, or a demo. 
typedef enum
{
	GS_LEVEL,
	GS_INTERMISSION,
	GS_FINALE,
	GS_DEMOSCREEN,
	GS_FULLCONSOLE,		// [RH]	Fullscreen console
	GS_HIDECONSOLE,		// [RH] The menu just did something that should hide fs console
	GS_STARTUP,			// [RH] Console is fullscreen, and game is just starting
	GS_TITLELEVEL,		// [RH] A combination of GS_LEVEL and GS_DEMOSCREEN

	GS_FORCEWIPE = -1,
	GS_FORCEWIPEFADE = -2
} gamestate_t;

//
// Difficulty/skill settings/filters.
//

// Skill flags.
#define MTF_EASY				1
#define MTF_NORMAL				2
#define MTF_HARD				4

// Deaf monsters/do not react to sound.
#define MTF_AMBUSH				8

typedef float skill_t;

enum ESkillLevels
{
	sk_baby,
	sk_easy,
	sk_medium,
	sk_hard,
	sk_nightmare
};



#define TELEFOGHEIGHT			(gameinfo.telefogheight)

//
// DOOM keyboard definition. Everything below 0x100 matches
// a DirectInput key code.
//
#define KEY_RIGHTARROW			0xcd	// DIK_RIGHT
#define KEY_LEFTARROW			0xcb	// DIK_LEFT
#define KEY_UPARROW 			0xc8	// DIK_UP
#define KEY_DOWNARROW			0xd0	// DIK_DOWN
#define KEY_ESCAPE				0x01	// DIK_ESCAPE
#define KEY_ENTER				0x1c	// DIK_RETURN
#define KEY_SPACE				0x39	// DIK_SPACE
#define KEY_TAB 				0x0f	// DIK_TAB
#define KEY_F1					0x3b	// DIK_F1
#define KEY_F2					0x3c	// DIK_F2
#define KEY_F3					0x3d	// DIK_F3
#define KEY_F4					0x3e	// DIK_F4
#define KEY_F5					0x3f	// DIK_F5
#define KEY_F6					0x40	// DIK_F6
#define KEY_F7					0x41	// DIK_F7
#define KEY_F8					0x42	// DIK_F8
#define KEY_F9					0x43	// DIK_F9
#define KEY_F10 				0x44	// DIK_F10
#define KEY_F11 				0x57	// DIK_F11
#define KEY_F12 				0x58	// DIK_F12

#define KEY_BACKSPACE			0x0e	// DIK_BACK
#define KEY_PAUSE				0xff

#define KEY_EQUALS				0x0d	// DIK_EQUALS
#define KEY_MINUS				0x0c	// DIK_MINUS

#define KEY_LSHIFT				0x2A	// DIK_LSHIFT
#define KEY_LCTRL				0x1d	// DIK_LCONTROL
#define KEY_LALT				0x38	// DIK_LMENU

#define	KEY_RSHIFT				KEY_LSHIFT
#define KEY_RCTRL				KEY_LCTRL
#define KEY_RALT				KEY_LALT

#define KEY_INS 				0xd2	// DIK_INSERT
#define KEY_DEL 				0xd3	// DIK_DELETE
#define KEY_END 				0xcf	// DIK_END
#define KEY_HOME				0xc7	// DIK_HOME
#define KEY_PGUP				0xc9	// DIK_PRIOR
#define KEY_PGDN				0xd1	// DIK_NEXT

#define KEY_MOUSE1				0x100
#define KEY_MOUSE2				0x101
#define KEY_MOUSE3				0x102
#define KEY_MOUSE4				0x103
#define KEY_MOUSE5				0x104
#define KEY_MOUSE6				0x105
#define KEY_MOUSE7				0x106
#define KEY_MOUSE8				0x107

#define KEY_FIRSTJOYBUTTON		0x108
#define KEY_LASTJOYBUTTON		0x187
#define KEY_JOYPOV1_UP			0x188
#define KEY_JOYPOV1_RIGHT		0x189
#define KEY_JOYPOV1_DOWN		0x18a
#define KEY_JOYPOV1_LEFT		0x18b
#define KEY_JOYPOV2_UP			0x18c
#define KEY_JOYPOV3_UP			0x190
#define KEY_JOYPOV4_UP			0x194

#define KEY_MWHEELUP			0x198
#define KEY_MWHEELDOWN			0x199

#define NUM_KEYS				0x19A

#define JOYAXIS_NONE			0
#define JOYAXIS_YAW				1
#define JOYAXIS_PITCH			2
#define JOYAXIS_FORWARD			3
#define JOYAXIS_SIDE			4
#define JOYAXIS_UP				5
//#define JOYAXIS_ROLL			6		// Ha ha. No roll for you.

// [RH] dmflags bits (based on Q2's)
enum
{
	DF_NO_HEALTH			= 1 << 0,	// Do not spawn health items (DM)
	DF_NO_ITEMS				= 1 << 1,	// Do not spawn powerups (DM)
	DF_WEAPONS_STAY			= 1 << 2,	// Leave weapons around after pickup (DM)
	DF_FORCE_FALLINGZD		= 1 << 3,	// Falling too far hurts (old ZDoom style)
	DF_FORCE_FALLINGHX		= 1 << 4,	// Falling too far hurts (Hexen style)
	DF_FORCE_FALLINGST		= 3 << 3,	// Falling too far hurts (Strife style)
//	DF_INVENTORY_ITEMS		= 1 << 5,	// Wait for player to use powerups when picked up
	DF_SAME_LEVEL			= 1 << 6,	// Stay on the same map when someone exits (DM)
	DF_SPAWN_FARTHEST		= 1 << 7,	// Spawn players as far as possible from other players (DM)
	DF_FORCE_RESPAWN		= 1 << 8,	// Automatically respawn dead players after respawn_time is up (DM)
	DF_NO_ARMOR				= 1 << 9,	// Do not spawn armor (DM)
	DF_NO_EXIT				= 1 << 10,	// Kill anyone who tries to exit the level (DM)
	DF_INFINITE_AMMO		= 1 << 11,	// Don't use up ammo when firing
	DF_NO_MONSTERS			= 1 << 12,	// Don't spawn monsters (replaces -nomonsters parm)
	DF_MONSTERS_RESPAWN		= 1 << 13,	// Monsters respawn sometime after their death (replaces -respawn parm)
	DF_ITEMS_RESPAWN		= 1 << 14,	// Items other than invuln. and invis. respawn
	DF_FAST_MONSTERS		= 1 << 15,	// Monsters are fast (replaces -fast parm)
	DF_NO_JUMP				= 1 << 16,	// Don't allow jumping
	DF_NO_FREELOOK			= 1 << 17,	// Don't allow freelook
	DF_RESPAWN_SUPER		= 1 << 18,	// Respawn invulnerability and invisibility
	DF_NO_FOV				= 1 << 19,	// Only let the arbitrator set FOV (for all players)
	DF_NO_COOP_WEAPON_SPAWN	= 1 << 20,	// Don't spawn multiplayer weapons in coop games
	DF_NO_CROUCH			= 1 << 21,	// Don't allow crouching
	DF_COOP_LOSE_INVENTORY	= 1 << 22,	// Lose all your old inventory when respawning in coop
	DF_COOP_LOSE_KEYS		= 1 << 23,	// Lose keys when respawning in coop
	DF_COOP_LOSE_WEAPONS	= 1 << 24,	// Lose weapons when respawning in coop
	DF_COOP_LOSE_ARMOR		= 1 << 25,	// Lose armor when respawning in coop
	DF_COOP_LOSE_POWERUPS	= 1 << 26,	// Lose powerups when respawning in coop
	DF_COOP_LOSE_AMMO		= 1 << 27,	// Lose ammo when respawning in coop
	DF_COOP_HALVE_AMMO		= 1 << 28,	// Lose half your ammo when respawning in coop (but not less than the normal starting amount)
};

// [BC] More dmflags. w00p!
enum
{
//	DF2_YES_IMPALING		= 1 << 0,	// Player gets implaed on MF2_IMPALE items
	DF2_YES_WEAPONDROP		= 1 << 1,	// Drop current weapon upon death

	// Don't spawn runes.
	DF2_NO_RUNES			= 1 << 2,

	// Instantly return flags and skulls when player carrying it dies (ST/CTF).
	DF2_INSTANT_RETURN		= 1 << 3,

	// Do not allow players to switch teams in teamgames.
	DF2_NO_TEAM_SWITCH		= 1 << 4,

	// Player is automatically placed on a team.
	DF2_NO_TEAM_SELECT		= 1 << 5,

	// Double amount of ammo that items give you like skill 1 and 5 do.
	DF2_YES_DOUBLEAMMO		= 1 << 6,

	// Player slowly loses health when over 100% (quake-style).
	DF2_YES_DEGENERATION	= 1 << 7,

	// Allow BFG freeaiming in multiplayer games.
	DF2_YES_FREEAIMBFG		= 1 << 8,

	// Barrels respawn (duh).
	DF2_BARRELS_RESPAWN		= 1 << 9,

	// No respawn invulnerability.
	DF2_NO_RESPAWN_INVUL	= 1 << 10,

	// All players start with a shotgun when they respawn.
	DF2_COOP_SHOTGUNSTART	= 1 << 11,

	// Players respawn in the same place they died (co-op).
	DF2_SAME_SPAWN_SPOT		= 1 << 12,

};

// [RH] Compatibility flags.
enum
{
	COMPATF_SHORTTEX		= 1 << 0,	// Use Doom's shortest texture around behavior?
	COMPATF_STAIRINDEX		= 1 << 1,	// Don't fix loop index for stair building?
	COMPATF_LIMITPAIN		= 1 << 2,	// Pain elemental is limited to 20 lost souls?
	COMPATF_SILENTPICKUP	= 1 << 3,	// Pickups are only heard locally?
	COMPATF_NO_PASSMOBJ		= 1 << 4,	// Pretend every actor is infinitely tall?
	COMPATF_MAGICSILENCE	= 1 << 5,	// Limit actors to one sound at a time?
	COMPATF_WALLRUN			= 1 << 6,	// Enable buggier wall clipping so players can wallrun?
	COMPATF_NOTOSSDROPS		= 1 << 7,	// Spawn dropped items directly on the floor?
	COMPATF_USEBLOCKING		= 1 << 8,	// Any special line can block a use line
	COMPATF_NODOORLIGHT		= 1 << 9,	// Don't do the BOOM local door light effect
	COMPATF_RAVENSCROLL		= 1 << 10,	// Raven's scrollers use their original carrying speed
	COMPATF_SOUNDTARGET		= 1 << 11,	// Use sector based sound target code.
	COMPATF_DEHHEALTH		= 1 << 12,	// Limit deh.MaxHealth to the health bonus (as in Doom2.exe)
	COMPATF_TRACE			= 1 << 13,	// Trace ignores lines with the same sector on both sides
	COMPATF_DROPOFF			= 1 << 14,	// Monsters cannot move when hanging over a dropoff
	COMPATF_BOOMSCROLL		= 1 << 15,	// Scrolling sectors are additive like in Boom

	// [BC] Start of new compatflags.

	// Limited movement in the air.
	COMPATF_LIMITED_AIRMOVEMENT	= 1 << 16,

	// Allow the map01 "plasma bump" bug.
	COMPATF_PLASMA_BUMP_BUG	= 1 << 17,

	// Allow instant respawn after death.
	COMPATF_INSTANTRESPAWN	= 1 << 18,

	// Taunting is disabled.
	COMPATF_DISABLETAUNTS	= 1 << 19,

	// Use doom2.exe's original sound curve.
	COMPATF_ORIGINALSOUNDCURVE	= 1 << 20,

	// Use doom2.exe's original intermission screens/music.
	COMPATF_OLDINTERMISSION		= 1 << 21,

	// Disable stealth monsters, since doom2.exe didn't have them.
	COMPATF_DISABLESTEALTHMONSTERS		= 1 << 22,

	// Disable cooperative backpacks.
//	COMPATF_DISABLECOOPERATIVEBACKPACKS	= 1 << 23,
};

// phares 3/20/98:
//
// Player friction is variable, based on controlling
// linedefs. More friction can create mud, sludge,
// magnetized floors, etc. Less friction can create ice.

#define MORE_FRICTION_MOMENTUM	15000	// mud factor based on momentum
#define ORIG_FRICTION			0xE800	// original value
#define ORIG_FRICTION_FACTOR	2048	// original value
#define FRICTION_LOW			0xf900
#define FRICTION_FLY			0xeb00


#define BLINKTHRESHOLD (4*32)

#ifndef WORDS_BIGENDIAN
#define MAKE_ID(a,b,c,d)	((a)|((b)<<8)|((c)<<16)|((d)<<24))
#else
#define MAKE_ID(a,b,c,d)	((d)|((c)<<8)|((b)<<16)|((a)<<24))
#endif

#endif	// __DOOMDEF_H__
