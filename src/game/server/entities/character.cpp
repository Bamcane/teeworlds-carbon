/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include <engine/shared/config.h>

#include <game/server/gamecontext.h>
#include <game/server/gamecontroller.h>
#include <game/server/player.h>
#include <game/server/weapons.h>
#include <generated/protocol.h>
#include <generated/server_data.h>

#include "character.h"
#include "laser.h"
#include "projectile.h"

MACRO_ALLOC_POOL_ID_IMPL(CCharacter, SERVER_MAX_CLIENTS)

// Character, "physical" player's part
CCharacter::CCharacter(CGameWorld *pWorld) :
	CHealthEntity(pWorld, CGameWorld::ENTTYPE_CHARACTER, vec2(0, 0), ms_PhysSize)
{
	m_TriggeredEvents = 0;
	SetMaxHealth(10);
	SetMaxArmor(10);
}

void CCharacter::Reset()
{
	Destroy();
}

bool CCharacter::Spawn(CPlayer *pPlayer, vec2 Pos)
{
	m_EmoteStop = -1;
	m_LastAction = -1;
	m_LastNoAmmoSound = -1;
	m_ActiveWeapon = WEAPON_GUN;
	m_LastWeapon = WEAPON_HAMMER;
	m_QueuedWeapon = -1;

	m_pPlayer = pPlayer;
	m_Pos = Pos;

	m_Core.Reset();
	m_Core.Init(&GameWorld()->m_Core, GameServer()->Collision());
	m_Core.m_Pos = m_Pos;
	GameWorld()->m_Core.m_apCharacters[m_pPlayer->GetCID()] = &m_Core;

	m_ReckoningTick = 0;
	mem_zero(&m_SendCore, sizeof(m_SendCore));
	mem_zero(&m_ReckoningCore, sizeof(m_ReckoningCore));

	GameWorld()->InsertEntity(this);
	m_Alive = true;

	GameServer()->GameController()->OnCharacterSpawn(this);

	return true;
}

void CCharacter::Destroy()
{
	GameWorld()->m_Core.m_apCharacters[m_pPlayer->GetCID()] = 0;
	m_Alive = false;
}

void CCharacter::SetWeapon(int W)
{
	if(W == m_ActiveWeapon)
		return;

	m_LastWeapon = m_ActiveWeapon;
	m_QueuedWeapon = -1;
	m_ActiveWeapon = W;
	GameServer()->CreateSound(m_Pos, SOUND_WEAPON_SWITCH);

	if(m_ActiveWeapon < 0 || m_ActiveWeapon >= NUM_WEAPONS)
		m_ActiveWeapon = 0;
	m_aWeapons[m_ActiveWeapon].m_AmmoRegenStart = -1;
}

bool CCharacter::IsGrounded()
{
	if(GameServer()->Collision()->CheckPoint(m_Pos.x + GetProximityRadius() / 2, m_Pos.y + GetProximityRadius() / 2 + 5))
		return true;
	if(GameServer()->Collision()->CheckPoint(m_Pos.x - GetProximityRadius() / 2, m_Pos.y + GetProximityRadius() / 2 + 5))
		return true;
	return false;
}

