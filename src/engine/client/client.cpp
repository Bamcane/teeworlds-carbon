/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include <algorithm>
#include <new>

#include <stdarg.h>

#include <base/math.h>
#include <base/system.h>

#include <engine/client.h>
#include <engine/config.h>
#include <engine/console.h>
#include <engine/editor.h>
#include <engine/engine.h>
#include <engine/graphics.h>
#include <engine/http.h>
#include <engine/input.h>
#include <engine/keys.h>
#include <engine/map.h>
#include <engine/serverbrowser.h>
#include <engine/sound.h>
#include <engine/storage.h>
#include <engine/textrender.h>

#include <engine/shared/compression.h>
#include <engine/shared/config.h>
#include <engine/shared/datafile.h>
#include <engine/shared/demo.h>
#include <engine/shared/filecollection.h>
#include <engine/shared/masterserver.h>
#include <engine/shared/network.h>
#include <engine/shared/packer.h>
#include <engine/shared/protocol.h>
#include <engine/shared/ringbuffer.h>
#include <engine/shared/snapshot.h>

#include <game/version.h>

#include "contacts.h"
#include "serverbrowser.h"

#include "client.h"

#include <SDL3/SDL.h>
#ifdef main
#undef main
#endif

void CGraph::Init(float Min, float Max)
{
	m_MinRange = m_Min = Min;
	m_MaxRange = m_Max = Max;
	m_Index = 0;
}

void CGraph::Scale()
{
	m_Min = m_MinRange;
	m_Max = m_MaxRange;
	for(int i = 0; i < MAX_VALUES; i++)
	{
		if(m_aValues[i] > m_Max)
			m_Max = m_aValues[i];
		else if(m_aValues[i] < m_Min)
			m_Min = m_aValues[i];
	}
}

void CGraph::Add(float v, float r, float g, float b)
{
	m_Index = (m_Index + 1) % MAX_VALUES;
	m_aValues[m_Index] = v;
	m_aColors[m_Index][0] = r;
	m_aColors[m_Index][1] = g;
	m_aColors[m_Index][2] = b;
}

void CGraph::Render(IGraphics *pGraphics, IGraphics::CTextureHandle FontTexture, float x, float y, float w, float h, const char *pDescription)
{
	// m_pGraphics->BlendNormal();

	pGraphics->TextureClear();

	pGraphics->QuadsBegin();
	pGraphics->SetColor(0, 0, 0, 0.75f);
	IGraphics::CQuadItem QuadItem(x, y, w, h);
	pGraphics->QuadsDrawTL(&QuadItem, 1);
	pGraphics->QuadsEnd();

	pGraphics->LinesBegin();
	pGraphics->SetColor(0.95f, 0.95f, 0.95f, 1.00f);
	IGraphics::CLineItem LineItem(x, y + h / 2, x + w, y + h / 2);
	pGraphics->LinesDraw(&LineItem, 1);
	pGraphics->SetColor(0.5f, 0.5f, 0.5f, 0.75f);
	IGraphics::CLineItem aLineItems[2] = {
		IGraphics::CLineItem(x, y + (h * 3) / 4, x + w, y + (h * 3) / 4),
		IGraphics::CLineItem(x, y + h / 4, x + w, y + h / 4)};
	pGraphics->LinesDraw(aLineItems, 2);
	for(int i = 1; i < MAX_VALUES; i++)
	{
		float a0 = (i - 1) / (float) MAX_VALUES;
		float a1 = i / (float) MAX_VALUES;
		int i0 = (m_Index + i - 1) % MAX_VALUES;
		int i1 = (m_Index + i) % MAX_VALUES;

		float v0 = (m_aValues[i0] - m_Min) / (m_Max - m_Min);
		float v1 = (m_aValues[i1] - m_Min) / (m_Max - m_Min);

		IGraphics::CColorVertex aColorVertices[2] = {
			IGraphics::CColorVertex(0, m_aColors[i0][0], m_aColors[i0][1], m_aColors[i0][2], 0.75f),
			IGraphics::CColorVertex(1, m_aColors[i1][0], m_aColors[i1][1], m_aColors[i1][2], 0.75f)};
		pGraphics->SetColorVertex(aColorVertices, 2);
		IGraphics::CLineItem LineItem(x + a0 * w, y + h - v0 * h, x + a1 * w, y + h - v1 * h);
		pGraphics->LinesDraw(&LineItem, 1);
	}
	pGraphics->LinesEnd();

	pGraphics->TextureSet(FontTexture);
	pGraphics->QuadsBegin();
	pGraphics->QuadsText(x + 2, y + h - 16, 16, pDescription);

	char aBuf[32];
	str_format(aBuf, sizeof(aBuf), "%.2f", m_Max);
	pGraphics->QuadsText(x + w - 8 * str_length(aBuf) - 8, y + 2, 16, aBuf);

	str_format(aBuf, sizeof(aBuf), "%.2f", m_Min);
	pGraphics->QuadsText(x + w - 8 * str_length(aBuf) - 8, y + h - 16, 16, aBuf);
	pGraphics->QuadsEnd();
}

void CSmoothTime::Init(int64 Target)
{
	m_Snap = time_get();
	m_Current = Target;
	m_Target = Target;
	m_aAdjustSpeed[0] = 0.3f;
	m_aAdjustSpeed[1] = 0.3f;
	m_Graph.Init(0.0f, 0.5f);
	m_SpikeCounter = 0;
	m_BadnessScore = -100;
}

void CSmoothTime::SetAdjustSpeed(int Direction, float Value)
{
	m_aAdjustSpeed[Direction] = Value;
}

int64 CSmoothTime::Get(int64 Now)
{
	int64 c = m_Current + (Now - m_Snap);
	int64 t = m_Target + (Now - m_Snap);

	// it's faster to adjust upward instead of downward
	// we might need to adjust these abit

	float AdjustSpeed = m_aAdjustSpeed[0];
	if(t > c)
		AdjustSpeed = m_aAdjustSpeed[1];

	float a = ((Now - m_Snap) / (float) time_freq()) * AdjustSpeed;
	if(a > 1.0f)
		a = 1.0f;

	int64 r = c + (int64) ((t - c) * a);

	m_Graph.Add(a + 0.5f, 1, 1, 1);

	return r;
}

void CSmoothTime::UpdateInt(int64 Target)
{
	int64 Now = time_get();
	m_Current = Get(Now);
	m_Snap = Now;
	m_Target = Target;
}

void CSmoothTime::Update(CGraph *pGraph, int64 Target, int TimeLeft, int AdjustDirection)
{
	int UpdateTimer = 1;

	if(TimeLeft < 0)
	{
		int IsSpike = 0;
		if(TimeLeft < -50)
		{
			IsSpike = 1;

			m_SpikeCounter += 5;
			if(m_SpikeCounter > 50)
				m_SpikeCounter = 50;
		}

		if(IsSpike && m_SpikeCounter < 15)
		{
			// ignore this ping spike
			UpdateTimer = 0;
			pGraph->Add(TimeLeft, 1, 1, 0.3f); // yellow
			m_BadnessScore += 10;
		}
		else
		{
			pGraph->Add(TimeLeft, 1, 0.3f, 0.3f); // red
			m_BadnessScore += 50;
			if(m_aAdjustSpeed[AdjustDirection] < 30.0f)
				m_aAdjustSpeed[AdjustDirection] *= 2.0f;
		}
	}
	else
	{
		if(m_SpikeCounter)
			m_SpikeCounter--;

		pGraph->Add(TimeLeft, 0.3f, 1, 0.3f); // green

		m_aAdjustSpeed[AdjustDirection] *= 0.95f;
		if(m_aAdjustSpeed[AdjustDirection] < 2.0f)
			m_aAdjustSpeed[AdjustDirection] = 2.0f;
	}

	if(UpdateTimer)
		UpdateInt(Target);

	m_BadnessScore -= 1 + m_BadnessScore / 100;
}

CClient::CClient() :
	m_DemoPlayer(&m_SnapshotDelta), m_DemoRecorder(&m_SnapshotDelta)
{
	m_pEditor = 0;
	m_pInput = 0;
	m_pGraphics = 0;
	m_pSound = 0;
	m_pGameClient = 0;
	m_pMap = 0;
	m_pConfigManager = 0;
	m_pConfig = 0;
	m_pConsole = 0;

	m_RenderFrameTime = 0.0001f;
	m_RenderFrameTimeLow = 1.0f;
	m_RenderFrameTimeHigh = 0.0f;
	m_RenderFrames = 0;
	m_LastRenderTime = time_get();
	m_LastCpuTime = time_get();
	m_LastAvgCpuFrameTime = 0;

	m_GameTickSpeed = SERVER_TICK_SPEED;

	m_WindowMustRefocus = 0;
	m_SnapCrcErrors = 0;
	m_AutoScreenshotRecycle = false;
	m_AutoStatScreenshotRecycle = false;
	m_EditorActive = false;

	m_AckGameTick = -1;
	m_CurrentRecvTick = 0;
	m_RconAuthed = 0;

	// version-checking
	m_aVersionStr[0] = '0';
	m_aVersionStr[1] = 0;

	// pinging
	m_PingStartTime = 0;

	//
	m_aCurrentMap[0] = 0;
	m_CurrentMapSha256 = SHA256_ZEROED;
	m_CurrentMapCrc = 0;

	//
	m_aCmdConnect[0] = 0;

	// map download
	m_aMapdownloadFilename[0] = 0;
	m_aMapdownloadFilenameTemp[0] = 0;
	m_aMapdownloadName[0] = 0;
	m_MapdownloadFileTemp = 0;
	m_MapdownloadChunk = 0;
	m_MapdownloadSha256 = SHA256_ZEROED;
	m_MapdownloadSha256Present = false;
	m_MapdownloadCrc = 0;
	m_MapdownloadAmount = -1;
	m_MapdownloadTotalsize = -1;

	m_CurrentInput = 0;

	m_State = IClient::STATE_OFFLINE;
	m_aServerAddressStr[0] = 0;
	m_aServerPassword[0] = 0;

	mem_zero(m_aSnapshots, sizeof(m_aSnapshots));
	m_SnapshotStorage.Init();
	m_ReceivedSnapshots = 0;
}

// ----- send functions -----
int CClient::SendMsg(CMsgPacker *pMsg, int Flags)
{
	CNetChunk Packet;

	if(State() == IClient::STATE_OFFLINE)
		return 0;

	mem_zero(&Packet, sizeof(CNetChunk));
	Packet.m_ClientID = 0;
	Packet.m_pData = pMsg->Data();
	Packet.m_DataSize = pMsg->Size();

	if(Flags & MSGFLAG_VITAL)
		Packet.m_Flags |= NETSENDFLAG_VITAL;
	if(Flags & MSGFLAG_FLUSH)
		Packet.m_Flags |= NETSENDFLAG_FLUSH;

	if(Flags & MSGFLAG_RECORD)
	{
		if(m_DemoRecorder.IsRecording())
			m_DemoRecorder.RecordMessage(Packet.m_pData, Packet.m_DataSize);
	}

	if(!(Flags & MSGFLAG_NOSEND))
		m_NetClient.Send(&Packet);
	return 0;
}

void CClient::SendInfo()
{
	// restore password of favorite if possible
	const char *pPassword = m_ServerBrowser.GetFavoritePassword(m_aServerAddressStr);
	if(!pPassword)
		pPassword = Config()->m_Password;
	str_copy(m_aServerPassword, pPassword, sizeof(m_aServerPassword));

	CMsgPacker Msg(NETMSG_INFO, true);
	Msg.AddString(GameClient()->NetVersion(), 128);
	Msg.AddString(m_aServerPassword, 128);
	Msg.AddInt(GameClient()->ClientVersion());
	SendMsg(&Msg, MSGFLAG_VITAL | MSGFLAG_FLUSH);
}

void CClient::SendEnterGame()
{
	CMsgPacker Msg(NETMSG_ENTERGAME, true);
	SendMsg(&Msg, MSGFLAG_VITAL | MSGFLAG_FLUSH);
}

void CClient::SendReady()
{
	CMsgPacker Msg(NETMSG_READY, true);
	SendMsg(&Msg, MSGFLAG_VITAL | MSGFLAG_FLUSH);
}

void CClient::SendRconAuth(const char *pName, const char *pPassword)
{
	if(RconAuthed())
		return;

	CMsgPacker Msg(NETMSG_RCON_AUTH, true);
	Msg.AddString(pPassword, 32);
	SendMsg(&Msg, MSGFLAG_VITAL);
}

