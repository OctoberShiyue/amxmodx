
#include "luaMngr.h"

// 命名空间 ke 是 AMTL 的默认命名空间

lua_State *g_L = nullptr;
// float g_fCurrentTime;
// float g_fNextActionTime;
// typedef ke::HashMap<ke::AString, int,int> g_FuncIdMap;
StringHashMap<int> g_LuaPawnFuncMap;


// 1. 定义数据结构
enum VarType {
    TYPE_FLOAT,
    TYPE_INT,
    TYPE_STRING,
    TYPE_VECTOR,
    TYPE_EDICT
};

struct EntityVarEntry {
    size_t      offset;
    VarType     type;
};

// 2. 定义 Hash 策略 (HashPolicy)
// 这是 am-hashmap.h 要求的第3个模板参数
struct StringPolicy {
    // 计算 Hash (使用简单的 DJB2 算法，或者 AMTL 自带的 HashString)
    static uint32_t hash(const char* str) {
        uint32_t hash = 5381;
        int c;
        while ((c = *str++))
            hash = ((hash << 5) + hash) + c; /* hash * 33 + c */
        return hash;
    }

    // 重载1: 针对 ke::AString 的 Hash
    static uint32_t hash(const ke::AString& str) {
        return hash(str.chars());
    }

    // 比较函数 matches
    // 必须支持 key 类型 (ke::AString) 和 lookup 类型 (const char*) 的混合比较
    static bool matches(const char* lookup, const ke::AString& key) {
        return strcmp(lookup, key.chars()) == 0;
    }
    
    static bool matches(const ke::AString& lookup, const ke::AString& key) {
        return strcmp(lookup.chars(), key.chars()) == 0;
    }
};

// 3. 定义全局 HashMap
// 参数: <Key类型, Value类型, Policy类型>
static ke::HashMap<ke::AString, EntityVarEntry, StringPolicy> g_EntVarMap;

// 辅助宏：修正了添加逻辑 (findForAdd + add)
#define REG_VAR(key_str, member_name, var_type) \
    { \
        /* 1. 先查找插入位置 */ \
        auto i = g_EntVarMap.findForAdd(key_str); \
        /* 2. 如果没存在，则添加 */ \
        if (!i.found()) { \
            EntityVarEntry e; \
            e.offset = offsetof(entvars_t, member_name); \
            e.type = var_type; \
            g_EntVarMap.add(i, key_str, e); \
        } \
    }
// 5. Lua 接口实现
static int L_GetEntityVar(lua_State* L)
{
    // 参数1: edict指针
    edict_t* pEnt = (edict_t*)lua_touserdata(L, 1);
    // 参数2: 属性名 (const char*)
    const char* key = lua_tostring(L, 2);

    if (!pEnt || pEnt->free || !key) {
        lua_pushnil(L);
        return 1;
    }

    // ★ 极速查找 ★
    // 这里传入 const char*，StringPolicy 会自动处理，不需要构造 AString
    auto result = g_EntVarMap.find(key);

    if (!result.found()) {
        lua_pushnil(L); 
        return 1;
    }

    // 获取 Value (result->value 是 EntityVarEntry)
    const EntityVarEntry& entry = result->value;
    
    // 计算内存地址
    void* pAddr = (char*)&(pEnt->v) + entry.offset;

    switch (entry.type) 
    {
        case TYPE_FLOAT:
            lua_pushnumber(L, *(float*)pAddr);
            return 1;

        case TYPE_INT:
            lua_pushinteger(L, *(int*)pAddr);
            return 1;

        case TYPE_STRING:
            lua_pushstring(L, STRING(*(string_t*)pAddr));
            return 1;

        case TYPE_VECTOR:
        {
            float* vec = (float*)pAddr;
            lua_pushnumber(L, vec[0]);
            lua_pushnumber(L, vec[1]);
            lua_pushnumber(L, vec[2]);
            return 3;
        }

        case TYPE_EDICT:
        {
            edict_t* e = *(edict_t**)pAddr;
            if (e) lua_pushlightuserdata(L, e);
            else lua_pushnil(L);
            return 1;
        }
    }

    return 0;
}
static int L_get_gametime(lua_State* L)
{
    lua_pushnumber(L, gpGlobals->time);
    return 1;
}

static int Lua_CallPawnFunction_Proxy(lua_State *L)
{
    if (!L)
    {
        MF_Log("Lua_CallPawnFunction_Proxy: Invalid Lua state.");
        return 0;
    }
    int forwardId = (int)lua_tointeger(L, lua_upvalueindex(1));
    cell pawn_ret = MF_ExecuteForward(forwardId, (cell)L);
    return pawn_ret;
}

cell AMX_NATIVE_CALL Native_LuaRegisterFunction(AMX *amx, cell *params)
{
    lua_State *L = (lua_State *)params[1];
    if (!L)
    {
        MF_LogError(amx, AMX_ERR_NATIVE, "Native_LuaRegisterFunction: Invalid Lua state.");
        return 0;
    }

    // 获取字符串 (建议加上非空判断)
    char *luaName = MF_GetAmxString(amx, params[2], 0, NULL);
    char *pawnFuncName = MF_GetAmxString(amx, params[3], 1, NULL);

    if (!luaName || !luaName[0] || !pawnFuncName || !pawnFuncName[0])
    {
        MF_LogError(amx, AMX_ERR_NATIVE, "Native_LuaRegisterFunction: Invalid function name(s).");
        return 0;
    }

    // 4. 哈希表查找优化 (只查找一次)
    // 假设使用的是 ke::HashMap 或类似的 AMTL 结构
    auto search = g_LuaPawnFuncMap.find(pawnFuncName);

    // 检查是否找到
    if (!search.found())
    {
        MF_LogError(amx, AMX_ERR_NATIVE, "Native_LuaRegisterFunction: Pawn function '%s' is not registered.", pawnFuncName);
        return 0;
    }

    // 5. 确保 Lua 栈有足够空间压入 upvalue 和 closure
    if (!lua_checkstack(L, 3))
    {
        MF_LogError(amx, AMX_ERR_NATIVE, "Native_LuaRegisterFunction: Lua stack overflow.");
        return 0;
    }

    // 使用查找结果 (value)
    lua_pushinteger(L, search->value);

    // 创建 C 闭包 (1个 upvalue)
    lua_pushcclosure(L, Lua_CallPawnFunction_Proxy, 1);

    // 设置为全局变量
    lua_setglobal(L, luaName);

    return 1;
}

/*
** Lua for AMX Mod X 完整适配层
** 支持 Lua 5.4.x
*/

// ---------------------------------------------------------
// Native 实现: 生命周期
// ---------------------------------------------------------
void InitLuaAPI(lua_State* L) {
    lua_register(L, "GetEntityVar", L_GetEntityVar);
}
static cell AMX_NATIVE_CALL n_lua_open(AMX *amx, cell *params)
{   
    g_EntVarMap.clear();
    // 初始化容量 (2的幂次方，比如 64, 128)
    g_EntVarMap.init(128);

    // --- Float ---
    REG_VAR("health",     health,     TYPE_FLOAT);
    REG_VAR("gravity",    gravity,    TYPE_FLOAT);
    REG_VAR("friction",   friction,   TYPE_FLOAT);
    REG_VAR("max_health", max_health, TYPE_FLOAT);
    REG_VAR("dmg",        dmg,        TYPE_FLOAT);
    REG_VAR("takedamage", takedamage, TYPE_FLOAT);

    // --- String ---
    REG_VAR("classname",  classname,  TYPE_STRING);
    REG_VAR("model",      model,      TYPE_STRING);
    REG_VAR("netname",    netname,    TYPE_STRING);
    REG_VAR("targetname", targetname, TYPE_STRING);

    // --- Int ---
    REG_VAR("flags",      flags,      TYPE_INT);
    REG_VAR("movetype",   movetype,   TYPE_INT);
    REG_VAR("solid",      solid,      TYPE_INT);
    REG_VAR("team",       team,       TYPE_INT);
    REG_VAR("button",     button,     TYPE_INT);
    REG_VAR("deadflag",   deadflag,   TYPE_INT);

    // --- Vector ---
    REG_VAR("origin",     origin,     TYPE_VECTOR);
    REG_VAR("angles",     angles,     TYPE_VECTOR);
    REG_VAR("velocity",   velocity,   TYPE_VECTOR);
    REG_VAR("v_angle",    v_angle,    TYPE_VECTOR);

    // --- Edict ---
    REG_VAR("owner",      owner,      TYPE_EDICT);
    REG_VAR("aiment",     aiment,     TYPE_EDICT);

    
    lua_State *L = luaL_newstate();
    luaL_openlibs(L);
    InitLuaAPI(L);
    g_L = L;
    return reinterpret_cast<cell>(L);
}

