/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef GAME_CLIENT_COMPONENTS_SCOREBOARD_H
#define GAME_CLIENT_COMPONENTS_SCOREBOARD_H
#include <game/client/component.h>

class CScoreboard : public CComponent
{
	void RenderGoals(float x, float y, float w);
	float RenderSpectators(float x, float y, float w);
	float RenderScoreboard(float x, float y, float w, int Team, const char *pTitle, int Align);
	void RenderRecordingNotification(float x, float w);
	void RenderNetworkQuality(float x, float w);

	static void ConKeyScoreboard(IConsole::IResult *pResult, void *pUserData);

	bool m_Active;
	bool m_Activate;
	class CUIRect m_TotalRect;

public:
	CScoreboard();
	void OnReset() override;
	void OnConsoleInit() override;
	void OnRender() override;
	void OnRelease() override;

	bool IsActive() const;
	void ResetPlayerStats(int ClientID);
	class CUIRect GetScoreboardRect() const { return m_TotalRect; }
	const char *GetClanName(int Team);
};

#endif
