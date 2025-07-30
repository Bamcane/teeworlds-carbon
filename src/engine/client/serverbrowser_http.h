#ifndef ENGINE_CLIENT_SERVERBROWSER_HTTP_H
#define ENGINE_CLIENT_SERVERBROWSER_HTTP_H

class CServerInfo;
class IHttp;
class CConfig;

class IServerBrowserHttp
{
public:
	virtual ~IServerBrowserHttp() {}

	virtual void Update() = 0;

	virtual bool IsRefreshing() const = 0;
	virtual bool IsError() const = 0;
	virtual void Refresh() = 0;

	virtual int NumServers() const = 0;
	virtual const CServerInfo &Server(int Index) const = 0;
};

IServerBrowserHttp *CreateServerBrowserHttp(IHttp *pHttp, CConfig *pConfig);
#endif // ENGINE_CLIENT_SERVERBROWSER_HTTP_H