static cell AMX_NATIVE_CALL n_lua_close(AMX *amx, cell *params)
{
    lua_State *L = (lua_State *)params[1];
    if (L)
        lua_close(L);
    g_L = nullptr;
    return 0;
}

static cell AMX_NATIVE_CALL n_lua_dostring(AMX *amx, cell *params)
{
    lua_State *L = (lua_State *)params[1];
    if (!L)
    {
        MF_Log("n_lua_dostring: Invalid Lua state.");
        return 0;
    }
    char *script = MF_GetAmxString(amx, params[2], 0, NULL);
    int ret = luaL_dostring(L, script);
    if (ret != LUA_OK)
    {
        MF_Log("Lua Error: %s", lua_tostring(L, -1));
        lua_pop(L, 1);
    }
    return ret;
}

// ---------------------------------------------------------
// Native 实现: 栈管理
// ---------------------------------------------------------

static cell AMX_NATIVE_CALL n_lua_gettop(AMX *amx, cell *params)
{
    if (!(lua_State *)params[1])
    {
        MF_Log("n_lua_gettop: Invalid Lua state.");
        return 0;
    }
    return lua_gettop((lua_State *)params[1]);
}

static cell AMX_NATIVE_CALL n_lua_settop(AMX *amx, cell *params)
{
    if (!(lua_State *)params[1])
    {
        MF_Log("n_lua_settop: Invalid Lua state.");
        return 0;
    }
    lua_settop((lua_State *)params[1], params[2]);
    return 0;
}

static cell AMX_NATIVE_CALL n_lua_pop(AMX *amx, cell *params)
{
    if (!(lua_State *)params[1])
    {
        MF_Log("n_lua_pop: Invalid Lua state.");
        return 0;
    }
    lua_pop((lua_State *)params[1], params[2]);
    return 0;
}

static cell AMX_NATIVE_CALL n_lua_type(AMX *amx, cell *params)
{
    if (!(lua_State *)params[1])
    {
        MF_Log("n_lua_type: Invalid Lua state.");
        return 0;
    }
    return lua_type((lua_State *)params[1], params[2]);
}

static cell AMX_NATIVE_CALL n_lua_pushvalue(AMX *amx, cell *params)
{
    if (!(lua_State *)params[1])
    {
        MF_Log("n_lua_pushvalue: Invalid Lua state.");
        return 0;
    }
    lua_pushvalue((lua_State *)params[1], params[2]);
    return 0;
}

static cell AMX_NATIVE_CALL n_lua_remove(AMX *amx, cell *params)
{
    if (!(lua_State *)params[1])
    {
        MF_Log("n_lua_remove: Invalid Lua state.");
        return 0;
    }
    lua_remove((lua_State *)params[1], params[2]);
    return 0;
}

// ---------------------------------------------------------
// Native 实现: 压栈 (Push)
// ---------------------------------------------------------

static cell AMX_NATIVE_CALL n_lua_pushinteger(AMX *amx, cell *params)
{
    if (!(lua_State *)params[1])
    {
        MF_Log("n_lua_pushinteger: Invalid Lua state.");
        return 0;
    }
    lua_pushinteger((lua_State *)params[1], (lua_Integer)params[2]);
    return 0;
}

static cell AMX_NATIVE_CALL n_lua_pushnumber(AMX *amx, cell *params)
{
    if (!(lua_State *)params[1])
    {
        MF_Log("n_lua_pushnumber: Invalid Lua state.");
        return 0;
    }
    lua_pushnumber((lua_State *)params[1], (lua_Number)amx_ctof(params[2]));
    return 0;
}

static cell AMX_NATIVE_CALL n_lua_pushstring(AMX *amx, cell *params)
{
    if (!(lua_State *)params[1])
    {
        MF_Log("n_lua_pushstring: Invalid Lua state.");
        return 0;
    }
    lua_pushstring((lua_State *)params[1], MF_GetAmxString(amx, params[2], 0, NULL));
    return 0;
}

static cell AMX_NATIVE_CALL n_lua_pushboolean(AMX *amx, cell *params)
{
    if (!(lua_State *)params[1])
    {
        MF_Log("n_lua_pushboolean: Invalid Lua state.");
        return 0;
    }
    lua_pushboolean((lua_State *)params[1], params[2]);
    return 0;
}

static cell AMX_NATIVE_CALL n_lua_pushnil(AMX *amx, cell *params)
{
    if (!(lua_State *)params[1])
    {
        MF_Log("n_lua_pushnil: Invalid Lua state.");
        return 0;
    }
    lua_pushnil((lua_State *)params[1]);
    return 0;
}

// ---------------------------------------------------------
// Native 实现: 转换 (To)
// ---------------------------------------------------------

static cell AMX_NATIVE_CALL n_lua_tointeger(AMX *amx, cell *params)
{
    if (!(lua_State *)params[1])
    {
        MF_Log("n_lua_tointeger: Invalid Lua state.");
        return 0;
    }
    return (cell)lua_tointeger((lua_State *)params[1], params[2]);
}

static cell AMX_NATIVE_CALL n_lua_tonumber(AMX *amx, cell *params)
{
    if (!(lua_State *)params[1])
    {
        MF_Log("n_lua_tonumber: Invalid Lua state.");
        return 0;
    }
    float res = (float)lua_tonumber((lua_State *)params[1], params[2]);
    return amx_ftoc(res);
}

static cell AMX_NATIVE_CALL n_lua_tostring(AMX *amx, cell *params)
{
    if (!(lua_State *)params[1])
    {
        MF_Log("n_lua_tostring: Invalid Lua state.");
        return 0;
    }
    const char *str = lua_tostring((lua_State *)params[1], params[2]);
    return MF_SetAmxStringUTF8Char(amx, params[3], str ? str : "", str ? strlen(str) : 0, params[4]);
}

static cell AMX_NATIVE_CALL n_lua_toboolean(AMX *amx, cell *params)
{
    if (!(lua_State *)params[1])
    {
        MF_Log("n_lua_toboolean: Invalid Lua state.");
        return 0;
    }
    return lua_toboolean((lua_State *)params[1], params[2]);
}

// ---------------------------------------------------------
// Native 实现: 表与变量操作
// ---------------------------------------------------------

static cell AMX_NATIVE_CALL n_lua_getglobal(AMX *amx, cell *params)
{
    if (!(lua_State *)params[1])
    {
        MF_Log("n_lua_getglobal: Invalid Lua state.");
        return 0;
    }
    return lua_getglobal((lua_State *)params[1], MF_GetAmxString(amx, params[2], 0, NULL));
}

static cell AMX_NATIVE_CALL n_lua_setglobal(AMX *amx, cell *params)
{
    if (!(lua_State *)params[1])
    {
        MF_Log("n_lua_setglobal: Invalid Lua state.");
        return 0;
    }
    lua_setglobal((lua_State *)params[1], MF_GetAmxString(amx, params[2], 0, NULL));
    return 0;
}

static cell AMX_NATIVE_CALL n_lua_getfield(AMX *amx, cell *params)
{
    if (!(lua_State *)params[1])
    {
        MF_Log("n_lua_getfield: Invalid Lua state.");
        return 0;
    }
    return lua_getfield((lua_State *)params[1], params[2], MF_GetAmxString(amx, params[3], 0, NULL));
}