void CCharacter::HandleNinja()
{
	if(m_ActiveWeapon != WEAPON_NINJA)
		return;

	if((Server()->Tick() - m_Ninja.m_ActivationTick) > (g_pData->m_Weapons.m_Ninja.m_Duration * Server()->TickSpeed() / 1000))
	{
		// time's up, return
		m_aWeapons[WEAPON_NINJA].m_Got = false;
		m_ActiveWeapon = m_LastWeapon;

		// reset velocity and current move
		if(m_Ninja.m_CurrentMoveTime > 0)
			m_Core.m_Vel = m_Ninja.m_ActivationDir * m_Ninja.m_OldVelAmount;
		m_Ninja.m_CurrentMoveTime = -1;

		SetWeapon(m_ActiveWeapon);
		return;
	}

	// force ninja Weapon
	SetWeapon(WEAPON_NINJA);

	m_Ninja.m_CurrentMoveTime--;

	if(m_Ninja.m_CurrentMoveTime == 0)
	{
		// reset velocity
		m_Core.m_Vel = m_Ninja.m_ActivationDir * m_Ninja.m_OldVelAmount;
	}
	else if(m_Ninja.m_CurrentMoveTime > 0)
	{
		// Set velocity
		m_Core.m_Vel = m_Ninja.m_ActivationDir * g_pData->m_Weapons.m_Ninja.m_Velocity;
		vec2 OldPos = m_Pos;
		GameServer()->Collision()->MoveBox(&m_Core.m_Pos, &m_Core.m_Vel, vec2(GetProximityRadius(), GetProximityRadius()), 0.f);

		// reset velocity so the client doesn't predict stuff
		m_Core.m_Vel = vec2(0.f, 0.f);

		// check if we hit anything along the way
		const float Radius = GetProximityRadius() * 2.0f;
		const vec2 Center = OldPos + (m_Pos - OldPos) * 0.5f;
		CHealthEntity *apEnts[MAX_CHECK_ENTITY];
		const int Num = GameWorld()->FindEntities(Center, Radius, (CEntity **) apEnts, MAX_CHECK_ENTITY, EEntityFlag::ENTFLAG_DAMAGE);

		for(int i = 0; i < Num; ++i)
		{
			if(apEnts[i] == this)
				continue;

			// make sure we haven't hit this object before
			bool AlreadyHit = false;
			for(int j = 0; j < m_Ninja.m_NumObjectsHit; j++)
			{
				if(m_Ninja.m_apHitObjects[j] == apEnts[i])
				{
					AlreadyHit = true;
					break;
				}
			}
			if(AlreadyHit)
				continue;

			// check so we are sufficiently close
			if(distance(apEnts[i]->GetPos(), m_Pos) > Radius)
				continue;

			// Hit a player, give him damage and stuffs...
			GameServer()->CreateSound(apEnts[i]->GetPos(), SOUND_NINJA_HIT);
			if(m_Ninja.m_NumObjectsHit < SERVER_MAX_CLIENTS)
				m_Ninja.m_apHitObjects[m_Ninja.m_NumObjectsHit++] = apEnts[i];

			// set his velocity to fast upward (for now)
			apEnts[i]->TakeDamage(vec2(0, -10.0f), m_Ninja.m_ActivationDir * -1, g_pData->m_Weapons.m_Ninja.m_pBase->m_Damage, this, WEAPON_NINJA);
		}
	}
}

void CCharacter::DoWeaponSwitch()
{
	// make sure we can switch
	if(m_ReloadTimer != 0 || m_QueuedWeapon == -1 || m_aWeapons[WEAPON_NINJA].m_Got)
		return;

	// switch Weapon
	SetWeapon(m_QueuedWeapon);
}

void CCharacter::HandleWeaponSwitch()
{
	int WantedWeapon = m_ActiveWeapon;
	if(m_QueuedWeapon != -1)
		WantedWeapon = m_QueuedWeapon;

	// select Weapon
	int Next = CountInput(m_LatestPrevInput.m_NextWeapon, m_LatestInput.m_NextWeapon).m_Presses;
	int Prev = CountInput(m_LatestPrevInput.m_PrevWeapon, m_LatestInput.m_PrevWeapon).m_Presses;

	if(Next < 128) // make sure we only try sane stuff
	{
		while(Next) // Next Weapon selection
		{
			WantedWeapon = (WantedWeapon + 1) % NUM_WEAPONS;
			if(m_aWeapons[WantedWeapon].m_Got)
				Next--;
		}
	}

	if(Prev < 128) // make sure we only try sane stuff
	{
		while(Prev) // Prev Weapon selection
		{
			WantedWeapon = (WantedWeapon - 1) < 0 ? NUM_WEAPONS - 1 : WantedWeapon - 1;
			if(m_aWeapons[WantedWeapon].m_Got)
				Prev--;
		}
	}

	// Direct Weapon selection
	if(m_LatestInput.m_WantedWeapon)
		WantedWeapon = m_Input.m_WantedWeapon - 1;

	// check for insane values
	if(WantedWeapon >= 0 && WantedWeapon < NUM_WEAPONS && WantedWeapon != m_ActiveWeapon && m_aWeapons[WantedWeapon].m_Got)
		m_QueuedWeapon = WantedWeapon;

	DoWeaponSwitch();
}

