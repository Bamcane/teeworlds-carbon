#include <base/tl/sorted_array.h>
#include <base/uuid.h>

#include <engine/console.h>
#include <engine/storage.h>

#include "config.h"
#include "jsonparser.h"
#include "localization.h"
#include "memheap.h"

#include <memory>
#include <unordered_map>

class CString
{
public:
    unsigned m_Hash;
    unsigned m_ContextHash;
    const char *m_pReplacement;

    bool operator <(const CString &Other) const { return m_Hash < Other.m_Hash || (m_Hash == Other.m_Hash && m_ContextHash < Other.m_ContextHash); }
    bool operator <=(const CString &Other) const { return m_Hash < Other.m_Hash || (m_Hash == Other.m_Hash && m_ContextHash <= Other.m_ContextHash); }
    bool operator ==(const CString &Other) const { return m_Hash == Other.m_Hash && m_ContextHash == Other.m_ContextHash; }
};

class CLocalization : public ILocalization
{
    IConsole *m_pConsole;
	IStorage *m_pStorage;
	CConfig *m_pConfig;
protected:
    IConsole *Console() const { return m_pConsole; }
    IStorage *Storage() const { return m_pStorage; }
    CConfig *Config() const { return m_pConfig; }

public:
    CLocalization(IStorage *pStorage, IConsole *pConsole, CConfig *pConfig);
	~CLocalization() = default;

    void Init() override;
	const char *Localize(const char *pCode, const char *pStr, const char *pContext);

private:
    class CLanguage
    {
        sorted_array<CString> m_Strings;
        CHeap m_StringsHeap;

        char m_aCode[8];
        char m_aParent[8];
        char m_aName[32];
        bool m_Loaded;
    public:
        CLanguage(const char *pCode, const char *pName, const char *pParent);

        void AddString(const char *pKey, const char *pValue, const char *pContext);
        void Load(IStorage *pStorage, IConsole *pConsole);

        const char *FindString(unsigned Hash, unsigned ContextHash) const;
        const char *Localize(const char *pStr, const char *pContext) const;

        const char *Code() { return m_aCode; }
        const char *Name() { return m_aName; }
        const char *Parent() { return m_aParent; }
		inline bool IsLoaded() { return m_Loaded; }
    };
    std::unordered_map<Uuid, std::shared_ptr<CLanguage>> m_vpLanguages;

    void AddLanguage(const char *pCode, const char *pName, const char *pParent);
};

CLocalization::CLocalization(IStorage *pStorage, IConsole *pConsole, CConfig *pConfig) : ILocalization()
{
	m_pStorage = pStorage;
    m_pConsole = pConsole;
	m_pConfig = pConfig;

    m_vpLanguages.clear();
}

void CLocalization::Init()
{
    CJsonParser JsonParser;
    const json_value *pJsonData = JsonParser.ParseFile("data/languages/index.json", Storage());
	if(pJsonData == nullptr)
	{
		Console()->Print(IConsole::OUTPUT_LEVEL_ADDINFO, "l10n", JsonParser.Error());
		return;
	}
    const json_value &rStart = (*pJsonData)["languages"];
	if(rStart.type == json_array)
	{
		for(unsigned i = 0; i < rStart.u.array.length; ++i)
		{
			const char *pCode = (const char *)rStart[i]["code"];
			const char *pName = (const char *)rStart[i]["name"];
			const char *pParent = nullptr;
			if(rStart[i]["parent"].type == json_string)
				pParent = (const char *)rStart[i]["parent"];
			AddLanguage(pCode, pName, pParent);
        }
    }
	Console()->Print(IConsole::OUTPUT_LEVEL_ADDINFO, "l10n", "initialized l10n");
}

const char *CLocalization::Localize(const char *pCode, const char *pStr, const char *pContext)
{
	if(str_comp(pCode, "en") == 0)
		return pStr;

	Uuid LanguageUuid = CalculateUuid(pCode);
	if(!m_vpLanguages.count(LanguageUuid))
		return pStr;
	if(!m_vpLanguages[LanguageUuid]->IsLoaded())
		m_vpLanguages[LanguageUuid]->Load(Storage(), Console());
	return m_vpLanguages[LanguageUuid]->Localize(pStr, pContext);
}

