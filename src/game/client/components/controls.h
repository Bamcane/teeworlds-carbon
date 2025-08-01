/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef GAME_CLIENT_COMPONENTS_CONTROLS_H
#define GAME_CLIENT_COMPONENTS_CONTROLS_H
#include <base/vmath.h>
#include <game/client/component.h>

class CControls : public CComponent
{
public:
	vec2 m_MousePos;
	vec2 m_TargetPos;

	CNetObj_PlayerInput m_InputData;
	CNetObj_PlayerInput m_LastData;
	int m_InputDirectionLeft;
	int m_InputDirectionRight;

	CControls();

	void OnReset() override;
	void OnRelease() override;
	void OnRender() override;
	void OnMessage(int MsgType, void *pRawMsg) override;
	bool OnCursorMove(float x, float y, int CursorType) override;
	void OnConsoleInit() override;
	void OnPlayerDeath();

	int SnapInput(int *pData);
	void ClampMousePos();
	float GetMaxMouseDistance() const;
};
#endif