void CClient::SendRcon(const char *pCmd)
{
	CMsgPacker Msg(NETMSG_RCON_CMD, true);
	Msg.AddString(pCmd, 256);
	SendMsg(&Msg, MSGFLAG_VITAL);
}

void CClient::SendInput()
{
	int64 Now = time_get();

	if(m_PredTick <= 0)
		return;

	// fetch input
	int Size = GameClient()->OnSnapInput(m_aInputs[m_CurrentInput].m_aData);

	if(!Size)
		return;

	// pack input
	CMsgPacker Msg(NETMSG_INPUT, true);
	Msg.AddInt(m_AckGameTick);
	Msg.AddInt(m_PredTick);
	Msg.AddInt(Size);

	m_aInputs[m_CurrentInput].m_Tick = m_PredTick;
	m_aInputs[m_CurrentInput].m_PredictedTime = m_PredictedTime.Get(Now);
	m_aInputs[m_CurrentInput].m_Time = Now;

	// pack it
	for(int i = 0; i < Size / 4; i++)
		Msg.AddInt(m_aInputs[m_CurrentInput].m_aData[i]);

	int PingCorrection = 0;
	int64 TagTime;
	if(m_SnapshotStorage.Get(m_AckGameTick, &TagTime, 0, 0) >= 0)
		PingCorrection = (int) (((Now - TagTime) * 1000) / time_freq());
	Msg.AddInt(PingCorrection);

	m_CurrentInput++;
	m_CurrentInput %= 200;

	SendMsg(&Msg, MSGFLAG_FLUSH);
}

const char *CClient::LatestVersion() const
{
	return m_aVersionStr;
}

bool CClient::ConnectionProblems() const
{
	return m_NetClient.GotProblems() != 0;
}

int CClient::GetInputtimeMarginStabilityScore()
{
	return m_PredictedTime.GetStabilityScore();
}

// TODO: OPT: do this alot smarter!
const int *CClient::GetInput(int Tick) const
{
	int Best = -1;
	for(int i = 0; i < 200; i++)
	{
		if(m_aInputs[i].m_Tick <= Tick && (Best == -1 || m_aInputs[Best].m_Tick < m_aInputs[i].m_Tick))
			Best = i;
	}

	if(Best != -1)
		return (const int *) m_aInputs[Best].m_aData;
	return 0;
}

// ------ state handling -----
void CClient::SetState(int s)
{
	if(m_State == IClient::STATE_QUITING)
		return;

	int Old = m_State;
	if(Config()->m_Debug)
	{
		char aBuf[128];
		str_format(aBuf, sizeof(aBuf), "state change. last=%d current=%d", m_State, s);
		m_pConsole->Print(IConsole::OUTPUT_LEVEL_DEBUG, "client", aBuf);
	}
	m_State = s;
	if(Old != s)
	{
		GameClient()->OnStateChange(m_State, Old);
		if(s == IClient::STATE_ONLINE)
			OnClientOnline();
	}
}

// called when the map is loaded and we should init for a new round
void CClient::OnEnterGame()
{
	// reset input
	int i;
	for(i = 0; i < 200; i++)
		m_aInputs[i].m_Tick = -1;
	m_CurrentInput = 0;

	// reset snapshots
	m_aSnapshots[SNAP_CURRENT] = 0;
	m_aSnapshots[SNAP_PREV] = 0;
	m_SnapshotStorage.PurgeAll();
	m_ReceivedSnapshots = 0;
	m_SnapshotParts = 0;
	m_PredTick = 0;
	m_CurrentRecvTick = 0;
	m_CurGameTick = 0;
	m_PrevGameTick = 0;
}

void CClient::EnterGame()
{
	if(State() == IClient::STATE_DEMOPLAYBACK)
		return;

	if(State() == IClient::STATE_ONLINE)
	{
		// Don't reset everything while already in game.
		return;
	}

	// now we will wait for two snapshots
	// to finish the connection
	SendEnterGame();
	OnEnterGame();
}

void CClient::OnClientOnline()
{
	DemoRecorder_HandleAutoStart();

	// store password and server as favorite if configured, if the server was password protected
	CServerInfo Info = {0};
	GetServerInfo(&Info);
	bool ShouldStorePassword = Config()->m_ClSaveServerPasswords == 2 || (Config()->m_ClSaveServerPasswords == 1 && Info.m_Favorite);
	if(m_aServerPassword[0] && ShouldStorePassword && (Info.m_Flags & IServerBrowser::FLAG_PASSWORD))
	{
		m_ServerBrowser.SetFavoritePassword(m_aServerAddressStr, m_aServerPassword);
	}
}

void CClient::Connect(const char *pAddress)
{
	char aBuf[512];
	int Port = 8303;

	Disconnect();

	str_copy(m_aServerAddressStr, pAddress, sizeof(m_aServerAddressStr));

	str_format(aBuf, sizeof(aBuf), "connecting to '%s'", m_aServerAddressStr);
	m_pConsole->Print(IConsole::OUTPUT_LEVEL_STANDARD, "client", aBuf);

	mem_zero(&m_CurrentServerInfo, sizeof(m_CurrentServerInfo));

	if(net_addr_from_str(&m_ServerAddress, m_aServerAddressStr) != 0 && net_host_lookup(m_aServerAddressStr, &m_ServerAddress, m_NetClient.NetType()) != 0)
	{
		str_format(aBuf, sizeof(aBuf), "could not find the address of %s, connecting to localhost", m_aServerAddressStr);
		m_pConsole->Print(IConsole::OUTPUT_LEVEL_STANDARD, "client", aBuf);
		net_host_lookup("localhost", &m_ServerAddress, m_NetClient.NetType());
	}

	m_RconAuthed = 0;
	m_UseTempRconCommands = 0;
	if(m_ServerAddress.port == 0)
		m_ServerAddress.port = Port;
	m_NetClient.Connect(&m_ServerAddress);
	SetState(IClient::STATE_CONNECTING);

	DemoRecorder_Stop();

	m_InputtimeMarginGraph.Init(-150.0f, 150.0f);
	m_GametimeMarginGraph.Init(-150.0f, 150.0f);
}

void CClient::DisconnectWithReason(const char *pReason)
{
	char aBuf[512];
	str_format(aBuf, sizeof(aBuf), "disconnecting. reason='%s'", pReason ? pReason : "unknown");
	m_pConsole->Print(IConsole::OUTPUT_LEVEL_STANDARD, "client", aBuf);

	// stop demo playback and recorder
	m_DemoPlayer.Stop();
	DemoRecorder_Stop();

	// reset password stored in favorites if it's invalid
	if(pReason && str_find_nocase(pReason, "password"))
	{
		const char *pPassword = m_ServerBrowser.GetFavoritePassword(m_aServerAddressStr);
		if(pPassword && str_comp(pPassword, m_aServerPassword) == 0)
			m_ServerBrowser.SetFavoritePassword(m_aServerAddressStr, 0);
	}

	//
	m_RconAuthed = 0;
	m_UseTempRconCommands = 0;
	m_pConsole->DeregisterTempAll();
	m_pConsole->DeregisterTempMapAll();
	m_NetClient.Disconnect(pReason);
	SetState(IClient::STATE_OFFLINE);
	m_pMap->Unload();

	// disable all downloads
	m_MapdownloadChunk = 0;
	if(m_MapdownloadFileTemp)
	{
		io_close(m_MapdownloadFileTemp);
		Storage()->RemoveFile(m_aMapdownloadFilenameTemp, IStorage::TYPE_SAVE);
	}
	m_MapdownloadFileTemp = 0;
	m_MapdownloadSha256 = SHA256_ZEROED;
	m_MapdownloadSha256Present = false;
	m_MapdownloadCrc = 0;
	m_MapdownloadTotalsize = -1;
	m_MapdownloadAmount = 0;

	// clear the current server info
	mem_zero(&m_CurrentServerInfo, sizeof(m_CurrentServerInfo));
	mem_zero(&m_ServerAddress, sizeof(m_ServerAddress));
	m_aServerAddressStr[0] = 0;
	m_aServerPassword[0] = 0;

	// clear snapshots
	m_aSnapshots[SNAP_CURRENT] = 0;
	m_aSnapshots[SNAP_PREV] = 0;
	m_ReceivedSnapshots = 0;
}

void CClient::Disconnect()
{
	DisconnectWithReason(0);
}

void CClient::GetServerInfo(CServerInfo *pServerInfo)
{
	mem_copy(pServerInfo, &m_CurrentServerInfo, sizeof(m_CurrentServerInfo));
	m_ServerBrowser.UpdateFavoriteState(pServerInfo);
}

// ---

const void *CClient::SnapGetItem(int SnapID, int Index, CSnapItem *pItem) const
{
	dbg_assert(SnapID >= 0 && SnapID < NUM_SNAPSHOT_TYPES, "invalid SnapID");
	const CSnapshotItem *i = m_aSnapshots[SnapID]->m_pAltSnap->GetItem(Index);
	pItem->m_DataSize = m_aSnapshots[SnapID]->m_pAltSnap->GetItemSize(Index);
	pItem->m_Type = i->Type();
	pItem->m_ID = i->ID();
	return i->Data();
}

void CClient::SnapInvalidateItem(int SnapID, int Index)
{
	dbg_assert(SnapID >= 0 && SnapID < NUM_SNAPSHOT_TYPES, "invalid SnapID");
	const CSnapshotItem *pItem = m_aSnapshots[SnapID]->m_pAltSnap->GetItem(Index);
	if(pItem)
		m_aSnapshots[SnapID]->m_pAltSnap->InvalidateItem(Index);
}

const void *CClient::SnapFindItem(int SnapID, int Type, int ID) const
{
	if(!m_aSnapshots[SnapID])
		return 0x0;

	CSnapshot *pAltSnap = m_aSnapshots[SnapID]->m_pAltSnap;
	int Key = (Type << 16) | (ID & 0xffff);
	int Index = pAltSnap->GetItemIndex(Key);
	if(Index != -1)
		return pAltSnap->GetItem(Index)->Data();

	return 0x0;
}

int CClient::SnapNumItems(int SnapID) const
{
	dbg_assert(SnapID >= 0 && SnapID < NUM_SNAPSHOT_TYPES, "invalid SnapID");
	if(!m_aSnapshots[SnapID])
		return 0;
	return m_aSnapshots[SnapID]->m_pSnap->NumItems();
}

void *CClient::SnapNewItem(int Type, int ID, int Size)
{
	dbg_assert(Type >= 0 && Type <= 0xffff, "incorrect type");
	dbg_assert(ID >= 0 && ID <= 0xffff, "incorrect id");
	return ID < 0 ? 0 : m_DemoRecSnapshotBuilder.NewItem(Type, ID, Size);
}

void CClient::SnapSetStaticsize(int ItemType, int Size)
{
	m_SnapshotDelta.SetStaticsize(ItemType, Size);
}

