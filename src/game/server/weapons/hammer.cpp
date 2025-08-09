#include <game/server/entity.h>
#include <game/server/gamecontext.h>
#include <game/server/gameworld.h>
#include <game/server/weapons.h>
#include <generated/server_data.h>

class CHammer : public IWeaponInterface
{
public:
	CHammer() { WeaponManager()->RegisterWeapon("Hammer", this); }

	//
	void OnFire(class CEntity *pFrom, class CGameWorld *pWorld, vec2 Pos, vec2 Direction, int *pReloadTimer = nullptr) override;
	const char *Name() override { return _("Hammer"); }
	bool FullAuto() override { return false; }
	int FireDelay() override { return g_pData->m_Weapons.m_Hammer.m_pBase->m_Firedelay; }
	int SnapStyle() override { return WEAPON_HAMMER; }

	// Ammo
	int AmmoRegenTime() override { return g_pData->m_Weapons.m_Hammer.m_pBase->m_Ammoregentime; }
	int DefaultAmmo() override { return g_pData->m_Weapons.m_Hammer.m_pBase->m_Maxammo; }
	int MaxAmmo() override { return g_pData->m_Weapons.m_Hammer.m_pBase->m_Maxammo; }
};

void CHammer::OnFire(CEntity *pFrom, CGameWorld *pWorld, vec2 Pos, vec2 Direction, int *pReloadTimer)
{
	pWorld->GameServer()->CreateSound(Pos, SOUND_HAMMER_FIRE);

	float ProximityRadius = pFrom ? pFrom->GetProximityRadius() : 28.f;

	CBaseHealthEntity *apEnts[MAX_CHECK_ENTITY];
	int Hits = 0;
	int Num = pWorld->FindEntities(Pos, ProximityRadius * 0.5f, (CEntity **) apEnts,
		MAX_CHECK_ENTITY, EEntityFlag::ENTFLAG_DAMAGE);

	for(int i = 0; i < Num; ++i)
	{
		CBaseHealthEntity *pTarget = apEnts[i];

		if((pTarget == pFrom) || pWorld->GameServer()->Collision()->IntersectLine(Pos, pTarget->GetPos(), NULL, NULL))
			continue;

		// set his velocity to fast upward (for now)
		if(length(pTarget->GetPos() - Pos) > 0.0f)
			pWorld->GameServer()->CreateHammerHit(pTarget->GetPos() - normalize(pTarget->GetPos() - Pos) * ProximityRadius * 0.5f);
		else
			pWorld->GameServer()->CreateHammerHit(Pos);

		vec2 Dir;
		if(length(pTarget->GetPos() - (pFrom ? pFrom->GetPos() : Pos)) > 0.0f)
			Dir = normalize(pTarget->GetPos() - (pFrom ? pFrom->GetPos() : Pos));
		else
			Dir = vec2(0.f, -1.f);

		pTarget->TakeDamage(vec2(0.f, -1.f) + normalize(Dir + vec2(0.f, -1.1f)) * 10.0f, Dir * -1, g_pData->m_Weapons.m_Hammer.m_pBase->m_Damage,
			pFrom, WEAPON_HAMMER);
		Hits++;
	}

	// if we Hit anything, we have to wait for the reload
	if(Hits && *pReloadTimer)
		*pReloadTimer = pWorld->Server()->TickSpeed() / 3;
}

static CHammer gs_WeaponHammer;
