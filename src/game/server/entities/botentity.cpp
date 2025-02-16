#include <engine/shared/config.h>

#include <game/server/botmanager.h>
#include <game/server/gamecontext.h>
#include <game/server/gamecontroller.h>
#include <game/server/player.h>

#include <generated/server_data.h>

#include "botentity.h"

CBotEntity::CBotEntity(CGameWorld *pWorld, vec2 Pos, int BotID, STeeInfo TeeInfos) :
	CDamageEntity(pWorld, CGameWorld::ENTTYPE_BOTENTITY, Pos, ms_PhysSize)
{
	m_BotID = BotID;
	m_Emote = random_int() % NUM_EMOTES;
	m_TeeInfos = TeeInfos;

	m_Core.Reset();
	m_Core.Init(GameServer()->BotManager()->BotWorldCore(), GameServer()->Collision());
	m_Core.m_Pos = m_Pos;

	m_ReckoningTick = 0;
	mem_zero(&m_SendCore, sizeof(m_SendCore));
	mem_zero(&m_ReckoningCore, sizeof(m_ReckoningCore));

	GameServer()->CreatePlayerSpawn(Pos);
	GameWorld()->InsertEntity(this);
}

void CBotEntity::Tick()
{
	m_Core.Tick(false);

	// handle leaving gamelayer
	if(GameLayerClipped(m_Pos))
	{
		Die(this, WEAPON_WORLD);
	}
}

void CBotEntity::TickDefered()
{
	static const vec2 ColBox(CCharacterCore::PHYS_SIZE, CCharacterCore::PHYS_SIZE);
	// advance the dummy
	{
		CWorldCore TempWorld;
		m_ReckoningCore.Init(&TempWorld, GameServer()->Collision());
		m_ReckoningCore.Tick(false);
		m_ReckoningCore.Move();
		m_ReckoningCore.Quantize();
	}

	// apply drag velocity
	// and set it back to 0 for the next tick
	m_Core.AddDragVelocity();
	m_Core.ResetDragVelocity();

	// lastsentcore
	vec2 StartPos = m_Core.m_Pos;
	vec2 StartVel = m_Core.m_Vel;
	bool StuckBefore = GameServer()->Collision()->TestBox(m_Core.m_Pos, ColBox);

	m_Core.Move();

	bool StuckAfterMove = GameServer()->Collision()->TestBox(m_Core.m_Pos, ColBox);
	m_Core.Quantize();
	bool StuckAfterQuant = GameServer()->Collision()->TestBox(m_Core.m_Pos, ColBox);
	m_Pos = m_Core.m_Pos;

	if(!StuckBefore && (StuckAfterMove || StuckAfterQuant))
	{
		// Hackish solution to get rid of strict-aliasing warning
		union
		{
			float f;
			unsigned u;
		} StartPosX, StartPosY, StartVelX, StartVelY;

		StartPosX.f = StartPos.x;
		StartPosY.f = StartPos.y;
		StartVelX.f = StartVel.x;
		StartVelY.f = StartVel.y;

		char aBuf[256];
		str_format(aBuf, sizeof(aBuf), "STUCK!!! %d %d %d %f %f %f %f %x %x %x %x",
			StuckBefore,
			StuckAfterMove,
			StuckAfterQuant,
			StartPos.x, StartPos.y,
			StartVel.x, StartVel.y,
			StartPosX.u, StartPosY.u,
			StartVelX.u, StartVelY.u);
		GameServer()->Console()->Print(IConsole::OUTPUT_LEVEL_DEBUG, "game", aBuf);
	}

	m_TriggeredEvents |= m_Core.m_TriggeredEvents;

	if(m_Core.m_Death)
	{
		// handle death-tiles
		Die(this, WEAPON_WORLD);
	}

	// update the m_SendCore if needed
	{
		CNetObj_Character Predicted;
		CNetObj_Character Current;
		mem_zero(&Predicted, sizeof(Predicted));
		mem_zero(&Current, sizeof(Current));
		m_ReckoningCore.Write(&Predicted);
		m_Core.Write(&Current);

		// only allow dead reckoning for a top of 3 seconds
		if(m_ReckoningTick + Server()->TickSpeed() * 3 < Server()->Tick() || mem_comp(&Predicted, &Current, sizeof(CNetObj_Character)) != 0)
		{
			m_ReckoningTick = Server()->Tick();
			m_SendCore = m_Core;
			m_ReckoningCore = m_Core;
		}
	}
}

