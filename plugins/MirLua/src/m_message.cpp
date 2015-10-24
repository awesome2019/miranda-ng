#include "stdafx.h"

static int lua_Paste(lua_State *L)
{
	MCONTACT hContact = luaL_checkinteger(L, 1);
	ptrT text(mir_utf8decodeT(luaL_checkstring(L, 2)));

	MessageWindowInputData mwid = { sizeof(MessageWindowInputData) };
	mwid.hContact = hContact;
	mwid.uFlags = MSG_WINDOW_UFLAG_MSG_BOTH;

	MessageWindowData mwd = { sizeof(MessageWindowData) };

	INT_PTR res = ::CallService(MS_MSG_GETWINDOWDATA, (WPARAM)&mwid, (LPARAM)&mwd);
	lua_pushinteger(L, res);
	if (res)
		return 1;

	HWND hEdit = GetDlgItem(mwd.hwndWindow, 1002 /*IDC_MESSAGE*/);
	if (!hEdit) hEdit = GetDlgItem(mwd.hwndWindow, 1009 /*IDC_CHATMESSAGE*/);

	SendMessage(hEdit, EM_REPLACESEL, TRUE, (LPARAM)text);

	return 1;
}

static int lua_Send(lua_State *L)
{
	MCONTACT hContact = luaL_checkinteger(L, 1);
	const char *message = luaL_checkstring(L, 2);

	INT_PTR res = 1;

	const char *szProto = GetContactProto(hContact);
	if (db_get_b(hContact, szProto, "ChatRoom", 0) == TRUE)
	{
		ptrT tszChatRoom(db_get_tsa(hContact, szProto, "ChatRoomID"));
		GCDEST gcd = { szProto, tszChatRoom, GC_EVENT_SENDMESSAGE };
		GCEVENT gce = { sizeof(gce), &gcd };
		gce.bIsMe = TRUE;
		gce.dwFlags = GCEF_ADDTOLOG;
		gce.ptszText = mir_utf8decodeT(message);
		gce.time = time(NULL);

		res = ::CallServiceSync(MS_GC_EVENT, WINDOW_VISIBLE, (LPARAM)&gce);
		lua_pushinteger(L, res);

		mir_free((void*)gce.ptszText);
	}
	else if ((res = ::CallContactService(hContact, PSS_MESSAGE, 0, (LPARAM)message)) != ACKRESULT_FAILED)
	{
		DBEVENTINFO dbei;
		dbei.cbSize = sizeof(dbei);
		dbei.szModule = MODULE;
		dbei.timestamp = time(NULL);
		dbei.eventType = EVENTTYPE_MESSAGE;
		dbei.cbBlob = mir_strlen(message);
		dbei.pBlob = (PBYTE)mir_strdup(message);
		dbei.flags = DBEF_UTF | DBEF_SENT;
		::db_event_add(hContact, &dbei);

		lua_pushinteger(L, res);
		return 1;
	}

	lua_pushinteger(L, res);

	return 1;
}

int MsgWindowEventHookEventObjParam(void *obj, WPARAM wParam, LPARAM lParam, LPARAM param)
{
	lua_State *L = (lua_State*)obj;

	int ref = param;
	lua_rawgeti(L, LUA_REGISTRYINDEX, ref);

	lua_pushnumber(L, wParam);

	MessageWindowEventData *ev = (MessageWindowEventData*)lParam;

	lua_newtable(L);
	lua_pushliteral(L, "Module");
	lua_pushstring(L, ptrA(mir_utf8encode(ev->szModule)));
	lua_settable(L, -3);
	lua_pushliteral(L, "Type");
	lua_pushinteger(L, ev->uType);
	lua_settable(L, -3);
	lua_pushliteral(L, "hContact");
	lua_pushinteger(L, ev->hContact);
	lua_settable(L, -3);
	lua_pushliteral(L, "Flags");
	lua_pushinteger(L, ev->uFlags);
	lua_settable(L, -3);

	if (lua_pcall(L, 2, 1, 0))
		printf("%s\n", lua_tostring(L, -1));

	int res = (int)lua_tointeger(L, 1);

	return res;
}

static int lua_OnMsgWindowEvent(lua_State *L)
{
	ObsoleteMethod(L, "Use m.HookEvent instead");

	if (!lua_isfunction(L, 1))
	{
		lua_pushlightuserdata(L, NULL);
		return 1;
	}

	lua_pushvalue(L, 1);
	int ref = luaL_ref(L, LUA_REGISTRYINDEX);

	HANDLE res = ::HookEventObjParam(ME_MSG_WINDOWEVENT, MsgWindowEventHookEventObjParam, L, ref);
	lua_pushlightuserdata(L, res);

	CMLua::Hooks.insert(res);
	CMLua::HookRefs.insert(new HandleRefParam(L, res, ref));

	return 1;
}

static luaL_Reg messageApi[] =
{
	{ "Paste", lua_Paste },
	{ "Send", lua_Send },

	{ "OnMsgWindowEvent", lua_OnMsgWindowEvent },

	{ NULL, NULL }
};

#define MT_MESSAGEWINDOWEVENTDATA "MessageWindowEventData"

static int mwed__init(lua_State *L)
{
	MessageWindowEventData *udata = (MessageWindowEventData*)lua_touserdata(L, 1);
	if (udata == NULL)
	{
		lua_pushnil(L);
		return 1;
	}

	MessageWindowEventData **mwed = (MessageWindowEventData**)lua_newuserdata(L, sizeof(MessageWindowEventData*));
	*mwed = udata;

	luaL_setmetatable(L, MT_MESSAGEWINDOWEVENTDATA);

	return 1;
}

static int mwed__index(lua_State *L)
{
	MessageWindowEventData *mwed = *(MessageWindowEventData**)luaL_checkudata(L, 1, MT_MESSAGEWINDOWEVENTDATA);
	const char *key = lua_tostring(L, 2);

	if (!mir_strcmpi(key, "Module") == 0)
		lua_pushstring(L, ptrA(mir_utf8encode(mwed->szModule)));
	else if (!mir_strcmpi(key, "Type") == 0)
		lua_pushinteger(L, mwed->uType);
	else if (!mir_strcmpi(key, "hContact") == 0)
		lua_pushinteger(L, mwed->hContact);
	else if (!mir_strcmpi(key, "Flags") == 0)
		lua_pushinteger(L, mwed->uFlags);
	else
		lua_pushnil(L);

	return 1;
}

static luaL_Reg mwedMeta[] =
{
	{ "__init", mwed__init },
	{ "__index", mwed__index },
	{ NULL, NULL }
};

LUAMOD_API int luaopen_m_message(lua_State *L)
{
	luaL_newlib(L, messageApi);

	luaL_newmetatable(L, MT_MESSAGEWINDOWEVENTDATA);
	luaL_setfuncs(L, mwedMeta, 0);
	lua_pop(L, 1);

	return 1;
}
