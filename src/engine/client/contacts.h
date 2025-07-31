/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef ENGINE_CLIENT_CONTACTS_H
#define ENGINE_CLIENT_CONTACTS_H

#include <engine/shared/console.h>
#include <engine/config.h>
#include <engine/contacts.h>

class IContactList
{
private:
	CContactInfo m_aContacts[CContactInfo::MAX_CONTACTS];
	int m_NumContacts;

public:
	IContactList();

	void ConfigSave(IConfigManager *pConfigManager, const char* pCmdStr);

	virtual void Init() = 0;

	int NumContacts() const { return m_NumContacts; }
	const CContactInfo *GetContact(int Index) const;
	int GetContactState(const char *pName, const char *pClan) const;
	bool IsContact(const char *pName, const char *pClan, bool PlayersOnly) const;

	void AddContact(const char *pName, const char *pClan);
	void RemoveContact(const char *pName, const char *pClan);
	void RemoveContact(int Index);
};

class CFriends: public IFriends, public IContactList
{
public:
	static void ConfigSaveCallback(IConfigManager *pConfigManager, void *pUserData);

	void Init() override;

	// bridges
	inline int NumFriends() const override { return IContactList::NumContacts(); }
	inline const CContactInfo *GetFriend(int Index) const override { return IContactList::GetContact(Index); }
	inline int GetFriendState(const char *pName, const char *pClan) const override { return IContactList::GetContactState(pName, pClan); }
	inline bool IsFriend(const char *pName, const char *pClan, bool PlayersOnly) const override { return IContactList::IsContact(pName, pClan, PlayersOnly); }
	inline void AddFriend(const char *pName, const char *pClan) override { IContactList::AddContact(pName, pClan); }
	inline void RemoveFriend(const char *pName, const char *pClan) override { IContactList::RemoveContact(pName, pClan); }

	static void ConAddFriend(IConsole::IResult *pResult, void *pUserData);
	static void ConRemoveFriend(IConsole::IResult *pResult, void *pUserData);
};

class CBlacklist: public IBlacklist, public IContactList
{
public:
	static void ConfigSaveCallback(IConfigManager *pConfigManager, void *pUserData);

	void Init() override;

	// bridges
	inline bool IsIgnored(const char *pName, const char *pClan, bool PlayersOnly) const override { return IContactList::IsContact(pName, pClan, PlayersOnly); }
	inline void AddIgnoredPlayer(const char *pName, const char *pClan) override { IContactList::AddContact(pName, pClan); }
	inline void RemoveIgnoredPlayer(const char *pName, const char *pClan) override { IContactList::RemoveContact(pName, pClan); }

	static void ConAddIgnore(IConsole::IResult *pResult, void *pUserData);
	static void ConRemoveIgnore(IConsole::IResult *pResult, void *pUserData);
};

#endif