static cell AMX_NATIVE_CALL n_lua_setfield(AMX *amx, cell *params)
{
    if (!(lua_State *)params[1])
    {
        MF_Log("n_lua_setfield: Invalid Lua state.");
        return 0;
    }
    lua_setfield((lua_State *)params[1], params[2], MF_GetAmxString(amx, params[3], 0, NULL));
    return 0;
}

static cell AMX_NATIVE_CALL n_lua_createtable(AMX *amx, cell *params)
{
    if (!(lua_State *)params[1])
    {
        MF_Log("n_lua_createtable: Invalid Lua state.");
        return 0;
    }
    lua_createtable((lua_State *)params[1], params[2], params[3]);
    return 0;
}

static cell AMX_NATIVE_CALL n_lua_settable(AMX *amx, cell *params)
{
    if (!(lua_State *)params[1])
    {
        MF_Log("n_lua_settable: Invalid Lua state.");
        return 0;
    }
    lua_settable((lua_State *)params[1], params[2]);
    return 0;
}

static cell AMX_NATIVE_CALL n_lua_gettable(AMX *amx, cell *params)
{
    if (!(lua_State *)params[1])
    {
        MF_Log("n_lua_gettable: Invalid Lua state.");
        return 0;
    }
    return lua_gettable((lua_State *)params[1], params[2]);
}

static cell AMX_NATIVE_CALL n_lua_rawlen(AMX *amx, cell *params)
{
    if (!(lua_State *)params[1])
    {
        MF_Log("n_lua_rawlen: Invalid Lua state.");
        return 0;
    }
    return (cell)lua_rawlen((lua_State *)params[1], params[2]);
}

cell AMX_NATIVE_CALL Native_LuaRef(AMX *amx, cell *params)
{
    if (!(lua_State *)params[1])
    {
        MF_Log("Native_LuaRef: Invalid Lua state.");
        return 0;
    }
    lua_State *L = (lua_State *)params[1];
    int idx = params[2];

    // 将指定位置的值(函数)压入栈顶，因为 luaL_ref 会消耗栈顶元素
    lua_pushvalue(L, idx);

    // 存入 Registry 表，并返回引用的整数 ID
    int ref_id = luaL_ref(L, LUA_REGISTRYINDEX);

    return ref_id;
}

cell AMX_NATIVE_CALL Native_LuaUnref(AMX *amx, cell *params)
{
    if (!(lua_State *)params[1])
    {
        MF_Log("Native_LuaUnref: Invalid Lua state.");
        return 0;
    }
    lua_State *L = (lua_State *)params[1];
    int ref_id = params[2];

    // 从 Registry 表中移除该引用
    luaL_unref(L, LUA_REGISTRYINDEX, ref_id);

    return 0;
}

cell AMX_NATIVE_CALL Native_LuaGetRef(AMX *amx, cell *params)
{
    if (!(lua_State *)params[1])
    {
        MF_Log("Native_LuaGetRef: Invalid Lua state.");
        return 0;
    }
    lua_State *L = (lua_State *)params[1];
    int ref_id = params[2];

    // 从 Registry 表中读取 ID 对应的内容压入栈顶
    lua_rawgeti(L, LUA_REGISTRYINDEX, ref_id);

    return 0;
}

// ---------------------------------------------------------
// Native 实现: 调用与错误处理
// ---------------------------------------------------------

static cell AMX_NATIVE_CALL n_lua_pcall(AMX *amx, cell *params)
{
    if (!(lua_State *)params[1])
    {
        MF_Log("n_lua_pcall: Invalid Lua state.");
        return 0;
    }
    return lua_pcall((lua_State *)params[1], params[2], params[3], params[4]);
}

// ---------------------------------------------------------
// 注册 Native 列表
// ---------------------------------------------------------

AMX_NATIVE_INFO LuaNatives[] = {
    {"lua_open", n_lua_open},
    {"lua_close", n_lua_close},
    {"lua_dostring", n_lua_dostring},
    {"lua_gettop", n_lua_gettop},
    {"lua_settop", n_lua_settop},
    {"lua_pop", n_lua_pop},
    {"lua_type", n_lua_type},
    {"lua_pushvalue", n_lua_pushvalue},
    {"lua_remove", n_lua_remove},
    {"lua_pushinteger", n_lua_pushinteger},
    {"lua_pushnumber", n_lua_pushnumber},
    {"lua_pushstring", n_lua_pushstring},
    {"lua_pushboolean", n_lua_pushboolean},
    {"lua_pushnil", n_lua_pushnil},
    {"lua_tointeger", n_lua_tointeger},
    {"lua_tonumber", n_lua_tonumber},
    {"lua_tostring", n_lua_tostring},
    {"lua_toboolean", n_lua_toboolean},
    {"lua_getglobal", n_lua_getglobal},
    {"lua_setglobal", n_lua_setglobal},
    {"lua_getfield", n_lua_getfield},
    {"lua_setfield", n_lua_setfield},
    {"lua_createtable", n_lua_createtable},
    {"lua_settable", n_lua_settable},
    {"lua_gettable", n_lua_gettable},
    {"lua_rawlen", n_lua_rawlen},
    {"lua_pcall", n_lua_pcall},
    {"lua_register_function", Native_LuaRegisterFunction},
    {"lua_getref", Native_LuaGetRef},
    {"lua_unref", Native_LuaUnref},
    {"lua_ref", Native_LuaRef},
    {NULL, NULL}};

void OnAmxxAttach()
{
    MF_AddNatives(LuaNatives);
}
void TrimString(char *str)
{
    size_t len = strlen(str);
    while (len > 0 && (str[len - 1] == '\n' || str[len - 1] == '\r' || str[len - 1] == ' '))
    {
        str[--len] = '\0';
    }
}
void OnPluginsLoaded()
{
    // 构建路径 (建议使用 MF_BuildPathname 以兼容不同模组目录，但这里先用你指定的路径)
    const char *szFile = "cstrike/addons/amxmodx/luascripting/function.txt";

    // 获取完整路径 (推荐做法，防止 cstrike 目录名变化)
    // char fullPath[256];
    // MF_BuildPathname(fullPath, sizeof(fullPath), "%s", szFile);

    FILE *fp = fopen(szFile, "rt");
    if (!fp)
    {
        MF_Log("Error: Failed to open file %s", szFile);
        return;
    }
    char buffer[128];
    // 逐行读取
    while (fgets(buffer, sizeof(buffer), fp))
    {
        // 1. 清理换行符
        TrimString(buffer);

        // 2. 跳过空行或注释 (# 或 //)
        if (buffer[0] == '\0' || buffer[0] == '#' || (buffer[0] == '/' && buffer[1] == '/'))
        {
            continue;
        }
        g_LuaPawnFuncMap.insert(buffer, MF_RegisterForward(buffer, ET_STOP, FP_CELL, FP_DONE));
        // MF_Log("Registered Pawn function '%s' for Lua", buffer);
    }

    fclose(fp);
}

/* pfnGameInit() */
void GameDLLInit(void)
{
    if (!g_L)
        RETURN_META(MRES_IGNORED);

    lua_getglobal(g_L, "MetaGameDLLInit");

    if (!lua_isfunction(g_L, -1))
    {
        lua_pop(g_L, 1);
        RETURN_META(MRES_IGNORED);
    }

    if (lua_pcall(g_L, 0, 0, 0) != 0)
    {
        lua_pop(g_L, 1);
    }

    RETURN_META(MRES_IGNORED);
}
/* pfnSpawn() */
int DispatchSpawn(edict_t *pent)
{
    if (!g_L)
        RETURN_META_VALUE(MRES_IGNORED, -1);

    lua_getglobal(g_L, "MetaDispatchSpawn");

    if (!lua_isfunction(g_L, -1))
    {
        lua_pop(g_L, 1);
        RETURN_META_VALUE(MRES_IGNORED, -1);
    }

    // Push: edict_t* (pointer)
    lua_pushlightuserdata(g_L, pent);

    if (lua_pcall(g_L, 1, 0, 0) != 0)
    {
        lua_pop(g_L, 1);
    }

    RETURN_META_VALUE(MRES_IGNORED, 0);
}

