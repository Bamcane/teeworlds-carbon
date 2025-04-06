#ifndef ENGINE_LOCALIZATION_H
#define ENGINE_LOCALIZATION_H

#include "kernel.h"

struct SLanguageInfo
{
	const char *m_pCode;
	const char *m_pName;
};

class ILocalization : public IInterface
{
	MACRO_INTERFACE("localization", 0)
public:
	virtual ~ILocalization() = default;

	virtual void Init() = 0;
	virtual const char *Localize(const char *pCode, const char *pStr, const char *pContext) = 0;

	// return: size of pInfo.
	virtual int GetLanguagesInfo(SLanguageInfo **ppInfo) = 0;
};

extern ILocalization *CreateLocalization(class IStorage *pStorage, class IConsole *pConsole, class CConfig *pConfig);

#endif // ENGINE_SHARED_LOCALIZATION_H
