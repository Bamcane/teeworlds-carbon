/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef GAME_CLIENT_COMPONENTS_EMOTICON_H
#define GAME_CLIENT_COMPONENTS_EMOTICON_H
#include <base/vmath.h>
#include <game/client/component.h>

class CEmoticon : public CComponent
{
	void DrawCircle(float x, float y, float r, int Segments);

	bool m_WasActive;
	bool m_Active;

	vec2 m_SelectorMouse;
	int m_SelectedEmote;

	static void ConKeyEmoticon(IConsole::IResult *pResult, void *pUserData);
	static void ConEmote(IConsole::IResult *pResult, void *pUserData);

public:
	CEmoticon();

	void OnReset() override;
	void OnConsoleInit() override;
	void OnRender() override;
	void OnRelease() override;
	bool OnCursorMove(float x, float y, int CursorType) override;

	void SendEmote(int Emoticon);
};

#endif
