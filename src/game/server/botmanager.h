#ifndef GAME_SERVER_BOTMANAGER_H
#define GAME_SERVER_BOTMANAGER_H

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
	int m_aaBotIDMaps[MAX_CLIENTS][MAX_BOTS];

	std::vector<int> m_vMarkedAsDestroy;
	std::unordered_map<int, class CBotEntity *> m_vpBots;

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

	void CreateDamage(vec2 Pos, int BotID, vec2 Source, int HealthAmount, int ArmorAmount, bool Self);
	void CreateDeath(vec2 Pos, int BotID);

	int FindClientID(int ClientID, int BotID);

	void OnBotDeath(int BotID);
	void OnClientRefresh(int ClientID);

	void PostSnap();
};

#endif // GAME_SERVER_BOTMANAGER_H
