/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include <base/math.h>

#include <engine/shared/config.h>

#include <game/client/component.h>
#include <game/client/components/chat.h>
#include <game/client/components/menus.h>
#include <game/client/components/scoreboard.h>
#include <game/client/gameclient.h>
#include <game/collision.h>

#include "controls.h"

CControls::CControls()
{
	mem_zero(&m_LastData, sizeof(m_LastData));
}

void CControls::OnReset()
{
	m_LastData.m_Direction = 0;
	m_LastData.m_Hook = 0;
	// simulate releasing the fire button
	if((m_LastData.m_Fire & 1) != 0)
		m_LastData.m_Fire++;
	m_LastData.m_Fire &= INPUT_STATE_MASK;
	m_LastData.m_Jump = 0;
	m_InputData = m_LastData;

	m_InputDirectionLeft = 0;
	m_InputDirectionRight = 0;
}

void CControls::OnRelease()
{
	OnReset();
}

void CControls::OnPlayerDeath()
{
	if(!m_pClient->m_Snap.m_pGameDataRace || !(m_pClient->m_Snap.m_pGameDataRace->m_RaceFlags & RACEFLAG_KEEP_WANTED_WEAPON))
		m_LastData.m_WantedWeapon = m_InputData.m_WantedWeapon = 0;
}

static void ConKeyInputState(IConsole::IResult *pResult, void *pUserData)
{
	((int *) pUserData)[0] = pResult->GetInteger(0);
}

static void ConKeyInputCounter(IConsole::IResult *pResult, void *pUserData)
{
	int *v = (int *) pUserData;
	if(((*v) & 1) != pResult->GetInteger(0))
		(*v)++;
	*v &= INPUT_STATE_MASK;
}

struct CInputSet
{
	CControls *m_pControls;
	int *m_pVariable;
	int m_Value;
};

static void ConKeyInputSet(IConsole::IResult *pResult, void *pUserData)
{
	CInputSet *pSet = (CInputSet *) pUserData;
	if(pResult->GetInteger(0))
		*pSet->m_pVariable = pSet->m_Value;
}

static void ConKeyInputNextPrevWeapon(IConsole::IResult *pResult, void *pUserData)
{
	CInputSet *pSet = (CInputSet *) pUserData;
	ConKeyInputCounter(pResult, pSet->m_pVariable);
	pSet->m_pControls->m_InputData.m_WantedWeapon = 0;
}

void CControls::OnConsoleInit()
{
	// game commands
	Console()->Register("+left", "", CFGFLAG_CLIENT, ConKeyInputState, &m_InputDirectionLeft, "Move left");
	Console()->Register("+right", "", CFGFLAG_CLIENT, ConKeyInputState, &m_InputDirectionRight, "Move right");
	Console()->Register("+jump", "", CFGFLAG_CLIENT, ConKeyInputState, &m_InputData.m_Jump, "Jump");
	Console()->Register("+hook", "", CFGFLAG_CLIENT, ConKeyInputState, &m_InputData.m_Hook, "Hook");
	Console()->Register("+fire", "", CFGFLAG_CLIENT, ConKeyInputCounter, &m_InputData.m_Fire, "Fire");

	{
		static CInputSet s_Set = {this, &m_InputData.m_WantedWeapon, 1};
		Console()->Register("+weapon1", "", CFGFLAG_CLIENT, ConKeyInputSet, (void *) &s_Set, "Switch to hammer");
	}
	{
		static CInputSet s_Set = {this, &m_InputData.m_WantedWeapon, 2};
		Console()->Register("+weapon2", "", CFGFLAG_CLIENT, ConKeyInputSet, (void *) &s_Set, "Switch to gun");
	}
	{
		static CInputSet s_Set = {this, &m_InputData.m_WantedWeapon, 3};
		Console()->Register("+weapon3", "", CFGFLAG_CLIENT, ConKeyInputSet, (void *) &s_Set, "Switch to shotgun");
	}
	{
		static CInputSet s_Set = {this, &m_InputData.m_WantedWeapon, 4};
		Console()->Register("+weapon4", "", CFGFLAG_CLIENT, ConKeyInputSet, (void *) &s_Set, "Switch to grenade");
	}
	{
		static CInputSet s_Set = {this, &m_InputData.m_WantedWeapon, 5};
		Console()->Register("+weapon5", "", CFGFLAG_CLIENT, ConKeyInputSet, (void *) &s_Set, "Switch to laser");
	}

	{
		static CInputSet s_Set = {this, &m_InputData.m_NextWeapon, 0};
		Console()->Register("+nextweapon", "", CFGFLAG_CLIENT, ConKeyInputNextPrevWeapon, (void *) &s_Set, "Switch to next weapon");
	}
	{
		static CInputSet s_Set = {this, &m_InputData.m_PrevWeapon, 0};
		Console()->Register("+prevweapon", "", CFGFLAG_CLIENT, ConKeyInputNextPrevWeapon, (void *) &s_Set, "Switch to previous weapon");
	}
}

