/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef ENGINE_CLIENT_SERVERBROWSER_H
#define ENGINE_CLIENT_SERVERBROWSER_H

#include <engine/serverbrowser.h>
#include "serverbrowser_entry.h"
#include "serverbrowser_fav.h"
#include "serverbrowser_filter.h"

class IServerBrowserHttp;

class CServerBrowser : public IServerBrowser
{
public:
	enum
	{
		SET_MASTER_ADD = 1,
		SET_FAV_ADD,
		SET_TOKEN,
	};

	CServerBrowser();
	void OnInitHttp();
	void Init(class CHttp *pHttp, class CNetClient *pClient, const char *pNetVersion);
	void Set(const NETADDR &Addr, int SetType, int Token, const CServerInfo *pInfo);
	void Update();

	// interface functions
	int GetType() override { return m_ActServerlistType; }
	void SetType(int Type) override;
	void Refresh(int RefreshFlags) override;
	bool IsRefreshing() const override { return m_pFirstReqServer != 0; }
	bool WasUpdated(bool Purge) override;
	int LoadingProgression() const override;
	void RequestResort() { m_NeedResort = true; }

	int NumServers() const override { return m_aServerlist[m_ActServerlistType].m_NumServers; }
	int NumPlayers() const override { return m_aServerlist[m_ActServerlistType].m_NumPlayers; }
	int NumClients() const override { return m_aServerlist[m_ActServerlistType].m_NumClients; }
	const CServerInfo *Get(int Index) const override { return &m_aServerlist[m_ActServerlistType].m_ppServerlist[Index]->m_Info; }

	int NumSortedServers(int FilterIndex) const override { return m_ServerBrowserFilter.GetNumSortedServers(FilterIndex); }
	int NumSortedPlayers(int FilterIndex) const override { return m_ServerBrowserFilter.GetNumSortedPlayers(FilterIndex); }
	const CServerInfo *SortedGet(int FilterIndex, int Index) const override { return &m_aServerlist[m_ActServerlistType].m_ppServerlist[m_ServerBrowserFilter.GetIndex(FilterIndex, Index)]->m_Info; }
	const void *GetID(int FilterIndex, int Index) const override { return m_ServerBrowserFilter.GetID(FilterIndex, Index); }

	void AddFavorite(const CServerInfo *pInfo) override;
	void RemoveFavorite(const CServerInfo *pInfo) override;
	void UpdateFavoriteState(CServerInfo *pInfo) override;
	void SetFavoritePassword(const char *pAddress, const char *pPassword) override;
	const char *GetFavoritePassword(const char *pAddress) override;

	int AddFilter(const CServerFilterInfo *pFilterInfo) override { return m_ServerBrowserFilter.AddFilter(pFilterInfo); }
	void SetFilter(int Index, const CServerFilterInfo *pFilterInfo) override { m_ServerBrowserFilter.SetFilter(Index, pFilterInfo); }
	void GetFilter(int Index, CServerFilterInfo *pFilterInfo) override { m_ServerBrowserFilter.GetFilter(Index, pFilterInfo); }
	void RemoveFilter(int Index) override { m_ServerBrowserFilter.RemoveFilter(Index); }

	static void CBFTrackPacket(int TrackID, void *pUser);

	void LoadServerlist();
	void SaveServerlist();

private:
	class IHttp *m_pHttpClient;
	class CNetClient *m_pNetClient;
	class CConfig *m_pConfig;
	class IConsole *m_pConsole;
	class IStorage *m_pStorage;

	class CServerBrowserFavorites m_ServerBrowserFavorites;
	class CServerBrowserFilter m_ServerBrowserFilter;

	class CConfig *Config() const { return m_pConfig; }
	class IConsole *Console() const { return m_pConsole; }
	class IStorage *Storage() const { return m_pStorage; }

	bool m_RefreshingHttp = false;
	IServerBrowserHttp *m_pHttp = nullptr;
	// serverlist
	int m_ActServerlistType;
	class CServerlist
	{
	public:
		class CHeap m_ServerlistHeap;

		int m_NumClients;
		int m_NumPlayers;
		int m_NumServers;
		int m_NumServerCapacity;

		CServerEntry *m_aServerlistIp[256]; // ip hash list
		CServerEntry **m_ppServerlist;

		~CServerlist();
		void Clear();
	} m_aServerlist[NUM_TYPES];

	CServerEntry *m_pFirstReqServer; // request list
	CServerEntry *m_pLastReqServer;
	int m_NumRequests;

	bool m_NeedRefresh;
	bool m_InfoUpdated;
	bool m_NeedResort;

	void UpdateFromHttp();
	// the token is to keep server refresh separated from each other
	int m_CurrentLanToken;

	int m_RefreshFlags;
	int64 m_BroadcastTime;

	CServerEntry *Add(int ServerlistType, const NETADDR &Addr);
	CServerEntry *Find(int ServerlistType, const NETADDR &Addr);
	void QueueRequest(CServerEntry *pEntry);
	void RemoveRequest(CServerEntry *pEntry);
	void RequestImpl(const NETADDR &Addr, CServerEntry *pEntry);
	void SetInfo(int ServerlistType, CServerEntry *pEntry, const CServerInfo &Info);
};

#endif