void CCharacter::FireWeapon()
{
	if(m_ReloadTimer != 0)
		return;

	DoWeaponSwitch();
	vec2 Direction = normalize(vec2(m_LatestInput.m_TargetX, m_LatestInput.m_TargetY));

	bool FullAuto = WeaponManager()->GetWeapon(m_aWeapons[m_ActiveWeapon].m_Weapon)->FullAuto();

	// check if we gonna fire
	bool WillFire = false;
	if(CountInput(m_LatestPrevInput.m_Fire, m_LatestInput.m_Fire).m_Presses)
		WillFire = true;

	if(FullAuto && (m_LatestInput.m_Fire & 1) && m_aWeapons[m_ActiveWeapon].m_Ammo)
		WillFire = true;

	if(!WillFire)
		return;

	// check for ammo
	if(!m_aWeapons[m_ActiveWeapon].m_Ammo)
	{
		// 125ms is a magical limit of how fast a human can click
		m_ReloadTimer = 125 * Server()->TickSpeed() / 1000;
		if(m_LastNoAmmoSound + Server()->TickSpeed() <= Server()->Tick())
		{
			GameServer()->CreateSound(m_Pos, SOUND_WEAPON_NOAMMO);
			m_LastNoAmmoSound = Server()->Tick();
		}
		return;
	}

	vec2 ProjStartPos = m_Pos + Direction * GetProximityRadius() * 0.75f;

	if(Config()->m_Debug)
	{
		char aBuf[256];
		str_format(aBuf, sizeof(aBuf), "shot player='%d:%s' team=%d weapon=%s", m_pPlayer->GetCID(), Server()->ClientName(m_pPlayer->GetCID()), m_pPlayer->GetTeam(), WeaponManager()->GetWeapon(m_aWeapons[m_ActiveWeapon].m_Weapon)->Name());
		GameServer()->Console()->Print(IConsole::OUTPUT_LEVEL_DEBUG, "game", aBuf);
	}

	WeaponManager()->GetWeapon(m_aWeapons[m_ActiveWeapon].m_Weapon)->OnFire(this, GameWorld(), ProjStartPos, Direction, &m_ReloadTimer);

	m_AttackTick = Server()->Tick();

	if(m_aWeapons[m_ActiveWeapon].m_Ammo > 0) // -1 == unlimited
		m_aWeapons[m_ActiveWeapon].m_Ammo--;

	if(!m_ReloadTimer)
		m_ReloadTimer = WeaponManager()->GetWeapon(m_aWeapons[m_ActiveWeapon].m_Weapon)->FireDelay() * Server()->TickSpeed() / 1000;
}

void CCharacter::HandleWeapons()
{
	// ninja
	HandleNinja();

	// check reload timer
	if(m_ReloadTimer)
	{
		m_ReloadTimer--;
		return;
	}

	// fire Weapon, if wanted
	FireWeapon();

	// ammo regen
	int AmmoRegenTime = WeaponManager()->GetWeapon(m_aWeapons[m_ActiveWeapon].m_Weapon)->AmmoRegenTime();
	if(AmmoRegenTime && m_aWeapons[m_ActiveWeapon].m_Ammo >= 0)
	{
		// If equipped and not active, regen ammo?
		if(m_ReloadTimer <= 0)
		{
			if(m_aWeapons[m_ActiveWeapon].m_AmmoRegenStart < 0)
				m_aWeapons[m_ActiveWeapon].m_AmmoRegenStart = Server()->Tick();

			if((Server()->Tick() - m_aWeapons[m_ActiveWeapon].m_AmmoRegenStart) >= AmmoRegenTime * Server()->TickSpeed() / 1000)
			{
				// Add some ammo
				m_aWeapons[m_ActiveWeapon].m_Ammo = minimum(m_aWeapons[m_ActiveWeapon].m_Ammo + 1,
					WeaponManager()->GetWeapon(m_aWeapons[m_ActiveWeapon].m_Weapon)->MaxAmmo());
				m_aWeapons[m_ActiveWeapon].m_AmmoRegenStart = -1;
			}
		}
		else
		{
			m_aWeapons[m_ActiveWeapon].m_AmmoRegenStart = -1;
		}
	}

	return;
}