void CClient::DebugRender()
{
	if(!Config()->m_Debug)
		return;

	static NETSTATS Prev, Current;
	static int64 LastSnap = 0;
	static float FrameTimeAvg = 0;
	static IGraphics::CTextureHandle s_Font = Graphics()->LoadTexture("ui/debug_font.png", IStorage::TYPE_ALL, CImageInfo::FORMAT_AUTO, IGraphics::TEXLOAD_NORESAMPLE);
	char aBuffer[256];
	int64 Now = time_get();

	// m_pGraphics->BlendNormal();
	Graphics()->TextureSet(s_Font);
	Graphics()->MapScreen(0, 0, Graphics()->ScreenWidth(), Graphics()->ScreenHeight());
	Graphics()->QuadsBegin();

	if(Now - LastSnap > time_freq())
	{
		LastSnap = Now;
		Prev = Current;
		net_stats(&Current);
	}

	/*
		eth = 14
		ip = 20
		udp = 8
		total = 42
	*/
	FrameTimeAvg = FrameTimeAvg * 0.9f + m_RenderFrameTime * 0.1f;
	str_format(aBuffer, sizeof(aBuffer), "ticks: %8d %8d gfxmem: %dk fps: %3d",
		m_CurGameTick, m_PredTick,
		Graphics()->MemoryUsage() / 1024,
		(int) (1.0f / FrameTimeAvg + 0.5f));
	Graphics()->QuadsText(2, 2, 16, aBuffer);

	{
		int SendPackets = (Current.sent_packets - Prev.sent_packets);
		int SendBytes = (Current.sent_bytes - Prev.sent_bytes);
		int SendTotal = SendBytes + SendPackets * 42;
		int RecvPackets = (Current.recv_packets - Prev.recv_packets);
		int RecvBytes = (Current.recv_bytes - Prev.recv_bytes);
		int RecvTotal = RecvBytes + RecvPackets * 42;

		if(!SendPackets)
			SendPackets++;
		if(!RecvPackets)
			RecvPackets++;
		str_format(aBuffer, sizeof(aBuffer), "send: %3d %5d+%4d=%5d (%3d kbps) avg: %5d\nrecv: %3d %5d+%4d=%5d (%3d kbps) avg: %5d",
			SendPackets, SendBytes, SendPackets * 42, SendTotal, (SendTotal * 8) / 1024, SendBytes / SendPackets,
			RecvPackets, RecvBytes, RecvPackets * 42, RecvTotal, (RecvTotal * 8) / 1024, RecvBytes / RecvPackets);
		Graphics()->QuadsText(2, 14, 16, aBuffer);
	}

	// render rates
	{
		int y = 0;
		for(int i = 0; i < 256; i++)
		{
			if(m_SnapshotDelta.GetDataRate(i))
			{
				str_format(aBuffer, sizeof(aBuffer), "%4d %20s: %8d %8d %8d", i, GameClient()->GetItemName(i), m_SnapshotDelta.GetDataRate(i) / 8, m_SnapshotDelta.GetDataUpdates(i),
					(m_SnapshotDelta.GetDataRate(i) / m_SnapshotDelta.GetDataUpdates(i)) / 8);
				Graphics()->QuadsText(2, 100 + y * 12, 16, aBuffer);
				y++;
			}
		}
	}

	str_format(aBuffer, sizeof(aBuffer), "pred: %d ms",
		(int) ((m_PredictedTime.Get(Now) - m_GameTime.Get(Now)) * 1000 / (float) time_freq()));
	Graphics()->QuadsText(2, 70, 16, aBuffer);
	Graphics()->QuadsEnd();

	// render graphs
	if(Config()->m_DbgGraphs)
	{
		float w = Graphics()->ScreenWidth() / 4.0f;
		float h = Graphics()->ScreenHeight() / 6.0f;
		float sp = Graphics()->ScreenWidth() / 100.0f;
		float x = Graphics()->ScreenWidth() - w - sp;

		m_FpsGraph.Scale();
		m_FpsGraph.Render(Graphics(), s_Font, x, sp * 5, w, h, "FPS");
		m_InputtimeMarginGraph.Scale();
		m_InputtimeMarginGraph.Render(Graphics(), s_Font, x, sp * 5 + h + sp, w, h, "Prediction Margin");
		m_GametimeMarginGraph.Scale();
		m_GametimeMarginGraph.Render(Graphics(), s_Font, x, sp * 5 + h + sp + h + sp, w, h, "Gametime Margin");
	}
}

void CClient::Quit()
{
	SetState(IClient::STATE_QUITING);
}

const char *CClient::ErrorString() const
{
	return m_NetClient.ErrorString();
}

void CClient::Render()
{
	if(m_EditorActive)
	{
		m_pEditor->OnRender();
	}
	else
	{
		GameClient()->OnRender();
	}
	DebugRender();
}

const char *CClient::LoadMap(const char *pName, const char *pFilename, const SHA256_DIGEST *pWantedSha256, unsigned WantedCrc)
{
	static char aErrorMsg[512];

	SetState(IClient::STATE_LOADING);

	if(!m_pMap->Load(pFilename))
	{
		str_format(aErrorMsg, sizeof(aErrorMsg), "map '%s' not found", pFilename);
		return aErrorMsg;
	}

	if(pWantedSha256 && m_pMap->Sha256() != *pWantedSha256)
	{
		char aSha256[SHA256_MAXSTRSIZE];
		char aWantedSha256[SHA256_MAXSTRSIZE];
		sha256_str(m_pMap->Sha256(), aSha256, sizeof(aSha256));
		sha256_str(*pWantedSha256, aWantedSha256, sizeof(aWantedSha256));
		str_format(aErrorMsg, sizeof(aErrorMsg), "map differs from the server. found = %s wanted = %s", aSha256, aWantedSha256);
		m_pConsole->Print(IConsole::OUTPUT_LEVEL_ADDINFO, "client", aErrorMsg);
		m_pMap->Unload();
		return aErrorMsg;
	}

	// get the crc of the map
	if(m_pMap->Crc() != WantedCrc)
	{
		str_format(aErrorMsg, sizeof(aErrorMsg), "map differs from the server. found = %08x wanted = %08x", m_pMap->Crc(), WantedCrc);
		m_pConsole->Print(IConsole::OUTPUT_LEVEL_ADDINFO, "client", aErrorMsg);
		m_pMap->Unload();
		return aErrorMsg;
	}

	// stop demo recording if we loaded a new map
	DemoRecorder_Stop();

	char aBuf[256];
	str_format(aBuf, sizeof(aBuf), "loaded map '%s'", pFilename);
	m_pConsole->Print(IConsole::OUTPUT_LEVEL_ADDINFO, "client", aBuf);
	m_ReceivedSnapshots = 0;

	str_copy(m_aCurrentMap, pName, sizeof(m_aCurrentMap));
	str_copy(m_aCurrentMapPath, pFilename, sizeof(m_aCurrentMapPath));
	m_CurrentMapSha256 = m_pMap->Sha256();
	m_CurrentMapCrc = m_pMap->Crc();

	return 0x0;
}

static void FormatMapDownloadFilename(const char *pName, const SHA256_DIGEST *pSha256, int Crc, bool Temp, char *pBuffer, int BufferSize)
{
	char aSuffix[32];
	if(Temp)
	{
		str_format(aSuffix, sizeof(aSuffix), ".%d.tmp", pid());
	}
	else
	{
		str_copy(aSuffix, ".map", sizeof(aSuffix));
	}

	if(pSha256)
	{
		char aSha256[SHA256_MAXSTRSIZE];
		sha256_str(*pSha256, aSha256, sizeof(aSha256));
		str_format(pBuffer, BufferSize, "downloadedmaps/%s_%s%s", pName, aSha256, aSuffix);
	}
	else
	{
		str_format(pBuffer, BufferSize, "downloadedmaps/%s_%08x%s", pName, Crc, aSuffix);
	}
}

const char *CClient::LoadMapSearch(const char *pMapName, const SHA256_DIGEST *pWantedSha256, int WantedCrc)
{
	const char *pError = 0;
	char aBuf[512];
	char aWanted[SHA256_MAXSTRSIZE + 16];
	aWanted[0] = 0;
	if(pWantedSha256)
	{
		char aWantedSha256[SHA256_MAXSTRSIZE];
		sha256_str(*pWantedSha256, aWantedSha256, sizeof(aWantedSha256));
		str_format(aWanted, sizeof(aWanted), "sha256=%s ", aWantedSha256);
	}
	str_format(aBuf, sizeof(aBuf), "loading map, map=%s wanted %scrc=%08x", pMapName, aWanted, WantedCrc);
	m_pConsole->Print(IConsole::OUTPUT_LEVEL_ADDINFO, "client", aBuf);
	SetState(IClient::STATE_LOADING);

	// try the normal maps folder
	str_format(aBuf, sizeof(aBuf), "maps/%s.map", pMapName);
	pError = LoadMap(pMapName, aBuf, pWantedSha256, WantedCrc);
	if(!pError)
		return pError;

	// try the downloaded maps
	FormatMapDownloadFilename(pMapName, pWantedSha256, WantedCrc, false, aBuf, sizeof(aBuf));
	pError = LoadMap(pMapName, aBuf, pWantedSha256, WantedCrc);
	if(!pError)
		return pError;

	// backward compatibility with old names
	if(pWantedSha256)
	{
		FormatMapDownloadFilename(pMapName, 0, WantedCrc, false, aBuf, sizeof(aBuf));
		pError = LoadMap(pMapName, aBuf, pWantedSha256, WantedCrc);
		if(!pError)
			return pError;
	}

	// search for the map within subfolders
	char aFilename[128];
	str_format(aFilename, sizeof(aFilename), "%s.map", pMapName);
	if(Storage()->FindFile(aFilename, "maps", IStorage::TYPE_ALL, aBuf, sizeof(aBuf)))
		pError = LoadMap(pMapName, aBuf, pWantedSha256, WantedCrc);

	return pError;
}

int CClient::UnpackServerInfo(CUnpacker *pUnpacker, CServerInfo *pInfo, int *pToken)
{
	if(pToken)
		*pToken = pUnpacker->GetInt();
	str_copy(pInfo->m_aVersion, pUnpacker->GetString(CUnpacker::SANITIZE_CC | CUnpacker::SKIP_START_WHITESPACES), sizeof(pInfo->m_aVersion));
	str_copy(pInfo->m_aName, pUnpacker->GetString(CUnpacker::SANITIZE_CC | CUnpacker::SKIP_START_WHITESPACES), sizeof(pInfo->m_aName));
	str_clean_whitespaces(pInfo->m_aName);
	str_copy(pInfo->m_aHostname, pUnpacker->GetString(CUnpacker::SANITIZE_CC | CUnpacker::SKIP_START_WHITESPACES), sizeof(pInfo->m_aHostname));
	if(pInfo->m_aHostname[0] == 0)
		str_copy(pInfo->m_aHostname, pInfo->m_aAddress, sizeof(pInfo->m_aHostname));
	str_copy(pInfo->m_aMap, pUnpacker->GetString(CUnpacker::SANITIZE_CC | CUnpacker::SKIP_START_WHITESPACES), sizeof(pInfo->m_aMap));
	str_copy(pInfo->m_aGameType, pUnpacker->GetString(CUnpacker::SANITIZE_CC | CUnpacker::SKIP_START_WHITESPACES), sizeof(pInfo->m_aGameType));
	int Flags = pUnpacker->GetInt();
	pInfo->m_Flags = 0;
	if(Flags & SERVERINFO_FLAG_PASSWORD)
		pInfo->m_Flags |= IServerBrowser::FLAG_PASSWORD;
	if(Flags & SERVERINFO_FLAG_TIMESCORE)
		pInfo->m_Flags |= IServerBrowser::FLAG_TIMESCORE;
	pInfo->m_ServerLevel = clamp<int>(pUnpacker->GetInt(), SERVERINFO_LEVEL_MIN, SERVERINFO_LEVEL_MAX);
	pInfo->m_NumPlayers = pUnpacker->GetInt();
	pInfo->m_MaxPlayers = pUnpacker->GetInt();
	pInfo->m_NumClients = pUnpacker->GetInt();
	pInfo->m_MaxClients = pUnpacker->GetInt();
	pInfo->m_NumBotPlayers = 0;
	pInfo->m_NumBotSpectators = 0;

	// don't add invalid info to the server browser list
	if(pInfo->m_NumClients < 0 || pInfo->m_NumClients > pInfo->m_MaxClients || pInfo->m_MaxClients < 0 || pInfo->m_MaxClients > MAX_CLIENTS || pInfo->m_MaxPlayers < pInfo->m_NumPlayers ||
		pInfo->m_NumPlayers < 0 || pInfo->m_NumPlayers > pInfo->m_NumClients || pInfo->m_MaxPlayers < 0 || pInfo->m_MaxPlayers > pInfo->m_MaxClients)
		return -1;
	// drop standard gametype with more than MAX_PLAYERS
	if(pInfo->m_MaxPlayers > 16 && (str_comp(pInfo->m_aGameType, "DM") == 0 || str_comp(pInfo->m_aGameType, "TDM") == 0 || str_comp(pInfo->m_aGameType, "CTF") == 0 ||
					       str_comp(pInfo->m_aGameType, "LTS") == 0 || str_comp(pInfo->m_aGameType, "LMS") == 0))
		return -1;

	// use short version
	if(!pToken)
		return 0;

	int NumPlayers = 0;
	int NumClients = 0;
	for(int i = 0; i < pInfo->m_NumClients; i++)
	{
		str_utf8_copy_num(pInfo->m_aClients[i].m_aName, pUnpacker->GetString(CUnpacker::SANITIZE_CC | CUnpacker::SKIP_START_WHITESPACES), sizeof(pInfo->m_aClients[i].m_aName), MAX_NAME_LENGTH);
		str_utf8_copy_num(pInfo->m_aClients[i].m_aClan, pUnpacker->GetString(CUnpacker::SANITIZE_CC | CUnpacker::SKIP_START_WHITESPACES), sizeof(pInfo->m_aClients[i].m_aClan), MAX_CLAN_LENGTH);
		pInfo->m_aClients[i].m_Country = pUnpacker->GetInt();
		pInfo->m_aClients[i].m_Score = pUnpacker->GetInt();
		pInfo->m_aClients[i].m_PlayerType = pUnpacker->GetInt() & CServerInfo::CClient::PLAYERFLAG_MASK;

		if(pInfo->m_aClients[i].m_PlayerType & CServerInfo::CClient::PLAYERFLAG_BOT)
		{
			if(pInfo->m_aClients[i].m_PlayerType & CServerInfo::CClient::PLAYERFLAG_SPEC)
				pInfo->m_NumBotSpectators++;
			else
				pInfo->m_NumBotPlayers++;
		}

		NumClients++;
		if(!(pInfo->m_aClients[i].m_PlayerType & CServerInfo::CClient::PLAYERFLAG_SPEC))
			NumPlayers++;
	}
	pInfo->m_NumPlayers = NumPlayers;
	pInfo->m_NumClients = NumClients;

	return 0;
}

