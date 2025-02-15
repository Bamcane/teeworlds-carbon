#ifndef GAME_SERVER_ENTITIES_BOTENTITY_H
#define GAME_SERVER_ENTITIES_BOTENTITY_H

#include <generated/protocol.h>

#include <game/gamecore.h>
#include <game/server/entity.h>
#include <game/server/teeinfo.h>

class CBotEntity : public CDamageEntity<CEntity>
{
public:
	// same as character's size
	static const int ms_PhysSize = 28;
	CBotEntity(CGameWorld *pWorld, vec2 Pos, int BotID, STeeInfo TeeInfos);
	~CBotEntity();

	void Tick() override;
    void TickDefered() override;
    void Snap(int SnappingClient) override;

    bool IsFriendlyDamage(CEntity *pFrom) override;
	bool TakeDamage(vec2 Force, vec2 Source, int Dmg, CEntity *pFrom, int Weapon) override;
	void Die(CEntity *pKiller, int Weapon) override;

    int GetBotID() const { return m_BotID; }
    STeeInfo *GetTeeInfos() { return &m_TeeInfos; }

private:
    STeeInfo m_TeeInfos;
    int m_BotID;

	int m_TriggeredEvents;
	// the core for the physics
	CCharacterCore m_Core;

	// info for dead reckoning
	int m_ReckoningTick; // tick that we are performing dead reckoning From
	CCharacterCore m_SendCore; // core that we should send
	CCharacterCore m_ReckoningCore; // the dead reckoning core
};

#endif
