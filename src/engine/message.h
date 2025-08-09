/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef ENGINE_MESSAGE_H
#define ENGINE_MESSAGE_H

#include <base/uuid.h>
#include <engine/shared/packer.h>

class CMsgPacker : public CPacker
{
public:
	CMsgPacker(int Type, bool System = false, bool Carbon = false)
	{
		Reset();
		if(Type < 0 || Type > 0x3FFFFFFF)
		{
			m_Error = true;
			return;
		}
		if(Carbon)
		{
			AddInt(System ? 1 : 0);
			AddRaw(&UUID_CARBON_NAMESPACE, sizeof(UUID_CARBON_NAMESPACE));
			AddInt(Type);
		}
		else
			AddInt((Type << 1) | (System ? 1 : 0));
	}
};

class CMsgUnpacker : public CUnpacker
{
	int m_Type;
	bool m_System;
	const Uuid *m_pNamespace;

public:
	CMsgUnpacker() = default;

	CMsgUnpacker(const void *pData, int Size)
	{
		Reset(pData, Size);
		const int Msg = GetInt();
		if(Msg < 0)
			m_Error = true;
		if(m_Error)
		{
			m_System = false;
			m_Type = 0;
			return;
		}
		m_System = Msg & 1;
		m_Type = Msg >> 1;
		if(m_Type == 0)
		{
			m_pNamespace = (const Uuid *) GetRaw(sizeof(*m_pNamespace));
			if(m_Error)
			{
				m_System = false;
				m_Type = 0;
				m_pNamespace = nullptr;
				return;
			}
		}
	}

	int Type() const { return m_Type; }
	bool System() const { return m_System; }
	const Uuid *Namespace() const { return m_pNamespace; }
	void ModifyType(int Type) { m_Type = Type; }
};

#endif