bool CCharacter::GiveWeapon(Uuid WeaponID, int Ammo)
{
	for(int i = 0; i < NUM_WEAPONS; i++)
	{
		if(!m_aWeapons[i].m_Got)
			continue;
		if(m_aWeapons[i].m_Weapon == WeaponID)
		{
			m_aWeapons[i].m_Ammo = minimum(WeaponManager()->GetWeapon(WeaponID)->MaxAmmo(), m_aWeapons[i].m_Ammo + Ammo);
			return true;
		}
	}
	return false;
}

void CCharacter::SetWeapon(int Place, Uuid WeaponID, int Ammo)
{
	if(Place < 0 || Place >= NUM_WEAPONS)
		return;

	m_aWeapons[Place].m_Got = true;
	m_aWeapons[Place].m_Weapon = WeaponID;
	m_aWeapons[Place].m_Ammo = minimum(WeaponManager()->GetWeapon(WeaponID)->MaxAmmo(), Ammo);
}

void CCharacter::GiveNinja()
{
	m_Ninja.m_ActivationTick = Server()->Tick();
	m_aWeapons[WEAPON_NINJA].m_Got = true;
	m_aWeapons[WEAPON_NINJA].m_Ammo = -1;
	if(m_ActiveWeapon != WEAPON_NINJA)
		m_LastWeapon = m_ActiveWeapon;
	m_ActiveWeapon = WEAPON_NINJA;

	GameServer()->CreateSound(m_Pos, SOUND_PICKUP_NINJA);
}

void CCharacter::SetEmote(int Emote, int Tick)
{
	m_EmoteType = Emote;
	m_EmoteStop = Tick;
}

void CCharacter::OnPredictedInput(CNetObj_PlayerInput *pNewInput)
{
	// check for changes
	if(mem_comp(&m_Input, pNewInput, sizeof(CNetObj_PlayerInput)) != 0)
		m_LastAction = Server()->Tick();

	// copy new input
	mem_copy(&m_Input, pNewInput, sizeof(m_Input));
	m_NumInputs++;

	// it is not allowed to aim in the center
	if(m_Input.m_TargetX == 0 && m_Input.m_TargetY == 0)
		m_Input.m_TargetY = -1;
}

void CCharacter::OnDirectInput(CNetObj_PlayerInput *pNewInput)
{
	mem_copy(&m_LatestPrevInput, &m_LatestInput, sizeof(m_LatestInput));
	mem_copy(&m_LatestInput, pNewInput, sizeof(m_LatestInput));

	// it is not allowed to aim in the center
	if(m_LatestInput.m_TargetX == 0 && m_LatestInput.m_TargetY == 0)
		m_LatestInput.m_TargetY = -1;

	if(m_NumInputs > 2 && m_pPlayer->GetTeam() != TEAM_SPECTATORS)
	{
		HandleWeaponSwitch();
		FireWeapon();
	}

	mem_copy(&m_LatestPrevInput, &m_LatestInput, sizeof(m_LatestInput));
}

void CCharacter::ResetInput()
{
	m_Input.m_Direction = 0;
	m_Input.m_Hook = 0;
	// simulate releasing the fire button
	if((m_Input.m_Fire & 1) != 0)
		m_Input.m_Fire++;
	m_Input.m_Fire &= INPUT_STATE_MASK;
	m_Input.m_Jump = 0;
	m_LatestPrevInput = m_LatestInput = m_Input;
}

void CCharacter::Tick()
{
	bool MoveAction = false;
	{
		MoveAction = MoveAction || (m_Input.m_Jump && !(m_Core.m_Jumped & 1));
		MoveAction = MoveAction || (m_Input.m_Direction != 0);
	}
	// handle sit
	if(IsSitting())
	{
		if(length(m_Core.m_Vel) > 1.0f || MoveAction)
		{
			SetSitting(false);
		}
		else
		{
			m_Core.m_Vel = vec2(0.f, 0.f);
			if(m_HealthRegenStart + (Config()->m_SvHealthRegenTime * Server()->TickSpeed() / 1000) < Server()->Tick())
			{
				m_HealthRegenStart = Server()->Tick();
				IncreaseHealth(1);
				GameServer()->CreateSound(m_Pos, SOUND_PICKUP_HEALTH, CmaskOne(m_pPlayer->GetCID()));
			}
		}
	}

	m_Core.m_Input = m_Input;
	m_Core.Tick(true);

	// handle sit
	if(IsSitting())
	{
		m_Core.m_Jumped &= ~2;
		m_Core.m_Vel = vec2(0.f, 0.f);
		if(length(m_SitPos - m_Pos) > 4.0f)
		{
			vec2 PosTo = m_Pos;
			PosTo += normalize(m_SitPos - m_Pos) * 4.0f;

			m_Core.m_Pos = PosTo;
			m_Pos = PosTo;
		}
		else
		{
			m_Core.m_Pos = m_SitPos;
			m_Pos = m_SitPos;
		}
	}

	// handle leaving gamelayer
	if(GameLayerClipped(m_Pos))
	{
		Die(this, WEAPON_WORLD);
	}

	// handle Weapons
	HandleWeapons();
}

