//-----------------------------------------------------------------------------
//
// Unlagged Source
// Copyright (C) 2010 "Spleen"
// Copyright (C) 2010-2012 Skulltag Development Team
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
//
// 1. Redistributions of source code must retain the above copyright notice,
//    this list of conditions and the following disclaimer.
// 2. Redistributions in binary form must reproduce the above copyright notice,
//    this list of conditions and the following disclaimer in the documentation
//    and/or other materials provided with the distribution.
// 3. Neither the name of the Skulltag Development Team nor the names of its
//    contributors may be used to endorse or promote products derived from this
//    software without specific prior written permission.
// 4. Redistributions in any form must be accompanied by information on how to
//    obtain complete source code for the software and any accompanying
//    software that uses the software. The source code must either be included
//    in the distribution or be available for no more than the cost of
//    distribution plus a nominal fee, and must be freely redistributable
//    under reasonable conditions. For an executable file, complete source
//    code means the source code for all modules it contains. It does not
//    include source code for modules or files that typically accompany the
//    major components of the operating system on which the executable file
//    runs.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
// ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
// LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
// CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
// SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
// INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
// CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
// ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
// POSSIBILITY OF SUCH DAMAGE.
//
//
//
// Filename: unlagged.cpp
//
// Description: Contains variables and routines related to the backwards
// reconciliation.
//
//-----------------------------------------------------------------------------


#include "unlagged.h"
#include "doomstat.h"
#include "doomdef.h"
#include "actor.h"
#include "d_player.h"
#include "r_defs.h"
#include "network.h"
#include "r_state.h"
#include "p_local.h"

CVAR(Flag, sv_unlagged, dmflags3, DF3_UNLAGGED);

//Figure out which tic to use for reconciliation
int		UnlaggedGametic( player_t *player )
{
	int ulPing, unlaggedGametic;


	ulPing = player->ulPing;
	unlaggedGametic = ( gametic - ( ulPing * TICRATE / MS_PER_SECOND ) );
	
	//don't look up tics that are too old
	if ( (gametic - unlaggedGametic) >= UNLAGGEDTICS)
		unlaggedGametic = gametic - UNLAGGEDTICS + 1;

	//don't look up negative tics
	if (unlaggedGametic < 0)
		unlaggedGametic = 0;


	return unlaggedGametic;
}

// Shift stuff back in time before doing hitscan calculations
// Call UnlaggedRestore afterwards to restore everything
void	UnlaggedReconcile( AActor *actor )
{
	//Only do anything if the actor to be reconciled is a player
	//and it's on a server with unlagged on
	if ( !actor->player || (NETWORK_GetState() != NETSTATE_SERVER) || !( dmflags3 & DF3_UNLAGGED ) )
		return;
	
	int unlaggedGametic, unlaggedIndex;
	

	unlaggedGametic = UnlaggedGametic( actor->player );

	//find the index
	unlaggedIndex = unlaggedGametic % UNLAGGEDTICS;

	
	//reconcile the sectors
	for (int i = 0; i < numsectors; i++)
	{
		sectors[i].floorplane.restoreD = sectors[i].floorplane.d;
		sectors[i].ceilingplane.restoreD = sectors[i].ceilingplane.d;
		
		//Don't reconcile if the unlagged gametic is the same as the current
		//because unlagged data for this tic may not be completely recorded yet
		if (gametic != unlaggedGametic)
		{
			sectors[i].floorplane.d = sectors[i].floorplane.unlaggedD[unlaggedIndex];
			sectors[i].ceilingplane.d = sectors[i].ceilingplane.unlaggedD[unlaggedIndex];
		}
	}
	
	
	//reconcile the players
	for (int i = 0; i < MAXPLAYERS; i++)
	{
		if (playeringame[i] && players[i].mo && !players[i].bSpectating)
		{
			players[i].restoreX = players[i].mo->x;
			players[i].restoreY = players[i].mo->y;
			players[i].restoreZ = players[i].mo->z;
			
			//Work around limitations of SetOrigin to prevent players
			//from getting stuck in ledges
			players[i].restoreFloorZ = players[i].mo->floorz;
			
			//Don't reconcile if the unlagged gametic is the same as the current
			//because unlagged data for this tic may not be completely recorded yet
			if (gametic == unlaggedGametic)
				continue;

			//Also, don't reconcile the shooter because the client is supposed
			//to predict him
			if (players+i != actor->player)
			{
				players[i].mo->SetOrigin(
					players[i].unlaggedX[unlaggedIndex],
					players[i].unlaggedY[unlaggedIndex],
					players[i].unlaggedZ[unlaggedIndex]
				);
			}
			else
			//However, the client sometimes mispredicts itself if it's on a moving sector.
			//We need to correct for that.
			{
				//current server floorz/ceilingz before reconciliation
				fixed_t serverFloorZ = actor->floorz;
				fixed_t serverCeilingZ = actor->ceilingz;
				

				//reset floorz/ceilingz with SetOrigin call
				actor->SetOrigin( actor->x, actor->y, actor->z );


				//force the shooter out of the floor/ceiling - a client has to mispredict in this case,
				//because not mispredicting would mean the client would think he's inside the floor/ceiling
				if (actor->z + actor->height > actor->ceilingz)
					actor->z = actor->ceilingz - actor->height;

				if (actor->z < actor->floorz)
					actor->z = actor->floorz;


				//floor moved up - a client might have mispredicted himself too low due to gravity
				//and the client thinking the floor is lower than it actually is
				if (serverFloorZ > actor->floorz)
				{
					//shooter was standing on the floor, let's pull him down to his floor if
					//he wasn't falling
					if ( (actor->z == serverFloorZ) && (actor->momz >= 0) )
						actor->z = actor->floorz;

					//todo: more correction for floor moving up
				}


				//todo: more correction for client misprediction
			}
		}
	}	
}