bool CompareScore(const CServerInfo::CClient &C1, const CServerInfo::CClient &C2)
{
	if(C1.m_PlayerType & CServerInfo::CClient::PLAYERFLAG_SPEC)
		return false;
	if(C2.m_PlayerType & CServerInfo::CClient::PLAYERFLAG_SPEC)
		return true;
	return C1.m_Score > C2.m_Score;
}

bool CompareTime(const CServerInfo::CClient &C1, const CServerInfo::CClient &C2)
{
	if(C1.m_PlayerType & CServerInfo::CClient::PLAYERFLAG_SPEC)
		return false;
	if(C2.m_PlayerType & CServerInfo::CClient::PLAYERFLAG_SPEC)
		return true;
	if(C1.m_Score < 0)
		return false;
	if(C2.m_Score < 0)
		return true;
	return C1.m_Score < C2.m_Score;
}

inline void SortClients(CServerInfo *pInfo)
{
	std::stable_sort(pInfo->m_aClients, pInfo->m_aClients + pInfo->m_NumClients,
		(pInfo->m_Flags & IServerBrowser::FLAG_TIMESCORE) ? CompareTime : CompareScore);
}

void CClient::ProcessConnlessPacket(CNetChunk *pPacket)
{
	// server info
	if(pPacket->m_DataSize >= (int) sizeof(SERVERBROWSE_INFO) && mem_comp(pPacket->m_pData, SERVERBROWSE_INFO, sizeof(SERVERBROWSE_INFO)) == 0)
	{
		CUnpacker Up;
		CServerInfo Info = {0};
		Up.Reset((unsigned char *) pPacket->m_pData + sizeof(SERVERBROWSE_INFO), pPacket->m_DataSize - sizeof(SERVERBROWSE_INFO));
		net_addr_str(&pPacket->m_Address, Info.m_aAddress, sizeof(Info.m_aAddress), true);
		int Token;
		if(!UnpackServerInfo(&Up, &Info, &Token) && !Up.Error())
		{
			SortClients(&Info);
			m_ServerBrowser.Set(pPacket->m_Address, CServerBrowser::SET_TOKEN, Token, &Info);
		}
	}
}

void CClient::ProcessServerPacket(CNetChunk *pPacket)
{
	CMsgUnpacker Unpacker(pPacket->m_pData, pPacket->m_DataSize);
	if(Unpacker.Error())
		return;

	if(Unpacker.System())
	{
		// system message
		if((pPacket->m_Flags & NET_CHUNKFLAG_VITAL) != 0 && Unpacker.Type() == NETMSG_MAP_CHANGE)
		{
			const char *pMap = Unpacker.GetString(CUnpacker::SANITIZE_CC | CUnpacker::SKIP_START_WHITESPACES);
			int MapCrc = Unpacker.GetInt();
			int MapSize = Unpacker.GetInt();
			int MapChunkNum = Unpacker.GetInt();
			int MapChunkSize = Unpacker.GetInt();
			if(Unpacker.Error())
				return;
			const SHA256_DIGEST *pMapSha256 = (const SHA256_DIGEST *) Unpacker.GetRaw(sizeof(*pMapSha256));
			const char *pError = 0;

			// protect the player from nasty map names
			for(int i = 0; pMap[i]; i++)
			{
				if(pMap[i] == '/' || pMap[i] == '\\')
					pError = "strange character in map name";
			}

			if(MapSize <= 0)
				pError = "invalid map size";

			if(pError)
				DisconnectWithReason(pError);
			else
			{
				pError = LoadMapSearch(pMap, pMapSha256, MapCrc);

				if(!pError)
				{
					m_pConsole->Print(IConsole::OUTPUT_LEVEL_ADDINFO, "client/network", "loading done");
					SendReady();
				}
				else
				{
					if(m_MapdownloadFileTemp)
					{
						io_close(m_MapdownloadFileTemp);
						Storage()->RemoveFile(m_aMapdownloadFilenameTemp, IStorage::TYPE_SAVE);
					}

					// start map download
					FormatMapDownloadFilename(pMap, pMapSha256, MapCrc, false, m_aMapdownloadFilename, sizeof(m_aMapdownloadFilename));
					FormatMapDownloadFilename(pMap, pMapSha256, MapCrc, true, m_aMapdownloadFilenameTemp, sizeof(m_aMapdownloadFilenameTemp));

					char aBuf[256];
					str_format(aBuf, sizeof(aBuf), "starting to download map to '%s'", m_aMapdownloadFilenameTemp);
					m_pConsole->Print(IConsole::OUTPUT_LEVEL_ADDINFO, "client/network", aBuf);

					str_copy(m_aMapdownloadName, pMap, sizeof(m_aMapdownloadName));
					m_MapdownloadFileTemp = Storage()->OpenFile(m_aMapdownloadFilenameTemp, IOFLAG_WRITE, IStorage::TYPE_SAVE);
					m_MapdownloadChunk = 0;
					m_MapdownloadChunkNum = MapChunkNum;
					m_MapDownloadChunkSize = MapChunkSize;
					m_MapdownloadSha256 = pMapSha256 ? *pMapSha256 : SHA256_ZEROED;
					m_MapdownloadSha256Present = pMapSha256;
					m_MapdownloadCrc = MapCrc;
					m_MapdownloadTotalsize = MapSize;
					m_MapdownloadAmount = 0;

					// request first chunk package of map data
					CMsgPacker Msg(NETMSG_REQUEST_MAP_DATA, true);
					SendMsg(&Msg, MSGFLAG_VITAL | MSGFLAG_FLUSH);

					if(Config()->m_Debug)
						m_pConsole->Print(IConsole::OUTPUT_LEVEL_DEBUG, "client/network", "requested first chunk package");
				}
			}
		}
		else if((pPacket->m_Flags & NET_CHUNKFLAG_VITAL) != 0 && Unpacker.Type() == NETMSG_MAP_DATA)
		{
			if(!m_MapdownloadFileTemp)
				return;

			const int Size = minimum(m_MapDownloadChunkSize, m_MapdownloadTotalsize - m_MapdownloadAmount);
			if(Size <= 0)
				return;
			const unsigned char *pData = Unpacker.GetRaw(Size);
			if(Unpacker.Error())
				return;

			io_write(m_MapdownloadFileTemp, pData, Size);
			++m_MapdownloadChunk;
			m_MapdownloadAmount += Size;

			if(m_MapdownloadAmount == m_MapdownloadTotalsize)
			{
				// map download complete
				m_pConsole->Print(IConsole::OUTPUT_LEVEL_ADDINFO, "client/network", "download complete, loading map");

				if(m_MapdownloadFileTemp)
					io_close(m_MapdownloadFileTemp);
				m_MapdownloadFileTemp = 0;
				m_MapdownloadAmount = 0;
				m_MapdownloadTotalsize = -1;

				Storage()->RemoveFile(m_aMapdownloadFilename, IStorage::TYPE_SAVE);
				Storage()->RenameFile(m_aMapdownloadFilenameTemp, m_aMapdownloadFilename, IStorage::TYPE_SAVE);

				// load map
				const char *pError = LoadMap(m_aMapdownloadName, m_aMapdownloadFilename, m_MapdownloadSha256Present ? &m_MapdownloadSha256 : 0, m_MapdownloadCrc);
				if(!pError)
				{
					m_pConsole->Print(IConsole::OUTPUT_LEVEL_ADDINFO, "client/network", "loading done");
					SendReady();
				}
				else
					DisconnectWithReason(pError);
			}
			else if(m_MapdownloadChunk % m_MapdownloadChunkNum == 0)
			{
				// request next chunk package of map data
				CMsgPacker Msg(NETMSG_REQUEST_MAP_DATA, true);
				SendMsg(&Msg, MSGFLAG_VITAL | MSGFLAG_FLUSH);

				if(Config()->m_Debug)
					m_pConsole->Print(IConsole::OUTPUT_LEVEL_DEBUG, "client/network", "requested next chunk package");
			}
		}
		else if((pPacket->m_Flags & NET_CHUNKFLAG_VITAL) != 0 && Unpacker.Type() == NETMSG_SERVERINFO)
		{
			CServerInfo Info = {0};
			net_addr_str(&pPacket->m_Address, Info.m_aAddress, sizeof(Info.m_aAddress), true);
			if(!UnpackServerInfo(&Unpacker, &Info, 0) && !Unpacker.Error())
			{
				SortClients(&Info);
				mem_copy(&m_CurrentServerInfo, &Info, sizeof(m_CurrentServerInfo));
				m_CurrentServerInfo.m_NetAddr = m_ServerAddress;
			}
		}
		else if((pPacket->m_Flags & NET_CHUNKFLAG_VITAL) != 0 && Unpacker.Type() == NETMSG_CON_READY)
		{
			GameClient()->OnConnected();
		}
		else if(Unpacker.Type() == NETMSG_PING)
		{
			CMsgPacker Msg(NETMSG_PING_REPLY, true);
			SendMsg(&Msg, MSGFLAG_FLUSH);
		}
		else if((pPacket->m_Flags & NET_CHUNKFLAG_VITAL) != 0 && Unpacker.Type() == NETMSG_RCON_CMD_ADD)
		{
			const char *pName = Unpacker.GetString(CUnpacker::SANITIZE_CC);
			const char *pHelp = Unpacker.GetString(CUnpacker::SANITIZE_CC);
			const char *pParams = Unpacker.GetString(CUnpacker::SANITIZE_CC);
			if(Unpacker.Error() == 0)
				m_pConsole->RegisterTemp(pName, pParams, CFGFLAG_SERVER, pHelp);
		}
		else if((pPacket->m_Flags & NET_CHUNKFLAG_VITAL) != 0 && Unpacker.Type() == NETMSG_RCON_CMD_REM)
		{
			const char *pName = Unpacker.GetString(CUnpacker::SANITIZE_CC);
			if(Unpacker.Error() == 0)
				m_pConsole->DeregisterTemp(pName);
		}
		else if((pPacket->m_Flags & NET_CHUNKFLAG_VITAL) != 0 && Unpacker.Type() == NETMSG_MAPLIST_ENTRY_ADD)
		{
			const char *pName = Unpacker.GetString(CUnpacker::SANITIZE_CC);
			if(Unpacker.Error() == 0)
				m_pConsole->RegisterTempMap(pName);
		}
		else if((pPacket->m_Flags & NET_CHUNKFLAG_VITAL) != 0 && Unpacker.Type() == NETMSG_MAPLIST_ENTRY_REM)
		{
			const char *pName = Unpacker.GetString(CUnpacker::SANITIZE_CC);
			if(Unpacker.Error() == 0)
				m_pConsole->DeregisterTempMap(pName);
		}
		else if((pPacket->m_Flags & NET_CHUNKFLAG_VITAL) != 0 && Unpacker.Type() == NETMSG_RCON_AUTH_ON)
		{
			m_RconAuthed = 1;
			m_UseTempRconCommands = 1;
		}
		else if((pPacket->m_Flags & NET_CHUNKFLAG_VITAL) != 0 && Unpacker.Type() == NETMSG_RCON_AUTH_OFF)
		{
			m_RconAuthed = 0;
			if(m_UseTempRconCommands)
				m_pConsole->DeregisterTempAll();
			m_UseTempRconCommands = 0;
			m_pConsole->DeregisterTempMapAll();
		}
		else if((pPacket->m_Flags & NET_CHUNKFLAG_VITAL) != 0 && Unpacker.Type() == NETMSG_RCON_LINE)
		{
			const char *pLine = Unpacker.GetString();
			if(Unpacker.Error() == 0)
				GameClient()->OnRconLine(pLine);
		}
		else if(Unpacker.Type() == NETMSG_PING_REPLY)
		{
			char aBuf[256];
			str_format(aBuf, sizeof(aBuf), "latency %.2f", (time_get() - m_PingStartTime) * 1000 / (float) time_freq());
			m_pConsole->Print(IConsole::OUTPUT_LEVEL_STANDARD, "client/network", aBuf);
		}
		else if(Unpacker.Type() == NETMSG_INPUTTIMING)
		{
			int InputPredTick = Unpacker.GetInt();
			int TimeLeft = Unpacker.GetInt();

			// adjust our prediction time
			int64 Target = 0;
			for(int k = 0; k < 200; k++)
			{
				if(m_aInputs[k].m_Tick == InputPredTick)
				{
					Target = m_aInputs[k].m_PredictedTime + (time_get() - m_aInputs[k].m_Time);
					Target = Target - (int64) (((TimeLeft - PREDICTION_MARGIN) / 1000.0f) * time_freq());
					break;
				}
			}

			if(Target)
				m_PredictedTime.Update(&m_InputtimeMarginGraph, Target, TimeLeft, 1);
		}
		else if(Unpacker.Type() == NETMSG_SNAP || Unpacker.Type() == NETMSG_SNAPSINGLE || Unpacker.Type() == NETMSG_SNAPEMPTY)
		{
			// we are not allowed to process snapshot yet
			if(State() < IClient::STATE_LOADING)
				return;

			const int GameTick = Unpacker.GetInt();
			const int DeltaTick = GameTick - Unpacker.GetInt();

			int NumParts = 1;
			int Part = 0;
			if(Unpacker.Type() == NETMSG_SNAP)
			{
				NumParts = Unpacker.GetInt();
				Part = Unpacker.GetInt();
				if(NumParts < 1 || NumParts > CSnapshot::MAX_PARTS || Part < 0 || Part >= NumParts)
					return;
			}

			int PartSize = 0;
			int Crc = 0;
			const char *pData = 0;
			if(Unpacker.Type() != NETMSG_SNAPEMPTY)
			{
				Crc = Unpacker.GetInt();
				PartSize = Unpacker.GetInt();
				if(PartSize < 0 || PartSize > MAX_SNAPSHOT_PACKSIZE)
					return;
				if(PartSize > 0)
					pData = (const char *) Unpacker.GetRaw(PartSize);
			}

			if(Unpacker.Error())
				return;

			if(GameTick >= m_CurrentRecvTick)
			{
				if(GameTick != m_CurrentRecvTick)
				{
					m_SnapshotParts = 0;
					m_CurrentRecvTick = GameTick;
				}

				// TODO: clean this up abit
				if(pData)
					mem_copy((char *) m_aSnapshotIncomingData + Part * MAX_SNAPSHOT_PACKSIZE, pData, PartSize);

				m_SnapshotParts |= 1 << Part;

				if(m_SnapshotParts == (unsigned) ((1 << NumParts) - 1))
				{
					static CSnapshot s_Emptysnap;
					CSnapshot *pDeltaShot = &s_Emptysnap;
					unsigned char aTmpBuffer2[CSnapshot::MAX_SIZE];
					unsigned char aTmpBuffer3[CSnapshot::MAX_SIZE];
					CSnapshot *pTmpBuffer3 = (CSnapshot *) aTmpBuffer3; // Fix compiler warning for strict-aliasing

					int CompleteSize = (NumParts - 1) * MAX_SNAPSHOT_PACKSIZE + PartSize;

					// reset snapshoting
					m_SnapshotParts = 0;

					// find snapshot that we should use as delta
					s_Emptysnap.Clear();

					// find delta
					if(DeltaTick >= 0)
					{
						int DeltashotSize = m_SnapshotStorage.Get(DeltaTick, 0, &pDeltaShot, 0);

						if(DeltashotSize < 0)
						{
							// couldn't find the delta snapshots that the server used
							// to compress this snapshot. force the server to resync
							if(Config()->m_Debug)
							{
								char aBuf[256];
								str_format(aBuf, sizeof(aBuf), "error, couldn't find the delta snapshot");
								m_pConsole->Print(IConsole::OUTPUT_LEVEL_DEBUG, "client", aBuf);
							}

							// ack snapshot
							// TODO: combine this with the input message
							m_AckGameTick = -1;
							return;
						}
					}

					// decompress snapshot
					const void *pDeltaData = m_SnapshotDelta.EmptyDelta();
					int DeltaSize = sizeof(int) * 3;

					if(CompleteSize)
					{
						int IntSize = CVariableInt::Decompress(m_aSnapshotIncomingData, CompleteSize, aTmpBuffer2, sizeof(aTmpBuffer2));

						if(IntSize < 0) // failure during decompression, bail
							return;

						pDeltaData = aTmpBuffer2;
						DeltaSize = IntSize;
					}

					// unpack delta
					int SnapSize = m_SnapshotDelta.UnpackDelta(pDeltaShot, pTmpBuffer3, pDeltaData, DeltaSize);
					if(SnapSize < 0)
					{
						char aBuf[64];
						str_format(aBuf, sizeof(aBuf), "delta unpack failed! (%d)", SnapSize);
						m_pConsole->Print(IConsole::OUTPUT_LEVEL_DEBUG, "client", aBuf);
						return;
					}

					if(Unpacker.Type() != NETMSG_SNAPEMPTY && pTmpBuffer3->Crc() != Crc)
					{
						if(Config()->m_Debug)
						{
							char aBuf[256];
							str_format(aBuf, sizeof(aBuf), "snapshot crc error #%d - tick=%d wantedcrc=%d gotcrc=%d compressed_size=%d delta_tick=%d",
								m_SnapCrcErrors, GameTick, Crc, pTmpBuffer3->Crc(), CompleteSize, DeltaTick);
							m_pConsole->Print(IConsole::OUTPUT_LEVEL_DEBUG, "client", aBuf);
						}

						m_SnapCrcErrors++;
						if(m_SnapCrcErrors > 10)
						{
							// to many errors, send reset
							m_AckGameTick = -1;
							SendInput();
							m_SnapCrcErrors = 0;
						}
						return;
					}
					else
					{
						if(m_SnapCrcErrors)
							m_SnapCrcErrors--;
					}

					// purge old snapshots
					int PurgeTick = DeltaTick;
					if(m_aSnapshots[SNAP_PREV] && m_aSnapshots[SNAP_PREV]->m_Tick < PurgeTick)
						PurgeTick = m_aSnapshots[SNAP_PREV]->m_Tick;
					if(m_aSnapshots[SNAP_CURRENT] && m_aSnapshots[SNAP_CURRENT]->m_Tick < PurgeTick)
						PurgeTick = m_aSnapshots[SNAP_CURRENT]->m_Tick;
					m_SnapshotStorage.PurgeUntil(PurgeTick);

					// add new
					m_SnapshotStorage.Add(GameTick, time_get(), SnapSize, pTmpBuffer3, 1);

					// add snapshot to demo
					if(m_DemoRecorder.IsRecording())
					{
						// build up snapshot and add local messages
						m_DemoRecSnapshotBuilder.Init(pTmpBuffer3);
						GameClient()->OnDemoRecSnap();
						SnapSize = m_DemoRecSnapshotBuilder.Finish(pTmpBuffer3);

						// write snapshot
						m_DemoRecorder.RecordSnapshot(GameTick, pTmpBuffer3, SnapSize);
					}

					// apply snapshot, cycle pointers
					m_ReceivedSnapshots++;

					m_CurrentRecvTick = GameTick;

					// we got two snapshots until we see us self as connected
					if(m_ReceivedSnapshots == 2)
					{
						// start at 200ms and work from there
						m_PredictedTime.Init(GameTick * time_freq() / SERVER_TICK_SPEED);
						m_PredictedTime.SetAdjustSpeed(1, 1000.0f);
						m_GameTime.Init((GameTick - 1) * time_freq() / SERVER_TICK_SPEED);
						m_aSnapshots[SNAP_PREV] = m_SnapshotStorage.m_pFirst;
						m_aSnapshots[SNAP_CURRENT] = m_SnapshotStorage.m_pLast;
						SetState(IClient::STATE_ONLINE);
					}

					// adjust game time
					if(m_ReceivedSnapshots > 2)
					{
						int64 Now = m_GameTime.Get(time_get());
						int64 TickStart = GameTick * time_freq() / SERVER_TICK_SPEED;
						int64 TimeLeft = (TickStart - Now) * 1000 / time_freq();
						m_GameTime.Update(&m_GametimeMarginGraph, (GameTick - 1) * time_freq() / SERVER_TICK_SPEED, TimeLeft, 0);
					}

					// ack snapshot
					m_AckGameTick = GameTick;
				}
			}
		}
	}
	else
	{
		if((pPacket->m_Flags & NET_CHUNKFLAG_VITAL) != 0)
		{
			// game message
			GameClient()->OnMessage(Unpacker.Type(), &Unpacker);

			if(m_RecordGameMessage && m_DemoRecorder.IsRecording())
				m_DemoRecorder.RecordMessage(pPacket->m_pData, pPacket->m_DataSize);
		}
	}
}

