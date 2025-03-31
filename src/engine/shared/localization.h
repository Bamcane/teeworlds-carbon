#ifndef ENGINE_SHARED_LOCALIZATION_H
#define ENGINE_SHARED_LOCALIZATION_H

class ILocalization
{
public:
	virtual ~ILocalization() = default;

	virtual void Init() = 0;
	virtual const char *Localize(const char *pCode, const char *pStr, const char *pContext) = 0;
};

extern ILocalization *CreateLocalization(class IStorage *pStorage, class IConsole *pConsole, class CConfig *pConfig);

#endif // ENGINE_SHARED_LOCALIZATION_H