void CCharacter::TickDefered()
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

	// apply drag velocity when the player is not firing ninja
	// and set it back to 0 for the next tick
	if(m_ActiveWeapon != WEAPON_NINJA || m_Ninja.m_CurrentMoveTime < 0 || IsSitting())
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

	if(m_pPlayer->GetTeam() == TEAM_SPECTATORS)
	{
		m_Pos.x = m_Input.m_TargetX;
		m_Pos.y = m_Input.m_TargetY;
	}
	else if(m_Core.m_Death)
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

void CCharacter::TickPaused()
{
	++m_AttackTick;
	++m_Ninja.m_ActivationTick;
	++m_ReckoningTick;
	if(m_LastAction != -1)
		++m_LastAction;
	if(m_aWeapons[m_ActiveWeapon].m_AmmoRegenStart > -1)
		++m_aWeapons[m_ActiveWeapon].m_AmmoRegenStart;
	if(m_EmoteStop > -1)
		++m_EmoteStop;
}

bool CCharacter::IsFriendlyDamage(CEntity *pFrom)
{
	if(!pFrom)
		return true;

	return false;
}

bool CCharacter::TakeDamage(vec2 Force, vec2 Source, int Dmg, CEntity *pFrom, int Weapon)
{
	m_Core.m_Vel += Force;
	int OldHealth = m_Health, OldArmor = m_Armor;
	bool Return = CHealthEntity::TakeDamage(Force, Source, Dmg, pFrom, Weapon);
	// create healthmod indicator
	GameServer()->CreateDamage(m_Pos, GetCID(), Source, OldHealth - m_Health, OldArmor - m_Armor, pFrom == this);

	// do damage Hit sound
	if(pFrom && pFrom->GetObjType() == CGameWorld::ENTTYPE_CHARACTER)
	{
		CCharacter *pChr = (CCharacter *) pFrom;
		int64 Mask = CmaskOne(pChr->GetCID());
		for(int i = 0; i < SERVER_MAX_CLIENTS; i++)
		{
			if(GameServer()->m_apPlayers[i] && (GameServer()->m_apPlayers[i]->GetTeam() == TEAM_SPECTATORS || GameServer()->m_apPlayers[i]->m_DeadSpecMode) &&
				GameServer()->m_apPlayers[i]->GetSpectatorID() == pChr->GetCID())
				Mask |= CmaskOne(i);
		}
		GameServer()->CreateSound(GameServer()->m_apPlayers[pChr->GetCID()]->m_ViewPos, SOUND_HIT, Mask);
	}

	if(Dmg > 2)
		GameServer()->CreateSound(m_Pos, SOUND_PLAYER_PAIN_LONG);
	else
		GameServer()->CreateSound(m_Pos, SOUND_PLAYER_PAIN_SHORT);

	SetEmote(EMOTE_PAIN, Server()->Tick() + 500 * Server()->TickSpeed() / 1000);

	return Return;
}