/* pfnThink() */
void DispatchThink(edict_t *pent)
{
    if (!g_L)
        RETURN_META(MRES_IGNORED);

    lua_getglobal(g_L, "MetaDispatchThink");

    if (!lua_isfunction(g_L, -1))
    {
        lua_pop(g_L, 1);
        RETURN_META(MRES_IGNORED);
    }

    // Push: edict_t* (pointer)
    lua_pushlightuserdata(g_L, pent);

    if (lua_pcall(g_L, 1, 0, 0) != 0)
    {
        lua_pop(g_L, 1);
    }

    RETURN_META(MRES_IGNORED);
}

/* pfnUse() */
void DispatchUse(edict_t *pentUsed, edict_t *pentOther)
{
    if (!g_L)
        RETURN_META(MRES_IGNORED);

    lua_getglobal(g_L, "MetaDispatchUse");

    if (!lua_isfunction(g_L, -1))
    {
        lua_pop(g_L, 1);
        RETURN_META(MRES_IGNORED);
    }

    // Push: edict_t* (Used), edict_t* (Other)
    lua_pushlightuserdata(g_L, pentUsed);
    lua_pushlightuserdata(g_L, pentOther);

    if (lua_pcall(g_L, 2, 0, 0) != 0)
    {
        lua_pop(g_L, 1);
    }

    RETURN_META(MRES_IGNORED);
}

/* pfnTouch() */
void DispatchTouch(edict_t *pentTouched, edict_t *pentOther)
{
    if (!g_L)
        RETURN_META(MRES_IGNORED);

    lua_getglobal(g_L, "MetaDispatchTouch");

    if (!lua_isfunction(g_L, -1))
    {
        lua_pop(g_L, 1);
        RETURN_META(MRES_IGNORED);
    }

    // Push: edict_t* (Touched), edict_t* (Other)
    lua_pushlightuserdata(g_L, pentTouched);
    lua_pushlightuserdata(g_L, pentOther);

    if (lua_pcall(g_L, 2, 0, 0) != 0)
    {
        lua_pop(g_L, 1);
    }

    RETURN_META(MRES_IGNORED);
}

/* pfnBlocked() */
void DispatchBlocked(edict_t *pentBlocked, edict_t *pentOther)
{
    if (!g_L)
        RETURN_META(MRES_IGNORED);

    lua_getglobal(g_L, "MetaDispatchBlocked");

    if (!lua_isfunction(g_L, -1))
    {
        lua_pop(g_L, 1);
        RETURN_META(MRES_IGNORED);
    }

    // Push: edict_t* (Blocked), edict_t* (Other)
    lua_pushlightuserdata(g_L, pentBlocked);
    lua_pushlightuserdata(g_L, pentOther);

    if (lua_pcall(g_L, 2, 0, 0) != 0)
    {
        lua_pop(g_L, 1);
    }

    RETURN_META(MRES_IGNORED);
}

/* pfnKeyValue() */
void DispatchKeyValue(edict_t *pentKeyvalue, KeyValueData *pkvd)
{
    if (!g_L)
        RETURN_META(MRES_IGNORED);

    lua_getglobal(g_L, "MetaDispatchKeyValue");

    if (!lua_isfunction(g_L, -1))
    {
        lua_pop(g_L, 1);
        RETURN_META(MRES_IGNORED);
    }

    // Push: edict_t* (Entity)
    lua_pushlightuserdata(g_L, pentKeyvalue);
    
    // Push: Key (string), Value (string), ClassName (string)
    // 这样在 Lua 里可以直接读取配置，而不是拿到一个空指针
    if (pkvd) {
        lua_pushstring(g_L, pkvd->szKeyName);
        lua_pushstring(g_L, pkvd->szValue);
        lua_pushstring(g_L, pkvd->szClassName);
    } else {
        lua_pushnil(g_L);
        lua_pushnil(g_L);
        lua_pushnil(g_L);
    }

    if (lua_pcall(g_L, 4, 0, 0) != 0)
    {
        lua_pop(g_L, 1);
    }

    RETURN_META(MRES_IGNORED);
}

/* pfnSave() */
void DispatchSave(edict_t *pent, SAVERESTOREDATA *pSaveData)
{
    if (!g_L)
        RETURN_META(MRES_IGNORED);

    lua_getglobal(g_L, "MetaDispatchSave");

    if (!lua_isfunction(g_L, -1))
    {
        lua_pop(g_L, 1);
        RETURN_META(MRES_IGNORED);
    }

    // Push: edict_t* (Entity), SAVERESTOREDATA* (pointer)
    lua_pushlightuserdata(g_L, pent);
    lua_pushlightuserdata(g_L, pSaveData);

    if (lua_pcall(g_L, 2, 0, 0) != 0)
    {
        lua_pop(g_L, 1);
    }

    RETURN_META(MRES_IGNORED);
}

/* pfnRestore() */
int DispatchRestore(edict_t *pent, SAVERESTOREDATA *pSaveData, int globalEntity)
{
    if (!g_L)
        RETURN_META_VALUE(MRES_IGNORED, 0);

    lua_getglobal(g_L, "MetaDispatchRestore");

    if (!lua_isfunction(g_L, -1))
    {
        lua_pop(g_L, 1);
        RETURN_META_VALUE(MRES_IGNORED, 0);
    }

    // Push: edict_t* (Entity), SAVERESTOREDATA* (pointer), globalEntity (int)
    lua_pushlightuserdata(g_L, pent);
    lua_pushlightuserdata(g_L, pSaveData);
    lua_pushinteger(g_L, globalEntity);

    if (lua_pcall(g_L, 3, 0, 0) != 0)
    {
        lua_pop(g_L, 1);
    }

    RETURN_META_VALUE(MRES_IGNORED, 0);
}

/* pfnSetAbsBox() */
void DispatchObjectCollsionBox(edict_t *pent)
{
    if (!g_L)
        RETURN_META(MRES_IGNORED);

    lua_getglobal(g_L, "MetaDispatchObjectCollsionBox");

    if (!lua_isfunction(g_L, -1))
    {
        lua_pop(g_L, 1);
        RETURN_META(MRES_IGNORED);
    }

    // Push: edict_t* (pointer)
    lua_pushlightuserdata(g_L, pent);

    if (lua_pcall(g_L, 1, 0, 0) != 0)
    {
        lua_pop(g_L, 1);
    }

    RETURN_META(MRES_IGNORED);
}

/* pfnSaveWriteFields() */
void SaveWriteFields(SAVERESTOREDATA *pSaveData, const char *pname, void *pBaseData, TYPEDESCRIPTION *pFields, int fieldCount)
{
    if (!g_L)
        RETURN_META(MRES_IGNORED);

    lua_getglobal(g_L, "MetaSaveWriteFields");

    if (!lua_isfunction(g_L, -1))
    {
        lua_pop(g_L, 1);
        RETURN_META(MRES_IGNORED);
    }

    // Push: SAVERESTOREDATA* (ptr)
    lua_pushlightuserdata(g_L, pSaveData);
    // Push: Name (string)
    lua_pushstring(g_L, pname);
    // Push: BaseData* (ptr)
    lua_pushlightuserdata(g_L, pBaseData);
    // Push: TYPEDESCRIPTION* (ptr)
    lua_pushlightuserdata(g_L, pFields);
    // Push: fieldCount (int)
    lua_pushinteger(g_L, fieldCount);

    if (lua_pcall(g_L, 5, 0, 0) != 0)
    {
        lua_pop(g_L, 1);
    }

    RETURN_META(MRES_IGNORED);
}

