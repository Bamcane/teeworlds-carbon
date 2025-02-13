/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */

#include "entity.h"
#include "gamecontext.h"
#include "player.h"

CEntity::CEntity(CGameWorld *pGameWorld, int ObjType, vec2 Pos, int ProximityRadius)
{
	m_pGameWorld = pGameWorld;

	m_pPrevTypeEntity = 0;
	m_pNextTypeEntity = 0;

	m_ID = Server()->SnapNewID();
	m_ObjType = ObjType;

	m_ProximityRadius = ProximityRadius;

	m_MarkedForDestroy = false;
	m_Pos = Pos;
}

CEntity::~CEntity()
{
	GameWorld()->RemoveEntity(this);
	Server()->SnapFreeID(m_ID);
}

int CEntity::NetworkClipped(int SnappingClient)
{
	return NetworkClipped(SnappingClient, m_Pos);
}

int CEntity::NetworkClipped(int SnappingClient, vec2 CheckPos)
{
	return ::NetworkClipped(SnappingClient, CheckPos, GameServer());
}

bool CEntity::GameLayerClipped(vec2 CheckPos)
{
	int rx = round_to_int(CheckPos.x) / 32;
	int ry = round_to_int(CheckPos.y) / 32;
	return (rx < -200 || rx >= GameServer()->Collision()->GetWidth() + 200) || (ry < -200 || ry >= GameServer()->Collision()->GetHeight() + 200);
}
