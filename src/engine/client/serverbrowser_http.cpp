#include "serverbrowser_http.h"

#include <engine/console.h>
#include <engine/engine.h>
#include <engine/external/json-parser/json.h>
#include <engine/serverbrowser.h>
#include <engine/shared/http.h>
#include <engine/shared/jobs.h>
#include <engine/shared/linereader.h>
#include <engine/shared/serverinfo.h>
#include <engine/storage.h>

#include <base/lock.h>
#include <base/system.h>

#include <memory>
#include <vector>

#include <chrono>

using namespace std::chrono_literals;

class CServerBrowserHttp : public IServerBrowserHttp
{
public:
	CServerBrowserHttp(IHttp *pHttp, CConfig *pConfig);
	~CServerBrowserHttp() override;
	void Update() override;
	bool IsError() const override { return m_Error; }
	bool IsRefreshing() const override { return m_State != STATE_DONE; }
	void Refresh() override;

	int NumServers() const override
	{
		return m_vServers.size();
	}
	const CServerInfo &Server(int Index) const override
	{
		return m_vServers[Index];
	}

private:
	bool m_Error;
	enum
	{
		STATE_DONE,
		STATE_WANTREFRESH,
		STATE_REFRESHING,
	};

	static bool Validate(json_value *pJson);
	static bool Parse(json_value *pJson, std::vector<CServerInfo> *pvServers);

	IHttp *m_pHttp;
	CConfig *m_pConfig;

	int m_State = STATE_WANTREFRESH;
	std::shared_ptr<CHttpRequest> m_pGetServers;

	std::vector<CServerInfo> m_vServers;
};

CServerBrowserHttp::CServerBrowserHttp(IHttp *pHttp, CConfig *pConfig) :
	m_pHttp(pHttp),
	m_pConfig(pConfig)
{
	m_Error = false;
}

CServerBrowserHttp::~CServerBrowserHttp()
{
	if(m_pGetServers != nullptr)
	{
		m_pGetServers->Abort();
	}
}

void CServerBrowserHttp::Update()
{
	if(m_State == STATE_WANTREFRESH)
	{
		m_pGetServers = HttpGet("https://api.status.tw/server/list", m_pConfig);
		// 10 seconds connection timeout, lower than 8KB/s for 10 seconds to fail.
		m_pGetServers->Timeout(CTimeout{10000, 0, 512000, 10});
		m_pHttp->Run(m_pGetServers);
		m_State = STATE_REFRESHING;
		m_Error = false;
	}
	else if(m_State == STATE_REFRESHING)
	{
		if(!m_pGetServers->Done())
		{
			return;
		}
		m_State = STATE_DONE;
		std::shared_ptr<CHttpRequest> pGetServers = nullptr;
		std::swap(m_pGetServers, pGetServers);

		bool Success = true;
		json_value *pJson = pGetServers->State() == EHttpState::DONE ? pGetServers->ResultJson() : nullptr;
		Success = Success && pJson;
		Success = Success && !Parse(pJson, &m_vServers);
		json_value_free(pJson);
		if(!Success)
		{
			dbg_msg("serverbrowser_http", "failed getting serverlist");
			m_Error = true;
		}
	}
}
void CServerBrowserHttp::Refresh()
{
	if(m_State == STATE_REFRESHING)
	{
		m_State = STATE_WANTREFRESH;
	}
	if(m_State == STATE_DONE)
		m_State = STATE_WANTREFRESH;
}

bool CServerBrowserHttp::Validate(json_value *pJson)
{
	std::vector<CServerInfo> vServers;
	return Parse(pJson, &vServers);
}
bool CServerBrowserHttp::Parse(json_value *pJson, std::vector<CServerInfo> *pvServers)
{
	std::vector<CServerInfo> vServers;

	const json_value &Servers = *pJson;
	if(Servers.type != json_array)
	{
		return true;
	}
	for(unsigned int i = 0; i < Servers.u.array.length; i++)
	{
		const json_value &Server = Servers[i];
		const json_value &Port = Server["port"];
		const json_value &Addresses = Server["ip"];
		const json_value &Supported = Server["supports7"];
		CServerInfo2 ParsedInfo;
		if(Addresses.type != json_string || Supported.type != json_boolean || Port.type != json_integer)
		{
			return true;
		}

		if(Supported.u.boolean == 0)
		{
			continue;
		}
		if(CServerInfo2::FromJson(&ParsedInfo, &Server))
		{
			// Only skip the current server on parsing
			// failure; the server info is "user input" by
			// the game server and can be set to arbitrary
			// values.
			continue;
		}
		CServerInfo SetInfo = ParsedInfo;
		str_format(SetInfo.m_aAddress, sizeof(SetInfo.m_aAddress), "%s:%d", Addresses.u.string.ptr, static_cast<int>(Port.u.integer));
		net_addr_from_str(&SetInfo.m_NetAddr, SetInfo.m_aAddress);
		SetInfo.m_InfoGotByHttp = true;
		vServers.push_back(SetInfo);
	}
	*pvServers = vServers;
	return false;
}

IServerBrowserHttp *CreateServerBrowserHttp(IHttp *pHttp, CConfig *pConfig)
{
	return new CServerBrowserHttp(pHttp, pConfig);
}
