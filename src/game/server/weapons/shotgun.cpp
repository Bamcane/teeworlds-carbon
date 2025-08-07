#include <game/server/entities/projectile.h>
#include <game/server/entity.h>
#include <game/server/gamecontext.h>
#include <game/server/gameworld.h>
#include <game/server/weapons.h>
#include <generated/server_data.h>

class CShotgun : public IWeaponInterface
{
public:
	CShotgun() { WeaponManager()->RegisterWeapon("Shotgun", this); }

	//
	void OnFire(class CEntity *pFrom, class CGameWorld *pWorld, vec2 Pos, vec2 Direction, int *pReloadTimer = nullptr) override;
    const char *Name() override {return _("Shotgun"); }
	bool FullAuto() override { return true; }
	int FireDelay() override { return g_pData->m_Weapons.m_Shotgun.m_pBase->m_Firedelay; }
	int SnapStyle() override { return WEAPON_SHOTGUN; }

	// Ammo
	int AmmoRegenTime() override { return g_pData->m_Weapons.m_Shotgun.m_pBase->m_Ammoregentime; }
	int DefaultAmmo() override { return g_pData->m_Weapons.m_Shotgun.m_pBase->m_Maxammo; }
	int MaxAmmo() override { return g_pData->m_Weapons.m_Shotgun.m_pBase->m_Maxammo; }
};

void CShotgun::OnFire(CEntity *pFrom, CGameWorld *pWorld, vec2 Pos, vec2 Direction, int *pReloadTimer)
{
	int ShotSpread = 2;

	for(int i = -ShotSpread; i <= ShotSpread; ++i)
	{
		float Spreading[] = {-0.185f, -0.070f, 0, 0.070f, 0.185f};
		float a = angle(Direction);
		a += Spreading[i + 2];
		float v = 1 - (absolute(i) / (float) ShotSpread);
		float Speed = mix((float) pWorld->GameServer()->Tuning()->m_ShotgunSpeeddiff, 1.0f, v);
		new CProjectile(pWorld, WEAPON_SHOTGUN,
			pFrom,
			Pos,
			vec2(cosf(a), sinf(a)) * Speed,
			(int) (pWorld->Server()->TickSpeed() * pWorld->GameServer()->Tuning()->m_ShotgunLifetime),
			g_pData->m_Weapons.m_Shotgun.m_pBase->m_Damage, false, 0, -1, WEAPON_SHOTGUN);
	}

	pWorld->GameServer()->CreateSound(Pos, SOUND_SHOTGUN_FIRE);
}

static CShotgun gs_WeaponShotgun;