void CClient::PumpNetwork()
{
	m_NetClient.Update();

	if(State() != IClient::STATE_DEMOPLAYBACK)
	{
		// check for errors
		if(State() != IClient::STATE_OFFLINE && State() != IClient::STATE_QUITING && m_NetClient.State() == NETSTATE_OFFLINE)
		{
			SetState(IClient::STATE_OFFLINE);
			DisconnectWithReason(m_NetClient.ErrorString());
			char aBuf[256];
			str_format(aBuf, sizeof(aBuf), "offline error='%s'", m_NetClient.ErrorString());
			m_pConsole->Print(IConsole::OUTPUT_LEVEL_STANDARD, "client", aBuf);
		}

		//
		if(State() == IClient::STATE_CONNECTING && m_NetClient.State() == NETSTATE_ONLINE)
		{
			// we switched to online
			m_pConsole->Print(IConsole::OUTPUT_LEVEL_STANDARD, "client", "connected, sending info");
			SetState(IClient::STATE_LOADING);
			SendInfo();
		}
	}

	// process non-connless packets
	CNetChunk Packet;
	while(m_NetClient.Recv(&Packet))
	{
		if(!(Packet.m_Flags & NETSENDFLAG_CONNLESS))
			ProcessServerPacket(&Packet);
	}

	// process connless packets data
	m_ContactClient.Update();
	while(m_ContactClient.Recv(&Packet))
	{
		if(Packet.m_Flags & NETSENDFLAG_CONNLESS)
			ProcessConnlessPacket(&Packet);
	}
}

void CClient::OnDemoPlayerSnapshot(void *pData, int Size)
{
	// update ticks, they could have changed
	const CDemoPlayer::CPlaybackInfo *pInfo = m_DemoPlayer.Info();
	CSnapshotStorage::CHolder *pTemp;
	m_CurGameTick = pInfo->m_Info.m_CurrentTick;
	m_PrevGameTick = pInfo->m_PreviousTick;

	// handle snapshots
	pTemp = m_aSnapshots[SNAP_PREV];
	m_aSnapshots[SNAP_PREV] = m_aSnapshots[SNAP_CURRENT];
	m_aSnapshots[SNAP_CURRENT] = pTemp;

	mem_copy(m_aSnapshots[SNAP_CURRENT]->m_pSnap, pData, Size);
	mem_copy(m_aSnapshots[SNAP_CURRENT]->m_pAltSnap, pData, Size);

	GameClient()->OnNewSnapshot();
}

void CClient::OnDemoPlayerMessage(void *pData, int Size)
{
	CMsgUnpacker Unpacker(pData, Size);
	if(Unpacker.Error())
		return;

	if(!Unpacker.System())
		GameClient()->OnMessage(Unpacker.Type(), &Unpacker);
}

