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
#include <extdll.h>

#include <amtl/am-vector.h>
#include "forward.h"
#include "hook.h"
#include "ham_const.h"
#include "hooklist.h"
#include "offsets.h"
#include <assert.h>
#include "DataHandler.h"
#include "hook_specialbot.h"
#include <HLTypeConversion.h>
#include "lualib/lua.hpp"
#ifdef _WIN32
#include <windows.h>
#include <psapi.h>
#endif
#if !defined(offsetof) && !defined(GNUC)
#define offsetof(s, m) (size_t)&(((s *)0)->m)
#endif
HLTypeConversion TypeConversion;

extern ke::Vector<Hook *> hooks[HAM_LAST_ENTRY_DONT_USE_ME_LOL];
extern CHamSpecialBotHandler SpecialbotHandler;

extern AMX_NATIVE_INFO RegisterNatives[];
extern AMX_NATIVE_INFO ReturnNatives[];
extern AMX_NATIVE_INFO pdata_natives[];
extern AMX_NATIVE_INFO pdata_natives_safe[];

extern hook_t hooklist[];

static hook_t *g_OriginalHamHooklist = nullptr;
static void *g_OriginalHamSpawnTarget = nullptr;
extern lua_State *g_L;

typedef void (*HamVoidVoidTarget)(void *, void *);

struct RawHookForDump
{
	void* preData;
	size_t preLen;
	size_t preCap;
	void* postData;
	size_t postLen;
	size_t postCap;
	void* func;
	void** vtable;
	int entry;
	void* target;
	int exec;
	int del;
	void* tramp;
	char* ent;
	int trampSize;
};
static bool IsReadableAddress(const void *ptr)
{
	MEMORY_BASIC_INFORMATION mbi;
	if (!VirtualQuery(ptr, &mbi, sizeof(mbi)))
	{
		return false;
	}
	if (mbi.State != MEM_COMMIT)
	{
		return false;
	}
	DWORD protect = mbi.Protect & 0xff;
	if (protect == PAGE_NOACCESS || protect == PAGE_GUARD)
	{
		return false;
	}
	return true;
}

static void DumpOriginalHook(void* hookPtr)
{
	if (!hookPtr)
	{
		MF_Log("[hamsandwichl] original hook dump: hook is null");
		return;
	}
	if (!IsReadableAddress(hookPtr))
	{
		MF_Log("[hamsandwichl] original hook dump: hook %p unreadable", hookPtr);
		return;
	}

	RawHookForDump* hook = reinterpret_cast<RawHookForDump*>(hookPtr);
	const char* ent = "<unreadable>";
	if (hook->ent && IsReadableAddress(hook->ent))
	{
		ent = hook->ent;
	}

	MF_Log("[hamsandwichl] original hook dump: hook=%p func=%p target=%p tramp=%p entry=%d exec=%d del=%d ent=%s",
		hookPtr,
		hook->func,
		hook->target,
		hook->tramp,
		hook->entry,
		hook->exec,
		hook->del,
		ent);
}

static int CallLuaHamSpawn(const char *name, void *pthis)
{
	if (!g_L)
	{
		return HAM_UNSET;
	}

	lua_getglobal(g_L, name);
	if (!lua_isfunction(g_L, -1))
	{
		lua_pop(g_L, 1);
		return HAM_UNSET;
	}

	int ent = TypeConversion.cbase_to_id(pthis);
	lua_pushinteger(g_L, ent);

	if (lua_pcall(g_L, 1, 1, 0) != 0)
	{
		const char *err = lua_tostring(g_L, -1);
		if (err)
		{
			MF_Log("[hamsandwichl] Lua ham callback error in %s: %s", name, err);
		}
		lua_pop(g_L, 1);
		return HAM_UNSET;
	}

	int result = HAM_UNSET;
	if (lua_isnumber(g_L, -1) || lua_isboolean(g_L, -1))
	{
		result = static_cast<int>(lua_tointeger(g_L, -1));
	}
	lua_pop(g_L, 1);
	return result;
}

static void Lua_Hook_Spawn_Void_Void(void *hook, void *pthis)
{
	DumpOriginalHook(hook);
	int preresult = CallLuaHamSpawn("ham_spawn", pthis);

	if (preresult < HAM_SUPERCEDE && g_OriginalHamSpawnTarget)
	{
		reinterpret_cast<HamVoidVoidTarget>(g_OriginalHamSpawnTarget)(hook, pthis);
	}

	CallLuaHamSpawn("ham_spawn_post", pthis);
}


static bool SafeStrEq(const char *s, const char *expected)
{
	__try
	{
		if (!s || !expected)
		{
			return false;
		}
		return strcmp(s, expected) == 0;
	}
	__except (EXCEPTION_EXECUTE_HANDLER)
	{
		return false;
	}
}

