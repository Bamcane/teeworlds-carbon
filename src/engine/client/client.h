/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef ENGINE_CLIENT_CLIENT_H
#define ENGINE_CLIENT_CLIENT_H

#include <base/hash.h>
#include <engine/shared/http.h>

class CGraph
{
	enum
	{
		MAX_VALUES = 128,
	};

	float m_Min, m_Max;
	float m_MinRange, m_MaxRange;
	float m_aValues[MAX_VALUES];
	float m_aColors[MAX_VALUES][3];
	int m_Index;

public:
	void Init(float Min, float Max);

	void Scale();
	void Add(float v, float r, float g, float b);
	void Render(IGraphics *pGraphics, IGraphics::CTextureHandle FontTexture, float x, float y, float w, float h, const char *pDescription);
};

class CSmoothTime
{
	int64 m_Snap;
	int64 m_Current;
	int64 m_Target;

	CGraph m_Graph;

	int m_SpikeCounter;
	int m_BadnessScore; // ranges between -100 (perfect) and MAX_INT

	float m_aAdjustSpeed[2]; // 0 = down, 1 = up
public:
	void Init(int64 Target);
	void SetAdjustSpeed(int Direction, float Value);

	int64 Get(int64 Now);
	inline int GetStabilityScore() const { return m_BadnessScore; }

	void UpdateInt(int64 Target);
	void Update(CGraph *pGraph, int64 Target, int TimeLeft, int AdjustDirection);
};

class CClient : public IClient, public CDemoPlayer::IListener
{
	// needed interfaces
	IEngine *m_pEngine;
	IEditor *m_pEditor;
	IEngineInput *m_pInput;
	IEngineGraphics *m_pGraphics;
	IEngineSound *m_pSound;
	IEngineTextRender *m_pTextRender;
	IGameClient *m_pGameClient;
	IEngineMap *m_pMap;
	IConfigManager *m_pConfigManager;
	CConfig *m_pConfig;
	IConsole *m_pConsole;
	IStorage *m_pStorage;
	CHttp m_Http;

	enum
	{
		NUM_SNAPSHOT_TYPES = 2,
		PREDICTION_MARGIN = 1000 / 50 / 2, // magic network prediction value
	};

	class CNetClient m_NetClient;
	class CNetClient m_ContactClient;
	class CDemoPlayer m_DemoPlayer;
	class CDemoRecorder m_DemoRecorder;
	class CServerBrowser m_ServerBrowser;
	class CFriends m_Friends;
	class CBlacklist m_Blacklist;

	char m_aServerAddressStr[256];
	char m_aServerPassword[128];

	unsigned m_SnapshotParts;
	int64 m_LocalStartTime;

	int64 m_LastRenderTime;
	int64 m_LastCpuTime;
	float m_LastAvgCpuFrameTime;
	float m_RenderFrameTimeLow;
	float m_RenderFrameTimeHigh;
	int m_RenderFrames;

	NETADDR m_ServerAddress;
	int m_WindowMustRefocus;
	int m_SnapCrcErrors;
	bool m_AutoScreenshotRecycle;
	bool m_AutoStatScreenshotRecycle;
	bool m_EditorActive;
	bool m_SoundInitFailed;
	bool m_RecordGameMessage;

	int m_AckGameTick;
	int m_CurrentRecvTick;
	int m_RconAuthed;
	int m_UseTempRconCommands;

	// version-checking
	char m_aVersionStr[10];

	// pinging
	int64 m_PingStartTime;

	//
	char m_aCurrentMap[256];
	char m_aCurrentMapPath[IO_MAX_PATH_LENGTH];
	SHA256_DIGEST m_CurrentMapSha256;
	unsigned m_CurrentMapCrc;

	//
	char m_aCmdConnect[256];

