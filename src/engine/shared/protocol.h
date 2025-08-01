/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef ENGINE_SHARED_PROTOCOL_H
#define ENGINE_SHARED_PROTOCOL_H

#include <base/system.h>

/*
	Connection diagram - How the initialization works.

	Client -> INFO -> Server
		Contains version info, name, and some other info.

	Client <- MAP <- Server
		Contains current map.

	Client -> READY -> Server
		The client has loaded the map and is ready to go,
		but the mod needs to send it's information aswell.
		modc_connected is called on the client and
		mods_connected is called on the server.
		The client should call client_entergame when the
		mod has done it's initialization.

	Client -> ENTERGAME -> Server
		Tells the server to start sending snapshots.
		client_entergame and server_client_enter is called.
*/

enum
{
	NETMSG_NULL = 0,

	// the first thing sent by the client
	// contains the version info for the client
	NETMSG_INFO = 1,

	// sent by server
	NETMSG_MAP_CHANGE, // sent when client should switch map
	NETMSG_MAP_DATA, // map transfer, contains a chunk of the map file
	NETMSG_SERVERINFO,
	NETMSG_CON_READY, // connection is ready, client should send start info
	NETMSG_SNAP, // normal snapshot, multiple parts
	NETMSG_SNAPEMPTY, // empty snapshot
	NETMSG_SNAPSINGLE, // ?
	NETMSG_SNAPSMALL, // todo 0.8: remove unused
	NETMSG_INPUTTIMING, // reports how off the input was
	NETMSG_RCON_AUTH_ON, // rcon authentication enabled
	NETMSG_RCON_AUTH_OFF, // rcon authentication disabled
	NETMSG_RCON_LINE, // line that should be printed to the remote console
	NETMSG_RCON_CMD_ADD,
	NETMSG_RCON_CMD_REM,

	NETMSG_AUTH_CHALLENGE, // unused
	NETMSG_AUTH_RESULT, // unused

	// sent by client
	NETMSG_READY, //
	NETMSG_ENTERGAME,
	NETMSG_INPUT, // contains the inputdata from the client
	NETMSG_RCON_CMD, //
	NETMSG_RCON_AUTH, //
	NETMSG_REQUEST_MAP_DATA, //

	NETMSG_AUTH_START, // unused
	NETMSG_AUTH_RESPONSE, // unused

	// sent by both
	NETMSG_PING,
	NETMSG_PING_REPLY,
	NETMSG_ERROR, // unused

	NETMSG_MAPLIST_ENTRY_ADD, // todo 0.8: move up
	NETMSG_MAPLIST_ENTRY_REM,
};

// this should be revised
enum
{
	SERVER_TICK_SPEED = 50,
	SERVERINFO_FLAG_PASSWORD = 0x1,
	SERVERINFO_FLAG_TIMESCORE = 0x2,
	SERVERINFO_LEVEL_MIN = 0,
	SERVERINFO_LEVEL_MAX = 2,

	MAX_CLIENTS = 64,
	// which means the max number of bots display together, isn't the max number of bots.
	MAX_BOTS = 32,
	SERVER_MAX_CLIENTS = MAX_CLIENTS - MAX_BOTS,

	MAX_INPUT_SIZE = 128,
	MAX_SNAPSHOT_PACKSIZE = 900,

	MAX_NAME_LENGTH = 16,
	MAX_NAME_ARRAY_SIZE = MAX_NAME_LENGTH * UTF8_BYTE_LENGTH + 1,
	MAX_CLAN_LENGTH = 12,
	MAX_CLAN_ARRAY_SIZE = MAX_CLAN_LENGTH * UTF8_BYTE_LENGTH + 1,
	MAX_SKIN_LENGTH = 24,
	MAX_SKIN_ARRAY_SIZE = MAX_SKIN_LENGTH * UTF8_BYTE_LENGTH + 1,

	// message packing
	MSGFLAG_VITAL = 1,
	MSGFLAG_FLUSH = 2,
	MSGFLAG_NORECORD = 4,
	MSGFLAG_RECORD = 8,
	MSGFLAG_NOSEND = 16
};

#endif
