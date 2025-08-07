#include <game/server/entities/projectile.h>
#include <game/server/entity.h>
#include <game/server/gamecontext.h>
#include <game/server/gameworld.h>
#include <game/server/weapons.h>
#include <generated/server_data.h>

class CGrenade : public IWeaponInterface
{
public:
	CGrenade() { WeaponManager()->RegisterWeapon("Grenade", this); }

	//
	void OnFire(class CEntity *pFrom, class CGameWorld *pWorld, vec2 Pos, vec2 Direction, int *pReloadTimer = nullptr) override;
    const char *Name() override {return _("Grenade"); }
	bool FullAuto() override { return true; }
	int FireDelay() override { return g_pData->m_Weapons.m_Grenade.m_pBase->m_Firedelay; }
	int SnapStyle() override { return WEAPON_GRENADE; }

	// Ammo
	int AmmoRegenTime() override { return g_pData->m_Weapons.m_Grenade.m_pBase->m_Ammoregentime; }
	int DefaultAmmo() override { return g_pData->m_Weapons.m_Grenade.m_pBase->m_Maxammo; }
	int MaxAmmo() override { return g_pData->m_Weapons.m_Grenade.m_pBase->m_Maxammo; }
};

void CGrenade::OnFire(CEntity *pFrom, CGameWorld *pWorld, vec2 Pos, vec2 Direction, int *pReloadTimer)
{
	new CProjectile(pWorld, WEAPON_GRENADE,
		pFrom,
		Pos,
		Direction,
		(int) (pWorld->Server()->TickSpeed() * pWorld->GameServer()->Tuning()->m_GrenadeLifetime),
		g_pData->m_Weapons.m_Grenade.m_pBase->m_Damage, true, 0, SOUND_GRENADE_EXPLODE, WEAPON_GRENADE);

	pWorld->GameServer()->CreateSound(Pos, SOUND_GRENADE_FIRE);
}

static CGrenade gs_WeaponGrenade;