/* pfnSaveReadFields() */
void SaveReadFields(SAVERESTOREDATA *pSaveData, const char *pname, void *pBaseData, TYPEDESCRIPTION *pFields, int fieldCount)
{
    if (!g_L)
        RETURN_META(MRES_IGNORED);

    lua_getglobal(g_L, "MetaSaveReadFields");

    if (!lua_isfunction(g_L, -1))
    {
        lua_pop(g_L, 1);
        RETURN_META(MRES_IGNORED);
    }

    // Push: SAVERESTOREDATA* (ptr)
    lua_pushlightuserdata(g_L, pSaveData);
    // Push: Name (string)
    lua_pushstring(g_L, pname);
    // Push: BaseData* (ptr)
    lua_pushlightuserdata(g_L, pBaseData);
    // Push: TYPEDESCRIPTION* (ptr)
    lua_pushlightuserdata(g_L, pFields);
    // Push: fieldCount (int)
    lua_pushinteger(g_L, fieldCount);

    if (lua_pcall(g_L, 5, 0, 0) != 0)
    {
        lua_pop(g_L, 1);
    }

    RETURN_META(MRES_IGNORED);
}

/* pfnSaveGlobalState() */
void SaveGlobalState(SAVERESTOREDATA *pSaveData)
{
    if (!g_L)
        RETURN_META(MRES_IGNORED);

    lua_getglobal(g_L, "MetaSaveGlobalState");

    if (!lua_isfunction(g_L, -1))
    {
        lua_pop(g_L, 1);
        RETURN_META(MRES_IGNORED);
    }

    // Push: SAVERESTOREDATA* (ptr)
    lua_pushlightuserdata(g_L, pSaveData);

    if (lua_pcall(g_L, 1, 0, 0) != 0)
    {
        lua_pop(g_L, 1);
    }

    RETURN_META(MRES_IGNORED);
}

/* pfnRestoreGlobalState() */
void RestoreGlobalState(SAVERESTOREDATA *pSaveData)
{
    if (!g_L)
        RETURN_META(MRES_IGNORED);

    lua_getglobal(g_L, "MetaRestoreGlobalState");

    if (!lua_isfunction(g_L, -1))
    {
        lua_pop(g_L, 1);
        RETURN_META(MRES_IGNORED);
    }

    // Push: SAVERESTOREDATA* (ptr)
    lua_pushlightuserdata(g_L, pSaveData);

    if (lua_pcall(g_L, 1, 0, 0) != 0)
    {
        lua_pop(g_L, 1);
    }

    RETURN_META(MRES_IGNORED);
}

/* pfnResetGlobalState() */
void ResetGlobalState(void)
{
    if (!g_L)
        RETURN_META(MRES_IGNORED);

    lua_getglobal(g_L, "MetaResetGlobalState");

    if (!lua_isfunction(g_L, -1))
    {
        lua_pop(g_L, 1);
        RETURN_META(MRES_IGNORED);
    }

    if (lua_pcall(g_L, 0, 0, 0) != 0)
    {
        lua_pop(g_L, 1);
    }

    RETURN_META(MRES_IGNORED);
}

/* pfnClientConnect() */
/* pfnClientConnect() */
qboolean ClientConnect(edict_t *pEntity, const char *pszName, const char *pszAddress, char szRejectReason[128])
{
    if (!g_L)
        RETURN_META_VALUE(MRES_IGNORED, 0); // 0 = FALSE, 但 META 会忽略

    lua_getglobal(g_L, "MetaClientConnect");

    if (!lua_isfunction(g_L, -1))
    {
        lua_pop(g_L, 1);
        RETURN_META_VALUE(MRES_IGNORED, 0);
    }

    // Push: edict_t* (玩家实体)
    lua_pushlightuserdata(g_L, pEntity);
    // Push: Name (名字)
    lua_pushstring(g_L, pszName);
    // Push: IP (IP地址)
    lua_pushstring(g_L, pszAddress);
    
    // 调用 Lua: 3个参数, 1个返回值
    if (lua_pcall(g_L, 3, 1, 0) != 0)
    {
        lua_pop(g_L, 1);
        RETURN_META_VALUE(MRES_IGNORED, 0);
    }

    // 处理返回值
    // 如果 Lua 返回了一个字符串，说明要拒绝连接，并且这个字符串是拒绝理由
    if (lua_isstring(g_L, -1))
    {
        const char* reason = lua_tostring(g_L, -1);
        
        // 把 Lua 的拒绝理由复制回 C++ 的 buffer
        // 注意防止溢出 (128字节)
        strncpy(szRejectReason, reason, 127);
        szRejectReason[127] = '\0'; // 确保结尾

        lua_pop(g_L, 1); // 弹出返回值

        // 返回 FALSE 告诉引擎拒绝连接，并使用 MRES_SUPERCEDE 覆盖引擎原本的逻辑
        RETURN_META_VALUE(MRES_SUPERCEDE, 0); 
    }

    // 如果没返回字符串（比如返回 nil 或 true），则允许连接
    lua_pop(g_L, 1);
    RETURN_META_VALUE(MRES_IGNORED, 0);
}

/* pfnClientDisconnect() */
void ClientDisconnect(edict_t *pEntity)
{
    if (!g_L)
        RETURN_META(MRES_IGNORED);

    lua_getglobal(g_L, "MetaClientDisconnect");

    if (!lua_isfunction(g_L, -1))
    {
        lua_pop(g_L, 1);
        RETURN_META(MRES_IGNORED);
    }

    // Push: edict_t* (Player Entity)
    lua_pushlightuserdata(g_L, pEntity);

    if (lua_pcall(g_L, 1, 0, 0) != 0)
    {
        lua_pop(g_L, 1);
    }

    RETURN_META(MRES_IGNORED);
}

/* pfnClientKill() */
void ClientKill(edict_t *pEntity)
{
    if (!g_L)
        RETURN_META(MRES_IGNORED);

    lua_getglobal(g_L, "MetaClientKill");

    if (!lua_isfunction(g_L, -1))
    {
        lua_pop(g_L, 1);
        RETURN_META(MRES_IGNORED);
    }

    // Push: edict_t* (Player Entity)
    lua_pushlightuserdata(g_L, pEntity);

    if (lua_pcall(g_L, 1, 0, 0) != 0)
    {
        lua_pop(g_L, 1);
    }

    RETURN_META(MRES_IGNORED);
}

/* pfnClientPutInServer() */
void ClientPutInServer(edict_t *pEntity)
{
    if (!g_L)
        RETURN_META(MRES_IGNORED);

    lua_getglobal(g_L, "MetaClientPutInServer");

    if (!lua_isfunction(g_L, -1))
    {
        lua_pop(g_L, 1);
        RETURN_META(MRES_IGNORED);
    }

    // Push: edict_t* (Player Entity)
    lua_pushlightuserdata(g_L, pEntity);

    if (lua_pcall(g_L, 1, 0, 0) != 0)
    {
        lua_pop(g_L, 1);
    }

    RETURN_META(MRES_IGNORED);
}

/* pfnClientCommand() */
void ClientCommand(edict_t *pEntity)
{
    if (!g_L)
        RETURN_META(MRES_IGNORED);

    lua_getglobal(g_L, "MetaClientCommand");

    if (!lua_isfunction(g_L, -1))
    {
        lua_pop(g_L, 1);
        RETURN_META(MRES_IGNORED);
    }

    // Push: edict_t* (Player Entity)
    lua_pushlightuserdata(g_L, pEntity);
    // 注意: 命令的具体内容在 Lua 中需要通过调用 engine.Cmd_Args() 等函数获取

    if (lua_pcall(g_L, 1, 0, 0) != 0)
    {
        lua_pop(g_L, 1);
    }

    RETURN_META(MRES_IGNORED);
}

/* pfnClientUserInfoChanged() */
void ClientUserInfoChanged(edict_t *pEntity, char *infobuffer)
{
    if (!g_L)
        RETURN_META(MRES_IGNORED);

    lua_getglobal(g_L, "MetaClientUserInfoChanged");

    if (!lua_isfunction(g_L, -1))
    {
        lua_pop(g_L, 1);
        RETURN_META(MRES_IGNORED);
    }

    // Push: edict_t* (Player Entity)
    lua_pushlightuserdata(g_L, pEntity);
    // Push: infobuffer string (因为只是读取，复制一份给Lua没问题)
    lua_pushstring(g_L, infobuffer);

    if (lua_pcall(g_L, 2, 0, 0) != 0)
    {
        lua_pop(g_L, 1);
    }

    RETURN_META(MRES_IGNORED);
}