void CBotEntity::Snap(int SnappingClient)
{
	int ClientID = GameServer()->BotManager()->FindClientID(SnappingClient, GetBotID());
	if(ClientID == -1)
		return;

	CNetObj_Character *pCharacter = static_cast<CNetObj_Character *>(Server()->SnapNewItem(NETOBJTYPE_CHARACTER, ClientID, sizeof(CNetObj_Character)));
	if(!pCharacter)
		return;

	// write down the m_Core
	if(!m_ReckoningTick || GameWorld()->m_Paused)
	{
		// no dead reckoning when paused because the client doesn't know
		// how far to perform the reckoning
		pCharacter->m_Tick = 0;
		m_Core.Write(pCharacter);
	}
	else
	{
		pCharacter->m_Tick = m_ReckoningTick;
		m_SendCore.Write(pCharacter);
	}

	pCharacter->m_Emote = m_Emote;

	pCharacter->m_AmmoCount = 0;
	pCharacter->m_Health = 0;
	pCharacter->m_Armor = 0;
	pCharacter->m_TriggeredEvents = m_TriggeredEvents;

	pCharacter->m_Weapon = WEAPON_HAMMER;
	pCharacter->m_AttackTick = 0;

	pCharacter->m_Direction = m_Core.m_Direction;

	if(ClientID == SnappingClient || SnappingClient == -1 ||
		(!Config()->m_SvStrictSpectateMode && ClientID == GameServer()->m_apPlayers[SnappingClient]->GetSpectatorID()))
	{
		pCharacter->m_Health = clamp(round_to_int(GetHealth() / (float) GetMaxHealth() * 10), 0, 10);
		pCharacter->m_Armor = clamp(round_to_int(GetArmor() / (float) GetMaxArmor() * 10), 0, 10);
		pCharacter->m_AmmoCount = 0;
	}

	CNetObj_PlayerInfo *pPlayerInfo = static_cast<CNetObj_PlayerInfo *>(Server()->SnapNewItem(NETOBJTYPE_PLAYERINFO, ClientID, sizeof(CNetObj_PlayerInfo)));
	if(!pPlayerInfo)
		return;

	pPlayerInfo->m_PlayerFlags = 0;
	pPlayerInfo->m_Latency = 0;
	pPlayerInfo->m_Score = 0;

	// demo recording
	if(SnappingClient == -1)
	{
		CNetObj_De_ClientInfo *pClientInfo = static_cast<CNetObj_De_ClientInfo *>(Server()->SnapNewItem(NETOBJTYPE_DE_CLIENTINFO, ClientID, sizeof(CNetObj_De_ClientInfo)));
		if(!pClientInfo)
			return;

		pClientInfo->m_Local = 0;
		pClientInfo->m_Team = TEAM_BLUE;
		StrToInts(pClientInfo->m_aName, 4, "");
		StrToInts(pClientInfo->m_aClan, 3, "");
		pClientInfo->m_Country = -1;

		for(int p = 0; p < NUM_SKINPARTS; p++)
		{
			StrToInts(pClientInfo->m_aaSkinPartNames[p], 6, m_TeeInfos.m_aaSkinPartNames[p]);
			pClientInfo->m_aUseCustomColors[p] = m_TeeInfos.m_aUseCustomColors[p];
			pClientInfo->m_aSkinPartColors[p] = m_TeeInfos.m_aSkinPartColors[p];
		}
	}
}

void CBotEntity::Die(CEntity *pKiller, int Weapon)
{
	int Killer = -1;
	if(pKiller && (pKiller->GetObjFlag() & EEntityFlag::ENTFLAG_OWNER))
		Killer = ((CBaseOwnerEntity *) pKiller)->GetOwner();

	// send the kill message
	CNetMsg_Sv_KillMsg Msg;
	Msg.m_ModeSpecial = 0;
	for(int i = 0; i < MAX_CLIENTS; i++)
	{
		if(!Server()->ClientIngame(i))
			continue;

		if(Killer < 0 && Server()->GetClientVersion(i) < MIN_KILLMESSAGE_CLIENTVERSION)
		{
			Msg.m_Killer = 0;
			Msg.m_Weapon = WEAPON_WORLD;
		}
		else
		{
			Msg.m_Killer = Killer;
			Msg.m_Weapon = Weapon;
		}
		Server()->SendPackMsg(&Msg, MSGFLAG_VITAL, i);
	}

	// a nice sound
	GameServer()->CreateSound(m_Pos, SOUND_PLAYER_DIE);

	CDamageEntity::Die(pKiller, Weapon);
	GameServer()->BotManager()->CreateDeath(m_Pos, GetBotID());

	if(GameServer()->BotManager())
		GameServer()->BotManager()->OnBotDeath(GetBotID());
}

bool CBotEntity::IsFriendlyDamage(CEntity *pFrom)
{
	if(!pFrom)
		return true;

	return false;
}

bool CBotEntity::TakeDamage(vec2 Force, vec2 Source, int Dmg, CEntity *pFrom, int Weapon)
{
	m_Core.m_Vel += Force;
	int Owner = -1;
	if(pFrom && (pFrom->GetObjFlag() & EEntityFlag::ENTFLAG_OWNER))
		Owner = ((CBaseOwnerEntity *) pFrom)->GetOwner();

	int OldHealth = m_Health, OldArmor = m_Armor;
	bool Return = CDamageEntity::TakeDamage(Force, Source, Dmg, pFrom, Weapon);
	// create healthmod indicator
	GameServer()->BotManager()->CreateDamage(m_Pos, m_BotID, Source, OldHealth - m_Health, OldArmor - m_Armor, pFrom == this);

	// do damage Hit sound
	if(pFrom && pFrom != this && Owner > -1 && GameServer()->m_apPlayers[Owner])
	{
		int64 Mask = CmaskOne(Owner);
		for(int i = 0; i < MAX_CLIENTS; i++)
		{
			if(GameServer()->m_apPlayers[i] && (GameServer()->m_apPlayers[i]->GetTeam() == TEAM_SPECTATORS || GameServer()->m_apPlayers[i]->m_DeadSpecMode) &&
				GameServer()->m_apPlayers[i]->GetSpectatorID() == Owner)
				Mask |= CmaskOne(i);
		}
		GameServer()->CreateSound(GameServer()->m_apPlayers[Owner]->m_ViewPos, SOUND_HIT, Mask);
	}

	if(Dmg > 2)
		GameServer()->CreateSound(m_Pos, SOUND_PLAYER_PAIN_LONG);
	else
		GameServer()->CreateSound(m_Pos, SOUND_PLAYER_PAIN_SHORT);

	return Return;
}