void CClient::Update()
{
	if(State() == IClient::STATE_DEMOPLAYBACK)
	{
		m_DemoPlayer.Update();
		if(m_DemoPlayer.IsPlaying())
		{
			// update timers
			const CDemoPlayer::CPlaybackInfo *pInfo = m_DemoPlayer.Info();
			m_CurGameTick = pInfo->m_Info.m_CurrentTick;
			m_PrevGameTick = pInfo->m_PreviousTick;
			m_GameIntraTick = pInfo->m_IntraTick;
			m_GameTickTime = pInfo->m_TickTime;
		}
		else
		{
			// disconnect on error
			Disconnect();
		}
	}
	else if(State() == IClient::STATE_ONLINE && m_ReceivedSnapshots >= 3)
	{
		// switch snapshot
		int Repredict = 0;
		int64 Freq = time_freq();
		int64 Now = m_GameTime.Get(time_get());
		int64 PredNow = m_PredictedTime.Get(time_get());

		while(1)
		{
			CSnapshotStorage::CHolder *pCur = m_aSnapshots[SNAP_CURRENT];
			int64 TickStart = (pCur->m_Tick) * Freq / SERVER_TICK_SPEED;

			if(TickStart < Now)
			{
				CSnapshotStorage::CHolder *pNext = m_aSnapshots[SNAP_CURRENT]->m_pNext;
				if(pNext)
				{
					m_aSnapshots[SNAP_PREV] = m_aSnapshots[SNAP_CURRENT];
					m_aSnapshots[SNAP_CURRENT] = pNext;

					// set ticks
					m_CurGameTick = m_aSnapshots[SNAP_CURRENT]->m_Tick;
					m_PrevGameTick = m_aSnapshots[SNAP_PREV]->m_Tick;

					if(m_aSnapshots[SNAP_CURRENT] && m_aSnapshots[SNAP_PREV])
					{
						GameClient()->OnNewSnapshot();
						Repredict = 1;
					}
				}
				else
					break;
			}
			else
				break;
		}

		if(m_aSnapshots[SNAP_CURRENT] && m_aSnapshots[SNAP_PREV])
		{
			int64 CurtickStart = m_aSnapshots[SNAP_CURRENT]->m_Tick * Freq / SERVER_TICK_SPEED;
			int64 PrevtickStart = m_aSnapshots[SNAP_PREV]->m_Tick * Freq / SERVER_TICK_SPEED;
			int PrevPredTick = (int) (PredNow * SERVER_TICK_SPEED / Freq);
			int NewPredTick = PrevPredTick + 1;

			m_GameIntraTick = (Now - PrevtickStart) / (float) (CurtickStart - PrevtickStart);
			m_GameTickTime = (Now - PrevtickStart) / (float) Freq;

			CurtickStart = NewPredTick * Freq / SERVER_TICK_SPEED;
			PrevtickStart = PrevPredTick * Freq / SERVER_TICK_SPEED;
			m_PredIntraTick = (PredNow - PrevtickStart) / (float) (CurtickStart - PrevtickStart);

			if(NewPredTick < m_aSnapshots[SNAP_PREV]->m_Tick - SERVER_TICK_SPEED || NewPredTick > m_aSnapshots[SNAP_PREV]->m_Tick + SERVER_TICK_SPEED)
			{
				m_pConsole->Print(IConsole::OUTPUT_LEVEL_ADDINFO, "client", "prediction time reset!");
				m_PredictedTime.Init(m_aSnapshots[SNAP_CURRENT]->m_Tick * Freq / SERVER_TICK_SPEED);
			}

			if(NewPredTick > m_PredTick)
			{
				m_PredTick = NewPredTick;
				Repredict = 1;

				// send input
				SendInput();
			}
		}

		// only do sane predictions
		if(Repredict)
		{
			if(m_PredTick > m_CurGameTick && m_PredTick < m_CurGameTick + SERVER_TICK_SPEED)
				GameClient()->OnPredict();
		}
	}

	// STRESS TEST: join the server again
#ifdef CONF_DEBUG
	if(Config()->m_DbgStress)
	{
		static int64 ActionTaken = 0;
		int64 Now = time_get();
		if(State() == IClient::STATE_OFFLINE)
		{
			if(Now > ActionTaken + time_freq() * 2)
			{
				m_pConsole->Print(IConsole::OUTPUT_LEVEL_DEBUG, "stress", "reconnecting!");
				Connect(Config()->m_DbgStressServer);
				ActionTaken = Now;
			}
		}
		else
		{
			if(Now > ActionTaken + time_freq() * (10 + Config()->m_DbgStress))
			{
				m_pConsole->Print(IConsole::OUTPUT_LEVEL_DEBUG, "stress", "disconnecting!");
				Disconnect();
				ActionTaken = Now;
			}
		}
	}
#endif

	// pump the network
	PumpNetwork();

	// update the server browser
	m_ServerBrowser.Update();

	// update editor/gameclient
	if(m_EditorActive)
		m_pEditor->OnUpdate();
	else
		GameClient()->OnUpdate();
}

void CClient::VersionUpdate()
{
}

void CClient::RegisterInterfaces()
{
	Kernel()->RegisterInterface(static_cast<IDemoRecorder *>(&m_DemoRecorder));
	Kernel()->RegisterInterface(static_cast<IDemoPlayer *>(&m_DemoPlayer));
	Kernel()->RegisterInterface(static_cast<IServerBrowser *>(&m_ServerBrowser));
	Kernel()->RegisterInterface(static_cast<IFriends *>(&m_Friends));
	Kernel()->RegisterInterface(static_cast<IBlacklist *>(&m_Blacklist));
}

void CClient::InitInterfaces()
{
	// fetch interfaces
	m_pEngine = Kernel()->RequestInterface<IEngine>();
	m_pEditor = Kernel()->RequestInterface<IEditor>();
	// m_pGraphics = Kernel()->RequestInterface<IEngineGraphics>();
	m_pSound = Kernel()->RequestInterface<IEngineSound>();
	m_pTextRender = Kernel()->RequestInterface<IEngineTextRender>();
	m_pGameClient = Kernel()->RequestInterface<IGameClient>();
	m_pInput = Kernel()->RequestInterface<IEngineInput>();
	m_pMap = Kernel()->RequestInterface<IEngineMap>();
	m_pConfigManager = Kernel()->RequestInterface<IConfigManager>();
	m_pConfig = m_pConfigManager->Values();
	m_pStorage = Kernel()->RequestInterface<IStorage>();
	Kernel()->RegisterInterface(static_cast<IHttp *>(&m_Http));

	//
	m_ServerBrowser.Init(&m_Http, &m_ContactClient, m_pGameClient->NetVersion());
	m_Friends.Init();
	m_Blacklist.Init();
	m_DemoRecorder.Init(Console(), m_pStorage);
	m_DemoPlayer.Init(Console(), m_pStorage);
}

bool CClient::LimitFps()
{
	if(Config()->m_GfxVsync || !Config()->m_GfxLimitFps)
		return false;

	/**
		If desired frame time is not reached:
			Skip rendering the frame
			Do another game loop

		If we don't have the time to do another game loop:
			Wait until desired frametime

		Returns true if frame should be skipped
	**/

#ifdef CONF_DEBUG
	static double DbgTimeWaited = 0.0;
	static int64 DbgFramesSkippedCount = 0;
	static int64 DbgLastSkippedDbgMsg = time_get();
#endif

	int64 Now = time_get();
	const double LastCpuFrameTime = (Now - m_LastCpuTime) / (double) time_freq();
	m_LastAvgCpuFrameTime = (m_LastAvgCpuFrameTime + LastCpuFrameTime * 4.0) / 5.0;
	m_LastCpuTime = Now;

	bool SkipFrame = true;
	double RenderDeltaTime = (Now - m_LastRenderTime) / (double) time_freq();
	const double DesiredTime = 1.0 / Config()->m_GfxMaxFps;

	// we can't skip another frame, so wait instead
	if(SkipFrame && RenderDeltaTime < DesiredTime &&
		m_LastAvgCpuFrameTime * 1.20 > (DesiredTime - RenderDeltaTime))
	{
#ifdef CONF_DEBUG
		DbgTimeWaited += DesiredTime - RenderDeltaTime;
#endif
		const double Freq = (double) time_freq();
		const int64 LastT = m_LastRenderTime;
		double d = DesiredTime - RenderDeltaTime;
		while(d > 0.00001)
		{
			Now = time_get();
			RenderDeltaTime = (Now - LastT) / Freq;
			d = DesiredTime - RenderDeltaTime;
			cpu_relax();
		}

		SkipFrame = false;
		m_LastCpuTime = Now;
	}

	// RenderDeltaTime exceeds DesiredTime, render
	if(SkipFrame && RenderDeltaTime > DesiredTime)
	{
		SkipFrame = false;
	}

#ifdef CONF_DEBUG
	DbgFramesSkippedCount += SkipFrame ? 1 : 0;

	Now = time_get();
	if(Config()->m_GfxLimitFps &&
		Config()->m_Debug &&
		(Now - DbgLastSkippedDbgMsg) / (double) time_freq() > 5.0)
	{
		char aBuf[128];
		str_format(aBuf, sizeof(aBuf), "LimitFps: FramesSkippedCount=%lu TimeWaited=%.3f (per sec)",
			(unsigned long) (DbgFramesSkippedCount / 5),
			DbgTimeWaited / 5.0);
		m_pConsole->Print(IConsole::OUTPUT_LEVEL_DEBUG, "client", aBuf);
		DbgFramesSkippedCount = 0;
		DbgTimeWaited = 0;
		DbgLastSkippedDbgMsg = Now;
	}
#endif

	return SkipFrame;
}