/* pfnServerActivate() */
void ServerActivate(edict_t *pEdictList, int edictCount, int clientMax)
{
    if (!g_L)
        RETURN_META(MRES_IGNORED);

    // 可以在这里做一些 Map 初始化相关的工作，比如清空之前的缓存
    // g_EntVarMap.clear(); // 如果需要在换图时重置，看具体需求

    lua_getglobal(g_L, "MetaServerActivate");

    if (!lua_isfunction(g_L, -1))
    {
        lua_pop(g_L, 1);
        RETURN_META(MRES_IGNORED);
    }

    // Push: pEdictList (指针)
    lua_pushlightuserdata(g_L, pEdictList);
    // Push: edictCount (int)
    lua_pushinteger(g_L, edictCount);
    // Push: clientMax (int)
    lua_pushinteger(g_L, clientMax);

    if (lua_pcall(g_L, 3, 0, 0) != 0)
    {
        lua_pop(g_L, 1);
    }

    RETURN_META(MRES_IGNORED);
}

/* pfnServerDeactivate() */
void ServerDeactivate(void)
{
    if (!g_L)
        RETURN_META(MRES_IGNORED);

    lua_getglobal(g_L, "MetaServerDeactivate");

    if (!lua_isfunction(g_L, -1))
    {
        lua_pop(g_L, 1);
        RETURN_META(MRES_IGNORED);
    }

    if (lua_pcall(g_L, 0, 0, 0) != 0)
    {
        lua_pop(g_L, 1);
    }

    RETURN_META(MRES_IGNORED);
}

/* pfnPlayerPreThink() */
void PlayerPreThink(edict_t *pEntity)
{
    if (!g_L)
        RETURN_META(MRES_IGNORED);

    lua_getglobal(g_L, "MetaPlayerPreThink");

    if (!lua_isfunction(g_L, -1))
    {
        lua_pop(g_L, 1);
        RETURN_META(MRES_IGNORED);
    }

    // Push: edict_t* (Player Entity)
    lua_pushlightuserdata(g_L, pEntity);

    if (lua_pcall(g_L, 1, 0, 0) != 0)
    {
        lua_pop(g_L, 1);
    }

    RETURN_META(MRES_IGNORED);
}

/* pfnPlayerPostThink() */
void PlayerPostThink(edict_t *pEntity)
{
    if (!g_L)
        RETURN_META(MRES_IGNORED);

    lua_getglobal(g_L, "MetaPlayerPostThink");

    if (!lua_isfunction(g_L, -1))
    {
        lua_pop(g_L, 1);
        RETURN_META(MRES_IGNORED);
    }

    // Push: edict_t* (Player Entity)
    lua_pushlightuserdata(g_L, pEntity);

    if (lua_pcall(g_L, 1, 0, 0) != 0)
    {
        lua_pop(g_L, 1);
    }

    RETURN_META(MRES_IGNORED);
}

/* pfnStartFrame() */
void StartFrame(void)
{
    if (!g_L)
        RETURN_META(MRES_IGNORED);

    lua_getglobal(g_L, "MetaStartFrame");

    if (!lua_isfunction(g_L, -1))
    {
        lua_pop(g_L, 1);
        RETURN_META(MRES_IGNORED);
    }

    if (lua_pcall(g_L, 0, 0, 0) != 0)
    {
        lua_pop(g_L, 1);
    }

    RETURN_META(MRES_IGNORED);
}
/* pfnParmsNewLevel() */
void ParmsNewLevel(void)
{
    if (!g_L)
        RETURN_META(MRES_IGNORED);

    lua_getglobal(g_L, "MetaParmsNewLevel");

    if (!lua_isfunction(g_L, -1))
    {
        lua_pop(g_L, 1);
        RETURN_META(MRES_IGNORED);
    }

    if (lua_pcall(g_L, 0, 0, 0) != 0)
    {
        lua_pop(g_L, 1);
    }

    RETURN_META(MRES_IGNORED);
}

/* pfnParmsChangeLevel() */
void ParmsChangeLevel(void)
{
    if (!g_L)
        RETURN_META(MRES_IGNORED);

    lua_getglobal(g_L, "MetaParmsChangeLevel");

    if (!lua_isfunction(g_L, -1))
    {
        lua_pop(g_L, 1);
        RETURN_META(MRES_IGNORED);
    }

    if (lua_pcall(g_L, 0, 0, 0) != 0)
    {
        lua_pop(g_L, 1);
    }

    RETURN_META(MRES_IGNORED);
}

/* pfnGetGameDescription() */
// 注意：这个函数返回 const char*，用于显示在服务器浏览器的 "Game" 列
const char *GetGameDescription(void)
{
    if (!g_L)
        RETURN_META_VALUE(MRES_IGNORED, NULL);

    lua_getglobal(g_L, "MetaGetGameDescription");

    if (!lua_isfunction(g_L, -1))
    {
        lua_pop(g_L, 1);
        RETURN_META_VALUE(MRES_IGNORED, NULL);
    }

    // 调用 Lua
    if (lua_pcall(g_L, 0, 1, 0) != 0)
    {
        lua_pop(g_L, 1);
        RETURN_META_VALUE(MRES_IGNORED, NULL);
    }

    // 处理返回值
    if (lua_isstring(g_L, -1))
    {
        // 我们必须使用静态缓冲区，因为一旦 lua_pop，指针可能失效
        static char staticGameDesc[64];
        const char *ret = lua_tostring(g_L, -1);
        
        strncpy(staticGameDesc, ret, 63);
        staticGameDesc[63] = '\0';

        lua_pop(g_L, 1);
        // 返回 SUPERCEDE 并带上我们的新名字
        RETURN_META_VALUE(MRES_SUPERCEDE, staticGameDesc);
    }

    lua_pop(g_L, 1);
    RETURN_META_VALUE(MRES_IGNORED, NULL);
}

/* pfnPlayerCustomization() */
void PlayerCustomization(edict_t *pEntity, customization_t *pCustom)
{
    if (!g_L)
        RETURN_META(MRES_IGNORED);

    lua_getglobal(g_L, "MetaPlayerCustomization");

    if (!lua_isfunction(g_L, -1))
    {
        lua_pop(g_L, 1);
        RETURN_META(MRES_IGNORED);
    }

    lua_pushlightuserdata(g_L, pEntity);
    lua_pushlightuserdata(g_L, pCustom);

    if (lua_pcall(g_L, 2, 0, 0) != 0)
    {
        lua_pop(g_L, 1);
    }

    RETURN_META(MRES_IGNORED);
}

/* pfnSpectatorConnect() */
void SpectatorConnect(edict_t *pEntity)
{
    if (!g_L)
        RETURN_META(MRES_IGNORED);

    lua_getglobal(g_L, "MetaSpectatorConnect");

    if (!lua_isfunction(g_L, -1))
    {
        lua_pop(g_L, 1);
        RETURN_META(MRES_IGNORED);
    }

    lua_pushlightuserdata(g_L, pEntity);

    if (lua_pcall(g_L, 1, 0, 0) != 0)
    {
        lua_pop(g_L, 1);
    }

    RETURN_META(MRES_IGNORED);
}

/* pfnSpectatorDisconnect() */
void SpectatorDisconnect(edict_t *pEntity)
{
    if (!g_L)
        RETURN_META(MRES_IGNORED);

    lua_getglobal(g_L, "MetaSpectatorDisconnect");

    if (!lua_isfunction(g_L, -1))
    {
        lua_pop(g_L, 1);
        RETURN_META(MRES_IGNORED);
    }

    lua_pushlightuserdata(g_L, pEntity);

    if (lua_pcall(g_L, 1, 0, 0) != 0)
    {
        lua_pop(g_L, 1);
    }

    RETURN_META(MRES_IGNORED);
}