	// map download
	char m_aMapdownloadFilename[IO_MAX_PATH_LENGTH];
	char m_aMapdownloadFilenameTemp[IO_MAX_PATH_LENGTH];
	char m_aMapdownloadName[IO_MAX_PATH_LENGTH];
	IOHANDLE m_MapdownloadFileTemp;
	int m_MapdownloadChunk;
	int m_MapdownloadChunkNum;
	int m_MapDownloadChunkSize;
	SHA256_DIGEST m_MapdownloadSha256;
	bool m_MapdownloadSha256Present;
	int m_MapdownloadCrc;
	int m_MapdownloadAmount;
	int m_MapdownloadTotalsize;

	// time
	CSmoothTime m_GameTime;
	CSmoothTime m_PredictedTime;

	// input
	struct // TODO: handle input better
	{
		int m_aData[MAX_INPUT_SIZE]; // the input data
		int m_Tick; // the tick that the input is for
		int64 m_PredictedTime; // prediction latency when we sent this input
		int64 m_Time;
	} m_aInputs[200];

	int m_CurrentInput;

	// graphs
	CGraph m_InputtimeMarginGraph;
	CGraph m_GametimeMarginGraph;
	CGraph m_FpsGraph;

	// the game snapshots are modifiable by the game
	class CSnapshotStorage m_SnapshotStorage;
	CSnapshotStorage::CHolder *m_aSnapshots[NUM_SNAPSHOT_TYPES];

	int m_ReceivedSnapshots;
	char m_aSnapshotIncomingData[CSnapshot::MAX_SIZE];

	class CSnapshotStorage::CHolder m_aDemorecSnapshotHolders[NUM_SNAPSHOT_TYPES];
	char *m_aDemorecSnapshotData[NUM_SNAPSHOT_TYPES][2][CSnapshot::MAX_SIZE];
	class CSnapshotBuilder m_DemoRecSnapshotBuilder;

	class CSnapshotDelta m_SnapshotDelta;

	//
	class CServerInfo m_CurrentServerInfo;

	int64 TickStartTime(int Tick);

public:
	IEngine *Engine() { return m_pEngine; }
	IEngineGraphics *Graphics() { return m_pGraphics; }
	IEngineInput *Input() { return m_pInput; }
	IEngineSound *Sound() { return m_pSound; }
	IGameClient *GameClient() { return m_pGameClient; }
	IConfigManager *ConfigManager() { return m_pConfigManager; }
	CConfig *Config() { return m_pConfig; }
	IConsole *Console() { return m_pConsole; }
	IStorage *Storage() { return m_pStorage; }

	CClient();

	// ----- send functions -----
	int SendMsg(CMsgPacker *pMsg, int Flags) override;

	void SendInfo();
	void SendEnterGame();
	void SendReady();
	void SendInput();
	void SendRconAuth(const char *pName, const char *pPassword) override;
	void SendRcon(const char *pCmd) override;

	bool RconAuthed() const override { return m_RconAuthed != 0; }
	bool UseTempRconCommands() const override { return m_UseTempRconCommands != 0; }

	bool ConnectionProblems() const override;
	int GetInputtimeMarginStabilityScore() override;

	bool SoundInitFailed() const override { return m_SoundInitFailed; }

	// TODO: OPT: do this alot smarter!
	const int *GetInput(int Tick) const override;

	const char *LatestVersion() const override;
	void VersionUpdate();

	// ------ state handling -----
	void SetState(int s);

	// called when the map is loaded and we should init for a new round
	void OnEnterGame();
	void EnterGame() override;
	void OnClientOnline();

	void Connect(const char *pAddress) override;
	void DisconnectWithReason(const char *pReason);
	void Disconnect() override;
	const char *ServerAddress() const override { return m_aServerAddressStr; }

	void GetServerInfo(CServerInfo *pServerInfo) override;

	// ---

	const void *SnapGetItem(int SnapID, int Index, CSnapItem *pItem) const override;
	void SnapInvalidateItem(int SnapID, int Index) override;
	const void *SnapFindItem(int SnapID, int Type, int ID) const override;
	int SnapNumItems(int SnapID) const override;
	void *SnapNewItem(int Type, int ID, int Size) override;
	void SnapSetStaticsize(int ItemType, int Size) override;

	void Render();
	void DebugRender();

	void Quit() override;

