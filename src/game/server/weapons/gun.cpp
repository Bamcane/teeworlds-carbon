#include <game/server/entities/projectile.h>
#include <game/server/entity.h>
#include <game/server/gamecontext.h>
#include <game/server/gameworld.h>
#include <game/server/weapons.h>
#include <generated/server_data.h>

class CGun : public IWeaponInterface
{
public:
	CGun() { WeaponManager()->RegisterWeapon("Gun", this); }

	//
	void OnFire(class CEntity *pFrom, class CGameWorld *pWorld, vec2 Pos, vec2 Direction, int *pReloadTimer = nullptr) override;
	const char *Name() override { return _("Gun"); }
	bool FullAuto() override { return false; }
	int FireDelay() override { return g_pData->m_Weapons.m_Gun.m_pBase->m_Firedelay; }
	int SnapStyle() override { return WEAPON_GUN; }

	// Ammo
	int AmmoRegenTime() override { return g_pData->m_Weapons.m_Gun.m_pBase->m_Ammoregentime; }
	int DefaultAmmo() override { return g_pData->m_Weapons.m_Gun.m_pBase->m_Maxammo; }
	int MaxAmmo() override { return g_pData->m_Weapons.m_Gun.m_pBase->m_Maxammo; }
};

void CGun::OnFire(CEntity *pFrom, CGameWorld *pWorld, vec2 Pos, vec2 Direction, int *pReloadTimer)
{
	new CProjectile(pWorld, WEAPON_GUN,
		pFrom,
		Pos,
		Direction,
		(int) (pWorld->Server()->TickSpeed() * pWorld->GameServer()->Tuning()->m_GunLifetime),
		g_pData->m_Weapons.m_Gun.m_pBase->m_Damage, false, 0, -1, WEAPON_GUN);

	pWorld->GameServer()->CreateSound(Pos, SOUND_GUN_FIRE);
}

static CGun gs_WeaponGun;