/* pfnSpectatorThink() */
void SpectatorThink(edict_t *pEntity)
{
    if (!g_L)
        RETURN_META(MRES_IGNORED);

    lua_getglobal(g_L, "MetaSpectatorThink");

    if (!lua_isfunction(g_L, -1))
    {
        lua_pop(g_L, 1);
        RETURN_META(MRES_IGNORED);
    }

    lua_pushlightuserdata(g_L, pEntity);

    if (lua_pcall(g_L, 1, 0, 0) != 0)
    {
        lua_pop(g_L, 1);
    }

    RETURN_META(MRES_IGNORED);
}

/* pfnSys_Error() */
void Sys_Error(const char *error_string)
{
    // 这个函数调用意味着服务器即将崩溃或强制关闭
    if (!g_L)
        RETURN_META(MRES_IGNORED);

    lua_getglobal(g_L, "MetaSys_Error");

    if (!lua_isfunction(g_L, -1))
    {
        lua_pop(g_L, 1);
        RETURN_META(MRES_IGNORED);
    }

    lua_pushstring(g_L, error_string);

    if (lua_pcall(g_L, 1, 0, 0) != 0)
    {
        lua_pop(g_L, 1);
    }

    RETURN_META(MRES_IGNORED);
}

/* pfnPM_Move() */
void PM_Move(struct playermove_s *ppmove, qboolean server)
{
    if (!g_L)
        RETURN_META(MRES_IGNORED);

    lua_getglobal(g_L, "MetaPM_Move");

    if (!lua_isfunction(g_L, -1))
    {
        lua_pop(g_L, 1);
        RETURN_META(MRES_IGNORED);
    }

    lua_pushlightuserdata(g_L, ppmove);
    lua_pushboolean(g_L, server); // qboolean 本质是 int，但在 Lua 里用 bool 更直观

    if (lua_pcall(g_L, 2, 0, 0) != 0)
    {
        lua_pop(g_L, 1);
    }

    RETURN_META(MRES_IGNORED);
}

/* pfnPM_Init() */
void PM_Init(struct playermove_s *ppmove)
{
    if (!g_L)
        RETURN_META(MRES_IGNORED);

    lua_getglobal(g_L, "MetaPM_Init");

    if (!lua_isfunction(g_L, -1))
    {
        lua_pop(g_L, 1);
        RETURN_META(MRES_IGNORED);
    }

    lua_pushlightuserdata(g_L, ppmove);

    if (lua_pcall(g_L, 1, 0, 0) != 0)
    {
        lua_pop(g_L, 1);
    }

    RETURN_META(MRES_IGNORED);
}

/* pfnSetupVisibility() */
void SetupVisibility(struct edict_s *pViewEntity, struct edict_s *pClient, unsigned char **pvs, unsigned char **pas)
{
    if (!g_L) RETURN_META(MRES_IGNORED);

    lua_getglobal(g_L, "MetaSetupVisibility");
    if (!lua_isfunction(g_L, -1)) {
        lua_pop(g_L, 1);
        RETURN_META(MRES_IGNORED);
    }

    // Args: ViewEntity, Client, pvs ptr, pas ptr
    lua_pushlightuserdata(g_L, pViewEntity);
    lua_pushlightuserdata(g_L, pClient);
    lua_pushlightuserdata(g_L, pvs);
    lua_pushlightuserdata(g_L, pas);

    if (lua_pcall(g_L, 4, 0, 0) != 0) {
        lua_pop(g_L, 1);
    }
    RETURN_META(MRES_IGNORED);
}

/* pfnUpdateClientData() */
void UpdateClientData(const struct edict_s *ent, int sendweapons, struct clientdata_s *cd)
{
    if (!g_L) RETURN_META(MRES_IGNORED);

    lua_getglobal(g_L, "MetaUpdateClientData");
    if (!lua_isfunction(g_L, -1)) {
        lua_pop(g_L, 1);
        RETURN_META(MRES_IGNORED);
    }

    // Args: Entity, SendWeapons(int), ClientData ptr
    lua_pushlightuserdata(g_L, (void*)ent);
    lua_pushinteger(g_L, sendweapons);
    lua_pushlightuserdata(g_L, cd);

    if (lua_pcall(g_L, 3, 0, 0) != 0) {
        lua_pop(g_L, 1);
    }
    RETURN_META(MRES_IGNORED);
}

/* pfnAddToFullPack() - 警告：高频调用 */
int AddToFullPack(struct entity_state_s *state, int e, edict_t *ent, edict_t *host, int hostflags, int player, unsigned char *pSet)
{
    if (!g_L) RETURN_META_VALUE(MRES_IGNORED, 0);

    lua_getglobal(g_L, "MetaAddToFullPack");
    if (!lua_isfunction(g_L, -1)) {
        lua_pop(g_L, 1);
        RETURN_META_VALUE(MRES_IGNORED, 0);
    }

    // Args: state*, e_index, ent*, host*, hostflags, player(bool/int), pSet*
    lua_pushlightuserdata(g_L, state);
    lua_pushinteger(g_L, e);
    lua_pushlightuserdata(g_L, ent);
    lua_pushlightuserdata(g_L, host);
    lua_pushinteger(g_L, hostflags);
    lua_pushinteger(g_L, player);
    lua_pushlightuserdata(g_L, pSet);

    if (lua_pcall(g_L, 7, 1, 0) != 0) {
        lua_pop(g_L, 1); // pop error
        RETURN_META_VALUE(MRES_IGNORED, 0);
    }

    // 如果 Lua 返回 1 或 0，我们可以覆盖引擎的判断
    if (lua_isnumber(g_L, -1)) {
        int ret = (int)lua_tointeger(g_L, -1);
        lua_pop(g_L, 1);
        RETURN_META_VALUE(MRES_SUPERCEDE, ret);
    }

    lua_pop(g_L, 1);
    RETURN_META_VALUE(MRES_IGNORED, 0);
}

/* pfnCreateBaseline() */
void CreateBaseline(int player, int eindex, struct entity_state_s *baseline, struct edict_s *entity, int playermodelindex, vec3_t player_mins, vec3_t player_maxs)
{
    if (!g_L) RETURN_META(MRES_IGNORED);

    lua_getglobal(g_L, "MetaCreateBaseline");
    if (!lua_isfunction(g_L, -1)) {
        lua_pop(g_L, 1);
        RETURN_META(MRES_IGNORED);
    }

    lua_pushinteger(g_L, player);
    lua_pushinteger(g_L, eindex);
    lua_pushlightuserdata(g_L, baseline);
    lua_pushlightuserdata(g_L, entity);
    lua_pushinteger(g_L, playermodelindex);
    // vec3_t 是 float数组，直接作为指针传过去
    lua_pushlightuserdata(g_L, player_mins); 
    lua_pushlightuserdata(g_L, player_maxs);

    if (lua_pcall(g_L, 7, 0, 0) != 0) {
        lua_pop(g_L, 1);
    }
    RETURN_META(MRES_IGNORED);
}

/* pfnRegisterEncoders() */
void RegisterEncoders(void)
{
    if (!g_L) RETURN_META(MRES_IGNORED);

    lua_getglobal(g_L, "MetaRegisterEncoders");
    if (!lua_isfunction(g_L, -1)) {
        lua_pop(g_L, 1);
        RETURN_META(MRES_IGNORED);
    }

    if (lua_pcall(g_L, 0, 0, 0) != 0) {
        lua_pop(g_L, 1);
    }
    RETURN_META(MRES_IGNORED);
}

