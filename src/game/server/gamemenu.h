#ifndef GAME_SERVER_GAMEMENU_H
#define GAME_SERVER_GAMEMENU_H

#include <base/uuid.h>
#include <game/voting.h>

#include <memory>
#include <vector>

#define MENU_MAIN_PAGE_UUID CalculateUuid("MAIN")
#define MENU_OPTIONS_NUM 12

struct SCallVoteStatus
{
	char m_aDesc[VOTE_DESC_LENGTH] = {'\0'};
	char m_aCmd[VOTE_CMD_LENGTH] = {'\0'};
	char m_aReason[VOTE_REASON_LENGTH] = {'\0'};
	bool m_Force = false; // admin
};

// return: true means that need to send vote msg
typedef bool (*MenuCallback)(int ClientID, SCallVoteStatus &VoteStatus, class CGameMenu *pMenu, void *pUserData);

struct SMenuPage
{
	Uuid m_Uuid = UUID_ZEROED;
	Uuid m_ParentUuid = UUID_ZEROED;
	MenuCallback m_pfnCallback = nullptr;
	char m_aTitle[VOTE_DESC_LENGTH] = {'\0'};
	void *m_pUserData = nullptr;
};

class CGameMenu
{
	class CGameContext *m_pGameServer;
	CGameContext *GameServer() const { return m_pGameServer; }
	class CConfig *Config() const;
	class IServer *Server() const;

public:
	CGameMenu(CGameContext *pGameServer);
	~CGameMenu() = default;

	void Register(const char *pPageName, const char *pTitle, MenuCallback pfnFunc, void *pUser, const char *pParent = "MAIN");

	void OnClientEntered(int ClientID);
	void OnMenuVote(int ClientID, SCallVoteStatus &VoteStatus, bool Sound = false);
	void SendMenuChat(int ClientID, const char *pChat);

	void ClearOptions(int ClientID);
	void SetPlayerPage(int ClientID, Uuid Page);
	void SetPlayerPage(int ClientID, const char *pPage);

	// generate menu
	void AddPageTitle();
	void AddSpace();
	void AddHorizontalRule();
	void AddOption(const char *pDesc, const char *pCommand, const char *pPrefix = "");
	void AddTranslatedOption(const char *pDesc, const char *pCommand, const char *pPrefix = "");

	const char *Localize(const char *pStr, const char *pContext = "");

private:
	int m_CurrentClientID;
	static bool MenuMain(int ClientID, SCallVoteStatus &VoteStatus, class CGameMenu *pMenu, void *pUserData);
	static bool MenuLanguage(int ClientID, SCallVoteStatus &VoteStatus, class CGameMenu *pMenu, void *pUserData);

	std::unordered_map<Uuid, std::shared_ptr<SMenuPage>> m_vpMenuPages;

	class CPlayerData
	{
	public:
		class CHeap *m_pVoteOptionHeap;
		CVoteOptionServer *m_pVoteOptionFirst;
		CVoteOptionServer *m_pVoteOptionLast;
		int m_NumVoteOptions;

		Uuid m_CurrentPage;
		char m_aMenuChat[48];

		void Reset(bool Clear = false)
		{
			if(Clear)
			{
				m_pVoteOptionFirst = nullptr;
				m_pVoteOptionLast = nullptr;
				m_NumVoteOptions = 0;
			}

			m_CurrentPage = MENU_MAIN_PAGE_UUID;
			m_aMenuChat[0] = '\0';
		}

		CPlayerData();
		~CPlayerData();
	};
	CPlayerData m_aPlayerData[SERVER_MAX_CLIENTS];
};

#endif