void CControls::OnMessage(int Msg, void *pRawMsg)
{
	if(Msg == NETMSGTYPE_SV_WEAPONPICKUP)
	{
		CNetMsg_Sv_WeaponPickup *pMsg = (CNetMsg_Sv_WeaponPickup *) pRawMsg;
		if(Config()->m_ClAutoswitchWeapons)
			m_InputData.m_WantedWeapon = pMsg->m_Weapon + 1;
	}
}

int CControls::SnapInput(int *pData)
{
	static int64 s_LastSendTime = 0;
	bool Send = false;

	// update player state
	if(m_pClient->m_pChat->IsActive())
		m_InputData.m_PlayerFlags = PLAYERFLAG_CHATTING;
	else
		m_InputData.m_PlayerFlags = 0;

	if(m_pClient->m_pScoreboard->IsActive())
		m_InputData.m_PlayerFlags |= PLAYERFLAG_SCOREBOARD;

	if(m_LastData.m_PlayerFlags != m_InputData.m_PlayerFlags)
		Send = true;

	m_LastData.m_PlayerFlags = m_InputData.m_PlayerFlags;

	// we freeze the input if chat or menu is activated
	if(m_pClient->m_pChat->IsActive() || m_pClient->m_pMenus->IsActive())
	{
		OnReset();

		mem_copy(pData, &m_InputData, sizeof(m_InputData));

		// send once a second just to be sure
		if(time_get() > s_LastSendTime + time_freq())
			Send = true;
	}
	else
	{
		m_InputData.m_TargetX = (int) m_MousePos.x;
		m_InputData.m_TargetY = (int) m_MousePos.y;
		if(!m_InputData.m_TargetX && !m_InputData.m_TargetY)
		{
			m_InputData.m_TargetX = 1;
			m_MousePos.x = 1;
		}

		// set direction
		m_InputData.m_Direction = 0;
		if(m_InputDirectionLeft && !m_InputDirectionRight)
			m_InputData.m_Direction = -1;
		if(!m_InputDirectionLeft && m_InputDirectionRight)
			m_InputData.m_Direction = 1;

		// stress testing
#ifdef CONF_DEBUG
		if(Config()->m_DbgStress)
		{
			float t = Client()->LocalTime();
			mem_zero(&m_InputData, sizeof(m_InputData));

			m_InputData.m_Direction = ((int) t / 2) % 3 - 1;
			m_InputData.m_Jump = ((int) t) & 1;
			m_InputData.m_Fire = ((int) (t * 10));
			m_InputData.m_Hook = ((int) (t * 2)) & 1;
			m_InputData.m_WantedWeapon = ((int) t) % NUM_WEAPONS;
			m_InputData.m_TargetX = (int) (sinf(t * 3) * 100.0f);
			m_InputData.m_TargetY = (int) (cosf(t * 3) * 100.0f);
		}
#endif

		// check if we need to send input
		if(m_InputData.m_Direction != m_LastData.m_Direction)
			Send = true;
		else if(m_InputData.m_Jump != m_LastData.m_Jump)
			Send = true;
		else if(m_InputData.m_Fire != m_LastData.m_Fire)
			Send = true;
		else if(m_InputData.m_Hook != m_LastData.m_Hook)
			Send = true;
		else if(m_InputData.m_WantedWeapon != m_LastData.m_WantedWeapon)
			Send = true;
		else if(m_InputData.m_NextWeapon != m_LastData.m_NextWeapon)
			Send = true;
		else if(m_InputData.m_PrevWeapon != m_LastData.m_PrevWeapon)
			Send = true;

		// send at at least 10hz
		if(time_get() > s_LastSendTime + time_freq() / 25)
			Send = true;
	}

	// copy and return size
	m_LastData = m_InputData;

	if(!Send)
		return 0;

	s_LastSendTime = time_get();
	mem_copy(pData, &m_InputData, sizeof(m_InputData));
	return sizeof(m_InputData);
}

