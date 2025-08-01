/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */

#include <base/system.h>
#include <base/tl/algorithm.h>

#include <engine/console.h>
#include <engine/storage.h>

#include <engine/shared/jsonparser.h>

#include "localization.h"

const char *Localize(const char *pStr, const char *pContext)
{
	const char *pNewStr = g_Localization.FindString(str_quickhash(pStr), str_quickhash(pContext));
	return pNewStr ? pNewStr : pStr;
}

CLocConstString::CLocConstString(const char *pStr, const char *pContext)
{
	m_pDefaultStr = pStr;
	m_Hash = str_quickhash(m_pDefaultStr);
	m_ContextHash = str_quickhash(pContext);
	m_Version = -1;
}

void CLocConstString::Reload()
{
	m_Version = g_Localization.Version();
	const char *pNewStr = g_Localization.FindString(m_Hash, m_ContextHash);
	m_pCurrentStr = pNewStr;
	if(!m_pCurrentStr)
		m_pCurrentStr = m_pDefaultStr;
}

CLocalizationDatabase::CLocalizationDatabase()
{
	m_VersionCounter = 0;
	m_CurrentVersion = 0;
}

void CLocalizationDatabase::AddString(const char *pOrgStr, const char *pNewStr, const char *pContext)
{
	CString s;
	s.m_Hash = str_quickhash(pOrgStr);
	s.m_ContextHash = str_quickhash(pContext);
	s.m_pReplacement = m_StringsHeap.StoreString(*pNewStr ? pNewStr : pOrgStr);
	m_Strings.add(s);
}

bool CLocalizationDatabase::Load(const char *pFilename, IStorage *pStorage, IConsole *pConsole)
{
	// empty string means unload
	if(pFilename[0] == 0)
	{
		m_Strings.clear();
		m_StringsHeap.Reset();
		m_CurrentVersion = 0;
		return true;
	}

	CJsonParser JsonParser;
	const json_value *pJsonData = JsonParser.ParseFile(pFilename, pStorage);
	if(pJsonData == 0)
	{
		pConsole->Print(IConsole::OUTPUT_LEVEL_ADDINFO, "localization", JsonParser.Error());
		return false;
	}

	char aBuf[256];
	str_format(aBuf, sizeof(aBuf), "loaded '%s'", pFilename);
	pConsole->Print(IConsole::OUTPUT_LEVEL_ADDINFO, "localization", aBuf);
	m_Strings.clear();
	m_StringsHeap.Reset();

	// extract data
	const json_value &rStart = (*pJsonData)["translated strings"];
	if(rStart.type == json_array)
	{
		for(unsigned i = 0; i < rStart.u.array.length; ++i)
		{
			bool Valid = true;
			const char *pOr = (const char *) rStart[i]["or"];
			const char *pTr = (const char *) rStart[i]["tr"];
			while(pOr[0] && pTr[0])
			{
				for(; pOr[0] && pOr[0] != '%'; ++pOr)
					;
				for(; pTr[0] && pTr[0] != '%'; ++pTr)
					;
				if(pOr[0] && pTr[0] && ((pOr[1] == ' ' && pTr[1] == 0) || (pOr[1] == 0 && pTr[1] == ' '))) // skip  false positive
					break;
				if((pOr[0] && (!pTr[0] || pOr[1] != pTr[1])) || (pTr[0] && (!pOr[0] || pTr[1] != pOr[1])))
				{
					Valid = false;
					str_format(aBuf, sizeof(aBuf), "skipping invalid entry or:'%s', tr:'%s'", (const char *) rStart[i]["or"], (const char *) rStart[i]["tr"]);
					pConsole->Print(IConsole::OUTPUT_LEVEL_ADDINFO, "localization", aBuf);
					break;
				}
				if(pOr[0])
					++pOr;
				if(pTr[0])
					++pTr;
			}
			if(Valid)
				AddString((const char *) rStart[i]["or"], (const char *) rStart[i]["tr"], (const char *) rStart[i]["context"]);
		}
	}

	m_CurrentVersion = ++m_VersionCounter;
	return true;
}

const char *CLocalizationDatabase::FindString(unsigned Hash, unsigned ContextHash) const
{
	CString String;
	String.m_Hash = Hash;
	String.m_ContextHash = ContextHash;
	String.m_pReplacement = 0x0;
	sorted_array<CString>::range r = ::find_binary(m_Strings.all(), String);
	if(r.empty())
		return 0;

	unsigned DefaultHash = str_quickhash("");
	unsigned DefaultIndex = 0;
	for(unsigned i = 0; i < r.size() && r.index(i).m_Hash == Hash; ++i)
	{
		const CString &rStr = r.index(i);
		if(rStr.m_ContextHash == ContextHash)
			return rStr.m_pReplacement;
		else if(rStr.m_ContextHash == DefaultHash)
			DefaultIndex = i;
	}

	return r.index(DefaultIndex).m_pReplacement;
}

CLocalizationDatabase g_Localization;
