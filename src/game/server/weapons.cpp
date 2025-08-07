#include <base/system.h>
#include <base/uuid.h>

#include "weapons.h"

#include <unordered_map>

class CWeaponHand : public IWeaponInterface
{
public:
	CWeaponHand() = default;

	//
	void OnFire(class CEntity *pFrom, class CGameWorld *pWorld, vec2 Pos, vec2 Direction, int *pReloadTimer = nullptr) {};
    const char *Name() override {return _("Hand"); }
	bool FullAuto() override { return false; }
	int FireDelay() override { return 0; }
	int SnapStyle() override { return -1; }

	// Ammo
	int AmmoRegenTime() override { return 0; }
	int DefaultAmmo() override { return 0; }
	int MaxAmmo() override { return 0; }
};

static CWeaponHand gs_WeaponHand;

class CWeaponManager : public IWeaponManager
{
	std::unordered_map<Uuid, IWeaponInterface *> m_upWeapons;

public:
	CWeaponManager();
	~CWeaponManager() override = default;

	void RegisterWeapon(const char *pWeapon, IWeaponInterface *pClass) override;
	void OutputRegisteredWeapons() override;

	IWeaponInterface *GetWeapon(Uuid WeaponID) override;
};

CWeaponManager::CWeaponManager()
{
	m_upWeapons.clear();
	RegisterWeapon("Hand", &gs_WeaponHand);
}

void CWeaponManager::RegisterWeapon(const char *pWeapon, IWeaponInterface *pClass)
{
	Uuid WeaponID = CalculateUuid(pWeapon);
	if(m_upWeapons.count(WeaponID))
		return; // weapon exists
	m_upWeapons[WeaponID] = pClass;
}

void CWeaponManager::OutputRegisteredWeapons()
{
	for(auto &[Uuid, pWeapon] : m_upWeapons)
	{
		char aUuid[UUID_MAXSTRSIZE];
		FormatUuid(Uuid, aUuid, sizeof(aUuid));
		dbg_msg("game/weapons", "registered weapon '%s' as uuid '%s'", pWeapon->Name(), aUuid);
	}
}

IWeaponInterface *CWeaponManager::GetWeapon(Uuid WeaponID)
{
	if(m_upWeapons.count(WeaponID))
		return m_upWeapons[WeaponID];
	return &gs_WeaponHand;
}

IWeaponManager *WeaponManager()
{
	static IWeaponManager *pWeaponManager = new CWeaponManager();
	return pWeaponManager;
}