void CControls::OnRender()
{
	ClampMousePos();

	// update target pos
	if(m_pClient->m_Snap.m_pGameData && !m_pClient->m_Snap.m_SpecInfo.m_Active)
		m_TargetPos = m_pClient->m_LocalCharacterPos + m_MousePos;
	else if(m_pClient->m_Snap.m_SpecInfo.m_Active && m_pClient->m_Snap.m_SpecInfo.m_UsePosition)
		m_TargetPos = m_pClient->m_Snap.m_SpecInfo.m_Position + m_MousePos;
	else
		m_TargetPos = m_MousePos;
}

bool CControls::OnCursorMove(float x, float y, int CursorType)
{
	if(m_pClient->IsWorldPaused() || (m_pClient->m_Snap.m_SpecInfo.m_Active && m_pClient->m_pChat->IsActive()))
		return false;

	if(CursorType == IInput::CURSOR_JOYSTICK && Config()->m_JoystickAbsolute && m_pClient->m_Snap.m_pGameData && !m_pClient->m_Snap.m_SpecInfo.m_Active)
	{
		float AbsX = 0.0f, AbsY = 0.0f;
		if(Input()->GetActiveJoystick()->Absolute(&AbsX, &AbsY))
			m_MousePos = vec2(AbsX, AbsY) * GetMaxMouseDistance();
		return true;
	}

	float Factor = 1.0f;
	switch(CursorType)
	{
	case IInput::CURSOR_MOUSE:
		Factor = Config()->m_InpMousesens / 100.0f;
		break;
	case IInput::CURSOR_JOYSTICK:
		Factor = Config()->m_JoystickSens / 100.0f;
		break;
	}

	m_MousePos += vec2(x, y) * Factor;
	return true;
}

void CControls::ClampMousePos()
{
	if(m_pClient->m_Snap.m_SpecInfo.m_Active && !m_pClient->m_Snap.m_SpecInfo.m_UsePosition)
	{
		m_MousePos.x = clamp(m_MousePos.x, 200.0f, Collision()->GetWidth() * 32 - 200.0f);
		m_MousePos.y = clamp(m_MousePos.y, 200.0f, Collision()->GetHeight() * 32 - 200.0f);
	}
	else
	{
		const float MouseMax = GetMaxMouseDistance();
		if(dot(m_MousePos, m_MousePos) > MouseMax * MouseMax)
			m_MousePos = normalize(m_MousePos) * MouseMax;
	}
}

float CControls::GetMaxMouseDistance() const
{
	if(Config()->m_ClDynamicCamera)
	{
		float CameraMaxDistance = 200.0f;
		float FollowFactor = Config()->m_ClMouseFollowfactor / 100.0f;
		return minimum(CameraMaxDistance / FollowFactor + Config()->m_ClMouseDeadzone, (float) Config()->m_ClMouseMaxDistanceDynamic);
	}
	else
		return (float) Config()->m_ClMouseMaxDistanceStatic;
}