int ReadConfig(void);

static hook_t *FindOriginalHamHooklistSimple()
{
	HMODULE mod = GetModuleHandleA("hamsandwich_amxx.dll");
	if (!mod)
	{
		return nullptr;
	}

	MODULEINFO mi;
	if (!GetModuleInformation(GetCurrentProcess(), mod, &mi, sizeof(mi)))
	{
		return nullptr;
	}

	uintptr_t base = reinterpret_cast<uintptr_t>(mi.lpBaseOfDll);
	size_t size = mi.SizeOfImage;

	const char targetString[] = "spawn";
	for (size_t i = 0; i + sizeof(targetString) <= size; ++i)
	{
		uintptr_t stringAddr = base + i;
		const void *addr = reinterpret_cast<void *>(stringAddr);
		if (!IsReadableAddress(addr))
		{
			continue;
		}

		bool match = false;
		__try
		{
			match = memcmp(addr, targetString, sizeof(targetString)) == 0;
		}
		__except (EXCEPTION_EXECUTE_HANDLER)
		{
			match = false;
		}
		if (!match)
		{
			continue;
		}

		uintptr_t ptrValue = stringAddr;
		for (size_t j = 0; j + sizeof(uintptr_t) <= size; j += sizeof(void *))
		{
			uintptr_t ptrAddr = base + j;
			const void *ptrLoc = reinterpret_cast<void *>(ptrAddr);
			if (!IsReadableAddress(ptrLoc))
			{
				continue;
			}

			bool ptrMatch = false;
			__try
			{
				ptrMatch = memcmp(ptrLoc, &ptrValue, sizeof(ptrValue)) == 0;
			}
			__except (EXCEPTION_EXECUTE_HANDLER)
			{
				ptrMatch = false;
			}
			if (!ptrMatch)
			{
				continue;
			}

			hook_t *hk = reinterpret_cast<hook_t *>(ptrAddr - offsetof(hook_t, name));
			if (!IsReadableAddress(hk) || !IsReadableAddress(hk + 1) || !IsReadableAddress(hk + 2))
			{
				continue;
			}
			if (!hk[0].name || !hk[1].name || !hk[2].name)
			{
				continue;
			}
			if (!SafeStrEq(hk[0].name, "spawn") || !SafeStrEq(hk[1].name, "precache") || !SafeStrEq(hk[2].name, "keyvalue"))
			{
				continue;
			}

			MF_Log("[hamsandwichl] simple scan ok: hooklist=%p name0=%s name1=%s name2=%s",
				   hk,
				   hk[0].name ? hk[0].name : "<null>",
				   hk[1].name ? hk[1].name : "<null>",
				   hk[2].name ? hk[2].name : "<null>");
			return hk;
		}
	}

	MF_Log("[hamsandwichl] simple scan: hooklist not found");
	return nullptr;
}
static cell AMX_NATIVE_CALL FindOriginalHamHooklistSimpleN(AMX *amx, cell *params)
{
#ifdef _WIN32
	hook_t *hk = FindOriginalHamHooklistSimple();
	if (hk)
	{
		g_OriginalHamHooklist = hk;
		return 1;
	}
	return 0;
#else
	return 0;
#endif
}

static cell AMX_NATIVE_CALL PatchOriginalHamSpawnL(AMX *amx, cell *params)
{
#ifdef _WIN32
	if (!g_OriginalHamHooklist)
	{
		g_OriginalHamHooklist = FindOriginalHamHooklistSimple();
	}
	if (!g_OriginalHamHooklist)
	{
		MF_Log("[hamsandwichl] cannot patch Ham_Spawn: original hooklist not found");
		return 0;
	}
	if (!g_OriginalHamSpawnTarget)
	{
		g_OriginalHamSpawnTarget = g_OriginalHamHooklist[Ham_Spawn].targetfunc;
	}
	g_OriginalHamHooklist[Ham_Spawn].targetfunc = reinterpret_cast<void *>(&Lua_Hook_Spawn_Void_Void);
	MF_Log("[hamsandwichl] patched original Ham_Spawn targetfunc");
	return 1;
#else
	return 0;
#endif
}

static cell AMX_NATIVE_CALL RestoreOriginalHamSpawnL(AMX *amx, cell *params)
{
#ifdef _WIN32
	if (g_OriginalHamHooklist && g_OriginalHamSpawnTarget)
	{
		g_OriginalHamHooklist[Ham_Spawn].targetfunc = g_OriginalHamSpawnTarget;
		MF_Log("[hamsandwichl] restored original Ham_Spawn targetfunc");
		return 1;
	}
#endif
	return 0;
}

AMX_NATIVE_INFO DebugNatives[] =
	{
		{"FindOriginalHamHooklistSimpleL", FindOriginalHamHooklistSimpleN},
		{"PatchOriginalHamSpawnL", PatchOriginalHamSpawnL},
		{"RestoreOriginalHamSpawnL", RestoreOriginalHamSpawnL},
		{NULL, NULL}};