	const char *ErrorString() const override;

	const char *LoadMap(const char *pName, const char *pFilename, const SHA256_DIGEST *pWantedSha256, unsigned WantedCrc);
	const char *LoadMapSearch(const char *pMapName, const SHA256_DIGEST *pWantedSha256, int WantedCrc);

	int UnpackServerInfo(CUnpacker *pUnpacker, CServerInfo *pInfo, int *pToken);
	void ProcessConnlessPacket(CNetChunk *pPacket);
	void ProcessServerPacket(CNetChunk *pPacket);

	const char *GetCurrentMapName() const override { return m_aCurrentMap; }
	const char *GetCurrentMapPath() const override { return m_aCurrentMapPath; }
	const char *MapDownloadName() const override { return m_aMapdownloadName; }
	int MapDownloadAmount() const override { return m_MapdownloadAmount; }
	int MapDownloadTotalsize() const override { return m_MapdownloadTotalsize; }

	void PumpNetwork();

	void OnDemoPlayerSnapshot(void *pData, int Size) override;
	void OnDemoPlayerMessage(void *pData, int Size) override;

	void Update();

	void RegisterInterfaces();
	void InitInterfaces();

	bool LimitFps();
	void Run();

	void ConnectOnStart(const char *pAddress);
	void DoVersionSpecificActions();

	static void Con_Connect(IConsole::IResult *pResult, void *pUserData);
	static void Con_Disconnect(IConsole::IResult *pResult, void *pUserData);
	static void Con_Quit(IConsole::IResult *pResult, void *pUserData);
	static void Con_Minimize(IConsole::IResult *pResult, void *pUserData);
	static void Con_Ping(IConsole::IResult *pResult, void *pUserData);
	static void Con_Screenshot(IConsole::IResult *pResult, void *pUserData);
	static void Con_Rcon(IConsole::IResult *pResult, void *pUserData);
	static void Con_RconAuth(IConsole::IResult *pResult, void *pUserData);
	static void Con_AddFavorite(IConsole::IResult *pResult, void *pUserData);
	static void Con_RemoveFavorite(IConsole::IResult *pResult, void *pUserData);
	static void Con_Record(IConsole::IResult *pResult, void *pUserData);
	static void Con_StopRecord(IConsole::IResult *pResult, void *pUserData);
	static void Con_AddDemoMarker(IConsole::IResult *pResult, void *pUserData);
	static void ConchainServerBrowserUpdate(IConsole::IResult *pResult, void *pUserData, IConsole::FCommandCallback pfnCallback, void *pCallbackUserData);
	static void ConchainFullscreen(IConsole::IResult *pResult, void *pUserData, IConsole::FCommandCallback pfnCallback, void *pCallbackUserData);
	static void ConchainWindowBordered(IConsole::IResult *pResult, void *pUserData, IConsole::FCommandCallback pfnCallback, void *pCallbackUserData);
	static void ConchainWindowScreen(IConsole::IResult *pResult, void *pUserData, IConsole::FCommandCallback pfnCallback, void *pCallbackUserData);
	static void ConchainWindowVSync(IConsole::IResult *pResult, void *pUserData, IConsole::FCommandCallback pfnCallback, void *pCallbackUserData);

	void RegisterCommands();

	const char *DemoPlayer_Play(const char *pFilename, int StorageType) override;
	void DemoRecorder_Start(const char *pFilename, bool WithTimestamp) override;
	void DemoRecorder_HandleAutoStart() override;
	void DemoRecorder_Stop(bool ErrorIfNotRecording = false) override;
	void DemoRecorder_AddDemoMarker();
	void RecordGameMessage(bool State) override { m_RecordGameMessage = State; }

	void AutoScreenshot_Start() override;
	void AutoStatScreenshot_Start() override;
	void AutoScreenshot_Cleanup();

	void ServerBrowserUpdate() override;

	// gfx
	void SwitchWindowScreen(int Index) override;
	bool ToggleFullscreen() override;
	void ToggleWindowBordered() override;
	void ToggleWindowVSync() override;
};
#endif
