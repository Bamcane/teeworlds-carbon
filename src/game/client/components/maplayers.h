/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef GAME_CLIENT_COMPONENTS_MAPLAYERS_H
#define GAME_CLIENT_COMPONENTS_MAPLAYERS_H
#include <base/tl/array.h>
#include <game/client/component.h>

class CMapLayers : public CComponent
{
	CLayers *m_pMenuLayers;
	IEngineMap *m_pMenuMap;

	int m_Type;
	float m_OnlineStartTime;

	array<CEnvPoint> m_lEnvPoints;
	array<CEnvPoint> m_lEnvPointsMenu;

	CTile *m_pEggTiles;
	int m_EggLayerWidth;
	int m_EggLayerHeight;

	static void EnvelopeEval(float TimeOffset, int Env, float *pChannels, void *pUser);

	void LoadEnvPoints(const CLayers *pLayers, array<CEnvPoint> &lEnvPoints);
	void LoadBackgroundMap();

	void PlaceEasterEggs(const CLayers *pLayers);

public:
	enum
	{
		TYPE_BACKGROUND = 0,
		TYPE_FOREGROUND,
	};

	CMapLayers(int Type);
	void OnStateChange(int NewState, int OldState) override;
	int GetInitAmount() const override;
	void OnInit() override;
	void OnShutdown() override;
	void OnRender() override;
	void OnMapLoad() override;

	static void ConchainBackgroundMap(IConsole::IResult *pResult, void *pUserData, IConsole::FCommandCallback pfnCallback, void *pCallbackUserData);

	void OnConsoleInit() override;

	void BackgroundMapUpdate();

	bool MenuMapLoaded() { return m_pMenuMap ? m_pMenuMap->IsLoaded() : false; }
};

#endif