void CClient::Run()
{
	m_LocalStartTime = time_get();
	m_SnapshotParts = 0;

	// init SDL
	{
		if(!SDL_Init(0))
		{
			dbg_msg("client", "unable to init SDL base: %s", SDL_GetError());
			return;
		}

		atexit(SDL_Quit);
	}

	// init graphics
	{
		m_pGraphics = CreateEngineGraphicsThreaded();

		bool RegisterFail = false;
		RegisterFail = RegisterFail || !Kernel()->RegisterInterface(static_cast<IEngineGraphics *>(m_pGraphics)); // register graphics as both
		RegisterFail = RegisterFail || !Kernel()->RegisterInterface(static_cast<IGraphics *>(m_pGraphics));

		if(RegisterFail || m_pGraphics->Init() != 0)
		{
			dbg_msg("client", "couldn't init graphics");
			return;
		}
	}

	if(!m_Http.Init(std::chrono::seconds{2}, Config()))
	{
		dbg_msg("server", "Failed to initialize the HTTP client.");
		return;
	}
	m_ServerBrowser.OnInitHttp();

	// init sound, allowed to fail
	m_SoundInitFailed = Sound()->Init() != 0;
	Sound()->SetMaxDistance(1.5f * Graphics()->ScreenWidth() / 2.0f);

	// open socket
	{
		NETADDR BindAddr;
		if(Config()->m_Bindaddr[0] && net_host_lookup(Config()->m_Bindaddr, &BindAddr, NETTYPE_ALL) == 0)
		{
			// got bindaddr
			BindAddr.type = NETTYPE_ALL;
		}
		else
		{
			mem_zero(&BindAddr, sizeof(BindAddr));
			BindAddr.type = NETTYPE_ALL;
		}
		if(!m_NetClient.Open(BindAddr, Config(), Console(), Engine(), BindAddr.port ? 0 : NETCREATE_FLAG_RANDOMPORT))
		{
			dbg_msg("client", "couldn't open socket(net)");
			return;
		}
		BindAddr.port = 0;
		if(!m_ContactClient.Open(BindAddr, Config(), Console(), Engine(), 0))
		{
			dbg_msg("client", "couldn't open socket(contact)");
			return;
		}
	}

	// init font rendering
	m_pTextRender->Init();

	// init the input
	Input()->Init();

	GameClient()->OnInit();

	char aBuf[256];
	str_format(aBuf, sizeof(aBuf), "netversion %s", GameClient()->NetVersion());
	m_pConsole->Print(IConsole::OUTPUT_LEVEL_STANDARD, "client", aBuf);
	if(str_comp(GameClient()->NetVersionHashUsed(), GameClient()->NetVersionHashReal()))
	{
		m_pConsole->Print(IConsole::OUTPUT_LEVEL_STANDARD, "client", "WARNING: netversion hash differs");
	}
	str_format(aBuf, sizeof(aBuf), "game version %s", GameClient()->Version());
	m_pConsole->Print(IConsole::OUTPUT_LEVEL_STANDARD, "client", aBuf);

	//
	m_FpsGraph.Init(0.0f, 120.0f);

	// never start with the editor
	Config()->m_ClEditor = 0;

	// process pending commands
	m_pConsole->StoreCommands(false);

	while(1)
	{
		//
		VersionUpdate();

		// handle pending connects
		if(m_aCmdConnect[0])
		{
			Connect(m_aCmdConnect);
			m_aCmdConnect[0] = 0;
		}

		// update input
		if(Input()->Update())
			break; // SDL_QUIT

		// update sound
		Sound()->Update();

		// release focus
		if(!m_pGraphics->WindowActive())
		{
			if(m_WindowMustRefocus == 0)
				Input()->MouseModeAbsolute();
			m_WindowMustRefocus = 1;
		}
		else if(Config()->m_DbgFocus && Input()->KeyPress(KEY_ESCAPE, true))
		{
			Input()->MouseModeAbsolute();
			m_WindowMustRefocus = 1;
		}

		// refocus
		if(m_WindowMustRefocus && m_pGraphics->WindowActive())
		{
			if(m_WindowMustRefocus < 3)
			{
				Input()->MouseModeAbsolute();
				m_WindowMustRefocus++;
			}

			if(m_WindowMustRefocus >= 3 || Input()->KeyPress(KEY_MOUSE_1, true))
			{
				Input()->MouseModeRelative();
				m_WindowMustRefocus = 0;

				// update screen in case it got moved
				int ActScreen = Graphics()->GetWindowScreen();
				if(ActScreen >= 0 && ActScreen != Config()->m_GfxScreen)
					Config()->m_GfxScreen = ActScreen;
			}
		}

		// panic quit button
		bool IsCtrlPressed = Input()->KeyIsPressed(KEY_LCTRL);
		bool IsLShiftPressed = Input()->KeyIsPressed(KEY_LSHIFT);
		if(IsCtrlPressed && IsLShiftPressed && Input()->KeyPress(KEY_Q, true))
		{
			Quit();
			break;
		}

		if(IsCtrlPressed && IsLShiftPressed && Input()->KeyPress(KEY_D, true))
			Config()->m_Debug ^= 1;

		if(IsCtrlPressed && IsLShiftPressed && Input()->KeyPress(KEY_G, true))
			Config()->m_DbgGraphs ^= 1;

		if(IsCtrlPressed && IsLShiftPressed && Input()->KeyPress(KEY_E, true))
		{
			Config()->m_ClEditor = Config()->m_ClEditor ^ 1;
			Input()->MouseModeRelative();
		}

		// render
		{
			if(Config()->m_ClEditor)
			{
				if(!m_EditorActive)
				{
					GameClient()->OnActivateEditor();
					Input()->MouseModeRelative();
					m_EditorActive = true;
				}
			}
			else if(m_EditorActive)
				m_EditorActive = false;

			m_pTextRender->Update();

			Update();

			const bool SkipFrame = LimitFps();

			if(!SkipFrame && (!Config()->m_GfxAsyncRender || m_pGraphics->IsIdle()))
			{
				m_RenderFrames++;

				// update frametime
				int64 Now = time_get();
				m_RenderFrameTime = (Now - m_LastRenderTime) / (float) time_freq();

				if(m_RenderFrameTime < m_RenderFrameTimeLow)
					m_RenderFrameTimeLow = m_RenderFrameTime;
				if(m_RenderFrameTime > m_RenderFrameTimeHigh)
					m_RenderFrameTimeHigh = m_RenderFrameTime;
				m_FpsGraph.Add(1.0f / m_RenderFrameTime, 1, 1, 1);

				m_LastRenderTime = Now;

				// when we are stress testing only render every 10th frame
#ifdef CONF_DEBUG
				if(!Config()->m_DbgStress || (m_RenderFrames % 10) == 0)
#endif
				{
					Render();
					m_pGraphics->Swap();
				}
			}
		}

		AutoScreenshot_Cleanup();

		// check conditions
		if(State() == IClient::STATE_QUITING)
			break;

		// beNice
		if(Config()->m_ClCpuThrottle)
			thread_sleep(Config()->m_ClCpuThrottle);
		else if(!m_pGraphics->WindowActive())
			thread_sleep(5);
#ifdef CONF_DEBUG
		else if(Config()->m_DbgStress)
			thread_sleep(5);
#endif

		if(Config()->m_DbgHitch)
		{
			thread_sleep(Config()->m_DbgHitch);
			Config()->m_DbgHitch = 0;
		}

		/*
		if(ReportTime < time_get())
		{
			if(0 && Config()->m_Debug)
			{
				dbg_msg("client/report", "fps=%.02f (%.02f %.02f) netstate=%d",
					m_Frames/(float)(ReportInterval/time_freq()),
					1.0f/m_RenderFrameTimeHigh,
					1.0f/m_RenderFrameTimeLow,
					m_NetClient.State());
			}
			m_RenderFrameTimeLow = 1;
			m_RenderFrameTimeHigh = 0;
			m_RenderFrames = 0;
			ReportTime += ReportInterval;
		}*/

		// update local time
		m_LocalTime = (time_get() - m_LocalStartTime) / (float) time_freq();
	}

	GameClient()->OnShutdown();
	Disconnect();

	m_pInput->Shutdown();
	m_pGraphics->Shutdown();
	m_pSound->Shutdown();
	m_pTextRender->Shutdown();

	m_ServerBrowser.SaveServerlist();

	// shutdown SDL
	SDL_Quit();
}

int64 CClient::TickStartTime(int Tick)
{
	return m_LocalStartTime + (time_freq() * Tick) / m_GameTickSpeed;
}

void CClient::Con_Connect(IConsole::IResult *pResult, void *pUserData)
{
	CClient *pSelf = (CClient *) pUserData;
	str_copy(pSelf->m_aCmdConnect, pResult->GetString(0), sizeof(pSelf->m_aCmdConnect));
}

void CClient::Con_Disconnect(IConsole::IResult *pResult, void *pUserData)
{
	CClient *pSelf = (CClient *) pUserData;
	pSelf->Disconnect();
}

void CClient::Con_Quit(IConsole::IResult *pResult, void *pUserData)
{
	CClient *pSelf = (CClient *) pUserData;
	pSelf->Quit();
}

void CClient::Con_Minimize(IConsole::IResult *pResult, void *pUserData)
{
	CClient *pSelf = (CClient *) pUserData;
	pSelf->Graphics()->Minimize();
}

void CClient::Con_Ping(IConsole::IResult *pResult, void *pUserData)
{
	CClient *pSelf = (CClient *) pUserData;

	CMsgPacker Msg(NETMSG_PING, true);
	pSelf->SendMsg(&Msg, MSGFLAG_FLUSH);
	pSelf->m_PingStartTime = time_get();
}

void CClient::AutoScreenshot_Start()
{
	if(Config()->m_ClAutoScreenshot)
	{
		Graphics()->TakeScreenshot("auto/autoscreen");
		m_AutoScreenshotRecycle = true;
	}
}

void CClient::AutoStatScreenshot_Start()
{
	if(Config()->m_ClAutoStatScreenshot)
	{
		Graphics()->TakeScreenshot("auto/stat");
		m_AutoStatScreenshotRecycle = true;
	}
}

void CClient::AutoScreenshot_Cleanup()
{
	if(m_AutoScreenshotRecycle)
	{
		if(Config()->m_ClAutoScreenshotMax)
		{
			// clean up auto taken screens
			CFileCollection AutoScreens;
			AutoScreens.Init(Storage(), "screenshots/auto", "autoscreen", ".png", Config()->m_ClAutoScreenshotMax);
		}
		m_AutoScreenshotRecycle = false;
	}
	if(m_AutoStatScreenshotRecycle)
	{
		if(Config()->m_ClAutoScreenshotMax)
		{
			// clean up auto taken stat screens
			CFileCollection AutoScreens;
			AutoScreens.Init(Storage(), "screenshots/auto", "stat", ".png", Config()->m_ClAutoScreenshotMax);
		}
		m_AutoStatScreenshotRecycle = false;
	}
}

void CClient::Con_Screenshot(IConsole::IResult *pResult, void *pUserData)
{
	CClient *pSelf = (CClient *) pUserData;
	pSelf->Graphics()->TakeScreenshot(0);
}

void CClient::Con_Rcon(IConsole::IResult *pResult, void *pUserData)
{
	CClient *pSelf = (CClient *) pUserData;
	pSelf->SendRcon(pResult->GetString(0));
}

void CClient::Con_RconAuth(IConsole::IResult *pResult, void *pUserData)
{
	CClient *pSelf = (CClient *) pUserData;
	pSelf->SendRconAuth("", pResult->GetString(0));
}

const char *CClient::DemoPlayer_Play(const char *pFilename, int StorageType)
{
	Disconnect();
	m_NetClient.ResetErrorString();

	// try to start playback
	m_DemoPlayer.SetListener(this);

	const char *pError = m_DemoPlayer.Load(pFilename, StorageType, GameClient()->NetVersion());
	if(pError)
		return pError;

	// load map
	const unsigned Crc = bytes_be_to_uint(m_DemoPlayer.Info()->m_Header.m_aMapCrc);
	pError = LoadMapSearch(m_DemoPlayer.Info()->m_Header.m_aMapName, 0, Crc);
	if(pError)
	{
		DisconnectWithReason(pError);
		return pError;
	}

	GameClient()->OnConnected();

	// setup buffers
	mem_zero(m_aDemorecSnapshotData, sizeof(m_aDemorecSnapshotData));

	m_aSnapshots[SNAP_CURRENT] = &m_aDemorecSnapshotHolders[SNAP_CURRENT];
	m_aSnapshots[SNAP_PREV] = &m_aDemorecSnapshotHolders[SNAP_PREV];

	m_aSnapshots[SNAP_CURRENT]->m_pSnap = (CSnapshot *) m_aDemorecSnapshotData[SNAP_CURRENT][0];
	m_aSnapshots[SNAP_CURRENT]->m_pAltSnap = (CSnapshot *) m_aDemorecSnapshotData[SNAP_CURRENT][1];
	m_aSnapshots[SNAP_CURRENT]->m_SnapSize = 0;
	m_aSnapshots[SNAP_CURRENT]->m_Tick = -1;

	m_aSnapshots[SNAP_PREV]->m_pSnap = (CSnapshot *) m_aDemorecSnapshotData[SNAP_PREV][0];
	m_aSnapshots[SNAP_PREV]->m_pAltSnap = (CSnapshot *) m_aDemorecSnapshotData[SNAP_PREV][1];
	m_aSnapshots[SNAP_PREV]->m_SnapSize = 0;
	m_aSnapshots[SNAP_PREV]->m_Tick = -1;

	// enter demo playback state
	SetState(IClient::STATE_DEMOPLAYBACK);

	m_DemoPlayer.Play();
	GameClient()->OnEnterGame();

	return 0;
}

void CClient::DemoRecorder_Start(const char *pFilename, bool WithTimestamp)
{
	if(State() != IClient::STATE_ONLINE)
		m_pConsole->Print(IConsole::OUTPUT_LEVEL_STANDARD, "demorec/record", "client is not online");
	else
	{
		char aFilename[128];
		if(WithTimestamp)
		{
			char aDate[20];
			str_timestamp(aDate, sizeof(aDate));
			str_format(aFilename, sizeof(aFilename), "demos/%s_%s.demo", pFilename, aDate);
		}
		else
			str_format(aFilename, sizeof(aFilename), "demos/%s.demo", pFilename);
		m_DemoRecorder.Start(aFilename, GameClient()->NetVersion(), m_aCurrentMap, m_CurrentMapSha256, m_CurrentMapCrc, "client");
	}
}

void CClient::DemoRecorder_HandleAutoStart()
{
	if(Config()->m_ClAutoDemoRecord)
	{
		DemoRecorder_Stop();
		DemoRecorder_Start("auto/autorecord", true);
		if(Config()->m_ClAutoDemoMax)
		{
			// clean up auto recorded demos
			CFileCollection AutoDemos;
			AutoDemos.Init(Storage(), "demos/auto", "autorecord", ".demo", Config()->m_ClAutoDemoMax);
		}
	}
}

void CClient::DemoRecorder_Stop(bool ErrorIfNotRecording)
{
	if(ErrorIfNotRecording || m_DemoRecorder.IsRecording())
		m_DemoRecorder.Stop();
}

void CClient::DemoRecorder_AddDemoMarker()
{
	m_DemoRecorder.AddDemoMarker();
}

void CClient::Con_Record(IConsole::IResult *pResult, void *pUserData)
{
	CClient *pSelf = (CClient *) pUserData;
	if(pResult->NumArguments())
		pSelf->DemoRecorder_Start(pResult->GetString(0), false);
	else
		pSelf->DemoRecorder_Start("demo", true);
}

void CClient::Con_StopRecord(IConsole::IResult *pResult, void *pUserData)
{
	CClient *pSelf = (CClient *) pUserData;
	pSelf->DemoRecorder_Stop(true);
}

