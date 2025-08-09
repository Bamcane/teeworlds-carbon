/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef GAME_SERVER_ENTITIES_CHARACTER_H
#define GAME_SERVER_ENTITIES_CHARACTER_H

#include <base/uuid.h>
#include <generated/protocol.h>

#include <game/gamecore.h>
#include <game/server/entity.h>

struct SNinja
{
	CEntity *m_apHitObjects[64];
	int m_NumObjectsHit;
	vec2 m_ActivationDir;
	int m_ActivationTick;
	int m_CurrentMoveTime;
	int m_OldVelAmount;
};

class CCharacter : public CHealthEntity<CEntity>
{
	MACRO_ALLOC_POOL_ID()

public:
	// character's size
	static const int ms_PhysSize = 28;

	CCharacter(CGameWorld *pWorld);

	void Reset() override;
	void Destroy() override;
	void Tick() override;
	void TickDefered() override;
	void TickPaused() override;
	void Snap(int SnappingClient) override;
	void PostSnap() override;

	bool IsGrounded();

	void SetWeapon(int W);
	void HandleWeaponSwitch();
	void DoWeaponSwitch();

	void HandleWeapons();
	void HandleNinja();

	void OnPredictedInput(CNetObj_PlayerInput *pNewInput);
	void OnDirectInput(CNetObj_PlayerInput *pNewInput);
	void ResetInput();
	void FireWeapon();

	bool IsFriendlyDamage(CEntity *pFrom) override;
	bool TakeDamage(vec2 Force, vec2 Source, int Dmg, CEntity *pFrom, int Weapon) override;
	void Die(CEntity *pKiller, int Weapon) override;

	bool Spawn(class CPlayer *pPlayer, vec2 Pos);

	bool GiveWeapon(Uuid WeaponID, int Ammo);
	void SetWeapon(int Place, Uuid WeaponID, int Ammo);
	void GiveNinja();

	void SetEmote(int Emote, int Tick);

	class CPlayer *GetPlayer() { return m_pPlayer; }

private:
	// player controlling this character
	class CPlayer *m_pPlayer;
	// weapon info
	struct WeaponStat
	{
		int m_AmmoRegenStart;
		int m_Ammo;
		Uuid m_Weapon;
		bool m_Got;

	} m_aWeapons[NUM_WEAPONS];

	int m_ActiveWeapon;
	int m_LastWeapon;
	int m_QueuedWeapon;

	int m_ReloadTimer;
	int m_AttackTick;

	int m_EmoteType;
	int m_EmoteStop;

	// last tick that the player took any action ie some input
	int m_LastAction;
	int m_LastNoAmmoSound;

	// these are non-heldback inputs
	CNetObj_PlayerInput m_LatestPrevInput;
	CNetObj_PlayerInput m_LatestInput;

	// input
	CNetObj_PlayerInput m_Input;
	int m_NumInputs;

	int m_TriggeredEvents;

	// ninja
	SNinja m_Ninja;

	// the player core for the physics
	CCharacterCore m_Core;

	// info for dead reckoning
	int m_ReckoningTick; // tick that we are performing dead reckoning From
	CCharacterCore m_SendCore; // core that we should send
	CCharacterCore m_ReckoningCore; // the dead reckoning core

	bool m_IsSitting;
	int m_HealthRegenStart;
	vec2 m_SitPos;

public:
	inline bool IsSitting() { return m_IsSitting; }
	void SetSitting(bool Flag) { m_IsSitting = Flag; }
	void SetSitPos(vec2 Pos) { m_SitPos = Pos; }
	int GetCID() const;

	SNinja *Ninja() { return &m_Ninja; }

	vec2 GetVel() const;
	void SetVel(vec2 Vel);
};

#endif
