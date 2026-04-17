// vim: set ts=4 sw=4 tw=99 noet:
//
// AMX Mod X, based on AMX Mod by Aleksander Naszko ("OLO").
// Copyright (C) The AMX Mod X Development Team.
//
// This software is licensed under the GNU General Public License, version 3 or higher.
// Additional exceptions apply. For full license details, see LICENSE.txt or visit:
//     https://alliedmods.net/amxmodx-license

//
// Ham Sandwich Module
//

#include "amxxmodule.h"

#include <amtl/am-vector.h>
#include <amtl/am-string.h>
#include <sh_stack.h>
#include "lualib/lua.hpp"
#include "DataHandler.h"

#include "ham_const.h"
#include "ham_utils.h"

CStack< Data * > ReturnStack;
CStack< Data * > OrigReturnStack;
CStack< ke::Vector< Data * > * > ParamStack;
CStack< int * > ReturnStatus;
lua_State* g_L = nullptr;

static inline void lua_setintconst(lua_State* L, const char* name, int value)
{
	lua_pushinteger(L, value);
	lua_setglobal(L, name);
}
#define CHECK_STACK(__STACK__)								\
	if (  ( __STACK__ ).size() <= 0)						\
	{																	\
		MF_LogError(amx, AMX_ERR_NATIVE, "%s is empty!", #__STACK__);	\
		return 0;														\
	}

#define PARSE_RETURN()										\
	if (ret==-2)											\
	{														\
		MF_LogError(amx, AMX_ERR_NATIVE, "Data pointer is NULL!");	\
	}														\
	else if (ret==-1)										\
	{														\
		MF_LogError(amx, AMX_ERR_NATIVE, "Wrong data type (data is of type %s)", returntypes[dat->GetType()]);	\
	}														\
	return ret

static const char *returntypes[] =
{
	"void",
	"integer",
	"float",
	"vector",
	"string",
	"entity",
	"entity",
	"traceresult",
	"iteminfo"
};

static cell AMX_NATIVE_CALL GetHamReturnInteger(AMX *amx, cell *params)
{
	CHECK_STACK(ReturnStack);
	Data *dat=ReturnStack.front();

	int ret=dat->GetInt(MF_GetAmxAddr(amx, params[1]));
	PARSE_RETURN();
}
static cell AMX_NATIVE_CALL GetOrigHamReturnInteger(AMX *amx, cell *params)
{
	CHECK_STACK(OrigReturnStack);
	Data *dat=OrigReturnStack.front();

	int ret=dat->GetInt(MF_GetAmxAddr(amx, params[1]));
	PARSE_RETURN();
}
static cell AMX_NATIVE_CALL GetHamReturnFloat(AMX *amx, cell *params)
{
	CHECK_STACK(ReturnStack);
	Data *dat=ReturnStack.front();

	int ret=dat->GetFloat(MF_GetAmxAddr(amx, params[1]));
	PARSE_RETURN();
}
static cell AMX_NATIVE_CALL GetOrigHamReturnFloat(AMX *amx, cell *params)
{
	CHECK_STACK(OrigReturnStack);
	Data *dat=OrigReturnStack.front();

	int ret=dat->GetFloat(MF_GetAmxAddr(amx, params[1]));
	PARSE_RETURN();
}
static cell AMX_NATIVE_CALL GetHamReturnVector(AMX *amx, cell *params)
{
	CHECK_STACK(ReturnStack);
	Data *dat=ReturnStack.front();

	int ret=dat->GetVector(MF_GetAmxAddr(amx, params[1]));
	PARSE_RETURN();
}
static cell AMX_NATIVE_CALL GetOrigHamReturnVector(AMX *amx, cell *params)
{
	CHECK_STACK(OrigReturnStack);
	Data *dat=OrigReturnStack.front();

	int ret=dat->GetVector(MF_GetAmxAddr(amx, params[1]));
	PARSE_RETURN();
}
static cell AMX_NATIVE_CALL GetHamReturnEntity(AMX *amx, cell *params)
{
	CHECK_STACK(ReturnStack);
	Data *dat=ReturnStack.front();

	int ret=dat->GetEntity(MF_GetAmxAddr(amx, params[1]));
	PARSE_RETURN();
}
static cell AMX_NATIVE_CALL GetOrigHamReturnEntity(AMX *amx, cell *params)
{
	CHECK_STACK(OrigReturnStack);
	Data *dat=OrigReturnStack.front();

	int ret=dat->GetEntity(MF_GetAmxAddr(amx, params[1]));
	PARSE_RETURN();
}
static cell AMX_NATIVE_CALL GetHamReturnString(AMX *amx, cell *params)
{
	CHECK_STACK(ReturnStack);
	Data *dat=ReturnStack.front();

	int ret=dat->GetString(MF_GetAmxAddr(amx, params[1]), params[2]);
	PARSE_RETURN();
}
static cell AMX_NATIVE_CALL GetOrigHamReturnString(AMX *amx, cell *params)
{
	CHECK_STACK(OrigReturnStack);
	Data *dat=OrigReturnStack.front();

	int ret=dat->GetString(MF_GetAmxAddr(amx, params[1]), params[2]);
	PARSE_RETURN();
}
static cell AMX_NATIVE_CALL SetHamReturnInteger(AMX *amx, cell *params)
{
	CHECK_STACK(ReturnStack);
	Data *dat=ReturnStack.front();

	int ret=dat->SetInt(&params[1]);
	PARSE_RETURN();
}
static cell AMX_NATIVE_CALL SetHamReturnFloat(AMX *amx, cell *params)
{
	CHECK_STACK(ReturnStack);
	Data *dat=ReturnStack.front();

	int ret=dat->SetFloat(&params[1]);
	PARSE_RETURN();
}
static cell AMX_NATIVE_CALL SetHamReturnVector(AMX *amx, cell *params)
{
	CHECK_STACK(ReturnStack);
	Data *dat=ReturnStack.front();

	int ret=dat->SetVector(MF_GetAmxAddr(amx, params[1]));
	PARSE_RETURN();
}
static cell AMX_NATIVE_CALL SetHamReturnEntity(AMX *amx, cell *params)
{
	CHECK_STACK(ReturnStack);
	Data *dat=ReturnStack.front();

	int ret=dat->SetEntity(&params[1]);
	PARSE_RETURN();
}
static cell AMX_NATIVE_CALL SetHamReturnString(AMX *amx, cell *params)
{
	CHECK_STACK(ReturnStack);
	Data *dat=ReturnStack.front();

	int ret=dat->SetString(MF_GetAmxAddr(amx, params[1]));
	PARSE_RETURN();
}
static cell AMX_NATIVE_CALL SetHamParamInteger(AMX *amx, cell *params)
{
	CHECK_STACK(ParamStack);
	ke::Vector<Data *> *vec = ParamStack.front();
	if (vec->length() < (unsigned)params[1]) 
	{ 
		MF_LogError(amx, AMX_ERR_NATIVE, "Invalid parameter number, got %d, expected %d", params[1], vec->length()); 
		return 0; 
	} 
	Data *dat=vec->at(params[1] - 1);

	int ret=dat->SetInt(&params[2]);
	PARSE_RETURN();
}
static cell AMX_NATIVE_CALL SetHamParamTraceResult(AMX *amx, cell *params)
{
	if (params[2] == 0)
	{
		MF_LogError(amx, AMX_ERR_NATIVE, "Null traceresult provided.");

		return 0;
	}
	CHECK_STACK(ParamStack);
	ke::Vector<Data *> *vec = ParamStack.front();
	if (vec->length() < (unsigned)params[1]) 
	{ 
		MF_LogError(amx, AMX_ERR_NATIVE, "Invalid parameter number, got %d, expected %d", params[1], vec->length()); 
		return 0; 
	} 
	Data *dat=vec->at(params[1] - 1);

	int ret=dat->SetInt(&params[2]);
	PARSE_RETURN();
}
static cell AMX_NATIVE_CALL SetHamParamFloat(AMX *amx, cell *params)
{
	CHECK_STACK(ParamStack);
	ke::Vector<Data *> *vec = ParamStack.front();
	if (vec->length() < (unsigned)params[1] || params[1] < 1) 
	{ 
		MF_LogError(amx, AMX_ERR_NATIVE, "Invalid parameter number, got %d, expected %d", params[1], vec->length()); 
		return 0; 
	} 
	Data *dat=vec->at(params[1] - 1);

	int ret=dat->SetFloat(&params[2]);
	PARSE_RETURN();
}
static cell AMX_NATIVE_CALL SetHamParamVector(AMX *amx, cell *params)
{
	CHECK_STACK(ParamStack);
	ke::Vector<Data *> *vec = ParamStack.front();
	if (vec->length() < (unsigned)params[1]) 
	{ 
		MF_LogError(amx, AMX_ERR_NATIVE, "Invalid parameter number, got %d, expected %d", params[1], vec->length()); 
		return 0; 
	} 
	Data *dat=vec->at(params[1] - 1);

	int ret=dat->SetVector(MF_GetAmxAddr(amx, params[2]));
	PARSE_RETURN();
}

cell SetParamEntity(AMX *amx, cell *params, bool updateIndex)
{
	CHECK_STACK(ParamStack);
	ke::Vector<Data *> *vec = ParamStack.front();
	if (vec->length() < (unsigned)params[1])
	{
		MF_LogError(amx, AMX_ERR_NATIVE, "Invalid parameter number, got %d, expected %d", params[1], vec->length());
		return 0;
	}
	Data *dat = vec->at(params[1] - 1);

	int ret = dat->SetEntity(&params[2], updateIndex);
	PARSE_RETURN();
}

static cell AMX_NATIVE_CALL SetHamParamEntity(AMX *amx, cell *params)
{
	return SetParamEntity(amx, params, false);
}

static cell AMX_NATIVE_CALL SetHamParamEntity2(AMX *amx, cell *params)
{
	return SetParamEntity(amx, params, true);
}

static cell AMX_NATIVE_CALL SetHamParamString(AMX *amx, cell *params)
{
	CHECK_STACK(ParamStack);
	ke::Vector<Data *> *vec=ParamStack.front(); 
	if (vec->length() < (unsigned)params[1]) 
	{ 
		MF_LogError(amx, AMX_ERR_NATIVE, "Invalid parameter number, got %d, expected %d", params[1], vec->length()); 
		return 0; 
	} 
	Data *dat=vec->at(params[1] - 1);

	int ret=dat->SetString(MF_GetAmxAddr(amx, params[2]));
	PARSE_RETURN();
}

static cell AMX_NATIVE_CALL SetHamParamItemInfo(AMX *amx, cell *params)
{
	if (params[2] == 0)
	{
		MF_LogError(amx, AMX_ERR_NATIVE, "Null ItemInfo handle provided.");
		return 0;
	}

	CHECK_STACK(ParamStack);
	ke::Vector<Data *> *vec = ParamStack.front();

	if (vec->length() < (unsigned)params[1])
	{
		MF_LogError(amx, AMX_ERR_NATIVE, "Invalid parameter number, got %d, expected %d", params[1], vec->length());
		return 0;
	}

	Data *dat = vec->at(params[1] - 1);

	int ret = dat->SetInt(&params[2]);
	PARSE_RETURN();
}


static cell AMX_NATIVE_CALL GetHamItemInfo(AMX *amx, cell *params)
{
	if (params[1] == 0)
	{
		MF_LogError(amx, AMX_ERR_NATIVE, "Null iteminfo handle provided.");
		return 0;
	}

	int type = params[2];

	if ((type == ItemInfo_pszAmmo1 || type == ItemInfo_pszAmmo2 || type == ItemInfo_pszName) && (*params / sizeof(cell)) != 4)
	{
		MF_LogError(amx, AMX_ERR_NATIVE, "Bad arg count.  Expected %d, got %d.", 4, *params / sizeof(cell));
		return 0;
	}

	ItemInfo *pItem = reinterpret_cast<ItemInfo *>(params[1]);

	switch (type)
	{
		case ItemInfo_iSlot:
			return pItem->iSlot;

		case ItemInfo_iPosition:
			return pItem->iPosition;

		case ItemInfo_pszAmmo1:
			return MF_SetAmxString( amx, params[3], pItem->pszAmmo1 ? pItem->pszAmmo1 : "", params[4] );

		case ItemInfo_iMaxAmmo1:
			return pItem->iMaxAmmo1;

		case ItemInfo_pszAmmo2:
			return MF_SetAmxString( amx, params[3], pItem->pszAmmo2 ? pItem->pszAmmo2 : "", params[4] );

		case ItemInfo_iMaxAmmo2:
			return pItem->iMaxAmmo2;

		case ItemInfo_pszName:
			return MF_SetAmxString( amx, params[3], pItem->pszName ? pItem->pszName : "", params[4] );

		case ItemInfo_iMaxClip:
			return pItem->iMaxClip;

		case ItemInfo_iId:
			return pItem->iId;

		case ItemInfo_iFlags:
			return pItem->iFlags;

		case ItemInfo_iWeight:
			return pItem->iWeight;
	}

	MF_LogError(amx, AMX_ERR_NATIVE, "Unknown ItemInfo type %d", type);
	return 0;
}

CStack<ItemInfo *> g_FreeIIs;

static cell AMX_NATIVE_CALL CreateHamItemInfo(AMX *amx, cell *params)
{
	ItemInfo *ii;

	if (g_FreeIIs.empty())
	{
		ii = new ItemInfo;
	}
	else 
	{
		ii = g_FreeIIs.front();
		g_FreeIIs.pop();
	}

	memset(ii, 0, sizeof(ItemInfo));

	return reinterpret_cast<cell>(ii);
}

static cell AMX_NATIVE_CALL FreeHamItemInfo(AMX *amx, cell *params)
{
	ItemInfo *ii = reinterpret_cast<ItemInfo *>(params[1]);

	if (!ii)
	{
		return 0;
	}

	g_FreeIIs.push(ii);

	return 1;
}


static cell AMX_NATIVE_CALL SetHamItemInfo(AMX *amx, cell *params)
{
	if (params[1] == 0)
	{
		MF_LogError(amx, AMX_ERR_NATIVE, "Null iteminfo handle provided.");
		return 0;
	}

	ItemInfo *pItem = reinterpret_cast<ItemInfo *>(params[1]);
	cell *ptr = MF_GetAmxAddr(amx, params[3]);
	int iLen;

	switch (params[2])
	{
		case ItemInfo_iSlot:
			pItem->iSlot = *ptr;
			break;

		case ItemInfo_iPosition:
			pItem->iPosition = *ptr;
			break;

		case ItemInfo_pszAmmo1:
			pItem->pszAmmo1 = MF_GetAmxString(amx, params[3], 0, &iLen);
			return iLen;

		case ItemInfo_iMaxAmmo1:
			pItem->iMaxAmmo1 = *ptr;
			break;

		case ItemInfo_pszAmmo2:
			pItem->pszAmmo2 = MF_GetAmxString(amx, params[3], 0, &iLen);
			return iLen;

		case ItemInfo_iMaxAmmo2:
			pItem->iMaxAmmo2 = *ptr;
			break;

		case ItemInfo_pszName:
			pItem->pszName = MF_GetAmxString(amx, params[3], 0, &iLen);
			return iLen;

		case ItemInfo_iMaxClip:
			pItem->iMaxClip = *ptr;
			break;

		case ItemInfo_iId:
			pItem->iId = *ptr;
			break;

		case ItemInfo_iFlags:
			pItem->iFlags = *ptr;
			break;

		case ItemInfo_iWeight:
			pItem->iWeight = *ptr;
			break;

		default:
			MF_LogError(amx, AMX_ERR_NATIVE, "Unknown ItemInfo type %d", params[2]);
			return 0;
	}

	return 1;
}

static cell AMX_NATIVE_CALL GetHamReturnStatus(AMX *amx, cell *params)
{
	CHECK_STACK(ReturnStatus);
	int *i=ReturnStatus.front();

	return *i;
}

static int L_GetHamReturnInteger(lua_State* L)
{
	if (ReturnStack.size() <= 0)
	{
		lua_pushnil(L);
		return 1;
	}
	Data* dat = ReturnStack.front();
	cell value = 0;
	if (dat->GetInt(&value) < 0)
	{
		lua_pushnil(L);
		return 1;
	}
	lua_pushinteger(L, value);
	return 1;
}

static int L_GetHamReturnFloat(lua_State* L)
{
	if (ReturnStack.size() <= 0)
	{
		lua_pushnil(L);
		return 1;
	}
	Data* dat = ReturnStack.front();
	cell value = 0;
	if (dat->GetFloat(&value) < 0)
	{
		lua_pushnil(L);
		return 1;
	}
	lua_pushnumber(L, amx_ctof(value));
	return 1;
}

static int L_GetHamReturnVector(lua_State* L)
{
	if (ReturnStack.size() <= 0)
	{
		lua_pushnil(L);
		return 1;
	}
	Data* dat = ReturnStack.front();
	cell value[3] = {0, 0, 0};
	if (dat->GetVector(value) < 0)
	{
		lua_pushnil(L);
		return 1;
	}
	lua_newtable(L);
	lua_pushnumber(L, amx_ctof(value[0])); lua_rawseti(L, -2, 1);
	lua_pushnumber(L, amx_ctof(value[1])); lua_rawseti(L, -2, 2);
	lua_pushnumber(L, amx_ctof(value[2])); lua_rawseti(L, -2, 3);
	return 1;
}

static int L_GetHamReturnEntity(lua_State* L)
{
	if (ReturnStack.size() <= 0)
	{
		lua_pushnil(L);
		return 1;
	}
	Data* dat = ReturnStack.front();
	cell value = 0;
	if (dat->GetEntity(&value) < 0)
	{
		lua_pushnil(L);
		return 1;
	}
	lua_pushinteger(L, value);
	return 1;
}

static int L_GetHamReturnString(lua_State* L)
{
	if (ReturnStack.size() <= 0)
	{
		lua_pushnil(L);
		return 1;
	}
	Data* dat = ReturnStack.front();
	cell buffer[1024] = {0};
	if (dat->GetString(buffer, 1023) < 0)
	{
		lua_pushnil(L);
		return 1;
	}
	char temp[1024];
	size_t i = 0;
	for (; i < 1023 && buffer[i] != 0; ++i)
	{
		temp[i] = static_cast<char>(buffer[i]);
	}
	temp[i] = '\0';
	lua_pushstring(L, temp);
	return 1;
}

static int L_GetOrigHamReturnInteger(lua_State* L)
{
	if (OrigReturnStack.size() <= 0)
	{
		lua_pushnil(L);
		return 1;
	}
	Data* dat = OrigReturnStack.front();
	cell value = 0;
	if (dat->GetInt(&value) < 0)
	{
		lua_pushnil(L);
		return 1;
	}
	lua_pushinteger(L, value);
	return 1;
}

static int L_GetOrigHamReturnFloat(lua_State* L)
{
	if (OrigReturnStack.size() <= 0)
	{
		lua_pushnil(L);
		return 1;
	}
	Data* dat = OrigReturnStack.front();
	cell value = 0;
	if (dat->GetFloat(&value) < 0)
	{
		lua_pushnil(L);
		return 1;
	}
	lua_pushnumber(L, amx_ctof(value));
	return 1;
}

static int L_GetOrigHamReturnVector(lua_State* L)
{
	if (OrigReturnStack.size() <= 0)
	{
		lua_pushnil(L);
		return 1;
	}
	Data* dat = OrigReturnStack.front();
	cell value[3] = {0, 0, 0};
	if (dat->GetVector(value) < 0)
	{
		lua_pushnil(L);
		return 1;
	}
	lua_newtable(L);
	lua_pushnumber(L, amx_ctof(value[0])); lua_rawseti(L, -2, 1);
	lua_pushnumber(L, amx_ctof(value[1])); lua_rawseti(L, -2, 2);
	lua_pushnumber(L, amx_ctof(value[2])); lua_rawseti(L, -2, 3);
	return 1;
}

static int L_GetOrigHamReturnEntity(lua_State* L)
{
	if (OrigReturnStack.size() <= 0)
	{
		lua_pushnil(L);
		return 1;
	}
	Data* dat = OrigReturnStack.front();
	cell value = 0;
	if (dat->GetEntity(&value) < 0)
	{
		lua_pushnil(L);
		return 1;
	}
	lua_pushinteger(L, value);
	return 1;
}

static int L_GetOrigHamReturnString(lua_State* L)
{
	if (OrigReturnStack.size() <= 0)
	{
		lua_pushnil(L);
		return 1;
	}
	Data* dat = OrigReturnStack.front();
	cell buffer[1024] = {0};
	if (dat->GetString(buffer, 1023) < 0)
	{
		lua_pushnil(L);
		return 1;
	}
	char temp[1024];
	size_t i = 0;
	for (; i < 1023 && buffer[i] != 0; ++i)
	{
		temp[i] = static_cast<char>(buffer[i]);
	}
	temp[i] = '\0';
	lua_pushstring(L, temp);
	return 1;
}

static int L_SetHamReturnInteger(lua_State* L)
{
	if (ReturnStack.size() <= 0)
	{
		lua_pushboolean(L, 0);
		return 1;
	}
	Data* dat = ReturnStack.front();
	cell value = static_cast<cell>(luaL_checkinteger(L, 1));
	lua_pushboolean(L, dat->SetInt(&value) == 0);
	return 1;
}

static int L_SetHamReturnFloat(lua_State* L)
{
	if (ReturnStack.size() <= 0)
	{
		lua_pushboolean(L, 0);
		return 1;
	}
	Data* dat = ReturnStack.front();
	cell value = amx_ftoc(static_cast<float>(luaL_checknumber(L, 1)));
	lua_pushboolean(L, dat->SetFloat(&value) == 0);
	return 1;
}

static int L_SetHamReturnVector(lua_State* L)
{
	if (ReturnStack.size() <= 0)
	{
		lua_pushboolean(L, 0);
		return 1;
	}
	Data* dat = ReturnStack.front();
	cell value[3];
	for (int i = 0; i < 3; ++i)
	{
		lua_rawgeti(L, 1, i + 1);
		value[i] = amx_ftoc(static_cast<float>(luaL_checknumber(L, -1)));
		lua_pop(L, 1);
	}
	lua_pushboolean(L, dat->SetVector(value) == 0);
	return 1;
}

static int L_SetHamReturnEntity(lua_State* L)
{
	if (ReturnStack.size() <= 0)
	{
		lua_pushboolean(L, 0);
		return 1;
	}
	Data* dat = ReturnStack.front();
	cell value = static_cast<cell>(luaL_checkinteger(L, 1));
	lua_pushboolean(L, dat->SetEntity(&value) == 0);
	return 1;
}

static int L_SetHamReturnString(lua_State* L)
{
	if (ReturnStack.size() <= 0)
	{
		lua_pushboolean(L, 0);
		return 1;
	}
	Data* dat = ReturnStack.front();
	const char* str = luaL_checkstring(L, 1);
	size_t len = strlen(str);
	cell* temp = new cell[len + 1];
	for (size_t i = 0; i < len; ++i)
	{
		temp[i] = static_cast<unsigned char>(str[i]);
	}
	temp[len] = 0;
	int ok = (dat->SetString(temp) == 0);
	delete[] temp;
	lua_pushboolean(L, ok);
	return 1;
}

static int L_SetHamParamInteger(lua_State* L)
{
	if (ParamStack.size() <= 0)
	{
		lua_pushboolean(L, 0);
		return 1;
	}
	int index = static_cast<int>(luaL_checkinteger(L, 1));
	cell value = static_cast<cell>(luaL_checkinteger(L, 2));
	ke::Vector<Data *> *vec = ParamStack.front();
	if (index < 1 || static_cast<size_t>(index) > vec->length())
	{
		lua_pushboolean(L, 0);
		return 1;
	}
	lua_pushboolean(L, vec->at(index - 1)->SetInt(&value) == 0);
	return 1;
}

static int L_SetHamParamFloat(lua_State* L)
{
	if (ParamStack.size() <= 0)
	{
		lua_pushboolean(L, 0);
		return 1;
	}
	int index = static_cast<int>(luaL_checkinteger(L, 1));
	cell value = amx_ftoc(static_cast<float>(luaL_checknumber(L, 2)));
	ke::Vector<Data *> *vec = ParamStack.front();
	if (index < 1 || static_cast<size_t>(index) > vec->length())
	{
		lua_pushboolean(L, 0);
		return 1;
	}
	lua_pushboolean(L, vec->at(index - 1)->SetFloat(&value) == 0);
	return 1;
}

static int L_SetHamParamVector(lua_State* L)
{
	if (ParamStack.size() <= 0)
	{
		lua_pushboolean(L, 0);
		return 1;
	}
	int index = static_cast<int>(luaL_checkinteger(L, 1));
	ke::Vector<Data *> *vec = ParamStack.front();
	if (index < 1 || static_cast<size_t>(index) > vec->length())
	{
		lua_pushboolean(L, 0);
		return 1;
	}
	cell value[3];
	for (int i = 0; i < 3; ++i)
	{
		lua_rawgeti(L, 2, i + 1);
		value[i] = amx_ftoc(static_cast<float>(luaL_checknumber(L, -1)));
		lua_pop(L, 1);
	}
	lua_pushboolean(L, vec->at(index - 1)->SetVector(value) == 0);
	return 1;
}

static int L_SetHamParamEntity(lua_State* L)
{
	if (ParamStack.size() <= 0)
	{
		lua_pushboolean(L, 0);
		return 1;
	}
	int index = static_cast<int>(luaL_checkinteger(L, 1));
	cell value = static_cast<cell>(luaL_checkinteger(L, 2));
	ke::Vector<Data *> *vec = ParamStack.front();
	if (index < 1 || static_cast<size_t>(index) > vec->length())
	{
		lua_pushboolean(L, 0);
		return 1;
	}
	lua_pushboolean(L, vec->at(index - 1)->SetEntity(&value) == 0);
	return 1;
}

static int L_SetHamParamString(lua_State* L)
{
	if (ParamStack.size() <= 0)
	{
		lua_pushboolean(L, 0);
		return 1;
	}
	int index = static_cast<int>(luaL_checkinteger(L, 1));
	const char* str = luaL_checkstring(L, 2);
	ke::Vector<Data *> *vec = ParamStack.front();
	if (index < 1 || static_cast<size_t>(index) > vec->length())
	{
		lua_pushboolean(L, 0);
		return 1;
	}
	size_t len = strlen(str);
	cell* temp = new cell[len + 1];
	for (size_t i = 0; i < len; ++i)
	{
		temp[i] = static_cast<unsigned char>(str[i]);
	}
	temp[len] = 0;
	int ok = (vec->at(index - 1)->SetString(temp) == 0);
	delete[] temp;
	lua_pushboolean(L, ok);
	return 1;
}

static int L_GetHamReturnStatus(lua_State* L)
{
	if (ReturnStatus.size() <= 0)
	{
		lua_pushnil(L);
		return 1;
	}
	lua_pushinteger(L, *ReturnStatus.front());
	return 1;
}

cell AMX_NATIVE_CALL amx_hamsandwichl_func_init(AMX* amx, cell* params)
{
	lua_State* L = (lua_State*)params[1];
	g_L = L;
	lua_setintconst(L, "HAM_UNSET", HAM_UNSET);
	lua_setintconst(L, "HAM_IGNORED", HAM_IGNORED);
	lua_setintconst(L, "HAM_HANDLED", HAM_HANDLED);
	lua_setintconst(L, "HAM_OVERRIDE", HAM_OVERRIDE);
	lua_setintconst(L, "HAM_SUPERCEDE", HAM_SUPERCEDE);
	lua_setintconst(L, "Ham_Spawn", Ham_Spawn);
	lua_setintconst(L, "Ham_Precache", Ham_Precache);
	lua_setintconst(L, "Ham_Keyvalue", Ham_Keyvalue);
	lua_setintconst(L, "Ham_ObjectCaps", Ham_ObjectCaps);
	lua_setintconst(L, "Ham_Activate", Ham_Activate);
	lua_setintconst(L, "Ham_SetObjectCollisionBox", Ham_SetObjectCollisionBox);
	lua_setintconst(L, "Ham_Classify", Ham_Classify);
	lua_setintconst(L, "Ham_DeathNotice", Ham_DeathNotice);
	lua_setintconst(L, "Ham_Think", Ham_Think);
	lua_setintconst(L, "Ham_Touch", Ham_Touch);
	lua_setintconst(L, "Ham_Use", Ham_Use);
	lua_setintconst(L, "Ham_Blocked", Ham_Blocked);
	lua_setintconst(L, "Ham_AddPoints", Ham_AddPoints);
	lua_setintconst(L, "Ham_AddPointsToTeam", Ham_AddPointsToTeam);
	lua_setintconst(L, "Ham_AddPlayerItem", Ham_AddPlayerItem);
	lua_setintconst(L, "Ham_RemovePlayerItem", Ham_RemovePlayerItem);
	lua_setintconst(L, "Ham_GiveAmmo", Ham_GiveAmmo);
	lua_setintconst(L, "Ham_GetDelay", Ham_GetDelay);
	lua_setintconst(L, "Ham_IsMoving", Ham_IsMoving);
	lua_setintconst(L, "Ham_OverrideReset", Ham_OverrideReset);
	lua_setintconst(L, "Ham_DamageDecal", Ham_DamageDecal);
	lua_setintconst(L, "Ham_SetToggleState", Ham_SetToggleState);
	lua_setintconst(L, "Ham_StartSneaking", Ham_StartSneaking);
	lua_setintconst(L, "Ham_StopSneaking", Ham_StopSneaking);
	lua_setintconst(L, "Ham_OnControls", Ham_OnControls);
	lua_setintconst(L, "Ham_IsSneaking", Ham_IsSneaking);
	lua_setintconst(L, "Ham_IsAlive", Ham_IsAlive);
	lua_setintconst(L, "Ham_IsBSPModel", Ham_IsBSPModel);
	lua_setintconst(L, "Ham_ReflectGauss", Ham_ReflectGauss);
	lua_setintconst(L, "Ham_HasTarget", Ham_HasTarget);
	lua_setintconst(L, "Ham_IsInWorld", Ham_IsInWorld);
	lua_setintconst(L, "Ham_IsPlayer", Ham_IsPlayer);
	lua_setintconst(L, "Ham_IsNetClient", Ham_IsNetClient);
	lua_setintconst(L, "Ham_TeamId", Ham_TeamId);
	lua_setintconst(L, "Ham_GetNextTarget", Ham_GetNextTarget);
	lua_setintconst(L, "Ham_TakeDamage", Ham_TakeDamage);
	lua_setintconst(L, "Ham_TakeHealth", Ham_TakeHealth);
	lua_setintconst(L, "Ham_Killed", Ham_Killed);
	lua_setintconst(L, "Ham_TraceAttack", Ham_TraceAttack);
	lua_setintconst(L, "Ham_BloodColor", Ham_BloodColor);
	lua_setintconst(L, "Ham_TraceBleed", Ham_TraceBleed);
	lua_setintconst(L, "Ham_IsTriggered", Ham_IsTriggered);
	lua_setintconst(L, "Ham_MyMonsterPointer", Ham_MyMonsterPointer);
	lua_setintconst(L, "Ham_MySquadMonsterPointer", Ham_MySquadMonsterPointer);
	lua_setintconst(L, "Ham_GetToggleState", Ham_GetToggleState);
	lua_setintconst(L, "Ham_Respawn", Ham_Respawn);
	lua_setintconst(L, "Ham_UpdateOwner", Ham_UpdateOwner);
	lua_setintconst(L, "Ham_FBecomeProne", Ham_FBecomeProne);
	lua_setintconst(L, "Ham_Center", Ham_Center);
	lua_setintconst(L, "Ham_EyePosition", Ham_EyePosition);
	lua_setintconst(L, "Ham_EarPosition", Ham_EarPosition);
	lua_setintconst(L, "Ham_BodyTarget", Ham_BodyTarget);
	lua_setintconst(L, "Ham_Illumination", Ham_Illumination);
	lua_setintconst(L, "Ham_FVisible", Ham_FVisible);
	lua_setintconst(L, "Ham_FVecVisible", Ham_FVecVisible);
	lua_setintconst(L, "Ham_Player_PreThink", Ham_Player_PreThink);
	lua_setintconst(L, "Ham_Player_PostThink", Ham_Player_PostThink);
	lua_setintconst(L, "Ham_Player_Jump", Ham_Player_Jump);
	lua_setintconst(L, "Ham_Player_Duck", Ham_Player_Duck);
	lua_setintconst(L, "Ham_Player_GetGunPosition", Ham_Player_GetGunPosition);
	lua_setintconst(L, "Ham_Player_ShouldFadeOnDeath", Ham_Player_ShouldFadeOnDeath);
	lua_setintconst(L, "Ham_Player_ImpulseCommands", Ham_Player_ImpulseCommands);
	lua_setintconst(L, "Ham_Player_UpdateClientData", Ham_Player_UpdateClientData);
	lua_setintconst(L, "Ham_Item_AddToPlayer", Ham_Item_AddToPlayer);
	lua_setintconst(L, "Ham_Item_AddDuplicate", Ham_Item_AddDuplicate);
	lua_setintconst(L, "Ham_Item_CanDeploy", Ham_Item_CanDeploy);
	lua_setintconst(L, "Ham_Item_Deploy", Ham_Item_Deploy);
	lua_setintconst(L, "Ham_Item_Holster", Ham_Item_Holster);
	lua_setintconst(L, "Ham_Item_CanHolster", Ham_Item_CanHolster);
	lua_setintconst(L, "Ham_Item_UpdateItemInfo", Ham_Item_UpdateItemInfo);
	lua_setintconst(L, "Ham_Item_PreFrame", Ham_Item_PreFrame);
	lua_setintconst(L, "Ham_Item_PostFrame", Ham_Item_PostFrame);
	lua_setintconst(L, "Ham_Item_Drop", Ham_Item_Drop);
	lua_setintconst(L, "Ham_Item_Kill", Ham_Item_Kill);
	lua_setintconst(L, "Ham_Item_AttachToPlayer", Ham_Item_AttachToPlayer);
	lua_setintconst(L, "Ham_Item_PrimaryAmmoIndex", Ham_Item_PrimaryAmmoIndex);
	lua_setintconst(L, "Ham_Item_SecondaryAmmoIndex", Ham_Item_SecondaryAmmoIndex);
	lua_setintconst(L, "Ham_Item_GetWeaponPtr", Ham_Item_GetWeaponPtr);
	lua_setintconst(L, "Ham_Item_ItemSlot", Ham_Item_ItemSlot);
	lua_setintconst(L, "Ham_Weapon_ExtractAmmo", Ham_Weapon_ExtractAmmo);
	lua_setintconst(L, "Ham_Weapon_ExtractClipAmmo", Ham_Weapon_ExtractClipAmmo);
	lua_setintconst(L, "Ham_Weapon_AddWeapon", Ham_Weapon_AddWeapon);
	lua_setintconst(L, "Ham_Weapon_PlayEmptySound", Ham_Weapon_PlayEmptySound);
	lua_setintconst(L, "Ham_Weapon_ResetEmptySound", Ham_Weapon_ResetEmptySound);
	lua_setintconst(L, "Ham_Weapon_SendWeaponAnim", Ham_Weapon_SendWeaponAnim);
	lua_setintconst(L, "Ham_Weapon_IsUsable", Ham_Weapon_IsUsable);
	lua_setintconst(L, "Ham_Weapon_PrimaryAttack", Ham_Weapon_PrimaryAttack);
	lua_setintconst(L, "Ham_Weapon_SecondaryAttack", Ham_Weapon_SecondaryAttack);
	lua_setintconst(L, "Ham_Weapon_Reload", Ham_Weapon_Reload);
	lua_setintconst(L, "Ham_Weapon_WeaponIdle", Ham_Weapon_WeaponIdle);
	lua_setintconst(L, "Ham_Weapon_RetireWeapon", Ham_Weapon_RetireWeapon);
	lua_setintconst(L, "Ham_Weapon_ShouldWeaponIdle", Ham_Weapon_ShouldWeaponIdle);
	lua_setintconst(L, "Ham_Weapon_UseDecrement", Ham_Weapon_UseDecrement);
	lua_setintconst(L, "Ham_CS_Restart", Ham_CS_Restart);
	lua_setintconst(L, "Ham_CS_RoundRespawn", Ham_CS_RoundRespawn);
	lua_setintconst(L, "Ham_CS_Item_CanDrop", Ham_CS_Item_CanDrop);
	lua_setintconst(L, "Ham_CS_Item_GetMaxSpeed", Ham_CS_Item_GetMaxSpeed);
	lua_setintconst(L, "Ham_DOD_RoundRespawn", Ham_DOD_RoundRespawn);
	lua_setintconst(L, "Ham_DOD_RoundRespawnEnt", Ham_DOD_RoundRespawnEnt);
	lua_setintconst(L, "Ham_DOD_RoundStore", Ham_DOD_RoundStore);
	lua_setintconst(L, "Ham_DOD_AreaSetIndex", Ham_DOD_AreaSetIndex);
	lua_setintconst(L, "Ham_DOD_AreaSendStatus", Ham_DOD_AreaSendStatus);
	lua_setintconst(L, "Ham_DOD_GetState", Ham_DOD_GetState);
	lua_setintconst(L, "Ham_DOD_GetStateEnt", Ham_DOD_GetStateEnt);
	lua_setintconst(L, "Ham_DOD_Item_CanDrop", Ham_DOD_Item_CanDrop);
	lua_setintconst(L, "Ham_TFC_EngineerUse", Ham_TFC_EngineerUse);
	lua_setintconst(L, "Ham_TFC_Finished", Ham_TFC_Finished);
	lua_setintconst(L, "Ham_TFC_EmpExplode", Ham_TFC_EmpExplode);
	lua_setintconst(L, "Ham_TFC_CalcEmpDmgRad", Ham_TFC_CalcEmpDmgRad);
	lua_setintconst(L, "Ham_TFC_TakeEmpBlast", Ham_TFC_TakeEmpBlast);
	lua_setintconst(L, "Ham_TFC_EmpRemove", Ham_TFC_EmpRemove);
	lua_setintconst(L, "Ham_TFC_TakeConcussionBlast", Ham_TFC_TakeConcussionBlast);
	lua_setintconst(L, "Ham_TFC_Concuss", Ham_TFC_Concuss);
	lua_setintconst(L, "Ham_TS_BreakableRespawn", Ham_TS_BreakableRespawn);
	lua_setintconst(L, "Ham_TS_CanUsedThroughWalls", Ham_TS_CanUsedThroughWalls);
	lua_setintconst(L, "Ham_TS_RespawnWait", Ham_TS_RespawnWait);
	lua_setintconst(L, "Ham_TS_GiveSlowMul", Ham_TS_GiveSlowMul);
	lua_setintconst(L, "Ham_TS_GoSlow", Ham_TS_GoSlow);
	lua_setintconst(L, "Ham_TS_InSlow", Ham_TS_InSlow);
	lua_setintconst(L, "Ham_TS_IsObjective", Ham_TS_IsObjective);
	lua_setintconst(L, "Ham_TS_EnableObjective", Ham_TS_EnableObjective);
	lua_setintconst(L, "Ham_TS_OnFreeEntPrivateData", Ham_TS_OnFreeEntPrivateData);
	lua_setintconst(L, "Ham_TS_ShouldCollide", Ham_TS_ShouldCollide);
	lua_setintconst(L, "Ham_NS_GetPointValue", Ham_NS_GetPointValue);
	lua_setintconst(L, "Ham_NS_AwardKill", Ham_NS_AwardKill);
	lua_setintconst(L, "Ham_NS_ResetEntity", Ham_NS_ResetEntity);
	lua_setintconst(L, "Ham_NS_UpdateOnRemove", Ham_NS_UpdateOnRemove);
	lua_setintconst(L, "Ham_ESF_IsEnvModel", Ham_ESF_IsEnvModel);
	lua_setintconst(L, "Ham_ESF_TakeDamage2", Ham_ESF_TakeDamage2);
	lua_setintconst(L, "Ham_ChangeYaw", Ham_ChangeYaw);
	lua_setintconst(L, "Ham_HasHumanGibs", Ham_HasHumanGibs);
	lua_setintconst(L, "Ham_HasAlienGibs", Ham_HasAlienGibs);
	lua_setintconst(L, "Ham_FadeMonster", Ham_FadeMonster);
	lua_setintconst(L, "Ham_GibMonster", Ham_GibMonster);
	lua_setintconst(L, "Ham_BecomeDead", Ham_BecomeDead);
	lua_setintconst(L, "Ham_IRelationship", Ham_IRelationship);
	lua_setintconst(L, "Ham_PainSound", Ham_PainSound);
	lua_setintconst(L, "Ham_ReportAIState", Ham_ReportAIState);
	lua_setintconst(L, "Ham_MonsterInitDead", Ham_MonsterInitDead);
	lua_setintconst(L, "Ham_Look", Ham_Look);
	lua_setintconst(L, "Ham_BestVisibleEnemy", Ham_BestVisibleEnemy);
	lua_setintconst(L, "Ham_FInViewCone", Ham_FInViewCone);
	lua_setintconst(L, "Ham_FVecInViewCone", Ham_FVecInViewCone);
	lua_setintconst(L, "Ham_GetDeathActivity", Ham_GetDeathActivity);
	lua_setintconst(L, "Ham_RunAI", Ham_RunAI);
	lua_setintconst(L, "Ham_MonsterThink", Ham_MonsterThink);
	lua_setintconst(L, "Ham_MonsterInit", Ham_MonsterInit);
	lua_setintconst(L, "Ham_CheckLocalMove", Ham_CheckLocalMove);
	lua_setintconst(L, "Ham_Move", Ham_Move);
	lua_setintconst(L, "Ham_MoveExecute", Ham_MoveExecute);
	lua_setintconst(L, "Ham_ShouldAdvanceRoute", Ham_ShouldAdvanceRoute);
	lua_setintconst(L, "Ham_GetStoppedActivity", Ham_GetStoppedActivity);
	lua_setintconst(L, "Ham_Stop", Ham_Stop);
	lua_setintconst(L, "Ham_CheckRangeAttack1", Ham_CheckRangeAttack1);
	lua_setintconst(L, "Ham_CheckRangeAttack2", Ham_CheckRangeAttack2);
	lua_setintconst(L, "Ham_CheckMeleeAttack1", Ham_CheckMeleeAttack1);
	lua_setintconst(L, "Ham_CheckMeleeAttack2", Ham_CheckMeleeAttack2);
	lua_setintconst(L, "Ham_ScheduleChange", Ham_ScheduleChange);
	lua_setintconst(L, "Ham_CanPlaySequence", Ham_CanPlaySequence);
	lua_setintconst(L, "Ham_CanPlaySentence2", Ham_CanPlaySentence2);
	lua_setintconst(L, "Ham_PlaySentence", Ham_PlaySentence);
	lua_setintconst(L, "Ham_PlayScriptedSentence", Ham_PlayScriptedSentence);
	lua_setintconst(L, "Ham_SentenceStop", Ham_SentenceStop);
	lua_setintconst(L, "Ham_GetIdealState", Ham_GetIdealState);
	lua_setintconst(L, "Ham_SetActivity", Ham_SetActivity);
	lua_setintconst(L, "Ham_CheckEnemy", Ham_CheckEnemy);
	lua_setintconst(L, "Ham_FTriangulate", Ham_FTriangulate);
	lua_setintconst(L, "Ham_SetYawSpeed", Ham_SetYawSpeed);
	lua_setintconst(L, "Ham_BuildNearestRoute", Ham_BuildNearestRoute);
	lua_setintconst(L, "Ham_FindCover", Ham_FindCover);
	lua_setintconst(L, "Ham_CoverRadius", Ham_CoverRadius);
	lua_setintconst(L, "Ham_FCanCheckAttacks", Ham_FCanCheckAttacks);
	lua_setintconst(L, "Ham_CheckAmmo", Ham_CheckAmmo);
	lua_setintconst(L, "Ham_IgnoreConditions", Ham_IgnoreConditions);
	lua_setintconst(L, "Ham_FValidateHintType", Ham_FValidateHintType);
	lua_setintconst(L, "Ham_FCanActiveIdle", Ham_FCanActiveIdle);
	lua_setintconst(L, "Ham_ISoundMask", Ham_ISoundMask);
	lua_setintconst(L, "Ham_HearingSensitivity", Ham_HearingSensitivity);
	lua_setintconst(L, "Ham_BarnacleVictimBitten", Ham_BarnacleVictimBitten);
	lua_setintconst(L, "Ham_BarnacleVictimReleased", Ham_BarnacleVictimReleased);
	lua_setintconst(L, "Ham_PrescheduleThink", Ham_PrescheduleThink);
	lua_setintconst(L, "Ham_DeathSound", Ham_DeathSound);
	lua_setintconst(L, "Ham_AlertSound", Ham_AlertSound);
	lua_setintconst(L, "Ham_IdleSound", Ham_IdleSound);
	lua_setintconst(L, "Ham_StopFollowing", Ham_StopFollowing);
	lua_setintconst(L, "Ham_CS_Weapon_SendWeaponAnim", Ham_CS_Weapon_SendWeaponAnim);
	lua_setintconst(L, "Ham_CS_Player_ResetMaxSpeed", Ham_CS_Player_ResetMaxSpeed);
	lua_setintconst(L, "Ham_CS_Player_IsBot", Ham_CS_Player_IsBot);
	lua_setintconst(L, "Ham_CS_Player_GetAutoaimVector", Ham_CS_Player_GetAutoaimVector);
	lua_setintconst(L, "Ham_CS_Player_Blind", Ham_CS_Player_Blind);
	lua_setintconst(L, "Ham_CS_Player_OnTouchingWeapon", Ham_CS_Player_OnTouchingWeapon);
	lua_setintconst(L, "Ham_DOD_SetScriptReset", Ham_DOD_SetScriptReset);
	lua_setintconst(L, "Ham_DOD_Item_SpawnDeploy", Ham_DOD_Item_SpawnDeploy);
	lua_setintconst(L, "Ham_DOD_Item_SetDmgTime", Ham_DOD_Item_SetDmgTime);
	lua_setintconst(L, "Ham_DOD_Item_DropGren", Ham_DOD_Item_DropGren);
	lua_setintconst(L, "Ham_DOD_Weapon_IsUseable", Ham_DOD_Weapon_IsUseable);
	lua_setintconst(L, "Ham_DOD_Weapon_Aim", Ham_DOD_Weapon_Aim);
	lua_setintconst(L, "Ham_DOD_Weapon_flAim", Ham_DOD_Weapon_flAim);
	lua_setintconst(L, "Ham_DOD_Weapon_RemoveStamina", Ham_DOD_Weapon_RemoveStamina);
	lua_setintconst(L, "Ham_DOD_Weapon_ChangeFOV", Ham_DOD_Weapon_ChangeFOV);
	lua_setintconst(L, "Ham_DOD_Weapon_ZoomOut", Ham_DOD_Weapon_ZoomOut);
	lua_setintconst(L, "Ham_DOD_Weapon_ZoomIn", Ham_DOD_Weapon_ZoomIn);
	lua_setintconst(L, "Ham_DOD_Weapon_GetFOV", Ham_DOD_Weapon_GetFOV);
	lua_setintconst(L, "Ham_DOD_Weapon_IsWaterSniping", Ham_DOD_Weapon_IsWaterSniping);
	lua_setintconst(L, "Ham_DOD_Weapon_UpdateZoomSpeed", Ham_DOD_Weapon_UpdateZoomSpeed);
	lua_setintconst(L, "Ham_DOD_Weapon_Special", Ham_DOD_Weapon_Special);
	lua_setintconst(L, "Ham_ShouldCollide", Ham_TS_ShouldCollide);
	lua_register(L, "GetHamReturnInteger", L_GetHamReturnInteger);
	lua_register(L, "GetHamReturnFloat", L_GetHamReturnFloat);
	lua_register(L, "GetHamReturnVector", L_GetHamReturnVector);
	lua_register(L, "GetHamReturnEntity", L_GetHamReturnEntity);
	lua_register(L, "GetHamReturnString", L_GetHamReturnString);
	lua_register(L, "GetOrigHamReturnInteger", L_GetOrigHamReturnInteger);
	lua_register(L, "GetOrigHamReturnFloat", L_GetOrigHamReturnFloat);
	lua_register(L, "GetOrigHamReturnVector", L_GetOrigHamReturnVector);
	lua_register(L, "GetOrigHamReturnEntity", L_GetOrigHamReturnEntity);
	lua_register(L, "GetOrigHamReturnString", L_GetOrigHamReturnString);
	lua_register(L, "SetHamReturnInteger", L_SetHamReturnInteger);
	lua_register(L, "SetHamReturnFloat", L_SetHamReturnFloat);
	lua_register(L, "SetHamReturnVector", L_SetHamReturnVector);
	lua_register(L, "SetHamReturnEntity", L_SetHamReturnEntity);
	lua_register(L, "SetHamReturnString", L_SetHamReturnString);
	lua_register(L, "SetHamParamInteger", L_SetHamParamInteger);
	lua_register(L, "SetHamParamFloat", L_SetHamParamFloat);
	lua_register(L, "SetHamParamVector", L_SetHamParamVector);
	lua_register(L, "SetHamParamEntity", L_SetHamParamEntity);
	lua_register(L, "SetHamParamString", L_SetHamParamString);
	lua_register(L, "GetHamReturnStatus", L_GetHamReturnStatus);
	return 1;
}

AMX_NATIVE_INFO ReturnNatives[] =
{
	{ "Lhamsandwichl_func_init",    amx_hamsandwichl_func_init },
	{ "GetHamReturnIntegerL",		GetHamReturnInteger },
	{ "GetHamReturnFloatL",			GetHamReturnFloat },
	{ "GetHamReturnVectorL",			GetHamReturnVector },
	{ "GetHamReturnEntityL",			GetHamReturnEntity },
	{ "GetHamReturnStringL",			GetHamReturnString },
	{ "GetOrigHamReturnIntegerL",	GetOrigHamReturnInteger },
	{ "GetOrigHamReturnFloatL",		GetOrigHamReturnFloat },
	{ "GetOrigHamReturnVectorL",		GetOrigHamReturnVector },
	{ "GetOrigHamReturnEntityL",		GetOrigHamReturnEntity },
	{ "GetOrigHamReturnStringL",		GetOrigHamReturnString },
	{ "SetHamReturnIntegerL",		SetHamReturnInteger },
	{ "SetHamReturnFloatL",			SetHamReturnFloat },
	{ "SetHamReturnVectorL",			SetHamReturnVector },
	{ "SetHamReturnEntityL",			SetHamReturnEntity },
	{ "SetHamReturnStringL",			SetHamReturnString },

	{ "GetHamReturnStatusL",			GetHamReturnStatus },

	{ "SetHamParamIntegerL",			SetHamParamInteger },
	{ "SetHamParamFloatL",			SetHamParamFloat },
	{ "SetHamParamVectorL",			SetHamParamVector },
	{ "SetHamParamEntityL",			SetHamParamEntity },
	{ "SetHamParamEntity2L",			SetHamParamEntity2 },
	{ "SetHamParamStringL",			SetHamParamString },
	{ "SetHamParamTraceResultL",		SetHamParamTraceResult },
	{ "SetHamParamItemInfoL",		SetHamParamItemInfo },

	{ "GetHamItemInfoL",				GetHamItemInfo },
	{ "SetHamItemInfoL",				SetHamItemInfo },
	{ "CreateHamItemInfoL",			CreateHamItemInfo },
	{ "FreeHamItemInfoL",			FreeHamItemInfo },

	{ NULL,							NULL },
};