void OnAmxxAttach(void)
{
	// Assert that the enum is aligned properly with the table

	assert(strcmp(hooklist[Ham_FVecVisible].name, "fvecvisible") == 0);
	assert(strcmp(hooklist[Ham_Player_UpdateClientData].name, "player_updateclientdata") == 0);
	assert(strcmp(hooklist[Ham_Item_AddToPlayer].name, "item_addtoplayer") == 0);
	assert(strcmp(hooklist[Ham_Weapon_ExtractAmmo].name, "weapon_extractammo") == 0);
	assert(strcmp(hooklist[Ham_TS_BreakableRespawn].name, "ts_breakablerespawn") == 0);
	assert(strcmp(hooklist[Ham_NS_UpdateOnRemove].name, "ns_updateonremove") == 0);
	assert(strcmp(hooklist[Ham_TS_ShouldCollide].name, "ts_shouldcollide") == 0);

	assert(strcmp(hooklist[Ham_GetDeathActivity].name, "getdeathactivity") == 0);
	assert(strcmp(hooklist[Ham_StopFollowing].name, "stopfollowing") == 0);
	assert(strcmp(hooklist[Ham_CS_Player_OnTouchingWeapon].name, "cstrike_player_ontouchingweapon") == 0);
	assert(strcmp(hooklist[Ham_DOD_Weapon_Special].name, "dod_weapon_special") == 0);
	assert(strcmp(hooklist[Ham_TFC_RadiusDamage2].name, "tfc_radiusdamage2") == 0);
	assert(strcmp(hooklist[Ham_ESF_Weapon_HolsterWhenMeleed].name, "esf_weapon_holsterwhenmeleed") == 0);
	assert(strcmp(hooklist[Ham_NS_Weapon_GetDeployTime].name, "ns_weapon_getdeploytime") == 0);
	assert(strcmp(hooklist[Ham_SC_MedicCallSound].name, "sc_mediccallsound") == 0);
	assert(strcmp(hooklist[Ham_SC_Player_CanTouchPlayer].name, "sc_player_cantouchplayer") == 0);
	assert(strcmp(hooklist[Ham_SC_Weapon_ChangeWeaponSkin].name, "sc_weapon_changeweaponskin") == 0);
	assert(strcmp(hooklist[Ham_Item_GetItemInfo].name, "item_getiteminfo") == 0);

	assert(strcmp(hooklist[Ham_SC_Item_AddToPlayer].name, "sc_item_addtoplayer") == 0);
	assert(strcmp(hooklist[Ham_SC_Weapon_ExtractAmmoFromItem].name, "sc_weapon_extractammofromitem") == 0);
	assert(strcmp(hooklist[Ham_SC_Player_EnteredObserver].name, "sc_player_enteredobserver") == 0);

	MF_AddNatives(pdata_natives_safe);
	MF_AddNatives(DebugNatives);

	if (ReadConfig() > 0)
	{
		if (Offsets.IsValid())
		{
			MF_AddNatives(RegisterNatives);
			MF_AddNatives(ReturnNatives);
			MF_AddNatives(pdata_natives);
		}
		else
		{
#ifdef _WIN32
			MF_Log("Error: pev and base not set for section \"%s windows\", cannot register natives.", MF_GetModname());
#elif defined(__linux__)
			MF_Log("Error: pev and base not set for section \"%s linux\", cannot register natives.", MF_GetModname());
#elif defined(__APPLE__)
			MF_Log("Error: pev and base not set for section \"%s mac\", cannot register natives.", MF_GetModname());
#endif
		}
	}
	else
	{
		MF_Log("Error: Cannot read config file, natives not registered!");
	}
}

extern CStack<ItemInfo *> g_FreeIIs;

void OnAmxxDetach()
{
	while (!g_FreeIIs.empty())
	{
		delete g_FreeIIs.front();
		g_FreeIIs.pop();
	}
}

void HamCommand(void);

void OnPluginsUnloaded(void)
{
	for (size_t i = 0; i < HAM_LAST_ENTRY_DONT_USE_ME_LOL; i++)
	{
		for (size_t j = 0; j < hooks[i].length(); ++j)
		{
			delete hooks[i].at(j);
		}
		hooks[i].clear();
	}
}

void OnPluginsLoaded(void)
{
	TypeConversion.init();
}

void OnMetaAttach(void)
{
	REG_SVR_COMMAND("ham", HamCommand);
}

void SetClientKeyValue(int clientIndex, char *infobuffer, const char *key, const char *value)
{
	SpecialbotHandler.CheckClientKeyValue(clientIndex, infobuffer, key, value);
	RETURN_META(MRES_IGNORED);
}
