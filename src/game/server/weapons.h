#ifndef GAME_SERVER_WEAPONS_H
#define GAME_SERVER_WEAPONS_H

#include <base/vmath.h>
#include <base/uuid.h>
#include <condition_variable>

class IWeaponInterface
{
public:
	IWeaponInterface() {};

	//
	virtual void OnFire(class CEntity *pFrom, class CGameWorld *pWorld, vec2 Pos, vec2 Direction, int *pReloadTimer) = 0;
	virtual const char *Name() = 0;
	virtual bool FullAuto() = 0;
	virtual int FireDelay() = 0;
	virtual int SnapStyle() = 0;

	// Ammo
	virtual int AmmoRegenTime() = 0;
	virtual int DefaultAmmo() = 0;
	virtual int MaxAmmo() = 0;
};

class IWeaponManager
{
public:
	IWeaponManager() {};
	virtual ~IWeaponManager() {};

	virtual void RegisterWeapon(const char *pWeapon, IWeaponInterface *pClass) = 0;
	virtual void OutputRegisteredWeapons() = 0;

	virtual IWeaponInterface *GetWeapon(Uuid WeaponID) = 0;
};

extern IWeaponManager *WeaponManager();

#endif // GAME_SERVER_WEAPONS_H