// Restore everything that has been shifted
// back in time by UnlaggedReconcile
void	UnlaggedRestore( AActor *actor )
{
	//Only do anything if the actor to be restored is a player
	//and it's on a server with unlagged on
	if ( !actor->player || (NETWORK_GetState() != NETSTATE_SERVER) || !( dmflags3 & DF3_UNLAGGED ) )
		return;
	
	
	//restore the sectors
	for (int i = 0; i < numsectors; i++)
	{
		sectors[i].floorplane.d = sectors[i].floorplane.restoreD;
		sectors[i].ceilingplane.d = sectors[i].ceilingplane.restoreD;
	}


	//restore the players
	for (int i = 0; i < MAXPLAYERS; i++)
	{
		if (playeringame[i] && players[i].mo && !players[i].bSpectating)
		{
			players[i].mo->SetOrigin( players[i].restoreX, players[i].restoreY, players[i].restoreZ );
			players[i].mo->floorz = players[i].restoreFloorZ;
		}
	}
}


// Record the positions of just one player
// in order to be able to reconcile them later
void	UnlaggedRecordPlayer( player_t *player )
{
	//Only do anything if it's on a server
	if (NETWORK_GetState() != NETSTATE_SERVER)
		return;

	
	int unlaggedIndex;

	//find the index
	unlaggedIndex = gametic % UNLAGGEDTICS;


	//record the player
	player->unlaggedX[unlaggedIndex] = player->mo->x;
	player->unlaggedY[unlaggedIndex] = player->mo->y;
	player->unlaggedZ[unlaggedIndex] = player->mo->z;
}


// Reset the reconciliation buffers of a player
// Should be called when a player is spawned
void	UnlaggedResetPlayer( player_t *player )
{
	//Only do anything if it's on a server
	if (NETWORK_GetState() != NETSTATE_SERVER)
		return;

	for (int unlaggedIndex = 0; unlaggedIndex < UNLAGGEDTICS; unlaggedIndex++)
	{
		player->unlaggedX[unlaggedIndex] = player->mo->x;
		player->unlaggedY[unlaggedIndex] = player->mo->y;
		player->unlaggedZ[unlaggedIndex] = player->mo->z;
	}
}


// Record the positions of the sectors
void	UnlaggedRecordSectors( )
{
	//Only do anything if it's on a server
	if (NETWORK_GetState() != NETSTATE_SERVER)
		return;

	
	int unlaggedIndex;

	//find the index
	unlaggedIndex = gametic % UNLAGGEDTICS;


	//record the sectors
	for (int i = 0; i < numsectors; i++)
	{
		sectors[i].floorplane.unlaggedD[unlaggedIndex] = sectors[i].floorplane.d;
		sectors[i].ceilingplane.unlaggedD[unlaggedIndex] = sectors[i].ceilingplane.d;
	}
}

bool UnlaggedDrawRailClientside ( AActor *attacker )
{
	if ( ( attacker == NULL ) || ( attacker->player == NULL ) )
		return false;

	// [BB] Rails are only client side when unlagged is on.
	if ( !( dmflags3 & DF3_UNLAGGED ) )
		return false;

	// [BB] A client should only draw rails for its own player.
	if ( ( attacker->player - players ) != consoleplayer )
		return false;

	return true;
}