void CCharacter::Die(CEntity *pKiller, int Weapon)
{
	// we got to wait 0.5 secs before respawning
	m_pPlayer->m_RespawnTick = Server()->Tick() + Server()->TickSpeed() / 2;
	int ModeSpecial = GameServer()->GameController()->OnCharacterDeath(this, pKiller->GetObjType() != CGameWorld::ENTTYPE_CHARACTER ? nullptr : static_cast<CCharacter *>(pKiller)->GetPlayer(), Weapon);

	char aBuf[256];
	int Killer = -1;
	if(pKiller->GetObjType() == CGameWorld::ENTTYPE_CHARACTER)
	{
		Killer = static_cast<CCharacter *>(pKiller)->GetCID();
		str_format(aBuf, sizeof(aBuf), "kill killer='%d:%d:%s' victim='%d:%d:%s' weapon=%d special=%d",
			Killer, static_cast<CCharacter *>(pKiller)->GetPlayer()->GetTeam(), Server()->ClientName(Killer),
			m_pPlayer->GetCID(), m_pPlayer->GetTeam(), Server()->ClientName(m_pPlayer->GetCID()), Weapon, ModeSpecial);
		GameServer()->Console()->Print(IConsole::OUTPUT_LEVEL_DEBUG, "game", aBuf);
	}

	// send the kill message
	CNetMsg_Sv_KillMsg Msg;
	Msg.m_Victim = m_pPlayer->GetCID();
	Msg.m_ModeSpecial = ModeSpecial;
	for(int i = 0; i < SERVER_MAX_CLIENTS; i++)
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

	// this is for auto respawn after 3 secs
	m_pPlayer->m_DieTick = Server()->Tick();

	CHealthEntity::Die(pKiller, Weapon);
	GameWorld()->m_Core.m_apCharacters[m_pPlayer->GetCID()] = 0;
	GameServer()->CreateDeath(m_Pos, m_pPlayer->GetCID());
}

void CCharacter::Snap(int SnappingClient)
{
	if(NetworkClipped(SnappingClient))
		return;

	CNetObj_Character *pCharacter = static_cast<CNetObj_Character *>(Server()->SnapNewItem(NETOBJTYPE_CHARACTER, m_pPlayer->GetCID(), sizeof(CNetObj_Character)));
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

	// set emote
	if(m_EmoteStop < Server()->Tick())
	{
		SetEmote(EMOTE_NORMAL, -1);
	}

	pCharacter->m_Emote = m_EmoteType;

	pCharacter->m_AmmoCount = 0;
	pCharacter->m_Health = 0;
	pCharacter->m_Armor = 0;
	pCharacter->m_TriggeredEvents = m_TriggeredEvents;

	pCharacter->m_Weapon = !m_aWeapons[m_ActiveWeapon].m_Got ? -1 : WeaponManager()->GetWeapon(m_aWeapons[m_ActiveWeapon].m_Weapon)->SnapStyle();
	pCharacter->m_AttackTick = m_AttackTick;

	pCharacter->m_Direction = m_Input.m_Direction;

	if(m_pPlayer->GetCID() == SnappingClient || SnappingClient == -1 ||
		(!Config()->m_SvStrictSpectateMode && m_pPlayer->GetCID() == GameServer()->m_apPlayers[SnappingClient]->GetSpectatorID()))
	{
		pCharacter->m_Health = clamp(round_to_int(GetHealth() / (float) GetMaxHealth() * 10), 0, 10);
		pCharacter->m_Armor = clamp(round_to_int(GetArmor() / (float) GetMaxArmor() * 10), 0, 10);
		if(m_ActiveWeapon == WEAPON_NINJA)
			pCharacter->m_AmmoCount = m_Ninja.m_ActivationTick + g_pData->m_Weapons.m_Ninja.m_Duration * Server()->TickSpeed() / 1000;
		else if(m_aWeapons[m_ActiveWeapon].m_Ammo > 0)
			pCharacter->m_AmmoCount = round_to_int((m_aWeapons[m_ActiveWeapon].m_Ammo / static_cast<float>(WeaponManager()->GetWeapon(m_aWeapons[m_ActiveWeapon].m_Weapon)->MaxAmmo())) * 10);
	}

	if(pCharacter->m_Emote == EMOTE_NORMAL)
	{
		if(5 * Server()->TickSpeed() - ((Server()->Tick() - m_LastAction) % (5 * Server()->TickSpeed())) < 5)
			pCharacter->m_Emote = EMOTE_BLINK;
	}
}

void CCharacter::PostSnap()
{
	m_TriggeredEvents = 0;
}

int CCharacter::GetCID() const
{
	return m_pPlayer->GetCID();
}

vec2 CCharacter::GetVel() const
{
	return m_Core.m_Vel;
}

void CCharacter::SetVel(vec2 Vel)
{
	m_Core.m_Vel = Vel;
}