CLocalization::CLanguage::CLanguage(const char *pCode, const char *pName, const char *pParent)
{
    str_copy(m_aCode, pCode, sizeof(m_aCode));
    str_copy(m_aName, pName, sizeof(m_aName));
    str_copy(m_aParent, pParent ? pParent : "en", sizeof(m_aParent));
    m_Loaded = false;

    m_Strings.clear();
    m_StringsHeap.Reset();
}

void CLocalization::CLanguage::AddString(const char *pKey, const char *pValue, const char *pContext)
{
	CString s;
	s.m_Hash = str_quickhash(pKey);
	s.m_ContextHash = str_quickhash(pContext);
	s.m_pReplacement = m_StringsHeap.StoreString(*pValue ? pValue : pKey);
	m_Strings.add(s);
}

void CLocalization::CLanguage::Load(IStorage *pStorage, IConsole *pConsole)
{
    char aPath[IO_MAX_PATH_LENGTH];
    str_format(aPath, sizeof(aPath), "data/languages/%s.json", m_aCode);

    CJsonParser JsonParser;
    const json_value *pJsonData = JsonParser.ParseFile(aPath, pStorage);
	if(pJsonData == nullptr)
	{
		pConsole->Print(IConsole::OUTPUT_LEVEL_ADDINFO, "l10n", JsonParser.Error());
		return;
	}

    char aBuf[256];
	str_format(aBuf, sizeof(aBuf), "loaded '%s'", aPath);
	pConsole->Print(IConsole::OUTPUT_LEVEL_ADDINFO, "l10n", aBuf);
	m_Strings.clear();
	m_StringsHeap.Reset();

	// extract data
	const json_value &rStart = (*pJsonData)["translated strings"];
	if(rStart.type == json_array)
	{
		for(unsigned i = 0; i < rStart.u.array.length; ++i)
		{
			bool Valid = true;
			const char *pKey = (const char *)rStart[i]["key"];
			const char *pValue = (const char *)rStart[i]["value"];
			while(pKey[0] && pValue[0])
			{
				for(; pKey[0] && pKey[0] != '%'; ++pKey);
				for(; pValue[0] && pValue[0] != '%'; ++pValue);
				if(pKey[0] && pValue[0] && ((pKey[1] == ' ' && pValue[1] == 0) || (pKey[1] == 0 && pValue[1] == ' ')))	// skip  false positive
					break;
				if((pKey[0] && (!pValue[0] || pKey[1] != pValue[1])) || (pValue[0] && (!pKey[0] || pValue[1] != pKey[1])))
				{
					Valid = false;
					str_format(aBuf, sizeof(aBuf), "skipping invalid entry key:'%s', value:'%s'", (const char *)rStart[i]["key"], (const char *)rStart[i]["value"]);
					pConsole->Print(IConsole::OUTPUT_LEVEL_ADDINFO, "l10n", aBuf);
					break;
				}
				if(pKey[0])
					++pKey;
				if(pValue[0])
					++pValue;
			}
			if(Valid)
				AddString((const char *)rStart[i]["key"], (const char *)rStart[i]["value"], (const char *)rStart[i]["context"]);
		}
	}

    m_Loaded = true;

	return;
}

const char *CLocalization::CLanguage::FindString(unsigned Hash, unsigned ContextHash) const
{
	CString String;
	String.m_Hash = Hash;
	String.m_ContextHash = ContextHash;
	String.m_pReplacement = nullptr;
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

const char *CLocalization::CLanguage::Localize(const char *pStr, const char *pContext) const
{
    const char *pNewStr = FindString(str_quickhash(pStr), str_quickhash(pContext));
    return pNewStr ? pNewStr : pStr;
}

void CLocalization::AddLanguage(const char *pCode, const char *pName, const char *pParent)
{
    Uuid LanguageUuid = CalculateUuid(pCode);
    if(m_vpLanguages.count(LanguageUuid))
        return;

    std::shared_ptr<CLanguage> pLanguage = std::make_shared<CLanguage>(pCode, pName, pParent);
    m_vpLanguages[LanguageUuid] = pLanguage;

	if(str_comp(Config()->m_SvDefaultLanguage, pCode) == 0)
		pLanguage->Load(Storage(), Console());
}

ILocalization *CreateLocalization(IStorage *pStorage, IConsole *pConsole, CConfig *pConfig)
{
    return new CLocalization(pStorage, pConsole, pConfig);
}