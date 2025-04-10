#ifndef GAME_SERVER_BOTMANAGER_H
#define GAME_SERVER_BOTMANAGER_H

#include <base/uuid.h>
#include <base/vmath.h>
#include <engine/shared/protocol.h>

#include <unordered_map>
#include <vector>

class CGameContext;
class CGameController;
class CGameWorld;

class CBotManager
{
	CGameContext *m_pGameServer;
	class CWorldCore *m_pWorldCore;
	Uuid m_aaBotIDMaps[MAX_CLIENTS][MAX_BOTS];

	std::vector<Uuid> m_vMarkedAsDestroy;
	std::unordered_map<Uuid, class CBotEntity *> m_vpBots;

	void ClearPlayerMap(int ClientID);
	void UpdatePlayerMap(int ClientID);
	bool CreateBot();

public:
	class CConfig *Config() const;
	CGameContext *GameServer() const;
	CGameController *GameController() const;
	CGameWorld *GameWorld() const;
	class IServer *Server() const;

	class CWorldCore *BotWorldCore() const;

	CBotManager(CGameContext *pGameServer);
	~CBotManager();

	void Tick();

	void CreateDamage(vec2 Pos, Uuid BotID, vec2 Source, int HealthAmount, int ArmorAmount, bool Self);
	void CreateDeath(vec2 Pos, Uuid BotID);

	int FindClientID(int ClientID, Uuid BotID);

	void OnBotDeath(Uuid BotID);
	void OnClientRefresh(int ClientID);

	void PostSnap();
};

#endif // GAME_SERVER_BOTMANAGER_H
