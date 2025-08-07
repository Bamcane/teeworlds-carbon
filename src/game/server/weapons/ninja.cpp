#include <game/server/entities/character.h>
#include <game/server/entity.h>
#include <game/server/gamecontext.h>
#include <game/server/gameworld.h>
#include <game/server/weapons.h>
#include <generated/server_data.h>

class CNinja : public IWeaponInterface
{
public:
	CNinja() { WeaponManager()->RegisterWeapon("Ninja", this); }

	//
	void OnFire(class CEntity *pFrom, class CGameWorld *pWorld, vec2 Pos, vec2 Direction, int *pReloadTimer = nullptr) override;
    const char *Name() override {return _("Ninja"); }
	bool FullAuto() override { return false; }
	int FireDelay() override { return g_pData->m_Weapons.m_Ninja.m_pBase->m_Firedelay; }
	int SnapStyle() override { return WEAPON_NINJA; }

	// Ammo
	int AmmoRegenTime() override { return g_pData->m_Weapons.m_Ninja.m_pBase->m_Ammoregentime; }
	int DefaultAmmo() override { return g_pData->m_Weapons.m_Ninja.m_pBase->m_Maxammo; }
	int MaxAmmo() override { return g_pData->m_Weapons.m_Ninja.m_pBase->m_Maxammo; }
};

void CNinja::OnFire(CEntity *pFrom, CGameWorld *pWorld, vec2 Pos, vec2 Direction, int *pReloadTimer)
{
    if(pFrom->GetObjType() != CGameWorld::ENTTYPE_CHARACTER)
        return;
    CCharacter *pCharacter = static_cast<CCharacter *>(pFrom);
    pCharacter->Ninja()->m_NumObjectsHit = 0;

    pCharacter->Ninja()->m_ActivationDir = Direction;
    pCharacter->Ninja()->m_CurrentMoveTime = g_pData->m_Weapons.m_Ninja.m_Movetime * pWorld->Server()->TickSpeed() / 1000;
    pCharacter->Ninja()->m_OldVelAmount = length(pCharacter->GetVel());

    pWorld->GameServer()->CreateSound(Pos, SOUND_NINJA_FIRE);
}

static CNinja gs_WeaponNinja;