/* pfnGetWeaponData() */
int GetWeaponData(struct edict_s *player, struct weapon_data_s *info)
{
    if (!g_L) RETURN_META_VALUE(MRES_IGNORED, 0);

    lua_getglobal(g_L, "MetaGetWeaponData");
    if (!lua_isfunction(g_L, -1)) {
        lua_pop(g_L, 1);
        RETURN_META_VALUE(MRES_IGNORED, 0);
    }

    lua_pushlightuserdata(g_L, player);
    lua_pushlightuserdata(g_L, info);

    if (lua_pcall(g_L, 2, 1, 0) != 0) {
        lua_pop(g_L, 1);
        RETURN_META_VALUE(MRES_IGNORED, 0);
    }

    if (lua_isnumber(g_L, -1)) {
        int ret = (int)lua_tointeger(g_L, -1);
        lua_pop(g_L, 1);
        RETURN_META_VALUE(MRES_SUPERCEDE, ret);
    }

    lua_pop(g_L, 1);
    RETURN_META_VALUE(MRES_IGNORED, 0);
}

/* pfnCmdStart() */
void CmdStart(const edict_t *player, const struct usercmd_s *cmd, unsigned int random_seed)
{
    if (!g_L) RETURN_META(MRES_IGNORED);

    lua_getglobal(g_L, "MetaCmdStart");
    if (!lua_isfunction(g_L, -1)) {
        lua_pop(g_L, 1);
        RETURN_META(MRES_IGNORED);
    }

    lua_pushlightuserdata(g_L, (void*)player);
    lua_pushlightuserdata(g_L, (void*)cmd);
    lua_pushinteger(g_L, random_seed);

    if (lua_pcall(g_L, 3, 0, 0) != 0) {
        lua_pop(g_L, 1);
    }
    RETURN_META(MRES_IGNORED);
}

/* pfnCmdEnd() */
void CmdEnd(const edict_t *player)
{
    if (!g_L) RETURN_META(MRES_IGNORED);

    lua_getglobal(g_L, "MetaCmdEnd");
    if (!lua_isfunction(g_L, -1)) {
        lua_pop(g_L, 1);
        RETURN_META(MRES_IGNORED);
    }

    lua_pushlightuserdata(g_L, (void*)player);

    if (lua_pcall(g_L, 1, 0, 0) != 0) {
        lua_pop(g_L, 1);
    }
    RETURN_META(MRES_IGNORED);
}

/* pfnConnectionlessPacket() */
int ConnectionlessPacket(const struct netadr_s *net_from, const char *args, char *response_buffer, int *response_buffer_size)
{
    if (!g_L) RETURN_META_VALUE(MRES_IGNORED, 0);

    lua_getglobal(g_L, "MetaConnectionlessPacket");
    if (!lua_isfunction(g_L, -1)) {
        lua_pop(g_L, 1);
        RETURN_META_VALUE(MRES_IGNORED, 0);
    }

    // Args: netadr ptr, args (string), response buffer ptr, size ptr
    lua_pushlightuserdata(g_L, (void*)net_from);
    lua_pushstring(g_L, args); // 直接传字符串内容
    lua_pushlightuserdata(g_L, response_buffer);
    lua_pushlightuserdata(g_L, response_buffer_size);

    if (lua_pcall(g_L, 4, 1, 0) != 0) {
        lua_pop(g_L, 1);
        RETURN_META_VALUE(MRES_IGNORED, 0);
    }

    if (lua_isnumber(g_L, -1)) {
        int ret = (int)lua_tointeger(g_L, -1);
        lua_pop(g_L, 1);
        RETURN_META_VALUE(MRES_SUPERCEDE, ret);
    }

    lua_pop(g_L, 1);
    RETURN_META_VALUE(MRES_IGNORED, 0);
}

/* pfnGetHullBounds() */
int GetHullBounds(int hullnumber, float *mins, float *maxs)
{
    if (!g_L) RETURN_META_VALUE(MRES_IGNORED, 0);

    lua_getglobal(g_L, "MetaGetHullBounds");
    if (!lua_isfunction(g_L, -1)) {
        lua_pop(g_L, 1);
        RETURN_META_VALUE(MRES_IGNORED, 0);
    }

    lua_pushinteger(g_L, hullnumber);
    lua_pushlightuserdata(g_L, mins);
    lua_pushlightuserdata(g_L, maxs);

    if (lua_pcall(g_L, 3, 1, 0) != 0) {
        lua_pop(g_L, 1);
        RETURN_META_VALUE(MRES_IGNORED, 0);
    }

    // Lua 如果返回 1，表示该 Hull 有效；0 表示无效
    if (lua_isnumber(g_L, -1)) {
        int ret = (int)lua_tointeger(g_L, -1);
        lua_pop(g_L, 1);
        RETURN_META_VALUE(MRES_SUPERCEDE, ret);
    }

    lua_pop(g_L, 1);
    RETURN_META_VALUE(MRES_IGNORED, 0);
}

/* pfnCreateInstancedBaselines() */
void CreateInstancedBaselines(void)
{
    if (!g_L)
        RETURN_META(MRES_IGNORED);

    lua_getglobal(g_L, "MetaCreateInstancedBaselines");

    if (!lua_isfunction(g_L, -1))
    {
        lua_pop(g_L, 1);
        RETURN_META(MRES_IGNORED);
    }

    if (lua_pcall(g_L, 0, 0, 0) != 0)
    {
        lua_pop(g_L, 1);
    }

    RETURN_META(MRES_IGNORED);
}

/* pfnInconsistentFile() */
int InconsistentFile(const struct edict_s *player, const char *filename, char *disconnect_message)
{
    if (!g_L)
        RETURN_META_VALUE(MRES_IGNORED, 0);

    lua_getglobal(g_L, "MetaInconsistentFile");

    if (!lua_isfunction(g_L, -1))
    {
        lua_pop(g_L, 1);
        RETURN_META_VALUE(MRES_IGNORED, 0);
    }

    // Push: player (edict_t*)
    lua_pushlightuserdata(g_L, (void*)player);
    // Push: filename (string)
    lua_pushstring(g_L, filename);

    // 调用 Lua: 2 参数, 1 返回值
    if (lua_pcall(g_L, 2, 1, 0) != 0)
    {
        lua_pop(g_L, 1);
        RETURN_META_VALUE(MRES_IGNORED, 0);
    }

    // 1. 如果 Lua 返回字符串，说明要断开连接并设置原因
    if (lua_isstring(g_L, -1))
    {
        const char *msg = lua_tostring(g_L, -1);
        if (disconnect_message && msg)
        {
            // 复制消息到输出缓冲区 (最大 256 字符)
            strncpy(disconnect_message, msg, 255);
            disconnect_message[255] = '\0';
        }
        lua_pop(g_L, 1);
        // 返回 1 表示强制断开
        RETURN_META_VALUE(MRES_SUPERCEDE, 1);
    }
    // 2. 如果 Lua 返回数字 (例如 1)
    else if (lua_isnumber(g_L, -1))
    {
        int ret = (int)lua_tointeger(g_L, -1);
        lua_pop(g_L, 1);
        if (ret != 0) // 只要不是0，就拦截
        {
            RETURN_META_VALUE(MRES_SUPERCEDE, ret);
        }
    }

    lua_pop(g_L, 1);
    RETURN_META_VALUE(MRES_IGNORED, 0);
}

/* pfnAllowLagCompensation() */
int AllowLagCompensation(void)
{
    if (!g_L)
        RETURN_META_VALUE(MRES_IGNORED, 0);

    lua_getglobal(g_L, "MetaAllowLagCompensation");

    if (!lua_isfunction(g_L, -1))
    {
        lua_pop(g_L, 1);
        RETURN_META_VALUE(MRES_IGNORED, 0);
    }

    if (lua_pcall(g_L, 0, 1, 0) != 0)
    {
        lua_pop(g_L, 1);
        RETURN_META_VALUE(MRES_IGNORED, 0);
    }

    // 如果 Lua 返回了 0 或 1，覆盖引擎设置
    if (lua_isnumber(g_L, -1))
    {
        int ret = (int)lua_tointeger(g_L, -1);
        lua_pop(g_L, 1);
        RETURN_META_VALUE(MRES_SUPERCEDE, ret);
    }

    lua_pop(g_L, 1);
    RETURN_META_VALUE(MRES_IGNORED, 0);
}