void CClient::Con_AddDemoMarker(IConsole::IResult *pResult, void *pUserData)
{
	CClient *pSelf = (CClient *) pUserData;
	pSelf->DemoRecorder_AddDemoMarker();
}

void CClient::ServerBrowserUpdate()
{
	m_ServerBrowser.RequestResort();
}

void CClient::ConchainServerBrowserUpdate(IConsole::IResult *pResult, void *pUserData, IConsole::FCommandCallback pfnCallback, void *pCallbackUserData)
{
	pfnCallback(pResult, pCallbackUserData);
	if(pResult->NumArguments())
		((CClient *) pUserData)->ServerBrowserUpdate();
}

void CClient::SwitchWindowScreen(int Index)
{
	// Todo SDL: remove this when fixed (changing screen when in fullscreen is bugged)
	if(Config()->m_GfxFullscreen)
	{
		ToggleFullscreen();
		if(Graphics()->SetWindowScreen(Index))
			Config()->m_GfxScreen = Index;
		ToggleFullscreen();
	}
	else
	{
		if(Graphics()->SetWindowScreen(Index))
			Config()->m_GfxScreen = Index;
	}
}

void CClient::ConchainWindowScreen(IConsole::IResult *pResult, void *pUserData, IConsole::FCommandCallback pfnCallback, void *pCallbackUserData)
{
	CClient *pSelf = (CClient *) pUserData;
	if(pSelf->Graphics() && pResult->NumArguments())
	{
		if(pSelf->Config()->m_GfxScreen != pResult->GetInteger(0))
			pSelf->SwitchWindowScreen(pResult->GetInteger(0));
	}
	else
		pfnCallback(pResult, pCallbackUserData);
}

bool CClient::ToggleFullscreen()
{
	if(Graphics()->Fullscreen(Config()->m_GfxFullscreen ^ 1))
		Config()->m_GfxFullscreen ^= 1;
	return true;
}

void CClient::ConchainFullscreen(IConsole::IResult *pResult, void *pUserData, IConsole::FCommandCallback pfnCallback, void *pCallbackUserData)
{
	CClient *pSelf = (CClient *) pUserData;
	if(pSelf->Graphics() && pResult->NumArguments())
	{
		if(pSelf->Config()->m_GfxFullscreen != pResult->GetInteger(0))
			pSelf->ToggleFullscreen();
	}
	else
		pfnCallback(pResult, pCallbackUserData);
}

void CClient::ToggleWindowBordered()
{
	Config()->m_GfxBorderless ^= 1;
	Graphics()->SetWindowBordered(!Config()->m_GfxBorderless);
}

void CClient::ConchainWindowBordered(IConsole::IResult *pResult, void *pUserData, IConsole::FCommandCallback pfnCallback, void *pCallbackUserData)
{
	CClient *pSelf = (CClient *) pUserData;
	if(pSelf->Graphics() && pResult->NumArguments())
	{
		if(!pSelf->Config()->m_GfxFullscreen && (pSelf->Config()->m_GfxBorderless != pResult->GetInteger(0)))
			pSelf->ToggleWindowBordered();
	}
	else
		pfnCallback(pResult, pCallbackUserData);
}

void CClient::ToggleWindowVSync()
{
	if(Graphics()->SetVSync(Config()->m_GfxVsync ^ 1))
		Config()->m_GfxVsync ^= 1;
}

void CClient::ConchainWindowVSync(IConsole::IResult *pResult, void *pUserData, IConsole::FCommandCallback pfnCallback, void *pCallbackUserData)
{
	CClient *pSelf = (CClient *) pUserData;
	if(pSelf->Graphics() && pResult->NumArguments())
	{
		if(pSelf->Config()->m_GfxVsync != pResult->GetInteger(0))
			pSelf->ToggleWindowVSync();
	}
	else
		pfnCallback(pResult, pCallbackUserData);
}

void CClient::RegisterCommands()
{
	m_pConsole = Kernel()->RequestInterface<IConsole>();

	m_pConsole->Register("quit", "", CFGFLAG_CLIENT | CFGFLAG_STORE, Con_Quit, this, "Quit Teeworlds");
	m_pConsole->Register("exit", "", CFGFLAG_CLIENT | CFGFLAG_STORE, Con_Quit, this, "Quit Teeworlds");
	m_pConsole->Register("minimize", "", CFGFLAG_CLIENT | CFGFLAG_STORE, Con_Minimize, this, "Minimize Teeworlds");
	m_pConsole->Register("connect", "s[host|ip]", CFGFLAG_CLIENT | CFGFLAG_STORE, Con_Connect, this, "Connect to the specified host/ip");
	m_pConsole->Register("disconnect", "", CFGFLAG_CLIENT, Con_Disconnect, this, "Disconnect from the server");
	m_pConsole->Register("ping", "", CFGFLAG_CLIENT, Con_Ping, this, "Ping the current server");
	m_pConsole->Register("screenshot", "", CFGFLAG_CLIENT, Con_Screenshot, this, "Take a screenshot");
	m_pConsole->Register("rcon", "r[command]", CFGFLAG_CLIENT, Con_Rcon, this, "Send specified command to rcon");
	m_pConsole->Register("rcon_auth", "s[password]", CFGFLAG_CLIENT, Con_RconAuth, this, "Authenticate to rcon");
	m_pConsole->Register("record", "?s[file]", CFGFLAG_CLIENT, Con_Record, this, "Record to the file");
	m_pConsole->Register("stoprecord", "", CFGFLAG_CLIENT, Con_StopRecord, this, "Stop recording");
	m_pConsole->Register("add_demomarker", "", CFGFLAG_CLIENT, Con_AddDemoMarker, this, "Add demo timeline marker");

	// used for server browser update
	m_pConsole->Chain("br_filter_string", ConchainServerBrowserUpdate, this);

	m_pConsole->Chain("gfx_screen", ConchainWindowScreen, this);
	m_pConsole->Chain("gfx_fullscreen", ConchainFullscreen, this);
	m_pConsole->Chain("gfx_borderless", ConchainWindowBordered, this);
	m_pConsole->Chain("gfx_vsync", ConchainWindowVSync, this);
}

static CClient *CreateClient()
{
	CClient *pClient = static_cast<CClient *>(mem_alloc(sizeof(CClient)));
	mem_zero(pClient, sizeof(CClient));
	return new(pClient) CClient;
}

void CClient::ConnectOnStart(const char *pAddress)
{
	str_copy(m_aCmdConnect, pAddress, sizeof(m_aCmdConnect));
}

void CClient::DoVersionSpecificActions()
{
	Config()->m_ClLastVersionPlayed = CLIENT_VERSION;
}

/*
	Server Time
	Client Mirror Time
	Client Predicted Time

	Snapshot Latency
		Downstream latency

	Prediction Latency
		Upstream latency
*/
#if defined(CONF_PLATFORM_MACOS)
extern "C" int TWMain(int argc, const char **argv)
#else
int main(int argc, const char **argv)
#endif
{
	cmdline_fix(&argc, &argv);
#if defined(CONF_FAMILY_WINDOWS)
	bool QuickEditMode = false;
	for(int i = 1; i < argc; i++)
	{
		if(str_comp("--quickeditmode", argv[i]) == 0)
		{
			QuickEditMode = true;
		}
	}
#endif

	bool UseDefaultConfig = false;
	for(int i = 1; i < argc; i++)
	{
		if(str_comp("-d", argv[i]) == 0 || str_comp("--default", argv[i]) == 0)
		{
			UseDefaultConfig = true;
			break;
		}
	}

	bool RandInitFailed = secure_random_init() != 0;

	CClient *pClient = CreateClient();
	IKernel *pKernel = IKernel::Create();
	pKernel->RegisterInterface(pClient);
	pClient->RegisterInterfaces();

	// create the components
	int FlagMask = CFGFLAG_CLIENT;
	IEngine *pEngine = CreateEngine("Teeworlds");
	IConsole *pConsole = CreateConsole(FlagMask);
	IStorage *pStorage = CreateStorage("Teeworlds", IStorage::STORAGETYPE_CLIENT, argc, argv);
	IConfigManager *pConfigManager = CreateConfigManager();
	IEngineSound *pEngineSound = CreateEngineSound();
	IEngineInput *pEngineInput = CreateEngineInput();
	IEngineTextRender *pEngineTextRender = CreateEngineTextRender();
	IEngineMap *pEngineMap = CreateEngineMap();

	if(RandInitFailed)
	{
		dbg_msg("secure", "could not initialize secure RNG");
		return -1;
	}

	{
		bool RegisterFail = false;

		RegisterFail = RegisterFail || !pKernel->RegisterInterface(pEngine);
		RegisterFail = RegisterFail || !pKernel->RegisterInterface(pConsole);
		RegisterFail = RegisterFail || !pKernel->RegisterInterface(pConfigManager);

		RegisterFail = RegisterFail || !pKernel->RegisterInterface(static_cast<IEngineSound *>(pEngineSound)); // register as both
		RegisterFail = RegisterFail || !pKernel->RegisterInterface(static_cast<ISound *>(pEngineSound));

		RegisterFail = RegisterFail || !pKernel->RegisterInterface(static_cast<IEngineInput *>(pEngineInput)); // register as both
		RegisterFail = RegisterFail || !pKernel->RegisterInterface(static_cast<IInput *>(pEngineInput));

		RegisterFail = RegisterFail || !pKernel->RegisterInterface(static_cast<IEngineTextRender *>(pEngineTextRender)); // register as both
		RegisterFail = RegisterFail || !pKernel->RegisterInterface(static_cast<ITextRender *>(pEngineTextRender));

		RegisterFail = RegisterFail || !pKernel->RegisterInterface(static_cast<IEngineMap *>(pEngineMap)); // register as both
		RegisterFail = RegisterFail || !pKernel->RegisterInterface(static_cast<IMap *>(pEngineMap));

		RegisterFail = RegisterFail || !pKernel->RegisterInterface(CreateEditor());
		RegisterFail = RegisterFail || !pKernel->RegisterInterface(CreateGameClient());
		RegisterFail = RegisterFail || !pKernel->RegisterInterface(pStorage);

		if(RegisterFail)
			return -1;
	}

	pEngine->Init();
	pConfigManager->Init(FlagMask);
	pConsole->Init();

	// register all console commands
	pClient->RegisterCommands();

	// init client's interfaces
	pClient->InitInterfaces();

	pKernel->RequestInterface<IGameClient>()->OnConsoleInit();

	if(!UseDefaultConfig)
	{
		// execute config file
		if(!pConsole->ExecuteFile(SETTINGS_FILENAME ".cfg"))
			pConsole->ExecuteFile("settings.cfg"); // fallback to legacy naming scheme

		// execute autoexec file
		pConsole->ExecuteFile("autoexec.cfg");

		// parse the command line arguments
		if(argc > 1)
		{
			const char *pAddress = 0;
			if(argc == 2)
			{
				pAddress = str_startswith(argv[1], "teeworlds:");
			}
			if(pAddress)
			{
				pClient->ConnectOnStart(pAddress);
			}
			else
			{
				pConsole->ParseArguments(argc - 1, &argv[1]);
			}
		}
	}
#if defined(CONF_FAMILY_WINDOWS)
	CConfig *pConfig = pConfigManager->Values();
	bool HideConsole = false;
#ifdef CONF_RELEASE
	if(!(pConfig->m_ShowConsoleWindow & 2))
#else
	if(!(pConfig->m_ShowConsoleWindow & 1))
#endif
	{
		HideConsole = true;
		dbg_console_hide();
	}
	else if(!QuickEditMode)
		dbg_console_init();
#endif

	pClient->DoVersionSpecificActions();

	// restore empty config strings to their defaults
	pConfigManager->RestoreStrings();

	pClient->Engine()->InitLogfile();

	// run the client
	dbg_msg("client", "starting...");
	pClient->Run();

	// wait for background jobs to finish
	pEngine->ShutdownJobs();

	// write down the config and quit
	pConfigManager->Save();

#if defined(CONF_FAMILY_WINDOWS)
	if(!HideConsole && !QuickEditMode)
		dbg_console_cleanup();
#endif
	// free components
	pClient->~CClient();
	mem_free(pClient);
	delete pKernel;
	delete pEngine;
	delete pConsole;
	delete pStorage;
	delete pConfigManager;
	delete pEngineSound;
	delete pEngineInput;
	delete pEngineTextRender;
	delete pEngineMap;

	secure_random_uninit();
	cmdline_free(argc, argv);
	return 0;
}
