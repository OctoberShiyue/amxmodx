
#include "luaMngr.h"
bool HasReHlds;
bool HasReGameDll;

// 命名空间 ke 是 AMTL 的默认命名空间

lua_State *g_L = nullptr;
// float g_fCurrentTime;
// float g_fNextActionTime;
// typedef ke::HashMap<ke::AString, int,int> g_FuncIdMap;
StringHashMap<int> g_LuaPawnFuncMap;

inline void lua_pushentity(lua_State* L,const edict_t* pent) {
    if (pent && !FNullEnt(pent)) 
        lua_pushinteger(L, ENTINDEX(pent));
    else 
        lua_pushinteger(L, 0);
}


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
    edict_t* pEnt = INDEXENT((int)lua_tointeger(L, 1));
    // 参数2: 属性名 (const char*)
    const char* key = lua_tostring(L, 2);

    if (!pEnt || pEnt->free || !key || pEnt->pvPrivateData == nullptr) {
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
static int L_random_num(lua_State* L)
{
    lua_pushinteger(L, RANDOM_LONG(lua_tointeger(L, 1), lua_tointeger(L, 2)));
    return 1;
}
static int L_edict2index(lua_State* L)
{
    edict_t* pEnt = (edict_t * )lua_touserdata(L, 1);
    if (!pEnt || pEnt->free || pEnt->pvPrivateData == nullptr) {
        lua_pushnil(L);
        return 1;
    }
    lua_pushinteger(L, ENTINDEX(pEnt));
    return 1;
}
static int L_index2edict(lua_State* L)
{
    edict_t* pEnt = INDEXENT(lua_tointeger(L, 1));
    if (!pEnt || pEnt->free || pEnt->pvPrivateData == nullptr) {
        lua_pushnil(L);
        return 1;
    }
    lua_pushlightuserdata(L, pEnt);
    return 1;
}
static int L_position2vector(lua_State* L)
{
    float x = lua_tonumber(L, 1);
    float y = lua_tonumber(L, 2);
    float z = lua_tonumber(L, 3);
    
    Vector* vec = (Vector*)lua_newuserdata(L, sizeof(Vector));
    vec->x = x; 
    vec->y = y; 
    vec->z = z;
    return 1;
}
static int L_vector2position(lua_State* L)
{
    Vector* vec = (Vector*)lua_touserdata(L, 1);
    if (!vec) {
        lua_pushnil(L);
        return 1;
    }
    lua_pushnumber(L, vec->x);
    lua_pushnumber(L, vec->y);
    lua_pushnumber(L, vec->z);
    return 3;
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
    lua_register(L, "amxx_get_gametime", L_get_gametime);
    lua_register(L, "amxx_random_num", L_random_num);
    lua_register(L, "edict2index", L_edict2index);
    lua_register(L, "index2edict", L_index2edict);
    lua_register(L, "position2vector", L_position2vector);
    lua_register(L, "vector2position", L_vector2position);
}
static cell AMX_NATIVE_CALL n_lua_open(AMX *amx, cell *params)
{   
    lua_State *L = luaL_newstate();
    luaL_openlibs(L);
    InitLuaAPI(L);
    LuaInit(L);
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
static cell AMX_NATIVE_CALL n_lua_getL(AMX *amx, cell *params)
{
    return reinterpret_cast<cell>(g_L);
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
    {"lua_getL", n_lua_getL},
    {NULL, NULL}};

void OnAmxxAttach()
{
    HasReHlds    = RehldsApi_Init();
	HasReGameDll = RegamedllApi_Init();

    // RehldsHookchains->Steam_NotifyClientConnect()->registerHook(&Steam_NotifyClientConnect, HC_PRIORITY_HIGH);
    // RehldsHookchains->SV_ConnectClient()->registerHook(&SV_ConnectClient, HC_PRIORITY_HIGH);
    // RehldsHookchains->SV_GetIDString()->registerHook(&SV_GetIDString, HC_PRIORITY_HIGH);
    // RehldsHookchains->SV_SendServerinfo()->registerHook(&SV_SendServerinfo, HC_PRIORITY_HIGH);
    // RehldsHookchains->SV_CheckProtocol()->registerHook(&SV_CheckProtocol, HC_PRIORITY_HIGH);
    // RehldsHookchains->SVC_GetChallenge_mod()->registerHook(&SVC_GetChallenge_mod, HC_PRIORITY_HIGH);
    // RehldsHookchains->SV_CheckKeyInfo()->registerHook(&SV_CheckKeyInfo, HC_PRIORITY_HIGH);
    // RehldsHookchains->SV_CheckIPRestrictions()->registerHook(&SV_CheckIPRestrictions, HC_PRIORITY_HIGH);
    // RehldsHookchains->SV_FinishCertificateCheck()->registerHook(&SV_FinishCertificateCheck, HC_PRIORITY_HIGH);
    // RehldsHookchains->Steam_NotifyBotConnect()->registerHook(&Steam_NotifyBotConnect, HC_PRIORITY_HIGH);
    // RehldsHookchains->SerializeSteamId()->registerHook(&SerializeSteamId, HC_PRIORITY_HIGH);
    // RehldsHookchains->SV_CompareUserID()->registerHook(&SV_CompareUserID, HC_PRIORITY_HIGH);
    // RehldsHookchains->Steam_NotifyClientDisconnect()->registerHook(&Steam_NotifyClientDisconnect, HC_PRIORITY_HIGH);
    // RehldsHookchains->PreprocessPacket()->registerHook(&PreProcessPacket, HC_PRIORITY_HIGH);
    // RehldsHookchains->ValidateCommand()->registerHook(&ValidateCommand, HC_PRIORITY_HIGH);
    // RehldsHookchains->ExecuteServerStringCmd()->registerHook(&ExecuteServerStringCmd, HC_PRIORITY_HIGH);
    // RehldsHookchains->ClientConnected()->registerHook(&ClientConnected, HC_PRIORITY_HIGH);
    // RehldsHookchains->HandleNetCommand()->registerHook(&HandleNetCommand, HC_PRIORITY_HIGH);
    // RehldsHookchains->Mod_LoadBrushModel()->registerHook(&Mod_LoadBrushModel, HC_PRIORITY_HIGH);
    // RehldsHookchains->Mod_LoadStudioModel()->registerHook(&Mod_LoadStudioModel, HC_PRIORITY_HIGH);
    // RehldsHookchains->SV_EmitEvents()->registerHook(&SV_EmitEvents, HC_PRIORITY_HIGH);
    // RehldsHookchains->EV_PlayReliableEvent()->registerHook(&EV_PlayReliableEvent, HC_PRIORITY_HIGH);
    // RehldsHookchains->SV_StartSound()->registerHook(&SV_StartSound, HC_PRIORITY_HIGH);
    // RehldsHookchains->PF_Remove_I()->registerHook(&PF_Remove_I, HC_PRIORITY_HIGH);
    // RehldsHookchains->PF_BuildSoundMsg_I()->registerHook(&PF_BuildSoundMsg_I, HC_PRIORITY_HIGH);
    // RehldsHookchains->SV_WriteFullClientUpdate()->registerHook(&SV_WriteFullClientUpdate, HC_PRIORITY_HIGH);
    // RehldsHookchains->SV_CheckConsistencyResponse()->registerHook(&SV_CheckConsistencyResponse, HC_PRIORITY_HIGH);
    // RehldsHookchains->SV_DropClient()->registerHook(&SV_DropClient, HC_PRIORITY_HIGH);
    // RehldsHookchains->SV_ActivateServer()->registerHook(&SV_ActivateServer, HC_PRIORITY_HIGH);
    // RehldsHookchains->SV_WriteVoiceCodec()->registerHook(&SV_WriteVoiceCodec, HC_PRIORITY_HIGH);
    // RehldsHookchains->Steam_GSGetSteamID()->registerHook(&Steam_GSGetSteamID, HC_PRIORITY_HIGH);
    // RehldsHookchains->SV_TransferConsistencyInfo()->registerHook(&SV_TransferConsistencyInfo, HC_PRIORITY_HIGH);
    // RehldsHookchains->Steam_GSBUpdateUserData()->registerHook(&Steam_GSBUpdateUserData, HC_PRIORITY_HIGH);
    // RehldsHookchains->Cvar_DirectSet()->registerHook(&Cvar_DirectSet, HC_PRIORITY_HIGH);
    // RehldsHookchains->SV_EstablishTimeBase()->registerHook(&SV_EstablishTimeBase, HC_PRIORITY_HIGH);
    // RehldsHookchains->SV_Spawn_f()->registerHook(&SV_Spawn_f, HC_PRIORITY_HIGH);
    // RehldsHookchains->SV_CreatePacketEntities()->registerHook(&SV_CreatePacketEntities, HC_PRIORITY_HIGH);
    // RehldsHookchains->SV_EmitSound2()->registerHook(&SV_EmitSound2, HC_PRIORITY_HIGH);
    // RehldsHookchains->CreateFakeClient()->registerHook(&CreateFakeClient, HC_PRIORITY_HIGH);
    // RehldsHookchains->SV_CheckConnectionLessRateLimits()->registerHook(&SV_CheckConnectionLessRateLimits, HC_PRIORITY_HIGH);
    // RehldsHookchains->SV_Frame()->registerHook(&SV_Frame, HC_PRIORITY_HIGH);
    // RehldsHookchains->SV_ShouldSendConsistencyList()->registerHook(&SV_ShouldSendConsistencyList, HC_PRIORITY_HIGH);
    // RehldsHookchains->GetEntityInit()->registerHook(&GetEntityInit, HC_PRIORITY_HIGH);
    // RehldsHookchains->SV_EmitPings()->registerHook(&SV_EmitPings, HC_PRIORITY_HIGH);
    RehldsHookchains->ED_Alloc()->registerHook(&ED_Alloc, HC_PRIORITY_LOW);
    RehldsHookchains->ED_Free()->registerHook(&ED_Free, HC_PRIORITY_HIGH);
    // RehldsHookchains->Con_Printf()->registerHook(&Con_Printf, HC_PRIORITY_HIGH);
    // RehldsHookchains->SV_CheckUserInfo()->registerHook(&SV_CheckUserInfo, HC_PRIORITY_HIGH);
    // RehldsHookchains->PF_precache_generic_I()->registerHook(&PF_precache_generic_I, HC_PRIORITY_HIGH);
    // RehldsHookchains->PF_precache_model_I()->registerHook(&PF_precache_model_I, HC_PRIORITY_HIGH);
    // RehldsHookchains->PF_precache_sound_I()->registerHook(&PF_precache_sound_I, HC_PRIORITY_HIGH);
    // RehldsHookchains->EV_Precache()->registerHook(&EV_Precache, HC_PRIORITY_HIGH);
    // RehldsHookchains->SV_AddResource()->registerHook(&SV_AddResource, HC_PRIORITY_HIGH);
    // RehldsHookchains->SV_ClientPrintf()->registerHook(&SV_ClientPrintf, HC_PRIORITY_HIGH);
    // RehldsHookchains->SV_AllowPhysent()->registerHook(&SV_AllowPhysent, HC_PRIORITY_HIGH);
    // // RehldsHookchains->SV_SendResources()->registerHook(&SV_SendResources, HC_PRIORITY_HIGH);

    // ReGameHookchains->CBasePlayer_Spawn()->registerHook(&CBasePlayer_Spawn, HC_PRIORITY_HIGH);
    // ReGameHookchains->CBasePlayer_Precache()->registerHook(&CBasePlayer_Precache, HC_PRIORITY_HIGH);
    // ReGameHookchains->CBasePlayer_ObjectCaps()->registerHook(&CBasePlayer_ObjectCaps, HC_PRIORITY_HIGH);
    // ReGameHookchains->CBasePlayer_Classify()->registerHook(&CBasePlayer_Classify, HC_PRIORITY_HIGH);
    // ReGameHookchains->CBasePlayer_TraceAttack()->registerHook(&CBasePlayer_TraceAttack, HC_PRIORITY_HIGH);
    // ReGameHookchains->CBasePlayer_TakeDamage()->registerHook(&CBasePlayer_TakeDamage, HC_PRIORITY_HIGH);
    // ReGameHookchains->CBasePlayer_TakeHealth()->registerHook(&CBasePlayer_TakeHealth, HC_PRIORITY_HIGH);
    // ReGameHookchains->CBasePlayer_Killed()->registerHook(&CBasePlayer_Killed, HC_PRIORITY_HIGH);
    // ReGameHookchains->CBasePlayer_AddPoints()->registerHook(&CBasePlayer_AddPoints, HC_PRIORITY_HIGH);
    // ReGameHookchains->CBasePlayer_AddPointsToTeam()->registerHook(&CBasePlayer_AddPointsToTeam, HC_PRIORITY_HIGH);
    // ReGameHookchains->CBasePlayer_RoundRespawn()->registerHook(&CBasePlayer_RoundRespawn, HC_PRIORITY_HIGH);
    // ReGameHookchains->CBasePlayer_Blind()->registerHook(&CBasePlayer_Blind, HC_PRIORITY_HIGH);
    // ReGameHookchains->CBasePlayer_Observer_IsValidTarget()->registerHook(&CBasePlayer_Observer_IsValidTarget, HC_PRIORITY_HIGH);
    // ReGameHookchains->CBasePlayer_SetAnimation()->registerHook(&CBasePlayer_SetAnimation, HC_PRIORITY_HIGH);
    // ReGameHookchains->CBasePlayer_GiveDefaultItems()->registerHook(&CBasePlayer_GiveDefaultItems, HC_PRIORITY_HIGH);
    // ReGameHookchains->CBasePlayer_GiveNamedItem()->registerHook(&CBasePlayer_GiveNamedItem, HC_PRIORITY_HIGH);
    // ReGameHookchains->CBasePlayer_AddAccount()->registerHook(&CBasePlayer_AddAccount, HC_PRIORITY_HIGH);
    // ReGameHookchains->CBasePlayer_GiveShield()->registerHook(&CBasePlayer_GiveShield, HC_PRIORITY_HIGH);
    // ReGameHookchains->CBasePlayer_SetClientUserInfoModel()->registerHook(&CBasePlayer_SetClientUserInfoModel, HC_PRIORITY_HIGH);
    // ReGameHookchains->CBasePlayer_SetClientUserInfoName()->registerHook(&CBasePlayer_SetClientUserInfoName, HC_PRIORITY_HIGH);
    // ReGameHookchains->CBasePlayer_HasRestrictItem()->registerHook(&CBasePlayer_HasRestrictItem, HC_PRIORITY_HIGH);
    // ReGameHookchains->CBasePlayer_DropPlayerItem()->registerHook(&CBasePlayer_DropPlayerItem, HC_PRIORITY_HIGH);
    // ReGameHookchains->CBasePlayer_DropShield()->registerHook(&CBasePlayer_DropShield, HC_PRIORITY_HIGH);
    // ReGameHookchains->CBasePlayer_OnSpawnEquip()->registerHook(&CBasePlayer_OnSpawnEquip, HC_PRIORITY_HIGH);
    // ReGameHookchains->CBasePlayer_Radio()->registerHook(&CBasePlayer_Radio, HC_PRIORITY_HIGH);
    // ReGameHookchains->CBasePlayer_Disappear()->registerHook(&CBasePlayer_Disappear, HC_PRIORITY_HIGH);
    // ReGameHookchains->CBasePlayer_MakeVIP()->registerHook(&CBasePlayer_MakeVIP, HC_PRIORITY_HIGH);
    // ReGameHookchains->CBasePlayer_MakeBomber()->registerHook(&CBasePlayer_MakeBomber, HC_PRIORITY_HIGH);
    // ReGameHookchains->CBasePlayer_StartObserver()->registerHook(&CBasePlayer_StartObserver, HC_PRIORITY_HIGH);
    // ReGameHookchains->CBasePlayer_GetIntoGame()->registerHook(&CBasePlayer_GetIntoGame, HC_PRIORITY_HIGH);
    // ReGameHookchains->CBaseAnimating_ResetSequenceInfo()->registerHook(&CBaseAnimating_ResetSequenceInfo, HC_PRIORITY_HIGH);
    // ReGameHookchains->GetForceCamera()->registerHook(&GetForceCamera, HC_PRIORITY_HIGH);
    // ReGameHookchains->PlayerBlind()->registerHook(&PlayerBlind, HC_PRIORITY_HIGH);
    // ReGameHookchains->RadiusFlash_TraceLine()->registerHook(&RadiusFlash_TraceLine, HC_PRIORITY_HIGH);
    // ReGameHookchains->RoundEnd()->registerHook(&RoundEnd, HC_PRIORITY_HIGH);
    // ReGameHookchains->InstallGameRules()->registerHook(&InstallGameRules, HC_PRIORITY_HIGH);
    // ReGameHookchains->PM_Init()->registerHook(&PM_Init, HC_PRIORITY_HIGH);
    // ReGameHookchains->PM_Move()->registerHook(&PM_Move, HC_PRIORITY_HIGH);
    // ReGameHookchains->PM_AirMove()->registerHook(&PM_AirMove, HC_PRIORITY_HIGH);
    // ReGameHookchains->HandleMenu_ChooseAppearance()->registerHook(&HandleMenu_ChooseAppearance, HC_PRIORITY_HIGH);
    // ReGameHookchains->HandleMenu_ChooseTeam()->registerHook(&HandleMenu_ChooseTeam, HC_PRIORITY_HIGH);
    // ReGameHookchains->ShowMenu()->registerHook(&ShowMenu, HC_PRIORITY_HIGH);
    // ReGameHookchains->ShowVGUIMenu()->registerHook(&ShowVGUIMenu, HC_PRIORITY_HIGH);
    // ReGameHookchains->BuyGunAmmo()->registerHook(&BuyGunAmmo, HC_PRIORITY_HIGH);
    // ReGameHookchains->BuyWeaponByWeaponID()->registerHook(&BuyWeaponByWeaponID, HC_PRIORITY_HIGH);
    // ReGameHookchains->InternalCommand()->registerHook(&InternalCommand, HC_PRIORITY_HIGH);
    // ReGameHookchains->CSGameRules_FShouldSwitchWeapon()->registerHook(&CSGameRules_FShouldSwitchWeapon, HC_PRIORITY_HIGH);
    // ReGameHookchains->CSGameRules_GetNextBestWeapon()->registerHook(&CSGameRules_GetNextBestWeapon, HC_PRIORITY_HIGH);
    // ReGameHookchains->CSGameRules_FlPlayerFallDamage()->registerHook(&CSGameRules_FlPlayerFallDamage, HC_PRIORITY_HIGH);
    // ReGameHookchains->CSGameRules_FPlayerCanTakeDamage()->registerHook(&CSGameRules_FPlayerCanTakeDamage, HC_PRIORITY_HIGH);
    // ReGameHookchains->CSGameRules_PlayerSpawn()->registerHook(&CSGameRules_PlayerSpawn, HC_PRIORITY_HIGH);
    // ReGameHookchains->CSGameRules_FPlayerCanRespawn()->registerHook(&CSGameRules_FPlayerCanRespawn, HC_PRIORITY_HIGH);
    // ReGameHookchains->CSGameRules_GetPlayerSpawnSpot()->registerHook(&CSGameRules_GetPlayerSpawnSpot, HC_PRIORITY_HIGH);
    // ReGameHookchains->CSGameRules_ClientUserInfoChanged()->registerHook(&CSGameRules_ClientUserInfoChanged, HC_PRIORITY_HIGH);
    // ReGameHookchains->CSGameRules_PlayerKilled()->registerHook(&CSGameRules_PlayerKilled, HC_PRIORITY_HIGH);
    // ReGameHookchains->CSGameRules_DeathNotice()->registerHook(&CSGameRules_DeathNotice, HC_PRIORITY_HIGH);
    // ReGameHookchains->CSGameRules_CanHavePlayerItem()->registerHook(&CSGameRules_CanHavePlayerItem, HC_PRIORITY_HIGH);
    // ReGameHookchains->CSGameRules_DeadPlayerWeapons()->registerHook(&CSGameRules_DeadPlayerWeapons, HC_PRIORITY_HIGH);
    // ReGameHookchains->CSGameRules_ServerDeactivate()->registerHook(&CSGameRules_ServerDeactivate, HC_PRIORITY_HIGH);
    // ReGameHookchains->CSGameRules_CheckMapConditions()->registerHook(&CSGameRules_CheckMapConditions, HC_PRIORITY_HIGH);
    // ReGameHookchains->CSGameRules_CleanUpMap()->registerHook(&CSGameRules_CleanUpMap, HC_PRIORITY_HIGH);
    // ReGameHookchains->CSGameRules_RestartRound()->registerHook(&CSGameRules_RestartRound, HC_PRIORITY_HIGH);
    // ReGameHookchains->CSGameRules_CheckWinConditions()->registerHook(&CSGameRules_CheckWinConditions, HC_PRIORITY_HIGH);
    // ReGameHookchains->CSGameRules_RemoveGuns()->registerHook(&CSGameRules_RemoveGuns, HC_PRIORITY_HIGH);
    // ReGameHookchains->CSGameRules_GiveC4()->registerHook(&CSGameRules_GiveC4, HC_PRIORITY_HIGH);
    // ReGameHookchains->CSGameRules_ChangeLevel()->registerHook(&CSGameRules_ChangeLevel, HC_PRIORITY_HIGH);
    // ReGameHookchains->CSGameRules_GoToIntermission()->registerHook(&CSGameRules_GoToIntermission, HC_PRIORITY_HIGH);
    // ReGameHookchains->CSGameRules_BalanceTeams()->registerHook(&CSGameRules_BalanceTeams, HC_PRIORITY_HIGH);
    // ReGameHookchains->CSGameRules_OnRoundFreezeEnd()->registerHook(&CSGameRules_OnRoundFreezeEnd, HC_PRIORITY_HIGH);
    // ReGameHookchains->PM_UpdateStepSound()->registerHook(&PM_UpdateStepSound, HC_PRIORITY_HIGH);
    // ReGameHookchains->CBasePlayer_StartDeathCam()->registerHook(&CBasePlayer_StartDeathCam, HC_PRIORITY_HIGH);
    // ReGameHookchains->CBasePlayer_SwitchTeam()->registerHook(&CBasePlayer_SwitchTeam, HC_PRIORITY_HIGH);
    // ReGameHookchains->CBasePlayer_CanSwitchTeam()->registerHook(&CBasePlayer_CanSwitchTeam, HC_PRIORITY_HIGH);
    // ReGameHookchains->CBasePlayer_ThrowGrenade()->registerHook(&CBasePlayer_ThrowGrenade, HC_PRIORITY_HIGH);
    // ReGameHookchains->CSGameRules_CanPlayerHearPlayer()->registerHook(&CSGameRules_CanPlayerHearPlayer, HC_PRIORITY_HIGH);
    // ReGameHookchains->CWeaponBox_SetModel()->registerHook(&CWeaponBox_SetModel, HC_PRIORITY_HIGH);
    // ReGameHookchains->CGrenade_DefuseBombStart()->registerHook(&CGrenade_DefuseBombStart, HC_PRIORITY_HIGH);
    // ReGameHookchains->CGrenade_DefuseBombEnd()->registerHook(&CGrenade_DefuseBombEnd, HC_PRIORITY_HIGH);
    // ReGameHookchains->CGrenade_ExplodeHeGrenade()->registerHook(&CGrenade_ExplodeHeGrenade, HC_PRIORITY_HIGH);
    // ReGameHookchains->CGrenade_ExplodeFlashbang()->registerHook(&CGrenade_ExplodeFlashbang, HC_PRIORITY_HIGH);
    // ReGameHookchains->CGrenade_ExplodeSmokeGrenade()->registerHook(&CGrenade_ExplodeSmokeGrenade, HC_PRIORITY_HIGH);
    // ReGameHookchains->CGrenade_ExplodeBomb()->registerHook(&CGrenade_ExplodeBomb, HC_PRIORITY_HIGH);
    // ReGameHookchains->ThrowHeGrenade()->registerHook(&ThrowHeGrenade, HC_PRIORITY_HIGH);
    // ReGameHookchains->ThrowFlashbang()->registerHook(&ThrowFlashbang, HC_PRIORITY_HIGH);
    // ReGameHookchains->ThrowSmokeGrenade()->registerHook(&ThrowSmokeGrenade, HC_PRIORITY_HIGH);
    // ReGameHookchains->PlantBomb()->registerHook(&PlantBomb, HC_PRIORITY_HIGH);
    // ReGameHookchains->CBasePlayer_RemoveSpawnProtection()->registerHook(&CBasePlayer_RemoveSpawnProtection, HC_PRIORITY_HIGH);
    // ReGameHookchains->CBasePlayer_SetSpawnProtection()->registerHook(&CBasePlayer_SetSpawnProtection, HC_PRIORITY_HIGH);
    // ReGameHookchains->IsPenetrableEntity()->registerHook(&IsPenetrableEntity, HC_PRIORITY_HIGH);
    // MF_AddNatives(LuaNatives);
    // g_EntVarMap.clear();
    // // 初始化容量 (2的幂次方，比如 64, 128)
    // g_EntVarMap.init(128);

    // // --- Float ---
    // REG_VAR("impacttime", impacttime,TYPE_FLOAT);
	// REG_VAR("starttime", starttime,TYPE_FLOAT);
    // REG_VAR("idealpitch", idealpitch,TYPE_FLOAT);
	// REG_VAR("pitch_speed", pitch_speed,TYPE_FLOAT);
	// REG_VAR("ideal_yaw", ideal_yaw,TYPE_FLOAT);
	// REG_VAR("yaw_speed", yaw_speed,TYPE_FLOAT);
	// REG_VAR("ltime", ltime,TYPE_FLOAT);
	// REG_VAR("nextthink", nextthink,TYPE_FLOAT);
	// REG_VAR("gravity", gravity,TYPE_FLOAT);
	// REG_VAR("friction", friction,TYPE_FLOAT);
	// REG_VAR("frame", frame,TYPE_FLOAT);
	// REG_VAR("animtime", animtime,TYPE_FLOAT);
	// REG_VAR("framerate", framerate,TYPE_FLOAT);
	// REG_VAR("scale", scale,TYPE_FLOAT)	
	// REG_VAR("renderamt", renderamt,TYPE_FLOAT);
	// REG_VAR("health", health,TYPE_FLOAT);
	// REG_VAR("frags", frags,TYPE_FLOAT);
	// REG_VAR("takedamage", takedamage,TYPE_FLOAT);
	// REG_VAR("max_health", max_health,TYPE_FLOAT);
	// REG_VAR("teleport_time", teleport_time,TYPE_FLOAT);
	// REG_VAR("armortype", armortype,TYPE_FLOAT);
	// REG_VAR("armorvalue", armorvalue,TYPE_FLOAT);
	// REG_VAR("dmg_take", dmg_take,TYPE_FLOAT);
	// REG_VAR("dmg_save", dmg_save,TYPE_FLOAT);
	// REG_VAR("dmg", dmg,TYPE_FLOAT);
	// REG_VAR("dmgtime", dmgtime,TYPE_FLOAT);
	// REG_VAR("speed", speed,TYPE_FLOAT);
	// REG_VAR("air_finished", air_finished,TYPE_FLOAT);
	// REG_VAR("pain_finished", pain_finished,TYPE_FLOAT);
	// REG_VAR("radsuit_finished", radsuit_finished,TYPE_FLOAT);
	// REG_VAR("maxspeed", maxspeed,TYPE_FLOAT);
	// REG_VAR("fov", fov,TYPE_FLOAT);
	// REG_VAR("flFallVelocity", flFallVelocity,TYPE_FLOAT);
	// REG_VAR("fuser1", fuser1,TYPE_FLOAT);
	// REG_VAR("fuser2", fuser2,TYPE_FLOAT);
	// REG_VAR("fuser3", fuser3,TYPE_FLOAT);
	// REG_VAR("fuser4", fuser4,TYPE_FLOAT);

    // // --- String ---
    // REG_VAR("classname",classname,TYPE_STRING);
	// REG_VAR("globalname",globalname,TYPE_STRING);
	// REG_VAR("model",model,TYPE_STRING);
	// REG_VAR("target",target,TYPE_STRING);
	// REG_VAR("targetname",targetname,TYPE_STRING);
	// REG_VAR("netname",netname,TYPE_STRING);
	// REG_VAR("message",message,TYPE_STRING);
	// REG_VAR("noise",noise,TYPE_STRING);
	// REG_VAR("noise1",noise1,TYPE_STRING);
	// REG_VAR("noise2",noise2,TYPE_STRING);
	// REG_VAR("noise3",noise3,TYPE_STRING);


    // // --- Int ---
    // REG_VAR("fixangle" , fixangle,TYPE_INT);
    // REG_VAR("modelindex" , modelindex,TYPE_INT);
    // REG_VAR("viewmodel" , viewmodel,TYPE_INT);
    // REG_VAR("weaponmodel" , weaponmodel,TYPE_INT);
    // REG_VAR("movetype" , movetype,TYPE_INT);
    // REG_VAR("solid" , solid,TYPE_INT);
    // REG_VAR("skin" , skin,TYPE_INT);
    // REG_VAR("body" , body,TYPE_INT);
    // REG_VAR("effects" , effects,TYPE_INT);
    // REG_VAR("light_level" , light_level,TYPE_INT);
    // REG_VAR("sequence" , sequence,TYPE_INT);
    // REG_VAR("gaitsequence" , gaitsequence,TYPE_INT);
    // REG_VAR("rendermode" , rendermode,TYPE_INT);
    // REG_VAR("renderfx" , renderfx,TYPE_INT);
    // REG_VAR("weapons" , weapons,TYPE_INT);
    // REG_VAR("deadflag" , deadflag,TYPE_INT);
    // REG_VAR("button" , button,TYPE_INT);
    // REG_VAR("impulse" , impulse,TYPE_INT);
    // REG_VAR("spawnflags" , spawnflags,TYPE_INT);
    // REG_VAR("flags" , flags,TYPE_INT);
    // REG_VAR("colormap" , colormap,TYPE_INT);
    // REG_VAR("team" , team,TYPE_INT);
    // REG_VAR("waterlevel" , waterlevel,TYPE_INT);
    // REG_VAR("watertype" , watertype,TYPE_INT);
    // REG_VAR("playerclass" , playerclass,TYPE_INT);
    // REG_VAR("weaponanim" , weaponanim,TYPE_INT);
    // REG_VAR("pushmsec" , pushmsec,TYPE_INT);
    // REG_VAR("bInDuck" , bInDuck,TYPE_INT);
    // REG_VAR("flTimeStepSound" , flTimeStepSound,TYPE_INT);
    // REG_VAR("flSwimTime" , flSwimTime,TYPE_INT);
    // REG_VAR("flDuckTime" , flDuckTime,TYPE_INT);
    // REG_VAR("iStepLeft" , iStepLeft,TYPE_INT);
    // REG_VAR("gamestate" , gamestate,TYPE_INT);
    // REG_VAR("oldbuttons" , oldbuttons,TYPE_INT);
    // REG_VAR("groupinfo" , groupinfo,TYPE_INT);
    // REG_VAR("iuser1" , iuser1,TYPE_INT);
    // REG_VAR("iuser2" , iuser2,TYPE_INT);
    // REG_VAR("iuser3" , iuser3,TYPE_INT);
    // REG_VAR("iuser4" , iuser4,TYPE_INT);

    // // --- Vector ---
    // REG_VAR("origin" , origin,TYPE_VECTOR);
	// REG_VAR("oldorigin" , oldorigin,TYPE_VECTOR);
	// REG_VAR("velocity" , velocity,TYPE_VECTOR);
	// REG_VAR("basevelocity" , basevelocity,TYPE_VECTOR);
	// REG_VAR("clbasevelocity" , clbasevelocity,TYPE_VECTOR);
	// REG_VAR("movedir" , movedir,TYPE_VECTOR);
	// REG_VAR("angles" , angles,TYPE_VECTOR);
	// REG_VAR("avelocity" , avelocity,TYPE_VECTOR);
	// REG_VAR("punchangle" , punchangle,TYPE_VECTOR);
	// REG_VAR("v_angle" , v_angle,TYPE_VECTOR);
	// REG_VAR("endpos" , endpos,TYPE_VECTOR);
	// REG_VAR("startpos" , startpos,TYPE_VECTOR);
	// REG_VAR("absmin" , absmin,TYPE_VECTOR);
	// REG_VAR("absmax" , absmax,TYPE_VECTOR);
	// REG_VAR("mins" , mins,TYPE_VECTOR);
	// REG_VAR("maxs" , maxs,TYPE_VECTOR);
	// REG_VAR("size" , size,TYPE_VECTOR);
	// REG_VAR("rendercolor" , rendercolor,TYPE_VECTOR);
	// REG_VAR("view_ofs" , view_ofs,TYPE_VECTOR);
	// REG_VAR("vuser1" , vuser1,TYPE_VECTOR);
	// REG_VAR("vuser2" , vuser2,TYPE_VECTOR);
	// REG_VAR("vuser3" , vuser3,TYPE_VECTOR);
	// REG_VAR("vuser4" , vuser4,TYPE_VECTOR);

    // // --- Edict ---
    // REG_VAR("owner",      owner,      TYPE_EDICT);
    // REG_VAR("aiment",     aiment,     TYPE_EDICT);

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
    OnPluginsLoaded2();
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
//----------------------------------
// - GetEntityAPI2 functions
//----------------------------------
//----------------------------------
//----------------------------------
//----------------------------------
//----------------------------------
//----------------------------------
//----------------------------------
//----------------------------------
//----------------------------------
//----------------------------------
// /* pfnGameInit() */
// void GameDLLInit(void)
// {
//     if (!g_L)
//         RETURN_META(MRES_IGNORED);

//     lua_getglobal(g_L, "MetaGameDLLInit");

//     if (!lua_isfunction(g_L, -1))
//     {
//         lua_pop(g_L, 1);
//         RETURN_META(MRES_IGNORED);
//     }

//     if (lua_pcall(g_L, 0, 0, 0) != 0)
//     {
//         lua_pop(g_L, 1);
//     }

//     RETURN_META(MRES_IGNORED);
// }
// /* pfnSpawn() */
// int DispatchSpawn(edict_t *pent)
// {
//     if (!g_L)
//         RETURN_META_VALUE(MRES_IGNORED, -1);

//     lua_getglobal(g_L, "MetaDispatchSpawn");

//     if (!lua_isfunction(g_L, -1))
//     {
//         lua_pop(g_L, 1);
//         RETURN_META_VALUE(MRES_IGNORED, -1);
//     }

//     // Push: edict_t* (pointer)
//     lua_pushentity(g_L, pent);

//     if (lua_pcall(g_L, 1, 0, 0) != 0)
//     {
//         lua_pop(g_L, 1);
//     }

//     RETURN_META_VALUE(MRES_IGNORED, 0);
// }

// /* pfnThink() */
// void DispatchThink(edict_t *pent)
// {
//     if (!g_L)
//         RETURN_META(MRES_IGNORED);

//     lua_getglobal(g_L, "MetaDispatchThink");

//     if (!lua_isfunction(g_L, -1))
//     {
//         lua_pop(g_L, 1);
//         RETURN_META(MRES_IGNORED);
//     }

//     // Push: edict_t* (pointer)
//     lua_pushentity(g_L, pent);

//     if (lua_pcall(g_L, 1, 0, 0) != 0)
//     {
//         lua_pop(g_L, 1);
//     }

//     RETURN_META(MRES_IGNORED);
// }

// /* pfnUse() */
// void DispatchUse(edict_t *pentUsed, edict_t *pentOther)
// {
//     if (!g_L)
//         RETURN_META(MRES_IGNORED);

//     lua_getglobal(g_L, "MetaDispatchUse");

//     if (!lua_isfunction(g_L, -1))
//     {
//         lua_pop(g_L, 1);
//         RETURN_META(MRES_IGNORED);
//     }

//     // Push: edict_t* (Used), edict_t* (Other)
//     lua_pushentity(g_L, pentUsed);
//     lua_pushentity(g_L, pentOther);

//     if (lua_pcall(g_L, 2, 0, 0) != 0)
//     {
//         lua_pop(g_L, 1);
//     }

//     RETURN_META(MRES_IGNORED);
// }

// /* pfnTouch() */
// void DispatchTouch(edict_t *pentTouched, edict_t *pentOther)
// {
//     if (!g_L)
//         RETURN_META(MRES_IGNORED);

//     lua_getglobal(g_L, "MetaDispatchTouch");

//     if (!lua_isfunction(g_L, -1))
//     {
//         lua_pop(g_L, 1);
//         RETURN_META(MRES_IGNORED);
//     }

//     // Push: edict_t* (Touched), edict_t* (Other)
//     lua_pushentity(g_L, pentTouched);
//     lua_pushentity(g_L, pentOther);

//     if (lua_pcall(g_L, 2, 0, 0) != 0)
//     {
//         lua_pop(g_L, 1);
//     }

//     RETURN_META(MRES_IGNORED);
// }

// /* pfnBlocked() */
// void DispatchBlocked(edict_t *pentBlocked, edict_t *pentOther)
// {
//     if (!g_L)
//         RETURN_META(MRES_IGNORED);

//     lua_getglobal(g_L, "MetaDispatchBlocked");

//     if (!lua_isfunction(g_L, -1))
//     {
//         lua_pop(g_L, 1);
//         RETURN_META(MRES_IGNORED);
//     }

//     // Push: edict_t* (Blocked), edict_t* (Other)
//     lua_pushentity(g_L, pentBlocked);
//     lua_pushentity(g_L, pentOther);

//     if (lua_pcall(g_L, 2, 0, 0) != 0)
//     {
//         lua_pop(g_L, 1);
//     }

//     RETURN_META(MRES_IGNORED);
// }

// /* pfnKeyValue() */
// void DispatchKeyValue(edict_t *pentKeyvalue, KeyValueData *pkvd)
// {
//     if (!g_L)
//         RETURN_META(MRES_IGNORED);

//     lua_getglobal(g_L, "MetaDispatchKeyValue");

//     if (!lua_isfunction(g_L, -1))
//     {
//         lua_pop(g_L, 1);
//         RETURN_META(MRES_IGNORED);
//     }

//     // Push: edict_t* (Entity)
//     lua_pushentity(g_L, pentKeyvalue);
    
//     // Push: Key (string), Value (string), ClassName (string)
//     // 这样在 Lua 里可以直接读取配置，而不是拿到一个空指针
//     if (pkvd) {
//         lua_pushstring(g_L, pkvd->szKeyName);
//         lua_pushstring(g_L, pkvd->szValue);
//         lua_pushstring(g_L, pkvd->szClassName);
//     } else {
//         lua_pushnil(g_L);
//         lua_pushnil(g_L);
//         lua_pushnil(g_L);
//     }

//     if (lua_pcall(g_L, 4, 0, 0) != 0)
//     {
//         lua_pop(g_L, 1);
//     }

//     RETURN_META(MRES_IGNORED);
// }

// /* pfnSave() */
// void DispatchSave(edict_t *pent, SAVERESTOREDATA *pSaveData)
// {
//     if (!g_L)
//         RETURN_META(MRES_IGNORED);

//     lua_getglobal(g_L, "MetaDispatchSave");

//     if (!lua_isfunction(g_L, -1))
//     {
//         lua_pop(g_L, 1);
//         RETURN_META(MRES_IGNORED);
//     }

//     // Push: edict_t* (Entity), SAVERESTOREDATA* (pointer)
//     lua_pushentity(g_L, pent);
//     lua_pushlightuserdata(g_L, pSaveData);

//     if (lua_pcall(g_L, 2, 0, 0) != 0)
//     {
//         lua_pop(g_L, 1);
//     }

//     RETURN_META(MRES_IGNORED);
// }

// /* pfnRestore() */
// int DispatchRestore(edict_t *pent, SAVERESTOREDATA *pSaveData, int globalEntity)
// {
//     if (!g_L)
//         RETURN_META_VALUE(MRES_IGNORED, 0);

//     lua_getglobal(g_L, "MetaDispatchRestore");

//     if (!lua_isfunction(g_L, -1))
//     {
//         lua_pop(g_L, 1);
//         RETURN_META_VALUE(MRES_IGNORED, 0);
//     }

//     // Push: edict_t* (Entity), SAVERESTOREDATA* (pointer), globalEntity (int)
//     lua_pushentity(g_L, pent);
//     lua_pushlightuserdata(g_L, pSaveData);
//     lua_pushinteger(g_L, globalEntity);

//     if (lua_pcall(g_L, 3, 0, 0) != 0)
//     {
//         lua_pop(g_L, 1);
//     }

//     RETURN_META_VALUE(MRES_IGNORED, 0);
// }

// /* pfnSetAbsBox() */
// void DispatchObjectCollsionBox(edict_t *pent)
// {
//     if (!g_L)
//         RETURN_META(MRES_IGNORED);

//     lua_getglobal(g_L, "MetaDispatchObjectCollsionBox");

//     if (!lua_isfunction(g_L, -1))
//     {
//         lua_pop(g_L, 1);
//         RETURN_META(MRES_IGNORED);
//     }

//     // Push: edict_t* (pointer)
//     lua_pushentity(g_L, pent);

//     if (lua_pcall(g_L, 1, 0, 0) != 0)
//     {
//         lua_pop(g_L, 1);
//     }

//     RETURN_META(MRES_IGNORED);
// }

// /* pfnSaveWriteFields() */
// void SaveWriteFields(SAVERESTOREDATA *pSaveData, const char *pname, void *pBaseData, TYPEDESCRIPTION *pFields, int fieldCount)
// {
//     if (!g_L)
//         RETURN_META(MRES_IGNORED);

//     lua_getglobal(g_L, "MetaSaveWriteFields");

//     if (!lua_isfunction(g_L, -1))
//     {
//         lua_pop(g_L, 1);
//         RETURN_META(MRES_IGNORED);
//     }

//     // Push: SAVERESTOREDATA* (ptr)
//     lua_pushlightuserdata(g_L, pSaveData);
//     // Push: Name (string)
//     lua_pushstring(g_L, pname);
//     // Push: BaseData* (ptr)
//     lua_pushlightuserdata(g_L, pBaseData);
//     // Push: TYPEDESCRIPTION* (ptr)
//     lua_pushlightuserdata(g_L, pFields);
//     // Push: fieldCount (int)
//     lua_pushinteger(g_L, fieldCount);

//     if (lua_pcall(g_L, 5, 0, 0) != 0)
//     {
//         lua_pop(g_L, 1);
//     }

//     RETURN_META(MRES_IGNORED);
// }

// /* pfnSaveReadFields() */
// void SaveReadFields(SAVERESTOREDATA *pSaveData, const char *pname, void *pBaseData, TYPEDESCRIPTION *pFields, int fieldCount)
// {
//     if (!g_L)
//         RETURN_META(MRES_IGNORED);

//     lua_getglobal(g_L, "MetaSaveReadFields");

//     if (!lua_isfunction(g_L, -1))
//     {
//         lua_pop(g_L, 1);
//         RETURN_META(MRES_IGNORED);
//     }

//     // Push: SAVERESTOREDATA* (ptr)
//     lua_pushlightuserdata(g_L, pSaveData);
//     // Push: Name (string)
//     lua_pushstring(g_L, pname);
//     // Push: BaseData* (ptr)
//     lua_pushlightuserdata(g_L, pBaseData);
//     // Push: TYPEDESCRIPTION* (ptr)
//     lua_pushlightuserdata(g_L, pFields);
//     // Push: fieldCount (int)
//     lua_pushinteger(g_L, fieldCount);

//     if (lua_pcall(g_L, 5, 0, 0) != 0)
//     {
//         lua_pop(g_L, 1);
//     }

//     RETURN_META(MRES_IGNORED);
// }

// /* pfnSaveGlobalState() */
// void SaveGlobalState(SAVERESTOREDATA *pSaveData)
// {
//     if (!g_L)
//         RETURN_META(MRES_IGNORED);

//     lua_getglobal(g_L, "MetaSaveGlobalState");

//     if (!lua_isfunction(g_L, -1))
//     {
//         lua_pop(g_L, 1);
//         RETURN_META(MRES_IGNORED);
//     }

//     // Push: SAVERESTOREDATA* (ptr)
//     lua_pushlightuserdata(g_L, pSaveData);

//     if (lua_pcall(g_L, 1, 0, 0) != 0)
//     {
//         lua_pop(g_L, 1);
//     }

//     RETURN_META(MRES_IGNORED);
// }

// /* pfnRestoreGlobalState() */
// void RestoreGlobalState(SAVERESTOREDATA *pSaveData)
// {
//     if (!g_L)
//         RETURN_META(MRES_IGNORED);

//     lua_getglobal(g_L, "MetaRestoreGlobalState");

//     if (!lua_isfunction(g_L, -1))
//     {
//         lua_pop(g_L, 1);
//         RETURN_META(MRES_IGNORED);
//     }

//     // Push: SAVERESTOREDATA* (ptr)
//     lua_pushlightuserdata(g_L, pSaveData);

//     if (lua_pcall(g_L, 1, 0, 0) != 0)
//     {
//         lua_pop(g_L, 1);
//     }

//     RETURN_META(MRES_IGNORED);
// }

// /* pfnResetGlobalState() */
// void ResetGlobalState(void)
// {
//     if (!g_L)
//         RETURN_META(MRES_IGNORED);

//     lua_getglobal(g_L, "MetaResetGlobalState");

//     if (!lua_isfunction(g_L, -1))
//     {
//         lua_pop(g_L, 1);
//         RETURN_META(MRES_IGNORED);
//     }

//     if (lua_pcall(g_L, 0, 0, 0) != 0)
//     {
//         lua_pop(g_L, 1);
//     }

//     RETURN_META(MRES_IGNORED);
// }

// /* pfnClientConnect() */
// qboolean ClientConnect(edict_t *pEntity, const char *pszName, const char *pszAddress, char szRejectReason[128])
// {
//     if (!g_L)
//         RETURN_META_VALUE(MRES_IGNORED, 0); // 0 = FALSE, 但 META 会忽略

//     lua_getglobal(g_L, "MetaClientConnect");

//     if (!lua_isfunction(g_L, -1))
//     {
//         lua_pop(g_L, 1);
//         RETURN_META_VALUE(MRES_IGNORED, 0);
//     }

//     // Push: edict_t* (玩家实体)
//     lua_pushentity(g_L, pEntity);
//     // Push: Name (名字)
//     lua_pushstring(g_L, pszName);
//     // Push: IP (IP地址)
//     lua_pushstring(g_L, pszAddress);
    
//     // 调用 Lua: 3个参数, 1个返回值
//     if (lua_pcall(g_L, 3, 1, 0) != 0)
//     {
//         lua_pop(g_L, 1);
//         RETURN_META_VALUE(MRES_IGNORED, 0);
//     }

//     // 处理返回值
//     // 如果 Lua 返回了一个字符串，说明要拒绝连接，并且这个字符串是拒绝理由
//     if (lua_isstring(g_L, -1))
//     {
//         const char* reason = lua_tostring(g_L, -1);
        
//         // 把 Lua 的拒绝理由复制回 C++ 的 buffer
//         // 注意防止溢出 (128字节)
//         strncpy(szRejectReason, reason, 127);
//         szRejectReason[127] = '\0'; // 确保结尾

//         lua_pop(g_L, 1); // 弹出返回值

//         // 返回 FALSE 告诉引擎拒绝连接，并使用 MRES_SUPERCEDE 覆盖引擎原本的逻辑
//         RETURN_META_VALUE(MRES_SUPERCEDE, 0); 
//     }

//     // 如果没返回字符串（比如返回 nil 或 true），则允许连接
//     lua_pop(g_L, 1);
//     RETURN_META_VALUE(MRES_IGNORED, 0);
// }

// /* pfnClientDisconnect() */
// void ClientDisconnect(edict_t *pEntity)
// {
//     if (!g_L)
//         RETURN_META(MRES_IGNORED);

//     lua_getglobal(g_L, "MetaClientDisconnect");

//     if (!lua_isfunction(g_L, -1))
//     {
//         lua_pop(g_L, 1);
//         RETURN_META(MRES_IGNORED);
//     }

//     // Push: edict_t* (Player Entity)
//     lua_pushentity(g_L, pEntity);

//     if (lua_pcall(g_L, 1, 0, 0) != 0)
//     {
//         lua_pop(g_L, 1);
//     }

//     RETURN_META(MRES_IGNORED);
// }

// /* pfnClientKill() */
// void ClientKill(edict_t *pEntity)
// {
//     if (!g_L)
//         RETURN_META(MRES_IGNORED);

//     lua_getglobal(g_L, "MetaClientKill");

//     if (!lua_isfunction(g_L, -1))
//     {
//         lua_pop(g_L, 1);
//         RETURN_META(MRES_IGNORED);
//     }

//     // Push: edict_t* (Player Entity)
//     lua_pushentity(g_L, pEntity);

//     if (lua_pcall(g_L, 1, 0, 0) != 0)
//     {
//         lua_pop(g_L, 1);
//     }

//     RETURN_META(MRES_IGNORED);
// }

// /* pfnClientPutInServer() */
// void ClientPutInServer(edict_t *pEntity)
// {
//     if (!g_L)
//         RETURN_META(MRES_IGNORED);

//     lua_getglobal(g_L, "MetaClientPutInServer");

//     if (!lua_isfunction(g_L, -1))
//     {
//         lua_pop(g_L, 1);
//         RETURN_META(MRES_IGNORED);
//     }

//     // Push: edict_t* (Player Entity)
//     lua_pushentity(g_L, pEntity);

//     if (lua_pcall(g_L, 1, 0, 0) != 0)
//     {
//         lua_pop(g_L, 1);
//     }

//     RETURN_META(MRES_IGNORED);
// }

// /* pfnClientCommand() */
// void ClientCommand(edict_t *pEntity)
// {
//     if (!g_L)
//         RETURN_META(MRES_IGNORED);

//     lua_getglobal(g_L, "MetaClientCommand");

//     if (!lua_isfunction(g_L, -1))
//     {
//         lua_pop(g_L, 1);
//         RETURN_META(MRES_IGNORED);
//     }

//     // Push: edict_t* (Player Entity)
//     lua_pushentity(g_L, pEntity);
//     // 注意: 命令的具体内容在 Lua 中需要通过调用 engine.Cmd_Args() 等函数获取

//     if (lua_pcall(g_L, 1, 0, 0) != 0)
//     {
//         lua_pop(g_L, 1);
//     }

//     RETURN_META(MRES_IGNORED);
// }

// /* pfnClientUserInfoChanged() */
// void ClientUserInfoChanged(edict_t *pEntity, char *infobuffer)
// {
//     if (!g_L)
//         RETURN_META(MRES_IGNORED);

//     lua_getglobal(g_L, "MetaClientUserInfoChanged");

//     if (!lua_isfunction(g_L, -1))
//     {
//         lua_pop(g_L, 1);
//         RETURN_META(MRES_IGNORED);
//     }

//     // Push: edict_t* (Player Entity)
//     lua_pushentity(g_L, pEntity);
//     // Push: infobuffer string (因为只是读取，复制一份给Lua没问题)
//     lua_pushstring(g_L, infobuffer);

//     if (lua_pcall(g_L, 2, 0, 0) != 0)
//     {
//         lua_pop(g_L, 1);
//     }

//     RETURN_META(MRES_IGNORED);
// }

// /* pfnServerActivate() */
// void ServerActivate(edict_t *pEdictList, int edictCount, int clientMax)
// {
//     if (!g_L)
//         RETURN_META(MRES_IGNORED);

//     // 可以在这里做一些 Map 初始化相关的工作，比如清空之前的缓存
//     // g_EntVarMap.clear(); // 如果需要在换图时重置，看具体需求

//     lua_getglobal(g_L, "MetaServerActivate");

//     if (!lua_isfunction(g_L, -1))
//     {
//         lua_pop(g_L, 1);
//         RETURN_META(MRES_IGNORED);
//     }

//     // Push: pEdictList (指针)
//     lua_pushentity(g_L, pEdictList);
//     // Push: edictCount (int)
//     lua_pushinteger(g_L, edictCount);
//     // Push: clientMax (int)
//     lua_pushinteger(g_L, clientMax);

//     if (lua_pcall(g_L, 3, 0, 0) != 0)
//     {
//         lua_pop(g_L, 1);
//     }

//     RETURN_META(MRES_IGNORED);
// }

// /* pfnServerDeactivate() */
// void ServerDeactivate(void)
// {
//     if (!g_L)
//         RETURN_META(MRES_IGNORED);

//     lua_getglobal(g_L, "MetaServerDeactivate");

//     if (!lua_isfunction(g_L, -1))
//     {
//         lua_pop(g_L, 1);
//         RETURN_META(MRES_IGNORED);
//     }

//     if (lua_pcall(g_L, 0, 0, 0) != 0)
//     {
//         lua_pop(g_L, 1);
//     }

//     RETURN_META(MRES_IGNORED);
// }

// /* pfnPlayerPreThink() */
// void PlayerPreThink(edict_t *pEntity)
// {
//     if (!g_L)
//         RETURN_META(MRES_IGNORED);

//     lua_getglobal(g_L, "MetaPlayerPreThink");

//     if (!lua_isfunction(g_L, -1))
//     {
//         lua_pop(g_L, 1);
//         RETURN_META(MRES_IGNORED);
//     }

//     // Push: edict_t* (Player Entity)
//     lua_pushentity(g_L, pEntity);

//     if (lua_pcall(g_L, 1, 0, 0) != 0)
//     {
//         lua_pop(g_L, 1);
//     }

//     RETURN_META(MRES_IGNORED);
// }

// /* pfnPlayerPostThink() */
// void PlayerPostThink(edict_t *pEntity)
// {
//     if (!g_L)
//         RETURN_META(MRES_IGNORED);

//     lua_getglobal(g_L, "MetaPlayerPostThink");

//     if (!lua_isfunction(g_L, -1))
//     {
//         lua_pop(g_L, 1);
//         RETURN_META(MRES_IGNORED);
//     }

//     // Push: edict_t* (Player Entity)
//     lua_pushentity(g_L, pEntity);

//     if (lua_pcall(g_L, 1, 0, 0) != 0)
//     {
//         lua_pop(g_L, 1);
//     }

//     RETURN_META(MRES_IGNORED);
// }

/* pfnStartFrame() */
void StartFrame(void)
{
    if (!g_L)
        RETURN_META(MRES_IGNORED);
    
    StartFrame2();

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
// /* pfnParmsNewLevel() */
// void ParmsNewLevel(void)
// {
//     if (!g_L)
//         RETURN_META(MRES_IGNORED);

//     lua_getglobal(g_L, "MetaParmsNewLevel");

//     if (!lua_isfunction(g_L, -1))
//     {
//         lua_pop(g_L, 1);
//         RETURN_META(MRES_IGNORED);
//     }

//     if (lua_pcall(g_L, 0, 0, 0) != 0)
//     {
//         lua_pop(g_L, 1);
//     }

//     RETURN_META(MRES_IGNORED);
// }

// /* pfnParmsChangeLevel() */
// void ParmsChangeLevel(void)
// {
//     if (!g_L)
//         RETURN_META(MRES_IGNORED);

//     lua_getglobal(g_L, "MetaParmsChangeLevel");

//     if (!lua_isfunction(g_L, -1))
//     {
//         lua_pop(g_L, 1);
//         RETURN_META(MRES_IGNORED);
//     }

//     if (lua_pcall(g_L, 0, 0, 0) != 0)
//     {
//         lua_pop(g_L, 1);
//     }

//     RETURN_META(MRES_IGNORED);
// }

// /* pfnGetGameDescription() */
// // 注意：这个函数返回 const char*，用于显示在服务器浏览器的 "Game" 列
// const char *GetGameDescription(void)
// {
//     if (!g_L)
//         RETURN_META_VALUE(MRES_IGNORED, NULL);

//     lua_getglobal(g_L, "MetaGetGameDescription");

//     if (!lua_isfunction(g_L, -1))
//     {
//         lua_pop(g_L, 1);
//         RETURN_META_VALUE(MRES_IGNORED, NULL);
//     }

//     // 调用 Lua
//     if (lua_pcall(g_L, 0, 1, 0) != 0)
//     {
//         lua_pop(g_L, 1);
//         RETURN_META_VALUE(MRES_IGNORED, NULL);
//     }

//     // 处理返回值
//     if (lua_isstring(g_L, -1))
//     {
//         // 我们必须使用静态缓冲区，因为一旦 lua_pop，指针可能失效
//         static char staticGameDesc[64];
//         const char *ret = lua_tostring(g_L, -1);
        
//         strncpy(staticGameDesc, ret, 63);
//         staticGameDesc[63] = '\0';

//         lua_pop(g_L, 1);
//         // 返回 SUPERCEDE 并带上我们的新名字
//         RETURN_META_VALUE(MRES_SUPERCEDE, staticGameDesc);
//     }

//     lua_pop(g_L, 1);
//     RETURN_META_VALUE(MRES_IGNORED, NULL);
// }

// /* pfnPlayerCustomization() */
// void PlayerCustomization(edict_t *pEntity, customization_t *pCustom)
// {
//     if (!g_L)
//         RETURN_META(MRES_IGNORED);

//     lua_getglobal(g_L, "MetaPlayerCustomization");

//     if (!lua_isfunction(g_L, -1))
//     {
//         lua_pop(g_L, 1);
//         RETURN_META(MRES_IGNORED);
//     }

//     lua_pushentity(g_L, pEntity);
//     lua_pushlightuserdata(g_L, pCustom);

//     if (lua_pcall(g_L, 2, 0, 0) != 0)
//     {
//         lua_pop(g_L, 1);
//     }

//     RETURN_META(MRES_IGNORED);
// }

// /* pfnSpectatorConnect() */
// void SpectatorConnect(edict_t *pEntity)
// {
//     if (!g_L)
//         RETURN_META(MRES_IGNORED);

//     lua_getglobal(g_L, "MetaSpectatorConnect");

//     if (!lua_isfunction(g_L, -1))
//     {
//         lua_pop(g_L, 1);
//         RETURN_META(MRES_IGNORED);
//     }

//     lua_pushentity(g_L, pEntity);

//     if (lua_pcall(g_L, 1, 0, 0) != 0)
//     {
//         lua_pop(g_L, 1);
//     }

//     RETURN_META(MRES_IGNORED);
// }

// /* pfnSpectatorDisconnect() */
// void SpectatorDisconnect(edict_t *pEntity)
// {
//     if (!g_L)
//         RETURN_META(MRES_IGNORED);

//     lua_getglobal(g_L, "MetaSpectatorDisconnect");

//     if (!lua_isfunction(g_L, -1))
//     {
//         lua_pop(g_L, 1);
//         RETURN_META(MRES_IGNORED);
//     }

//     lua_pushentity(g_L, pEntity);

//     if (lua_pcall(g_L, 1, 0, 0) != 0)
//     {
//         lua_pop(g_L, 1);
//     }

//     RETURN_META(MRES_IGNORED);
// }

// /* pfnSpectatorThink() */
// void SpectatorThink(edict_t *pEntity)
// {
//     if (!g_L)
//         RETURN_META(MRES_IGNORED);

//     lua_getglobal(g_L, "MetaSpectatorThink");

//     if (!lua_isfunction(g_L, -1))
//     {
//         lua_pop(g_L, 1);
//         RETURN_META(MRES_IGNORED);
//     }

//     lua_pushentity(g_L, pEntity);

//     if (lua_pcall(g_L, 1, 0, 0) != 0)
//     {
//         lua_pop(g_L, 1);
//     }

//     RETURN_META(MRES_IGNORED);
// }

// /* pfnSys_Error() */
// void Sys_Error(const char *error_string)
// {
//     // 这个函数调用意味着服务器即将崩溃或强制关闭
//     if (!g_L)
//         RETURN_META(MRES_IGNORED);

//     lua_getglobal(g_L, "MetaSys_Error");

//     if (!lua_isfunction(g_L, -1))
//     {
//         lua_pop(g_L, 1);
//         RETURN_META(MRES_IGNORED);
//     }

//     lua_pushstring(g_L, error_string);

//     if (lua_pcall(g_L, 1, 0, 0) != 0)
//     {
//         lua_pop(g_L, 1);
//     }

//     RETURN_META(MRES_IGNORED);
// }

// /* pfnPM_Move() */
// void PM_Move(struct playermove_s *ppmove, qboolean server)
// {
//     if (!g_L)
//         RETURN_META(MRES_IGNORED);

//     lua_getglobal(g_L, "MetaPM_Move");

//     if (!lua_isfunction(g_L, -1))
//     {
//         lua_pop(g_L, 1);
//         RETURN_META(MRES_IGNORED);
//     }

//     lua_pushlightuserdata(g_L, ppmove);
//     lua_pushboolean(g_L, server); // qboolean 本质是 int，但在 Lua 里用 bool 更直观

//     if (lua_pcall(g_L, 2, 0, 0) != 0)
//     {
//         lua_pop(g_L, 1);
//     }

//     RETURN_META(MRES_IGNORED);
// }

// /* pfnPM_Init() */
// void PM_Init(struct playermove_s *ppmove)
// {
//     if (!g_L)
//         RETURN_META(MRES_IGNORED);

//     lua_getglobal(g_L, "MetaPM_Init");

//     if (!lua_isfunction(g_L, -1))
//     {
//         lua_pop(g_L, 1);
//         RETURN_META(MRES_IGNORED);
//     }

//     lua_pushlightuserdata(g_L, ppmove);

//     if (lua_pcall(g_L, 1, 0, 0) != 0)
//     {
//         lua_pop(g_L, 1);
//     }

//     RETURN_META(MRES_IGNORED);
// }

// /* pfnSetupVisibility() */
// void SetupVisibility(struct edict_s *pViewEntity, struct edict_s *pClient, unsigned char **pvs, unsigned char **pas)
// {
//     if (!g_L) RETURN_META(MRES_IGNORED);

//     lua_getglobal(g_L, "MetaSetupVisibility");
//     if (!lua_isfunction(g_L, -1)) {
//         lua_pop(g_L, 1);
//         RETURN_META(MRES_IGNORED);
//     }

//     // Args: ViewEntity, Client, pvs ptr, pas ptr
//     lua_pushentity(g_L, pViewEntity);
//     lua_pushentity(g_L, pClient);
//     lua_pushlightuserdata(g_L, pvs);
//     lua_pushlightuserdata(g_L, pas);

//     if (lua_pcall(g_L, 4, 0, 0) != 0) {
//         lua_pop(g_L, 1);
//     }
//     RETURN_META(MRES_IGNORED);
// }

// /* pfnUpdateClientData() */
// void UpdateClientData(const struct edict_s *ent, int sendweapons, struct clientdata_s *cd)
// {
//     if (!g_L) RETURN_META(MRES_IGNORED);

//     lua_getglobal(g_L, "MetaUpdateClientData");
//     if (!lua_isfunction(g_L, -1)) {
//         lua_pop(g_L, 1);
//         RETURN_META(MRES_IGNORED);
//     }

//     // Args: Entity, SendWeapons(int), ClientData ptr
//     lua_pushlightuserdata(g_L, (void*)ent);
//     lua_pushinteger(g_L, sendweapons);
//     lua_pushlightuserdata(g_L, cd);

//     if (lua_pcall(g_L, 3, 0, 0) != 0) {
//         lua_pop(g_L, 1);
//     }
//     RETURN_META(MRES_IGNORED);
// }

// /* pfnAddToFullPack() - 警告：高频调用 */
// int AddToFullPack(struct entity_state_s *state, int e, edict_t *ent, edict_t *host, int hostflags, int player, unsigned char *pSet)
// {
//     if (!g_L) RETURN_META_VALUE(MRES_IGNORED, 0);

//     lua_getglobal(g_L, "MetaAddToFullPack");
//     if (!lua_isfunction(g_L, -1)) {
//         lua_pop(g_L, 1);
//         RETURN_META_VALUE(MRES_IGNORED, 0);
//     }

//     // Args: state*, e_index, ent*, host*, hostflags, player(bool/int), pSet*
//     lua_pushlightuserdata(g_L, state);
//     lua_pushinteger(g_L, e);
//     lua_pushentity(g_L, ent);
//     lua_pushentity(g_L, host);
//     lua_pushinteger(g_L, hostflags);
//     lua_pushinteger(g_L, player);
//     lua_pushlightuserdata(g_L, pSet);

//     if (lua_pcall(g_L, 7, 1, 0) != 0) {
//         lua_pop(g_L, 1); // pop error
//         RETURN_META_VALUE(MRES_IGNORED, 0);
//     }

//     // 如果 Lua 返回 1 或 0，我们可以覆盖引擎的判断
//     if (lua_isnumber(g_L, -1)) {
//         int ret = (int)lua_tointeger(g_L, -1);
//         lua_pop(g_L, 1);
//         RETURN_META_VALUE(MRES_SUPERCEDE, ret);
//     }

//     lua_pop(g_L, 1);
//     RETURN_META_VALUE(MRES_IGNORED, 0);
// }

// /* pfnCreateBaseline() */
// void CreateBaseline(int player, int eindex, struct entity_state_s *baseline, struct edict_s *entity, int playermodelindex, vec3_t player_mins, vec3_t player_maxs)
// {
//     if (!g_L) RETURN_META(MRES_IGNORED);

//     lua_getglobal(g_L, "MetaCreateBaseline");
//     if (!lua_isfunction(g_L, -1)) {
//         lua_pop(g_L, 1);
//         RETURN_META(MRES_IGNORED);
//     }

//     lua_pushinteger(g_L, player);
//     lua_pushinteger(g_L, eindex);
//     lua_pushlightuserdata(g_L, baseline);
//     lua_pushentity(g_L, entity);
//     lua_pushinteger(g_L, playermodelindex);
//     // vec3_t 是 float数组，直接作为指针传过去
//     lua_pushlightuserdata(g_L, player_mins); 
//     lua_pushlightuserdata(g_L, player_maxs);

//     if (lua_pcall(g_L, 7, 0, 0) != 0) {
//         lua_pop(g_L, 1);
//     }
//     RETURN_META(MRES_IGNORED);
// }

// /* pfnRegisterEncoders() */
// void RegisterEncoders(void)
// {
//     if (!g_L) RETURN_META(MRES_IGNORED);

//     lua_getglobal(g_L, "MetaRegisterEncoders");
//     if (!lua_isfunction(g_L, -1)) {
//         lua_pop(g_L, 1);
//         RETURN_META(MRES_IGNORED);
//     }

//     if (lua_pcall(g_L, 0, 0, 0) != 0) {
//         lua_pop(g_L, 1);
//     }
//     RETURN_META(MRES_IGNORED);
// }

// /* pfnGetWeaponData() */
// int GetWeaponData(struct edict_s *player, struct weapon_data_s *info)
// {
//     if (!g_L) RETURN_META_VALUE(MRES_IGNORED, 0);

//     lua_getglobal(g_L, "MetaGetWeaponData");
//     if (!lua_isfunction(g_L, -1)) {
//         lua_pop(g_L, 1);
//         RETURN_META_VALUE(MRES_IGNORED, 0);
//     }

//     lua_pushentity(g_L, player);
//     lua_pushlightuserdata(g_L, info);

//     if (lua_pcall(g_L, 2, 1, 0) != 0) {
//         lua_pop(g_L, 1);
//         RETURN_META_VALUE(MRES_IGNORED, 0);
//     }

//     if (lua_isnumber(g_L, -1)) {
//         int ret = (int)lua_tointeger(g_L, -1);
//         lua_pop(g_L, 1);
//         RETURN_META_VALUE(MRES_SUPERCEDE, ret);
//     }

//     lua_pop(g_L, 1);
//     RETURN_META_VALUE(MRES_IGNORED, 0);
// }

// /* pfnCmdStart() */
// void CmdStart(const edict_t *player, const struct usercmd_s *cmd, unsigned int random_seed)
// {
//     if (!g_L) RETURN_META(MRES_IGNORED);

//     lua_getglobal(g_L, "MetaCmdStart");
//     if (!lua_isfunction(g_L, -1)) {
//         lua_pop(g_L, 1);
//         RETURN_META(MRES_IGNORED);
//     }

//     lua_pushlightuserdata(g_L, (void*)player);
//     lua_pushlightuserdata(g_L, (void*)cmd);
//     lua_pushinteger(g_L, random_seed);

//     if (lua_pcall(g_L, 3, 0, 0) != 0) {
//         lua_pop(g_L, 1);
//     }
//     RETURN_META(MRES_IGNORED);
// }

// /* pfnCmdEnd() */
// void CmdEnd(const edict_t *player)
// {
//     if (!g_L) RETURN_META(MRES_IGNORED);

//     lua_getglobal(g_L, "MetaCmdEnd");
//     if (!lua_isfunction(g_L, -1)) {
//         lua_pop(g_L, 1);
//         RETURN_META(MRES_IGNORED);
//     }

//     lua_pushlightuserdata(g_L, (void*)player);

//     if (lua_pcall(g_L, 1, 0, 0) != 0) {
//         lua_pop(g_L, 1);
//     }
//     RETURN_META(MRES_IGNORED);
// }

// /* pfnConnectionlessPacket() */
// int ConnectionlessPacket(const struct netadr_s *net_from, const char *args, char *response_buffer, int *response_buffer_size)
// {
//     if (!g_L) RETURN_META_VALUE(MRES_IGNORED, 0);

//     lua_getglobal(g_L, "MetaConnectionlessPacket");
//     if (!lua_isfunction(g_L, -1)) {
//         lua_pop(g_L, 1);
//         RETURN_META_VALUE(MRES_IGNORED, 0);
//     }

//     // Args: netadr ptr, args (string), response buffer ptr, size ptr
//     lua_pushlightuserdata(g_L, (void*)net_from);
//     lua_pushstring(g_L, args); // 直接传字符串内容
//     lua_pushlightuserdata(g_L, response_buffer);
//     lua_pushlightuserdata(g_L, response_buffer_size);

//     if (lua_pcall(g_L, 4, 1, 0) != 0) {
//         lua_pop(g_L, 1);
//         RETURN_META_VALUE(MRES_IGNORED, 0);
//     }

//     if (lua_isnumber(g_L, -1)) {
//         int ret = (int)lua_tointeger(g_L, -1);
//         lua_pop(g_L, 1);
//         RETURN_META_VALUE(MRES_SUPERCEDE, ret);
//     }

//     lua_pop(g_L, 1);
//     RETURN_META_VALUE(MRES_IGNORED, 0);
// }

// /* pfnGetHullBounds() */
// int GetHullBounds(int hullnumber, float *mins, float *maxs)
// {
//     if (!g_L) RETURN_META_VALUE(MRES_IGNORED, 0);

//     lua_getglobal(g_L, "MetaGetHullBounds");
//     if (!lua_isfunction(g_L, -1)) {
//         lua_pop(g_L, 1);
//         RETURN_META_VALUE(MRES_IGNORED, 0);
//     }

//     lua_pushinteger(g_L, hullnumber);
//     lua_pushnumber(g_L, *mins);
//     lua_pushnumber(g_L, *maxs);

//     if (lua_pcall(g_L, 3, 1, 0) != 0) {
//         lua_pop(g_L, 1);
//         RETURN_META_VALUE(MRES_IGNORED, 0);
//     }

//     // Lua 如果返回 1，表示该 Hull 有效；0 表示无效
//     if (lua_isnumber(g_L, -1)) {
//         int ret = (int)lua_tointeger(g_L, -1);
//         lua_pop(g_L, 1);
//         RETURN_META_VALUE(MRES_SUPERCEDE, ret);
//     }

//     lua_pop(g_L, 1);
//     RETURN_META_VALUE(MRES_IGNORED, 0);
// }

// /* pfnCreateInstancedBaselines() */
// void CreateInstancedBaselines(void)
// {
//     if (!g_L)
//         RETURN_META(MRES_IGNORED);

//     lua_getglobal(g_L, "MetaCreateInstancedBaselines");

//     if (!lua_isfunction(g_L, -1))
//     {
//         lua_pop(g_L, 1);
//         RETURN_META(MRES_IGNORED);
//     }

//     if (lua_pcall(g_L, 0, 0, 0) != 0)
//     {
//         lua_pop(g_L, 1);
//     }

//     RETURN_META(MRES_IGNORED);
// }

// /* pfnInconsistentFile() */
// int InconsistentFile(const struct edict_s *player, const char *filename, char *disconnect_message)
// {
//     if (!g_L)
//         RETURN_META_VALUE(MRES_IGNORED, 0);

//     lua_getglobal(g_L, "MetaInconsistentFile");

//     if (!lua_isfunction(g_L, -1))
//     {
//         lua_pop(g_L, 1);
//         RETURN_META_VALUE(MRES_IGNORED, 0);
//     }

//     // Push: player (edict_t*)
//     lua_pushlightuserdata(g_L, (void*)player);
//     // Push: filename (string)
//     lua_pushstring(g_L, filename);

//     // 调用 Lua: 2 参数, 1 返回值
//     if (lua_pcall(g_L, 2, 1, 0) != 0)
//     {
//         lua_pop(g_L, 1);
//         RETURN_META_VALUE(MRES_IGNORED, 0);
//     }

//     // 1. 如果 Lua 返回字符串，说明要断开连接并设置原因
//     if (lua_isstring(g_L, -1))
//     {
//         const char *msg = lua_tostring(g_L, -1);
//         if (disconnect_message && msg)
//         {
//             // 复制消息到输出缓冲区 (最大 256 字符)
//             strncpy(disconnect_message, msg, 255);
//             disconnect_message[255] = '\0';
//         }
//         lua_pop(g_L, 1);
//         // 返回 1 表示强制断开
//         RETURN_META_VALUE(MRES_SUPERCEDE, 1);
//     }
//     // 2. 如果 Lua 返回数字 (例如 1)
//     else if (lua_isnumber(g_L, -1))
//     {
//         int ret = (int)lua_tointeger(g_L, -1);
//         lua_pop(g_L, 1);
//         if (ret != 0) // 只要不是0，就拦截
//         {
//             RETURN_META_VALUE(MRES_SUPERCEDE, ret);
//         }
//     }

//     lua_pop(g_L, 1);
//     RETURN_META_VALUE(MRES_IGNORED, 0);
// }

// /* pfnAllowLagCompensation() */
// int AllowLagCompensation(void)
// {
//     if (!g_L)
//         RETURN_META_VALUE(MRES_IGNORED, 0);

//     lua_getglobal(g_L, "MetaAllowLagCompensation");

//     if (!lua_isfunction(g_L, -1))
//     {
//         lua_pop(g_L, 1);
//         RETURN_META_VALUE(MRES_IGNORED, 0);
//     }

//     if (lua_pcall(g_L, 0, 1, 0) != 0)
//     {
//         lua_pop(g_L, 1);
//         RETURN_META_VALUE(MRES_IGNORED, 0);
//     }

//     // 如果 Lua 返回了 0 或 1，覆盖引擎设置
//     if (lua_isnumber(g_L, -1))
//     {
//         int ret = (int)lua_tointeger(g_L, -1);
//         lua_pop(g_L, 1);
//         RETURN_META_VALUE(MRES_SUPERCEDE, ret);
//     }

//     lua_pop(g_L, 1);
//     RETURN_META_VALUE(MRES_IGNORED, 0);
// }

// //----------------------------------
// // - GetEntityAPI2_Post functions
// //----------------------------------
// //----------------------------------
// //----------------------------------
// //----------------------------------
// //----------------------------------
// //----------------------------------
// //----------------------------------
// //----------------------------------
// //----------------------------------
// //----------------------------------

// /* pfnGameInit() */
// void GameDLLInit_Post(void)
// {
//     if (!g_L)
//         RETURN_META(MRES_IGNORED);

//     lua_getglobal(g_L, "MetaGameDLLInit_Post");

//     if (!lua_isfunction(g_L, -1))
//     {
//         lua_pop(g_L, 1);
//         RETURN_META(MRES_IGNORED);
//     }

//     if (lua_pcall(g_L, 0, 0, 0) != 0)
//     {
//         lua_pop(g_L, 1);
//     }

//     RETURN_META(MRES_IGNORED);
// }
// /* pfnSpawn() */
// int DispatchSpawn_Post(edict_t *pent)
// {
//     if (!g_L)
//         RETURN_META_VALUE(MRES_IGNORED, -1);

//     lua_getglobal(g_L, "MetaDispatchSpawn_Post");

//     if (!lua_isfunction(g_L, -1))
//     {
//         lua_pop(g_L, 1);
//         RETURN_META_VALUE(MRES_IGNORED, -1);
//     }

//     // Push: edict_t* (pointer)
//     lua_pushentity(g_L, pent);

//     if (lua_pcall(g_L, 1, 0, 0) != 0)
//     {
//         lua_pop(g_L, 1);
//     }

//     RETURN_META_VALUE(MRES_IGNORED, 0);
// }

// /* pfnThink() */
// void DispatchThink_Post(edict_t *pent)
// {
//     if (!g_L)
//         RETURN_META(MRES_IGNORED);

//     lua_getglobal(g_L, "MetaDispatchThink_Post");

//     if (!lua_isfunction(g_L, -1))
//     {
//         lua_pop(g_L, 1);
//         RETURN_META(MRES_IGNORED);
//     }

//     // Push: edict_t* (pointer)
//     lua_pushentity(g_L, pent);

//     if (lua_pcall(g_L, 1, 0, 0) != 0)
//     {
//         lua_pop(g_L, 1);
//     }

//     RETURN_META(MRES_IGNORED);
// }

// /* pfnUse() */
// void DispatchUse_Post(edict_t *pentUsed, edict_t *pentOther)
// {
//     if (!g_L)
//         RETURN_META(MRES_IGNORED);

//     lua_getglobal(g_L, "MetaDispatchUse_Post");

//     if (!lua_isfunction(g_L, -1))
//     {
//         lua_pop(g_L, 1);
//         RETURN_META(MRES_IGNORED);
//     }

//     // Push: edict_t* (Used), edict_t* (Other)
//     lua_pushentity(g_L, pentUsed);
//     lua_pushentity(g_L, pentOther);

//     if (lua_pcall(g_L, 2, 0, 0) != 0)
//     {
//         lua_pop(g_L, 1);
//     }

//     RETURN_META(MRES_IGNORED);
// }

// /* pfnTouch() */
// void DispatchTouch_Post(edict_t *pentTouched, edict_t *pentOther)
// {
//     if (!g_L)
//         RETURN_META(MRES_IGNORED);

//     lua_getglobal(g_L, "MetaDispatchTouch_Post");

//     if (!lua_isfunction(g_L, -1))
//     {
//         lua_pop(g_L, 1);
//         RETURN_META(MRES_IGNORED);
//     }

//     // Push: edict_t* (Touched), edict_t* (Other)
//     lua_pushentity(g_L, pentTouched);
//     lua_pushentity(g_L, pentOther);

//     if (lua_pcall(g_L, 2, 0, 0) != 0)
//     {
//         lua_pop(g_L, 1);
//     }

//     RETURN_META(MRES_IGNORED);
// }

// /* pfnBlocked() */
// void DispatchBlocked_Post(edict_t *pentBlocked, edict_t *pentOther)
// {
//     if (!g_L)
//         RETURN_META(MRES_IGNORED);

//     lua_getglobal(g_L, "MetaDispatchBlocked_Post");

//     if (!lua_isfunction(g_L, -1))
//     {
//         lua_pop(g_L, 1);
//         RETURN_META(MRES_IGNORED);
//     }

//     // Push: edict_t* (Blocked), edict_t* (Other)
//     lua_pushentity(g_L, pentBlocked);
//     lua_pushentity(g_L, pentOther);

//     if (lua_pcall(g_L, 2, 0, 0) != 0)
//     {
//         lua_pop(g_L, 1);
//     }

//     RETURN_META(MRES_IGNORED);
// }

// /* pfnKeyValue() */
// void DispatchKeyValue_Post(edict_t *pentKeyvalue, KeyValueData *pkvd)
// {
//     if (!g_L)
//         RETURN_META(MRES_IGNORED);

//     lua_getglobal(g_L, "MetaDispatchKeyValue_Post");

//     if (!lua_isfunction(g_L, -1))
//     {
//         lua_pop(g_L, 1);
//         RETURN_META(MRES_IGNORED);
//     }

//     // Push: edict_t* (Entity)
//     lua_pushentity(g_L, pentKeyvalue);
    
//     // Push: Key (string), Value (string), ClassName (string)
//     // 这样在 Lua 里可以直接读取配置，而不是拿到一个空指针
//     if (pkvd) {
//         lua_pushstring(g_L, pkvd->szKeyName);
//         lua_pushstring(g_L, pkvd->szValue);
//         lua_pushstring(g_L, pkvd->szClassName);
//     } else {
//         lua_pushnil(g_L);
//         lua_pushnil(g_L);
//         lua_pushnil(g_L);
//     }

//     if (lua_pcall(g_L, 4, 0, 0) != 0)
//     {
//         lua_pop(g_L, 1);
//     }

//     RETURN_META(MRES_IGNORED);
// }

// /* pfnSave() */
// void DispatchSave_Post(edict_t *pent, SAVERESTOREDATA *pSaveData)
// {
//     if (!g_L)
//         RETURN_META(MRES_IGNORED);

//     lua_getglobal(g_L, "MetaDispatchSave_Post");

//     if (!lua_isfunction(g_L, -1))
//     {
//         lua_pop(g_L, 1);
//         RETURN_META(MRES_IGNORED);
//     }

//     // Push: edict_t* (Entity), SAVERESTOREDATA* (pointer)
//     lua_pushentity(g_L, pent);
//     lua_pushlightuserdata(g_L, pSaveData);

//     if (lua_pcall(g_L, 2, 0, 0) != 0)
//     {
//         lua_pop(g_L, 1);
//     }

//     RETURN_META(MRES_IGNORED);
// }

// /* pfnRestore() */
// int DispatchRestore_Post(edict_t *pent, SAVERESTOREDATA *pSaveData, int globalEntity)
// {
//     if (!g_L)
//         RETURN_META_VALUE(MRES_IGNORED, 0);

//     lua_getglobal(g_L, "MetaDispatchRestore_Post");

//     if (!lua_isfunction(g_L, -1))
//     {
//         lua_pop(g_L, 1);
//         RETURN_META_VALUE(MRES_IGNORED, 0);
//     }

//     // Push: edict_t* (Entity), SAVERESTOREDATA* (pointer), globalEntity (int)
//     lua_pushentity(g_L, pent);
//     lua_pushlightuserdata(g_L, pSaveData);
//     lua_pushinteger(g_L, globalEntity);

//     if (lua_pcall(g_L, 3, 0, 0) != 0)
//     {
//         lua_pop(g_L, 1);
//     }

//     RETURN_META_VALUE(MRES_IGNORED, 0);
// }

// /* pfnSetAbsBox() */
// void DispatchObjectCollsionBox_Post(edict_t *pent)
// {
//     if (!g_L)
//         RETURN_META(MRES_IGNORED);

//     lua_getglobal(g_L, "MetaDispatchObjectCollsionBox_Post");

//     if (!lua_isfunction(g_L, -1))
//     {
//         lua_pop(g_L, 1);
//         RETURN_META(MRES_IGNORED);
//     }

//     // Push: edict_t* (pointer)
//     lua_pushentity(g_L, pent);

//     if (lua_pcall(g_L, 1, 0, 0) != 0)
//     {
//         lua_pop(g_L, 1);
//     }

//     RETURN_META(MRES_IGNORED);
// }

// /* pfnSaveWriteFields() */
// void SaveWriteFields_Post(SAVERESTOREDATA *pSaveData, const char *pname, void *pBaseData, TYPEDESCRIPTION *pFields, int fieldCount)
// {
//     if (!g_L)
//         RETURN_META(MRES_IGNORED);

//     lua_getglobal(g_L, "MetaSaveWriteFields_Post");

//     if (!lua_isfunction(g_L, -1))
//     {
//         lua_pop(g_L, 1);
//         RETURN_META(MRES_IGNORED);
//     }

//     // Push: SAVERESTOREDATA* (ptr)
//     lua_pushlightuserdata(g_L, pSaveData);
//     // Push: Name (string)
//     lua_pushstring(g_L, pname);
//     // Push: BaseData* (ptr)
//     lua_pushlightuserdata(g_L, pBaseData);
//     // Push: TYPEDESCRIPTION* (ptr)
//     lua_pushlightuserdata(g_L, pFields);
//     // Push: fieldCount (int)
//     lua_pushinteger(g_L, fieldCount);

//     if (lua_pcall(g_L, 5, 0, 0) != 0)
//     {
//         lua_pop(g_L, 1);
//     }

//     RETURN_META(MRES_IGNORED);
// }

// /* pfnSaveReadFields() */
// void SaveReadFields_Post(SAVERESTOREDATA *pSaveData, const char *pname, void *pBaseData, TYPEDESCRIPTION *pFields, int fieldCount)
// {
//     if (!g_L)
//         RETURN_META(MRES_IGNORED);

//     lua_getglobal(g_L, "MetaSaveReadFields_Post");

//     if (!lua_isfunction(g_L, -1))
//     {
//         lua_pop(g_L, 1);
//         RETURN_META(MRES_IGNORED);
//     }

//     // Push: SAVERESTOREDATA* (ptr)
//     lua_pushlightuserdata(g_L, pSaveData);
//     // Push: Name (string)
//     lua_pushstring(g_L, pname);
//     // Push: BaseData* (ptr)
//     lua_pushlightuserdata(g_L, pBaseData);
//     // Push: TYPEDESCRIPTION* (ptr)
//     lua_pushlightuserdata(g_L, pFields);
//     // Push: fieldCount (int)
//     lua_pushinteger(g_L, fieldCount);

//     if (lua_pcall(g_L, 5, 0, 0) != 0)
//     {
//         lua_pop(g_L, 1);
//     }

//     RETURN_META(MRES_IGNORED);
// }

// /* pfnSaveGlobalState() */
// void SaveGlobalState_Post(SAVERESTOREDATA *pSaveData)
// {
//     if (!g_L)
//         RETURN_META(MRES_IGNORED);

//     lua_getglobal(g_L, "MetaSaveGlobalState_Post");

//     if (!lua_isfunction(g_L, -1))
//     {
//         lua_pop(g_L, 1);
//         RETURN_META(MRES_IGNORED);
//     }

//     // Push: SAVERESTOREDATA* (ptr)
//     lua_pushlightuserdata(g_L, pSaveData);

//     if (lua_pcall(g_L, 1, 0, 0) != 0)
//     {
//         lua_pop(g_L, 1);
//     }

//     RETURN_META(MRES_IGNORED);
// }

// /* pfnRestoreGlobalState() */
// void RestoreGlobalState_Post(SAVERESTOREDATA *pSaveData)
// {
//     if (!g_L)
//         RETURN_META(MRES_IGNORED);

//     lua_getglobal(g_L, "MetaRestoreGlobalState_Post");

//     if (!lua_isfunction(g_L, -1))
//     {
//         lua_pop(g_L, 1);
//         RETURN_META(MRES_IGNORED);
//     }

//     // Push: SAVERESTOREDATA* (ptr)
//     lua_pushlightuserdata(g_L, pSaveData);

//     if (lua_pcall(g_L, 1, 0, 0) != 0)
//     {
//         lua_pop(g_L, 1);
//     }

//     RETURN_META(MRES_IGNORED);
// }

// /* pfnResetGlobalState() */
// void ResetGlobalState_Post(void)
// {
//     if (!g_L)
//         RETURN_META(MRES_IGNORED);

//     lua_getglobal(g_L, "MetaResetGlobalState_Post");

//     if (!lua_isfunction(g_L, -1))
//     {
//         lua_pop(g_L, 1);
//         RETURN_META(MRES_IGNORED);
//     }

//     if (lua_pcall(g_L, 0, 0, 0) != 0)
//     {
//         lua_pop(g_L, 1);
//     }

//     RETURN_META(MRES_IGNORED);
// }

// /* pfnClientConnect() */
// qboolean ClientConnect_Post(edict_t *pEntity, const char *pszName, const char *pszAddress, char szRejectReason[128])
// {
//     if (!g_L)
//         RETURN_META_VALUE(MRES_IGNORED, 0); // 0 = FALSE, 但 META 会忽略

//     lua_getglobal(g_L, "MetaClientConnect_Post");

//     if (!lua_isfunction(g_L, -1))
//     {
//         lua_pop(g_L, 1);
//         RETURN_META_VALUE(MRES_IGNORED, 0);
//     }

//     // Push: edict_t* (玩家实体)
//     lua_pushentity(g_L, pEntity);
//     // Push: Name (名字)
//     lua_pushstring(g_L, pszName);
//     // Push: IP (IP地址)
//     lua_pushstring(g_L, pszAddress);
    
//     // 调用 Lua: 3个参数, 1个返回值
//     if (lua_pcall(g_L, 3, 1, 0) != 0)
//     {
//         lua_pop(g_L, 1);
//         RETURN_META_VALUE(MRES_IGNORED, 0);
//     }

//     // 处理返回值
//     // 如果 Lua 返回了一个字符串，说明要拒绝连接，并且这个字符串是拒绝理由
//     if (lua_isstring(g_L, -1))
//     {
//         const char* reason = lua_tostring(g_L, -1);
        
//         // 把 Lua 的拒绝理由复制回 C++ 的 buffer
//         // 注意防止溢出 (128字节)
//         strncpy(szRejectReason, reason, 127);
//         szRejectReason[127] = '\0'; // 确保结尾

//         lua_pop(g_L, 1); // 弹出返回值

//         // 返回 FALSE 告诉引擎拒绝连接，并使用 MRES_SUPERCEDE 覆盖引擎原本的逻辑
//         RETURN_META_VALUE(MRES_SUPERCEDE, 0); 
//     }

//     // 如果没返回字符串（比如返回 nil 或 true），则允许连接
//     lua_pop(g_L, 1);
//     RETURN_META_VALUE(MRES_IGNORED, 0);
// }

// /* pfnClientDisconnect() */
// void ClientDisconnect_Post(edict_t *pEntity)
// {
//     if (!g_L)
//         RETURN_META(MRES_IGNORED);

//     lua_getglobal(g_L, "MetaClientDisconnect_Post");

//     if (!lua_isfunction(g_L, -1))
//     {
//         lua_pop(g_L, 1);
//         RETURN_META(MRES_IGNORED);
//     }

//     // Push: edict_t* (Player Entity)
//     lua_pushentity(g_L, pEntity);

//     if (lua_pcall(g_L, 1, 0, 0) != 0)
//     {
//         lua_pop(g_L, 1);
//     }

//     RETURN_META(MRES_IGNORED);
// }

// /* pfnClientKill() */
// void ClientKill_Post(edict_t *pEntity)
// {
//     if (!g_L)
//         RETURN_META(MRES_IGNORED);

//     lua_getglobal(g_L, "MetaClientKill_Post");

//     if (!lua_isfunction(g_L, -1))
//     {
//         lua_pop(g_L, 1);
//         RETURN_META(MRES_IGNORED);
//     }

//     // Push: edict_t* (Player Entity)
//     lua_pushentity(g_L, pEntity);

//     if (lua_pcall(g_L, 1, 0, 0) != 0)
//     {
//         lua_pop(g_L, 1);
//     }

//     RETURN_META(MRES_IGNORED);
// }

// /* pfnClientPutInServer() */
// void ClientPutInServer_Post(edict_t *pEntity)
// {
//     if (!g_L)
//         RETURN_META(MRES_IGNORED);

//     lua_getglobal(g_L, "MetaClientPutInServer_Post");

//     if (!lua_isfunction(g_L, -1))
//     {
//         lua_pop(g_L, 1);
//         RETURN_META(MRES_IGNORED);
//     }

//     // Push: edict_t* (Player Entity)
//     lua_pushentity(g_L, pEntity);

//     if (lua_pcall(g_L, 1, 0, 0) != 0)
//     {
//         lua_pop(g_L, 1);
//     }

//     RETURN_META(MRES_IGNORED);
// }

// /* pfnClientCommand() */
// void ClientCommand_Post(edict_t *pEntity)
// {
//     if (!g_L)
//         RETURN_META(MRES_IGNORED);

//     lua_getglobal(g_L, "MetaClientCommand_Post");

//     if (!lua_isfunction(g_L, -1))
//     {
//         lua_pop(g_L, 1);
//         RETURN_META(MRES_IGNORED);
//     }

//     // Push: edict_t* (Player Entity)
//     lua_pushentity(g_L, pEntity);
//     // 注意: 命令的具体内容在 Lua 中需要通过调用 engine.Cmd_Args() 等函数获取

//     if (lua_pcall(g_L, 1, 0, 0) != 0)
//     {
//         lua_pop(g_L, 1);
//     }

//     RETURN_META(MRES_IGNORED);
// }

// /* pfnClientUserInfoChanged() */
// void ClientUserInfoChanged_Post(edict_t *pEntity, char *infobuffer)
// {
//     if (!g_L)
//         RETURN_META(MRES_IGNORED);

//     lua_getglobal(g_L, "MetaClientUserInfoChanged_Post");

//     if (!lua_isfunction(g_L, -1))
//     {
//         lua_pop(g_L, 1);
//         RETURN_META(MRES_IGNORED);
//     }

//     // Push: edict_t* (Player Entity)
//     lua_pushentity(g_L, pEntity);
//     // Push: infobuffer string (因为只是读取，复制一份给Lua没问题)
//     lua_pushstring(g_L, infobuffer);

//     if (lua_pcall(g_L, 2, 0, 0) != 0)
//     {
//         lua_pop(g_L, 1);
//     }

//     RETURN_META(MRES_IGNORED);
// }

// /* pfnServerActivate() */
// void ServerActivate_Post(edict_t *pEdictList, int edictCount, int clientMax)
// {
//     if (!g_L)
//         RETURN_META(MRES_IGNORED);

//     // 可以在这里做一些 Map 初始化相关的工作，比如清空之前的缓存
//     // g_EntVarMap.clear(); // 如果需要在换图时重置，看具体需求

//     lua_getglobal(g_L, "MetaServerActivate_Post");

//     if (!lua_isfunction(g_L, -1))
//     {
//         lua_pop(g_L, 1);
//         RETURN_META(MRES_IGNORED);
//     }

//     // Push: pEdictList (指针)
//     lua_pushentity(g_L, pEdictList);
//     // Push: edictCount (int)
//     lua_pushinteger(g_L, edictCount);
//     // Push: clientMax (int)
//     lua_pushinteger(g_L, clientMax);

//     if (lua_pcall(g_L, 3, 0, 0) != 0)
//     {
//         lua_pop(g_L, 1);
//     }

//     RETURN_META(MRES_IGNORED);
// }

// /* pfnServerDeactivate() */
// void ServerDeactivate_Post(void)
// {
//     if (!g_L)
//         RETURN_META(MRES_IGNORED);

//     lua_getglobal(g_L, "MetaServerDeactivate_Post");

//     if (!lua_isfunction(g_L, -1))
//     {
//         lua_pop(g_L, 1);
//         RETURN_META(MRES_IGNORED);
//     }

//     if (lua_pcall(g_L, 0, 0, 0) != 0)
//     {
//         lua_pop(g_L, 1);
//     }

//     RETURN_META(MRES_IGNORED);
// }

// /* pfnPlayerPreThink() */
// void PlayerPreThink_Post(edict_t *pEntity)
// {
//     if (!g_L)
//         RETURN_META(MRES_IGNORED);

//     lua_getglobal(g_L, "MetaPlayerPreThink_Post");

//     if (!lua_isfunction(g_L, -1))
//     {
//         lua_pop(g_L, 1);
//         RETURN_META(MRES_IGNORED);
//     }

//     // Push: edict_t* (Player Entity)
//     lua_pushentity(g_L, pEntity);

//     if (lua_pcall(g_L, 1, 0, 0) != 0)
//     {
//         lua_pop(g_L, 1);
//     }

//     RETURN_META(MRES_IGNORED);
// }

// /* pfnPlayerPostThink() */
// void PlayerPostThink_Post(edict_t *pEntity)
// {
//     if (!g_L)
//         RETURN_META(MRES_IGNORED);

//     lua_getglobal(g_L, "MetaPlayerPostThink_Post");

//     if (!lua_isfunction(g_L, -1))
//     {
//         lua_pop(g_L, 1);
//         RETURN_META(MRES_IGNORED);
//     }

//     // Push: edict_t* (Player Entity)
//     lua_pushentity(g_L, pEntity);

//     if (lua_pcall(g_L, 1, 0, 0) != 0)
//     {
//         lua_pop(g_L, 1);
//     }

//     RETURN_META(MRES_IGNORED);
// }

// /* pfnStartFrame() */
// void StartFrame_Post(void)
// {
//     if (!g_L)
//         RETURN_META(MRES_IGNORED);

//     lua_getglobal(g_L, "MetaStartFrame_Post");

//     if (!lua_isfunction(g_L, -1))
//     {
//         lua_pop(g_L, 1);
//         RETURN_META(MRES_IGNORED);
//     }

//     if (lua_pcall(g_L, 0, 0, 0) != 0)
//     {
//         lua_pop(g_L, 1);
//     }

//     RETURN_META(MRES_IGNORED);
// }
// /* pfnParmsNewLevel() */
// void ParmsNewLevel_Post(void)
// {
//     if (!g_L)
//         RETURN_META(MRES_IGNORED);

//     lua_getglobal(g_L, "MetaParmsNewLevel_Post");

//     if (!lua_isfunction(g_L, -1))
//     {
//         lua_pop(g_L, 1);
//         RETURN_META(MRES_IGNORED);
//     }

//     if (lua_pcall(g_L, 0, 0, 0) != 0)
//     {
//         lua_pop(g_L, 1);
//     }

//     RETURN_META(MRES_IGNORED);
// }

// /* pfnParmsChangeLevel() */
// void ParmsChangeLevel_Post(void)
// {
//     if (!g_L)
//         RETURN_META(MRES_IGNORED);

//     lua_getglobal(g_L, "MetaParmsChangeLevel_Post");

//     if (!lua_isfunction(g_L, -1))
//     {
//         lua_pop(g_L, 1);
//         RETURN_META(MRES_IGNORED);
//     }

//     if (lua_pcall(g_L, 0, 0, 0) != 0)
//     {
//         lua_pop(g_L, 1);
//     }

//     RETURN_META(MRES_IGNORED);
// }

// /* pfnGetGameDescription() */
// // 注意：这个函数返回 const char*，用于显示在服务器浏览器的 "Game" 列
// const char *GetGameDescription_Post(void)
// {
//     if (!g_L)
//         RETURN_META_VALUE(MRES_IGNORED, NULL);

//     lua_getglobal(g_L, "MetaGetGameDescription_Post");

//     if (!lua_isfunction(g_L, -1))
//     {
//         lua_pop(g_L, 1);
//         RETURN_META_VALUE(MRES_IGNORED, NULL);
//     }

//     // 调用 Lua
//     if (lua_pcall(g_L, 0, 1, 0) != 0)
//     {
//         lua_pop(g_L, 1);
//         RETURN_META_VALUE(MRES_IGNORED, NULL);
//     }

//     // 处理返回值
//     if (lua_isstring(g_L, -1))
//     {
//         // 我们必须使用静态缓冲区，因为一旦 lua_pop，指针可能失效
//         static char staticGameDesc[64];
//         const char *ret = lua_tostring(g_L, -1);
        
//         strncpy(staticGameDesc, ret, 63);
//         staticGameDesc[63] = '\0';

//         lua_pop(g_L, 1);
//         // 返回 SUPERCEDE 并带上我们的新名字
//         RETURN_META_VALUE(MRES_SUPERCEDE, staticGameDesc);
//     }

//     lua_pop(g_L, 1);
//     RETURN_META_VALUE(MRES_IGNORED, NULL);
// }

// /* pfnPlayerCustomization() */
// void PlayerCustomization_Post(edict_t *pEntity, customization_t *pCustom)
// {
//     if (!g_L)
//         RETURN_META(MRES_IGNORED);

//     lua_getglobal(g_L, "MetaPlayerCustomization_Post");

//     if (!lua_isfunction(g_L, -1))
//     {
//         lua_pop(g_L, 1);
//         RETURN_META(MRES_IGNORED);
//     }

//     lua_pushentity(g_L, pEntity);
//     lua_pushlightuserdata(g_L, pCustom);

//     if (lua_pcall(g_L, 2, 0, 0) != 0)
//     {
//         lua_pop(g_L, 1);
//     }

//     RETURN_META(MRES_IGNORED);
// }

// /* pfnSpectatorConnect() */
// void SpectatorConnect_Post(edict_t *pEntity)
// {
//     if (!g_L)
//         RETURN_META(MRES_IGNORED);

//     lua_getglobal(g_L, "MetaSpectatorConnect_Post");

//     if (!lua_isfunction(g_L, -1))
//     {
//         lua_pop(g_L, 1);
//         RETURN_META(MRES_IGNORED);
//     }

//     lua_pushentity(g_L, pEntity);

//     if (lua_pcall(g_L, 1, 0, 0) != 0)
//     {
//         lua_pop(g_L, 1);
//     }

//     RETURN_META(MRES_IGNORED);
// }

// /* pfnSpectatorDisconnect() */
// void SpectatorDisconnect_Post(edict_t *pEntity)
// {
//     if (!g_L)
//         RETURN_META(MRES_IGNORED);

//     lua_getglobal(g_L, "MetaSpectatorDisconnect_Post");

//     if (!lua_isfunction(g_L, -1))
//     {
//         lua_pop(g_L, 1);
//         RETURN_META(MRES_IGNORED);
//     }

//     lua_pushentity(g_L, pEntity);

//     if (lua_pcall(g_L, 1, 0, 0) != 0)
//     {
//         lua_pop(g_L, 1);
//     }

//     RETURN_META(MRES_IGNORED);
// }

// /* pfnSpectatorThink() */
// void SpectatorThink_Post(edict_t *pEntity)
// {
//     if (!g_L)
//         RETURN_META(MRES_IGNORED);

//     lua_getglobal(g_L, "MetaSpectatorThink_Post");

//     if (!lua_isfunction(g_L, -1))
//     {
//         lua_pop(g_L, 1);
//         RETURN_META(MRES_IGNORED);
//     }

//     lua_pushentity(g_L, pEntity);

//     if (lua_pcall(g_L, 1, 0, 0) != 0)
//     {
//         lua_pop(g_L, 1);
//     }

//     RETURN_META(MRES_IGNORED);
// }

// /* pfnSys_Error() */
// void Sys_Error_Post(const char *error_string)
// {
//     // 这个函数调用意味着服务器即将崩溃或强制关闭
//     if (!g_L)
//         RETURN_META(MRES_IGNORED);

//     lua_getglobal(g_L, "MetaSys_Error_Post");

//     if (!lua_isfunction(g_L, -1))
//     {
//         lua_pop(g_L, 1);
//         RETURN_META(MRES_IGNORED);
//     }

//     lua_pushstring(g_L, error_string);

//     if (lua_pcall(g_L, 1, 0, 0) != 0)
//     {
//         lua_pop(g_L, 1);
//     }

//     RETURN_META(MRES_IGNORED);
// }

// /* pfnPM_Move() */
// void PM_Move_Post(struct playermove_s *ppmove, qboolean server)
// {
//     if (!g_L)
//         RETURN_META(MRES_IGNORED);

//     lua_getglobal(g_L, "MetaPM_Move_Post");

//     if (!lua_isfunction(g_L, -1))
//     {
//         lua_pop(g_L, 1);
//         RETURN_META(MRES_IGNORED);
//     }

//     lua_pushlightuserdata(g_L, ppmove);
//     lua_pushboolean(g_L, server); // qboolean 本质是 int，但在 Lua 里用 bool 更直观

//     if (lua_pcall(g_L, 2, 0, 0) != 0)
//     {
//         lua_pop(g_L, 1);
//     }

//     RETURN_META(MRES_IGNORED);
// }

// /* pfnPM_Init() */
// void PM_Init_Post(struct playermove_s *ppmove)
// {
//     if (!g_L)
//         RETURN_META(MRES_IGNORED);

//     lua_getglobal(g_L, "MetaPM_Init_Post");

//     if (!lua_isfunction(g_L, -1))
//     {
//         lua_pop(g_L, 1);
//         RETURN_META(MRES_IGNORED);
//     }

//     lua_pushlightuserdata(g_L, ppmove);

//     if (lua_pcall(g_L, 1, 0, 0) != 0)
//     {
//         lua_pop(g_L, 1);
//     }

//     RETURN_META(MRES_IGNORED);
// }

// /* pfnSetupVisibility() */
// void SetupVisibility_Post(struct edict_s *pViewEntity, struct edict_s *pClient, unsigned char **pvs, unsigned char **pas)
// {
//     if (!g_L) RETURN_META(MRES_IGNORED);

//     lua_getglobal(g_L, "MetaSetupVisibility_Post");
//     if (!lua_isfunction(g_L, -1)) {
//         lua_pop(g_L, 1);
//         RETURN_META(MRES_IGNORED);
//     }

//     // Args: ViewEntity, Client, pvs ptr, pas ptr
//     lua_pushentity(g_L, pViewEntity);
//     lua_pushentity(g_L, pClient);
//     lua_pushlightuserdata(g_L, pvs);
//     lua_pushlightuserdata(g_L, pas);

//     if (lua_pcall(g_L, 4, 0, 0) != 0) {
//         lua_pop(g_L, 1);
//     }
//     RETURN_META(MRES_IGNORED);
// }

// /* pfnUpdateClientData() */
// void UpdateClientData_Post(const struct edict_s *ent, int sendweapons, struct clientdata_s *cd)
// {
//     if (!g_L) RETURN_META(MRES_IGNORED);

//     lua_getglobal(g_L, "MetaUpdateClientData_Post");
//     if (!lua_isfunction(g_L, -1)) {
//         lua_pop(g_L, 1);
//         RETURN_META(MRES_IGNORED);
//     }

//     // Args: Entity, SendWeapons(int), ClientData ptr
//     lua_pushlightuserdata(g_L, (void*)ent);
//     lua_pushinteger(g_L, sendweapons);
//     lua_pushlightuserdata(g_L, cd);

//     if (lua_pcall(g_L, 3, 0, 0) != 0) {
//         lua_pop(g_L, 1);
//     }
//     RETURN_META(MRES_IGNORED);
// }

// /* pfnAddToFullPack() - 警告：高频调用 */
// int AddToFullPack_Post(struct entity_state_s *state, int e, edict_t *ent, edict_t *host, int hostflags, int player, unsigned char *pSet)
// {
//     if (!g_L) RETURN_META_VALUE(MRES_IGNORED, 0);

//     lua_getglobal(g_L, "MetaAddToFullPack_Post");
//     if (!lua_isfunction(g_L, -1)) {
//         lua_pop(g_L, 1);
//         RETURN_META_VALUE(MRES_IGNORED, 0);
//     }

//     // Args: state*, e_index, ent*, host*, hostflags, player(bool/int), pSet*
//     lua_pushlightuserdata(g_L, state);
//     lua_pushinteger(g_L, e);
//     lua_pushentity(g_L, ent);
//     lua_pushentity(g_L, host);
//     lua_pushinteger(g_L, hostflags);
//     lua_pushinteger(g_L, player);
//     lua_pushlightuserdata(g_L, pSet);

//     if (lua_pcall(g_L, 7, 1, 0) != 0) {
//         lua_pop(g_L, 1); // pop error
//         RETURN_META_VALUE(MRES_IGNORED, 0);
//     }

//     // 如果 Lua 返回 1 或 0，我们可以覆盖引擎的判断
//     if (lua_isnumber(g_L, -1)) {
//         int ret = (int)lua_tointeger(g_L, -1);
//         lua_pop(g_L, 1);
//         RETURN_META_VALUE(MRES_SUPERCEDE, ret);
//     }

//     lua_pop(g_L, 1);
//     RETURN_META_VALUE(MRES_IGNORED, 0);
// }

// /* pfnCreateBaseline() */
// void CreateBaseline_Post(int player, int eindex, struct entity_state_s *baseline, struct edict_s *entity, int playermodelindex, vec3_t player_mins, vec3_t player_maxs)
// {
//     if (!g_L) RETURN_META(MRES_IGNORED);

//     lua_getglobal(g_L, "MetaCreateBaseline_Post");
//     if (!lua_isfunction(g_L, -1)) {
//         lua_pop(g_L, 1);
//         RETURN_META(MRES_IGNORED);
//     }

//     lua_pushinteger(g_L, player);
//     lua_pushinteger(g_L, eindex);
//     lua_pushlightuserdata(g_L, baseline);
//     lua_pushentity(g_L, entity);
//     lua_pushinteger(g_L, playermodelindex);
//     // vec3_t 是 float数组，直接作为指针传过去
//     lua_pushlightuserdata(g_L, player_mins); 
//     lua_pushlightuserdata(g_L, player_maxs);

//     if (lua_pcall(g_L, 7, 0, 0) != 0) {
//         lua_pop(g_L, 1);
//     }
//     RETURN_META(MRES_IGNORED);
// }

// /* pfnRegisterEncoders() */
// void RegisterEncoders_Post(void)
// {
//     if (!g_L) RETURN_META(MRES_IGNORED);

//     lua_getglobal(g_L, "MetaRegisterEncoders_Post");
//     if (!lua_isfunction(g_L, -1)) {
//         lua_pop(g_L, 1);
//         RETURN_META(MRES_IGNORED);
//     }

//     if (lua_pcall(g_L, 0, 0, 0) != 0) {
//         lua_pop(g_L, 1);
//     }
//     RETURN_META(MRES_IGNORED);
// }

// /* pfnGetWeaponData() */
// int GetWeaponData_Post(struct edict_s *player, struct weapon_data_s *info)
// {
//     if (!g_L) RETURN_META_VALUE(MRES_IGNORED, 0);

//     lua_getglobal(g_L, "MetaGetWeaponData_Post");
//     if (!lua_isfunction(g_L, -1)) {
//         lua_pop(g_L, 1);
//         RETURN_META_VALUE(MRES_IGNORED, 0);
//     }

//     lua_pushentity(g_L, player);
//     lua_pushlightuserdata(g_L, info);

//     if (lua_pcall(g_L, 2, 1, 0) != 0) {
//         lua_pop(g_L, 1);
//         RETURN_META_VALUE(MRES_IGNORED, 0);
//     }

//     if (lua_isnumber(g_L, -1)) {
//         int ret = (int)lua_tointeger(g_L, -1);
//         lua_pop(g_L, 1);
//         RETURN_META_VALUE(MRES_SUPERCEDE, ret);
//     }

//     lua_pop(g_L, 1);
//     RETURN_META_VALUE(MRES_IGNORED, 0);
// }

// /* pfnCmdStart() */
// void CmdStart_Post(const edict_t *player, const struct usercmd_s *cmd, unsigned int random_seed)
// {
//     if (!g_L) RETURN_META(MRES_IGNORED);

//     lua_getglobal(g_L, "MetaCmdStart_Post");
//     if (!lua_isfunction(g_L, -1)) {
//         lua_pop(g_L, 1);
//         RETURN_META(MRES_IGNORED);
//     }

//     lua_pushlightuserdata(g_L, (void*)player);
//     lua_pushlightuserdata(g_L, (void*)cmd);
//     lua_pushinteger(g_L, random_seed);

//     if (lua_pcall(g_L, 3, 0, 0) != 0) {
//         lua_pop(g_L, 1);
//     }
//     RETURN_META(MRES_IGNORED);
// }

// /* pfnCmdEnd() */
// void CmdEnd_Post(const edict_t *player)
// {
//     if (!g_L) RETURN_META(MRES_IGNORED);

//     lua_getglobal(g_L, "MetaCmdEnd_Post");
//     if (!lua_isfunction(g_L, -1)) {
//         lua_pop(g_L, 1);
//         RETURN_META(MRES_IGNORED);
//     }

//     lua_pushlightuserdata(g_L, (void*)player);

//     if (lua_pcall(g_L, 1, 0, 0) != 0) {
//         lua_pop(g_L, 1);
//     }
//     RETURN_META(MRES_IGNORED);
// }

// /* pfnConnectionlessPacket() */
// int ConnectionlessPacket_Post(const struct netadr_s *net_from, const char *args, char *response_buffer, int *response_buffer_size)
// {
//     if (!g_L) RETURN_META_VALUE(MRES_IGNORED, 0);

//     lua_getglobal(g_L, "MetaConnectionlessPacket_Post");
//     if (!lua_isfunction(g_L, -1)) {
//         lua_pop(g_L, 1);
//         RETURN_META_VALUE(MRES_IGNORED, 0);
//     }

//     // Args: netadr ptr, args (string), response buffer ptr, size ptr
//     lua_pushlightuserdata(g_L, (void*)net_from);
//     lua_pushstring(g_L, args); // 直接传字符串内容
//     lua_pushlightuserdata(g_L, response_buffer);
//     lua_pushlightuserdata(g_L, response_buffer_size);

//     if (lua_pcall(g_L, 4, 1, 0) != 0) {
//         lua_pop(g_L, 1);
//         RETURN_META_VALUE(MRES_IGNORED, 0);
//     }

//     if (lua_isnumber(g_L, -1)) {
//         int ret = (int)lua_tointeger(g_L, -1);
//         lua_pop(g_L, 1);
//         RETURN_META_VALUE(MRES_SUPERCEDE, ret);
//     }

//     lua_pop(g_L, 1);
//     RETURN_META_VALUE(MRES_IGNORED, 0);
// }

// /* pfnGetHullBounds() */
// int GetHullBounds_Post(int hullnumber, float *mins, float *maxs)
// {
//     if (!g_L) RETURN_META_VALUE(MRES_IGNORED, 0);

//     lua_getglobal(g_L, "MetaGetHullBounds_Post");
//     if (!lua_isfunction(g_L, -1)) {
//         lua_pop(g_L, 1);
//         RETURN_META_VALUE(MRES_IGNORED, 0);
//     }

//     lua_pushinteger(g_L, hullnumber);
//     lua_pushnumber(g_L, *mins);
//     lua_pushnumber(g_L, *maxs);

//     if (lua_pcall(g_L, 3, 1, 0) != 0) {
//         lua_pop(g_L, 1);
//         RETURN_META_VALUE(MRES_IGNORED, 0);
//     }

//     // Lua 如果返回 1，表示该 Hull 有效；0 表示无效
//     if (lua_isnumber(g_L, -1)) {
//         int ret = (int)lua_tointeger(g_L, -1);
//         lua_pop(g_L, 1);
//         RETURN_META_VALUE(MRES_SUPERCEDE, ret);
//     }

//     lua_pop(g_L, 1);
//     RETURN_META_VALUE(MRES_IGNORED, 0);
// }

// /* pfnCreateInstancedBaselines() */
// void CreateInstancedBaselines_Post(void)
// {
//     if (!g_L)
//         RETURN_META(MRES_IGNORED);

//     lua_getglobal(g_L, "MetaCreateInstancedBaselines_Post");

//     if (!lua_isfunction(g_L, -1))
//     {
//         lua_pop(g_L, 1);
//         RETURN_META(MRES_IGNORED);
//     }

//     if (lua_pcall(g_L, 0, 0, 0) != 0)
//     {
//         lua_pop(g_L, 1);
//     }

//     RETURN_META(MRES_IGNORED);
// }

// /* pfnInconsistentFile() */
// int InconsistentFile_Post(const struct edict_s *player, const char *filename, char *disconnect_message)
// {
//     if (!g_L)
//         RETURN_META_VALUE(MRES_IGNORED, 0);

//     lua_getglobal(g_L, "MetaInconsistentFile_Post");

//     if (!lua_isfunction(g_L, -1))
//     {
//         lua_pop(g_L, 1);
//         RETURN_META_VALUE(MRES_IGNORED, 0);
//     }

//     // Push: player (edict_t*)
//     lua_pushlightuserdata(g_L, (void*)player);
//     // Push: filename (string)
//     lua_pushstring(g_L, filename);

//     // 调用 Lua: 2 参数, 1 返回值
//     if (lua_pcall(g_L, 2, 1, 0) != 0)
//     {
//         lua_pop(g_L, 1);
//         RETURN_META_VALUE(MRES_IGNORED, 0);
//     }

//     // 1. 如果 Lua 返回字符串，说明要断开连接并设置原因
//     if (lua_isstring(g_L, -1))
//     {
//         const char *msg = lua_tostring(g_L, -1);
//         if (disconnect_message && msg)
//         {
//             // 复制消息到输出缓冲区 (最大 256 字符)
//             strncpy(disconnect_message, msg, 255);
//             disconnect_message[255] = '\0';
//         }
//         lua_pop(g_L, 1);
//         // 返回 1 表示强制断开
//         RETURN_META_VALUE(MRES_SUPERCEDE, 1);
//     }
//     // 2. 如果 Lua 返回数字 (例如 1)
//     else if (lua_isnumber(g_L, -1))
//     {
//         int ret = (int)lua_tointeger(g_L, -1);
//         lua_pop(g_L, 1);
//         if (ret != 0) // 只要不是0，就拦截
//         {
//             RETURN_META_VALUE(MRES_SUPERCEDE, ret);
//         }
//     }

//     lua_pop(g_L, 1);
//     RETURN_META_VALUE(MRES_IGNORED, 0);
// }

// /* pfnAllowLagCompensation() */
// int AllowLagCompensation_Post(void)
// {
//     if (!g_L)
//         RETURN_META_VALUE(MRES_IGNORED, 0);

//     lua_getglobal(g_L, "MetaAllowLagCompensation_Post");

//     if (!lua_isfunction(g_L, -1))
//     {
//         lua_pop(g_L, 1);
//         RETURN_META_VALUE(MRES_IGNORED, 0);
//     }

//     if (lua_pcall(g_L, 0, 1, 0) != 0)
//     {
//         lua_pop(g_L, 1);
//         RETURN_META_VALUE(MRES_IGNORED, 0);
//     }

//     // 如果 Lua 返回了 0 或 1，覆盖引擎设置
//     if (lua_isnumber(g_L, -1))
//     {
//         int ret = (int)lua_tointeger(g_L, -1);
//         lua_pop(g_L, 1);
//         RETURN_META_VALUE(MRES_SUPERCEDE, ret);
//     }

//     lua_pop(g_L, 1);
//     RETURN_META_VALUE(MRES_IGNORED, 0);
// }
// //----------------------------------
// // - GetEngineAPI functions
// //----------------------------------
// //----------------------------------
// //----------------------------------
// //----------------------------------
// //----------------------------------
// //----------------------------------
// //----------------------------------
// //----------------------------------
// //----------------------------------
// //----------------------------------
// /* pfnPrecacheModel() */
// int PrecacheModel(const char* s)
// {
//     if (!g_L)
//         RETURN_META_VALUE(MRES_IGNORED, 0);

//     lua_getglobal(g_L, "MetaPrecacheModel");

//     if (!lua_isfunction(g_L, -1))
//     {
//         lua_pop(g_L, 1);
//         RETURN_META_VALUE(MRES_IGNORED, 0);
//     }

//     lua_pushstring(g_L, s);

//     if (lua_pcall(g_L, 1, 1, 0) != 0)
//     {
//         lua_pop(g_L, 1);
//         RETURN_META_VALUE(MRES_IGNORED, 0);
//     }

//     // 如果 Lua 返回一个数字，则覆盖引擎的返回值
//     if (lua_isnumber(g_L, -1))
//     {
//         int ret = (int)lua_tointeger(g_L, -1);
//         lua_pop(g_L, 1);
//         RETURN_META_VALUE(MRES_SUPERCEDE, ret);
//     }

//     lua_pop(g_L, 1);
//     RETURN_META_VALUE(MRES_IGNORED, 0);
// }

// /* pfnPrecacheSound() */
// int PrecacheSound(const char* s)
// {
//     if (!g_L)
//         RETURN_META_VALUE(MRES_IGNORED, 0);

//     lua_getglobal(g_L, "MetaPrecacheSound");

//     if (!lua_isfunction(g_L, -1))
//     {
//         lua_pop(g_L, 1);
//         RETURN_META_VALUE(MRES_IGNORED, 0);
//     }

//     lua_pushstring(g_L, s);

//     if (lua_pcall(g_L, 1, 1, 0) != 0)
//     {
//         lua_pop(g_L, 1);
//         RETURN_META_VALUE(MRES_IGNORED, 0);
//     }

//     if (lua_isnumber(g_L, -1))
//     {
//         int ret = (int)lua_tointeger(g_L, -1);
//         lua_pop(g_L, 1);
//         RETURN_META_VALUE(MRES_SUPERCEDE, ret);
//     }

//     lua_pop(g_L, 1);
//     RETURN_META_VALUE(MRES_IGNORED, 0);
// }

// /* pfnSetModel() */
// void SetModel(edict_t *e, const char *m)
// {
//     if (!g_L)
//         RETURN_META(MRES_IGNORED);

//     lua_getglobal(g_L, "MetaSetModel");

//     if (!lua_isfunction(g_L, -1))
//     {
//         lua_pop(g_L, 1);
//         RETURN_META(MRES_IGNORED);
//     }

//     // 使用自定义的 pushentity
//     lua_pushentity(g_L, e);
//     lua_pushstring(g_L, m);

//     if (lua_pcall(g_L, 2, 0, 0) != 0)
//     {
//         lua_pop(g_L, 1);
//     }

//     RETURN_META(MRES_IGNORED);
// }

// /* pfnModelIndex() */
// int ModelIndex(const char *m)
// {
//     if (!g_L)
//         RETURN_META_VALUE(MRES_IGNORED, 0);

//     lua_getglobal(g_L, "MetaModelIndex");

//     if (!lua_isfunction(g_L, -1))
//     {
//         lua_pop(g_L, 1);
//         RETURN_META_VALUE(MRES_IGNORED, 0);
//     }

//     lua_pushstring(g_L, m);

//     if (lua_pcall(g_L, 1, 1, 0) != 0)
//     {
//         lua_pop(g_L, 1);
//         RETURN_META_VALUE(MRES_IGNORED, 0);
//     }

//     if (lua_isnumber(g_L, -1))
//     {
//         int ret = (int)lua_tointeger(g_L, -1);
//         lua_pop(g_L, 1);
//         RETURN_META_VALUE(MRES_SUPERCEDE, ret);
//     }

//     lua_pop(g_L, 1);
//     RETURN_META_VALUE(MRES_IGNORED, 0);
// }

// /* pfnModelFrames() */
// int ModelFrames(int modelIndex)
// {
//     if (!g_L)
//         RETURN_META_VALUE(MRES_IGNORED, 0);

//     lua_getglobal(g_L, "MetaModelFrames");

//     if (!lua_isfunction(g_L, -1))
//     {
//         lua_pop(g_L, 1);
//         RETURN_META_VALUE(MRES_IGNORED, 0);
//     }

//     lua_pushinteger(g_L, modelIndex);

//     if (lua_pcall(g_L, 1, 1, 0) != 0)
//     {
//         lua_pop(g_L, 1);
//         RETURN_META_VALUE(MRES_IGNORED, 0);
//     }

//     if (lua_isnumber(g_L, -1))
//     {
//         int ret = (int)lua_tointeger(g_L, -1);
//         lua_pop(g_L, 1);
//         RETURN_META_VALUE(MRES_SUPERCEDE, ret);
//     }

//     lua_pop(g_L, 1);
//     RETURN_META_VALUE(MRES_IGNORED, 0);
// }

// /* pfnSetSize() */
// void SetSize(edict_t *e, const float *rgflMin, const float *rgflMax)
// {
//     if (!g_L)
//         RETURN_META(MRES_IGNORED);

//     lua_getglobal(g_L, "MetaSetSize");

//     if (!lua_isfunction(g_L, -1))
//     {
//         lua_pop(g_L, 1);
//         RETURN_META(MRES_IGNORED);
//     }

//     // Entity
//     lua_pushentity(g_L, e);
//     // Min/Max vectors (传递指针，因为是 const float*)
//     lua_pushlightuserdata(g_L, (void*)rgflMin);
//     lua_pushlightuserdata(g_L, (void*)rgflMax);

//     if (lua_pcall(g_L, 3, 0, 0) != 0)
//     {
//         lua_pop(g_L, 1);
//     }

//     RETURN_META(MRES_IGNORED);
// }

// /* pfnChangeLevel() */
// void ChangeLevel(const char* s1, const char* s2)
// {
//     if (!g_L)
//         RETURN_META(MRES_IGNORED);

//     lua_getglobal(g_L, "MetaChangeLevel");

//     if (!lua_isfunction(g_L, -1))
//     {
//         lua_pop(g_L, 1);
//         RETURN_META(MRES_IGNORED);
//     }

//     // Map name
//     lua_pushstring(g_L, s1);
//     // Landmark name
//     lua_pushstring(g_L, s2);

//     if (lua_pcall(g_L, 2, 0, 0) != 0)
//     {
//         lua_pop(g_L, 1);
//     }

//     RETURN_META(MRES_IGNORED);
// }

// /* pfnGetSpawnParms() */
// void GetSpawnParms(edict_t *ent)
// {
//     if (!g_L)
//         RETURN_META(MRES_IGNORED);

//     lua_getglobal(g_L, "MetaGetSpawnParms");

//     if (!lua_isfunction(g_L, -1))
//     {
//         lua_pop(g_L, 1);
//         RETURN_META(MRES_IGNORED);
//     }

//     lua_pushentity(g_L, ent);

//     if (lua_pcall(g_L, 1, 0, 0) != 0)
//     {
//         lua_pop(g_L, 1);
//     }

//     RETURN_META(MRES_IGNORED);
// }

// /* pfnSaveSpawnParms() */
// void SaveSpawnParms(edict_t *ent)
// {
//     if (!g_L)
//         RETURN_META(MRES_IGNORED);

//     lua_getglobal(g_L, "MetaSaveSpawnParms");

//     if (!lua_isfunction(g_L, -1))
//     {
//         lua_pop(g_L, 1);
//         RETURN_META(MRES_IGNORED);
//     }

//     lua_pushentity(g_L, ent);

//     if (lua_pcall(g_L, 1, 0, 0) != 0)
//     {
//         lua_pop(g_L, 1);
//     }

//     RETURN_META(MRES_IGNORED);
// }

// /* pfnVecToYaw() */
// float VecToYaw(const float *rgflVector)
// {
//     if (!g_L)
//         RETURN_META_VALUE(MRES_IGNORED, 0.0f);

//     lua_getglobal(g_L, "MetaVecToYaw");

//     if (!lua_isfunction(g_L, -1))
//     {
//         lua_pop(g_L, 1);
//         RETURN_META_VALUE(MRES_IGNORED, 0.0f);
//     }

//     // Vector pointer
//     lua_pushlightuserdata(g_L, (void*)rgflVector);

//     if (lua_pcall(g_L, 1, 1, 0) != 0)
//     {
//         lua_pop(g_L, 1);
//         RETURN_META_VALUE(MRES_IGNORED, 0.0f);
//     }

//     // 如果 Lua 返回了数值，覆盖引擎计算的 Yaw
//     if (lua_isnumber(g_L, -1))
//     {
//         float ret = (float)lua_tonumber(g_L, -1);
//         lua_pop(g_L, 1);
//         RETURN_META_VALUE(MRES_SUPERCEDE, ret);
//     }

//     lua_pop(g_L, 1);
//     RETURN_META_VALUE(MRES_IGNORED, 0.0f);
// }
// /* pfnVecToAngles() */
// void VecToAngles(const float *rgflVectorIn, float *rgflVectorOut)
// {
//     if (!g_L)
//         RETURN_META(MRES_IGNORED);

//     lua_getglobal(g_L, "MetaVecToAngles");

//     if (!lua_isfunction(g_L, -1))
//     {
//         lua_pop(g_L, 1);
//         RETURN_META(MRES_IGNORED);
//     }

//     // Push: VectorIn ptr, VectorOut ptr
//     lua_pushlightuserdata(g_L, (void*)rgflVectorIn);
//     lua_pushlightuserdata(g_L, (void*)rgflVectorOut);

//     if (lua_pcall(g_L, 2, 0, 0) != 0)
//     {
//         lua_pop(g_L, 1);
//     }

//     RETURN_META(MRES_IGNORED);
// }

// /* pfnMoveToOrigin() */
// void MoveToOrigin(edict_t *ent, const float *pflGoal, float dist, int iMoveType)
// {
//     if (!g_L)
//         RETURN_META(MRES_IGNORED);

//     lua_getglobal(g_L, "MetaMoveToOrigin");

//     if (!lua_isfunction(g_L, -1))
//     {
//         lua_pop(g_L, 1);
//         RETURN_META(MRES_IGNORED);
//     }

//     lua_pushentity(g_L, ent);
//     lua_pushlightuserdata(g_L, (void*)pflGoal); // Goal Vector
//     lua_pushnumber(g_L, dist);
//     lua_pushinteger(g_L, iMoveType);

//     if (lua_pcall(g_L, 4, 0, 0) != 0)
//     {
//         lua_pop(g_L, 1);
//     }

//     RETURN_META(MRES_IGNORED);
// }

// /* pfnChangeYaw() */
// void ChangeYaw(edict_t *ent)
// {
//     if (!g_L)
//         RETURN_META(MRES_IGNORED);

//     lua_getglobal(g_L, "MetaChangeYaw");

//     if (!lua_isfunction(g_L, -1))
//     {
//         lua_pop(g_L, 1);
//         RETURN_META(MRES_IGNORED);
//     }

//     lua_pushentity(g_L, ent);

//     if (lua_pcall(g_L, 1, 0, 0) != 0)
//     {
//         lua_pop(g_L, 1);
//     }

//     RETURN_META(MRES_IGNORED);
// }

// /* pfnChangePitch() */
// void ChangePitch(edict_t *ent)
// {
//     if (!g_L)
//         RETURN_META(MRES_IGNORED);

//     lua_getglobal(g_L, "MetaChangePitch");

//     if (!lua_isfunction(g_L, -1))
//     {
//         lua_pop(g_L, 1);
//         RETURN_META(MRES_IGNORED);
//     }

//     lua_pushentity(g_L, ent);

//     if (lua_pcall(g_L, 1, 0, 0) != 0)
//     {
//         lua_pop(g_L, 1);
//     }

//     RETURN_META(MRES_IGNORED);
// }

// /* pfnFindEntityByString() */
// edict_t* FindEntityByString(edict_t *pEdictStartSearchAfter, const char *pszField, const char *pszValue)
// {
//     if (!g_L)
//         RETURN_META_VALUE(MRES_IGNORED, NULL);

//     lua_getglobal(g_L, "MetaFindEntityByString");

//     if (!lua_isfunction(g_L, -1))
//     {
//         lua_pop(g_L, 1);
//         RETURN_META_VALUE(MRES_IGNORED, NULL);
//     }

//     lua_pushentity(g_L, pEdictStartSearchAfter);
//     lua_pushstring(g_L, pszField);
//     lua_pushstring(g_L, pszValue);

//     if (lua_pcall(g_L, 3, 1, 0) != 0)
//     {
//         lua_pop(g_L, 1);
//         RETURN_META_VALUE(MRES_IGNORED, NULL);
//     }

//     // 如果 Lua 返回数字 (Index)，我们将其转换为 edict_t* 并返回
//     if (lua_isnumber(g_L, -1))
//     {
//         int entIndex = (int)lua_tointeger(g_L, -1);
//         lua_pop(g_L, 1);
        
//         edict_t* retEnt = NULL;
//         if (entIndex > 0)
//         {
//             retEnt = g_engfuncs.pfnPEntityOfEntIndex(entIndex);
//         }
        
//         // 如果 Index 是 0 或无效，retEnt 为 NULL，符合 FindEntityByString 的“未找到”语义
//         RETURN_META_VALUE(MRES_SUPERCEDE, retEnt);
//     }

//     lua_pop(g_L, 1);
//     RETURN_META_VALUE(MRES_IGNORED, NULL);
// }

// /* pfnGetEntityIllum() */
// int GetEntityIllum(edict_t* pEnt)
// {
//     if (!g_L)
//         RETURN_META_VALUE(MRES_IGNORED, 0);

//     lua_getglobal(g_L, "MetaGetEntityIllum");

//     if (!lua_isfunction(g_L, -1))
//     {
//         lua_pop(g_L, 1);
//         RETURN_META_VALUE(MRES_IGNORED, 0);
//     }

//     lua_pushentity(g_L, pEnt);

//     if (lua_pcall(g_L, 1, 1, 0) != 0)
//     {
//         lua_pop(g_L, 1);
//         RETURN_META_VALUE(MRES_IGNORED, 0);
//     }

//     if (lua_isnumber(g_L, -1))
//     {
//         int ret = (int)lua_tointeger(g_L, -1);
//         lua_pop(g_L, 1);
//         RETURN_META_VALUE(MRES_SUPERCEDE, ret);
//     }

//     lua_pop(g_L, 1);
//     RETURN_META_VALUE(MRES_IGNORED, 0);
// }

// /* pfnFindEntityInSphere() */
// edict_t* FindEntityInSphere(edict_t *pEdictStartSearchAfter, const float *org, float rad)
// {
//     if (!g_L)
//         RETURN_META_VALUE(MRES_IGNORED, NULL);

//     lua_getglobal(g_L, "MetaFindEntityInSphere");

//     if (!lua_isfunction(g_L, -1))
//     {
//         lua_pop(g_L, 1);
//         RETURN_META_VALUE(MRES_IGNORED, NULL);
//     }

//     lua_pushentity(g_L, pEdictStartSearchAfter);
//     lua_pushlightuserdata(g_L, (void*)org); // Origin Vector
//     lua_pushnumber(g_L, rad);

//     if (lua_pcall(g_L, 3, 1, 0) != 0)
//     {
//         lua_pop(g_L, 1);
//         RETURN_META_VALUE(MRES_IGNORED, NULL);
//     }

//     // Lua 返回找到的 Entity Index
//     if (lua_isnumber(g_L, -1))
//     {
//         int entIndex = (int)lua_tointeger(g_L, -1);
//         lua_pop(g_L, 1);

//         edict_t* retEnt = NULL;
//         if (entIndex > 0)
//             retEnt = g_engfuncs.pfnPEntityOfEntIndex(entIndex);

//         RETURN_META_VALUE(MRES_SUPERCEDE, retEnt);
//     }

//     lua_pop(g_L, 1);
//     RETURN_META_VALUE(MRES_IGNORED, NULL);
// }

// /* pfnFindClientInPVS() */
// edict_t* FindClientInPVS(edict_t *pEdict)
// {
//     if (!g_L)
//         RETURN_META_VALUE(MRES_IGNORED, NULL);

//     lua_getglobal(g_L, "MetaFindClientInPVS");

//     if (!lua_isfunction(g_L, -1))
//     {
//         lua_pop(g_L, 1);
//         RETURN_META_VALUE(MRES_IGNORED, NULL);
//     }

//     lua_pushentity(g_L, pEdict);

//     if (lua_pcall(g_L, 1, 1, 0) != 0)
//     {
//         lua_pop(g_L, 1);
//         RETURN_META_VALUE(MRES_IGNORED, NULL);
//     }

//     if (lua_isnumber(g_L, -1))
//     {
//         int entIndex = (int)lua_tointeger(g_L, -1);
//         lua_pop(g_L, 1);

//         edict_t* retEnt = NULL;
//         if (entIndex > 0)
//             retEnt = g_engfuncs.pfnPEntityOfEntIndex(entIndex);

//         RETURN_META_VALUE(MRES_SUPERCEDE, retEnt);
//     }

//     lua_pop(g_L, 1);
//     RETURN_META_VALUE(MRES_IGNORED, NULL);
// }

// /* pfnEntitiesInPVS() */
// edict_t* EntitiesInPVS(edict_t *pplayer)
// {
//     if (!g_L)
//         RETURN_META_VALUE(MRES_IGNORED, NULL);

//     lua_getglobal(g_L, "MetaEntitiesInPVS");

//     if (!lua_isfunction(g_L, -1))
//     {
//         lua_pop(g_L, 1);
//         RETURN_META_VALUE(MRES_IGNORED, NULL);
//     }

//     lua_pushentity(g_L, pplayer);

//     if (lua_pcall(g_L, 1, 1, 0) != 0)
//     {
//         lua_pop(g_L, 1);
//         RETURN_META_VALUE(MRES_IGNORED, NULL);
//     }

//     // 注意：EntitiesInPVS 在 HL 引擎中返回的是一个链表头或者第一个实体
//     if (lua_isnumber(g_L, -1))
//     {
//         int entIndex = (int)lua_tointeger(g_L, -1);
//         lua_pop(g_L, 1);

//         edict_t* retEnt = NULL;
//         if (entIndex > 0)
//             retEnt = g_engfuncs.pfnPEntityOfEntIndex(entIndex);

//         RETURN_META_VALUE(MRES_SUPERCEDE, retEnt);
//     }

//     lua_pop(g_L, 1);
//     RETURN_META_VALUE(MRES_IGNORED, NULL);
// }

// /* pfnMakeVectors() */
// void MakeVectors(const float *rgflVector)
// {
//     if (!g_L)
//         RETURN_META(MRES_IGNORED);

//     lua_getglobal(g_L, "MetaMakeVectors");

//     if (!lua_isfunction(g_L, -1))
//     {
//         lua_pop(g_L, 1);
//         RETURN_META(MRES_IGNORED);
//     }

//     lua_pushlightuserdata(g_L, (void*)rgflVector);

//     if (lua_pcall(g_L, 1, 0, 0) != 0)
//     {
//         lua_pop(g_L, 1);
//     }

//     RETURN_META(MRES_IGNORED);
// }

// /* pfnAngleVectors() */
// void AngleVectors(const float *rgflVector, float *forward, float *right, float *up)
// {
//     if (!g_L)
//         RETURN_META(MRES_IGNORED);

//     lua_getglobal(g_L, "MetaAngleVectors");

//     if (!lua_isfunction(g_L, -1))
//     {
//         lua_pop(g_L, 1);
//         RETURN_META(MRES_IGNORED);
//     }

//     // Push inputs and outputs as pointers
//     lua_pushlightuserdata(g_L, (void*)rgflVector);
//     lua_pushlightuserdata(g_L, (void*)forward);
//     lua_pushlightuserdata(g_L, (void*)right);
//     lua_pushlightuserdata(g_L, (void*)up);

//     if (lua_pcall(g_L, 4, 0, 0) != 0)
//     {
//         lua_pop(g_L, 1);
//     }

//     RETURN_META(MRES_IGNORED);
// }

// /* pfnCreateEntity() */
// edict_t* CreateEntity(void)
// {
//     if (!g_L)
//         RETURN_META_VALUE(MRES_IGNORED, NULL);

//     lua_getglobal(g_L, "MetaCreateEntity");

//     if (!lua_isfunction(g_L, -1))
//     {
//         lua_pop(g_L, 1);
//         RETURN_META_VALUE(MRES_IGNORED, NULL);
//     }

//     if (lua_pcall(g_L, 0, 1, 0) != 0)
//     {
//         lua_pop(g_L, 1);
//         RETURN_META_VALUE(MRES_IGNORED, NULL);
//     }

//     // Lua 返回 Entity Index
//     if (lua_isnumber(g_L, -1))
//     {
//         int entIndex = (int)lua_tointeger(g_L, -1);
//         lua_pop(g_L, 1);

//         edict_t* retEnt = NULL;
//         if (entIndex > 0)
//             retEnt = g_engfuncs.pfnPEntityOfEntIndex(entIndex);
        
//         RETURN_META_VALUE(MRES_SUPERCEDE, retEnt);
//     }

//     lua_pop(g_L, 1);
//     RETURN_META_VALUE(MRES_IGNORED, NULL);
// }

// /* pfnRemoveEntity() */
// void RemoveEntity(edict_t* e)
// {
//     if (!g_L)
//         RETURN_META(MRES_IGNORED);

//     lua_getglobal(g_L, "MetaRemoveEntity");

//     if (!lua_isfunction(g_L, -1))
//     {
//         lua_pop(g_L, 1);
//         RETURN_META(MRES_IGNORED);
//     }

//     lua_pushentity(g_L, e);

//     if (lua_pcall(g_L, 1, 0, 0) != 0)
//     {
//         lua_pop(g_L, 1);
//     }

//     RETURN_META(MRES_IGNORED);
// }

// /* pfnCreateNamedEntity() */
// edict_t* CreateNamedEntity(int className)
// {
//     if (!g_L)
//         RETURN_META_VALUE(MRES_IGNORED, NULL);

//     lua_getglobal(g_L, "MetaCreateNamedEntity");

//     if (!lua_isfunction(g_L, -1))
//     {
//         lua_pop(g_L, 1);
//         RETURN_META_VALUE(MRES_IGNORED, NULL);
//     }

//     // className 是字符串索引 (AllocString 的结果)
//     lua_pushinteger(g_L, className);

//     if (lua_pcall(g_L, 1, 1, 0) != 0)
//     {
//         lua_pop(g_L, 1);
//         RETURN_META_VALUE(MRES_IGNORED, NULL);
//     }

//     // Lua 返回 Entity Index
//     if (lua_isnumber(g_L, -1))
//     {
//         int entIndex = (int)lua_tointeger(g_L, -1);
//         lua_pop(g_L, 1);

//         edict_t* retEnt = NULL;
//         if (entIndex > 0)
//             retEnt = g_engfuncs.pfnPEntityOfEntIndex(entIndex);

//         RETURN_META_VALUE(MRES_SUPERCEDE, retEnt);
//     }

//     lua_pop(g_L, 1);
//     RETURN_META_VALUE(MRES_IGNORED, NULL);
// }

// /* pfnMakeStatic() */
// void MakeStatic(edict_t *ent)
// {
//     if (!g_L)
//         RETURN_META(MRES_IGNORED);

//     lua_getglobal(g_L, "MetaMakeStatic");

//     if (!lua_isfunction(g_L, -1))
//     {
//         lua_pop(g_L, 1);
//         RETURN_META(MRES_IGNORED);
//     }

//     lua_pushentity(g_L, ent);

//     if (lua_pcall(g_L, 1, 0, 0) != 0)
//     {
//         lua_pop(g_L, 1);
//     }

//     RETURN_META(MRES_IGNORED);
// }

// /* pfnEntIsOnFloor() */
// int EntIsOnFloor(edict_t *e)
// {
//     if (!g_L)
//         RETURN_META_VALUE(MRES_IGNORED, 0);

//     lua_getglobal(g_L, "MetaEntIsOnFloor");

//     if (!lua_isfunction(g_L, -1))
//     {
//         lua_pop(g_L, 1);
//         RETURN_META_VALUE(MRES_IGNORED, 0);
//     }

//     lua_pushentity(g_L, e);

//     if (lua_pcall(g_L, 1, 1, 0) != 0)
//     {
//         lua_pop(g_L, 1);
//         RETURN_META_VALUE(MRES_IGNORED, 0);
//     }

//     if (lua_isnumber(g_L, -1))
//     {
//         int ret = (int)lua_tointeger(g_L, -1);
//         lua_pop(g_L, 1);
//         RETURN_META_VALUE(MRES_SUPERCEDE, ret);
//     }

//     lua_pop(g_L, 1);
//     RETURN_META_VALUE(MRES_IGNORED, 0);
// }

// /* pfnDropToFloor() */
// int DropToFloor(edict_t* e)
// {
//     if (!g_L)
//         RETURN_META_VALUE(MRES_IGNORED, 0);

//     lua_getglobal(g_L, "MetaDropToFloor");

//     if (!lua_isfunction(g_L, -1))
//     {
//         lua_pop(g_L, 1);
//         RETURN_META_VALUE(MRES_IGNORED, 0);
//     }

//     lua_pushentity(g_L, e);

//     if (lua_pcall(g_L, 1, 1, 0) != 0)
//     {
//         lua_pop(g_L, 1);
//         RETURN_META_VALUE(MRES_IGNORED, 0);
//     }

//     if (lua_isnumber(g_L, -1))
//     {
//         int ret = (int)lua_tointeger(g_L, -1);
//         lua_pop(g_L, 1);
//         RETURN_META_VALUE(MRES_SUPERCEDE, ret);
//     }

//     lua_pop(g_L, 1);
//     RETURN_META_VALUE(MRES_IGNORED, 0);
// }

// /* pfnWalkMove() */
// int WalkMove(edict_t *ent, float yaw, float dist, int iMode)
// {
//     if (!g_L)
//         RETURN_META_VALUE(MRES_IGNORED, 0);

//     lua_getglobal(g_L, "MetaWalkMove");

//     if (!lua_isfunction(g_L, -1))
//     {
//         lua_pop(g_L, 1);
//         RETURN_META_VALUE(MRES_IGNORED, 0);
//     }

//     lua_pushentity(g_L, ent);
//     lua_pushnumber(g_L, yaw);
//     lua_pushnumber(g_L, dist);
//     lua_pushinteger(g_L, iMode);

//     if (lua_pcall(g_L, 4, 1, 0) != 0)
//     {
//         lua_pop(g_L, 1);
//         RETURN_META_VALUE(MRES_IGNORED, 0);
//     }

//     if (lua_isnumber(g_L, -1))
//     {
//         int ret = (int)lua_tointeger(g_L, -1);
//         lua_pop(g_L, 1);
//         RETURN_META_VALUE(MRES_SUPERCEDE, ret);
//     }

//     lua_pop(g_L, 1);
//     RETURN_META_VALUE(MRES_IGNORED, 0);
// }

// /* pfnSetOrigin() */
// void SetOrigin(edict_t *e, const float *rgflOrigin)
// {
//     if (!g_L)
//         RETURN_META(MRES_IGNORED);

//     lua_getglobal(g_L, "MetaSetOrigin");

//     if (!lua_isfunction(g_L, -1))
//     {
//         lua_pop(g_L, 1);
//         RETURN_META(MRES_IGNORED);
//     }

//     lua_pushentity(g_L, e);
//     lua_pushlightuserdata(g_L, (void*)rgflOrigin); // Vector ptr

//     if (lua_pcall(g_L, 2, 0, 0) != 0)
//     {
//         lua_pop(g_L, 1);
//     }

//     RETURN_META(MRES_IGNORED);
// }

// /* pfnEmitSound() */
// void EmitSound(edict_t *entity, int channel, const char *sample, float volume, float attenuation, int fFlags, int pitch)
// {
//     if (!g_L)
//         RETURN_META(MRES_IGNORED);

//     lua_getglobal(g_L, "MetaEmitSound");

//     if (!lua_isfunction(g_L, -1))
//     {
//         lua_pop(g_L, 1);
//         RETURN_META(MRES_IGNORED);
//     }

//     lua_pushentity(g_L, entity);
//     lua_pushinteger(g_L, channel);
//     lua_pushstring(g_L, sample);
//     lua_pushnumber(g_L, volume);
//     lua_pushnumber(g_L, attenuation);
//     lua_pushinteger(g_L, fFlags);
//     lua_pushinteger(g_L, pitch);

//     if (lua_pcall(g_L, 7, 0, 0) != 0)
//     {
//         lua_pop(g_L, 1);
//     }

//     RETURN_META(MRES_IGNORED);
// }
// /* pfnEmitAmbientSound() */
// void EmitAmbientSound(edict_t *entity, float *pos, const char *samp, float vol, float attenuation, int fFlags, int pitch)
// {
//     if (!g_L)
//         RETURN_META(MRES_IGNORED);

//     lua_getglobal(g_L, "MetaEmitAmbientSound");

//     if (!lua_isfunction(g_L, -1))
//     {
//         lua_pop(g_L, 1);
//         RETURN_META(MRES_IGNORED);
//     }

//     lua_pushentity(g_L, entity);
//     lua_pushlightuserdata(g_L, (void*)pos); // Vector ptr
//     lua_pushstring(g_L, samp);
//     lua_pushnumber(g_L, vol);
//     lua_pushnumber(g_L, attenuation);
//     lua_pushinteger(g_L, fFlags);
//     lua_pushinteger(g_L, pitch);

//     if (lua_pcall(g_L, 7, 0, 0) != 0)
//     {
//         lua_pop(g_L, 1);
//     }

//     RETURN_META(MRES_IGNORED);
// }

// /* pfnTraceLine() */
// void TraceLine(const float *v1, const float *v2, int fNoMonsters, edict_t *pentToSkip, TraceResult *ptr)
// {
//     if (!g_L)
//         RETURN_META(MRES_IGNORED);

//     lua_getglobal(g_L, "MetaTraceLine");

//     if (!lua_isfunction(g_L, -1))
//     {
//         lua_pop(g_L, 1);
//         RETURN_META(MRES_IGNORED);
//     }

//     lua_pushlightuserdata(g_L, (void*)v1);
//     lua_pushlightuserdata(g_L, (void*)v2);
//     lua_pushinteger(g_L, fNoMonsters);
//     lua_pushentity(g_L, pentToSkip);
//     lua_pushlightuserdata(g_L, (void*)ptr); // TraceResult ptr

//     if (lua_pcall(g_L, 5, 0, 0) != 0)
//     {
//         lua_pop(g_L, 1);
//     }

//     RETURN_META(MRES_IGNORED);
// }

// /* pfnTraceToss() */
// void TraceToss(edict_t* pent, edict_t* pentToIgnore, TraceResult *ptr)
// {
//     if (!g_L)
//         RETURN_META(MRES_IGNORED);

//     lua_getglobal(g_L, "MetaTraceToss");

//     if (!lua_isfunction(g_L, -1))
//     {
//         lua_pop(g_L, 1);
//         RETURN_META(MRES_IGNORED);
//     }

//     lua_pushentity(g_L, pent);
//     lua_pushentity(g_L, pentToIgnore);
//     lua_pushlightuserdata(g_L, (void*)ptr);

//     if (lua_pcall(g_L, 3, 0, 0) != 0)
//     {
//         lua_pop(g_L, 1);
//     }

//     RETURN_META(MRES_IGNORED);
// }

// /* pfnTraceMonsterHull() */
// int TraceMonsterHull(edict_t *pEdict, const float *v1, const float *v2, int fNoMonsters, edict_t *pentToSkip, TraceResult *ptr)
// {
//     if (!g_L)
//         RETURN_META_VALUE(MRES_IGNORED, 0);

//     lua_getglobal(g_L, "MetaTraceMonsterHull");

//     if (!lua_isfunction(g_L, -1))
//     {
//         lua_pop(g_L, 1);
//         RETURN_META_VALUE(MRES_IGNORED, 0);
//     }

//     lua_pushentity(g_L, pEdict);
//     lua_pushlightuserdata(g_L, (void*)v1);
//     lua_pushlightuserdata(g_L, (void*)v2);
//     lua_pushinteger(g_L, fNoMonsters);
//     lua_pushentity(g_L, pentToSkip);
//     lua_pushlightuserdata(g_L, (void*)ptr);

//     if (lua_pcall(g_L, 6, 1, 0) != 0)
//     {
//         lua_pop(g_L, 1);
//         RETURN_META_VALUE(MRES_IGNORED, 0);
//     }

//     if (lua_isnumber(g_L, -1))
//     {
//         int ret = (int)lua_tointeger(g_L, -1);
//         lua_pop(g_L, 1);
//         RETURN_META_VALUE(MRES_SUPERCEDE, ret);
//     }

//     lua_pop(g_L, 1);
//     RETURN_META_VALUE(MRES_IGNORED, 0);
// }

// /* pfnTraceHull() */
// void TraceHull(const float *v1, const float *v2, int fNoMonsters, int hullNumber, edict_t *pentToSkip, TraceResult *ptr)
// {
//     if (!g_L)
//         RETURN_META(MRES_IGNORED);

//     lua_getglobal(g_L, "MetaTraceHull");

//     if (!lua_isfunction(g_L, -1))
//     {
//         lua_pop(g_L, 1);
//         RETURN_META(MRES_IGNORED);
//     }

//     lua_pushlightuserdata(g_L, (void*)v1);
//     lua_pushlightuserdata(g_L, (void*)v2);
//     lua_pushinteger(g_L, fNoMonsters);
//     lua_pushinteger(g_L, hullNumber);
//     lua_pushentity(g_L, pentToSkip);
//     lua_pushlightuserdata(g_L, (void*)ptr);

//     if (lua_pcall(g_L, 6, 0, 0) != 0)
//     {
//         lua_pop(g_L, 1);
//     }

//     RETURN_META(MRES_IGNORED);
// }

// /* pfnTraceModel() */
// void TraceModel(const float *v1, const float *v2, int hullNumber, edict_t *pent, TraceResult *ptr)
// {
//     if (!g_L)
//         RETURN_META(MRES_IGNORED);

//     lua_getglobal(g_L, "MetaTraceModel");

//     if (!lua_isfunction(g_L, -1))
//     {
//         lua_pop(g_L, 1);
//         RETURN_META(MRES_IGNORED);
//     }

//     lua_pushlightuserdata(g_L, (void*)v1);
//     lua_pushlightuserdata(g_L, (void*)v2);
//     lua_pushinteger(g_L, hullNumber);
//     lua_pushentity(g_L, pent);
//     lua_pushlightuserdata(g_L, (void*)ptr);

//     if (lua_pcall(g_L, 5, 0, 0) != 0)
//     {
//         lua_pop(g_L, 1);
//     }

//     RETURN_META(MRES_IGNORED);
// }

// /* pfnTraceTexture() */
// const char *TraceTexture(edict_t *pTextureEntity, const float *v1, const float *v2)
// {
//     if (!g_L)
//         RETURN_META_VALUE(MRES_IGNORED, NULL);

//     lua_getglobal(g_L, "MetaTraceTexture");

//     if (!lua_isfunction(g_L, -1))
//     {
//         lua_pop(g_L, 1);
//         RETURN_META_VALUE(MRES_IGNORED, NULL);
//     }

//     lua_pushentity(g_L, pTextureEntity);
//     lua_pushlightuserdata(g_L, (void*)v1);
//     lua_pushlightuserdata(g_L, (void*)v2);

//     if (lua_pcall(g_L, 3, 1, 0) != 0)
//     {
//         lua_pop(g_L, 1);
//         RETURN_META_VALUE(MRES_IGNORED, NULL);
//     }

//     if (lua_isstring(g_L, -1))
//     {
//         // 静态缓冲区用于存储纹理名称返回给引擎
//         static char staticTextureName[64];
//         const char *ret = lua_tostring(g_L, -1);
        
//         strncpy(staticTextureName, ret, 63);
//         staticTextureName[63] = '\0';

//         lua_pop(g_L, 1);
//         RETURN_META_VALUE(MRES_SUPERCEDE, staticTextureName);
//     }

//     lua_pop(g_L, 1);
//     RETURN_META_VALUE(MRES_IGNORED, NULL);
// }

// /* pfnTraceSphere() */
// void TraceSphere(const float *v1, const float *v2, int fNoMonsters, float radius, edict_t *pentToSkip, TraceResult *ptr)
// {
//     if (!g_L)
//         RETURN_META(MRES_IGNORED);

//     lua_getglobal(g_L, "MetaTraceSphere");

//     if (!lua_isfunction(g_L, -1))
//     {
//         lua_pop(g_L, 1);
//         RETURN_META(MRES_IGNORED);
//     }

//     lua_pushlightuserdata(g_L, (void*)v1);
//     lua_pushlightuserdata(g_L, (void*)v2);
//     lua_pushinteger(g_L, fNoMonsters);
//     lua_pushnumber(g_L, radius);
//     lua_pushentity(g_L, pentToSkip);
//     lua_pushlightuserdata(g_L, (void*)ptr);

//     if (lua_pcall(g_L, 6, 0, 0) != 0)
//     {
//         lua_pop(g_L, 1);
//     }

//     RETURN_META(MRES_IGNORED);
// }

// /* pfnGetAimVector() */
// void GetAimVector(edict_t* ent, float speed, float *rgflReturn)
// {
//     if (!g_L)
//         RETURN_META(MRES_IGNORED);

//     lua_getglobal(g_L, "MetaGetAimVector");

//     if (!lua_isfunction(g_L, -1))
//     {
//         lua_pop(g_L, 1);
//         RETURN_META(MRES_IGNORED);
//     }

//     lua_pushentity(g_L, ent);
//     lua_pushnumber(g_L, speed);
//     lua_pushlightuserdata(g_L, (void*)rgflReturn); // Vector Output ptr

//     if (lua_pcall(g_L, 3, 0, 0) != 0)
//     {
//         lua_pop(g_L, 1);
//     }

//     RETURN_META(MRES_IGNORED);
// }

// /* pfnServerCommand() */
// // void ServerCommand(const char* str)
// // {
// //     if (!g_L)
// //         RETURN_META(MRES_IGNORED);

// //     lua_getglobal(g_L, "MetaServerCommand");

// //     if (!lua_isfunction(g_L, -1))
// //     {
// //         lua_pop(g_L, 1);
// //         RETURN_META(MRES_IGNORED);
// //     }

// //     lua_pushstring(g_L, str);

// //     if (lua_pcall(g_L, 1, 0, 0) != 0)
// //     {
// //         lua_pop(g_L, 1);
// //     }

// //     RETURN_META(MRES_IGNORED);
// // }

// /* pfnServerExecute() */
// void ServerExecute(void)
// {
//     if (!g_L)
//         RETURN_META(MRES_IGNORED);

//     lua_getglobal(g_L, "MetaServerExecute");

//     if (!lua_isfunction(g_L, -1))
//     {
//         lua_pop(g_L, 1);
//         RETURN_META(MRES_IGNORED);
//     }

//     if (lua_pcall(g_L, 0, 0, 0) != 0)
//     {
//         lua_pop(g_L, 1);
//     }

//     RETURN_META(MRES_IGNORED);
// }

// /* pfnClientCommand() (Engine Version) */
// // void engClientCommand(edict_t* pEdict, const char* szFmt, ...)
// // {
// //     if (!g_L)
// //         RETURN_META(MRES_IGNORED);

// //     lua_getglobal(g_L, "MetaEngClientCommand");

// //     if (!lua_isfunction(g_L, -1))
// //     {
// //         lua_pop(g_L, 1);
// //         RETURN_META(MRES_IGNORED);
// //     }

// //     lua_pushentity(g_L, pEdict);

// //     // 处理变参: 将格式化字符串和参数合并成最终的命令字符串
// //     static char command_buffer[1024];
// //     va_list argptr;
// //     va_start(argptr, szFmt);
// //     vsnprintf(command_buffer, sizeof(command_buffer), szFmt, argptr);
// //     va_end(argptr);

// //     lua_pushstring(g_L, command_buffer);

// //     if (lua_pcall(g_L, 2, 0, 0) != 0)
// //     {
// //         lua_pop(g_L, 1);
// //     }

// //     RETURN_META(MRES_IGNORED);
// // }

// /* pfnParticleEffect() */
// void ParticleEffect(const float *org, const float *dir, float color, float count)
// {
//     if (!g_L)
//         RETURN_META(MRES_IGNORED);

//     lua_getglobal(g_L, "MetaParticleEffect");

//     if (!lua_isfunction(g_L, -1))
//     {
//         lua_pop(g_L, 1);
//         RETURN_META(MRES_IGNORED);
//     }

//     lua_pushlightuserdata(g_L, (void*)org);
//     lua_pushlightuserdata(g_L, (void*)dir);
//     lua_pushnumber(g_L, color);
//     lua_pushnumber(g_L, count);

//     if (lua_pcall(g_L, 4, 0, 0) != 0)
//     {
//         lua_pop(g_L, 1);
//     }

//     RETURN_META(MRES_IGNORED);
// }

// /* pfnLightStyle() */
// void LightStyle(int style, const char* val)
// {
//     if (!g_L)
//         RETURN_META(MRES_IGNORED);

//     lua_getglobal(g_L, "MetaLightStyle");

//     if (!lua_isfunction(g_L, -1))
//     {
//         lua_pop(g_L, 1);
//         RETURN_META(MRES_IGNORED);
//     }

//     lua_pushinteger(g_L, style);
//     lua_pushstring(g_L, val);

//     if (lua_pcall(g_L, 2, 0, 0) != 0)
//     {
//         lua_pop(g_L, 1);
//     }

//     RETURN_META(MRES_IGNORED);
// }

// /* pfnDecalIndex() */
// int DecalIndex(const char *name)
// {
//     if (!g_L)
//         RETURN_META_VALUE(MRES_IGNORED, 0);

//     lua_getglobal(g_L, "MetaDecalIndex");

//     if (!lua_isfunction(g_L, -1))
//     {
//         lua_pop(g_L, 1);
//         RETURN_META_VALUE(MRES_IGNORED, 0);
//     }

//     lua_pushstring(g_L, name);

//     if (lua_pcall(g_L, 1, 1, 0) != 0)
//     {
//         lua_pop(g_L, 1);
//         RETURN_META_VALUE(MRES_IGNORED, 0);
//     }

//     if (lua_isnumber(g_L, -1))
//     {
//         int ret = (int)lua_tointeger(g_L, -1);
//         lua_pop(g_L, 1);
//         RETURN_META_VALUE(MRES_SUPERCEDE, ret);
//     }

//     lua_pop(g_L, 1);
//     RETURN_META_VALUE(MRES_IGNORED, 0);
// }

// /* pfnPointContents() */
// int PointContents(const float *rgflVector)
// {
//     if (!g_L)
//         RETURN_META_VALUE(MRES_IGNORED, 0);

//     lua_getglobal(g_L, "MetaPointContents");

//     if (!lua_isfunction(g_L, -1))
//     {
//         lua_pop(g_L, 1);
//         RETURN_META_VALUE(MRES_IGNORED, 0);
//     }

//     lua_pushlightuserdata(g_L, (void*)rgflVector);

//     if (lua_pcall(g_L, 1, 1, 0) != 0)
//     {
//         lua_pop(g_L, 1);
//         RETURN_META_VALUE(MRES_IGNORED, 0);
//     }

//     if (lua_isnumber(g_L, -1))
//     {
//         int ret = (int)lua_tointeger(g_L, -1);
//         lua_pop(g_L, 1);
//         RETURN_META_VALUE(MRES_SUPERCEDE, ret);
//     }

//     lua_pop(g_L, 1);
//     RETURN_META_VALUE(MRES_IGNORED, 0);
// }

// /* pfnMessageBegin() */
// void MessageBegin(int msg_dest, int msg_type, const float *pOrigin, edict_t *ed)
// {
//     if (!g_L)
//         RETURN_META(MRES_IGNORED);

//     lua_getglobal(g_L, "MetaMessageBegin");

//     if (!lua_isfunction(g_L, -1))
//     {
//         lua_pop(g_L, 1);
//         RETURN_META(MRES_IGNORED);
//     }

//     lua_pushinteger(g_L, msg_dest);
//     lua_pushinteger(g_L, msg_type);
    
//     // pOrigin 可能是 NULL，传递 NULL 指针给 Lua
//     lua_pushlightuserdata(g_L, (void*)pOrigin);
    
//     lua_pushentity(g_L, ed);

//     if (lua_pcall(g_L, 4, 0, 0) != 0)
//     {
//         lua_pop(g_L, 1);
//     }

//     RETURN_META(MRES_IGNORED);
// }

// /* pfnMessageEnd() */
// void MessageEnd(void)
// {
//     if (!g_L)
//         RETURN_META(MRES_IGNORED);

//     lua_getglobal(g_L, "MetaMessageEnd");

//     if (!lua_isfunction(g_L, -1))
//     {
//         lua_pop(g_L, 1);
//         RETURN_META(MRES_IGNORED);
//     }

//     if (lua_pcall(g_L, 0, 0, 0) != 0)
//     {
//         lua_pop(g_L, 1);
//     }

//     RETURN_META(MRES_IGNORED);
// }

// /* pfnWriteByte() */
// void WriteByte(int iValue)
// {
//     if (!g_L)
//         RETURN_META(MRES_IGNORED);

//     lua_getglobal(g_L, "MetaWriteByte");

//     if (!lua_isfunction(g_L, -1))
//     {
//         lua_pop(g_L, 1);
//         RETURN_META(MRES_IGNORED);
//     }

//     lua_pushinteger(g_L, iValue);

//     if (lua_pcall(g_L, 1, 0, 0) != 0)
//     {
//         lua_pop(g_L, 1);
//     }

//     RETURN_META(MRES_IGNORED);
// }

// /* pfnWriteChar() */
// void WriteChar(int iValue)
// {
//     if (!g_L)
//         RETURN_META(MRES_IGNORED);

//     lua_getglobal(g_L, "MetaWriteChar");

//     if (!lua_isfunction(g_L, -1))
//     {
//         lua_pop(g_L, 1);
//         RETURN_META(MRES_IGNORED);
//     }

//     lua_pushinteger(g_L, iValue);

//     if (lua_pcall(g_L, 1, 0, 0) != 0)
//     {
//         lua_pop(g_L, 1);
//     }

//     RETURN_META(MRES_IGNORED);
// }

// /* pfnWriteShort() */
// void WriteShort(int iValue)
// {
//     if (!g_L)
//         RETURN_META(MRES_IGNORED);

//     lua_getglobal(g_L, "MetaWriteShort");

//     if (!lua_isfunction(g_L, -1))
//     {
//         lua_pop(g_L, 1);
//         RETURN_META(MRES_IGNORED);
//     }

//     lua_pushinteger(g_L, iValue);

//     if (lua_pcall(g_L, 1, 0, 0) != 0)
//     {
//         lua_pop(g_L, 1);
//     }

//     RETURN_META(MRES_IGNORED);
// }

// /* pfnWriteLong() */
// void WriteLong(int iValue)
// {
//     if (!g_L)
//         RETURN_META(MRES_IGNORED);

//     lua_getglobal(g_L, "MetaWriteLong");

//     if (!lua_isfunction(g_L, -1))
//     {
//         lua_pop(g_L, 1);
//         RETURN_META(MRES_IGNORED);
//     }

//     lua_pushinteger(g_L, iValue);

//     if (lua_pcall(g_L, 1, 0, 0) != 0)
//     {
//         lua_pop(g_L, 1);
//     }

//     RETURN_META(MRES_IGNORED);
// }

// /* pfnWriteAngle() */
// void WriteAngle(float flValue)
// {
//     if (!g_L)
//         RETURN_META(MRES_IGNORED);

//     lua_getglobal(g_L, "MetaWriteAngle");

//     if (!lua_isfunction(g_L, -1))
//     {
//         lua_pop(g_L, 1);
//         RETURN_META(MRES_IGNORED);
//     }

//     lua_pushnumber(g_L, flValue);

//     if (lua_pcall(g_L, 1, 0, 0) != 0)
//     {
//         lua_pop(g_L, 1);
//     }

//     RETURN_META(MRES_IGNORED);
// }

// /* pfnWriteCoord() */
// void WriteCoord(float flValue)
// {
//     if (!g_L)
//         RETURN_META(MRES_IGNORED);

//     lua_getglobal(g_L, "MetaWriteCoord");

//     if (!lua_isfunction(g_L, -1))
//     {
//         lua_pop(g_L, 1);
//         RETURN_META(MRES_IGNORED);
//     }

//     lua_pushnumber(g_L, flValue);

//     if (lua_pcall(g_L, 1, 0, 0) != 0)
//     {
//         lua_pop(g_L, 1);
//     }

//     RETURN_META(MRES_IGNORED);
// }

// /* pfnWriteString() */
// void WriteString(const char *sz)
// {
//     if (!g_L)
//         RETURN_META(MRES_IGNORED);

//     lua_getglobal(g_L, "MetaWriteString");

//     if (!lua_isfunction(g_L, -1))
//     {
//         lua_pop(g_L, 1);
//         RETURN_META(MRES_IGNORED);
//     }

//     lua_pushstring(g_L, sz);

//     if (lua_pcall(g_L, 1, 0, 0) != 0)
//     {
//         lua_pop(g_L, 1);
//     }

//     RETURN_META(MRES_IGNORED);
// }

// /* pfnWriteEntity() */
// void WriteEntity(int iValue)
// {
//     if (!g_L)
//         RETURN_META(MRES_IGNORED);

//     lua_getglobal(g_L, "MetaWriteEntity");

//     if (!lua_isfunction(g_L, -1))
//     {
//         lua_pop(g_L, 1);
//         RETURN_META(MRES_IGNORED);
//     }

//     lua_pushinteger(g_L, iValue);

//     if (lua_pcall(g_L, 1, 0, 0) != 0)
//     {
//         lua_pop(g_L, 1);
//     }

//     RETURN_META(MRES_IGNORED);
// }

// /* pfnCVarRegister() */
// void CVarRegister(cvar_t *pCvar)
// {
//     if (!g_L)
//         RETURN_META(MRES_IGNORED);

//     lua_getglobal(g_L, "MetaCVarRegister");

//     if (!lua_isfunction(g_L, -1))
//     {
//         lua_pop(g_L, 1);
//         RETURN_META(MRES_IGNORED);
//     }

//     lua_pushlightuserdata(g_L, (void*)pCvar);

//     if (lua_pcall(g_L, 1, 0, 0) != 0)
//     {
//         lua_pop(g_L, 1);
//     }

//     RETURN_META(MRES_IGNORED);
// }

// /* pfnCVarGetFloat() */
// float CVarGetFloat(const char *szVarName)
// {
//     if (!g_L)
//         RETURN_META_VALUE(MRES_IGNORED, 0.0f);

//     lua_getglobal(g_L, "MetaCVarGetFloat");

//     if (!lua_isfunction(g_L, -1))
//     {
//         lua_pop(g_L, 1);
//         RETURN_META_VALUE(MRES_IGNORED, 0.0f);
//     }

//     lua_pushstring(g_L, szVarName);

//     if (lua_pcall(g_L, 1, 1, 0) != 0)
//     {
//         lua_pop(g_L, 1);
//         RETURN_META_VALUE(MRES_IGNORED, 0.0f);
//     }

//     if (lua_isnumber(g_L, -1))
//     {
//         float ret = (float)lua_tonumber(g_L, -1);
//         lua_pop(g_L, 1);
//         RETURN_META_VALUE(MRES_SUPERCEDE, ret);
//     }

//     lua_pop(g_L, 1);
//     RETURN_META_VALUE(MRES_IGNORED, 0.0f);
// }
// /* pfnCVarGetString() */
// const char* CVarGetString(const char *szVarName)
// {
//     if (!g_L)
//         RETURN_META_VALUE(MRES_IGNORED, NULL);

//     lua_getglobal(g_L, "MetaCVarGetString");

//     if (!lua_isfunction(g_L, -1))
//     {
//         lua_pop(g_L, 1);
//         RETURN_META_VALUE(MRES_IGNORED, NULL);
//     }

//     lua_pushstring(g_L, szVarName);

//     if (lua_pcall(g_L, 1, 1, 0) != 0)
//     {
//         lua_pop(g_L, 1);
//         RETURN_META_VALUE(MRES_IGNORED, NULL);
//     }

//     if (lua_isstring(g_L, -1))
//     {
//         static char staticBuf[256];
//         const char *ret = lua_tostring(g_L, -1);
        
//         strncpy(staticBuf, ret, 255);
//         staticBuf[255] = '\0';

//         lua_pop(g_L, 1);
//         RETURN_META_VALUE(MRES_SUPERCEDE, staticBuf);
//     }

//     lua_pop(g_L, 1);
//     RETURN_META_VALUE(MRES_IGNORED, NULL);
// }

// /* pfnCVarSetFloat() */
// void CVarSetFloat(const char *szVarName, float flValue)
// {
//     if (!g_L)
//         RETURN_META(MRES_IGNORED);

//     lua_getglobal(g_L, "MetaCVarSetFloat");

//     if (!lua_isfunction(g_L, -1))
//     {
//         lua_pop(g_L, 1);
//         RETURN_META(MRES_IGNORED);
//     }

//     lua_pushstring(g_L, szVarName);
//     lua_pushnumber(g_L, flValue);

//     if (lua_pcall(g_L, 2, 0, 0) != 0)
//     {
//         lua_pop(g_L, 1);
//     }

//     RETURN_META(MRES_IGNORED);
// }

// /* pfnCVarSetString() */
// void CVarSetString(const char *szVarName, const char *szValue)
// {
//     if (!g_L)
//         RETURN_META(MRES_IGNORED);

//     lua_getglobal(g_L, "MetaCVarSetString");

//     if (!lua_isfunction(g_L, -1))
//     {
//         lua_pop(g_L, 1);
//         RETURN_META(MRES_IGNORED);
//     }

//     lua_pushstring(g_L, szVarName);
//     lua_pushstring(g_L, szValue);

//     if (lua_pcall(g_L, 2, 0, 0) != 0)
//     {
//         lua_pop(g_L, 1);
//     }

//     RETURN_META(MRES_IGNORED);
// }

// /* pfnAlertMessage() - 变参函数 */
// void AlertMessage(ALERT_TYPE atype, const char *szFmt, ...)
// {
//     if (!g_L)
//         RETURN_META(MRES_IGNORED);

//     lua_getglobal(g_L, "MetaAlertMessage");

//     if (!lua_isfunction(g_L, -1))
//     {
//         lua_pop(g_L, 1);
//         RETURN_META(MRES_IGNORED);
//     }

//     lua_pushinteger(g_L, atype);

//     // 格式化字符串
//     static char buffer[2048];
//     va_list argptr;
//     va_start(argptr, szFmt);
//     vsnprintf(buffer, sizeof(buffer), szFmt, argptr);
//     va_end(argptr);

//     lua_pushstring(g_L, buffer);

//     if (lua_pcall(g_L, 2, 0, 0) != 0)
//     {
//         lua_pop(g_L, 1);
//     }

//     RETURN_META(MRES_IGNORED);
// }

// /* pfnEngineFprintf() - 变参函数 */
// void EngineFprintf(void *pfile, const char *szFmt, ...)
// {
//     if (!g_L)
//         RETURN_META(MRES_IGNORED);

//     lua_getglobal(g_L, "MetaEngineFprintf");

//     if (!lua_isfunction(g_L, -1))
//     {
//         lua_pop(g_L, 1);
//         RETURN_META(MRES_IGNORED);
//     }

//     lua_pushlightuserdata(g_L, pfile);

//     // 格式化字符串
//     static char buffer[2048];
//     va_list argptr;
//     va_start(argptr, szFmt);
//     vsnprintf(buffer, sizeof(buffer), szFmt, argptr);
//     va_end(argptr);

//     lua_pushstring(g_L, buffer);

//     if (lua_pcall(g_L, 2, 0, 0) != 0)
//     {
//         lua_pop(g_L, 1);
//     }

//     RETURN_META(MRES_IGNORED);
// }

// /* pfnPvAllocEntPrivateData() */
// void* PvAllocEntPrivateData(edict_t *pEdict, int32 cb)
// {
//     if (!g_L)
//         RETURN_META_VALUE(MRES_IGNORED, NULL);

//     lua_getglobal(g_L, "MetaPvAllocEntPrivateData");

//     if (!lua_isfunction(g_L, -1))
//     {
//         lua_pop(g_L, 1);
//         RETURN_META_VALUE(MRES_IGNORED, NULL);
//     }

//     lua_pushentity(g_L, pEdict);
//     lua_pushinteger(g_L, cb);

//     if (lua_pcall(g_L, 2, 1, 0) != 0)
//     {
//         lua_pop(g_L, 1);
//         RETURN_META_VALUE(MRES_IGNORED, NULL);
//     }

//     // 如果 Lua 返回了 userdata 或 lightuserdata，我们可以覆盖
//     if (lua_isuserdata(g_L, -1))
//     {
//         void* ret = lua_touserdata(g_L, -1);
//         lua_pop(g_L, 1);
//         RETURN_META_VALUE(MRES_SUPERCEDE, ret);
//     }

//     lua_pop(g_L, 1);
//     RETURN_META_VALUE(MRES_IGNORED, NULL);
// }

// /* pfnPvEntPrivateData() */
// void* PvEntPrivateData(edict_t *pEdict)
// {
//     if (!g_L)
//         RETURN_META_VALUE(MRES_IGNORED, NULL);

//     lua_getglobal(g_L, "MetaPvEntPrivateData");

//     if (!lua_isfunction(g_L, -1))
//     {
//         lua_pop(g_L, 1);
//         RETURN_META_VALUE(MRES_IGNORED, NULL);
//     }

//     lua_pushentity(g_L, pEdict);

//     if (lua_pcall(g_L, 1, 1, 0) != 0)
//     {
//         lua_pop(g_L, 1);
//         RETURN_META_VALUE(MRES_IGNORED, NULL);
//     }

//     if (lua_isuserdata(g_L, -1))
//     {
//         void* ret = lua_touserdata(g_L, -1);
//         lua_pop(g_L, 1);
//         RETURN_META_VALUE(MRES_SUPERCEDE, ret);
//     }

//     lua_pop(g_L, 1);
//     RETURN_META_VALUE(MRES_IGNORED, NULL);
// }

// /* pfnFreeEntPrivateData() */
// void FreeEntPrivateData(edict_t *pEdict)
// {
//     if (!g_L)
//         RETURN_META(MRES_IGNORED);

//     lua_getglobal(g_L, "MetaFreeEntPrivateData");

//     if (!lua_isfunction(g_L, -1))
//     {
//         lua_pop(g_L, 1);
//         RETURN_META(MRES_IGNORED);
//     }

//     lua_pushentity(g_L, pEdict);

//     if (lua_pcall(g_L, 1, 0, 0) != 0)
//     {
//         lua_pop(g_L, 1);
//     }

//     RETURN_META(MRES_IGNORED);
// }

// /* pfnSzFromIndex() */
// const char* SzFromIndex(int iString)
// {
//     if (!g_L)
//         RETURN_META_VALUE(MRES_IGNORED, NULL);

//     lua_getglobal(g_L, "MetaSzFromIndex");

//     if (!lua_isfunction(g_L, -1))
//     {
//         lua_pop(g_L, 1);
//         RETURN_META_VALUE(MRES_IGNORED, NULL);
//     }

//     lua_pushinteger(g_L, iString);

//     if (lua_pcall(g_L, 1, 1, 0) != 0)
//     {
//         lua_pop(g_L, 1);
//         RETURN_META_VALUE(MRES_IGNORED, NULL);
//     }

//     if (lua_isstring(g_L, -1))
//     {
//         static char staticBuf[256];
//         const char *ret = lua_tostring(g_L, -1);
        
//         strncpy(staticBuf, ret, 255);
//         staticBuf[255] = '\0';

//         lua_pop(g_L, 1);
//         RETURN_META_VALUE(MRES_SUPERCEDE, staticBuf);
//     }

//     lua_pop(g_L, 1);
//     RETURN_META_VALUE(MRES_IGNORED, NULL);
// }

// /* pfnAllocString() */
// int AllocString(const char *szValue)
// {
//     if (!g_L)
//         RETURN_META_VALUE(MRES_IGNORED, 0);

//     lua_getglobal(g_L, "MetaAllocString");

//     if (!lua_isfunction(g_L, -1))
//     {
//         lua_pop(g_L, 1);
//         RETURN_META_VALUE(MRES_IGNORED, 0);
//     }

//     lua_pushstring(g_L, szValue);

//     if (lua_pcall(g_L, 1, 1, 0) != 0)
//     {
//         lua_pop(g_L, 1);
//         RETURN_META_VALUE(MRES_IGNORED, 0);
//     }

//     if (lua_isnumber(g_L, -1))
//     {
//         int ret = (int)lua_tointeger(g_L, -1);
//         lua_pop(g_L, 1);
//         RETURN_META_VALUE(MRES_SUPERCEDE, ret);
//     }

//     lua_pop(g_L, 1);
//     RETURN_META_VALUE(MRES_IGNORED, 0);
// }
// /* pfnGetVarsOfEnt() */
// struct entvars_s* GetVarsOfEnt(edict_t *pEdict)
// {
//     if (!g_L)
//         RETURN_META_VALUE(MRES_IGNORED, NULL);

//     lua_getglobal(g_L, "MetaGetVarsOfEnt");

//     if (!lua_isfunction(g_L, -1))
//     {
//         lua_pop(g_L, 1);
//         RETURN_META_VALUE(MRES_IGNORED, NULL);
//     }

//     lua_pushentity(g_L, pEdict);

//     if (lua_pcall(g_L, 1, 1, 0) != 0)
//     {
//         lua_pop(g_L, 1);
//         RETURN_META_VALUE(MRES_IGNORED, NULL);
//     }

//     // 如果 Lua 返回了 userdata (指针)，覆盖返回值
//     if (lua_isuserdata(g_L, -1))
//     {
//         struct entvars_s* ret = (struct entvars_s*)lua_touserdata(g_L, -1);
//         lua_pop(g_L, 1);
//         RETURN_META_VALUE(MRES_SUPERCEDE, ret);
//     }

//     lua_pop(g_L, 1);
//     RETURN_META_VALUE(MRES_IGNORED, NULL);
// }

// /* pfnPEntityOfEntOffset() */
// edict_t* PEntityOfEntOffset(int iEntOffset)
// {
//     if (!g_L)
//         RETURN_META_VALUE(MRES_IGNORED, NULL);

//     lua_getglobal(g_L, "MetaPEntityOfEntOffset");

//     if (!lua_isfunction(g_L, -1))
//     {
//         lua_pop(g_L, 1);
//         RETURN_META_VALUE(MRES_IGNORED, NULL);
//     }

//     lua_pushinteger(g_L, iEntOffset);

//     if (lua_pcall(g_L, 1, 1, 0) != 0)
//     {
//         lua_pop(g_L, 1);
//         RETURN_META_VALUE(MRES_IGNORED, NULL);
//     }

//     if (lua_isnumber(g_L, -1))
//     {
//         int entIndex = (int)lua_tointeger(g_L, -1);
//         lua_pop(g_L, 1);

//         edict_t* retEnt = NULL;
//         if (entIndex > 0)
//             retEnt = g_engfuncs.pfnPEntityOfEntIndex(entIndex);

//         RETURN_META_VALUE(MRES_SUPERCEDE, retEnt);
//     }

//     lua_pop(g_L, 1);
//     RETURN_META_VALUE(MRES_IGNORED, NULL);
// }

// /* pfnEntOffsetOfPEntity() */
// int EntOffsetOfPEntity(const edict_t *pEdict)
// {
//     if (!g_L)
//         RETURN_META_VALUE(MRES_IGNORED, 0);

//     lua_getglobal(g_L, "MetaEntOffsetOfPEntity");

//     if (!lua_isfunction(g_L, -1))
//     {
//         lua_pop(g_L, 1);
//         RETURN_META_VALUE(MRES_IGNORED, 0);
//     }

//     lua_pushentity(g_L, (edict_t*)pEdict);

//     if (lua_pcall(g_L, 1, 1, 0) != 0)
//     {
//         lua_pop(g_L, 1);
//         RETURN_META_VALUE(MRES_IGNORED, 0);
//     }

//     if (lua_isnumber(g_L, -1))
//     {
//         int ret = (int)lua_tointeger(g_L, -1);
//         lua_pop(g_L, 1);
//         RETURN_META_VALUE(MRES_SUPERCEDE, ret);
//     }

//     lua_pop(g_L, 1);
//     RETURN_META_VALUE(MRES_IGNORED, 0);
// }

// /* pfnIndexOfEdict() */
// int IndexOfEdict(const edict_t *pEdict)
// {
//     if (!g_L)
//         RETURN_META_VALUE(MRES_IGNORED, 0);

//     lua_getglobal(g_L, "MetaIndexOfEdict");

//     if (!lua_isfunction(g_L, -1))
//     {
//         lua_pop(g_L, 1);
//         RETURN_META_VALUE(MRES_IGNORED, 0);
//     }

//     lua_pushentity(g_L, (edict_t*)pEdict);

//     if (lua_pcall(g_L, 1, 1, 0) != 0)
//     {
//         lua_pop(g_L, 1);
//         RETURN_META_VALUE(MRES_IGNORED, 0);
//     }

//     if (lua_isnumber(g_L, -1))
//     {
//         int ret = (int)lua_tointeger(g_L, -1);
//         lua_pop(g_L, 1);
//         RETURN_META_VALUE(MRES_SUPERCEDE, ret);
//     }

//     lua_pop(g_L, 1);
//     RETURN_META_VALUE(MRES_IGNORED, 0);
// }

// /* pfnPEntityOfEntIndex() */
// edict_t* PEntityOfEntIndex(int iEntIndex)
// {
//     if (!g_L)
//         RETURN_META_VALUE(MRES_IGNORED, NULL);

//     lua_getglobal(g_L, "MetaPEntityOfEntIndex");

//     if (!lua_isfunction(g_L, -1))
//     {
//         lua_pop(g_L, 1);
//         RETURN_META_VALUE(MRES_IGNORED, NULL);
//     }

//     lua_pushinteger(g_L, iEntIndex);

//     if (lua_pcall(g_L, 1, 1, 0) != 0)
//     {
//         lua_pop(g_L, 1);
//         RETURN_META_VALUE(MRES_IGNORED, NULL);
//     }

//     if (lua_isnumber(g_L, -1))
//     {
//         int entIndex = (int)lua_tointeger(g_L, -1);
//         lua_pop(g_L, 1);

//         edict_t* retEnt = NULL;
//         if (entIndex > 0)
//             retEnt = g_engfuncs.pfnPEntityOfEntIndex(entIndex);

//         RETURN_META_VALUE(MRES_SUPERCEDE, retEnt);
//     }

//     lua_pop(g_L, 1);
//     RETURN_META_VALUE(MRES_IGNORED, NULL);
// }

// /* pfnFindEntityByVars() */
// edict_t* FindEntityByVars(struct entvars_s* pvars)
// {
//     if (!g_L)
//         RETURN_META_VALUE(MRES_IGNORED, NULL);

//     lua_getglobal(g_L, "MetaFindEntityByVars");

//     if (!lua_isfunction(g_L, -1))
//     {
//         lua_pop(g_L, 1);
//         RETURN_META_VALUE(MRES_IGNORED, NULL);
//     }

//     lua_pushlightuserdata(g_L, pvars);

//     if (lua_pcall(g_L, 1, 1, 0) != 0)
//     {
//         lua_pop(g_L, 1);
//         RETURN_META_VALUE(MRES_IGNORED, NULL);
//     }

//     // Lua 返回 Index -> edict_t*
//     if (lua_isnumber(g_L, -1))
//     {
//         int entIndex = (int)lua_tointeger(g_L, -1);
//         lua_pop(g_L, 1);

//         edict_t* retEnt = NULL;
//         if (entIndex > 0)
//             retEnt = g_engfuncs.pfnPEntityOfEntIndex(entIndex);

//         RETURN_META_VALUE(MRES_SUPERCEDE, retEnt);
//     }

//     lua_pop(g_L, 1);
//     RETURN_META_VALUE(MRES_IGNORED, NULL);
// }

// /* pfnGetModelPtr() */
// void* GetModelPtr(edict_t* pEdict)
// {
//     if (!g_L)
//         RETURN_META_VALUE(MRES_IGNORED, NULL);

//     lua_getglobal(g_L, "MetaGetModelPtr");

//     if (!lua_isfunction(g_L, -1))
//     {
//         lua_pop(g_L, 1);
//         RETURN_META_VALUE(MRES_IGNORED, NULL);
//     }

//     lua_pushentity(g_L, pEdict);

//     if (lua_pcall(g_L, 1, 1, 0) != 0)
//     {
//         lua_pop(g_L, 1);
//         RETURN_META_VALUE(MRES_IGNORED, NULL);
//     }

//     // Lua 返回指针
//     if (lua_isuserdata(g_L, -1))
//     {
//         void* ret = lua_touserdata(g_L, -1);
//         lua_pop(g_L, 1);
//         RETURN_META_VALUE(MRES_SUPERCEDE, ret);
//     }

//     lua_pop(g_L, 1);
//     RETURN_META_VALUE(MRES_IGNORED, NULL);
// }

// /* pfnRegUserMsg() */
// int RegUserMsg(const char *pszName, int iSize)
// {
//     if (!g_L)
//         RETURN_META_VALUE(MRES_IGNORED, 0);

//     lua_getglobal(g_L, "MetaRegUserMsg");

//     if (!lua_isfunction(g_L, -1))
//     {
//         lua_pop(g_L, 1);
//         RETURN_META_VALUE(MRES_IGNORED, 0);
//     }

//     lua_pushstring(g_L, pszName);
//     lua_pushinteger(g_L, iSize);

//     if (lua_pcall(g_L, 2, 1, 0) != 0)
//     {
//         lua_pop(g_L, 1);
//         RETURN_META_VALUE(MRES_IGNORED, 0);
//     }

//     if (lua_isnumber(g_L, -1))
//     {
//         int ret = (int)lua_tointeger(g_L, -1);
//         lua_pop(g_L, 1);
//         RETURN_META_VALUE(MRES_SUPERCEDE, ret);
//     }

//     lua_pop(g_L, 1);
//     RETURN_META_VALUE(MRES_IGNORED, 0);
// }

// /* pfnAnimationAutomove() */
// void AnimationAutomove(const edict_t* pEdict, float flTime)
// {
//     if (!g_L)
//         RETURN_META(MRES_IGNORED);

//     lua_getglobal(g_L, "MetaAnimationAutomove");

//     if (!lua_isfunction(g_L, -1))
//     {
//         lua_pop(g_L, 1);
//         RETURN_META(MRES_IGNORED);
//     }

//     lua_pushentity(g_L, (edict_t*)pEdict);
//     lua_pushnumber(g_L, flTime);

//     if (lua_pcall(g_L, 2, 0, 0) != 0)
//     {
//         lua_pop(g_L, 1);
//     }

//     RETURN_META(MRES_IGNORED);
// }

// /* pfnGetBonePosition() */
// void GetBonePosition(const edict_t* pEdict, int iBone, float *rgflOrigin, float *rgflAngles)
// {
//     if (!g_L)
//         RETURN_META(MRES_IGNORED);

//     lua_getglobal(g_L, "MetaGetBonePosition");

//     if (!lua_isfunction(g_L, -1))
//     {
//         lua_pop(g_L, 1);
//         RETURN_META(MRES_IGNORED);
//     }

//     lua_pushentity(g_L, (edict_t*)pEdict);
//     lua_pushinteger(g_L, iBone);
//     lua_pushlightuserdata(g_L, (void*)rgflOrigin); // Vector Output
//     lua_pushlightuserdata(g_L, (void*)rgflAngles); // Vector Output

//     if (lua_pcall(g_L, 4, 0, 0) != 0)
//     {
//         lua_pop(g_L, 1);
//     }

//     RETURN_META(MRES_IGNORED);
// }
// /* pfnFunctionFromName() */
// uint32 FunctionFromName(const char *pName)
// {
//     if (!g_L)
//         RETURN_META_VALUE(MRES_IGNORED, 0);

//     lua_getglobal(g_L, "MetaFunctionFromName");

//     if (!lua_isfunction(g_L, -1))
//     {
//         lua_pop(g_L, 1);
//         RETURN_META_VALUE(MRES_IGNORED, 0);
//     }

//     lua_pushstring(g_L, pName);

//     if (lua_pcall(g_L, 1, 1, 0) != 0)
//     {
//         lua_pop(g_L, 1);
//         RETURN_META_VALUE(MRES_IGNORED, 0);
//     }

//     if (lua_isnumber(g_L, -1))
//     {
//         uint32 ret = (uint32)lua_tointeger(g_L, -1);
//         lua_pop(g_L, 1);
//         RETURN_META_VALUE(MRES_SUPERCEDE, ret);
//     }

//     lua_pop(g_L, 1);
//     RETURN_META_VALUE(MRES_IGNORED, 0);
// }

// /* pfnNameForFunction() */
// const char *NameForFunction(uint32 function)
// {
//     if (!g_L)
//         RETURN_META_VALUE(MRES_IGNORED, NULL);

//     lua_getglobal(g_L, "MetaNameForFunction");

//     if (!lua_isfunction(g_L, -1))
//     {
//         lua_pop(g_L, 1);
//         RETURN_META_VALUE(MRES_IGNORED, NULL);
//     }

//     lua_pushinteger(g_L, (lua_Integer)function);

//     if (lua_pcall(g_L, 1, 1, 0) != 0)
//     {
//         lua_pop(g_L, 1);
//         RETURN_META_VALUE(MRES_IGNORED, NULL);
//     }

//     if (lua_isstring(g_L, -1))
//     {
//         static char staticName[256];
//         const char *ret = lua_tostring(g_L, -1);
        
//         strncpy(staticName, ret, 255);
//         staticName[255] = '\0';

//         lua_pop(g_L, 1);
//         RETURN_META_VALUE(MRES_SUPERCEDE, staticName);
//     }

//     lua_pop(g_L, 1);
//     RETURN_META_VALUE(MRES_IGNORED, NULL);
// }

// /* pfnClientPrintf() */
// void ClientPrintf(edict_t* pEdict, PRINT_TYPE ptype, const char *szMsg)
// {
//     if (!g_L)
//         RETURN_META(MRES_IGNORED);

//     lua_getglobal(g_L, "MetaClientPrintf");

//     if (!lua_isfunction(g_L, -1))
//     {
//         lua_pop(g_L, 1);
//         RETURN_META(MRES_IGNORED);
//     }

//     lua_pushentity(g_L, pEdict);
//     lua_pushinteger(g_L, (int)ptype);
//     lua_pushstring(g_L, szMsg);

//     if (lua_pcall(g_L, 3, 0, 0) != 0)
//     {
//         lua_pop(g_L, 1);
//     }

//     RETURN_META(MRES_IGNORED);
// }

// /* pfnServerPrint() */
// void ServerPrint(const char *szMsg)
// {
//     if (!g_L)
//         RETURN_META(MRES_IGNORED);

//     lua_getglobal(g_L, "MetaServerPrint");

//     if (!lua_isfunction(g_L, -1))
//     {
//         lua_pop(g_L, 1);
//         RETURN_META(MRES_IGNORED);
//     }

//     lua_pushstring(g_L, szMsg);

//     if (lua_pcall(g_L, 1, 0, 0) != 0)
//     {
//         lua_pop(g_L, 1);
//     }

//     RETURN_META(MRES_IGNORED);
// }

// /* pfnCmd_Args() */
// const char *Cmd_Args(void)
// {
//     if (!g_L)
//         RETURN_META_VALUE(MRES_IGNORED, NULL);

//     lua_getglobal(g_L, "MetaCmd_Args");

//     if (!lua_isfunction(g_L, -1))
//     {
//         lua_pop(g_L, 1);
//         RETURN_META_VALUE(MRES_IGNORED, NULL);
//     }

//     if (lua_pcall(g_L, 0, 1, 0) != 0)
//     {
//         lua_pop(g_L, 1);
//         RETURN_META_VALUE(MRES_IGNORED, NULL);
//     }

//     if (lua_isstring(g_L, -1))
//     {
//         static char staticArgs[1024];
//         const char *ret = lua_tostring(g_L, -1);
        
//         strncpy(staticArgs, ret, 1023);
//         staticArgs[1023] = '\0';

//         lua_pop(g_L, 1);
//         RETURN_META_VALUE(MRES_SUPERCEDE, staticArgs);
//     }

//     lua_pop(g_L, 1);
//     RETURN_META_VALUE(MRES_IGNORED, NULL);
// }

// /* pfnCmd_Argv() */
// const char *Cmd_Argv(int argc)
// {
//     if (!g_L)
//         RETURN_META_VALUE(MRES_IGNORED, NULL);

//     lua_getglobal(g_L, "MetaCmd_Argv");

//     if (!lua_isfunction(g_L, -1))
//     {
//         lua_pop(g_L, 1);
//         RETURN_META_VALUE(MRES_IGNORED, NULL);
//     }

//     lua_pushinteger(g_L, argc);

//     if (lua_pcall(g_L, 1, 1, 0) != 0)
//     {
//         lua_pop(g_L, 1);
//         RETURN_META_VALUE(MRES_IGNORED, NULL);
//     }

//     if (lua_isstring(g_L, -1))
//     {
//         static char staticArgv[256];
//         const char *ret = lua_tostring(g_L, -1);
        
//         strncpy(staticArgv, ret, 255);
//         staticArgv[255] = '\0';

//         lua_pop(g_L, 1);
//         RETURN_META_VALUE(MRES_SUPERCEDE, staticArgv);
//     }

//     lua_pop(g_L, 1);
//     RETURN_META_VALUE(MRES_IGNORED, NULL);
// }

// /* pfnCmd_Argc() */
// int Cmd_Argc(void)
// {
//     if (!g_L)
//         RETURN_META_VALUE(MRES_IGNORED, 0);

//     lua_getglobal(g_L, "MetaCmd_Argc");

//     if (!lua_isfunction(g_L, -1))
//     {
//         lua_pop(g_L, 1);
//         RETURN_META_VALUE(MRES_IGNORED, 0);
//     }

//     if (lua_pcall(g_L, 0, 1, 0) != 0)
//     {
//         lua_pop(g_L, 1);
//         RETURN_META_VALUE(MRES_IGNORED, 0);
//     }

//     if (lua_isnumber(g_L, -1))
//     {
//         int ret = (int)lua_tointeger(g_L, -1);
//         lua_pop(g_L, 1);
//         RETURN_META_VALUE(MRES_SUPERCEDE, ret);
//     }

//     lua_pop(g_L, 1);
//     RETURN_META_VALUE(MRES_IGNORED, 0);
// }

// /* pfnGetAttachment() */
// void GetAttachment(const edict_t *pEdict, int iAttachment, float *rgflOrigin, float *rgflAngles)
// {
//     if (!g_L)
//         RETURN_META(MRES_IGNORED);

//     lua_getglobal(g_L, "MetaGetAttachment");

//     if (!lua_isfunction(g_L, -1))
//     {
//         lua_pop(g_L, 1);
//         RETURN_META(MRES_IGNORED);
//     }

//     lua_pushentity(g_L, (edict_t*)pEdict);
//     lua_pushinteger(g_L, iAttachment);
//     lua_pushlightuserdata(g_L, (void*)rgflOrigin); // Vector Output
//     lua_pushlightuserdata(g_L, (void*)rgflAngles); // Vector Output

//     if (lua_pcall(g_L, 4, 0, 0) != 0)
//     {
//         lua_pop(g_L, 1);
//     }

//     RETURN_META(MRES_IGNORED);
// }

// /* pfnCRC32_Init() */
// void CRC32_Init(CRC32_t *pulCRC)
// {
//     if (!g_L)
//         RETURN_META(MRES_IGNORED);

//     lua_getglobal(g_L, "MetaCRC32_Init");

//     if (!lua_isfunction(g_L, -1))
//     {
//         lua_pop(g_L, 1);
//         RETURN_META(MRES_IGNORED);
//     }

//     lua_pushlightuserdata(g_L, (void*)pulCRC);

//     if (lua_pcall(g_L, 1, 0, 0) != 0)
//     {
//         lua_pop(g_L, 1);
//     }

//     RETURN_META(MRES_IGNORED);
// }

// /* pfnCRC32_ProcessBuffer() */
// void CRC32_ProcessBuffer(CRC32_t *pulCRC, void *p, int len)
// {
//     if (!g_L)
//         RETURN_META(MRES_IGNORED);

//     lua_getglobal(g_L, "MetaCRC32_ProcessBuffer");

//     if (!lua_isfunction(g_L, -1))
//     {
//         lua_pop(g_L, 1);
//         RETURN_META(MRES_IGNORED);
//     }

//     lua_pushlightuserdata(g_L, (void*)pulCRC);
//     lua_pushlightuserdata(g_L, p);
//     lua_pushinteger(g_L, len);

//     if (lua_pcall(g_L, 3, 0, 0) != 0)
//     {
//         lua_pop(g_L, 1);
//     }

//     RETURN_META(MRES_IGNORED);
// }
// /* pfnCRC32_ProcessByte() */
// void CRC32_ProcessByte(CRC32_t *pulCRC, unsigned char ch)
// {
//     if (!g_L)
//         RETURN_META(MRES_IGNORED);

//     lua_getglobal(g_L, "MetaCRC32_ProcessByte");

//     if (!lua_isfunction(g_L, -1))
//     {
//         lua_pop(g_L, 1);
//         RETURN_META(MRES_IGNORED);
//     }

//     lua_pushlightuserdata(g_L, (void*)pulCRC);
//     lua_pushinteger(g_L, (int)ch);

//     if (lua_pcall(g_L, 2, 0, 0) != 0)
//     {
//         lua_pop(g_L, 1);
//     }

//     RETURN_META(MRES_IGNORED);
// }

// /* pfnCRC32_Final() */
// CRC32_t CRC32_Final(CRC32_t pulCRC)
// {
//     if (!g_L)
//         RETURN_META_VALUE(MRES_IGNORED, pulCRC);

//     lua_getglobal(g_L, "MetaCRC32_Final");

//     if (!lua_isfunction(g_L, -1))
//     {
//         lua_pop(g_L, 1);
//         RETURN_META_VALUE(MRES_IGNORED, pulCRC);
//     }

//     // CRC32_t 通常是 unsigned long (int)
//     lua_pushinteger(g_L, (lua_Integer)pulCRC);

//     if (lua_pcall(g_L, 1, 1, 0) != 0)
//     {
//         lua_pop(g_L, 1);
//         RETURN_META_VALUE(MRES_IGNORED, pulCRC);
//     }

//     if (lua_isnumber(g_L, -1))
//     {
//         CRC32_t ret = (CRC32_t)lua_tointeger(g_L, -1);
//         lua_pop(g_L, 1);
//         RETURN_META_VALUE(MRES_SUPERCEDE, ret);
//     }

//     lua_pop(g_L, 1);
//     RETURN_META_VALUE(MRES_IGNORED, pulCRC);
// }

// /* pfnRandomLong() */
// int32 RandomLong(int32 lLow, int32 lHigh)
// {
//     if (!g_L)
//         RETURN_META_VALUE(MRES_IGNORED, 0);

//     lua_getglobal(g_L, "MetaRandomLong");

//     if (!lua_isfunction(g_L, -1))
//     {
//         lua_pop(g_L, 1);
//         RETURN_META_VALUE(MRES_IGNORED, 0);
//     }

//     lua_pushinteger(g_L, lLow);
//     lua_pushinteger(g_L, lHigh);

//     if (lua_pcall(g_L, 2, 1, 0) != 0)
//     {
//         lua_pop(g_L, 1);
//         RETURN_META_VALUE(MRES_IGNORED, 0);
//     }

//     if (lua_isnumber(g_L, -1))
//     {
//         int32 ret = (int32)lua_tointeger(g_L, -1);
//         lua_pop(g_L, 1);
//         RETURN_META_VALUE(MRES_SUPERCEDE, ret);
//     }

//     lua_pop(g_L, 1);
//     RETURN_META_VALUE(MRES_IGNORED, 0);
// }

// /* pfnRandomFloat() */
// float RandomFloat(float flLow, float flHigh)
// {
//     if (!g_L)
//         RETURN_META_VALUE(MRES_IGNORED, 0.0f);

//     lua_getglobal(g_L, "MetaRandomFloat");

//     if (!lua_isfunction(g_L, -1))
//     {
//         lua_pop(g_L, 1);
//         RETURN_META_VALUE(MRES_IGNORED, 0.0f);
//     }

//     lua_pushnumber(g_L, flLow);
//     lua_pushnumber(g_L, flHigh);

//     if (lua_pcall(g_L, 2, 1, 0) != 0)
//     {
//         lua_pop(g_L, 1);
//         RETURN_META_VALUE(MRES_IGNORED, 0.0f);
//     }

//     if (lua_isnumber(g_L, -1))
//     {
//         float ret = (float)lua_tonumber(g_L, -1);
//         lua_pop(g_L, 1);
//         RETURN_META_VALUE(MRES_SUPERCEDE, ret);
//     }

//     lua_pop(g_L, 1);
//     RETURN_META_VALUE(MRES_IGNORED, 0.0f);
// }

// /* pfnSetView() */
// void SetView(const edict_t *pClient, const edict_t *pViewent)
// {
//     if (!g_L)
//         RETURN_META(MRES_IGNORED);

//     lua_getglobal(g_L, "MetaSetView");

//     if (!lua_isfunction(g_L, -1))
//     {
//         lua_pop(g_L, 1);
//         RETURN_META(MRES_IGNORED);
//     }

//     lua_pushentity(g_L, (edict_t*)pClient);
//     lua_pushentity(g_L, (edict_t*)pViewent);

//     if (lua_pcall(g_L, 2, 0, 0) != 0)
//     {
//         lua_pop(g_L, 1);
//     }

//     RETURN_META(MRES_IGNORED);
// }

// /* pfnTime() */
// float Time(void)
// {
//     if (!g_L)
//         RETURN_META_VALUE(MRES_IGNORED, 0.0f);

//     lua_getglobal(g_L, "MetaTime");

//     if (!lua_isfunction(g_L, -1))
//     {
//         lua_pop(g_L, 1);
//         RETURN_META_VALUE(MRES_IGNORED, 0.0f);
//     }

//     if (lua_pcall(g_L, 0, 1, 0) != 0)
//     {
//         lua_pop(g_L, 1);
//         RETURN_META_VALUE(MRES_IGNORED, 0.0f);
//     }

//     if (lua_isnumber(g_L, -1))
//     {
//         float ret = (float)lua_tonumber(g_L, -1);
//         lua_pop(g_L, 1);
//         RETURN_META_VALUE(MRES_SUPERCEDE, ret);
//     }

//     lua_pop(g_L, 1);
//     RETURN_META_VALUE(MRES_IGNORED, 0.0f);
// }

// /* pfnCrosshairAngle() */
// void CrosshairAngle(const edict_t *pClient, float pitch, float yaw)
// {
//     if (!g_L)
//         RETURN_META(MRES_IGNORED);

//     lua_getglobal(g_L, "MetaCrosshairAngle");

//     if (!lua_isfunction(g_L, -1))
//     {
//         lua_pop(g_L, 1);
//         RETURN_META(MRES_IGNORED);
//     }

//     lua_pushentity(g_L, (edict_t*)pClient);
//     lua_pushnumber(g_L, pitch);
//     lua_pushnumber(g_L, yaw);

//     if (lua_pcall(g_L, 3, 0, 0) != 0)
//     {
//         lua_pop(g_L, 1);
//     }

//     RETURN_META(MRES_IGNORED);
// }

// /* pfnLoadFileForMe() */
// byte* LoadFileForMe(const char *filename, int *pLength)
// {
//     if (!g_L)
//         RETURN_META_VALUE(MRES_IGNORED, NULL);

//     lua_getglobal(g_L, "MetaLoadFileForMe");

//     if (!lua_isfunction(g_L, -1))
//     {
//         lua_pop(g_L, 1);
//         RETURN_META_VALUE(MRES_IGNORED, NULL);
//     }

//     lua_pushstring(g_L, filename);
//     lua_pushlightuserdata(g_L, pLength);

//     if (lua_pcall(g_L, 2, 1, 0) != 0)
//     {
//         lua_pop(g_L, 1);
//         RETURN_META_VALUE(MRES_IGNORED, NULL);
//     }

//     // 如果 Lua 返回一个指针 (userdata/lightuserdata)，则视为覆盖返回的 buffer
//     if (lua_isuserdata(g_L, -1))
//     {
//         byte* ret = (byte*)lua_touserdata(g_L, -1);
//         lua_pop(g_L, 1);
//         RETURN_META_VALUE(MRES_SUPERCEDE, ret);
//     }

//     lua_pop(g_L, 1);
//     RETURN_META_VALUE(MRES_IGNORED, NULL);
// }

// /* pfnFreeFile() */
// void FreeFile(void *buffer)
// {
//     if (!g_L)
//         RETURN_META(MRES_IGNORED);

//     lua_getglobal(g_L, "MetaFreeFile");

//     if (!lua_isfunction(g_L, -1))
//     {
//         lua_pop(g_L, 1);
//         RETURN_META(MRES_IGNORED);
//     }

//     lua_pushlightuserdata(g_L, buffer);

//     if (lua_pcall(g_L, 1, 0, 0) != 0)
//     {
//         lua_pop(g_L, 1);
//     }

//     RETURN_META(MRES_IGNORED);
// }

// /* pfnEndSection() */
// void EndSection(const char *pszSectionName)
// {
//     if (!g_L)
//         RETURN_META(MRES_IGNORED);

//     lua_getglobal(g_L, "MetaEndSection");

//     if (!lua_isfunction(g_L, -1))
//     {
//         lua_pop(g_L, 1);
//         RETURN_META(MRES_IGNORED);
//     }

//     lua_pushstring(g_L, pszSectionName);

//     if (lua_pcall(g_L, 1, 0, 0) != 0)
//     {
//         lua_pop(g_L, 1);
//     }

//     RETURN_META(MRES_IGNORED);
// }
// /* pfnCompareFileTime() */
// int CompareFileTime(char *filename1, char *filename2, int *iCompare)
// {
//     if (!g_L)
//         RETURN_META_VALUE(MRES_IGNORED, 0);

//     lua_getglobal(g_L, "MetaCompareFileTime");

//     if (!lua_isfunction(g_L, -1))
//     {
//         lua_pop(g_L, 1);
//         RETURN_META_VALUE(MRES_IGNORED, 0);
//     }

//     lua_pushstring(g_L, filename1);
//     lua_pushstring(g_L, filename2);
//     lua_pushlightuserdata(g_L, iCompare); // Output pointer

//     if (lua_pcall(g_L, 3, 1, 0) != 0)
//     {
//         lua_pop(g_L, 1);
//         RETURN_META_VALUE(MRES_IGNORED, 0);
//     }

//     if (lua_isnumber(g_L, -1))
//     {
//         int ret = (int)lua_tointeger(g_L, -1);
//         lua_pop(g_L, 1);
//         RETURN_META_VALUE(MRES_SUPERCEDE, ret);
//     }

//     lua_pop(g_L, 1);
//     RETURN_META_VALUE(MRES_IGNORED, 0);
// }

// /* pfnGetGameDir() */
// void GetGameDir(char *szGetGameDir)
// {
//     if (!g_L)
//         RETURN_META(MRES_IGNORED);

//     lua_getglobal(g_L, "MetaGetGameDir");

//     if (!lua_isfunction(g_L, -1))
//     {
//         lua_pop(g_L, 1);
//         RETURN_META(MRES_IGNORED);
//     }

//     // 虽然 szGetGameDir 是输出buffer，但这里还没数据，传指针给 Lua
//     // Lua 如果想修改，应该返回一个字符串
//     lua_pushlightuserdata(g_L, szGetGameDir); 

//     if (lua_pcall(g_L, 1, 1, 0) != 0)
//     {
//         lua_pop(g_L, 1);
//         RETURN_META(MRES_IGNORED);
//     }

//     // 如果 Lua 返回字符串，复制到缓冲区
//     if (lua_isstring(g_L, -1))
//     {
//         const char *ret = lua_tostring(g_L, -1);
//         // 假设 buffer 足够大 (MAX_PATH)，通常是安全的
//         strcpy(szGetGameDir, ret);
        
//         lua_pop(g_L, 1);
//         RETURN_META(MRES_SUPERCEDE);
//     }

//     lua_pop(g_L, 1);
//     RETURN_META(MRES_IGNORED);
// }

// /* pfnCvar_RegisterVariable() */
// void Cvar_RegisterVariable(cvar_t *variable)
// {
//     if (!g_L)
//         RETURN_META(MRES_IGNORED);

//     lua_getglobal(g_L, "MetaCvar_RegisterVariable");

//     if (!lua_isfunction(g_L, -1))
//     {
//         lua_pop(g_L, 1);
//         RETURN_META(MRES_IGNORED);
//     }

//     lua_pushlightuserdata(g_L, (void*)variable);

//     if (lua_pcall(g_L, 1, 0, 0) != 0)
//     {
//         lua_pop(g_L, 1);
//     }

//     RETURN_META(MRES_IGNORED);
// }

// /* pfnFadeClientVolume() */
// void FadeClientVolume(const edict_t *pEdict, int fadePercent, int fadeOutSeconds, int holdTime, int fadeInSeconds)
// {
//     if (!g_L)
//         RETURN_META(MRES_IGNORED);

//     lua_getglobal(g_L, "MetaFadeClientVolume");

//     if (!lua_isfunction(g_L, -1))
//     {
//         lua_pop(g_L, 1);
//         RETURN_META(MRES_IGNORED);
//     }

//     lua_pushentity(g_L, (edict_t*)pEdict);
//     lua_pushinteger(g_L, fadePercent);
//     lua_pushinteger(g_L, fadeOutSeconds);
//     lua_pushinteger(g_L, holdTime);
//     lua_pushinteger(g_L, fadeInSeconds);

//     if (lua_pcall(g_L, 5, 0, 0) != 0)
//     {
//         lua_pop(g_L, 1);
//     }

//     RETURN_META(MRES_IGNORED);
// }

// /* pfnSetClientMaxspeed() */
// void SetClientMaxspeed(const edict_t *pEdict, float fNewMaxspeed)
// {
//     if (!g_L)
//         RETURN_META(MRES_IGNORED);

//     lua_getglobal(g_L, "MetaSetClientMaxspeed");

//     if (!lua_isfunction(g_L, -1))
//     {
//         lua_pop(g_L, 1);
//         RETURN_META(MRES_IGNORED);
//     }

//     lua_pushentity(g_L, pEdict);
//     lua_pushnumber(g_L, fNewMaxspeed);

//     if (lua_pcall(g_L, 2, 0, 0) != 0)
//     {
//         lua_pop(g_L, 1);
//     }

//     RETURN_META(MRES_IGNORED);
// }

// /* pfnCreateFakeClient() */
// edict_t* CreateFakeClient(const char *netname)
// {
//     if (!g_L)
//         RETURN_META_VALUE(MRES_IGNORED, NULL);

//     lua_getglobal(g_L, "MetaCreateFakeClient");

//     if (!lua_isfunction(g_L, -1))
//     {
//         lua_pop(g_L, 1);
//         RETURN_META_VALUE(MRES_IGNORED, NULL);
//     }

//     lua_pushstring(g_L, netname);

//     if (lua_pcall(g_L, 1, 1, 0) != 0)
//     {
//         lua_pop(g_L, 1);
//         RETURN_META_VALUE(MRES_IGNORED, NULL);
//     }

//     // Lua 返回 Entity Index -> edict_t*
//     if (lua_isnumber(g_L, -1))
//     {
//         int entIndex = (int)lua_tointeger(g_L, -1);
//         lua_pop(g_L, 1);

//         edict_t* retEnt = NULL;
//         if (entIndex > 0)
//             retEnt = g_engfuncs.pfnPEntityOfEntIndex(entIndex);

//         RETURN_META_VALUE(MRES_SUPERCEDE, retEnt);
//     }

//     lua_pop(g_L, 1);
//     RETURN_META_VALUE(MRES_IGNORED, NULL);
// }

// /* pfnRunPlayerMove() */
// void RunPlayerMove(edict_t *fakeclient, const float *viewangles, float forwardmove, float sidemove, float upmove, unsigned short buttons, byte impulse, byte msec)
// {
//     if (!g_L)
//         RETURN_META(MRES_IGNORED);

//     lua_getglobal(g_L, "MetaRunPlayerMove");

//     if (!lua_isfunction(g_L, -1))
//     {
//         lua_pop(g_L, 1);
//         RETURN_META(MRES_IGNORED);
//     }

//     lua_pushentity(g_L, fakeclient);
//     lua_pushlightuserdata(g_L, (void*)viewangles); // Vector ptr
//     lua_pushnumber(g_L, forwardmove);
//     lua_pushnumber(g_L, sidemove);
//     lua_pushnumber(g_L, upmove);
//     lua_pushinteger(g_L, buttons);
//     lua_pushinteger(g_L, impulse);
//     lua_pushinteger(g_L, msec);

//     if (lua_pcall(g_L, 8, 0, 0) != 0)
//     {
//         lua_pop(g_L, 1);
//     }

//     RETURN_META(MRES_IGNORED);
// }

// /* pfnNumberOfEntities() */
// int NumberOfEntities(void)
// {
//     if (!g_L)
//         RETURN_META_VALUE(MRES_IGNORED, 0);

//     lua_getglobal(g_L, "MetaNumberOfEntities");

//     if (!lua_isfunction(g_L, -1))
//     {
//         lua_pop(g_L, 1);
//         RETURN_META_VALUE(MRES_IGNORED, 0);
//     }

//     if (lua_pcall(g_L, 0, 1, 0) != 0)
//     {
//         lua_pop(g_L, 1);
//         RETURN_META_VALUE(MRES_IGNORED, 0);
//     }

//     if (lua_isnumber(g_L, -1))
//     {
//         int ret = (int)lua_tointeger(g_L, -1);
//         lua_pop(g_L, 1);
//         RETURN_META_VALUE(MRES_SUPERCEDE, ret);
//     }

//     lua_pop(g_L, 1);
//     RETURN_META_VALUE(MRES_IGNORED, 0);
// }

// /* pfnGetInfoKeyBuffer() */
// char* GetInfoKeyBuffer(edict_t *e)
// {
//     if (!g_L)
//         RETURN_META_VALUE(MRES_IGNORED, NULL);

//     lua_getglobal(g_L, "MetaGetInfoKeyBuffer");

//     if (!lua_isfunction(g_L, -1))
//     {
//         lua_pop(g_L, 1);
//         RETURN_META_VALUE(MRES_IGNORED, NULL);
//     }

//     lua_pushentity(g_L, e);

//     if (lua_pcall(g_L, 1, 1, 0) != 0)
//     {
//         lua_pop(g_L, 1);
//         RETURN_META_VALUE(MRES_IGNORED, NULL);
//     }

//     // 如果 Lua 返回字符串，我们覆盖引擎返回的 buffer 指针
//     if (lua_isstring(g_L, -1))
//     {
//         static char staticBuf[2048]; // Info buffer 通常较大
//         const char *ret = lua_tostring(g_L, -1);
        
//         strncpy(staticBuf, ret, 2047);
//         staticBuf[2047] = '\0';

//         lua_pop(g_L, 1);
//         RETURN_META_VALUE(MRES_SUPERCEDE, staticBuf);
//     }

//     lua_pop(g_L, 1);
//     RETURN_META_VALUE(MRES_IGNORED, NULL);
// }

// /* pfnInfoKeyValue() */
// char* InfoKeyValue(char *infobuffer, const char *key)
// {
//     if (!g_L)
//         RETURN_META_VALUE(MRES_IGNORED, NULL);

//     lua_getglobal(g_L, "MetaInfoKeyValue");

//     if (!lua_isfunction(g_L, -1))
//     {
//         lua_pop(g_L, 1);
//         RETURN_META_VALUE(MRES_IGNORED, NULL);
//     }

//     // infobuffer 是内容字符串，复制一份给 Lua
//     lua_pushstring(g_L, infobuffer);
//     lua_pushstring(g_L, key);

//     if (lua_pcall(g_L, 2, 1, 0) != 0)
//     {
//         lua_pop(g_L, 1);
//         RETURN_META_VALUE(MRES_IGNORED, NULL);
//     }

//     if (lua_isstring(g_L, -1))
//     {
//         static char staticVal[256];
//         const char *ret = lua_tostring(g_L, -1);
        
//         strncpy(staticVal, ret, 255);
//         staticVal[255] = '\0';

//         lua_pop(g_L, 1);
//         RETURN_META_VALUE(MRES_SUPERCEDE, staticVal);
//     }

//     lua_pop(g_L, 1);
//     RETURN_META_VALUE(MRES_IGNORED, NULL);
// }

// /* pfnSetKeyValue() */
// void SetKeyValue(char *infobuffer, const char *key, const char *value)
// {
//     if (!g_L)
//         RETURN_META(MRES_IGNORED);

//     lua_getglobal(g_L, "MetaSetKeyValue");

//     if (!lua_isfunction(g_L, -1))
//     {
//         lua_pop(g_L, 1);
//         RETURN_META(MRES_IGNORED);
//     }

//     lua_pushstring(g_L, infobuffer);
//     lua_pushstring(g_L, key);
//     lua_pushstring(g_L, value);

//     if (lua_pcall(g_L, 3, 0, 0) != 0)
//     {
//         lua_pop(g_L, 1);
//     }

//     RETURN_META(MRES_IGNORED);
// }

// /* pfnSetClientKeyValue() */
// void SetClientKeyValue(int clientIndex, char *infobuffer, const char *key, const char *value)
// {
//     if (!g_L)
//         RETURN_META(MRES_IGNORED);

//     lua_getglobal(g_L, "MetaSetClientKeyValue");

//     if (!lua_isfunction(g_L, -1))
//     {
//         lua_pop(g_L, 1);
//         RETURN_META(MRES_IGNORED);
//     }

//     lua_pushinteger(g_L, clientIndex);
//     lua_pushstring(g_L, infobuffer);
//     lua_pushstring(g_L, key);
//     lua_pushstring(g_L, value);

//     if (lua_pcall(g_L, 4, 0, 0) != 0)
//     {
//         lua_pop(g_L, 1);
//     }

//     RETURN_META(MRES_IGNORED);
// }

// /* pfnIsMapValid() */
// int IsMapValid(const char *filename)
// {
//     if (!g_L)
//         RETURN_META_VALUE(MRES_IGNORED, 0);

//     lua_getglobal(g_L, "MetaIsMapValid");

//     if (!lua_isfunction(g_L, -1))
//     {
//         lua_pop(g_L, 1);
//         RETURN_META_VALUE(MRES_IGNORED, 0);
//     }

//     lua_pushstring(g_L, filename);

//     if (lua_pcall(g_L, 1, 1, 0) != 0)
//     {
//         lua_pop(g_L, 1);
//         RETURN_META_VALUE(MRES_IGNORED, 0);
//     }

//     if (lua_isnumber(g_L, -1))
//     {
//         int ret = (int)lua_tointeger(g_L, -1);
//         lua_pop(g_L, 1);
//         RETURN_META_VALUE(MRES_SUPERCEDE, ret);
//     }

//     lua_pop(g_L, 1);
//     RETURN_META_VALUE(MRES_IGNORED, 0);
// }

// /* pfnStaticDecal() */
// void StaticDecal(const float *origin, int decalIndex, int entityIndex, int modelIndex)
// {
//     if (!g_L)
//         RETURN_META(MRES_IGNORED);

//     lua_getglobal(g_L, "MetaStaticDecal");

//     if (!lua_isfunction(g_L, -1))
//     {
//         lua_pop(g_L, 1);
//         RETURN_META(MRES_IGNORED);
//     }

//     lua_pushlightuserdata(g_L, (void*)origin);
//     lua_pushinteger(g_L, decalIndex);
//     lua_pushinteger(g_L, entityIndex);
//     lua_pushinteger(g_L, modelIndex);

//     if (lua_pcall(g_L, 4, 0, 0) != 0)
//     {
//         lua_pop(g_L, 1);
//     }

//     RETURN_META(MRES_IGNORED);
// }

// /* pfnPrecacheGeneric() */
// int PrecacheGeneric(const char* s)
// {
//     if (!g_L)
//         RETURN_META_VALUE(MRES_IGNORED, 0);

//     lua_getglobal(g_L, "MetaPrecacheGeneric");

//     if (!lua_isfunction(g_L, -1))
//     {
//         lua_pop(g_L, 1);
//         RETURN_META_VALUE(MRES_IGNORED, 0);
//     }

//     lua_pushstring(g_L, s);

//     if (lua_pcall(g_L, 1, 1, 0) != 0)
//     {
//         lua_pop(g_L, 1);
//         RETURN_META_VALUE(MRES_IGNORED, 0);
//     }

//     if (lua_isnumber(g_L, -1))
//     {
//         int ret = (int)lua_tointeger(g_L, -1);
//         lua_pop(g_L, 1);
//         RETURN_META_VALUE(MRES_SUPERCEDE, ret);
//     }

//     lua_pop(g_L, 1);
//     RETURN_META_VALUE(MRES_IGNORED, 0);
// }

// /* pfnGetPlayerUserId() */
// int GetPlayerUserId(edict_t *e)
// {
//     if (!g_L)
//         RETURN_META_VALUE(MRES_IGNORED, 0);

//     lua_getglobal(g_L, "MetaGetPlayerUserId");

//     if (!lua_isfunction(g_L, -1))
//     {
//         lua_pop(g_L, 1);
//         RETURN_META_VALUE(MRES_IGNORED, 0);
//     }

//     lua_pushentity(g_L, e);

//     if (lua_pcall(g_L, 1, 1, 0) != 0)
//     {
//         lua_pop(g_L, 1);
//         RETURN_META_VALUE(MRES_IGNORED, 0);
//     }

//     if (lua_isnumber(g_L, -1))
//     {
//         int ret = (int)lua_tointeger(g_L, -1);
//         lua_pop(g_L, 1);
//         RETURN_META_VALUE(MRES_SUPERCEDE, ret);
//     }

//     lua_pop(g_L, 1);
//     RETURN_META_VALUE(MRES_IGNORED, 0);
// }

// /* pfnBuildSoundMsg() */
// void BuildSoundMsg(edict_t *entity, int channel, const char *sample, float volume, float attenuation, int fFlags, int pitch, int msg_dest, int msg_type, const float *pOrigin, edict_t *ed)
// {
//     if (!g_L)
//         RETURN_META(MRES_IGNORED);

//     lua_getglobal(g_L, "MetaBuildSoundMsg");

//     if (!lua_isfunction(g_L, -1))
//     {
//         lua_pop(g_L, 1);
//         RETURN_META(MRES_IGNORED);
//     }

//     lua_pushentity(g_L, entity);
//     lua_pushinteger(g_L, channel);
//     lua_pushstring(g_L, sample);
//     lua_pushnumber(g_L, volume);
//     lua_pushnumber(g_L, attenuation);
//     lua_pushinteger(g_L, fFlags);
//     lua_pushinteger(g_L, pitch);
//     lua_pushinteger(g_L, msg_dest);
//     lua_pushinteger(g_L, msg_type);
//     lua_pushlightuserdata(g_L, (void*)pOrigin);
//     lua_pushentity(g_L, ed);

//     if (lua_pcall(g_L, 11, 0, 0) != 0)
//     {
//         lua_pop(g_L, 1);
//     }

//     RETURN_META(MRES_IGNORED);
// }

// /* pfnIsDedicatedServer() */
// int IsDedicatedServer(void)
// {
//     if (!g_L)
//         RETURN_META_VALUE(MRES_IGNORED, 0);

//     lua_getglobal(g_L, "MetaIsDedicatedServer");

//     if (!lua_isfunction(g_L, -1))
//     {
//         lua_pop(g_L, 1);
//         RETURN_META_VALUE(MRES_IGNORED, 0);
//     }

//     if (lua_pcall(g_L, 0, 1, 0) != 0)
//     {
//         lua_pop(g_L, 1);
//         RETURN_META_VALUE(MRES_IGNORED, 0);
//     }

//     if (lua_isnumber(g_L, -1))
//     {
//         int ret = (int)lua_tointeger(g_L, -1);
//         lua_pop(g_L, 1);
//         RETURN_META_VALUE(MRES_SUPERCEDE, ret);
//     }

//     lua_pop(g_L, 1);
//     RETURN_META_VALUE(MRES_IGNORED, 0);
// }

// /* pfnCVarGetPointer() */
// cvar_t* CVarGetPointer(const char *szVarName)
// {
//     if (!g_L)
//         RETURN_META_VALUE(MRES_IGNORED, NULL);

//     lua_getglobal(g_L, "MetaCVarGetPointer");

//     if (!lua_isfunction(g_L, -1))
//     {
//         lua_pop(g_L, 1);
//         RETURN_META_VALUE(MRES_IGNORED, NULL);
//     }

//     lua_pushstring(g_L, szVarName);

//     if (lua_pcall(g_L, 1, 1, 0) != 0)
//     {
//         lua_pop(g_L, 1);
//         RETURN_META_VALUE(MRES_IGNORED, NULL);
//     }

//     if (lua_isuserdata(g_L, -1))
//     {
//         cvar_t* ret = (cvar_t*)lua_touserdata(g_L, -1);
//         lua_pop(g_L, 1);
//         RETURN_META_VALUE(MRES_SUPERCEDE, ret);
//     }

//     lua_pop(g_L, 1);
//     RETURN_META_VALUE(MRES_IGNORED, NULL);
// }

// /* pfnGetPlayerWONId() */
// unsigned int GetPlayerWONId(edict_t *e)
// {
//     if (!g_L)
//         RETURN_META_VALUE(MRES_IGNORED, 0);

//     lua_getglobal(g_L, "MetaGetPlayerWONId");

//     if (!lua_isfunction(g_L, -1))
//     {
//         lua_pop(g_L, 1);
//         RETURN_META_VALUE(MRES_IGNORED, 0);
//     }

//     lua_pushentity(g_L, e);

//     if (lua_pcall(g_L, 1, 1, 0) != 0)
//     {
//         lua_pop(g_L, 1);
//         RETURN_META_VALUE(MRES_IGNORED, 0);
//     }

//     if (lua_isnumber(g_L, -1))
//     {
//         unsigned int ret = (unsigned int)lua_tointeger(g_L, -1);
//         lua_pop(g_L, 1);
//         RETURN_META_VALUE(MRES_SUPERCEDE, ret);
//     }

//     lua_pop(g_L, 1);
//     RETURN_META_VALUE(MRES_IGNORED, 0);
// }
// /* pfnInfo_RemoveKey() */
// void Info_RemoveKey(char *s, const char *key)
// {
//     if (!g_L)
//         RETURN_META(MRES_IGNORED);

//     lua_getglobal(g_L, "MetaInfo_RemoveKey");

//     if (!lua_isfunction(g_L, -1))
//     {
//         lua_pop(g_L, 1);
//         RETURN_META(MRES_IGNORED);
//     }

//     lua_pushstring(g_L, s); // Info buffer content
//     lua_pushstring(g_L, key);

//     if (lua_pcall(g_L, 2, 0, 0) != 0)
//     {
//         lua_pop(g_L, 1);
//     }

//     RETURN_META(MRES_IGNORED);
// }

// /* pfnGetPhysicsKeyValue() */
// const char* GetPhysicsKeyValue(const edict_t *pClient, const char *key)
// {
//     if (!g_L)
//         RETURN_META_VALUE(MRES_IGNORED, NULL);

//     lua_getglobal(g_L, "MetaGetPhysicsKeyValue");

//     if (!lua_isfunction(g_L, -1))
//     {
//         lua_pop(g_L, 1);
//         RETURN_META_VALUE(MRES_IGNORED, NULL);
//     }

//     lua_pushentity(g_L, (edict_t*)pClient);
//     lua_pushstring(g_L, key);

//     if (lua_pcall(g_L, 2, 1, 0) != 0)
//     {
//         lua_pop(g_L, 1);
//         RETURN_META_VALUE(MRES_IGNORED, NULL);
//     }

//     if (lua_isstring(g_L, -1))
//     {
//         static char staticVal[256];
//         const char *ret = lua_tostring(g_L, -1);
        
//         strncpy(staticVal, ret, 255);
//         staticVal[255] = '\0';

//         lua_pop(g_L, 1);
//         RETURN_META_VALUE(MRES_SUPERCEDE, staticVal);
//     }

//     lua_pop(g_L, 1);
//     RETURN_META_VALUE(MRES_IGNORED, NULL);
// }

// /* pfnSetPhysicsKeyValue() */
// void SetPhysicsKeyValue(const edict_t *pClient, const char *key, const char *value)
// {
//     if (!g_L)
//         RETURN_META(MRES_IGNORED);

//     lua_getglobal(g_L, "MetaSetPhysicsKeyValue");

//     if (!lua_isfunction(g_L, -1))
//     {
//         lua_pop(g_L, 1);
//         RETURN_META(MRES_IGNORED);
//     }

//     lua_pushentity(g_L, (edict_t*)pClient);
//     lua_pushstring(g_L, key);
//     lua_pushstring(g_L, value);

//     if (lua_pcall(g_L, 3, 0, 0) != 0)
//     {
//         lua_pop(g_L, 1);
//     }

//     RETURN_META(MRES_IGNORED);
// }

// /* pfnGetPhysicsInfoString() */
// const char* GetPhysicsInfoString(const edict_t *pClient)
// {
//     if (!g_L)
//         RETURN_META_VALUE(MRES_IGNORED, NULL);

//     lua_getglobal(g_L, "MetaGetPhysicsInfoString");

//     if (!lua_isfunction(g_L, -1))
//     {
//         lua_pop(g_L, 1);
//         RETURN_META_VALUE(MRES_IGNORED, NULL);
//     }

//     lua_pushentity(g_L, (edict_t*)pClient);

//     if (lua_pcall(g_L, 1, 1, 0) != 0)
//     {
//         lua_pop(g_L, 1);
//         RETURN_META_VALUE(MRES_IGNORED, NULL);
//     }

//     if (lua_isstring(g_L, -1))
//     {
//         static char staticInfo[2048];
//         const char *ret = lua_tostring(g_L, -1);
        
//         strncpy(staticInfo, ret, 2047);
//         staticInfo[2047] = '\0';

//         lua_pop(g_L, 1);
//         RETURN_META_VALUE(MRES_SUPERCEDE, staticInfo);
//     }

//     lua_pop(g_L, 1);
//     RETURN_META_VALUE(MRES_IGNORED, NULL);
// }

// /* pfnPrecacheEvent() */
// unsigned short PrecacheEvent(int type, const char* psz)
// {
//     if (!g_L)
//         RETURN_META_VALUE(MRES_IGNORED, 0);

//     lua_getglobal(g_L, "MetaPrecacheEvent");

//     if (!lua_isfunction(g_L, -1))
//     {
//         lua_pop(g_L, 1);
//         RETURN_META_VALUE(MRES_IGNORED, 0);
//     }

//     lua_pushinteger(g_L, type);
//     lua_pushstring(g_L, psz);

//     if (lua_pcall(g_L, 2, 1, 0) != 0)
//     {
//         lua_pop(g_L, 1);
//         RETURN_META_VALUE(MRES_IGNORED, 0);
//     }

//     if (lua_isnumber(g_L, -1))
//     {
//         unsigned short ret = (unsigned short)lua_tointeger(g_L, -1);
//         lua_pop(g_L, 1);
//         RETURN_META_VALUE(MRES_SUPERCEDE, ret);
//     }

//     lua_pop(g_L, 1);
//     RETURN_META_VALUE(MRES_IGNORED, 0);
// }

// /* pfnPlaybackEvent() */
// void PlaybackEvent(int flags, const edict_t *pInvoker, unsigned short eventindex, float delay, float *origin, float *angles, float fparam1, float fparam2, int iparam1, int iparam2, int bparam1, int bparam2)
// {
//     if (!g_L)
//         RETURN_META(MRES_IGNORED);

//     lua_getglobal(g_L, "MetaPlaybackEvent");

//     if (!lua_isfunction(g_L, -1))
//     {
//         lua_pop(g_L, 1);
//         RETURN_META(MRES_IGNORED);
//     }

//     lua_pushinteger(g_L, flags);
//     lua_pushentity(g_L, (edict_t*)pInvoker);
//     lua_pushinteger(g_L, eventindex);
//     lua_pushnumber(g_L, delay);
//     lua_pushlightuserdata(g_L, (void*)origin); // Vector ptr
//     lua_pushlightuserdata(g_L, (void*)angles); // Vector ptr
//     lua_pushnumber(g_L, fparam1);
//     lua_pushnumber(g_L, fparam2);
//     lua_pushinteger(g_L, iparam1);
//     lua_pushinteger(g_L, iparam2);
//     lua_pushinteger(g_L, bparam1);
//     lua_pushinteger(g_L, bparam2);

//     if (lua_pcall(g_L, 12, 0, 0) != 0)
//     {
//         lua_pop(g_L, 1);
//     }

//     RETURN_META(MRES_IGNORED);
// }

// /* pfnSetFatPVS() */
// unsigned char *SetFatPVS(float *org)
// {
//     if (!g_L)
//         RETURN_META_VALUE(MRES_IGNORED, NULL);

//     lua_getglobal(g_L, "MetaSetFatPVS");

//     if (!lua_isfunction(g_L, -1))
//     {
//         lua_pop(g_L, 1);
//         RETURN_META_VALUE(MRES_IGNORED, NULL);
//     }

//     lua_pushlightuserdata(g_L, (void*)org);

//     if (lua_pcall(g_L, 1, 1, 0) != 0)
//     {
//         lua_pop(g_L, 1);
//         RETURN_META_VALUE(MRES_IGNORED, NULL);
//     }

//     if (lua_isuserdata(g_L, -1))
//     {
//         unsigned char* ret = (unsigned char*)lua_touserdata(g_L, -1);
//         lua_pop(g_L, 1);
//         RETURN_META_VALUE(MRES_SUPERCEDE, ret);
//     }

//     lua_pop(g_L, 1);
//     RETURN_META_VALUE(MRES_IGNORED, NULL);
// }

// /* pfnSetFatPAS() */
// unsigned char *SetFatPAS(float *org)
// {
//     if (!g_L)
//         RETURN_META_VALUE(MRES_IGNORED, NULL);

//     lua_getglobal(g_L, "MetaSetFatPAS");

//     if (!lua_isfunction(g_L, -1))
//     {
//         lua_pop(g_L, 1);
//         RETURN_META_VALUE(MRES_IGNORED, NULL);
//     }

//     lua_pushlightuserdata(g_L, (void*)org);

//     if (lua_pcall(g_L, 1, 1, 0) != 0)
//     {
//         lua_pop(g_L, 1);
//         RETURN_META_VALUE(MRES_IGNORED, NULL);
//     }

//     if (lua_isuserdata(g_L, -1))
//     {
//         unsigned char* ret = (unsigned char*)lua_touserdata(g_L, -1);
//         lua_pop(g_L, 1);
//         RETURN_META_VALUE(MRES_SUPERCEDE, ret);
//     }

//     lua_pop(g_L, 1);
//     RETURN_META_VALUE(MRES_IGNORED, NULL);
// }

// /* pfnCheckVisibility() */
// int CheckVisibility(const edict_t *entity, unsigned char *pset)
// {
//     if (!g_L)
//         RETURN_META_VALUE(MRES_IGNORED, 0);

//     lua_getglobal(g_L, "MetaCheckVisibility");

//     if (!lua_isfunction(g_L, -1))
//     {
//         lua_pop(g_L, 1);
//         RETURN_META_VALUE(MRES_IGNORED, 0);
//     }

//     lua_pushentity(g_L, (edict_t*)entity);
//     lua_pushlightuserdata(g_L, (void*)pset);

//     if (lua_pcall(g_L, 2, 1, 0) != 0)
//     {
//         lua_pop(g_L, 1);
//         RETURN_META_VALUE(MRES_IGNORED, 0);
//     }

//     if (lua_isnumber(g_L, -1))
//     {
//         int ret = (int)lua_tointeger(g_L, -1);
//         lua_pop(g_L, 1);
//         RETURN_META_VALUE(MRES_SUPERCEDE, ret);
//     }

//     lua_pop(g_L, 1);
//     RETURN_META_VALUE(MRES_IGNORED, 0);
// }

// /* pfnDeltaSetField() */
// void DeltaSetField(struct delta_s *pFields, const char *fieldname)
// {
//     if (!g_L)
//         RETURN_META(MRES_IGNORED);

//     lua_getglobal(g_L, "MetaDeltaSetField");

//     if (!lua_isfunction(g_L, -1))
//     {
//         lua_pop(g_L, 1);
//         RETURN_META(MRES_IGNORED);
//     }

//     lua_pushlightuserdata(g_L, (void*)pFields);
//     lua_pushstring(g_L, fieldname);

//     if (lua_pcall(g_L, 2, 0, 0) != 0)
//     {
//         lua_pop(g_L, 1);
//     }

//     RETURN_META(MRES_IGNORED);
// }
// /* pfnDeltaUnsetField() */
// void DeltaUnsetField(struct delta_s *pFields, const char *fieldname)
// {
//     if (!g_L)
//         RETURN_META(MRES_IGNORED);

//     lua_getglobal(g_L, "MetaDeltaUnsetField");

//     if (!lua_isfunction(g_L, -1))
//     {
//         lua_pop(g_L, 1);
//         RETURN_META(MRES_IGNORED);
//     }

//     lua_pushlightuserdata(g_L, (void*)pFields);
//     lua_pushstring(g_L, fieldname);

//     if (lua_pcall(g_L, 2, 0, 0) != 0)
//     {
//         lua_pop(g_L, 1);
//     }

//     RETURN_META(MRES_IGNORED);
// }

// /* pfnDeltaAddEncoder() */
// // void DeltaAddEncoder(char *name, void (*conditionalencode)(struct delta_s *pFields, const unsigned char *from, const unsigned char *to))
// // {
// //     if (!g_L)
// //         RETURN_META(MRES_IGNORED);

// //     lua_getglobal(g_L, "MetaDeltaAddEncoder");

// //     if (!lua_isfunction(g_L, -1))
// //     {
// //         lua_pop(g_L, 1);
// //         RETURN_META(MRES_IGNORED);
// //     }

// //     lua_pushstring(g_L, name);
// //     // 函数指针只能作为 lightuserdata 传递
// //     lua_pushlightuserdata(g_L, (void*)conditionalencode);

// //     if (lua_pcall(g_L, 2, 0, 0) != 0)
// //     {
// //         lua_pop(g_L, 1);
// //     }

// //     RETURN_META(MRES_IGNORED);
// // }

// /* pfnGetCurrentPlayer() */
// int GetCurrentPlayer(void)
// {
//     if (!g_L)
//         RETURN_META_VALUE(MRES_IGNORED, 0);

//     lua_getglobal(g_L, "MetaGetCurrentPlayer");

//     if (!lua_isfunction(g_L, -1))
//     {
//         lua_pop(g_L, 1);
//         RETURN_META_VALUE(MRES_IGNORED, 0);
//     }

//     if (lua_pcall(g_L, 0, 1, 0) != 0)
//     {
//         lua_pop(g_L, 1);
//         RETURN_META_VALUE(MRES_IGNORED, 0);
//     }

//     if (lua_isnumber(g_L, -1))
//     {
//         int ret = (int)lua_tointeger(g_L, -1);
//         lua_pop(g_L, 1);
//         RETURN_META_VALUE(MRES_SUPERCEDE, ret);
//     }

//     lua_pop(g_L, 1);
//     RETURN_META_VALUE(MRES_IGNORED, 0);
// }

// /* pfnCanSkipPlayer() */
// int CanSkipPlayer(const edict_t *pPlayer)
// {
//     if (!g_L)
//         RETURN_META_VALUE(MRES_IGNORED, 0);

//     lua_getglobal(g_L, "MetaCanSkipPlayer");

//     if (!lua_isfunction(g_L, -1))
//     {
//         lua_pop(g_L, 1);
//         RETURN_META_VALUE(MRES_IGNORED, 0);
//     }

//     lua_pushentity(g_L, (edict_t*)pPlayer);

//     if (lua_pcall(g_L, 1, 1, 0) != 0)
//     {
//         lua_pop(g_L, 1);
//         RETURN_META_VALUE(MRES_IGNORED, 0);
//     }

//     if (lua_isnumber(g_L, -1))
//     {
//         int ret = (int)lua_tointeger(g_L, -1);
//         lua_pop(g_L, 1);
//         RETURN_META_VALUE(MRES_SUPERCEDE, ret);
//     }

//     lua_pop(g_L, 1);
//     RETURN_META_VALUE(MRES_IGNORED, 0);
// }

// /* pfnDeltaFindField() */
// int DeltaFindField(struct delta_s *pFields, const char *fieldname)
// {
//     if (!g_L)
//         RETURN_META_VALUE(MRES_IGNORED, 0);

//     lua_getglobal(g_L, "MetaDeltaFindField");

//     if (!lua_isfunction(g_L, -1))
//     {
//         lua_pop(g_L, 1);
//         RETURN_META_VALUE(MRES_IGNORED, 0);
//     }

//     lua_pushlightuserdata(g_L, (void*)pFields);
//     lua_pushstring(g_L, fieldname);

//     if (lua_pcall(g_L, 2, 1, 0) != 0)
//     {
//         lua_pop(g_L, 1);
//         RETURN_META_VALUE(MRES_IGNORED, 0);
//     }

//     if (lua_isnumber(g_L, -1))
//     {
//         int ret = (int)lua_tointeger(g_L, -1);
//         lua_pop(g_L, 1);
//         RETURN_META_VALUE(MRES_SUPERCEDE, ret);
//     }

//     lua_pop(g_L, 1);
//     RETURN_META_VALUE(MRES_IGNORED, 0);
// }

// /* pfnDeltaSetFieldByIndex() */
// void DeltaSetFieldByIndex(struct delta_s *pFields, int fieldNumber)
// {
//     if (!g_L)
//         RETURN_META(MRES_IGNORED);

//     lua_getglobal(g_L, "MetaDeltaSetFieldByIndex");

//     if (!lua_isfunction(g_L, -1))
//     {
//         lua_pop(g_L, 1);
//         RETURN_META(MRES_IGNORED);
//     }

//     lua_pushlightuserdata(g_L, (void*)pFields);
//     lua_pushinteger(g_L, fieldNumber);

//     if (lua_pcall(g_L, 2, 0, 0) != 0)
//     {
//         lua_pop(g_L, 1);
//     }

//     RETURN_META(MRES_IGNORED);
// }

// /* pfnDeltaUnsetFieldByIndex() */
// void DeltaUnsetFieldByIndex(struct delta_s *pFields, int fieldNumber)
// {
//     if (!g_L)
//         RETURN_META(MRES_IGNORED);

//     lua_getglobal(g_L, "MetaDeltaUnsetFieldByIndex");

//     if (!lua_isfunction(g_L, -1))
//     {
//         lua_pop(g_L, 1);
//         RETURN_META(MRES_IGNORED);
//     }

//     lua_pushlightuserdata(g_L, (void*)pFields);
//     lua_pushinteger(g_L, fieldNumber);

//     if (lua_pcall(g_L, 2, 0, 0) != 0)
//     {
//         lua_pop(g_L, 1);
//     }

//     RETURN_META(MRES_IGNORED);
// }

// /* pfnSetGroupMask() */
// void SetGroupMask(int mask, int op)
// {
//     if (!g_L)
//         RETURN_META(MRES_IGNORED);

//     lua_getglobal(g_L, "MetaSetGroupMask");

//     if (!lua_isfunction(g_L, -1))
//     {
//         lua_pop(g_L, 1);
//         RETURN_META(MRES_IGNORED);
//     }

//     lua_pushinteger(g_L, mask);
//     lua_pushinteger(g_L, op);

//     if (lua_pcall(g_L, 2, 0, 0) != 0)
//     {
//         lua_pop(g_L, 1);
//     }

//     RETURN_META(MRES_IGNORED);
// }

// /* pfnengCreateInstancedBaseline() */
// int engCreateInstancedBaseline(int classname, struct entity_state_s *baseline)
// {
//     if (!g_L)
//         RETURN_META_VALUE(MRES_IGNORED, 0);

//     lua_getglobal(g_L, "MetaEngCreateInstancedBaseline");

//     if (!lua_isfunction(g_L, -1))
//     {
//         lua_pop(g_L, 1);
//         RETURN_META_VALUE(MRES_IGNORED, 0);
//     }

//     lua_pushinteger(g_L, classname);
//     lua_pushlightuserdata(g_L, (void*)baseline);

//     if (lua_pcall(g_L, 2, 1, 0) != 0)
//     {
//         lua_pop(g_L, 1);
//         RETURN_META_VALUE(MRES_IGNORED, 0);
//     }

//     if (lua_isnumber(g_L, -1))
//     {
//         int ret = (int)lua_tointeger(g_L, -1);
//         lua_pop(g_L, 1);
//         RETURN_META_VALUE(MRES_SUPERCEDE, ret);
//     }

//     lua_pop(g_L, 1);
//     RETURN_META_VALUE(MRES_IGNORED, 0);
// }

// /* pfnCvar_DirectSet() */
// // void Cvar_DirectSet(struct cvar_s *var, char *value)
// // {
// //     if (!g_L)
// //         RETURN_META(MRES_IGNORED);

// //     lua_getglobal(g_L, "MetaCvar_DirectSet");

// //     if (!lua_isfunction(g_L, -1))
// //     {
// //         lua_pop(g_L, 1);
// //         RETURN_META(MRES_IGNORED);
// //     }

// //     lua_pushlightuserdata(g_L, (void*)var);
// //     lua_pushstring(g_L, value);

// //     if (lua_pcall(g_L, 2, 0, 0) != 0)
// //     {
// //         lua_pop(g_L, 1);
// //     }

// //     RETURN_META(MRES_IGNORED);
// // }
// /* pfnForceUnmodified() */
// void ForceUnmodified(const float *mins, const float *absmin, const float *maxs, const float *absmax, const char *filename)
// {
//     if (!g_L)
//         RETURN_META(MRES_IGNORED);

//     lua_getglobal(g_L, "MetaForceUnmodified");

//     if (!lua_isfunction(g_L, -1))
//     {
//         lua_pop(g_L, 1);
//         RETURN_META(MRES_IGNORED);
//     }

//     lua_pushlightuserdata(g_L, (void*)mins);
//     lua_pushlightuserdata(g_L, (void*)absmin);
//     lua_pushlightuserdata(g_L, (void*)maxs);
//     lua_pushlightuserdata(g_L, (void*)absmax);
//     lua_pushstring(g_L, filename);

//     if (lua_pcall(g_L, 5, 0, 0) != 0)
//     {
//         lua_pop(g_L, 1);
//     }

//     RETURN_META(MRES_IGNORED);
// }

// /* pfnGetPlayerStats() */
// void GetPlayerStats(const edict_t *pClient, int *ping, int *packet_loss)
// {
//     if (!g_L)
//         RETURN_META(MRES_IGNORED);

//     lua_getglobal(g_L, "MetaGetPlayerStats");

//     if (!lua_isfunction(g_L, -1))
//     {
//         lua_pop(g_L, 1);
//         RETURN_META(MRES_IGNORED);
//     }

//     lua_pushentity(g_L, (edict_t*)pClient);
//     lua_pushlightuserdata(g_L, (void*)ping);        // 输出参数指针
//     lua_pushlightuserdata(g_L, (void*)packet_loss); // 输出参数指针

//     if (lua_pcall(g_L, 3, 0, 0) != 0)
//     {
//         lua_pop(g_L, 1);
//     }

//     RETURN_META(MRES_IGNORED);
// }

// /* pfnAddServerCommand() */
// void AddServerCommand(char *cmd_name, void (*function)(void))
// {
//     if (!g_L)
//         RETURN_META(MRES_IGNORED);

//     lua_getglobal(g_L, "MetaAddServerCommand");

//     if (!lua_isfunction(g_L, -1))
//     {
//         lua_pop(g_L, 1);
//         RETURN_META(MRES_IGNORED);
//     }

//     lua_pushstring(g_L, cmd_name);
//     lua_pushlightuserdata(g_L, (void*)function); // 函数指针

//     if (lua_pcall(g_L, 2, 0, 0) != 0)
//     {
//         lua_pop(g_L, 1);
//     }

//     RETURN_META(MRES_IGNORED);
// }

// /* pfnVoice_GetClientListening() */
// qboolean Voice_GetClientListening(int iReceiver, int iSender)
// {
//     if (!g_L)
//         RETURN_META_VALUE(MRES_IGNORED, 0);

//     lua_getglobal(g_L, "MetaVoice_GetClientListening");

//     if (!lua_isfunction(g_L, -1))
//     {
//         lua_pop(g_L, 1);
//         RETURN_META_VALUE(MRES_IGNORED, 0);
//     }

//     lua_pushinteger(g_L, iReceiver);
//     lua_pushinteger(g_L, iSender);

//     if (lua_pcall(g_L, 2, 1, 0) != 0)
//     {
//         lua_pop(g_L, 1);
//         RETURN_META_VALUE(MRES_IGNORED, 0);
//     }

//     if (lua_isnumber(g_L, -1))
//     {
//         qboolean ret = (qboolean)lua_tointeger(g_L, -1);
//         lua_pop(g_L, 1);
//         RETURN_META_VALUE(MRES_SUPERCEDE, ret);
//     }

//     lua_pop(g_L, 1);
//     RETURN_META_VALUE(MRES_IGNORED, 0);
// }

// /* pfnVoice_SetClientListening() */
// qboolean Voice_SetClientListening(int iReceiver, int iSender, qboolean bListen)
// {
//     if (!g_L)
//         RETURN_META_VALUE(MRES_IGNORED, 0);

//     lua_getglobal(g_L, "MetaVoice_SetClientListening");

//     if (!lua_isfunction(g_L, -1))
//     {
//         lua_pop(g_L, 1);
//         RETURN_META_VALUE(MRES_IGNORED, 0);
//     }

//     lua_pushinteger(g_L, iReceiver);
//     lua_pushinteger(g_L, iSender);
//     lua_pushinteger(g_L, bListen);

//     if (lua_pcall(g_L, 3, 1, 0) != 0)
//     {
//         lua_pop(g_L, 1);
//         RETURN_META_VALUE(MRES_IGNORED, 0);
//     }

//     if (lua_isnumber(g_L, -1))
//     {
//         qboolean ret = (qboolean)lua_tointeger(g_L, -1);
//         lua_pop(g_L, 1);
//         RETURN_META_VALUE(MRES_SUPERCEDE, ret);
//     }

//     lua_pop(g_L, 1);
//     RETURN_META_VALUE(MRES_IGNORED, 0);
// }

// /* pfnGetPlayerAuthId() */
// const char *GetPlayerAuthId(edict_t *e)
// {
//     if (!g_L)
//         RETURN_META_VALUE(MRES_IGNORED, NULL);

//     lua_getglobal(g_L, "MetaGetPlayerAuthId");

//     if (!lua_isfunction(g_L, -1))
//     {
//         lua_pop(g_L, 1);
//         RETURN_META_VALUE(MRES_IGNORED, NULL);
//     }

//     lua_pushentity(g_L, e);

//     if (lua_pcall(g_L, 1, 1, 0) != 0)
//     {
//         lua_pop(g_L, 1);
//         RETURN_META_VALUE(MRES_IGNORED, NULL);
//     }

//     if (lua_isstring(g_L, -1))
//     {
//         static char staticAuth[64];
//         const char *ret = lua_tostring(g_L, -1);
        
//         strncpy(staticAuth, ret, 63);
//         staticAuth[63] = '\0';

//         lua_pop(g_L, 1);
//         RETURN_META_VALUE(MRES_SUPERCEDE, staticAuth);
//     }

//     lua_pop(g_L, 1);
//     RETURN_META_VALUE(MRES_IGNORED, NULL);
// }

// // -------------------------------------------------------------
// // 以下函数通常位于 DLL_FUNCTIONS (Game DLL) 中，而非 enginefuncs_t
// // -------------------------------------------------------------

// /* OnFreeEntPrivateData */
// void OnFreeEntPrivateData(edict_t *pEnt)
// {
//     if (!g_L)
//         RETURN_META(MRES_IGNORED);

//     lua_getglobal(g_L, "MetaOnFreeEntPrivateData");

//     if (!lua_isfunction(g_L, -1))
//     {
//         lua_pop(g_L, 1);
//         RETURN_META(MRES_IGNORED);
//     }

//     lua_pushentity(g_L, pEnt);

//     if (lua_pcall(g_L, 1, 0, 0) != 0)
//     {
//         lua_pop(g_L, 1);
//     }

//     RETURN_META(MRES_IGNORED);
// }

// /* GameShutdown */
// void GameShutdown(void)
// {
//     if (!g_L)
//         RETURN_META(MRES_IGNORED);

//     lua_getglobal(g_L, "MetaGameShutdown");

//     if (!lua_isfunction(g_L, -1))
//     {
//         lua_pop(g_L, 1);
//         RETURN_META(MRES_IGNORED);
//     }

//     if (lua_pcall(g_L, 0, 0, 0) != 0)
//     {
//         lua_pop(g_L, 1);
//     }

//     RETURN_META(MRES_IGNORED);
// }

// /* ShouldCollide */
// int ShouldCollide(edict_t *pentTouched, edict_t *pentOther)
// {
//     if (!g_L)
//         RETURN_META_VALUE(MRES_IGNORED, 0);

//     lua_getglobal(g_L, "MetaShouldCollide");

//     if (!lua_isfunction(g_L, -1))
//     {
//         lua_pop(g_L, 1);
//         RETURN_META_VALUE(MRES_IGNORED, 0);
//     }

//     lua_pushentity(g_L, pentTouched);
//     lua_pushentity(g_L, pentOther);

//     if (lua_pcall(g_L, 2, 1, 0) != 0)
//     {
//         lua_pop(g_L, 1);
//         RETURN_META_VALUE(MRES_IGNORED, 0);
//     }

//     if (lua_isnumber(g_L, -1))
//     {
//         int ret = (int)lua_tointeger(g_L, -1);
//         lua_pop(g_L, 1);
//         RETURN_META_VALUE(MRES_SUPERCEDE, ret);
//     }

//     lua_pop(g_L, 1);
//     RETURN_META_VALUE(MRES_IGNORED, 0);
// }
// //----------------------------------
// // - GetEngineAPI_Post functions
// //----------------------------------
// //----------------------------------
// //----------------------------------
// //----------------------------------
// //----------------------------------
// //----------------------------------
// //----------------------------------
// //----------------------------------
// //----------------------------------
// //----------------------------------
// /* pfnPrecacheModel() */
// int PrecacheModel_Post(const char* s)
// {
//     if (!g_L)
//         RETURN_META_VALUE(MRES_IGNORED, 0);

//     lua_getglobal(g_L, "MetaPrecacheModel_Post");

//     if (!lua_isfunction(g_L, -1))
//     {
//         lua_pop(g_L, 1);
//         RETURN_META_VALUE(MRES_IGNORED, 0);
//     }

//     lua_pushstring(g_L, s);

//     if (lua_pcall(g_L, 1, 1, 0) != 0)
//     {
//         lua_pop(g_L, 1);
//         RETURN_META_VALUE(MRES_IGNORED, 0);
//     }

//     // 如果 Lua 返回一个数字，则覆盖引擎的返回值
//     if (lua_isnumber(g_L, -1))
//     {
//         int ret = (int)lua_tointeger(g_L, -1);
//         lua_pop(g_L, 1);
//         RETURN_META_VALUE(MRES_SUPERCEDE, ret);
//     }

//     lua_pop(g_L, 1);
//     RETURN_META_VALUE(MRES_IGNORED, 0);
// }

// /* pfnPrecacheSound() */
// int PrecacheSound_Post(const char* s)
// {
//     if (!g_L)
//         RETURN_META_VALUE(MRES_IGNORED, 0);

//     lua_getglobal(g_L, "MetaPrecacheSound_Post");

//     if (!lua_isfunction(g_L, -1))
//     {
//         lua_pop(g_L, 1);
//         RETURN_META_VALUE(MRES_IGNORED, 0);
//     }

//     lua_pushstring(g_L, s);

//     if (lua_pcall(g_L, 1, 1, 0) != 0)
//     {
//         lua_pop(g_L, 1);
//         RETURN_META_VALUE(MRES_IGNORED, 0);
//     }

//     if (lua_isnumber(g_L, -1))
//     {
//         int ret = (int)lua_tointeger(g_L, -1);
//         lua_pop(g_L, 1);
//         RETURN_META_VALUE(MRES_SUPERCEDE, ret);
//     }

//     lua_pop(g_L, 1);
//     RETURN_META_VALUE(MRES_IGNORED, 0);
// }

// /* pfnSetModel() */
// void SetModel_Post(edict_t *e, const char *m)
// {
//     if (!g_L)
//         RETURN_META(MRES_IGNORED);

//     lua_getglobal(g_L, "MetaSetModel_Post");

//     if (!lua_isfunction(g_L, -1))
//     {
//         lua_pop(g_L, 1);
//         RETURN_META(MRES_IGNORED);
//     }

//     // 使用自定义的 pushentity
//     lua_pushentity(g_L, e);
//     lua_pushstring(g_L, m);

//     if (lua_pcall(g_L, 2, 0, 0) != 0)
//     {
//         lua_pop(g_L, 1);
//     }

//     RETURN_META(MRES_IGNORED);
// }

// /* pfnModelIndex() */
// int ModelIndex_Post(const char *m)
// {
//     if (!g_L)
//         RETURN_META_VALUE(MRES_IGNORED, 0);

//     lua_getglobal(g_L, "MetaModelIndex_Post");

//     if (!lua_isfunction(g_L, -1))
//     {
//         lua_pop(g_L, 1);
//         RETURN_META_VALUE(MRES_IGNORED, 0);
//     }

//     lua_pushstring(g_L, m);

//     if (lua_pcall(g_L, 1, 1, 0) != 0)
//     {
//         lua_pop(g_L, 1);
//         RETURN_META_VALUE(MRES_IGNORED, 0);
//     }

//     if (lua_isnumber(g_L, -1))
//     {
//         int ret = (int)lua_tointeger(g_L, -1);
//         lua_pop(g_L, 1);
//         RETURN_META_VALUE(MRES_SUPERCEDE, ret);
//     }

//     lua_pop(g_L, 1);
//     RETURN_META_VALUE(MRES_IGNORED, 0);
// }

// /* pfnModelFrames() */
// int ModelFrames_Post(int modelIndex)
// {
//     if (!g_L)
//         RETURN_META_VALUE(MRES_IGNORED, 0);

//     lua_getglobal(g_L, "MetaModelFrames_Post");

//     if (!lua_isfunction(g_L, -1))
//     {
//         lua_pop(g_L, 1);
//         RETURN_META_VALUE(MRES_IGNORED, 0);
//     }

//     lua_pushinteger(g_L, modelIndex);

//     if (lua_pcall(g_L, 1, 1, 0) != 0)
//     {
//         lua_pop(g_L, 1);
//         RETURN_META_VALUE(MRES_IGNORED, 0);
//     }

//     if (lua_isnumber(g_L, -1))
//     {
//         int ret = (int)lua_tointeger(g_L, -1);
//         lua_pop(g_L, 1);
//         RETURN_META_VALUE(MRES_SUPERCEDE, ret);
//     }

//     lua_pop(g_L, 1);
//     RETURN_META_VALUE(MRES_IGNORED, 0);
// }

// /* pfnSetSize() */
// void SetSize_Post(edict_t *e, const float *rgflMin, const float *rgflMax)
// {
//     if (!g_L)
//         RETURN_META(MRES_IGNORED);

//     lua_getglobal(g_L, "MetaSetSize_Post");

//     if (!lua_isfunction(g_L, -1))
//     {
//         lua_pop(g_L, 1);
//         RETURN_META(MRES_IGNORED);
//     }

//     // Entity
//     lua_pushentity(g_L, e);
//     // Min/Max vectors (传递指针，因为是 const float*)
//     lua_pushlightuserdata(g_L, (void*)rgflMin);
//     lua_pushlightuserdata(g_L, (void*)rgflMax);

//     if (lua_pcall(g_L, 3, 0, 0) != 0)
//     {
//         lua_pop(g_L, 1);
//     }

//     RETURN_META(MRES_IGNORED);
// }

// /* pfnChangeLevel() */
// void ChangeLevel_Post(const char* s1, const char* s2)
// {
//     if (!g_L)
//         RETURN_META(MRES_IGNORED);

//     lua_getglobal(g_L, "MetaChangeLevel_Post");

//     if (!lua_isfunction(g_L, -1))
//     {
//         lua_pop(g_L, 1);
//         RETURN_META(MRES_IGNORED);
//     }

//     // Map name
//     lua_pushstring(g_L, s1);
//     // Landmark name
//     lua_pushstring(g_L, s2);

//     if (lua_pcall(g_L, 2, 0, 0) != 0)
//     {
//         lua_pop(g_L, 1);
//     }

//     RETURN_META(MRES_IGNORED);
// }

// /* pfnGetSpawnParms() */
// void GetSpawnParms_Post(edict_t *ent)
// {
//     if (!g_L)
//         RETURN_META(MRES_IGNORED);

//     lua_getglobal(g_L, "MetaGetSpawnParms_Post");

//     if (!lua_isfunction(g_L, -1))
//     {
//         lua_pop(g_L, 1);
//         RETURN_META(MRES_IGNORED);
//     }

//     lua_pushentity(g_L, ent);

//     if (lua_pcall(g_L, 1, 0, 0) != 0)
//     {
//         lua_pop(g_L, 1);
//     }

//     RETURN_META(MRES_IGNORED);
// }

// /* pfnSaveSpawnParms() */
// void SaveSpawnParms_Post(edict_t *ent)
// {
//     if (!g_L)
//         RETURN_META(MRES_IGNORED);

//     lua_getglobal(g_L, "MetaSaveSpawnParms_Post");

//     if (!lua_isfunction(g_L, -1))
//     {
//         lua_pop(g_L, 1);
//         RETURN_META(MRES_IGNORED);
//     }

//     lua_pushentity(g_L, ent);

//     if (lua_pcall(g_L, 1, 0, 0) != 0)
//     {
//         lua_pop(g_L, 1);
//     }

//     RETURN_META(MRES_IGNORED);
// }

// /* pfnVecToYaw() */
// float VecToYaw_Post(const float *rgflVector)
// {
//     if (!g_L)
//         RETURN_META_VALUE(MRES_IGNORED, 0.0f);

//     lua_getglobal(g_L, "MetaVecToYaw_Post");

//     if (!lua_isfunction(g_L, -1))
//     {
//         lua_pop(g_L, 1);
//         RETURN_META_VALUE(MRES_IGNORED, 0.0f);
//     }

//     // Vector pointer
//     lua_pushlightuserdata(g_L, (void*)rgflVector);

//     if (lua_pcall(g_L, 1, 1, 0) != 0)
//     {
//         lua_pop(g_L, 1);
//         RETURN_META_VALUE(MRES_IGNORED, 0.0f);
//     }

//     // 如果 Lua 返回了数值，覆盖引擎计算的 Yaw
//     if (lua_isnumber(g_L, -1))
//     {
//         float ret = (float)lua_tonumber(g_L, -1);
//         lua_pop(g_L, 1);
//         RETURN_META_VALUE(MRES_SUPERCEDE, ret);
//     }

//     lua_pop(g_L, 1);
//     RETURN_META_VALUE(MRES_IGNORED, 0.0f);
// }
// /* pfnVecToAngles() */
// void VecToAngles_Post(const float *rgflVectorIn, float *rgflVectorOut)
// {
//     if (!g_L)
//         RETURN_META(MRES_IGNORED);

//     lua_getglobal(g_L, "MetaVecToAngles_Post");

//     if (!lua_isfunction(g_L, -1))
//     {
//         lua_pop(g_L, 1);
//         RETURN_META(MRES_IGNORED);
//     }

//     // Push: VectorIn ptr, VectorOut ptr
//     lua_pushlightuserdata(g_L, (void*)rgflVectorIn);
//     lua_pushlightuserdata(g_L, (void*)rgflVectorOut);

//     if (lua_pcall(g_L, 2, 0, 0) != 0)
//     {
//         lua_pop(g_L, 1);
//     }

//     RETURN_META(MRES_IGNORED);
// }

// /* pfnMoveToOrigin() */
// void MoveToOrigin_Post(edict_t *ent, const float *pflGoal, float dist, int iMoveType)
// {
//     if (!g_L)
//         RETURN_META(MRES_IGNORED);

//     lua_getglobal(g_L, "MetaMoveToOrigin_Post");

//     if (!lua_isfunction(g_L, -1))
//     {
//         lua_pop(g_L, 1);
//         RETURN_META(MRES_IGNORED);
//     }

//     lua_pushentity(g_L, ent);
//     lua_pushlightuserdata(g_L, (void*)pflGoal); // Goal Vector
//     lua_pushnumber(g_L, dist);
//     lua_pushinteger(g_L, iMoveType);

//     if (lua_pcall(g_L, 4, 0, 0) != 0)
//     {
//         lua_pop(g_L, 1);
//     }

//     RETURN_META(MRES_IGNORED);
// }

// /* pfnChangeYaw() */
// void ChangeYaw_Post(edict_t *ent)
// {
//     if (!g_L)
//         RETURN_META(MRES_IGNORED);

//     lua_getglobal(g_L, "MetaChangeYaw_Post");

//     if (!lua_isfunction(g_L, -1))
//     {
//         lua_pop(g_L, 1);
//         RETURN_META(MRES_IGNORED);
//     }

//     lua_pushentity(g_L, ent);

//     if (lua_pcall(g_L, 1, 0, 0) != 0)
//     {
//         lua_pop(g_L, 1);
//     }

//     RETURN_META(MRES_IGNORED);
// }

// /* pfnChangePitch() */
// void ChangePitch_Post(edict_t *ent)
// {
//     if (!g_L)
//         RETURN_META(MRES_IGNORED);

//     lua_getglobal(g_L, "MetaChangePitch_Post");

//     if (!lua_isfunction(g_L, -1))
//     {
//         lua_pop(g_L, 1);
//         RETURN_META(MRES_IGNORED);
//     }

//     lua_pushentity(g_L, ent);

//     if (lua_pcall(g_L, 1, 0, 0) != 0)
//     {
//         lua_pop(g_L, 1);
//     }

//     RETURN_META(MRES_IGNORED);
// }

// /* pfnFindEntityByString() */
// edict_t* FindEntityByString_Post(edict_t *pEdictStartSearchAfter, const char *pszField, const char *pszValue)
// {
//     if (!g_L)
//         RETURN_META_VALUE(MRES_IGNORED, NULL);

//     lua_getglobal(g_L, "MetaFindEntityByString_Post");

//     if (!lua_isfunction(g_L, -1))
//     {
//         lua_pop(g_L, 1);
//         RETURN_META_VALUE(MRES_IGNORED, NULL);
//     }

//     lua_pushentity(g_L, pEdictStartSearchAfter);
//     lua_pushstring(g_L, pszField);
//     lua_pushstring(g_L, pszValue);

//     if (lua_pcall(g_L, 3, 1, 0) != 0)
//     {
//         lua_pop(g_L, 1);
//         RETURN_META_VALUE(MRES_IGNORED, NULL);
//     }

//     // 如果 Lua 返回数字 (Index)，我们将其转换为 edict_t* 并返回
//     if (lua_isnumber(g_L, -1))
//     {
//         int entIndex = (int)lua_tointeger(g_L, -1);
//         lua_pop(g_L, 1);
        
//         edict_t* retEnt = NULL;
//         if (entIndex > 0)
//         {
//             retEnt = g_engfuncs.pfnPEntityOfEntIndex(entIndex);
//         }
        
//         // 如果 Index 是 0 或无效，retEnt 为 NULL，符合 FindEntityByString 的“未找到”语义
//         RETURN_META_VALUE(MRES_SUPERCEDE, retEnt);
//     }

//     lua_pop(g_L, 1);
//     RETURN_META_VALUE(MRES_IGNORED, NULL);
// }

// /* pfnGetEntityIllum() */
// int GetEntityIllum_Post(edict_t* pEnt)
// {
//     if (!g_L)
//         RETURN_META_VALUE(MRES_IGNORED, 0);

//     lua_getglobal(g_L, "MetaGetEntityIllum_Post");

//     if (!lua_isfunction(g_L, -1))
//     {
//         lua_pop(g_L, 1);
//         RETURN_META_VALUE(MRES_IGNORED, 0);
//     }

//     lua_pushentity(g_L, pEnt);

//     if (lua_pcall(g_L, 1, 1, 0) != 0)
//     {
//         lua_pop(g_L, 1);
//         RETURN_META_VALUE(MRES_IGNORED, 0);
//     }

//     if (lua_isnumber(g_L, -1))
//     {
//         int ret = (int)lua_tointeger(g_L, -1);
//         lua_pop(g_L, 1);
//         RETURN_META_VALUE(MRES_SUPERCEDE, ret);
//     }

//     lua_pop(g_L, 1);
//     RETURN_META_VALUE(MRES_IGNORED, 0);
// }

// /* pfnFindEntityInSphere() */
// edict_t* FindEntityInSphere_Post(edict_t *pEdictStartSearchAfter, const float *org, float rad)
// {
//     if (!g_L)
//         RETURN_META_VALUE(MRES_IGNORED, NULL);

//     lua_getglobal(g_L, "MetaFindEntityInSphere_Post");

//     if (!lua_isfunction(g_L, -1))
//     {
//         lua_pop(g_L, 1);
//         RETURN_META_VALUE(MRES_IGNORED, NULL);
//     }

//     lua_pushentity(g_L, pEdictStartSearchAfter);
//     lua_pushlightuserdata(g_L, (void*)org); // Origin Vector
//     lua_pushnumber(g_L, rad);

//     if (lua_pcall(g_L, 3, 1, 0) != 0)
//     {
//         lua_pop(g_L, 1);
//         RETURN_META_VALUE(MRES_IGNORED, NULL);
//     }

//     // Lua 返回找到的 Entity Index
//     if (lua_isnumber(g_L, -1))
//     {
//         int entIndex = (int)lua_tointeger(g_L, -1);
//         lua_pop(g_L, 1);

//         edict_t* retEnt = NULL;
//         if (entIndex > 0)
//             retEnt = g_engfuncs.pfnPEntityOfEntIndex(entIndex);

//         RETURN_META_VALUE(MRES_SUPERCEDE, retEnt);
//     }

//     lua_pop(g_L, 1);
//     RETURN_META_VALUE(MRES_IGNORED, NULL);
// }

// /* pfnFindClientInPVS() */
// edict_t* FindClientInPVS_Post(edict_t *pEdict)
// {
//     if (!g_L)
//         RETURN_META_VALUE(MRES_IGNORED, NULL);

//     lua_getglobal(g_L, "MetaFindClientInPVS_Post");

//     if (!lua_isfunction(g_L, -1))
//     {
//         lua_pop(g_L, 1);
//         RETURN_META_VALUE(MRES_IGNORED, NULL);
//     }

//     lua_pushentity(g_L, pEdict);

//     if (lua_pcall(g_L, 1, 1, 0) != 0)
//     {
//         lua_pop(g_L, 1);
//         RETURN_META_VALUE(MRES_IGNORED, NULL);
//     }

//     if (lua_isnumber(g_L, -1))
//     {
//         int entIndex = (int)lua_tointeger(g_L, -1);
//         lua_pop(g_L, 1);

//         edict_t* retEnt = NULL;
//         if (entIndex > 0)
//             retEnt = g_engfuncs.pfnPEntityOfEntIndex(entIndex);

//         RETURN_META_VALUE(MRES_SUPERCEDE, retEnt);
//     }

//     lua_pop(g_L, 1);
//     RETURN_META_VALUE(MRES_IGNORED, NULL);
// }

// /* pfnEntitiesInPVS() */
// edict_t* EntitiesInPVS_Post(edict_t *pplayer)
// {
//     if (!g_L)
//         RETURN_META_VALUE(MRES_IGNORED, NULL);

//     lua_getglobal(g_L, "MetaEntitiesInPVS_Post");

//     if (!lua_isfunction(g_L, -1))
//     {
//         lua_pop(g_L, 1);
//         RETURN_META_VALUE(MRES_IGNORED, NULL);
//     }

//     lua_pushentity(g_L, pplayer);

//     if (lua_pcall(g_L, 1, 1, 0) != 0)
//     {
//         lua_pop(g_L, 1);
//         RETURN_META_VALUE(MRES_IGNORED, NULL);
//     }

//     // 注意：EntitiesInPVS 在 HL 引擎中返回的是一个链表头或者第一个实体
//     if (lua_isnumber(g_L, -1))
//     {
//         int entIndex = (int)lua_tointeger(g_L, -1);
//         lua_pop(g_L, 1);

//         edict_t* retEnt = NULL;
//         if (entIndex > 0)
//             retEnt = g_engfuncs.pfnPEntityOfEntIndex(entIndex);

//         RETURN_META_VALUE(MRES_SUPERCEDE, retEnt);
//     }

//     lua_pop(g_L, 1);
//     RETURN_META_VALUE(MRES_IGNORED, NULL);
// }

// /* pfnMakeVectors() */
// void MakeVectors_Post(const float *rgflVector)
// {
//     if (!g_L)
//         RETURN_META(MRES_IGNORED);

//     lua_getglobal(g_L, "MetaMakeVectors_Post");

//     if (!lua_isfunction(g_L, -1))
//     {
//         lua_pop(g_L, 1);
//         RETURN_META(MRES_IGNORED);
//     }

//     lua_pushlightuserdata(g_L, (void*)rgflVector);

//     if (lua_pcall(g_L, 1, 0, 0) != 0)
//     {
//         lua_pop(g_L, 1);
//     }

//     RETURN_META(MRES_IGNORED);
// }

// /* pfnAngleVectors() */
// void AngleVectors_Post(const float *rgflVector, float *forward, float *right, float *up)
// {
//     if (!g_L)
//         RETURN_META(MRES_IGNORED);

//     lua_getglobal(g_L, "MetaAngleVectors_Post");

//     if (!lua_isfunction(g_L, -1))
//     {
//         lua_pop(g_L, 1);
//         RETURN_META(MRES_IGNORED);
//     }

//     // Push inputs and outputs as pointers
//     lua_pushlightuserdata(g_L, (void*)rgflVector);
//     lua_pushlightuserdata(g_L, (void*)forward);
//     lua_pushlightuserdata(g_L, (void*)right);
//     lua_pushlightuserdata(g_L, (void*)up);

//     if (lua_pcall(g_L, 4, 0, 0) != 0)
//     {
//         lua_pop(g_L, 1);
//     }

//     RETURN_META(MRES_IGNORED);
// }

// /* pfnCreateEntity() */
// edict_t* CreateEntity_Post(void)
// {
//     if (!g_L)
//         RETURN_META_VALUE(MRES_IGNORED, NULL);

//     lua_getglobal(g_L, "MetaCreateEntity_Post");

//     if (!lua_isfunction(g_L, -1))
//     {
//         lua_pop(g_L, 1);
//         RETURN_META_VALUE(MRES_IGNORED, NULL);
//     }

//     if (lua_pcall(g_L, 0, 1, 0) != 0)
//     {
//         lua_pop(g_L, 1);
//         RETURN_META_VALUE(MRES_IGNORED, NULL);
//     }

//     // Lua 返回 Entity Index
//     if (lua_isnumber(g_L, -1))
//     {
//         int entIndex = (int)lua_tointeger(g_L, -1);
//         lua_pop(g_L, 1);

//         edict_t* retEnt = NULL;
//         if (entIndex > 0)
//             retEnt = g_engfuncs.pfnPEntityOfEntIndex(entIndex);
        
//         RETURN_META_VALUE(MRES_SUPERCEDE, retEnt);
//     }

//     lua_pop(g_L, 1);
//     RETURN_META_VALUE(MRES_IGNORED, NULL);
// }

// /* pfnRemoveEntity() */
// void RemoveEntity_Post(edict_t* e)
// {
//     if (!g_L)
//         RETURN_META(MRES_IGNORED);

//     lua_getglobal(g_L, "MetaRemoveEntity_Post");

//     if (!lua_isfunction(g_L, -1))
//     {
//         lua_pop(g_L, 1);
//         RETURN_META(MRES_IGNORED);
//     }

//     lua_pushentity(g_L, e);

//     if (lua_pcall(g_L, 1, 0, 0) != 0)
//     {
//         lua_pop(g_L, 1);
//     }

//     RETURN_META(MRES_IGNORED);
// }

// /* pfnCreateNamedEntity() */
// edict_t* CreateNamedEntity_Post(int className)
// {
//     if (!g_L)
//         RETURN_META_VALUE(MRES_IGNORED, NULL);

//     lua_getglobal(g_L, "MetaCreateNamedEntity_Post");

//     if (!lua_isfunction(g_L, -1))
//     {
//         lua_pop(g_L, 1);
//         RETURN_META_VALUE(MRES_IGNORED, NULL);
//     }

//     // className 是字符串索引 (AllocString 的结果)
//     lua_pushinteger(g_L, className);

//     if (lua_pcall(g_L, 1, 1, 0) != 0)
//     {
//         lua_pop(g_L, 1);
//         RETURN_META_VALUE(MRES_IGNORED, NULL);
//     }

//     // Lua 返回 Entity Index
//     if (lua_isnumber(g_L, -1))
//     {
//         int entIndex = (int)lua_tointeger(g_L, -1);
//         lua_pop(g_L, 1);

//         edict_t* retEnt = NULL;
//         if (entIndex > 0)
//             retEnt = g_engfuncs.pfnPEntityOfEntIndex(entIndex);

//         RETURN_META_VALUE(MRES_SUPERCEDE, retEnt);
//     }

//     lua_pop(g_L, 1);
//     RETURN_META_VALUE(MRES_IGNORED, NULL);
// }

// /* pfnMakeStatic() */
// void MakeStatic_Post(edict_t *ent)
// {
//     if (!g_L)
//         RETURN_META(MRES_IGNORED);

//     lua_getglobal(g_L, "MetaMakeStatic_Post");

//     if (!lua_isfunction(g_L, -1))
//     {
//         lua_pop(g_L, 1);
//         RETURN_META(MRES_IGNORED);
//     }

//     lua_pushentity(g_L, ent);

//     if (lua_pcall(g_L, 1, 0, 0) != 0)
//     {
//         lua_pop(g_L, 1);
//     }

//     RETURN_META(MRES_IGNORED);
// }

// /* pfnEntIsOnFloor() */
// int EntIsOnFloor_Post(edict_t *e)
// {
//     if (!g_L)
//         RETURN_META_VALUE(MRES_IGNORED, 0);

//     lua_getglobal(g_L, "MetaEntIsOnFloor_Post");

//     if (!lua_isfunction(g_L, -1))
//     {
//         lua_pop(g_L, 1);
//         RETURN_META_VALUE(MRES_IGNORED, 0);
//     }

//     lua_pushentity(g_L, e);

//     if (lua_pcall(g_L, 1, 1, 0) != 0)
//     {
//         lua_pop(g_L, 1);
//         RETURN_META_VALUE(MRES_IGNORED, 0);
//     }

//     if (lua_isnumber(g_L, -1))
//     {
//         int ret = (int)lua_tointeger(g_L, -1);
//         lua_pop(g_L, 1);
//         RETURN_META_VALUE(MRES_SUPERCEDE, ret);
//     }

//     lua_pop(g_L, 1);
//     RETURN_META_VALUE(MRES_IGNORED, 0);
// }

// /* pfnDropToFloor() */
// int DropToFloor_Post(edict_t* e)
// {
//     if (!g_L)
//         RETURN_META_VALUE(MRES_IGNORED, 0);

//     lua_getglobal(g_L, "MetaDropToFloor_Post");

//     if (!lua_isfunction(g_L, -1))
//     {
//         lua_pop(g_L, 1);
//         RETURN_META_VALUE(MRES_IGNORED, 0);
//     }

//     lua_pushentity(g_L, e);

//     if (lua_pcall(g_L, 1, 1, 0) != 0)
//     {
//         lua_pop(g_L, 1);
//         RETURN_META_VALUE(MRES_IGNORED, 0);
//     }

//     if (lua_isnumber(g_L, -1))
//     {
//         int ret = (int)lua_tointeger(g_L, -1);
//         lua_pop(g_L, 1);
//         RETURN_META_VALUE(MRES_SUPERCEDE, ret);
//     }

//     lua_pop(g_L, 1);
//     RETURN_META_VALUE(MRES_IGNORED, 0);
// }

// /* pfnWalkMove() */
// int WalkMove_Post(edict_t *ent, float yaw, float dist, int iMode)
// {
//     if (!g_L)
//         RETURN_META_VALUE(MRES_IGNORED, 0);

//     lua_getglobal(g_L, "MetaWalkMove_Post");

//     if (!lua_isfunction(g_L, -1))
//     {
//         lua_pop(g_L, 1);
//         RETURN_META_VALUE(MRES_IGNORED, 0);
//     }

//     lua_pushentity(g_L, ent);
//     lua_pushnumber(g_L, yaw);
//     lua_pushnumber(g_L, dist);
//     lua_pushinteger(g_L, iMode);

//     if (lua_pcall(g_L, 4, 1, 0) != 0)
//     {
//         lua_pop(g_L, 1);
//         RETURN_META_VALUE(MRES_IGNORED, 0);
//     }

//     if (lua_isnumber(g_L, -1))
//     {
//         int ret = (int)lua_tointeger(g_L, -1);
//         lua_pop(g_L, 1);
//         RETURN_META_VALUE(MRES_SUPERCEDE, ret);
//     }

//     lua_pop(g_L, 1);
//     RETURN_META_VALUE(MRES_IGNORED, 0);
// }

// /* pfnSetOrigin() */
// void SetOrigin_Post(edict_t *e, const float *rgflOrigin)
// {
//     if (!g_L)
//         RETURN_META(MRES_IGNORED);

//     lua_getglobal(g_L, "MetaSetOrigin_Post");

//     if (!lua_isfunction(g_L, -1))
//     {
//         lua_pop(g_L, 1);
//         RETURN_META(MRES_IGNORED);
//     }

//     lua_pushentity(g_L, e);
//     lua_pushlightuserdata(g_L, (void*)rgflOrigin); // Vector ptr

//     if (lua_pcall(g_L, 2, 0, 0) != 0)
//     {
//         lua_pop(g_L, 1);
//     }

//     RETURN_META(MRES_IGNORED);
// }

// /* pfnEmitSound() */
// void EmitSound_Post(edict_t *entity, int channel, const char *sample, float volume, float attenuation, int fFlags, int pitch)
// {
//     if (!g_L)
//         RETURN_META(MRES_IGNORED);

//     lua_getglobal(g_L, "MetaEmitSound_Post");

//     if (!lua_isfunction(g_L, -1))
//     {
//         lua_pop(g_L, 1);
//         RETURN_META(MRES_IGNORED);
//     }

//     lua_pushentity(g_L, entity);
//     lua_pushinteger(g_L, channel);
//     lua_pushstring(g_L, sample);
//     lua_pushnumber(g_L, volume);
//     lua_pushnumber(g_L, attenuation);
//     lua_pushinteger(g_L, fFlags);
//     lua_pushinteger(g_L, pitch);

//     if (lua_pcall(g_L, 7, 0, 0) != 0)
//     {
//         lua_pop(g_L, 1);
//     }

//     RETURN_META(MRES_IGNORED);
// }
// /* pfnEmitAmbientSound() */
// void EmitAmbientSound_Post(edict_t *entity, float *pos, const char *samp, float vol, float attenuation, int fFlags, int pitch)
// {
//     if (!g_L)
//         RETURN_META(MRES_IGNORED);

//     lua_getglobal(g_L, "MetaEmitAmbientSound_Post");

//     if (!lua_isfunction(g_L, -1))
//     {
//         lua_pop(g_L, 1);
//         RETURN_META(MRES_IGNORED);
//     }

//     lua_pushentity(g_L, entity);
//     lua_pushlightuserdata(g_L, (void*)pos); // Vector ptr
//     lua_pushstring(g_L, samp);
//     lua_pushnumber(g_L, vol);
//     lua_pushnumber(g_L, attenuation);
//     lua_pushinteger(g_L, fFlags);
//     lua_pushinteger(g_L, pitch);

//     if (lua_pcall(g_L, 7, 0, 0) != 0)
//     {
//         lua_pop(g_L, 1);
//     }

//     RETURN_META(MRES_IGNORED);
// }

// /* pfnTraceLine() */
// void TraceLine_Post(const float *v1, const float *v2, int fNoMonsters, edict_t *pentToSkip, TraceResult *ptr)
// {
//     if (!g_L)
//         RETURN_META(MRES_IGNORED);

//     lua_getglobal(g_L, "MetaTraceLine_Post");

//     if (!lua_isfunction(g_L, -1))
//     {
//         lua_pop(g_L, 1);
//         RETURN_META(MRES_IGNORED);
//     }

//     lua_pushlightuserdata(g_L, (void*)v1);
//     lua_pushlightuserdata(g_L, (void*)v2);
//     lua_pushinteger(g_L, fNoMonsters);
//     lua_pushentity(g_L, pentToSkip);
//     lua_pushlightuserdata(g_L, (void*)ptr); // TraceResult ptr

//     if (lua_pcall(g_L, 5, 0, 0) != 0)
//     {
//         lua_pop(g_L, 1);
//     }

//     RETURN_META(MRES_IGNORED);
// }

// /* pfnTraceToss() */
// void TraceToss_Post(edict_t* pent, edict_t* pentToIgnore, TraceResult *ptr)
// {
//     if (!g_L)
//         RETURN_META(MRES_IGNORED);

//     lua_getglobal(g_L, "MetaTraceToss_Post");

//     if (!lua_isfunction(g_L, -1))
//     {
//         lua_pop(g_L, 1);
//         RETURN_META(MRES_IGNORED);
//     }

//     lua_pushentity(g_L, pent);
//     lua_pushentity(g_L, pentToIgnore);
//     lua_pushlightuserdata(g_L, (void*)ptr);

//     if (lua_pcall(g_L, 3, 0, 0) != 0)
//     {
//         lua_pop(g_L, 1);
//     }

//     RETURN_META(MRES_IGNORED);
// }

// /* pfnTraceMonsterHull() */
// int TraceMonsterHull_Post(edict_t *pEdict, const float *v1, const float *v2, int fNoMonsters, edict_t *pentToSkip, TraceResult *ptr)
// {
//     if (!g_L)
//         RETURN_META_VALUE(MRES_IGNORED, 0);

//     lua_getglobal(g_L, "MetaTraceMonsterHull_Post");

//     if (!lua_isfunction(g_L, -1))
//     {
//         lua_pop(g_L, 1);
//         RETURN_META_VALUE(MRES_IGNORED, 0);
//     }

//     lua_pushentity(g_L, pEdict);
//     lua_pushlightuserdata(g_L, (void*)v1);
//     lua_pushlightuserdata(g_L, (void*)v2);
//     lua_pushinteger(g_L, fNoMonsters);
//     lua_pushentity(g_L, pentToSkip);
//     lua_pushlightuserdata(g_L, (void*)ptr);

//     if (lua_pcall(g_L, 6, 1, 0) != 0)
//     {
//         lua_pop(g_L, 1);
//         RETURN_META_VALUE(MRES_IGNORED, 0);
//     }

//     if (lua_isnumber(g_L, -1))
//     {
//         int ret = (int)lua_tointeger(g_L, -1);
//         lua_pop(g_L, 1);
//         RETURN_META_VALUE(MRES_SUPERCEDE, ret);
//     }

//     lua_pop(g_L, 1);
//     RETURN_META_VALUE(MRES_IGNORED, 0);
// }

// /* pfnTraceHull() */
// void TraceHull_Post(const float *v1, const float *v2, int fNoMonsters, int hullNumber, edict_t *pentToSkip, TraceResult *ptr)
// {
//     if (!g_L)
//         RETURN_META(MRES_IGNORED);

//     lua_getglobal(g_L, "MetaTraceHull_Post");

//     if (!lua_isfunction(g_L, -1))
//     {
//         lua_pop(g_L, 1);
//         RETURN_META(MRES_IGNORED);
//     }

//     lua_pushlightuserdata(g_L, (void*)v1);
//     lua_pushlightuserdata(g_L, (void*)v2);
//     lua_pushinteger(g_L, fNoMonsters);
//     lua_pushinteger(g_L, hullNumber);
//     lua_pushentity(g_L, pentToSkip);
//     lua_pushlightuserdata(g_L, (void*)ptr);

//     if (lua_pcall(g_L, 6, 0, 0) != 0)
//     {
//         lua_pop(g_L, 1);
//     }

//     RETURN_META(MRES_IGNORED);
// }

// /* pfnTraceModel() */
// void TraceModel_Post(const float *v1, const float *v2, int hullNumber, edict_t *pent, TraceResult *ptr)
// {
//     if (!g_L)
//         RETURN_META(MRES_IGNORED);

//     lua_getglobal(g_L, "MetaTraceModel_Post");

//     if (!lua_isfunction(g_L, -1))
//     {
//         lua_pop(g_L, 1);
//         RETURN_META(MRES_IGNORED);
//     }

//     lua_pushlightuserdata(g_L, (void*)v1);
//     lua_pushlightuserdata(g_L, (void*)v2);
//     lua_pushinteger(g_L, hullNumber);
//     lua_pushentity(g_L, pent);
//     lua_pushlightuserdata(g_L, (void*)ptr);

//     if (lua_pcall(g_L, 5, 0, 0) != 0)
//     {
//         lua_pop(g_L, 1);
//     }

//     RETURN_META(MRES_IGNORED);
// }

// /* pfnTraceTexture() */
// const char *TraceTexture_Post(edict_t *pTextureEntity, const float *v1, const float *v2)
// {
//     if (!g_L)
//         RETURN_META_VALUE(MRES_IGNORED, NULL);

//     lua_getglobal(g_L, "MetaTraceTexture_Post");

//     if (!lua_isfunction(g_L, -1))
//     {
//         lua_pop(g_L, 1);
//         RETURN_META_VALUE(MRES_IGNORED, NULL);
//     }

//     lua_pushentity(g_L, pTextureEntity);
//     lua_pushlightuserdata(g_L, (void*)v1);
//     lua_pushlightuserdata(g_L, (void*)v2);

//     if (lua_pcall(g_L, 3, 1, 0) != 0)
//     {
//         lua_pop(g_L, 1);
//         RETURN_META_VALUE(MRES_IGNORED, NULL);
//     }

//     if (lua_isstring(g_L, -1))
//     {
//         // 静态缓冲区用于存储纹理名称返回给引擎
//         static char staticTextureName[64];
//         const char *ret = lua_tostring(g_L, -1);
        
//         strncpy(staticTextureName, ret, 63);
//         staticTextureName[63] = '\0';

//         lua_pop(g_L, 1);
//         RETURN_META_VALUE(MRES_SUPERCEDE, staticTextureName);
//     }

//     lua_pop(g_L, 1);
//     RETURN_META_VALUE(MRES_IGNORED, NULL);
// }

// /* pfnTraceSphere() */
// void TraceSphere_Post(const float *v1, const float *v2, int fNoMonsters, float radius, edict_t *pentToSkip, TraceResult *ptr)
// {
//     if (!g_L)
//         RETURN_META(MRES_IGNORED);

//     lua_getglobal(g_L, "MetaTraceSphere_Post");

//     if (!lua_isfunction(g_L, -1))
//     {
//         lua_pop(g_L, 1);
//         RETURN_META(MRES_IGNORED);
//     }

//     lua_pushlightuserdata(g_L, (void*)v1);
//     lua_pushlightuserdata(g_L, (void*)v2);
//     lua_pushinteger(g_L, fNoMonsters);
//     lua_pushnumber(g_L, radius);
//     lua_pushentity(g_L, pentToSkip);
//     lua_pushlightuserdata(g_L, (void*)ptr);

//     if (lua_pcall(g_L, 6, 0, 0) != 0)
//     {
//         lua_pop(g_L, 1);
//     }

//     RETURN_META(MRES_IGNORED);
// }

// /* pfnGetAimVector() */
// void GetAimVector_Post(edict_t* ent, float speed, float *rgflReturn)
// {
//     if (!g_L)
//         RETURN_META(MRES_IGNORED);

//     lua_getglobal(g_L, "MetaGetAimVector_Post");

//     if (!lua_isfunction(g_L, -1))
//     {
//         lua_pop(g_L, 1);
//         RETURN_META(MRES_IGNORED);
//     }

//     lua_pushentity(g_L, ent);
//     lua_pushnumber(g_L, speed);
//     lua_pushlightuserdata(g_L, (void*)rgflReturn); // Vector Output ptr

//     if (lua_pcall(g_L, 3, 0, 0) != 0)
//     {
//         lua_pop(g_L, 1);
//     }

//     RETURN_META(MRES_IGNORED);
// }

// /* pfnServerCommand() */
// // void ServerCommand_Post(const char* str)
// // {
// //     if (!g_L)
// //         RETURN_META(MRES_IGNORED);

// //     lua_getglobal(g_L, "MetaServerCommand_Post");

// //     if (!lua_isfunction(g_L, -1))
// //     {
// //         lua_pop(g_L, 1);
// //         RETURN_META(MRES_IGNORED);
// //     }

// //     lua_pushstring(g_L, str);

// //     if (lua_pcall(g_L, 1, 0, 0) != 0)
// //     {
// //         lua_pop(g_L, 1);
// //     }

// //     RETURN_META(MRES_IGNORED);
// // }

// /* pfnServerExecute() */
// void ServerExecute_Post(void)
// {
//     if (!g_L)
//         RETURN_META(MRES_IGNORED);

//     lua_getglobal(g_L, "MetaServerExecute_Post");

//     if (!lua_isfunction(g_L, -1))
//     {
//         lua_pop(g_L, 1);
//         RETURN_META(MRES_IGNORED);
//     }

//     if (lua_pcall(g_L, 0, 0, 0) != 0)
//     {
//         lua_pop(g_L, 1);
//     }

//     RETURN_META(MRES_IGNORED);
// }

// /* pfnClientCommand() (Engine Version) */
// // void engClientCommand_Post(edict_t* pEdict, const char* szFmt, ...)
// // {
// //     if (!g_L)
// //         RETURN_META(MRES_IGNORED);

// //     lua_getglobal(g_L, "MetaEngClientCommand_Post");

// //     if (!lua_isfunction(g_L, -1))
// //     {
// //         lua_pop(g_L, 1);
// //         RETURN_META(MRES_IGNORED);
// //     }

// //     lua_pushentity(g_L, pEdict);

// //     // 处理变参: 将格式化字符串和参数合并成最终的命令字符串
// //     static char command_buffer[1024];
// //     va_list argptr;
// //     va_start(argptr, szFmt);
// //     vsnprintf(command_buffer, sizeof(command_buffer), szFmt, argptr);
// //     va_end(argptr);

// //     lua_pushstring(g_L, command_buffer);

// //     if (lua_pcall(g_L, 2, 0, 0) != 0)
// //     {
// //         lua_pop(g_L, 1);
// //     }

// //     RETURN_META(MRES_IGNORED);
// // }

// /* pfnParticleEffect() */
// void ParticleEffect_Post(const float *org, const float *dir, float color, float count)
// {
//     if (!g_L)
//         RETURN_META(MRES_IGNORED);

//     lua_getglobal(g_L, "MetaParticleEffect_Post");

//     if (!lua_isfunction(g_L, -1))
//     {
//         lua_pop(g_L, 1);
//         RETURN_META(MRES_IGNORED);
//     }

//     lua_pushlightuserdata(g_L, (void*)org);
//     lua_pushlightuserdata(g_L, (void*)dir);
//     lua_pushnumber(g_L, color);
//     lua_pushnumber(g_L, count);

//     if (lua_pcall(g_L, 4, 0, 0) != 0)
//     {
//         lua_pop(g_L, 1);
//     }

//     RETURN_META(MRES_IGNORED);
// }

// /* pfnLightStyle() */
// void LightStyle_Post(int style, const char* val)
// {
//     if (!g_L)
//         RETURN_META(MRES_IGNORED);

//     lua_getglobal(g_L, "MetaLightStyle_Post");

//     if (!lua_isfunction(g_L, -1))
//     {
//         lua_pop(g_L, 1);
//         RETURN_META(MRES_IGNORED);
//     }

//     lua_pushinteger(g_L, style);
//     lua_pushstring(g_L, val);

//     if (lua_pcall(g_L, 2, 0, 0) != 0)
//     {
//         lua_pop(g_L, 1);
//     }

//     RETURN_META(MRES_IGNORED);
// }

// /* pfnDecalIndex() */
// int DecalIndex_Post(const char *name)
// {
//     if (!g_L)
//         RETURN_META_VALUE(MRES_IGNORED, 0);

//     lua_getglobal(g_L, "MetaDecalIndex_Post");

//     if (!lua_isfunction(g_L, -1))
//     {
//         lua_pop(g_L, 1);
//         RETURN_META_VALUE(MRES_IGNORED, 0);
//     }

//     lua_pushstring(g_L, name);

//     if (lua_pcall(g_L, 1, 1, 0) != 0)
//     {
//         lua_pop(g_L, 1);
//         RETURN_META_VALUE(MRES_IGNORED, 0);
//     }

//     if (lua_isnumber(g_L, -1))
//     {
//         int ret = (int)lua_tointeger(g_L, -1);
//         lua_pop(g_L, 1);
//         RETURN_META_VALUE(MRES_SUPERCEDE, ret);
//     }

//     lua_pop(g_L, 1);
//     RETURN_META_VALUE(MRES_IGNORED, 0);
// }

// /* pfnPointContents() */
// int PointContents_Post(const float *rgflVector)
// {
//     if (!g_L)
//         RETURN_META_VALUE(MRES_IGNORED, 0);

//     lua_getglobal(g_L, "MetaPointContents_Post");

//     if (!lua_isfunction(g_L, -1))
//     {
//         lua_pop(g_L, 1);
//         RETURN_META_VALUE(MRES_IGNORED, 0);
//     }

//     lua_pushlightuserdata(g_L, (void*)rgflVector);

//     if (lua_pcall(g_L, 1, 1, 0) != 0)
//     {
//         lua_pop(g_L, 1);
//         RETURN_META_VALUE(MRES_IGNORED, 0);
//     }

//     if (lua_isnumber(g_L, -1))
//     {
//         int ret = (int)lua_tointeger(g_L, -1);
//         lua_pop(g_L, 1);
//         RETURN_META_VALUE(MRES_SUPERCEDE, ret);
//     }

//     lua_pop(g_L, 1);
//     RETURN_META_VALUE(MRES_IGNORED, 0);
// }

// /* pfnMessageBegin() */
// void MessageBegin_Post(int msg_dest, int msg_type, const float *pOrigin, edict_t *ed)
// {
//     if (!g_L)
//         RETURN_META(MRES_IGNORED);

//     lua_getglobal(g_L, "MetaMessageBegin_Post");

//     if (!lua_isfunction(g_L, -1))
//     {
//         lua_pop(g_L, 1);
//         RETURN_META(MRES_IGNORED);
//     }

//     lua_pushinteger(g_L, msg_dest);
//     lua_pushinteger(g_L, msg_type);
    
//     // pOrigin 可能是 NULL，传递 NULL 指针给 Lua
//     lua_pushlightuserdata(g_L, (void*)pOrigin);
    
//     lua_pushentity(g_L, ed);

//     if (lua_pcall(g_L, 4, 0, 0) != 0)
//     {
//         lua_pop(g_L, 1);
//     }

//     RETURN_META(MRES_IGNORED);
// }

// /* pfnMessageEnd() */
// void MessageEnd_Post(void)
// {
//     if (!g_L)
//         RETURN_META(MRES_IGNORED);

//     lua_getglobal(g_L, "MetaMessageEnd_Post");

//     if (!lua_isfunction(g_L, -1))
//     {
//         lua_pop(g_L, 1);
//         RETURN_META(MRES_IGNORED);
//     }

//     if (lua_pcall(g_L, 0, 0, 0) != 0)
//     {
//         lua_pop(g_L, 1);
//     }

//     RETURN_META(MRES_IGNORED);
// }

// /* pfnWriteByte() */
// void WriteByte_Post(int iValue)
// {
//     if (!g_L)
//         RETURN_META(MRES_IGNORED);

//     lua_getglobal(g_L, "MetaWriteByte_Post");

//     if (!lua_isfunction(g_L, -1))
//     {
//         lua_pop(g_L, 1);
//         RETURN_META(MRES_IGNORED);
//     }

//     lua_pushinteger(g_L, iValue);

//     if (lua_pcall(g_L, 1, 0, 0) != 0)
//     {
//         lua_pop(g_L, 1);
//     }

//     RETURN_META(MRES_IGNORED);
// }

// /* pfnWriteChar() */
// void WriteChar_Post(int iValue)
// {
//     if (!g_L)
//         RETURN_META(MRES_IGNORED);

//     lua_getglobal(g_L, "MetaWriteChar_Post");

//     if (!lua_isfunction(g_L, -1))
//     {
//         lua_pop(g_L, 1);
//         RETURN_META(MRES_IGNORED);
//     }

//     lua_pushinteger(g_L, iValue);

//     if (lua_pcall(g_L, 1, 0, 0) != 0)
//     {
//         lua_pop(g_L, 1);
//     }

//     RETURN_META(MRES_IGNORED);
// }

// /* pfnWriteShort() */
// void WriteShort_Post(int iValue)
// {
//     if (!g_L)
//         RETURN_META(MRES_IGNORED);

//     lua_getglobal(g_L, "MetaWriteShort_Post");

//     if (!lua_isfunction(g_L, -1))
//     {
//         lua_pop(g_L, 1);
//         RETURN_META(MRES_IGNORED);
//     }

//     lua_pushinteger(g_L, iValue);

//     if (lua_pcall(g_L, 1, 0, 0) != 0)
//     {
//         lua_pop(g_L, 1);
//     }

//     RETURN_META(MRES_IGNORED);
// }

// /* pfnWriteLong() */
// void WriteLong_Post(int iValue)
// {
//     if (!g_L)
//         RETURN_META(MRES_IGNORED);

//     lua_getglobal(g_L, "MetaWriteLong_Post");

//     if (!lua_isfunction(g_L, -1))
//     {
//         lua_pop(g_L, 1);
//         RETURN_META(MRES_IGNORED);
//     }

//     lua_pushinteger(g_L, iValue);

//     if (lua_pcall(g_L, 1, 0, 0) != 0)
//     {
//         lua_pop(g_L, 1);
//     }

//     RETURN_META(MRES_IGNORED);
// }

// /* pfnWriteAngle() */
// void WriteAngle_Post(float flValue)
// {
//     if (!g_L)
//         RETURN_META(MRES_IGNORED);

//     lua_getglobal(g_L, "MetaWriteAngle_Post");

//     if (!lua_isfunction(g_L, -1))
//     {
//         lua_pop(g_L, 1);
//         RETURN_META(MRES_IGNORED);
//     }

//     lua_pushnumber(g_L, flValue);

//     if (lua_pcall(g_L, 1, 0, 0) != 0)
//     {
//         lua_pop(g_L, 1);
//     }

//     RETURN_META(MRES_IGNORED);
// }

// /* pfnWriteCoord() */
// void WriteCoord_Post(float flValue)
// {
//     if (!g_L)
//         RETURN_META(MRES_IGNORED);

//     lua_getglobal(g_L, "MetaWriteCoord_Post");

//     if (!lua_isfunction(g_L, -1))
//     {
//         lua_pop(g_L, 1);
//         RETURN_META(MRES_IGNORED);
//     }

//     lua_pushnumber(g_L, flValue);

//     if (lua_pcall(g_L, 1, 0, 0) != 0)
//     {
//         lua_pop(g_L, 1);
//     }

//     RETURN_META(MRES_IGNORED);
// }

// /* pfnWriteString() */
// void WriteString_Post(const char *sz)
// {
//     if (!g_L)
//         RETURN_META(MRES_IGNORED);

//     lua_getglobal(g_L, "MetaWriteString_Post");

//     if (!lua_isfunction(g_L, -1))
//     {
//         lua_pop(g_L, 1);
//         RETURN_META(MRES_IGNORED);
//     }

//     lua_pushstring(g_L, sz);

//     if (lua_pcall(g_L, 1, 0, 0) != 0)
//     {
//         lua_pop(g_L, 1);
//     }

//     RETURN_META(MRES_IGNORED);
// }

// /* pfnWriteEntity() */
// void WriteEntity_Post(int iValue)
// {
//     if (!g_L)
//         RETURN_META(MRES_IGNORED);

//     lua_getglobal(g_L, "MetaWriteEntity_Post");

//     if (!lua_isfunction(g_L, -1))
//     {
//         lua_pop(g_L, 1);
//         RETURN_META(MRES_IGNORED);
//     }

//     lua_pushinteger(g_L, iValue);

//     if (lua_pcall(g_L, 1, 0, 0) != 0)
//     {
//         lua_pop(g_L, 1);
//     }

//     RETURN_META(MRES_IGNORED);
// }

// /* pfnCVarRegister() */
// void CVarRegister_Post(cvar_t *pCvar)
// {
//     if (!g_L)
//         RETURN_META(MRES_IGNORED);

//     lua_getglobal(g_L, "MetaCVarRegister_Post");

//     if (!lua_isfunction(g_L, -1))
//     {
//         lua_pop(g_L, 1);
//         RETURN_META(MRES_IGNORED);
//     }

//     lua_pushlightuserdata(g_L, (void*)pCvar);

//     if (lua_pcall(g_L, 1, 0, 0) != 0)
//     {
//         lua_pop(g_L, 1);
//     }

//     RETURN_META(MRES_IGNORED);
// }

// /* pfnCVarGetFloat() */
// float CVarGetFloat_Post(const char *szVarName)
// {
//     if (!g_L)
//         RETURN_META_VALUE(MRES_IGNORED, 0.0f);

//     lua_getglobal(g_L, "MetaCVarGetFloat_Post");

//     if (!lua_isfunction(g_L, -1))
//     {
//         lua_pop(g_L, 1);
//         RETURN_META_VALUE(MRES_IGNORED, 0.0f);
//     }

//     lua_pushstring(g_L, szVarName);

//     if (lua_pcall(g_L, 1, 1, 0) != 0)
//     {
//         lua_pop(g_L, 1);
//         RETURN_META_VALUE(MRES_IGNORED, 0.0f);
//     }

//     if (lua_isnumber(g_L, -1))
//     {
//         float ret = (float)lua_tonumber(g_L, -1);
//         lua_pop(g_L, 1);
//         RETURN_META_VALUE(MRES_SUPERCEDE, ret);
//     }

//     lua_pop(g_L, 1);
//     RETURN_META_VALUE(MRES_IGNORED, 0.0f);
// }
// /* pfnCVarGetString() */
// const char* CVarGetString_Post(const char *szVarName)
// {
//     if (!g_L)
//         RETURN_META_VALUE(MRES_IGNORED, NULL);

//     lua_getglobal(g_L, "MetaCVarGetString_Post");

//     if (!lua_isfunction(g_L, -1))
//     {
//         lua_pop(g_L, 1);
//         RETURN_META_VALUE(MRES_IGNORED, NULL);
//     }

//     lua_pushstring(g_L, szVarName);

//     if (lua_pcall(g_L, 1, 1, 0) != 0)
//     {
//         lua_pop(g_L, 1);
//         RETURN_META_VALUE(MRES_IGNORED, NULL);
//     }

//     if (lua_isstring(g_L, -1))
//     {
//         static char staticBuf[256];
//         const char *ret = lua_tostring(g_L, -1);
        
//         strncpy(staticBuf, ret, 255);
//         staticBuf[255] = '\0';

//         lua_pop(g_L, 1);
//         RETURN_META_VALUE(MRES_SUPERCEDE, staticBuf);
//     }

//     lua_pop(g_L, 1);
//     RETURN_META_VALUE(MRES_IGNORED, NULL);
// }

// /* pfnCVarSetFloat() */
// void CVarSetFloat_Post(const char *szVarName, float flValue)
// {
//     if (!g_L)
//         RETURN_META(MRES_IGNORED);

//     lua_getglobal(g_L, "MetaCVarSetFloat_Post");

//     if (!lua_isfunction(g_L, -1))
//     {
//         lua_pop(g_L, 1);
//         RETURN_META(MRES_IGNORED);
//     }

//     lua_pushstring(g_L, szVarName);
//     lua_pushnumber(g_L, flValue);

//     if (lua_pcall(g_L, 2, 0, 0) != 0)
//     {
//         lua_pop(g_L, 1);
//     }

//     RETURN_META(MRES_IGNORED);
// }

// /* pfnCVarSetString() */
// void CVarSetString_Post(const char *szVarName, const char *szValue)
// {
//     if (!g_L)
//         RETURN_META(MRES_IGNORED);

//     lua_getglobal(g_L, "MetaCVarSetString_Post");

//     if (!lua_isfunction(g_L, -1))
//     {
//         lua_pop(g_L, 1);
//         RETURN_META(MRES_IGNORED);
//     }

//     lua_pushstring(g_L, szVarName);
//     lua_pushstring(g_L, szValue);

//     if (lua_pcall(g_L, 2, 0, 0) != 0)
//     {
//         lua_pop(g_L, 1);
//     }

//     RETURN_META(MRES_IGNORED);
// }

// /* pfnAlertMessage() - 变参函数 */
// void AlertMessage_Post(ALERT_TYPE atype, const char *szFmt, ...)
// {
//     if (!g_L)
//         RETURN_META(MRES_IGNORED);

//     lua_getglobal(g_L, "MetaAlertMessage_Post");

//     if (!lua_isfunction(g_L, -1))
//     {
//         lua_pop(g_L, 1);
//         RETURN_META(MRES_IGNORED);
//     }

//     lua_pushinteger(g_L, atype);

//     // 格式化字符串
//     static char buffer[2048];
//     va_list argptr;
//     va_start(argptr, szFmt);
//     vsnprintf(buffer, sizeof(buffer), szFmt, argptr);
//     va_end(argptr);

//     lua_pushstring(g_L, buffer);

//     if (lua_pcall(g_L, 2, 0, 0) != 0)
//     {
//         lua_pop(g_L, 1);
//     }

//     RETURN_META(MRES_IGNORED);
// }

// /* pfnEngineFprintf() - 变参函数 */
// void EngineFprintf_Post(void *pfile, const char *szFmt, ...)
// {
//     if (!g_L)
//         RETURN_META(MRES_IGNORED);

//     lua_getglobal(g_L, "MetaEngineFprintf_Post");

//     if (!lua_isfunction(g_L, -1))
//     {
//         lua_pop(g_L, 1);
//         RETURN_META(MRES_IGNORED);
//     }

//     lua_pushlightuserdata(g_L, pfile);

//     // 格式化字符串
//     static char buffer[2048];
//     va_list argptr;
//     va_start(argptr, szFmt);
//     vsnprintf(buffer, sizeof(buffer), szFmt, argptr);
//     va_end(argptr);

//     lua_pushstring(g_L, buffer);

//     if (lua_pcall(g_L, 2, 0, 0) != 0)
//     {
//         lua_pop(g_L, 1);
//     }

//     RETURN_META(MRES_IGNORED);
// }

// /* pfnPvAllocEntPrivateData() */
// void* PvAllocEntPrivateData_Post(edict_t *pEdict, int32 cb)
// {
//     if (!g_L)
//         RETURN_META_VALUE(MRES_IGNORED, NULL);

//     lua_getglobal(g_L, "MetaPvAllocEntPrivateData_Post");

//     if (!lua_isfunction(g_L, -1))
//     {
//         lua_pop(g_L, 1);
//         RETURN_META_VALUE(MRES_IGNORED, NULL);
//     }

//     lua_pushentity(g_L, pEdict);
//     lua_pushinteger(g_L, cb);

//     if (lua_pcall(g_L, 2, 1, 0) != 0)
//     {
//         lua_pop(g_L, 1);
//         RETURN_META_VALUE(MRES_IGNORED, NULL);
//     }

//     // 如果 Lua 返回了 userdata 或 lightuserdata，我们可以覆盖
//     if (lua_isuserdata(g_L, -1))
//     {
//         void* ret = lua_touserdata(g_L, -1);
//         lua_pop(g_L, 1);
//         RETURN_META_VALUE(MRES_SUPERCEDE, ret);
//     }

//     lua_pop(g_L, 1);
//     RETURN_META_VALUE(MRES_IGNORED, NULL);
// }

// /* pfnPvEntPrivateData() */
// void* PvEntPrivateData_Post(edict_t *pEdict)
// {
//     if (!g_L)
//         RETURN_META_VALUE(MRES_IGNORED, NULL);

//     lua_getglobal(g_L, "MetaPvEntPrivateData_Post");

//     if (!lua_isfunction(g_L, -1))
//     {
//         lua_pop(g_L, 1);
//         RETURN_META_VALUE(MRES_IGNORED, NULL);
//     }

//     lua_pushentity(g_L, pEdict);

//     if (lua_pcall(g_L, 1, 1, 0) != 0)
//     {
//         lua_pop(g_L, 1);
//         RETURN_META_VALUE(MRES_IGNORED, NULL);
//     }

//     if (lua_isuserdata(g_L, -1))
//     {
//         void* ret = lua_touserdata(g_L, -1);
//         lua_pop(g_L, 1);
//         RETURN_META_VALUE(MRES_SUPERCEDE, ret);
//     }

//     lua_pop(g_L, 1);
//     RETURN_META_VALUE(MRES_IGNORED, NULL);
// }

// /* pfnFreeEntPrivateData() */
// void FreeEntPrivateData_Post(edict_t *pEdict)
// {
//     if (!g_L)
//         RETURN_META(MRES_IGNORED);

//     lua_getglobal(g_L, "MetaFreeEntPrivateData_Post");

//     if (!lua_isfunction(g_L, -1))
//     {
//         lua_pop(g_L, 1);
//         RETURN_META(MRES_IGNORED);
//     }

//     lua_pushentity(g_L, pEdict);

//     if (lua_pcall(g_L, 1, 0, 0) != 0)
//     {
//         lua_pop(g_L, 1);
//     }

//     RETURN_META(MRES_IGNORED);
// }

// /* pfnSzFromIndex() */
// const char* SzFromIndex_Post(int iString)
// {
//     if (!g_L)
//         RETURN_META_VALUE(MRES_IGNORED, NULL);

//     lua_getglobal(g_L, "MetaSzFromIndex_Post");

//     if (!lua_isfunction(g_L, -1))
//     {
//         lua_pop(g_L, 1);
//         RETURN_META_VALUE(MRES_IGNORED, NULL);
//     }

//     lua_pushinteger(g_L, iString);

//     if (lua_pcall(g_L, 1, 1, 0) != 0)
//     {
//         lua_pop(g_L, 1);
//         RETURN_META_VALUE(MRES_IGNORED, NULL);
//     }

//     if (lua_isstring(g_L, -1))
//     {
//         static char staticBuf[256];
//         const char *ret = lua_tostring(g_L, -1);
        
//         strncpy(staticBuf, ret, 255);
//         staticBuf[255] = '\0';

//         lua_pop(g_L, 1);
//         RETURN_META_VALUE(MRES_SUPERCEDE, staticBuf);
//     }

//     lua_pop(g_L, 1);
//     RETURN_META_VALUE(MRES_IGNORED, NULL);
// }

// /* pfnAllocString() */
// int AllocString_Post(const char *szValue)
// {
//     if (!g_L)
//         RETURN_META_VALUE(MRES_IGNORED, 0);

//     lua_getglobal(g_L, "MetaAllocString_Post");

//     if (!lua_isfunction(g_L, -1))
//     {
//         lua_pop(g_L, 1);
//         RETURN_META_VALUE(MRES_IGNORED, 0);
//     }

//     lua_pushstring(g_L, szValue);

//     if (lua_pcall(g_L, 1, 1, 0) != 0)
//     {
//         lua_pop(g_L, 1);
//         RETURN_META_VALUE(MRES_IGNORED, 0);
//     }

//     if (lua_isnumber(g_L, -1))
//     {
//         int ret = (int)lua_tointeger(g_L, -1);
//         lua_pop(g_L, 1);
//         RETURN_META_VALUE(MRES_SUPERCEDE, ret);
//     }

//     lua_pop(g_L, 1);
//     RETURN_META_VALUE(MRES_IGNORED, 0);
// }
// /* pfnGetVarsOfEnt() */
// struct entvars_s* GetVarsOfEnt_Post(edict_t *pEdict)
// {
//     if (!g_L)
//         RETURN_META_VALUE(MRES_IGNORED, NULL);

//     lua_getglobal(g_L, "MetaGetVarsOfEnt_Post");

//     if (!lua_isfunction(g_L, -1))
//     {
//         lua_pop(g_L, 1);
//         RETURN_META_VALUE(MRES_IGNORED, NULL);
//     }

//     lua_pushentity(g_L, pEdict);

//     if (lua_pcall(g_L, 1, 1, 0) != 0)
//     {
//         lua_pop(g_L, 1);
//         RETURN_META_VALUE(MRES_IGNORED, NULL);
//     }

//     // 如果 Lua 返回了 userdata (指针)，覆盖返回值
//     if (lua_isuserdata(g_L, -1))
//     {
//         struct entvars_s* ret = (struct entvars_s*)lua_touserdata(g_L, -1);
//         lua_pop(g_L, 1);
//         RETURN_META_VALUE(MRES_SUPERCEDE, ret);
//     }

//     lua_pop(g_L, 1);
//     RETURN_META_VALUE(MRES_IGNORED, NULL);
// }

// /* pfnPEntityOfEntOffset() */
// edict_t* PEntityOfEntOffset_Post(int iEntOffset)
// {
//     if (!g_L)
//         RETURN_META_VALUE(MRES_IGNORED, NULL);

//     lua_getglobal(g_L, "MetaPEntityOfEntOffset_Post");

//     if (!lua_isfunction(g_L, -1))
//     {
//         lua_pop(g_L, 1);
//         RETURN_META_VALUE(MRES_IGNORED, NULL);
//     }

//     lua_pushinteger(g_L, iEntOffset);

//     if (lua_pcall(g_L, 1, 1, 0) != 0)
//     {
//         lua_pop(g_L, 1);
//         RETURN_META_VALUE(MRES_IGNORED, NULL);
//     }

//     if (lua_isnumber(g_L, -1))
//     {
//         int entIndex = (int)lua_tointeger(g_L, -1);
//         lua_pop(g_L, 1);

//         edict_t* retEnt = NULL;
//         if (entIndex > 0)
//             retEnt = g_engfuncs.pfnPEntityOfEntIndex(entIndex);

//         RETURN_META_VALUE(MRES_SUPERCEDE, retEnt);
//     }

//     lua_pop(g_L, 1);
//     RETURN_META_VALUE(MRES_IGNORED, NULL);
// }

// /* pfnEntOffsetOfPEntity() */
// int EntOffsetOfPEntity_Post(const edict_t *pEdict)
// {
//     if (!g_L)
//         RETURN_META_VALUE(MRES_IGNORED, 0);

//     lua_getglobal(g_L, "MetaEntOffsetOfPEntity_Post");

//     if (!lua_isfunction(g_L, -1))
//     {
//         lua_pop(g_L, 1);
//         RETURN_META_VALUE(MRES_IGNORED, 0);
//     }

//     lua_pushentity(g_L, (edict_t*)pEdict);

//     if (lua_pcall(g_L, 1, 1, 0) != 0)
//     {
//         lua_pop(g_L, 1);
//         RETURN_META_VALUE(MRES_IGNORED, 0);
//     }

//     if (lua_isnumber(g_L, -1))
//     {
//         int ret = (int)lua_tointeger(g_L, -1);
//         lua_pop(g_L, 1);
//         RETURN_META_VALUE(MRES_SUPERCEDE, ret);
//     }

//     lua_pop(g_L, 1);
//     RETURN_META_VALUE(MRES_IGNORED, 0);
// }

// /* pfnIndexOfEdict() */
// int IndexOfEdict_Post(const edict_t *pEdict)
// {
//     if (!g_L)
//         RETURN_META_VALUE(MRES_IGNORED, 0);

//     lua_getglobal(g_L, "MetaIndexOfEdict_Post");

//     if (!lua_isfunction(g_L, -1))
//     {
//         lua_pop(g_L, 1);
//         RETURN_META_VALUE(MRES_IGNORED, 0);
//     }

//     lua_pushentity(g_L, (edict_t*)pEdict);

//     if (lua_pcall(g_L, 1, 1, 0) != 0)
//     {
//         lua_pop(g_L, 1);
//         RETURN_META_VALUE(MRES_IGNORED, 0);
//     }

//     if (lua_isnumber(g_L, -1))
//     {
//         int ret = (int)lua_tointeger(g_L, -1);
//         lua_pop(g_L, 1);
//         RETURN_META_VALUE(MRES_SUPERCEDE, ret);
//     }

//     lua_pop(g_L, 1);
//     RETURN_META_VALUE(MRES_IGNORED, 0);
// }

// /* pfnPEntityOfEntIndex() */
// edict_t* PEntityOfEntIndex_Post(int iEntIndex)
// {
//     if (!g_L)
//         RETURN_META_VALUE(MRES_IGNORED, NULL);

//     lua_getglobal(g_L, "MetaPEntityOfEntIndex_Post");

//     if (!lua_isfunction(g_L, -1))
//     {
//         lua_pop(g_L, 1);
//         RETURN_META_VALUE(MRES_IGNORED, NULL);
//     }

//     lua_pushinteger(g_L, iEntIndex);

//     if (lua_pcall(g_L, 1, 1, 0) != 0)
//     {
//         lua_pop(g_L, 1);
//         RETURN_META_VALUE(MRES_IGNORED, NULL);
//     }

//     if (lua_isnumber(g_L, -1))
//     {
//         int entIndex = (int)lua_tointeger(g_L, -1);
//         lua_pop(g_L, 1);

//         edict_t* retEnt = NULL;
//         if (entIndex > 0)
//             retEnt = g_engfuncs.pfnPEntityOfEntIndex(entIndex);

//         RETURN_META_VALUE(MRES_SUPERCEDE, retEnt);
//     }

//     lua_pop(g_L, 1);
//     RETURN_META_VALUE(MRES_IGNORED, NULL);
// }

// /* pfnFindEntityByVars() */
// edict_t* FindEntityByVars_Post(struct entvars_s* pvars)
// {
//     if (!g_L)
//         RETURN_META_VALUE(MRES_IGNORED, NULL);

//     lua_getglobal(g_L, "MetaFindEntityByVars_Post");

//     if (!lua_isfunction(g_L, -1))
//     {
//         lua_pop(g_L, 1);
//         RETURN_META_VALUE(MRES_IGNORED, NULL);
//     }

//     lua_pushlightuserdata(g_L, pvars);

//     if (lua_pcall(g_L, 1, 1, 0) != 0)
//     {
//         lua_pop(g_L, 1);
//         RETURN_META_VALUE(MRES_IGNORED, NULL);
//     }

//     // Lua 返回 Index -> edict_t*
//     if (lua_isnumber(g_L, -1))
//     {
//         int entIndex = (int)lua_tointeger(g_L, -1);
//         lua_pop(g_L, 1);

//         edict_t* retEnt = NULL;
//         if (entIndex > 0)
//             retEnt = g_engfuncs.pfnPEntityOfEntIndex(entIndex);

//         RETURN_META_VALUE(MRES_SUPERCEDE, retEnt);
//     }

//     lua_pop(g_L, 1);
//     RETURN_META_VALUE(MRES_IGNORED, NULL);
// }

// /* pfnGetModelPtr() */
// void* GetModelPtr_Post(edict_t* pEdict)
// {
//     if (!g_L)
//         RETURN_META_VALUE(MRES_IGNORED, NULL);

//     lua_getglobal(g_L, "MetaGetModelPtr_Post");

//     if (!lua_isfunction(g_L, -1))
//     {
//         lua_pop(g_L, 1);
//         RETURN_META_VALUE(MRES_IGNORED, NULL);
//     }

//     lua_pushentity(g_L, pEdict);

//     if (lua_pcall(g_L, 1, 1, 0) != 0)
//     {
//         lua_pop(g_L, 1);
//         RETURN_META_VALUE(MRES_IGNORED, NULL);
//     }

//     // Lua 返回指针
//     if (lua_isuserdata(g_L, -1))
//     {
//         void* ret = lua_touserdata(g_L, -1);
//         lua_pop(g_L, 1);
//         RETURN_META_VALUE(MRES_SUPERCEDE, ret);
//     }

//     lua_pop(g_L, 1);
//     RETURN_META_VALUE(MRES_IGNORED, NULL);
// }

// /* pfnRegUserMsg() */
// int RegUserMsg_Post(const char *pszName, int iSize)
// {
//     if (!g_L)
//         RETURN_META_VALUE(MRES_IGNORED, 0);

//     lua_getglobal(g_L, "MetaRegUserMsg_Post");

//     if (!lua_isfunction(g_L, -1))
//     {
//         lua_pop(g_L, 1);
//         RETURN_META_VALUE(MRES_IGNORED, 0);
//     }

//     lua_pushstring(g_L, pszName);
//     lua_pushinteger(g_L, iSize);

//     if (lua_pcall(g_L, 2, 1, 0) != 0)
//     {
//         lua_pop(g_L, 1);
//         RETURN_META_VALUE(MRES_IGNORED, 0);
//     }

//     if (lua_isnumber(g_L, -1))
//     {
//         int ret = (int)lua_tointeger(g_L, -1);
//         lua_pop(g_L, 1);
//         RETURN_META_VALUE(MRES_SUPERCEDE, ret);
//     }

//     lua_pop(g_L, 1);
//     RETURN_META_VALUE(MRES_IGNORED, 0);
// }

// /* pfnAnimationAutomove() */
// void AnimationAutomove_Post(const edict_t* pEdict, float flTime)
// {
//     if (!g_L)
//         RETURN_META(MRES_IGNORED);

//     lua_getglobal(g_L, "MetaAnimationAutomove_Post");

//     if (!lua_isfunction(g_L, -1))
//     {
//         lua_pop(g_L, 1);
//         RETURN_META(MRES_IGNORED);
//     }

//     lua_pushentity(g_L, (edict_t*)pEdict);
//     lua_pushnumber(g_L, flTime);

//     if (lua_pcall(g_L, 2, 0, 0) != 0)
//     {
//         lua_pop(g_L, 1);
//     }

//     RETURN_META(MRES_IGNORED);
// }

// /* pfnGetBonePosition() */
// void GetBonePosition_Post(const edict_t* pEdict, int iBone, float *rgflOrigin, float *rgflAngles)
// {
//     if (!g_L)
//         RETURN_META(MRES_IGNORED);

//     lua_getglobal(g_L, "MetaGetBonePosition_Post");

//     if (!lua_isfunction(g_L, -1))
//     {
//         lua_pop(g_L, 1);
//         RETURN_META(MRES_IGNORED);
//     }

//     lua_pushentity(g_L, (edict_t*)pEdict);
//     lua_pushinteger(g_L, iBone);
//     lua_pushlightuserdata(g_L, (void*)rgflOrigin); // Vector Output
//     lua_pushlightuserdata(g_L, (void*)rgflAngles); // Vector Output

//     if (lua_pcall(g_L, 4, 0, 0) != 0)
//     {
//         lua_pop(g_L, 1);
//     }

//     RETURN_META(MRES_IGNORED);
// }
// /* pfnFunctionFromName() */
// uint32 FunctionFromName_Post(const char *pName)
// {
//     if (!g_L)
//         RETURN_META_VALUE(MRES_IGNORED, 0);

//     lua_getglobal(g_L, "MetaFunctionFromName_Post");

//     if (!lua_isfunction(g_L, -1))
//     {
//         lua_pop(g_L, 1);
//         RETURN_META_VALUE(MRES_IGNORED, 0);
//     }

//     lua_pushstring(g_L, pName);

//     if (lua_pcall(g_L, 1, 1, 0) != 0)
//     {
//         lua_pop(g_L, 1);
//         RETURN_META_VALUE(MRES_IGNORED, 0);
//     }

//     if (lua_isnumber(g_L, -1))
//     {
//         uint32 ret = (uint32)lua_tointeger(g_L, -1);
//         lua_pop(g_L, 1);
//         RETURN_META_VALUE(MRES_SUPERCEDE, ret);
//     }

//     lua_pop(g_L, 1);
//     RETURN_META_VALUE(MRES_IGNORED, 0);
// }

// /* pfnNameForFunction() */
// const char *NameForFunction_Post(uint32 function)
// {
//     if (!g_L)
//         RETURN_META_VALUE(MRES_IGNORED, NULL);

//     lua_getglobal(g_L, "MetaNameForFunction_Post");

//     if (!lua_isfunction(g_L, -1))
//     {
//         lua_pop(g_L, 1);
//         RETURN_META_VALUE(MRES_IGNORED, NULL);
//     }

//     lua_pushinteger(g_L, (lua_Integer)function);

//     if (lua_pcall(g_L, 1, 1, 0) != 0)
//     {
//         lua_pop(g_L, 1);
//         RETURN_META_VALUE(MRES_IGNORED, NULL);
//     }

//     if (lua_isstring(g_L, -1))
//     {
//         static char staticName[256];
//         const char *ret = lua_tostring(g_L, -1);
        
//         strncpy(staticName, ret, 255);
//         staticName[255] = '\0';

//         lua_pop(g_L, 1);
//         RETURN_META_VALUE(MRES_SUPERCEDE, staticName);
//     }

//     lua_pop(g_L, 1);
//     RETURN_META_VALUE(MRES_IGNORED, NULL);
// }

// /* pfnClientPrintf() */
// void ClientPrintf_Post(edict_t* pEdict, PRINT_TYPE ptype, const char *szMsg)
// {
//     if (!g_L)
//         RETURN_META(MRES_IGNORED);

//     lua_getglobal(g_L, "MetaClientPrintf_Post");

//     if (!lua_isfunction(g_L, -1))
//     {
//         lua_pop(g_L, 1);
//         RETURN_META(MRES_IGNORED);
//     }

//     lua_pushentity(g_L, pEdict);
//     lua_pushinteger(g_L, (int)ptype);
//     lua_pushstring(g_L, szMsg);

//     if (lua_pcall(g_L, 3, 0, 0) != 0)
//     {
//         lua_pop(g_L, 1);
//     }

//     RETURN_META(MRES_IGNORED);
// }

// /* pfnServerPrint() */
// void ServerPrint_Post(const char *szMsg)
// {
//     if (!g_L)
//         RETURN_META(MRES_IGNORED);

//     lua_getglobal(g_L, "MetaServerPrint_Post");

//     if (!lua_isfunction(g_L, -1))
//     {
//         lua_pop(g_L, 1);
//         RETURN_META(MRES_IGNORED);
//     }

//     lua_pushstring(g_L, szMsg);

//     if (lua_pcall(g_L, 1, 0, 0) != 0)
//     {
//         lua_pop(g_L, 1);
//     }

//     RETURN_META(MRES_IGNORED);
// }

// /* pfnCmd_Args() */
// const char *Cmd_Args_Post(void)
// {
//     if (!g_L)
//         RETURN_META_VALUE(MRES_IGNORED, NULL);

//     lua_getglobal(g_L, "MetaCmd_Args_Post");

//     if (!lua_isfunction(g_L, -1))
//     {
//         lua_pop(g_L, 1);
//         RETURN_META_VALUE(MRES_IGNORED, NULL);
//     }

//     if (lua_pcall(g_L, 0, 1, 0) != 0)
//     {
//         lua_pop(g_L, 1);
//         RETURN_META_VALUE(MRES_IGNORED, NULL);
//     }

//     if (lua_isstring(g_L, -1))
//     {
//         static char staticArgs[1024];
//         const char *ret = lua_tostring(g_L, -1);
        
//         strncpy(staticArgs, ret, 1023);
//         staticArgs[1023] = '\0';

//         lua_pop(g_L, 1);
//         RETURN_META_VALUE(MRES_SUPERCEDE, staticArgs);
//     }

//     lua_pop(g_L, 1);
//     RETURN_META_VALUE(MRES_IGNORED, NULL);
// }

// /* pfnCmd_Argv() */
// const char *Cmd_Argv_Post(int argc)
// {
//     if (!g_L)
//         RETURN_META_VALUE(MRES_IGNORED, NULL);

//     lua_getglobal(g_L, "MetaCmd_Argv_Post");

//     if (!lua_isfunction(g_L, -1))
//     {
//         lua_pop(g_L, 1);
//         RETURN_META_VALUE(MRES_IGNORED, NULL);
//     }

//     lua_pushinteger(g_L, argc);

//     if (lua_pcall(g_L, 1, 1, 0) != 0)
//     {
//         lua_pop(g_L, 1);
//         RETURN_META_VALUE(MRES_IGNORED, NULL);
//     }

//     if (lua_isstring(g_L, -1))
//     {
//         static char staticArgv[256];
//         const char *ret = lua_tostring(g_L, -1);
        
//         strncpy(staticArgv, ret, 255);
//         staticArgv[255] = '\0';

//         lua_pop(g_L, 1);
//         RETURN_META_VALUE(MRES_SUPERCEDE, staticArgv);
//     }

//     lua_pop(g_L, 1);
//     RETURN_META_VALUE(MRES_IGNORED, NULL);
// }

// /* pfnCmd_Argc() */
// int Cmd_Argc_Post(void)
// {
//     if (!g_L)
//         RETURN_META_VALUE(MRES_IGNORED, 0);

//     lua_getglobal(g_L, "MetaCmd_Argc_Post");

//     if (!lua_isfunction(g_L, -1))
//     {
//         lua_pop(g_L, 1);
//         RETURN_META_VALUE(MRES_IGNORED, 0);
//     }

//     if (lua_pcall(g_L, 0, 1, 0) != 0)
//     {
//         lua_pop(g_L, 1);
//         RETURN_META_VALUE(MRES_IGNORED, 0);
//     }

//     if (lua_isnumber(g_L, -1))
//     {
//         int ret = (int)lua_tointeger(g_L, -1);
//         lua_pop(g_L, 1);
//         RETURN_META_VALUE(MRES_SUPERCEDE, ret);
//     }

//     lua_pop(g_L, 1);
//     RETURN_META_VALUE(MRES_IGNORED, 0);
// }

// /* pfnGetAttachment() */
// void GetAttachment_Post(const edict_t *pEdict, int iAttachment, float *rgflOrigin, float *rgflAngles)
// {
//     if (!g_L)
//         RETURN_META(MRES_IGNORED);

//     lua_getglobal(g_L, "MetaGetAttachment_Post");

//     if (!lua_isfunction(g_L, -1))
//     {
//         lua_pop(g_L, 1);
//         RETURN_META(MRES_IGNORED);
//     }

//     lua_pushentity(g_L, (edict_t*)pEdict);
//     lua_pushinteger(g_L, iAttachment);
//     lua_pushlightuserdata(g_L, (void*)rgflOrigin); // Vector Output
//     lua_pushlightuserdata(g_L, (void*)rgflAngles); // Vector Output

//     if (lua_pcall(g_L, 4, 0, 0) != 0)
//     {
//         lua_pop(g_L, 1);
//     }

//     RETURN_META(MRES_IGNORED);
// }

// /* pfnCRC32_Init() */
// void CRC32_Init_Post(CRC32_t *pulCRC)
// {
//     if (!g_L)
//         RETURN_META(MRES_IGNORED);

//     lua_getglobal(g_L, "MetaCRC32_Init_Post");

//     if (!lua_isfunction(g_L, -1))
//     {
//         lua_pop(g_L, 1);
//         RETURN_META(MRES_IGNORED);
//     }

//     lua_pushlightuserdata(g_L, (void*)pulCRC);

//     if (lua_pcall(g_L, 1, 0, 0) != 0)
//     {
//         lua_pop(g_L, 1);
//     }

//     RETURN_META(MRES_IGNORED);
// }

// /* pfnCRC32_ProcessBuffer() */
// void CRC32_ProcessBuffer_Post(CRC32_t *pulCRC, void *p, int len)
// {
//     if (!g_L)
//         RETURN_META(MRES_IGNORED);

//     lua_getglobal(g_L, "MetaCRC32_ProcessBuffer_Post");

//     if (!lua_isfunction(g_L, -1))
//     {
//         lua_pop(g_L, 1);
//         RETURN_META(MRES_IGNORED);
//     }

//     lua_pushlightuserdata(g_L, (void*)pulCRC);
//     lua_pushlightuserdata(g_L, p);
//     lua_pushinteger(g_L, len);

//     if (lua_pcall(g_L, 3, 0, 0) != 0)
//     {
//         lua_pop(g_L, 1);
//     }

//     RETURN_META(MRES_IGNORED);
// }
// /* pfnCRC32_ProcessByte() */
// void CRC32_ProcessByte_Post(CRC32_t *pulCRC, unsigned char ch)
// {
//     if (!g_L)
//         RETURN_META(MRES_IGNORED);

//     lua_getglobal(g_L, "MetaCRC32_ProcessByte_Post");

//     if (!lua_isfunction(g_L, -1))
//     {
//         lua_pop(g_L, 1);
//         RETURN_META(MRES_IGNORED);
//     }

//     lua_pushlightuserdata(g_L, (void*)pulCRC);
//     lua_pushinteger(g_L, (int)ch);

//     if (lua_pcall(g_L, 2, 0, 0) != 0)
//     {
//         lua_pop(g_L, 1);
//     }

//     RETURN_META(MRES_IGNORED);
// }

// /* pfnCRC32_Final() */
// CRC32_t CRC32_Final_Post(CRC32_t pulCRC)
// {
//     if (!g_L)
//         RETURN_META_VALUE(MRES_IGNORED, pulCRC);

//     lua_getglobal(g_L, "MetaCRC32_Final_Post");

//     if (!lua_isfunction(g_L, -1))
//     {
//         lua_pop(g_L, 1);
//         RETURN_META_VALUE(MRES_IGNORED, pulCRC);
//     }

//     // CRC32_t 通常是 unsigned long (int)
//     lua_pushinteger(g_L, (lua_Integer)pulCRC);

//     if (lua_pcall(g_L, 1, 1, 0) != 0)
//     {
//         lua_pop(g_L, 1);
//         RETURN_META_VALUE(MRES_IGNORED, pulCRC);
//     }

//     if (lua_isnumber(g_L, -1))
//     {
//         CRC32_t ret = (CRC32_t)lua_tointeger(g_L, -1);
//         lua_pop(g_L, 1);
//         RETURN_META_VALUE(MRES_SUPERCEDE, ret);
//     }

//     lua_pop(g_L, 1);
//     RETURN_META_VALUE(MRES_IGNORED, pulCRC);
// }

// /* pfnRandomLong() */
// int32 RandomLong_Post(int32 lLow, int32 lHigh)
// {
//     if (!g_L)
//         RETURN_META_VALUE(MRES_IGNORED, 0);

//     lua_getglobal(g_L, "MetaRandomLong_Post");

//     if (!lua_isfunction(g_L, -1))
//     {
//         lua_pop(g_L, 1);
//         RETURN_META_VALUE(MRES_IGNORED, 0);
//     }

//     lua_pushinteger(g_L, lLow);
//     lua_pushinteger(g_L, lHigh);

//     if (lua_pcall(g_L, 2, 1, 0) != 0)
//     {
//         lua_pop(g_L, 1);
//         RETURN_META_VALUE(MRES_IGNORED, 0);
//     }

//     if (lua_isnumber(g_L, -1))
//     {
//         int32 ret = (int32)lua_tointeger(g_L, -1);
//         lua_pop(g_L, 1);
//         RETURN_META_VALUE(MRES_SUPERCEDE, ret);
//     }

//     lua_pop(g_L, 1);
//     RETURN_META_VALUE(MRES_IGNORED, 0);
// }

// /* pfnRandomFloat() */
// float RandomFloat_Post(float flLow, float flHigh)
// {
//     if (!g_L)
//         RETURN_META_VALUE(MRES_IGNORED, 0.0f);

//     lua_getglobal(g_L, "MetaRandomFloat_Post");

//     if (!lua_isfunction(g_L, -1))
//     {
//         lua_pop(g_L, 1);
//         RETURN_META_VALUE(MRES_IGNORED, 0.0f);
//     }

//     lua_pushnumber(g_L, flLow);
//     lua_pushnumber(g_L, flHigh);

//     if (lua_pcall(g_L, 2, 1, 0) != 0)
//     {
//         lua_pop(g_L, 1);
//         RETURN_META_VALUE(MRES_IGNORED, 0.0f);
//     }

//     if (lua_isnumber(g_L, -1))
//     {
//         float ret = (float)lua_tonumber(g_L, -1);
//         lua_pop(g_L, 1);
//         RETURN_META_VALUE(MRES_SUPERCEDE, ret);
//     }

//     lua_pop(g_L, 1);
//     RETURN_META_VALUE(MRES_IGNORED, 0.0f);
// }

// /* pfnSetView() */
// void SetView_Post(const edict_t *pClient, const edict_t *pViewent)
// {
//     if (!g_L)
//         RETURN_META(MRES_IGNORED);

//     lua_getglobal(g_L, "MetaSetView_Post");

//     if (!lua_isfunction(g_L, -1))
//     {
//         lua_pop(g_L, 1);
//         RETURN_META(MRES_IGNORED);
//     }

//     lua_pushentity(g_L, (edict_t*)pClient);
//     lua_pushentity(g_L, (edict_t*)pViewent);

//     if (lua_pcall(g_L, 2, 0, 0) != 0)
//     {
//         lua_pop(g_L, 1);
//     }

//     RETURN_META(MRES_IGNORED);
// }

// /* pfnTime() */
// float Time_Post(void)
// {
//     if (!g_L)
//         RETURN_META_VALUE(MRES_IGNORED, 0.0f);

//     lua_getglobal(g_L, "MetaTime_Post");

//     if (!lua_isfunction(g_L, -1))
//     {
//         lua_pop(g_L, 1);
//         RETURN_META_VALUE(MRES_IGNORED, 0.0f);
//     }

//     if (lua_pcall(g_L, 0, 1, 0) != 0)
//     {
//         lua_pop(g_L, 1);
//         RETURN_META_VALUE(MRES_IGNORED, 0.0f);
//     }

//     if (lua_isnumber(g_L, -1))
//     {
//         float ret = (float)lua_tonumber(g_L, -1);
//         lua_pop(g_L, 1);
//         RETURN_META_VALUE(MRES_SUPERCEDE, ret);
//     }

//     lua_pop(g_L, 1);
//     RETURN_META_VALUE(MRES_IGNORED, 0.0f);
// }

// /* pfnCrosshairAngle() */
// void CrosshairAngle_Post(const edict_t *pClient, float pitch, float yaw)
// {
//     if (!g_L)
//         RETURN_META(MRES_IGNORED);

//     lua_getglobal(g_L, "MetaCrosshairAngle_Post");

//     if (!lua_isfunction(g_L, -1))
//     {
//         lua_pop(g_L, 1);
//         RETURN_META(MRES_IGNORED);
//     }

//     lua_pushentity(g_L, (edict_t*)pClient);
//     lua_pushnumber(g_L, pitch);
//     lua_pushnumber(g_L, yaw);

//     if (lua_pcall(g_L, 3, 0, 0) != 0)
//     {
//         lua_pop(g_L, 1);
//     }

//     RETURN_META(MRES_IGNORED);
// }

// /* pfnLoadFileForMe() */
// byte* LoadFileForMe_Post(const char *filename, int *pLength)
// {
//     if (!g_L)
//         RETURN_META_VALUE(MRES_IGNORED, NULL);

//     lua_getglobal(g_L, "MetaLoadFileForMe_Post");

//     if (!lua_isfunction(g_L, -1))
//     {
//         lua_pop(g_L, 1);
//         RETURN_META_VALUE(MRES_IGNORED, NULL);
//     }

//     lua_pushstring(g_L, filename);
//     lua_pushlightuserdata(g_L, pLength);

//     if (lua_pcall(g_L, 2, 1, 0) != 0)
//     {
//         lua_pop(g_L, 1);
//         RETURN_META_VALUE(MRES_IGNORED, NULL);
//     }

//     // 如果 Lua 返回一个指针 (userdata/lightuserdata)，则视为覆盖返回的 buffer
//     if (lua_isuserdata(g_L, -1))
//     {
//         byte* ret = (byte*)lua_touserdata(g_L, -1);
//         lua_pop(g_L, 1);
//         RETURN_META_VALUE(MRES_SUPERCEDE, ret);
//     }

//     lua_pop(g_L, 1);
//     RETURN_META_VALUE(MRES_IGNORED, NULL);
// }

// /* pfnFreeFile() */
// void FreeFile_Post(void *buffer)
// {
//     if (!g_L)
//         RETURN_META(MRES_IGNORED);

//     lua_getglobal(g_L, "MetaFreeFile_Post");

//     if (!lua_isfunction(g_L, -1))
//     {
//         lua_pop(g_L, 1);
//         RETURN_META(MRES_IGNORED);
//     }

//     lua_pushlightuserdata(g_L, buffer);

//     if (lua_pcall(g_L, 1, 0, 0) != 0)
//     {
//         lua_pop(g_L, 1);
//     }

//     RETURN_META(MRES_IGNORED);
// }

// /* pfnEndSection() */
// void EndSection_Post(const char *pszSectionName)
// {
//     if (!g_L)
//         RETURN_META(MRES_IGNORED);

//     lua_getglobal(g_L, "MetaEndSection_Post");

//     if (!lua_isfunction(g_L, -1))
//     {
//         lua_pop(g_L, 1);
//         RETURN_META(MRES_IGNORED);
//     }

//     lua_pushstring(g_L, pszSectionName);

//     if (lua_pcall(g_L, 1, 0, 0) != 0)
//     {
//         lua_pop(g_L, 1);
//     }

//     RETURN_META(MRES_IGNORED);
// }
// /* pfnCompareFileTime() */
// int CompareFileTime_Post(char *filename1, char *filename2, int *iCompare)
// {
//     if (!g_L)
//         RETURN_META_VALUE(MRES_IGNORED, 0);

//     lua_getglobal(g_L, "MetaCompareFileTime_Post");

//     if (!lua_isfunction(g_L, -1))
//     {
//         lua_pop(g_L, 1);
//         RETURN_META_VALUE(MRES_IGNORED, 0);
//     }

//     lua_pushstring(g_L, filename1);
//     lua_pushstring(g_L, filename2);
//     lua_pushlightuserdata(g_L, iCompare); // Output pointer

//     if (lua_pcall(g_L, 3, 1, 0) != 0)
//     {
//         lua_pop(g_L, 1);
//         RETURN_META_VALUE(MRES_IGNORED, 0);
//     }

//     if (lua_isnumber(g_L, -1))
//     {
//         int ret = (int)lua_tointeger(g_L, -1);
//         lua_pop(g_L, 1);
//         RETURN_META_VALUE(MRES_SUPERCEDE, ret);
//     }

//     lua_pop(g_L, 1);
//     RETURN_META_VALUE(MRES_IGNORED, 0);
// }

// /* pfnGetGameDir() */
// void GetGameDir_Post(char *szGetGameDir)
// {
//     if (!g_L)
//         RETURN_META(MRES_IGNORED);

//     lua_getglobal(g_L, "MetaGetGameDir_Post");

//     if (!lua_isfunction(g_L, -1))
//     {
//         lua_pop(g_L, 1);
//         RETURN_META(MRES_IGNORED);
//     }

//     // 虽然 szGetGameDir 是输出buffer，但这里还没数据，传指针给 Lua
//     // Lua 如果想修改，应该返回一个字符串
//     lua_pushlightuserdata(g_L, szGetGameDir); 

//     if (lua_pcall(g_L, 1, 1, 0) != 0)
//     {
//         lua_pop(g_L, 1);
//         RETURN_META(MRES_IGNORED);
//     }

//     // 如果 Lua 返回字符串，复制到缓冲区
//     if (lua_isstring(g_L, -1))
//     {
//         const char *ret = lua_tostring(g_L, -1);
//         // 假设 buffer 足够大 (MAX_PATH)，通常是安全的
//         strcpy(szGetGameDir, ret);
        
//         lua_pop(g_L, 1);
//         RETURN_META(MRES_SUPERCEDE);
//     }

//     lua_pop(g_L, 1);
//     RETURN_META(MRES_IGNORED);
// }

// /* pfnCvar_RegisterVariable() */
// void Cvar_RegisterVariable_Post(cvar_t *variable)
// {
//     if (!g_L)
//         RETURN_META(MRES_IGNORED);

//     lua_getglobal(g_L, "MetaCvar_RegisterVariable_Post");

//     if (!lua_isfunction(g_L, -1))
//     {
//         lua_pop(g_L, 1);
//         RETURN_META(MRES_IGNORED);
//     }

//     lua_pushlightuserdata(g_L, (void*)variable);

//     if (lua_pcall(g_L, 1, 0, 0) != 0)
//     {
//         lua_pop(g_L, 1);
//     }

//     RETURN_META(MRES_IGNORED);
// }

// /* pfnFadeClientVolume() */
// void FadeClientVolume_Post(const edict_t *pEdict, int fadePercent, int fadeOutSeconds, int holdTime, int fadeInSeconds)
// {
//     if (!g_L)
//         RETURN_META(MRES_IGNORED);

//     lua_getglobal(g_L, "MetaFadeClientVolume_Post");

//     if (!lua_isfunction(g_L, -1))
//     {
//         lua_pop(g_L, 1);
//         RETURN_META(MRES_IGNORED);
//     }

//     lua_pushentity(g_L, (edict_t*)pEdict);
//     lua_pushinteger(g_L, fadePercent);
//     lua_pushinteger(g_L, fadeOutSeconds);
//     lua_pushinteger(g_L, holdTime);
//     lua_pushinteger(g_L, fadeInSeconds);

//     if (lua_pcall(g_L, 5, 0, 0) != 0)
//     {
//         lua_pop(g_L, 1);
//     }

//     RETURN_META(MRES_IGNORED);
// }

// /* pfnSetClientMaxspeed() */
// void SetClientMaxspeed_Post(const edict_t *pEdict, float fNewMaxspeed)
// {
//     if (!g_L)
//         RETURN_META(MRES_IGNORED);

//     lua_getglobal(g_L, "MetaSetClientMaxspeed_Post");

//     if (!lua_isfunction(g_L, -1))
//     {
//         lua_pop(g_L, 1);
//         RETURN_META(MRES_IGNORED);
//     }

//     lua_pushentity(g_L, pEdict);
//     lua_pushnumber(g_L, fNewMaxspeed);

//     if (lua_pcall(g_L, 2, 0, 0) != 0)
//     {
//         lua_pop(g_L, 1);
//     }

//     RETURN_META(MRES_IGNORED);
// }

// /* pfnCreateFakeClient() */
// edict_t* CreateFakeClient_Post(const char *netname)
// {
//     if (!g_L)
//         RETURN_META_VALUE(MRES_IGNORED, NULL);

//     lua_getglobal(g_L, "MetaCreateFakeClient_Post");

//     if (!lua_isfunction(g_L, -1))
//     {
//         lua_pop(g_L, 1);
//         RETURN_META_VALUE(MRES_IGNORED, NULL);
//     }

//     lua_pushstring(g_L, netname);

//     if (lua_pcall(g_L, 1, 1, 0) != 0)
//     {
//         lua_pop(g_L, 1);
//         RETURN_META_VALUE(MRES_IGNORED, NULL);
//     }

//     // Lua 返回 Entity Index -> edict_t*
//     if (lua_isnumber(g_L, -1))
//     {
//         int entIndex = (int)lua_tointeger(g_L, -1);
//         lua_pop(g_L, 1);

//         edict_t* retEnt = NULL;
//         if (entIndex > 0)
//             retEnt = g_engfuncs.pfnPEntityOfEntIndex(entIndex);

//         RETURN_META_VALUE(MRES_SUPERCEDE, retEnt);
//     }

//     lua_pop(g_L, 1);
//     RETURN_META_VALUE(MRES_IGNORED, NULL);
// }

// /* pfnRunPlayerMove() */
// void RunPlayerMove_Post(edict_t *fakeclient, const float *viewangles, float forwardmove, float sidemove, float upmove, unsigned short buttons, byte impulse, byte msec)
// {
//     if (!g_L)
//         RETURN_META(MRES_IGNORED);

//     lua_getglobal(g_L, "MetaRunPlayerMove_Post");

//     if (!lua_isfunction(g_L, -1))
//     {
//         lua_pop(g_L, 1);
//         RETURN_META(MRES_IGNORED);
//     }

//     lua_pushentity(g_L, fakeclient);
//     lua_pushlightuserdata(g_L, (void*)viewangles); // Vector ptr
//     lua_pushnumber(g_L, forwardmove);
//     lua_pushnumber(g_L, sidemove);
//     lua_pushnumber(g_L, upmove);
//     lua_pushinteger(g_L, buttons);
//     lua_pushinteger(g_L, impulse);
//     lua_pushinteger(g_L, msec);

//     if (lua_pcall(g_L, 8, 0, 0) != 0)
//     {
//         lua_pop(g_L, 1);
//     }

//     RETURN_META(MRES_IGNORED);
// }

// /* pfnNumberOfEntities() */
// int NumberOfEntities_Post(void)
// {
//     if (!g_L)
//         RETURN_META_VALUE(MRES_IGNORED, 0);

//     lua_getglobal(g_L, "MetaNumberOfEntities_Post");

//     if (!lua_isfunction(g_L, -1))
//     {
//         lua_pop(g_L, 1);
//         RETURN_META_VALUE(MRES_IGNORED, 0);
//     }

//     if (lua_pcall(g_L, 0, 1, 0) != 0)
//     {
//         lua_pop(g_L, 1);
//         RETURN_META_VALUE(MRES_IGNORED, 0);
//     }

//     if (lua_isnumber(g_L, -1))
//     {
//         int ret = (int)lua_tointeger(g_L, -1);
//         lua_pop(g_L, 1);
//         RETURN_META_VALUE(MRES_SUPERCEDE, ret);
//     }

//     lua_pop(g_L, 1);
//     RETURN_META_VALUE(MRES_IGNORED, 0);
// }

// /* pfnGetInfoKeyBuffer() */
// char* GetInfoKeyBuffer_Post(edict_t *e)
// {
//     if (!g_L)
//         RETURN_META_VALUE(MRES_IGNORED, NULL);

//     lua_getglobal(g_L, "MetaGetInfoKeyBuffer_Post");

//     if (!lua_isfunction(g_L, -1))
//     {
//         lua_pop(g_L, 1);
//         RETURN_META_VALUE(MRES_IGNORED, NULL);
//     }

//     lua_pushentity(g_L, e);

//     if (lua_pcall(g_L, 1, 1, 0) != 0)
//     {
//         lua_pop(g_L, 1);
//         RETURN_META_VALUE(MRES_IGNORED, NULL);
//     }

//     // 如果 Lua 返回字符串，我们覆盖引擎返回的 buffer 指针
//     if (lua_isstring(g_L, -1))
//     {
//         static char staticBuf[2048]; // Info buffer 通常较大
//         const char *ret = lua_tostring(g_L, -1);
        
//         strncpy(staticBuf, ret, 2047);
//         staticBuf[2047] = '\0';

//         lua_pop(g_L, 1);
//         RETURN_META_VALUE(MRES_SUPERCEDE, staticBuf);
//     }

//     lua_pop(g_L, 1);
//     RETURN_META_VALUE(MRES_IGNORED, NULL);
// }

// /* pfnInfoKeyValue() */
// char* InfoKeyValue_Post(char *infobuffer, const char *key)
// {
//     if (!g_L)
//         RETURN_META_VALUE(MRES_IGNORED, NULL);

//     lua_getglobal(g_L, "MetaInfoKeyValue_Post");

//     if (!lua_isfunction(g_L, -1))
//     {
//         lua_pop(g_L, 1);
//         RETURN_META_VALUE(MRES_IGNORED, NULL);
//     }

//     // infobuffer 是内容字符串，复制一份给 Lua
//     lua_pushstring(g_L, infobuffer);
//     lua_pushstring(g_L, key);

//     if (lua_pcall(g_L, 2, 1, 0) != 0)
//     {
//         lua_pop(g_L, 1);
//         RETURN_META_VALUE(MRES_IGNORED, NULL);
//     }

//     if (lua_isstring(g_L, -1))
//     {
//         static char staticVal[256];
//         const char *ret = lua_tostring(g_L, -1);
        
//         strncpy(staticVal, ret, 255);
//         staticVal[255] = '\0';

//         lua_pop(g_L, 1);
//         RETURN_META_VALUE(MRES_SUPERCEDE, staticVal);
//     }

//     lua_pop(g_L, 1);
//     RETURN_META_VALUE(MRES_IGNORED, NULL);
// }

// /* pfnSetKeyValue() */
// void SetKeyValue_Post(char *infobuffer, const char *key, const char *value)
// {
//     if (!g_L)
//         RETURN_META(MRES_IGNORED);

//     lua_getglobal(g_L, "MetaSetKeyValue_Post");

//     if (!lua_isfunction(g_L, -1))
//     {
//         lua_pop(g_L, 1);
//         RETURN_META(MRES_IGNORED);
//     }

//     lua_pushstring(g_L, infobuffer);
//     lua_pushstring(g_L, key);
//     lua_pushstring(g_L, value);

//     if (lua_pcall(g_L, 3, 0, 0) != 0)
//     {
//         lua_pop(g_L, 1);
//     }

//     RETURN_META(MRES_IGNORED);
// }

// /* pfnSetClientKeyValue() */
// void SetClientKeyValue_Post(int clientIndex, char *infobuffer, const char *key, const char *value)
// {
//     if (!g_L)
//         RETURN_META(MRES_IGNORED);

//     lua_getglobal(g_L, "MetaSetClientKeyValue_Post");

//     if (!lua_isfunction(g_L, -1))
//     {
//         lua_pop(g_L, 1);
//         RETURN_META(MRES_IGNORED);
//     }

//     lua_pushinteger(g_L, clientIndex);
//     lua_pushstring(g_L, infobuffer);
//     lua_pushstring(g_L, key);
//     lua_pushstring(g_L, value);

//     if (lua_pcall(g_L, 4, 0, 0) != 0)
//     {
//         lua_pop(g_L, 1);
//     }

//     RETURN_META(MRES_IGNORED);
// }

// /* pfnIsMapValid() */
// int IsMapValid_Post(const char *filename)
// {
//     if (!g_L)
//         RETURN_META_VALUE(MRES_IGNORED, 0);

//     lua_getglobal(g_L, "MetaIsMapValid_Post");

//     if (!lua_isfunction(g_L, -1))
//     {
//         lua_pop(g_L, 1);
//         RETURN_META_VALUE(MRES_IGNORED, 0);
//     }

//     lua_pushstring(g_L, filename);

//     if (lua_pcall(g_L, 1, 1, 0) != 0)
//     {
//         lua_pop(g_L, 1);
//         RETURN_META_VALUE(MRES_IGNORED, 0);
//     }

//     if (lua_isnumber(g_L, -1))
//     {
//         int ret = (int)lua_tointeger(g_L, -1);
//         lua_pop(g_L, 1);
//         RETURN_META_VALUE(MRES_SUPERCEDE, ret);
//     }

//     lua_pop(g_L, 1);
//     RETURN_META_VALUE(MRES_IGNORED, 0);
// }

// /* pfnStaticDecal() */
// void StaticDecal_Post(const float *origin, int decalIndex, int entityIndex, int modelIndex)
// {
//     if (!g_L)
//         RETURN_META(MRES_IGNORED);

//     lua_getglobal(g_L, "MetaStaticDecal_Post");

//     if (!lua_isfunction(g_L, -1))
//     {
//         lua_pop(g_L, 1);
//         RETURN_META(MRES_IGNORED);
//     }

//     lua_pushlightuserdata(g_L, (void*)origin);
//     lua_pushinteger(g_L, decalIndex);
//     lua_pushinteger(g_L, entityIndex);
//     lua_pushinteger(g_L, modelIndex);

//     if (lua_pcall(g_L, 4, 0, 0) != 0)
//     {
//         lua_pop(g_L, 1);
//     }

//     RETURN_META(MRES_IGNORED);
// }

// /* pfnPrecacheGeneric() */
// int PrecacheGeneric_Post(const char* s)
// {
//     if (!g_L)
//         RETURN_META_VALUE(MRES_IGNORED, 0);

//     lua_getglobal(g_L, "MetaPrecacheGeneric_Post");

//     if (!lua_isfunction(g_L, -1))
//     {
//         lua_pop(g_L, 1);
//         RETURN_META_VALUE(MRES_IGNORED, 0);
//     }

//     lua_pushstring(g_L, s);

//     if (lua_pcall(g_L, 1, 1, 0) != 0)
//     {
//         lua_pop(g_L, 1);
//         RETURN_META_VALUE(MRES_IGNORED, 0);
//     }

//     if (lua_isnumber(g_L, -1))
//     {
//         int ret = (int)lua_tointeger(g_L, -1);
//         lua_pop(g_L, 1);
//         RETURN_META_VALUE(MRES_SUPERCEDE, ret);
//     }

//     lua_pop(g_L, 1);
//     RETURN_META_VALUE(MRES_IGNORED, 0);
// }

// /* pfnGetPlayerUserId() */
// int GetPlayerUserId_Post(edict_t *e)
// {
//     if (!g_L)
//         RETURN_META_VALUE(MRES_IGNORED, 0);

//     lua_getglobal(g_L, "MetaGetPlayerUserId_Post");

//     if (!lua_isfunction(g_L, -1))
//     {
//         lua_pop(g_L, 1);
//         RETURN_META_VALUE(MRES_IGNORED, 0);
//     }

//     lua_pushentity(g_L, e);

//     if (lua_pcall(g_L, 1, 1, 0) != 0)
//     {
//         lua_pop(g_L, 1);
//         RETURN_META_VALUE(MRES_IGNORED, 0);
//     }

//     if (lua_isnumber(g_L, -1))
//     {
//         int ret = (int)lua_tointeger(g_L, -1);
//         lua_pop(g_L, 1);
//         RETURN_META_VALUE(MRES_SUPERCEDE, ret);
//     }

//     lua_pop(g_L, 1);
//     RETURN_META_VALUE(MRES_IGNORED, 0);
// }

// /* pfnBuildSoundMsg() */
// void BuildSoundMsg_Post(edict_t *entity, int channel, const char *sample, float volume, float attenuation, int fFlags, int pitch, int msg_dest, int msg_type, const float *pOrigin, edict_t *ed)
// {
//     if (!g_L)
//         RETURN_META(MRES_IGNORED);

//     lua_getglobal(g_L, "MetaBuildSoundMsg_Post");

//     if (!lua_isfunction(g_L, -1))
//     {
//         lua_pop(g_L, 1);
//         RETURN_META(MRES_IGNORED);
//     }

//     lua_pushentity(g_L, entity);
//     lua_pushinteger(g_L, channel);
//     lua_pushstring(g_L, sample);
//     lua_pushnumber(g_L, volume);
//     lua_pushnumber(g_L, attenuation);
//     lua_pushinteger(g_L, fFlags);
//     lua_pushinteger(g_L, pitch);
//     lua_pushinteger(g_L, msg_dest);
//     lua_pushinteger(g_L, msg_type);
//     lua_pushlightuserdata(g_L, (void*)pOrigin);
//     lua_pushentity(g_L, ed);

//     if (lua_pcall(g_L, 11, 0, 0) != 0)
//     {
//         lua_pop(g_L, 1);
//     }

//     RETURN_META(MRES_IGNORED);
// }

// /* pfnIsDedicatedServer() */
// int IsDedicatedServer_Post(void)
// {
//     if (!g_L)
//         RETURN_META_VALUE(MRES_IGNORED, 0);

//     lua_getglobal(g_L, "MetaIsDedicatedServer_Post");

//     if (!lua_isfunction(g_L, -1))
//     {
//         lua_pop(g_L, 1);
//         RETURN_META_VALUE(MRES_IGNORED, 0);
//     }

//     if (lua_pcall(g_L, 0, 1, 0) != 0)
//     {
//         lua_pop(g_L, 1);
//         RETURN_META_VALUE(MRES_IGNORED, 0);
//     }

//     if (lua_isnumber(g_L, -1))
//     {
//         int ret = (int)lua_tointeger(g_L, -1);
//         lua_pop(g_L, 1);
//         RETURN_META_VALUE(MRES_SUPERCEDE, ret);
//     }

//     lua_pop(g_L, 1);
//     RETURN_META_VALUE(MRES_IGNORED, 0);
// }

// /* pfnCVarGetPointer() */
// cvar_t* CVarGetPointer_Post(const char *szVarName)
// {
//     if (!g_L)
//         RETURN_META_VALUE(MRES_IGNORED, NULL);

//     lua_getglobal(g_L, "MetaCVarGetPointer_Post");

//     if (!lua_isfunction(g_L, -1))
//     {
//         lua_pop(g_L, 1);
//         RETURN_META_VALUE(MRES_IGNORED, NULL);
//     }

//     lua_pushstring(g_L, szVarName);

//     if (lua_pcall(g_L, 1, 1, 0) != 0)
//     {
//         lua_pop(g_L, 1);
//         RETURN_META_VALUE(MRES_IGNORED, NULL);
//     }

//     if (lua_isuserdata(g_L, -1))
//     {
//         cvar_t* ret = (cvar_t*)lua_touserdata(g_L, -1);
//         lua_pop(g_L, 1);
//         RETURN_META_VALUE(MRES_SUPERCEDE, ret);
//     }

//     lua_pop(g_L, 1);
//     RETURN_META_VALUE(MRES_IGNORED, NULL);
// }

// /* pfnGetPlayerWONId() */
// unsigned int GetPlayerWONId_Post(edict_t *e)
// {
//     if (!g_L)
//         RETURN_META_VALUE(MRES_IGNORED, 0);

//     lua_getglobal(g_L, "MetaGetPlayerWONId_Post");

//     if (!lua_isfunction(g_L, -1))
//     {
//         lua_pop(g_L, 1);
//         RETURN_META_VALUE(MRES_IGNORED, 0);
//     }

//     lua_pushentity(g_L, e);

//     if (lua_pcall(g_L, 1, 1, 0) != 0)
//     {
//         lua_pop(g_L, 1);
//         RETURN_META_VALUE(MRES_IGNORED, 0);
//     }

//     if (lua_isnumber(g_L, -1))
//     {
//         unsigned int ret = (unsigned int)lua_tointeger(g_L, -1);
//         lua_pop(g_L, 1);
//         RETURN_META_VALUE(MRES_SUPERCEDE, ret);
//     }

//     lua_pop(g_L, 1);
//     RETURN_META_VALUE(MRES_IGNORED, 0);
// }
// /* pfnInfo_RemoveKey() */
// void Info_RemoveKey_Post(char *s, const char *key)
// {
//     if (!g_L)
//         RETURN_META(MRES_IGNORED);

//     lua_getglobal(g_L, "MetaInfo_RemoveKey_Post");

//     if (!lua_isfunction(g_L, -1))
//     {
//         lua_pop(g_L, 1);
//         RETURN_META(MRES_IGNORED);
//     }

//     lua_pushstring(g_L, s); // Info buffer content
//     lua_pushstring(g_L, key);

//     if (lua_pcall(g_L, 2, 0, 0) != 0)
//     {
//         lua_pop(g_L, 1);
//     }

//     RETURN_META(MRES_IGNORED);
// }

// /* pfnGetPhysicsKeyValue() */
// const char* GetPhysicsKeyValue_Post(const edict_t *pClient, const char *key)
// {
//     if (!g_L)
//         RETURN_META_VALUE(MRES_IGNORED, NULL);

//     lua_getglobal(g_L, "MetaGetPhysicsKeyValue_Post");

//     if (!lua_isfunction(g_L, -1))
//     {
//         lua_pop(g_L, 1);
//         RETURN_META_VALUE(MRES_IGNORED, NULL);
//     }

//     lua_pushentity(g_L, (edict_t*)pClient);
//     lua_pushstring(g_L, key);

//     if (lua_pcall(g_L, 2, 1, 0) != 0)
//     {
//         lua_pop(g_L, 1);
//         RETURN_META_VALUE(MRES_IGNORED, NULL);
//     }

//     if (lua_isstring(g_L, -1))
//     {
//         static char staticVal[256];
//         const char *ret = lua_tostring(g_L, -1);
        
//         strncpy(staticVal, ret, 255);
//         staticVal[255] = '\0';

//         lua_pop(g_L, 1);
//         RETURN_META_VALUE(MRES_SUPERCEDE, staticVal);
//     }

//     lua_pop(g_L, 1);
//     RETURN_META_VALUE(MRES_IGNORED, NULL);
// }

// /* pfnSetPhysicsKeyValue() */
// void SetPhysicsKeyValue_Post(const edict_t *pClient, const char *key, const char *value)
// {
//     if (!g_L)
//         RETURN_META(MRES_IGNORED);

//     lua_getglobal(g_L, "MetaSetPhysicsKeyValue_Post");

//     if (!lua_isfunction(g_L, -1))
//     {
//         lua_pop(g_L, 1);
//         RETURN_META(MRES_IGNORED);
//     }

//     lua_pushentity(g_L, (edict_t*)pClient);
//     lua_pushstring(g_L, key);
//     lua_pushstring(g_L, value);

//     if (lua_pcall(g_L, 3, 0, 0) != 0)
//     {
//         lua_pop(g_L, 1);
//     }

//     RETURN_META(MRES_IGNORED);
// }

// /* pfnGetPhysicsInfoString() */
// const char* GetPhysicsInfoString_Post(const edict_t *pClient)
// {
//     if (!g_L)
//         RETURN_META_VALUE(MRES_IGNORED, NULL);

//     lua_getglobal(g_L, "MetaGetPhysicsInfoString_Post");

//     if (!lua_isfunction(g_L, -1))
//     {
//         lua_pop(g_L, 1);
//         RETURN_META_VALUE(MRES_IGNORED, NULL);
//     }

//     lua_pushentity(g_L, (edict_t*)pClient);

//     if (lua_pcall(g_L, 1, 1, 0) != 0)
//     {
//         lua_pop(g_L, 1);
//         RETURN_META_VALUE(MRES_IGNORED, NULL);
//     }

//     if (lua_isstring(g_L, -1))
//     {
//         static char staticInfo[2048];
//         const char *ret = lua_tostring(g_L, -1);
        
//         strncpy(staticInfo, ret, 2047);
//         staticInfo[2047] = '\0';

//         lua_pop(g_L, 1);
//         RETURN_META_VALUE(MRES_SUPERCEDE, staticInfo);
//     }

//     lua_pop(g_L, 1);
//     RETURN_META_VALUE(MRES_IGNORED, NULL);
// }

// /* pfnPrecacheEvent() */
// unsigned short PrecacheEvent_Post(int type, const char* psz)
// {
//     if (!g_L)
//         RETURN_META_VALUE(MRES_IGNORED, 0);

//     lua_getglobal(g_L, "MetaPrecacheEvent_Post");

//     if (!lua_isfunction(g_L, -1))
//     {
//         lua_pop(g_L, 1);
//         RETURN_META_VALUE(MRES_IGNORED, 0);
//     }

//     lua_pushinteger(g_L, type);
//     lua_pushstring(g_L, psz);

//     if (lua_pcall(g_L, 2, 1, 0) != 0)
//     {
//         lua_pop(g_L, 1);
//         RETURN_META_VALUE(MRES_IGNORED, 0);
//     }

//     if (lua_isnumber(g_L, -1))
//     {
//         unsigned short ret = (unsigned short)lua_tointeger(g_L, -1);
//         lua_pop(g_L, 1);
//         RETURN_META_VALUE(MRES_SUPERCEDE, ret);
//     }

//     lua_pop(g_L, 1);
//     RETURN_META_VALUE(MRES_IGNORED, 0);
// }

// /* pfnPlaybackEvent() */
// void PlaybackEvent_Post(int flags, const edict_t *pInvoker, unsigned short eventindex, float delay, float *origin, float *angles, float fparam1, float fparam2, int iparam1, int iparam2, int bparam1, int bparam2)
// {
//     if (!g_L)
//         RETURN_META(MRES_IGNORED);

//     lua_getglobal(g_L, "MetaPlaybackEvent_Post");

//     if (!lua_isfunction(g_L, -1))
//     {
//         lua_pop(g_L, 1);
//         RETURN_META(MRES_IGNORED);
//     }

//     lua_pushinteger(g_L, flags);
//     lua_pushentity(g_L, (edict_t*)pInvoker);
//     lua_pushinteger(g_L, eventindex);
//     lua_pushnumber(g_L, delay);
//     lua_pushlightuserdata(g_L, (void*)origin); // Vector ptr
//     lua_pushlightuserdata(g_L, (void*)angles); // Vector ptr
//     lua_pushnumber(g_L, fparam1);
//     lua_pushnumber(g_L, fparam2);
//     lua_pushinteger(g_L, iparam1);
//     lua_pushinteger(g_L, iparam2);
//     lua_pushinteger(g_L, bparam1);
//     lua_pushinteger(g_L, bparam2);

//     if (lua_pcall(g_L, 12, 0, 0) != 0)
//     {
//         lua_pop(g_L, 1);
//     }

//     RETURN_META(MRES_IGNORED);
// }

// /* pfnSetFatPVS() */
// unsigned char *SetFatPVS_Post(float *org)
// {
//     if (!g_L)
//         RETURN_META_VALUE(MRES_IGNORED, NULL);

//     lua_getglobal(g_L, "MetaSetFatPVS_Post");

//     if (!lua_isfunction(g_L, -1))
//     {
//         lua_pop(g_L, 1);
//         RETURN_META_VALUE(MRES_IGNORED, NULL);
//     }

//     lua_pushlightuserdata(g_L, (void*)org);

//     if (lua_pcall(g_L, 1, 1, 0) != 0)
//     {
//         lua_pop(g_L, 1);
//         RETURN_META_VALUE(MRES_IGNORED, NULL);
//     }

//     if (lua_isuserdata(g_L, -1))
//     {
//         unsigned char* ret = (unsigned char*)lua_touserdata(g_L, -1);
//         lua_pop(g_L, 1);
//         RETURN_META_VALUE(MRES_SUPERCEDE, ret);
//     }

//     lua_pop(g_L, 1);
//     RETURN_META_VALUE(MRES_IGNORED, NULL);
// }

// /* pfnSetFatPAS() */
// unsigned char *SetFatPAS_Post(float *org)
// {
//     if (!g_L)
//         RETURN_META_VALUE(MRES_IGNORED, NULL);

//     lua_getglobal(g_L, "MetaSetFatPAS_Post");

//     if (!lua_isfunction(g_L, -1))
//     {
//         lua_pop(g_L, 1);
//         RETURN_META_VALUE(MRES_IGNORED, NULL);
//     }

//     lua_pushlightuserdata(g_L, (void*)org);

//     if (lua_pcall(g_L, 1, 1, 0) != 0)
//     {
//         lua_pop(g_L, 1);
//         RETURN_META_VALUE(MRES_IGNORED, NULL);
//     }

//     if (lua_isuserdata(g_L, -1))
//     {
//         unsigned char* ret = (unsigned char*)lua_touserdata(g_L, -1);
//         lua_pop(g_L, 1);
//         RETURN_META_VALUE(MRES_SUPERCEDE, ret);
//     }

//     lua_pop(g_L, 1);
//     RETURN_META_VALUE(MRES_IGNORED, NULL);
// }

// /* pfnCheckVisibility() */
// int CheckVisibility_Post(const edict_t *entity, unsigned char *pset)
// {
//     if (!g_L)
//         RETURN_META_VALUE(MRES_IGNORED, 0);

//     lua_getglobal(g_L, "MetaCheckVisibility_Post");

//     if (!lua_isfunction(g_L, -1))
//     {
//         lua_pop(g_L, 1);
//         RETURN_META_VALUE(MRES_IGNORED, 0);
//     }

//     lua_pushentity(g_L, (edict_t*)entity);
//     lua_pushlightuserdata(g_L, (void*)pset);

//     if (lua_pcall(g_L, 2, 1, 0) != 0)
//     {
//         lua_pop(g_L, 1);
//         RETURN_META_VALUE(MRES_IGNORED, 0);
//     }

//     if (lua_isnumber(g_L, -1))
//     {
//         int ret = (int)lua_tointeger(g_L, -1);
//         lua_pop(g_L, 1);
//         RETURN_META_VALUE(MRES_SUPERCEDE, ret);
//     }

//     lua_pop(g_L, 1);
//     RETURN_META_VALUE(MRES_IGNORED, 0);
// }

// /* pfnDeltaSetField() */
// void DeltaSetField_Post(struct delta_s *pFields, const char *fieldname)
// {
//     if (!g_L)
//         RETURN_META(MRES_IGNORED);

//     lua_getglobal(g_L, "MetaDeltaSetField_Post");

//     if (!lua_isfunction(g_L, -1))
//     {
//         lua_pop(g_L, 1);
//         RETURN_META(MRES_IGNORED);
//     }

//     lua_pushlightuserdata(g_L, (void*)pFields);
//     lua_pushstring(g_L, fieldname);

//     if (lua_pcall(g_L, 2, 0, 0) != 0)
//     {
//         lua_pop(g_L, 1);
//     }

//     RETURN_META(MRES_IGNORED);
// }
// /* pfnDeltaUnsetField() */
// void DeltaUnsetField_Post(struct delta_s *pFields, const char *fieldname)
// {
//     if (!g_L)
//         RETURN_META(MRES_IGNORED);

//     lua_getglobal(g_L, "MetaDeltaUnsetField_Post");

//     if (!lua_isfunction(g_L, -1))
//     {
//         lua_pop(g_L, 1);
//         RETURN_META(MRES_IGNORED);
//     }

//     lua_pushlightuserdata(g_L, (void*)pFields);
//     lua_pushstring(g_L, fieldname);

//     if (lua_pcall(g_L, 2, 0, 0) != 0)
//     {
//         lua_pop(g_L, 1);
//     }

//     RETURN_META(MRES_IGNORED);
// }

// /* pfnDeltaAddEncoder() */
// // void DeltaAddEncoder_Post(char *name, void (*conditionalencode)(struct delta_s *pFields, const unsigned char *from, const unsigned char *to))
// // {
// //     if (!g_L)
// //         RETURN_META(MRES_IGNORED);

// //     lua_getglobal(g_L, "MetaDeltaAddEncoder_Post");

// //     if (!lua_isfunction(g_L, -1))
// //     {
// //         lua_pop(g_L, 1);
// //         RETURN_META(MRES_IGNORED);
// //     }

// //     lua_pushstring(g_L, name);
// //     // 函数指针只能作为 lightuserdata 传递
// //     lua_pushlightuserdata(g_L, (void*)conditionalencode);

// //     if (lua_pcall(g_L, 2, 0, 0) != 0)
// //     {
// //         lua_pop(g_L, 1);
// //     }

// //     RETURN_META(MRES_IGNORED);
// // }

// /* pfnGetCurrentPlayer() */
// int GetCurrentPlayer_Post(void)
// {
//     if (!g_L)
//         RETURN_META_VALUE(MRES_IGNORED, 0);

//     lua_getglobal(g_L, "MetaGetCurrentPlayer_Post");

//     if (!lua_isfunction(g_L, -1))
//     {
//         lua_pop(g_L, 1);
//         RETURN_META_VALUE(MRES_IGNORED, 0);
//     }

//     if (lua_pcall(g_L, 0, 1, 0) != 0)
//     {
//         lua_pop(g_L, 1);
//         RETURN_META_VALUE(MRES_IGNORED, 0);
//     }

//     if (lua_isnumber(g_L, -1))
//     {
//         int ret = (int)lua_tointeger(g_L, -1);
//         lua_pop(g_L, 1);
//         RETURN_META_VALUE(MRES_SUPERCEDE, ret);
//     }

//     lua_pop(g_L, 1);
//     RETURN_META_VALUE(MRES_IGNORED, 0);
// }

// /* pfnCanSkipPlayer() */
// int CanSkipPlayer_Post(const edict_t *pPlayer)
// {
//     if (!g_L)
//         RETURN_META_VALUE(MRES_IGNORED, 0);

//     lua_getglobal(g_L, "MetaCanSkipPlayer_Post");

//     if (!lua_isfunction(g_L, -1))
//     {
//         lua_pop(g_L, 1);
//         RETURN_META_VALUE(MRES_IGNORED, 0);
//     }

//     lua_pushentity(g_L, (edict_t*)pPlayer);

//     if (lua_pcall(g_L, 1, 1, 0) != 0)
//     {
//         lua_pop(g_L, 1);
//         RETURN_META_VALUE(MRES_IGNORED, 0);
//     }

//     if (lua_isnumber(g_L, -1))
//     {
//         int ret = (int)lua_tointeger(g_L, -1);
//         lua_pop(g_L, 1);
//         RETURN_META_VALUE(MRES_SUPERCEDE, ret);
//     }

//     lua_pop(g_L, 1);
//     RETURN_META_VALUE(MRES_IGNORED, 0);
// }

// /* pfnDeltaFindField() */
// int DeltaFindField_Post(struct delta_s *pFields, const char *fieldname)
// {
//     if (!g_L)
//         RETURN_META_VALUE(MRES_IGNORED, 0);

//     lua_getglobal(g_L, "MetaDeltaFindField_Post");

//     if (!lua_isfunction(g_L, -1))
//     {
//         lua_pop(g_L, 1);
//         RETURN_META_VALUE(MRES_IGNORED, 0);
//     }

//     lua_pushlightuserdata(g_L, (void*)pFields);
//     lua_pushstring(g_L, fieldname);

//     if (lua_pcall(g_L, 2, 1, 0) != 0)
//     {
//         lua_pop(g_L, 1);
//         RETURN_META_VALUE(MRES_IGNORED, 0);
//     }

//     if (lua_isnumber(g_L, -1))
//     {
//         int ret = (int)lua_tointeger(g_L, -1);
//         lua_pop(g_L, 1);
//         RETURN_META_VALUE(MRES_SUPERCEDE, ret);
//     }

//     lua_pop(g_L, 1);
//     RETURN_META_VALUE(MRES_IGNORED, 0);
// }

// /* pfnDeltaSetFieldByIndex() */
// void DeltaSetFieldByIndex_Post(struct delta_s *pFields, int fieldNumber)
// {
//     if (!g_L)
//         RETURN_META(MRES_IGNORED);

//     lua_getglobal(g_L, "MetaDeltaSetFieldByIndex_Post");

//     if (!lua_isfunction(g_L, -1))
//     {
//         lua_pop(g_L, 1);
//         RETURN_META(MRES_IGNORED);
//     }

//     lua_pushlightuserdata(g_L, (void*)pFields);
//     lua_pushinteger(g_L, fieldNumber);

//     if (lua_pcall(g_L, 2, 0, 0) != 0)
//     {
//         lua_pop(g_L, 1);
//     }

//     RETURN_META(MRES_IGNORED);
// }

// /* pfnDeltaUnsetFieldByIndex() */
// void DeltaUnsetFieldByIndex_Post(struct delta_s *pFields, int fieldNumber)
// {
//     if (!g_L)
//         RETURN_META(MRES_IGNORED);

//     lua_getglobal(g_L, "MetaDeltaUnsetFieldByIndex_Post");

//     if (!lua_isfunction(g_L, -1))
//     {
//         lua_pop(g_L, 1);
//         RETURN_META(MRES_IGNORED);
//     }

//     lua_pushlightuserdata(g_L, (void*)pFields);
//     lua_pushinteger(g_L, fieldNumber);

//     if (lua_pcall(g_L, 2, 0, 0) != 0)
//     {
//         lua_pop(g_L, 1);
//     }

//     RETURN_META(MRES_IGNORED);
// }

// /* pfnSetGroupMask() */
// void SetGroupMask_Post(int mask, int op)
// {
//     if (!g_L)
//         RETURN_META(MRES_IGNORED);

//     lua_getglobal(g_L, "MetaSetGroupMask_Post");

//     if (!lua_isfunction(g_L, -1))
//     {
//         lua_pop(g_L, 1);
//         RETURN_META(MRES_IGNORED);
//     }

//     lua_pushinteger(g_L, mask);
//     lua_pushinteger(g_L, op);

//     if (lua_pcall(g_L, 2, 0, 0) != 0)
//     {
//         lua_pop(g_L, 1);
//     }

//     RETURN_META(MRES_IGNORED);
// }

// /* pfnengCreateInstancedBaseline() */
// int engCreateInstancedBaseline_Post(int classname, struct entity_state_s *baseline)
// {
//     if (!g_L)
//         RETURN_META_VALUE(MRES_IGNORED, 0);

//     lua_getglobal(g_L, "MetaEngCreateInstancedBaseline_Post");

//     if (!lua_isfunction(g_L, -1))
//     {
//         lua_pop(g_L, 1);
//         RETURN_META_VALUE(MRES_IGNORED, 0);
//     }

//     lua_pushinteger(g_L, classname);
//     lua_pushlightuserdata(g_L, (void*)baseline);

//     if (lua_pcall(g_L, 2, 1, 0) != 0)
//     {
//         lua_pop(g_L, 1);
//         RETURN_META_VALUE(MRES_IGNORED, 0);
//     }

//     if (lua_isnumber(g_L, -1))
//     {
//         int ret = (int)lua_tointeger(g_L, -1);
//         lua_pop(g_L, 1);
//         RETURN_META_VALUE(MRES_SUPERCEDE, ret);
//     }

//     lua_pop(g_L, 1);
//     RETURN_META_VALUE(MRES_IGNORED, 0);
// }

// /* pfnCvar_DirectSet() */
// // void Cvar_DirectSet_Post(struct cvar_s *var, char *value)
// // {
// //     if (!g_L)
// //         RETURN_META(MRES_IGNORED);

// //     lua_getglobal(g_L, "MetaCvar_DirectSet_Post");

// //     if (!lua_isfunction(g_L, -1))
// //     {
// //         lua_pop(g_L, 1);
// //         RETURN_META(MRES_IGNORED);
// //     }

// //     lua_pushlightuserdata(g_L, (void*)var);
// //     lua_pushstring(g_L, value);

// //     if (lua_pcall(g_L, 2, 0, 0) != 0)
// //     {
// //         lua_pop(g_L, 1);
// //     }

// //     RETURN_META(MRES_IGNORED);
// // }
// /* pfnForceUnmodified() */
// void ForceUnmodified_Post(const float *mins, const float *absmin, const float *maxs, const float *absmax, const char *filename)
// {
//     if (!g_L)
//         RETURN_META(MRES_IGNORED);

//     lua_getglobal(g_L, "MetaForceUnmodified_Post");

//     if (!lua_isfunction(g_L, -1))
//     {
//         lua_pop(g_L, 1);
//         RETURN_META(MRES_IGNORED);
//     }

//     lua_pushlightuserdata(g_L, (void*)mins);
//     lua_pushlightuserdata(g_L, (void*)absmin);
//     lua_pushlightuserdata(g_L, (void*)maxs);
//     lua_pushlightuserdata(g_L, (void*)absmax);
//     lua_pushstring(g_L, filename);

//     if (lua_pcall(g_L, 5, 0, 0) != 0)
//     {
//         lua_pop(g_L, 1);
//     }

//     RETURN_META(MRES_IGNORED);
// }

// /* pfnGetPlayerStats() */
// void GetPlayerStats_Post(const edict_t *pClient, int *ping, int *packet_loss)
// {
//     if (!g_L)
//         RETURN_META(MRES_IGNORED);

//     lua_getglobal(g_L, "MetaGetPlayerStats_Post");

//     if (!lua_isfunction(g_L, -1))
//     {
//         lua_pop(g_L, 1);
//         RETURN_META(MRES_IGNORED);
//     }

//     lua_pushentity(g_L, (edict_t*)pClient);
//     lua_pushlightuserdata(g_L, (void*)ping);        // 输出参数指针
//     lua_pushlightuserdata(g_L, (void*)packet_loss); // 输出参数指针

//     if (lua_pcall(g_L, 3, 0, 0) != 0)
//     {
//         lua_pop(g_L, 1);
//     }

//     RETURN_META(MRES_IGNORED);
// }

// /* pfnAddServerCommand() */
// void AddServerCommand_Post(char *cmd_name, void (*function)(void))
// {
//     if (!g_L)
//         RETURN_META(MRES_IGNORED);

//     lua_getglobal(g_L, "MetaAddServerCommand_Post");

//     if (!lua_isfunction(g_L, -1))
//     {
//         lua_pop(g_L, 1);
//         RETURN_META(MRES_IGNORED);
//     }

//     lua_pushstring(g_L, cmd_name);
//     lua_pushlightuserdata(g_L, (void*)function); // 函数指针

//     if (lua_pcall(g_L, 2, 0, 0) != 0)
//     {
//         lua_pop(g_L, 1);
//     }

//     RETURN_META(MRES_IGNORED);
// }

// /* pfnVoice_GetClientListening() */
// qboolean Voice_GetClientListening_Post(int iReceiver, int iSender)
// {
//     if (!g_L)
//         RETURN_META_VALUE(MRES_IGNORED, 0);

//     lua_getglobal(g_L, "MetaVoice_GetClientListening_Post");

//     if (!lua_isfunction(g_L, -1))
//     {
//         lua_pop(g_L, 1);
//         RETURN_META_VALUE(MRES_IGNORED, 0);
//     }

//     lua_pushinteger(g_L, iReceiver);
//     lua_pushinteger(g_L, iSender);

//     if (lua_pcall(g_L, 2, 1, 0) != 0)
//     {
//         lua_pop(g_L, 1);
//         RETURN_META_VALUE(MRES_IGNORED, 0);
//     }

//     if (lua_isnumber(g_L, -1))
//     {
//         qboolean ret = (qboolean)lua_tointeger(g_L, -1);
//         lua_pop(g_L, 1);
//         RETURN_META_VALUE(MRES_SUPERCEDE, ret);
//     }

//     lua_pop(g_L, 1);
//     RETURN_META_VALUE(MRES_IGNORED, 0);
// }

// /* pfnVoice_SetClientListening() */
// qboolean Voice_SetClientListening_Post(int iReceiver, int iSender, qboolean bListen)
// {
//     if (!g_L)
//         RETURN_META_VALUE(MRES_IGNORED, 0);

//     lua_getglobal(g_L, "MetaVoice_SetClientListening_Post");

//     if (!lua_isfunction(g_L, -1))
//     {
//         lua_pop(g_L, 1);
//         RETURN_META_VALUE(MRES_IGNORED, 0);
//     }

//     lua_pushinteger(g_L, iReceiver);
//     lua_pushinteger(g_L, iSender);
//     lua_pushinteger(g_L, bListen);

//     if (lua_pcall(g_L, 3, 1, 0) != 0)
//     {
//         lua_pop(g_L, 1);
//         RETURN_META_VALUE(MRES_IGNORED, 0);
//     }

//     if (lua_isnumber(g_L, -1))
//     {
//         qboolean ret = (qboolean)lua_tointeger(g_L, -1);
//         lua_pop(g_L, 1);
//         RETURN_META_VALUE(MRES_SUPERCEDE, ret);
//     }

//     lua_pop(g_L, 1);
//     RETURN_META_VALUE(MRES_IGNORED, 0);
// }

// /* pfnGetPlayerAuthId() */
// const char *GetPlayerAuthId_Post(edict_t *e)
// {
//     if (!g_L)
//         RETURN_META_VALUE(MRES_IGNORED, NULL);

//     lua_getglobal(g_L, "MetaGetPlayerAuthId_Post");

//     if (!lua_isfunction(g_L, -1))
//     {
//         lua_pop(g_L, 1);
//         RETURN_META_VALUE(MRES_IGNORED, NULL);
//     }

//     lua_pushentity(g_L, e);

//     if (lua_pcall(g_L, 1, 1, 0) != 0)
//     {
//         lua_pop(g_L, 1);
//         RETURN_META_VALUE(MRES_IGNORED, NULL);
//     }

//     if (lua_isstring(g_L, -1))
//     {
//         static char staticAuth[64];
//         const char *ret = lua_tostring(g_L, -1);
        
//         strncpy(staticAuth, ret, 63);
//         staticAuth[63] = '\0';

//         lua_pop(g_L, 1);
//         RETURN_META_VALUE(MRES_SUPERCEDE, staticAuth);
//     }

//     lua_pop(g_L, 1);
//     RETURN_META_VALUE(MRES_IGNORED, NULL);
// }

// // -------------------------------------------------------------
// // 以下函数通常位于 DLL_FUNCTIONS (Game DLL) 中，而非 enginefuncs_t
// // -------------------------------------------------------------

// /* OnFreeEntPrivateData */
// void OnFreeEntPrivateData_Post(edict_t *pEnt)
// {
//     if (!g_L)
//         RETURN_META(MRES_IGNORED);

//     lua_getglobal(g_L, "MetaOnFreeEntPrivateData_Post");

//     if (!lua_isfunction(g_L, -1))
//     {
//         lua_pop(g_L, 1);
//         RETURN_META(MRES_IGNORED);
//     }

//     lua_pushentity(g_L, pEnt);

//     if (lua_pcall(g_L, 1, 0, 0) != 0)
//     {
//         lua_pop(g_L, 1);
//     }

//     RETURN_META(MRES_IGNORED);
// }

// /* GameShutdown */
// void GameShutdown_Post(void)
// {
//     if (!g_L)
//         RETURN_META(MRES_IGNORED);

//     lua_getglobal(g_L, "MetaGameShutdown_Post");

//     if (!lua_isfunction(g_L, -1))
//     {
//         lua_pop(g_L, 1);
//         RETURN_META(MRES_IGNORED);
//     }

//     if (lua_pcall(g_L, 0, 0, 0) != 0)
//     {
//         lua_pop(g_L, 1);
//     }

//     RETURN_META(MRES_IGNORED);
// }

// /* ShouldCollide */
// int ShouldCollide_Post(edict_t *pentTouched, edict_t *pentOther)
// {
//     if (!g_L)
//         RETURN_META_VALUE(MRES_IGNORED, 0);

//     lua_getglobal(g_L, "MetaShouldCollide_Post");

//     if (!lua_isfunction(g_L, -1))
//     {
//         lua_pop(g_L, 1);
//         RETURN_META_VALUE(MRES_IGNORED, 0);
//     }

//     lua_pushentity(g_L, pentTouched);
//     lua_pushentity(g_L, pentOther);

//     if (lua_pcall(g_L, 2, 1, 0) != 0)
//     {
//         lua_pop(g_L, 1);
//         RETURN_META_VALUE(MRES_IGNORED, 0);
//     }

//     if (lua_isnumber(g_L, -1))
//     {
//         int ret = (int)lua_tointeger(g_L, -1);
//         lua_pop(g_L, 1);
//         RETURN_META_VALUE(MRES_SUPERCEDE, ret);
//     }

//     lua_pop(g_L, 1);
//     RETURN_META_VALUE(MRES_IGNORED, 0);
// }

// /* 1. CBasePlayer::Spawn */
// void CBasePlayer_Spawn(IReGameHook_CBasePlayer_Spawn *chain, CBasePlayer *pthis)
// {
//     // Spawn 没有可修改的参数，只做通知
//     if (g_L) 
//     {
//         lua_getglobal(g_L, "ReGame_CBasePlayer_Spawn");
//         if (lua_isfunction(g_L, -1)) 
//         {
//             edict_t* pEnt = pthis ? ENT(((CCSPlayer*)pthis)->pev) : NULL;
//             lua_pushentity(g_L, pEnt);

//             if (lua_pcall(g_L, 1, 0, 0) != 0) lua_pop(g_L, 1);
//         }
//         else lua_pop(g_L, 1);
//     }
//     chain->callNext(pthis);
// }

// /* 2. CBasePlayer::Precache */
// void CBasePlayer_Precache(IReGameHook_CBasePlayer_Precache *chain, CBasePlayer *pthis)
// {
//     // Precache 没有可修改的参数
//     if (g_L) 
//     {
//         lua_getglobal(g_L, "ReGame_CBasePlayer_Precache");
//         if (lua_isfunction(g_L, -1)) 
//         {
//             edict_t* pEnt = pthis ? ENT(((CCSPlayer*)pthis)->pev) : NULL;
//             lua_pushentity(g_L, pEnt);

//             if (lua_pcall(g_L, 1, 0, 0) != 0) lua_pop(g_L, 1);
//         }
//         else lua_pop(g_L, 1);
//     }
//     chain->callNext(pthis);
// }

// /* 3. CBasePlayer::ObjectCaps */
// int CBasePlayer_ObjectCaps(IReGameHook_CBasePlayer_ObjectCaps *chain, CBasePlayer *pthis)
// {
//     // ObjectCaps 没有参数，但 Lua 可能返回一个 Int 直接覆盖结果
//     if (g_L) 
//     {
//         lua_getglobal(g_L, "ReGame_CBasePlayer_ObjectCaps");
//         if (lua_isfunction(g_L, -1)) 
//         {
//             edict_t* pEnt = pthis ? ENT(((CCSPlayer*)pthis)->pev) : NULL;
//             lua_pushentity(g_L, pEnt);

//             if (lua_pcall(g_L, 1, 1, 0) == 0)
//             {
//                 // 如果 Lua 返回了值，直接作为结果返回 (Supercede)
//                 if (lua_isnumber(g_L, -1))
//                 {
//                     int ret = (int)lua_tointeger(g_L, -1);
//                     lua_pop(g_L, 1);
//                     return ret;
//                 }
//                 lua_pop(g_L, 1);
//             }
//             else lua_pop(g_L, 1);
//         }
//         else lua_pop(g_L, 1);
//     }
//     return chain->callNext(pthis);
// }

// /* 4. CBasePlayer::Classify */
// int CBasePlayer_Classify(IReGameHook_CBasePlayer_Classify *chain, CBasePlayer *pthis)
// {
//     // Classify 没有参数，Lua 返回 Int 覆盖结果
//     if (g_L) 
//     {
//         lua_getglobal(g_L, "ReGame_CBasePlayer_Classify");
//         if (lua_isfunction(g_L, -1)) 
//         {
//             edict_t* pEnt = pthis ? ENT(((CCSPlayer*)pthis)->pev) : NULL;
//             lua_pushentity(g_L, pEnt);

//             if (lua_pcall(g_L, 1, 1, 0) == 0)
//             {
//                 if (lua_isnumber(g_L, -1))
//                 {
//                     int ret = (int)lua_tointeger(g_L, -1);
//                     lua_pop(g_L, 1);
//                     return ret;
//                 }
//                 lua_pop(g_L, 1);
//             }
//             else lua_pop(g_L, 1);
//         }
//         else lua_pop(g_L, 1);
//     }
//     return chain->callNext(pthis);
// }

// /* 5. CBasePlayer::TraceAttack */
// void CBasePlayer_TraceAttack(IReGameHook_CBasePlayer_TraceAttack *chain, CBasePlayer *pthis, entvars_t *pevAttacker, float flDamage, Vector &vecDir, TraceResult *ptr, int bitsDamageType)
// {
//     // 准备本地变量，以便 Lua 修改
//     float _flDamage = flDamage;
//     int _bitsDamageType = bitsDamageType;

//     if (g_L) 
//     {
//         lua_getglobal(g_L, "ReGame_CBasePlayer_TraceAttack");
//         if (lua_isfunction(g_L, -1)) 
//         {
//             edict_t* pVictim = pthis ? ENT(((CCSPlayer*)pthis)->pev) : NULL;
//             lua_pushentity(g_L, pVictim);
            
//             edict_t* pAttacker = pevAttacker ? ENT(pevAttacker) : NULL;
//             lua_pushentity(g_L, pAttacker);

//             lua_pushnumber(g_L, _flDamage);
//             lua_pushlightuserdata(g_L, &vecDir); // 向量指针通常 Lua 只读或通过 API 改，不通过 Return 改
//             lua_pushlightuserdata(g_L, ptr);
//             lua_pushinteger(g_L, _bitsDamageType);

//             // 期望返回 2 个值: newDamage, newBits
//             if (lua_pcall(g_L, 6, 2, 0) == 0)
//             {
//                 // 栈顶是 bits (-1), 次顶是 damage (-2)
//                 if (lua_isnumber(g_L, -2)) _flDamage = (float)lua_tonumber(g_L, -2);
//                 if (lua_isnumber(g_L, -1)) _bitsDamageType = (int)lua_tointeger(g_L, -1);
                
//                 lua_pop(g_L, 2);
//             }
//             else lua_pop(g_L, 1);
//         }
//         else lua_pop(g_L, 1);
//     }
//     // 使用可能被 Lua 修改过的变量调用 callNext
//     chain->callNext(pthis, pevAttacker, _flDamage, vecDir, ptr, _bitsDamageType);
// }

// /* 6. CBasePlayer::TakeDamage */
// BOOL CBasePlayer_TakeDamage(IReGameHook_CBasePlayer_TakeDamage *chain, CBasePlayer *pthis, entvars_t *pevInflictor, entvars_t *pevAttacker, float &flDamage, int bitsDamageType)
// {
//     float _flDamage = flDamage;
//     int _bitsDamageType = bitsDamageType;

//     if (g_L) 
//     {
//         lua_getglobal(g_L, "ReGame_CBasePlayer_TakeDamage");
//         if (lua_isfunction(g_L, -1)) 
//         {
//             edict_t* pVictim = pthis ? ENT(((CCSPlayer*)pthis)->pev) : NULL;
//             lua_pushentity(g_L, pVictim);

//             edict_t* pInflictor = pevInflictor ? ENT(pevInflictor) : NULL;
//             lua_pushentity(g_L, pInflictor);

//             edict_t* pAttacker = pevAttacker ? ENT(pevAttacker) : NULL;
//             lua_pushentity(g_L, pAttacker);

//             lua_pushnumber(g_L, _flDamage);
//             lua_pushinteger(g_L, _bitsDamageType);

//             // 期望返回 2 个值: newDamage, newBits
//             if (lua_pcall(g_L, 5, 2, 0) == 0)
//             {
//                 if (lua_isnumber(g_L, -2)) _flDamage = (float)lua_tonumber(g_L, -2);
//                 if (lua_isnumber(g_L, -1)) _bitsDamageType = (int)lua_tointeger(g_L, -1);

//                 lua_pop(g_L, 2);
//             }
//             else lua_pop(g_L, 1);
//         }
//         else lua_pop(g_L, 1);
//     }
//     // 注意：flDamage 是引用传递，我们将本地修改后的值传给 callNext
//     return chain->callNext(pthis, pevInflictor, pevAttacker, _flDamage, _bitsDamageType);
// }

// /* 7. CBasePlayer::TakeHealth */
// BOOL CBasePlayer_TakeHealth(IReGameHook_CBasePlayer_TakeHealth *chain, CBasePlayer *pthis, float flHealth, int bitsDamageType)
// {
//     float _flHealth = flHealth;
//     int _bitsDamageType = bitsDamageType;

//     if (g_L) 
//     {
//         lua_getglobal(g_L, "ReGame_CBasePlayer_TakeHealth");
//         if (lua_isfunction(g_L, -1)) 
//         {
//             edict_t* pVictim = pthis ? ENT(((CCSPlayer*)pthis)->pev) : NULL;
//             lua_pushentity(g_L, pVictim);

//             lua_pushnumber(g_L, _flHealth);
//             lua_pushinteger(g_L, _bitsDamageType);

//             // 期望返回 2 个值: newHealth, newBits
//             if (lua_pcall(g_L, 3, 2, 0) == 0)
//             {
//                 if (lua_isnumber(g_L, -2)) _flHealth = (float)lua_tonumber(g_L, -2);
//                 if (lua_isnumber(g_L, -1)) _bitsDamageType = (int)lua_tointeger(g_L, -1);
                
//                 lua_pop(g_L, 2);
//             }
//             else lua_pop(g_L, 1);
//         }
//         else lua_pop(g_L, 1);
//     }
//     return chain->callNext(pthis, _flHealth, _bitsDamageType);
// }

// /* 8. CBasePlayer::Killed */
// void CBasePlayer_Killed(IReGameHook_CBasePlayer_Killed *chain, CBasePlayer *pthis, entvars_t *pevAttacker, int iGib)
// {
//     int _iGib = iGib;

//     if (g_L) 
//     {
//         lua_getglobal(g_L, "ReGame_CBasePlayer_Killed");
//         if (lua_isfunction(g_L, -1)) 
//         {
//             edict_t* pVictim = pthis ? ENT(((CCSPlayer*)pthis)->pev) : NULL;
//             lua_pushentity(g_L, pVictim);

//             edict_t* pAttacker = pevAttacker ? ENT(pevAttacker) : NULL;
//             lua_pushentity(g_L, pAttacker);

//             lua_pushinteger(g_L, _iGib);

//             // 期望返回 1 个值: newGib
//             if (lua_pcall(g_L, 3, 1, 0) == 0)
//             {
//                 if (lua_isnumber(g_L, -1)) _iGib = (int)lua_tointeger(g_L, -1);
//                 lua_pop(g_L, 1);
//             }
//             else lua_pop(g_L, 1);
//         }
//         else lua_pop(g_L, 1);
//     }
//     chain->callNext(pthis, pevAttacker, _iGib);
// }

// /* 9. CBasePlayer::AddPoints */
// void CBasePlayer_AddPoints(IReGameHook_CBasePlayer_AddPoints *chain, CBasePlayer *pthis, int score, BOOL bAllowNegative)
// {
//     int _score = score;
//     BOOL _bAllowNegative = bAllowNegative;

//     if (g_L) 
//     {
//         lua_getglobal(g_L, "ReGame_CBasePlayer_AddPoints");
//         if (lua_isfunction(g_L, -1)) 
//         {
//             edict_t* pEnt = pthis ? ENT(((CCSPlayer*)pthis)->pev) : NULL;
//             lua_pushentity(g_L, pEnt);

//             lua_pushinteger(g_L, _score);
//             lua_pushboolean(g_L, _bAllowNegative);

//             // 期望返回 2 个值: newScore, newAllowNegative
//             if (lua_pcall(g_L, 3, 2, 0) == 0)
//             {
//                 if (lua_isnumber(g_L, -2)) _score = (int)lua_tointeger(g_L, -2);
//                 if (lua_isboolean(g_L, -1)) _bAllowNegative = (BOOL)lua_toboolean(g_L, -1);
//                 lua_pop(g_L, 2);
//             }
//             else lua_pop(g_L, 1);
//         }
//         else lua_pop(g_L, 1);
//     }
//     chain->callNext(pthis, _score, _bAllowNegative);
// }

// /* 10. CBasePlayer::AddPointsToTeam */
// void CBasePlayer_AddPointsToTeam(IReGameHook_CBasePlayer_AddPointsToTeam *chain, CBasePlayer *pthis, int score, BOOL bAllowNegative)
// {
//     int _score = score;
//     BOOL _bAllowNegative = bAllowNegative;

//     if (g_L) 
//     {
//         lua_getglobal(g_L, "ReGame_CBasePlayer_AddPointsToTeam");
//         if (lua_isfunction(g_L, -1)) 
//         {
//             edict_t* pEnt = pthis ? ENT(((CCSPlayer*)pthis)->pev) : NULL;
//             lua_pushentity(g_L, pEnt);

//             lua_pushinteger(g_L, _score);
//             lua_pushboolean(g_L, _bAllowNegative);

//             // 期望返回 2 个值: newScore, newAllowNegative
//             if (lua_pcall(g_L, 3, 2, 0) == 0)
//             {
//                 if (lua_isnumber(g_L, -2)) _score = (int)lua_tointeger(g_L, -2);
//                 if (lua_isboolean(g_L, -1)) _bAllowNegative = (BOOL)lua_toboolean(g_L, -1);
//                 lua_pop(g_L, 2);
//             }
//             else lua_pop(g_L, 1);
//         }
//         else lua_pop(g_L, 1);
//     }
//     chain->callNext(pthis, _score, _bAllowNegative);
// }
// /* 11. CBasePlayer::RoundRespawn */
// void CBasePlayer_RoundRespawn(IReGameHook_CBasePlayer_RoundRespawn *chain, CBasePlayer *pthis)
// {
//     if (g_L) 
//     {
//         lua_getglobal(g_L, "ReGame_CBasePlayer_RoundRespawn");
//         if (lua_isfunction(g_L, -1)) 
//         {
//             edict_t* pEnt = pthis ? ENT(((CCSPlayer*)pthis)->pev) : NULL;
//             lua_pushentity(g_L, pEnt);

//             if (lua_pcall(g_L, 1, 1, 0) == 0)
//             {
//                 if (lua_isboolean(g_L, -1) && lua_toboolean(g_L, -1))
//                 {
//                     lua_pop(g_L, 1);
//                     return;
//                 }
//                 lua_pop(g_L, 1);
//             }
//             else lua_pop(g_L, 1);
//         }
//         else lua_pop(g_L, 1);
//     }
//     chain->callNext(pthis);
// }

// /* 12. CBasePlayer::Blind */
// void CBasePlayer_Blind(IReGameHook_CBasePlayer_Blind *chain, CBasePlayer *pthis, float view_fade_time, float view_fade_hold, float view_fade_alpha, int view_fade_flags)
// {
//     float _time = view_fade_time;
//     float _hold = view_fade_hold;
//     float _alpha = view_fade_alpha;
//     int _flags = view_fade_flags;

//     if (g_L) 
//     {
//         lua_getglobal(g_L, "ReGame_CBasePlayer_Blind");
//         if (lua_isfunction(g_L, -1)) 
//         {
//             edict_t* pEnt = pthis ? ENT(((CCSPlayer*)pthis)->pev) : NULL;
//             lua_pushentity(g_L, pEnt);

//             lua_pushnumber(g_L, _time);
//             lua_pushnumber(g_L, _hold);
//             lua_pushnumber(g_L, _alpha);
//             lua_pushinteger(g_L, _flags);

//             if (lua_pcall(g_L, 5, LUA_MULTRET, 0) == 0)
//             {
//                 int nRet = lua_gettop(g_L);
//                 if (nRet == 1 && lua_isboolean(g_L, -1) && lua_toboolean(g_L, -1))
//                 {
//                     lua_pop(g_L, 1);
//                     return;
//                 }
//                 else if (nRet == 4)
//                 {
//                     if (lua_isnumber(g_L, -4)) _time = (float)lua_tonumber(g_L, -4);
//                     if (lua_isnumber(g_L, -3)) _hold = (float)lua_tonumber(g_L, -3);
//                     if (lua_isnumber(g_L, -2)) _alpha = (float)lua_tonumber(g_L, -2);
//                     if (lua_isnumber(g_L, -1)) _flags = (int)lua_tointeger(g_L, -1);
//                     lua_pop(g_L, 4);
//                 }
//                 else
//                 {
//                     lua_pop(g_L, nRet);
//                 }
//             }
//             else lua_pop(g_L, 1);
//         }
//         else lua_pop(g_L, 1);
//     }
//     chain->callNext(pthis, _time, _hold, _alpha, _flags);
// }

// /* 13. CBasePlayer::Observer_IsValidTarget */
// CBasePlayer *CBasePlayer_Observer_IsValidTarget(IReGameHook_CBasePlayer_Observer_IsValidTarget *chain, CBasePlayer *pthis, int iTargetIndex, bool bSameTeam)
// {
//     int _iTargetIndex = iTargetIndex;
//     bool _bSameTeam = bSameTeam;

//     if (g_L) 
//     {
//         lua_getglobal(g_L, "ReGame_CBasePlayer_Observer_IsValidTarget");
//         if (lua_isfunction(g_L, -1)) 
//         {
//             edict_t* pEnt = pthis ? ENT(((CCSPlayer*)pthis)->pev) : NULL;
//             lua_pushentity(g_L, pEnt);

//             lua_pushinteger(g_L, _iTargetIndex);
//             lua_pushboolean(g_L, _bSameTeam);

//             if (lua_pcall(g_L, 3, LUA_MULTRET, 0) == 0)
//             {
//                 int nRet = lua_gettop(g_L);
//                 if (nRet == 1)
//                 {
//                     if (lua_isnumber(g_L, -1) || lua_isuserdata(g_L, -1))
//                     {
//                         // 假设 Lua 返回的是 Entity Index 或 Edict，需要转换回 CBasePlayer*
//                         // 这里使用 GET_PRIVATE 宏进行转换，前提是实体有效且有 Private Data
//                          edict_t* pRetEnt = NULL;
//                          if (lua_isnumber(g_L, -1)) {
//                              int idx = (int)lua_tointeger(g_L, -1);
//                              if (idx > 0 && idx <= gpGlobals->maxClients) pRetEnt = INDEXENT(idx);
//                          } else if (lua_isuserdata(g_L, -1)) {
//                              pRetEnt = (edict_t*)lua_touserdata(g_L, -1); // 这里的 to userdata 需根据你的 lua_pushentity 实现调整
//                          }
                         
//                          CBasePlayer* pRetPlayer = pRetEnt ? (CBasePlayer*)GET_PRIVATE(pRetEnt) : NULL;
//                          lua_pop(g_L, 1);
//                          return pRetPlayer;
//                     }
//                     else if (lua_isnil(g_L, -1))
//                     {
//                          lua_pop(g_L, 1);
//                          return NULL;
//                     }
//                     lua_pop(g_L, 1);
//                 }
//                 else if (nRet == 2)
//                 {
//                     if (lua_isnumber(g_L, -2)) _iTargetIndex = (int)lua_tointeger(g_L, -2);
//                     if (lua_isboolean(g_L, -1)) _bSameTeam = (bool)lua_toboolean(g_L, -1);
//                     lua_pop(g_L, 2);
//                 }
//                 else
//                 {
//                     lua_pop(g_L, nRet);
//                 }
//             }
//             else lua_pop(g_L, 1);
//         }
//         else lua_pop(g_L, 1);
//     }
//     return chain->callNext(pthis, _iTargetIndex, _bSameTeam);
// }

// /* 14. CBasePlayer::SetAnimation */
// void CBasePlayer_SetAnimation(IReGameHook_CBasePlayer_SetAnimation *chain, CBasePlayer *pthis, PLAYER_ANIM playerAnim)
// {
//     int _anim = (int)playerAnim;

//     if (g_L) 
//     {
//         lua_getglobal(g_L, "ReGame_CBasePlayer_SetAnimation");
//         if (lua_isfunction(g_L, -1)) 
//         {
//             edict_t* pEnt = pthis ? ENT(((CCSPlayer*)pthis)->pev) : NULL;
//             lua_pushentity(g_L, pEnt);

//             lua_pushinteger(g_L, _anim);

//             if (lua_pcall(g_L, 2, LUA_MULTRET, 0) == 0)
//             {
//                 int nRet = lua_gettop(g_L);
//                 if (nRet == 1)
//                 {
//                     if (lua_isboolean(g_L, -1) && lua_toboolean(g_L, -1)) {
//                         lua_pop(g_L, 1);
//                         return;
//                     }
//                     if (lua_isnumber(g_L, -1)) {
//                         _anim = (int)lua_tointeger(g_L, -1);
//                     }
//                     lua_pop(g_L, 1);
//                 }
//                 else lua_pop(g_L, nRet);
//             }
//             else lua_pop(g_L, 1);
//         }
//         else lua_pop(g_L, 1);
//     }
//     chain->callNext(pthis, (PLAYER_ANIM)_anim);
// }

// /* 15. CBasePlayer::GiveDefaultItems */
// void CBasePlayer_GiveDefaultItems(IReGameHook_CBasePlayer_GiveDefaultItems *chain, CBasePlayer *pthis)
// {
//     if (g_L) 
//     {
//         lua_getglobal(g_L, "ReGame_CBasePlayer_GiveDefaultItems");
//         if (lua_isfunction(g_L, -1)) 
//         {
//             edict_t* pEnt = pthis ? ENT(((CCSPlayer*)pthis)->pev) : NULL;
//             lua_pushentity(g_L, pEnt);

//             if (lua_pcall(g_L, 1, 1, 0) == 0)
//             {
//                 if (lua_isboolean(g_L, -1) && lua_toboolean(g_L, -1))
//                 {
//                     lua_pop(g_L, 1);
//                     return;
//                 }
//                 lua_pop(g_L, 1);
//             }
//             else lua_pop(g_L, 1);
//         }
//         else lua_pop(g_L, 1);
//     }
//     chain->callNext(pthis);
// }

// /* 16. CBasePlayer::GiveNamedItem */
// CBaseEntity *CBasePlayer_GiveNamedItem(IReGameHook_CBasePlayer_GiveNamedItem *chain, CBasePlayer *pthis, const char *pszName)
// {
//     const char* _pszName = pszName;
//     static char szNameBuf[64];

//     if (g_L) 
//     {
//         lua_getglobal(g_L, "ReGame_CBasePlayer_GiveNamedItem");
//         if (lua_isfunction(g_L, -1)) 
//         {
//             edict_t* pEnt = pthis ? ENT(((CCSPlayer*)pthis)->pev) : NULL;
//             lua_pushentity(g_L, pEnt);

//             lua_pushstring(g_L, _pszName);

//             if (lua_pcall(g_L, 2, LUA_MULTRET, 0) == 0)
//             {
//                 int nRet = lua_gettop(g_L);
//                 if (nRet == 1)
//                 {
//                     if (lua_isstring(g_L, -1))
//                     {
//                         // 只有一个 String 返回，视为修改参数
//                         strncpy(szNameBuf, lua_tostring(g_L, -1), 63);
//                         szNameBuf[63] = 0;
//                         _pszName = szNameBuf;
//                     }
//                     else if (lua_isuserdata(g_L, -1) || lua_isnil(g_L, -1))
//                     {
//                         // 返回 Entity/Nil，视为 Supercede
//                         edict_t* pRetEnt = NULL;
//                         // ... (转换逻辑同上)
//                         CBaseEntity* pRet = pRetEnt ? (CBaseEntity*)GET_PRIVATE(pRetEnt) : NULL;
//                         lua_pop(g_L, 1);
//                         return pRet;
//                     }
//                     lua_pop(g_L, 1);
//                 }
//                 else lua_pop(g_L, nRet);
//             }
//             else lua_pop(g_L, 1);
//         }
//         else lua_pop(g_L, 1);
//     }
//     return chain->callNext(pthis, _pszName);
// }

// /* 17. CBasePlayer::AddAccount */
// void CBasePlayer_AddAccount(IReGameHook_CBasePlayer_AddAccount *chain, CBasePlayer *pthis, int amount, RewardType type, bool bTrackChange)
// {
//     int _amount = amount;
//     int _type = (int)type;
//     bool _bTrackChange = bTrackChange;

//     if (g_L) 
//     {
//         lua_getglobal(g_L, "ReGame_CBasePlayer_AddAccount");
//         if (lua_isfunction(g_L, -1)) 
//         {
//             edict_t* pEnt = pthis ? ENT(((CCSPlayer*)pthis)->pev) : NULL;
//             lua_pushentity(g_L, pEnt);

//             lua_pushinteger(g_L, _amount);
//             lua_pushinteger(g_L, _type);
//             lua_pushboolean(g_L, _bTrackChange);

//             if (lua_pcall(g_L, 4, LUA_MULTRET, 0) == 0)
//             {
//                 int nRet = lua_gettop(g_L);
//                 if (nRet == 1 && lua_isboolean(g_L, -1) && lua_toboolean(g_L, -1))
//                 {
//                     lua_pop(g_L, 1);
//                     return;
//                 }
//                 else if (nRet == 3)
//                 {
//                     if (lua_isnumber(g_L, -3)) _amount = (int)lua_tointeger(g_L, -3);
//                     if (lua_isnumber(g_L, -2)) _type = (int)lua_tointeger(g_L, -2);
//                     if (lua_isboolean(g_L, -1)) _bTrackChange = (bool)lua_toboolean(g_L, -1);
//                     lua_pop(g_L, 3);
//                 }
//                 else
//                 {
//                     lua_pop(g_L, nRet);
//                 }
//             }
//             else lua_pop(g_L, 1);
//         }
//         else lua_pop(g_L, 1);
//     }
//     chain->callNext(pthis, _amount, (RewardType)_type, _bTrackChange);
// }

// /* 18. CBasePlayer::GiveShield */
// void CBasePlayer_GiveShield(IReGameHook_CBasePlayer_GiveShield *chain, CBasePlayer *pthis, bool bDeploy)
// {
//     bool _bDeploy = bDeploy;

//     if (g_L) 
//     {
//         lua_getglobal(g_L, "ReGame_CBasePlayer_GiveShield");
//         if (lua_isfunction(g_L, -1)) 
//         {
//             edict_t* pEnt = pthis ? ENT(((CCSPlayer*)pthis)->pev) : NULL;
//             lua_pushentity(g_L, pEnt);

//             lua_pushboolean(g_L, _bDeploy);

//             if (lua_pcall(g_L, 2, LUA_MULTRET, 0) == 0)
//             {
//                 int nRet = lua_gettop(g_L);
//                 if (nRet == 1)
//                 {
//                    if (lua_isboolean(g_L, -1)) {
//                        // 如果是 bool，可能是拦截 true，也可能是修改参数(bool)
//                        // 这里约定: true 拦截, false 继续(但不修改参数?), 或者只作为修改参数？
//                        // 通常 Void 函数单返回值 Bool = 拦截。多返回值 = 参数修改。
//                        // 这里只有一个 Bool 参数。
//                        // 策略：如果 Lua 返回 true，拦截。如果返回 false，继续。
//                        // 如果想修改参数，比较麻烦。这里假设返回 True 拦截。
//                        if (lua_toboolean(g_L, -1)) {
//                            lua_pop(g_L, 1);
//                            return;
//                        }
//                    }
//                    lua_pop(g_L, 1);
//                 }
//                 // 假如需要修改参数，Lua 需返回两个值 (true/false block, newDeploy)? 不，保持统一：Void 函数 True 拦截。
//                 // 如果需要修改参数，我们检查是否返回了 bool 且不拦截？
//                 // 简单起见，这里 Void 函数只支持拦截。
//                 else lua_pop(g_L, nRet);
//             }
//             else lua_pop(g_L, 1);
//         }
//         else lua_pop(g_L, 1);
//     }
//     chain->callNext(pthis, _bDeploy);
// }

// /* 19. CBasePlayer::SetClientUserInfoModel */
// void CBasePlayer_SetClientUserInfoModel(IReGameHook_CBasePlayer_SetClientUserInfoModel *chain, CBasePlayer *pthis, char *infobuffer, char *newValue)
// {
//     if (g_L) 
//     {
//         lua_getglobal(g_L, "ReGame_CBasePlayer_SetClientUserInfoModel");
//         if (lua_isfunction(g_L, -1)) 
//         {
//             edict_t* pEnt = pthis ? ENT(((CCSPlayer*)pthis)->pev) : NULL;
//             lua_pushentity(g_L, pEnt);

//             lua_pushstring(g_L, infobuffer);
//             lua_pushstring(g_L, newValue);

//             if (lua_pcall(g_L, 3, LUA_MULTRET, 0) == 0)
//             {
//                 int nRet = lua_gettop(g_L);
//                 if (nRet == 1 && lua_isboolean(g_L, -1) && lua_toboolean(g_L, -1))
//                 {
//                     lua_pop(g_L, 1);
//                     return;
//                 }
//                 else if (nRet == 2)
//                 {
//                     if (lua_isstring(g_L, -2)) strncpy(infobuffer, lua_tostring(g_L, -2), 255); // 注意 buffer 长度安全，这里假设标准长度
//                     if (lua_isstring(g_L, -1)) strncpy(newValue, lua_tostring(g_L, -1), 255);
//                     lua_pop(g_L, 2);
//                 }
//                 else
//                 {
//                     lua_pop(g_L, nRet);
//                 }
//             }
//             else lua_pop(g_L, 1);
//         }
//         else lua_pop(g_L, 1);
//     }
//     chain->callNext(pthis, infobuffer, newValue);
// }

// /* 20. CBasePlayer::SetClientUserInfoName */
// bool CBasePlayer_SetClientUserInfoName(IReGameHook_CBasePlayer_SetClientUserInfoName *chain, CBasePlayer *pthis, char *infobuffer, char *newValue)
// {
//     if (g_L) 
//     {
//         lua_getglobal(g_L, "ReGame_CBasePlayer_SetClientUserInfoName");
//         if (lua_isfunction(g_L, -1)) 
//         {
//             edict_t* pEnt = pthis ? ENT(((CCSPlayer*)pthis)->pev) : NULL;
//             lua_pushentity(g_L, pEnt);

//             lua_pushstring(g_L, infobuffer);
//             lua_pushstring(g_L, newValue);

//             if (lua_pcall(g_L, 3, LUA_MULTRET, 0) == 0)
//             {
//                 int nRet = lua_gettop(g_L);
//                 if (nRet == 1 && lua_isboolean(g_L, -1))
//                 {
//                     bool ret = (bool)lua_toboolean(g_L, -1);
//                     lua_pop(g_L, 1);
//                     return ret;
//                 }
//                 else if (nRet == 2)
//                 {
//                     if (lua_isstring(g_L, -2)) strncpy(infobuffer, lua_tostring(g_L, -2), 255);
//                     if (lua_isstring(g_L, -1)) strncpy(newValue, lua_tostring(g_L, -1), 255);
//                     lua_pop(g_L, 2);
//                 }
//                 else
//                 {
//                     lua_pop(g_L, nRet);
//                 }
//             }
//             else lua_pop(g_L, 1);
//         }
//         else lua_pop(g_L, 1);
//     }
//     return chain->callNext(pthis, infobuffer, newValue);
// }

// /* 21. CBasePlayer::HasRestrictItem */
// bool CBasePlayer_HasRestrictItem(IReGameHook_CBasePlayer_HasRestrictItem *chain, CBasePlayer *pthis, ItemID item, ItemRestType type)
// {
//     int _item = (int)item;
//     int _type = (int)type;

//     if (g_L) 
//     {
//         lua_getglobal(g_L, "ReGame_CBasePlayer_HasRestrictItem");
//         if (lua_isfunction(g_L, -1)) 
//         {
//             edict_t* pEnt = pthis ? ENT(((CCSPlayer*)pthis)->pev) : NULL;
//             lua_pushentity(g_L, pEnt);

//             lua_pushinteger(g_L, _item);
//             lua_pushinteger(g_L, _type);

//             if (lua_pcall(g_L, 3, LUA_MULTRET, 0) == 0)
//             {
//                 int nRet = lua_gettop(g_L);
//                 if (nRet == 1 && lua_isboolean(g_L, -1))
//                 {
//                     bool ret = (bool)lua_toboolean(g_L, -1);
//                     lua_pop(g_L, 1);
//                     return ret;
//                 }
//                 else if (nRet == 2)
//                 {
//                     if (lua_isnumber(g_L, -2)) _item = (int)lua_tointeger(g_L, -2);
//                     if (lua_isnumber(g_L, -1)) _type = (int)lua_tointeger(g_L, -1);
//                     lua_pop(g_L, 2);
//                 }
//                 else lua_pop(g_L, nRet);
//             }
//             else lua_pop(g_L, 1);
//         }
//         else lua_pop(g_L, 1);
//     }
//     return chain->callNext(pthis, (ItemID)_item, (ItemRestType)_type);
// }

// /* 22. CBasePlayer::DropPlayerItem */
// CBaseEntity *CBasePlayer_DropPlayerItem(IReGameHook_CBasePlayer_DropPlayerItem *chain, CBasePlayer *pthis, const char *pszItemName)
// {
//     const char* _pszItemName = pszItemName;
//     static char szBuf[64];

//     if (g_L) 
//     {
//         lua_getglobal(g_L, "ReGame_CBasePlayer_DropPlayerItem");
//         if (lua_isfunction(g_L, -1)) 
//         {
//             edict_t* pEnt = pthis ? ENT(((CCSPlayer*)pthis)->pev) : NULL;
//             lua_pushentity(g_L, pEnt);

//             lua_pushstring(g_L, _pszItemName);

//             if (lua_pcall(g_L, 2, LUA_MULTRET, 0) == 0)
//             {
//                 int nRet = lua_gettop(g_L);
//                 if (nRet == 1)
//                 {
//                     if (lua_isstring(g_L, -1)) {
//                         strncpy(szBuf, lua_tostring(g_L, -1), 63);
//                         szBuf[63] = 0;
//                         _pszItemName = szBuf;
//                     }
//                     else if (lua_isuserdata(g_L, -1) || lua_isnil(g_L, -1)) {
//                          // 返回 Entity/Nil 覆盖返回值
//                          // ... (转换逻辑)
//                          lua_pop(g_L, 1);
//                          return NULL; // 简化处理，若需支持返回 Entity 请自行添加转换
//                     }
//                     lua_pop(g_L, 1);
//                 }
//                 else lua_pop(g_L, nRet);
//             }
//             else lua_pop(g_L, 1);
//         }
//         else lua_pop(g_L, 1);
//     }
//     return chain->callNext(pthis, _pszItemName);
// }

// /* 23. CBasePlayer::DropShield */
// CBaseEntity *CBasePlayer_DropShield(IReGameHook_CBasePlayer_DropShield *chain, CBasePlayer *pthis, bool bDeploy)
// {
//     bool _bDeploy = bDeploy;

//     if (g_L) 
//     {
//         lua_getglobal(g_L, "ReGame_CBasePlayer_DropShield");
//         if (lua_isfunction(g_L, -1)) 
//         {
//             edict_t* pEnt = pthis ? ENT(((CCSPlayer*)pthis)->pev) : NULL;
//             lua_pushentity(g_L, pEnt);

//             lua_pushboolean(g_L, _bDeploy);

//             if (lua_pcall(g_L, 2, LUA_MULTRET, 0) == 0)
//             {
//                 int nRet = lua_gettop(g_L);
//                 if (nRet == 1)
//                 {
//                     if (lua_isboolean(g_L, -1)) _bDeploy = (bool)lua_toboolean(g_L, -1); // 只有一个 bool，视为修改参数？还是拦截？
//                     // 歧义：Returns CBaseEntity*. 
//                     // 如果返回 bool，无法转换为 Entity。所以这里只能是修改参数。
//                     // 除非返回 Userdata/Nil 才是拦截。
//                     else if (lua_isnil(g_L, -1)) { lua_pop(g_L, 1); return NULL; }
//                     lua_pop(g_L, 1);
//                 }
//                 else lua_pop(g_L, nRet);
//             }
//             else lua_pop(g_L, 1);
//         }
//         else lua_pop(g_L, 1);
//     }
//     return chain->callNext(pthis, _bDeploy);
// }

// /* 24. CBasePlayer::OnSpawnEquip */
// void CBasePlayer_OnSpawnEquip(IReGameHook_CBasePlayer_OnSpawnEquip *chain, CBasePlayer *pthis, bool bAddDefault, bool bEquipGame)
// {
//     bool _bAddDefault = bAddDefault;
//     bool _bEquipGame = bEquipGame;

//     if (g_L) 
//     {
//         lua_getglobal(g_L, "ReGame_CBasePlayer_OnSpawnEquip");
//         if (lua_isfunction(g_L, -1)) 
//         {
//             edict_t* pEnt = pthis ? ENT(((CCSPlayer*)pthis)->pev) : NULL;
//             lua_pushentity(g_L, pEnt);

//             lua_pushboolean(g_L, _bAddDefault);
//             lua_pushboolean(g_L, _bEquipGame);

//             if (lua_pcall(g_L, 3, LUA_MULTRET, 0) == 0)
//             {
//                 int nRet = lua_gettop(g_L);
//                 if (nRet == 1 && lua_isboolean(g_L, -1) && lua_toboolean(g_L, -1)) {
//                     lua_pop(g_L, 1);
//                     return;
//                 }
//                 else if (nRet == 2) {
//                     if (lua_isboolean(g_L, -2)) _bAddDefault = (bool)lua_toboolean(g_L, -2);
//                     if (lua_isboolean(g_L, -1)) _bEquipGame = (bool)lua_toboolean(g_L, -1);
//                     lua_pop(g_L, 2);
//                 }
//                 else lua_pop(g_L, nRet);
//             }
//             else lua_pop(g_L, 1);
//         }
//         else lua_pop(g_L, 1);
//     }
//     chain->callNext(pthis, _bAddDefault, _bEquipGame);
// }

// /* 25. CBasePlayer::Radio */
// void CBasePlayer_Radio(IReGameHook_CBasePlayer_Radio *chain, CBasePlayer *pthis, const char *pszRadioName, const char *pszRadioMessage, short iPitch, bool bShowIcon)
// {
//     const char* _pszName = pszRadioName;
//     const char* _pszMsg = pszRadioMessage;
//     int _iPitch = iPitch;
//     bool _bShowIcon = bShowIcon;
//     static char szNameBuf[64];
//     static char szMsgBuf[64];

//     if (g_L) 
//     {
//         lua_getglobal(g_L, "ReGame_CBasePlayer_Radio");
//         if (lua_isfunction(g_L, -1)) 
//         {
//             edict_t* pEnt = pthis ? ENT(((CCSPlayer*)pthis)->pev) : NULL;
//             lua_pushentity(g_L, pEnt);

//             lua_pushstring(g_L, _pszName);
//             lua_pushstring(g_L, _pszMsg);
//             lua_pushinteger(g_L, _iPitch);
//             lua_pushboolean(g_L, _bShowIcon);

//             if (lua_pcall(g_L, 5, LUA_MULTRET, 0) == 0)
//             {
//                 int nRet = lua_gettop(g_L);
//                 if (nRet == 1 && lua_isboolean(g_L, -1) && lua_toboolean(g_L, -1)) {
//                     lua_pop(g_L, 1);
//                     return;
//                 }
//                 else if (nRet == 4) {
//                     if (lua_isstring(g_L, -4)) { strncpy(szNameBuf, lua_tostring(g_L, -4), 63); _pszName = szNameBuf; }
//                     if (lua_isstring(g_L, -3)) { strncpy(szMsgBuf, lua_tostring(g_L, -3), 63); _pszMsg = szMsgBuf; }
//                     if (lua_isnumber(g_L, -2)) _iPitch = (short)lua_tointeger(g_L, -2);
//                     if (lua_isboolean(g_L, -1)) _bShowIcon = (bool)lua_toboolean(g_L, -1);
//                     lua_pop(g_L, 4);
//                 }
//                 else lua_pop(g_L, nRet);
//             }
//             else lua_pop(g_L, 1);
//         }
//         else lua_pop(g_L, 1);
//     }
//     chain->callNext(pthis, _pszName, _pszMsg, _iPitch, _bShowIcon);
// }

// /* 26. CBasePlayer::Disappear */
// void CBasePlayer_Disappear(IReGameHook_CBasePlayer_Disappear *chain, CBasePlayer *pthis)
// {
//     if (g_L) 
//     {
//         lua_getglobal(g_L, "ReGame_CBasePlayer_Disappear");
//         if (lua_isfunction(g_L, -1)) 
//         {
//             edict_t* pEnt = pthis ? ENT(((CCSPlayer*)pthis)->pev) : NULL;
//             lua_pushentity(g_L, pEnt);

//             if (lua_pcall(g_L, 1, 1, 0) == 0)
//             {
//                 if (lua_isboolean(g_L, -1) && lua_toboolean(g_L, -1)) { lua_pop(g_L, 1); return; }
//                 lua_pop(g_L, 1);
//             }
//             else lua_pop(g_L, 1);
//         }
//         else lua_pop(g_L, 1);
//     }
//     chain->callNext(pthis);
// }

// /* 27. CBasePlayer::MakeVIP */
// void CBasePlayer_MakeVIP(IReGameHook_CBasePlayer_MakeVIP *chain, CBasePlayer *pthis)
// {
//     if (g_L) 
//     {
//         lua_getglobal(g_L, "ReGame_CBasePlayer_MakeVIP");
//         if (lua_isfunction(g_L, -1)) 
//         {
//             edict_t* pEnt = pthis ? ENT(((CCSPlayer*)pthis)->pev) : NULL;
//             lua_pushentity(g_L, pEnt);

//             if (lua_pcall(g_L, 1, 1, 0) == 0)
//             {
//                 if (lua_isboolean(g_L, -1) && lua_toboolean(g_L, -1)) { lua_pop(g_L, 1); return; }
//                 lua_pop(g_L, 1);
//             }
//             else lua_pop(g_L, 1);
//         }
//         else lua_pop(g_L, 1);
//     }
//     chain->callNext(pthis);
// }

// /* 28. CBasePlayer::MakeBomber */
// bool CBasePlayer_MakeBomber(IReGameHook_CBasePlayer_MakeBomber *chain, CBasePlayer *pthis)
// {
//     if (g_L) 
//     {
//         lua_getglobal(g_L, "ReGame_CBasePlayer_MakeBomber");
//         if (lua_isfunction(g_L, -1)) 
//         {
//             edict_t* pEnt = pthis ? ENT(((CCSPlayer*)pthis)->pev) : NULL;
//             lua_pushentity(g_L, pEnt);

//             if (lua_pcall(g_L, 1, 1, 0) == 0)
//             {
//                 if (lua_isboolean(g_L, -1))
//                 {
//                     bool ret = (bool)lua_toboolean(g_L, -1);
//                     lua_pop(g_L, 1);
//                     return ret;
//                 }
//                 lua_pop(g_L, 1);
//             }
//             else lua_pop(g_L, 1);
//         }
//         else lua_pop(g_L, 1);
//     }
//     return chain->callNext(pthis);
// }

// /* 29. CBasePlayer::StartObserver */
// void CBasePlayer_StartObserver(IReGameHook_CBasePlayer_StartObserver *chain, CBasePlayer *pthis, Vector &vecPosition, Vector &vecViewAngle)
// {
//     // 向量通过指针传递，Lua 难以直接返回 2 个 Vector 对象来修改它们。
//     // 这里仅支持拦截 (Block)。
//     if (g_L) 
//     {
//         lua_getglobal(g_L, "ReGame_CBasePlayer_StartObserver");
//         if (lua_isfunction(g_L, -1)) 
//         {
//             edict_t* pEnt = pthis ? ENT(((CCSPlayer*)pthis)->pev) : NULL;
//             lua_pushentity(g_L, pEnt);

//             lua_pushlightuserdata(g_L, &vecPosition);
//             lua_pushlightuserdata(g_L, &vecViewAngle);

//             if (lua_pcall(g_L, 3, 1, 0) == 0)
//             {
//                 if (lua_isboolean(g_L, -1) && lua_toboolean(g_L, -1)) { lua_pop(g_L, 1); return; }
//                 lua_pop(g_L, 1);
//             }
//             else lua_pop(g_L, 1);
//         }
//         else lua_pop(g_L, 1);
//     }
//     chain->callNext(pthis, vecPosition, vecViewAngle);
// }

// /* 30. CBasePlayer::GetIntoGame */
// bool CBasePlayer_GetIntoGame(IReGameHook_CBasePlayer_GetIntoGame *chain, CBasePlayer *pthis)
// {
//     if (g_L) 
//     {
//         lua_getglobal(g_L, "ReGame_CBasePlayer_GetIntoGame");
//         if (lua_isfunction(g_L, -1)) 
//         {
//             edict_t* pEnt = pthis ? ENT(((CCSPlayer*)pthis)->pev) : NULL;
//             lua_pushentity(g_L, pEnt);

//             if (lua_pcall(g_L, 1, 1, 0) == 0)
//             {
//                 if (lua_isboolean(g_L, -1)) {
//                     bool ret = (bool)lua_toboolean(g_L, -1);
//                     lua_pop(g_L, 1);
//                     return ret;
//                 }
//                 lua_pop(g_L, 1);
//             }
//             else lua_pop(g_L, 1);
//         }
//         else lua_pop(g_L, 1);
//     }
//     return chain->callNext(pthis);
// }
// /* 31. CBaseAnimating::ResetSequenceInfo */
// void CBaseAnimating_ResetSequenceInfo(IReGameHook_CBaseAnimating_ResetSequenceInfo *chain, CBaseAnimating *pthis)
// {
//     if (g_L) 
//     {
//         lua_getglobal(g_L, "ReGame_CBaseAnimating_ResetSequenceInfo");
//         if (lua_isfunction(g_L, -1)) 
//         {
//             edict_t* pEnt = pthis ? ENT(((CCSAnimating*)pthis)->pev) : NULL;
//             lua_pushentity(g_L, pEnt);

//             if (lua_pcall(g_L, 1, 1, 0) == 0)
//             {
//                 if (lua_isboolean(g_L, -1) && lua_toboolean(g_L, -1)) { lua_pop(g_L, 1); return; }
//                 lua_pop(g_L, 1);
//             }
//             else lua_pop(g_L, 1);
//         }
//         else lua_pop(g_L, 1);
//     }
//     chain->callNext(pthis);
// }

// /* 32. GetForceCamera */
// int GetForceCamera(IReGameHook_GetForceCamera *chain, CBasePlayer *pPlayer)
// {
//     if (g_L) 
//     {
//         lua_getglobal(g_L, "ReGame_GetForceCamera");
//         if (lua_isfunction(g_L, -1)) 
//         {
//             edict_t* pEnt = pPlayer ? ENT(((CCSPlayer*)pPlayer)->pev) : NULL;
//             lua_pushentity(g_L, pEnt);

//             if (lua_pcall(g_L, 1, 1, 0) == 0)
//             {
//                 if (lua_isnumber(g_L, -1))
//                 {
//                     int ret = (int)lua_tointeger(g_L, -1);
//                     lua_pop(g_L, 1);
//                     return ret;
//                 }
//                 lua_pop(g_L, 1);
//             }
//             else lua_pop(g_L, 1);
//         }
//         else lua_pop(g_L, 1);
//     }
//     return chain->callNext(pPlayer);
// }

// /* 33. PlayerBlind */
// void PlayerBlind(IReGameHook_PlayerBlind *chain, CBasePlayer *pPlayer, entvars_t *pevInflictor, entvars_t *pevAttacker, float fadeTime, float fadeHold, int alpha, Vector &color)
// {
//     float _fadeTime = fadeTime;
//     float _fadeHold = fadeHold;
//     int _alpha = alpha;

//     if (g_L) 
//     {
//         lua_getglobal(g_L, "ReGame_PlayerBlind");
//         if (lua_isfunction(g_L, -1)) 
//         {
//             edict_t* pEnt = pPlayer ? ENT(((CCSPlayer*)pPlayer)->pev) : NULL;
//             lua_pushentity(g_L, pEnt);

//             edict_t* pInflictor = pevInflictor ? ENT(pevInflictor) : NULL;
//             lua_pushentity(g_L, pInflictor);

//             edict_t* pAttacker = pevAttacker ? ENT(pevAttacker) : NULL;
//             lua_pushentity(g_L, pAttacker);

//             lua_pushnumber(g_L, _fadeTime);
//             lua_pushnumber(g_L, _fadeHold);
//             lua_pushinteger(g_L, _alpha);
//             lua_pushlightuserdata(g_L, &color);

//             // 期望返回: true(block) 或 newTime, newHold, newAlpha
//             if (lua_pcall(g_L, 7, LUA_MULTRET, 0) == 0)
//             {
//                 int nRet = lua_gettop(g_L);
//                 if (nRet == 1 && lua_isboolean(g_L, -1) && lua_toboolean(g_L, -1)) {
//                     lua_pop(g_L, 1);
//                     return;
//                 }
//                 else if (nRet == 3) {
//                     if (lua_isnumber(g_L, -3)) _fadeTime = (float)lua_tonumber(g_L, -3);
//                     if (lua_isnumber(g_L, -2)) _fadeHold = (float)lua_tonumber(g_L, -2);
//                     if (lua_isnumber(g_L, -1)) _alpha = (int)lua_tointeger(g_L, -1);
//                     lua_pop(g_L, 3);
//                 }
//                 else lua_pop(g_L, nRet);
//             }
//             else lua_pop(g_L, 1);
//         }
//         else lua_pop(g_L, 1);
//     }
//     chain->callNext(pPlayer, pevInflictor, pevAttacker, _fadeTime, _fadeHold, _alpha, color);
// }

// /* 34. RadiusFlash_TraceLine */
// void RadiusFlash_TraceLine(IReGameHook_RadiusFlash_TraceLine *chain, CBasePlayer *pPlayer, entvars_t *pevInflictor, entvars_t *pevAttacker, Vector &vecSrc, Vector &vecSpot, TraceResult *ptr)
// {
//     if (g_L) 
//     {
//         lua_getglobal(g_L, "ReGame_RadiusFlash_TraceLine");
//         if (lua_isfunction(g_L, -1)) 
//         {
//             edict_t* pEnt = pPlayer ? ENT(((CCSPlayer*)pPlayer)->pev) : NULL;
//             lua_pushentity(g_L, pEnt);

//             edict_t* pInflictor = pevInflictor ? ENT(pevInflictor) : NULL;
//             lua_pushentity(g_L, pInflictor);

//             edict_t* pAttacker = pevAttacker ? ENT(pevAttacker) : NULL;
//             lua_pushentity(g_L, pAttacker);

//             lua_pushlightuserdata(g_L, &vecSrc);
//             lua_pushlightuserdata(g_L, &vecSpot);
//             lua_pushlightuserdata(g_L, ptr);

//             if (lua_pcall(g_L, 6, 1, 0) == 0)
//             {
//                 if (lua_isboolean(g_L, -1) && lua_toboolean(g_L, -1)) { lua_pop(g_L, 1); return; }
//                 lua_pop(g_L, 1);
//             }
//             else lua_pop(g_L, 1);
//         }
//         else lua_pop(g_L, 1);
//     }
//     chain->callNext(pPlayer, pevInflictor, pevAttacker, vecSrc, vecSpot, ptr);
// }

// /* 35. RoundEnd */
// bool RoundEnd(IReGameHook_RoundEnd *chain, int winStatus, ScenarioEventEndRound event, float tmDelay)
// {
//     int _winStatus = winStatus;
//     int _event = (int)event;
//     float _tmDelay = tmDelay;

//     if (g_L) 
//     {
//         lua_getglobal(g_L, "ReGame_RoundEnd");
//         if (lua_isfunction(g_L, -1)) 
//         {
//             lua_pushinteger(g_L, _winStatus);
//             lua_pushinteger(g_L, _event);
//             lua_pushnumber(g_L, _tmDelay);

//             if (lua_pcall(g_L, 3, LUA_MULTRET, 0) == 0)
//             {
//                 int nRet = lua_gettop(g_L);
//                 if (nRet == 1 && lua_isboolean(g_L, -1)) {
//                     bool ret = (bool)lua_toboolean(g_L, -1);
//                     lua_pop(g_L, 1);
//                     return ret;
//                 }
//                 else if (nRet == 3) {
//                     if (lua_isnumber(g_L, -3)) _winStatus = (int)lua_tointeger(g_L, -3);
//                     if (lua_isnumber(g_L, -2)) _event = (int)lua_tointeger(g_L, -2);
//                     if (lua_isnumber(g_L, -1)) _tmDelay = (float)lua_tonumber(g_L, -1);
//                     lua_pop(g_L, 3);
//                 }
//                 else lua_pop(g_L, nRet);
//             }
//             else lua_pop(g_L, 1);
//         }
//         else lua_pop(g_L, 1);
//     }
//     return chain->callNext(_winStatus, (ScenarioEventEndRound)_event, _tmDelay);
// }

// /* 36. InstallGameRules */
// CGameRules *InstallGameRules(IReGameHook_InstallGameRules *chain)
// {
//     if (g_L) 
//     {
//         lua_getglobal(g_L, "ReGame_InstallGameRules");
//         if (lua_isfunction(g_L, -1)) 
//         {
//             if (lua_pcall(g_L, 0, 1, 0) == 0)
//             {
//                 if (lua_isuserdata(g_L, -1)) {
//                     // 如果 Lua 返回了指针，尝试直接返回
//                     // CGameRules* ret = (CGameRules*)lua_touserdata(g_L, -1);
//                     // lua_pop(g_L, 1);
//                     // return ret;
//                     // 安全起见，InstallGameRules 极少被完全替换，这里仅做演示
//                 }
//                 lua_pop(g_L, 1);
//             }
//             else lua_pop(g_L, 1);
//         }
//         else lua_pop(g_L, 1);
//     }
//     return chain->callNext();
// }

// /* 37. PM_Init */
// void PM_Init(IReGameHook_PM_Init *chain, struct playermove_s *ppmove)
// {
//     if (g_L) 
//     {
//         lua_getglobal(g_L, "ReGame_PM_Init");
//         if (lua_isfunction(g_L, -1)) 
//         {
//             lua_pushlightuserdata(g_L, ppmove);

//             if (lua_pcall(g_L, 1, 1, 0) == 0)
//             {
//                 if (lua_isboolean(g_L, -1) && lua_toboolean(g_L, -1)) { lua_pop(g_L, 1); return; }
//                 lua_pop(g_L, 1);
//             }
//             else lua_pop(g_L, 1);
//         }
//         else lua_pop(g_L, 1);
//     }
//     chain->callNext(ppmove);
// }

// /* 38. PM_Move */
// void PM_Move(IReGameHook_PM_Move *chain, struct playermove_s *ppmove, int server)
// {
//     if (g_L) 
//     {
//         lua_getglobal(g_L, "ReGame_PM_Move");
//         if (lua_isfunction(g_L, -1)) 
//         {
//             lua_pushlightuserdata(g_L, ppmove);
//             lua_pushinteger(g_L, server);

//             if (lua_pcall(g_L, 2, 1, 0) == 0)
//             {
//                 if (lua_isboolean(g_L, -1) && lua_toboolean(g_L, -1)) { lua_pop(g_L, 1); return; }
//                 lua_pop(g_L, 1);
//             }
//             else lua_pop(g_L, 1);
//         }
//         else lua_pop(g_L, 1);
//     }
//     chain->callNext(ppmove, server);
// }

// /* 39. PM_AirMove */
// void PM_AirMove(IReGameHook_PM_AirMove *chain, int server)
// {
//     if (g_L) 
//     {
//         lua_getglobal(g_L, "ReGame_PM_AirMove");
//         if (lua_isfunction(g_L, -1)) 
//         {
//             lua_pushinteger(g_L, server);

//             if (lua_pcall(g_L, 1, 1, 0) == 0)
//             {
//                 if (lua_isboolean(g_L, -1) && lua_toboolean(g_L, -1)) { lua_pop(g_L, 1); return; }
//                 lua_pop(g_L, 1);
//             }
//             else lua_pop(g_L, 1);
//         }
//         else lua_pop(g_L, 1);
//     }
//     chain->callNext(server);
// }

// /* 40. HandleMenu_ChooseAppearance */
// void HandleMenu_ChooseAppearance(IReGameHook_HandleMenu_ChooseAppearance *chain, CBasePlayer *pPlayer, int slot)
// {
//     int _slot = slot;

//     if (g_L) 
//     {
//         lua_getglobal(g_L, "ReGame_HandleMenu_ChooseAppearance");
//         if (lua_isfunction(g_L, -1)) 
//         {
//             edict_t* pEnt = pPlayer ? ENT(((CCSPlayer*)pPlayer)->pev) : NULL;
//             lua_pushentity(g_L, pEnt);

//             lua_pushinteger(g_L, _slot);

//             if (lua_pcall(g_L, 2, LUA_MULTRET, 0) == 0)
//             {
//                 int nRet = lua_gettop(g_L);
//                 if (nRet == 1) {
//                     if (lua_isboolean(g_L, -1) && lua_toboolean(g_L, -1)) { lua_pop(g_L, 1); return; }
//                     if (lua_isnumber(g_L, -1)) _slot = (int)lua_tointeger(g_L, -1);
//                     lua_pop(g_L, 1);
//                 }
//                 else lua_pop(g_L, nRet);
//             }
//             else lua_pop(g_L, 1);
//         }
//         else lua_pop(g_L, 1);
//     }
//     chain->callNext(pPlayer, _slot);
// }

// /* 41. HandleMenu_ChooseTeam */
// BOOL HandleMenu_ChooseTeam(IReGameHook_HandleMenu_ChooseTeam *chain, CBasePlayer *pPlayer, int slot)
// {
//     int _slot = slot;

//     if (g_L) 
//     {
//         lua_getglobal(g_L, "ReGame_HandleMenu_ChooseTeam");
//         if (lua_isfunction(g_L, -1)) 
//         {
//             edict_t* pEnt = pPlayer ? ENT(((CCSPlayer*)pPlayer)->pev) : NULL;
//             lua_pushentity(g_L, pEnt);

//             lua_pushinteger(g_L, _slot);

//             if (lua_pcall(g_L, 2, LUA_MULTRET, 0) == 0)
//             {
//                 int nRet = lua_gettop(g_L);
//                 if (nRet == 1) {
//                     if (lua_isboolean(g_L, -1)) {
//                         BOOL ret = (BOOL)lua_toboolean(g_L, -1);
//                         lua_pop(g_L, 1);
//                         return ret;
//                     }
//                     if (lua_isnumber(g_L, -1)) _slot = (int)lua_tointeger(g_L, -1);
//                     lua_pop(g_L, 1);
//                 }
//                 else lua_pop(g_L, nRet);
//             }
//             else lua_pop(g_L, 1);
//         }
//         else lua_pop(g_L, 1);
//     }
//     return chain->callNext(pPlayer, _slot);
// }

// /* 42. ShowMenu */
// void ShowMenu(IReGameHook_ShowMenu *chain, CBasePlayer *pPlayer, int slots, int displaytime, BOOL needmore, char *pszText)
// {
//     int _slots = slots;
//     int _displaytime = displaytime;
//     BOOL _needmore = needmore;
//     static char szTextBuf[512];

//     if (g_L) 
//     {
//         lua_getglobal(g_L, "ReGame_ShowMenu");
//         if (lua_isfunction(g_L, -1)) 
//         {
//             edict_t* pEnt = pPlayer ? ENT(((CCSPlayer*)pPlayer)->pev) : NULL;
//             lua_pushentity(g_L, pEnt);

//             lua_pushinteger(g_L, _slots);
//             lua_pushinteger(g_L, _displaytime);
//             lua_pushboolean(g_L, _needmore);
//             lua_pushstring(g_L, pszText);

//             if (lua_pcall(g_L, 5, LUA_MULTRET, 0) == 0)
//             {
//                 int nRet = lua_gettop(g_L);
//                 if (nRet == 1 && lua_isboolean(g_L, -1) && lua_toboolean(g_L, -1)) {
//                     lua_pop(g_L, 1);
//                     return;
//                 }
//                 else if (nRet == 4) {
//                     if (lua_isnumber(g_L, -4)) _slots = (int)lua_tointeger(g_L, -4);
//                     if (lua_isnumber(g_L, -3)) _displaytime = (int)lua_tointeger(g_L, -3);
//                     if (lua_isboolean(g_L, -2)) _needmore = (BOOL)lua_toboolean(g_L, -2);
//                     if (lua_isstring(g_L, -1)) {
//                         strncpy(szTextBuf, lua_tostring(g_L, -1), 511);
//                         szTextBuf[511] = 0;
//                         if (pszText) strcpy(pszText, szTextBuf); // 注意：修改原指针内容需确保安全，通常 ShowMenu 的 pszText 是只读的，这里尽量小心
//                     }
//                     lua_pop(g_L, 4);
//                 }
//                 else lua_pop(g_L, nRet);
//             }
//             else lua_pop(g_L, 1);
//         }
//         else lua_pop(g_L, 1);
//     }
//     chain->callNext(pPlayer, _slots, _displaytime, _needmore, pszText);
// }

// /* 43. ShowVGUIMenu */
// void ShowVGUIMenu(IReGameHook_ShowVGUIMenu *chain, CBasePlayer *pPlayer, int menuType, int slots, char *pszOldMenu)
// {
//     int _menuType = menuType;
//     int _slots = slots;
//     static char szOldMenuBuf[64];

//     if (g_L) 
//     {
//         lua_getglobal(g_L, "ReGame_ShowVGUIMenu");
//         if (lua_isfunction(g_L, -1)) 
//         {
//             edict_t* pEnt = pPlayer ? ENT(((CCSPlayer*)pPlayer)->pev) : NULL;
//             lua_pushentity(g_L, pEnt);

//             lua_pushinteger(g_L, _menuType);
//             lua_pushinteger(g_L, _slots);
//             lua_pushstring(g_L, pszOldMenu);

//             if (lua_pcall(g_L, 4, LUA_MULTRET, 0) == 0)
//             {
//                 int nRet = lua_gettop(g_L);
//                 if (nRet == 1 && lua_isboolean(g_L, -1) && lua_toboolean(g_L, -1)) {
//                     lua_pop(g_L, 1);
//                     return;
//                 }
//                 else if (nRet == 3) {
//                     if (lua_isnumber(g_L, -3)) _menuType = (int)lua_tointeger(g_L, -3);
//                     if (lua_isnumber(g_L, -2)) _slots = (int)lua_tointeger(g_L, -2);
//                     if (lua_isstring(g_L, -1)) {
//                         strncpy(szOldMenuBuf, lua_tostring(g_L, -1), 63);
//                         szOldMenuBuf[63] = 0;
//                         if (pszOldMenu) strcpy(pszOldMenu, szOldMenuBuf);
//                     }
//                     lua_pop(g_L, 3);
//                 }
//                 else lua_pop(g_L, nRet);
//             }
//             else lua_pop(g_L, 1);
//         }
//         else lua_pop(g_L, 1);
//     }
//     chain->callNext(pPlayer, _menuType, _slots, pszOldMenu);
// }

// /* 44. BuyGunAmmo */
// bool BuyGunAmmo(IReGameHook_BuyGunAmmo *chain, CBasePlayer *pPlayer, CBasePlayerItem *pItem, bool bIsPrimary)
// {
//     bool _bIsPrimary = bIsPrimary;

//     if (g_L) 
//     {
//         lua_getglobal(g_L, "ReGame_BuyGunAmmo");
//         if (lua_isfunction(g_L, -1)) 
//         {
//             edict_t* pEnt = pPlayer ? ENT(((CCSPlayer*)pPlayer)->pev) : NULL;
//             lua_pushentity(g_L, pEnt);

//             edict_t* pItemEnt = pItem ? ENT(((CCSPlayerItem*)pItem)->pev) : NULL;
//             lua_pushentity(g_L, pItemEnt);

//             lua_pushboolean(g_L, _bIsPrimary);

//             if (lua_pcall(g_L, 3, LUA_MULTRET, 0) == 0)
//             {
//                 int nRet = lua_gettop(g_L);
//                 if (nRet == 1) {
//                     if (lua_isboolean(g_L, -1)) {
//                         bool ret = (bool)lua_toboolean(g_L, -1);
//                         lua_pop(g_L, 1);
//                         return ret;
//                     }
//                     lua_pop(g_L, 1);
//                 }
//                 else if (nRet == 2) {
//                     // Update args? Ptr can't be updated simply. Just update bool.
//                     if (lua_isboolean(g_L, -1)) _bIsPrimary = (bool)lua_toboolean(g_L, -1);
//                     lua_pop(g_L, 2);
//                 }
//                 else lua_pop(g_L, nRet);
//             }
//             else lua_pop(g_L, 1);
//         }
//         else lua_pop(g_L, 1);
//     }
//     return chain->callNext(pPlayer, pItem, _bIsPrimary);
// }

// /* 45. BuyWeaponByWeaponID */
// CBaseEntity *BuyWeaponByWeaponID(IReGameHook_BuyWeaponByWeaponID *chain, CBasePlayer *pPlayer, WeaponIdType weaponID)
// {
//     int _weaponID = (int)weaponID;

//     if (g_L) 
//     {
//         lua_getglobal(g_L, "ReGame_BuyWeaponByWeaponID");
//         if (lua_isfunction(g_L, -1)) 
//         {
//             edict_t* pEnt = pPlayer ? ENT(((CCSPlayer*)pPlayer)->pev) : NULL;
//             lua_pushentity(g_L, pEnt);

//             lua_pushinteger(g_L, _weaponID);

//             if (lua_pcall(g_L, 2, LUA_MULTRET, 0) == 0)
//             {
//                 int nRet = lua_gettop(g_L);
//                 if (nRet == 1) {
//                     if (lua_isuserdata(g_L, -1) || lua_isnil(g_L, -1)) {
//                         // Return Entity/Nil -> Supercede
//                         // ... Conversion ...
//                         lua_pop(g_L, 1);
//                         return NULL; 
//                     }
//                     if (lua_isnumber(g_L, -1)) _weaponID = (int)lua_tointeger(g_L, -1);
//                     lua_pop(g_L, 1);
//                 }
//                 else lua_pop(g_L, nRet);
//             }
//             else lua_pop(g_L, 1);
//         }
//         else lua_pop(g_L, 1);
//     }
//     return chain->callNext(pPlayer, (WeaponIdType)_weaponID);
// }

// /* 46. InternalCommand */
// void InternalCommand(IReGameHook_InternalCommand *chain, edict_t *pEdict, const char *szCmd, const char *szVal)
// {
//     const char* _szCmd = szCmd;
//     const char* _szVal = szVal;
//     static char szCmdBuf[64];
//     static char szValBuf[64];

//     if (g_L) 
//     {
//         lua_getglobal(g_L, "ReGame_InternalCommand");
//         if (lua_isfunction(g_L, -1)) 
//         {
//             lua_pushentity(g_L, pEdict);
//             lua_pushstring(g_L, _szCmd);
//             lua_pushstring(g_L, _szVal);

//             if (lua_pcall(g_L, 3, LUA_MULTRET, 0) == 0)
//             {
//                 int nRet = lua_gettop(g_L);
//                 if (nRet == 1 && lua_isboolean(g_L, -1) && lua_toboolean(g_L, -1)) {
//                     lua_pop(g_L, 1);
//                     return;
//                 }
//                 else if (nRet == 2) {
//                     if (lua_isstring(g_L, -2)) { strncpy(szCmdBuf, lua_tostring(g_L, -2), 63); szCmdBuf[63]=0; _szCmd = szCmdBuf; }
//                     if (lua_isstring(g_L, -1)) { strncpy(szValBuf, lua_tostring(g_L, -1), 63); szValBuf[63]=0; _szVal = szValBuf; }
//                     lua_pop(g_L, 2);
//                 }
//                 else lua_pop(g_L, nRet);
//             }
//             else lua_pop(g_L, 1);
//         }
//         else lua_pop(g_L, 1);
//     }
//     chain->callNext(pEdict, _szCmd, _szVal);
// }

// /* 47. CSGameRules_FShouldSwitchWeapon */
// BOOL CSGameRules_FShouldSwitchWeapon(IReGameHook_CSGameRules_FShouldSwitchWeapon *chain, CBasePlayer *pPlayer, CBasePlayerItem *pWeapon)
// {
//     if (g_L) 
//     {
//         lua_getglobal(g_L, "ReGame_CSGameRules_FShouldSwitchWeapon");
//         if (lua_isfunction(g_L, -1)) 
//         {
//             edict_t* pEnt = pPlayer ? ENT(((CCSPlayer*)pPlayer)->pev) : NULL;
//             lua_pushentity(g_L, pEnt);

//             edict_t* pWeaponEnt = pWeapon ? ENT(((CCSPlayerItem*)pWeapon)->pev) : NULL;
//             lua_pushentity(g_L, pWeaponEnt);

//             if (lua_pcall(g_L, 2, 1, 0) == 0)
//             {
//                 if (lua_isboolean(g_L, -1) || lua_isnumber(g_L, -1)) {
//                     BOOL ret = (BOOL)lua_tointeger(g_L, -1);
//                     lua_pop(g_L, 1);
//                     return ret;
//                 }
//                 lua_pop(g_L, 1);
//             }
//             else lua_pop(g_L, 1);
//         }
//         else lua_pop(g_L, 1);
//     }
//     return chain->callNext(pPlayer, pWeapon);
// }

// /* 48. CSGameRules_GetNextBestWeapon */
// BOOL CSGameRules_GetNextBestWeapon(IReGameHook_CSGameRules_GetNextBestWeapon *chain, CBasePlayer *pPlayer, CBasePlayerItem *pWeapon)
// {
//     if (g_L) 
//     {
//         lua_getglobal(g_L, "ReGame_CSGameRules_GetNextBestWeapon");
//         if (lua_isfunction(g_L, -1)) 
//         {
//             edict_t* pEnt = pPlayer ? ENT(((CCSPlayer*)pPlayer)->pev) : NULL;
//             lua_pushentity(g_L, pEnt);

//             edict_t* pWeaponEnt = pWeapon ? ENT(((CCSPlayerItem*)pWeapon)->pev) : NULL;
//             lua_pushentity(g_L, pWeaponEnt);

//             if (lua_pcall(g_L, 2, 1, 0) == 0)
//             {
//                 if (lua_isboolean(g_L, -1) || lua_isnumber(g_L, -1)) {
//                     BOOL ret = (BOOL)lua_tointeger(g_L, -1);
//                     lua_pop(g_L, 1);
//                     return ret;
//                 }
//                 lua_pop(g_L, 1);
//             }
//             else lua_pop(g_L, 1);
//         }
//         else lua_pop(g_L, 1);
//     }
//     return chain->callNext(pPlayer, pWeapon);
// }

// /* 49. CSGameRules_FlPlayerFallDamage */
// float CSGameRules_FlPlayerFallDamage(IReGameHook_CSGameRules_FlPlayerFallDamage *chain, CBasePlayer *pPlayer)
// {
//     if (g_L) 
//     {
//         lua_getglobal(g_L, "ReGame_CSGameRules_FlPlayerFallDamage");
//         if (lua_isfunction(g_L, -1)) 
//         {
//             edict_t* pEnt = pPlayer ? ENT(((CCSPlayer*)pPlayer)->pev) : NULL;
//             lua_pushentity(g_L, pEnt);

//             if (lua_pcall(g_L, 1, 1, 0) == 0)
//             {
//                 if (lua_isnumber(g_L, -1)) {
//                     float ret = (float)lua_tonumber(g_L, -1);
//                     lua_pop(g_L, 1);
//                     return ret;
//                 }
//                 lua_pop(g_L, 1);
//             }
//             else lua_pop(g_L, 1);
//         }
//         else lua_pop(g_L, 1);
//     }
//     return chain->callNext(pPlayer);
// }

// /* 50. CSGameRules_FPlayerCanTakeDamage */
// BOOL CSGameRules_FPlayerCanTakeDamage(IReGameHook_CSGameRules_FPlayerCanTakeDamage *chain, CBasePlayer *pPlayer, CBaseEntity *pAttacker)
// {
//     if (g_L) 
//     {
//         lua_getglobal(g_L, "ReGame_CSGameRules_FPlayerCanTakeDamage");
//         if (lua_isfunction(g_L, -1)) 
//         {
//             edict_t* pEnt = pPlayer ? ENT(((CCSPlayer*)pPlayer)->pev) : NULL;
//             lua_pushentity(g_L, pEnt);

//             edict_t* pAttackerEnt = pAttacker ? ENT(((CCSEntity*)pAttacker)->pev) : NULL;
//             lua_pushentity(g_L, pAttackerEnt);

//             if (lua_pcall(g_L, 2, 1, 0) == 0)
//             {
//                 if (lua_isboolean(g_L, -1) || lua_isnumber(g_L, -1)) {
//                     BOOL ret = (BOOL)lua_tointeger(g_L, -1);
//                     lua_pop(g_L, 1);
//                     return ret;
//                 }
//                 lua_pop(g_L, 1);
//             }
//             else lua_pop(g_L, 1);
//         }
//         else lua_pop(g_L, 1);
//     }
//     return chain->callNext(pPlayer, pAttacker);
// }

// /* 51. CSGameRules_PlayerSpawn */
// void CSGameRules_PlayerSpawn(IReGameHook_CSGameRules_PlayerSpawn *chain, CBasePlayer *pPlayer)
// {
//     if (g_L) 
//     {
//         lua_getglobal(g_L, "ReGame_CSGameRules_PlayerSpawn");
//         if (lua_isfunction(g_L, -1)) 
//         {
//             edict_t* pEnt = pPlayer ? ENT(((CCSPlayer*)pPlayer)->pev) : NULL;
//             lua_pushentity(g_L, pEnt);

//             if (lua_pcall(g_L, 1, 1, 0) == 0)
//             {
//                 if (lua_isboolean(g_L, -1) && lua_toboolean(g_L, -1)) { lua_pop(g_L, 1); return; }
//                 lua_pop(g_L, 1);
//             }
//             else lua_pop(g_L, 1);
//         }
//         else lua_pop(g_L, 1);
//     }
//     chain->callNext(pPlayer);
// }

// /* 52. CSGameRules_FPlayerCanRespawn */
// BOOL CSGameRules_FPlayerCanRespawn(IReGameHook_CSGameRules_FPlayerCanRespawn *chain, CBasePlayer *pPlayer)
// {
//     if (g_L) 
//     {
//         lua_getglobal(g_L, "ReGame_CSGameRules_FPlayerCanRespawn");
//         if (lua_isfunction(g_L, -1)) 
//         {
//             edict_t* pEnt = pPlayer ? ENT(((CCSPlayer*)pPlayer)->pev) : NULL;
//             lua_pushentity(g_L, pEnt);

//             if (lua_pcall(g_L, 1, 1, 0) == 0)
//             {
//                 if (lua_isboolean(g_L, -1) || lua_isnumber(g_L, -1)) {
//                     BOOL ret = (BOOL)lua_tointeger(g_L, -1);
//                     lua_pop(g_L, 1);
//                     return ret;
//                 }
//                 lua_pop(g_L, 1);
//             }
//             else lua_pop(g_L, 1);
//         }
//         else lua_pop(g_L, 1);
//     }
//     return chain->callNext(pPlayer);
// }

// /* 53. CSGameRules_GetPlayerSpawnSpot */
// edict_t *CSGameRules_GetPlayerSpawnSpot(IReGameHook_CSGameRules_GetPlayerSpawnSpot *chain, CBasePlayer *pPlayer)
// {
//     if (g_L) 
//     {
//         lua_getglobal(g_L, "ReGame_CSGameRules_GetPlayerSpawnSpot");
//         if (lua_isfunction(g_L, -1)) 
//         {
//             edict_t* pEnt = pPlayer ? ENT(((CCSPlayer*)pPlayer)->pev) : NULL;
//             lua_pushentity(g_L, pEnt);

//             if (lua_pcall(g_L, 1, 1, 0) == 0)
//             {
//                 if (lua_isuserdata(g_L, -1)) {
//                     // 假设 Lua 返回了 Edict 指针
//                     // edict_t* ret = (edict_t*)lua_touserdata(g_L, -1);
//                     // lua_pop(g_L, 1);
//                     // return ret;
//                 }
//                 lua_pop(g_L, 1);
//             }
//             else lua_pop(g_L, 1);
//         }
//         else lua_pop(g_L, 1);
//     }
//     return chain->callNext(pPlayer);
// }

// /* 54. CSGameRules_ClientUserInfoChanged */
// void CSGameRules_ClientUserInfoChanged(IReGameHook_CSGameRules_ClientUserInfoChanged *chain, CBasePlayer *pPlayer, char *infobuffer)
// {
//     if (g_L) 
//     {
//         lua_getglobal(g_L, "ReGame_CSGameRules_ClientUserInfoChanged");
//         if (lua_isfunction(g_L, -1)) 
//         {
//             edict_t* pEnt = pPlayer ? ENT(((CCSPlayer*)pPlayer)->pev) : NULL;
//             lua_pushentity(g_L, pEnt);

//             lua_pushstring(g_L, infobuffer);

//             if (lua_pcall(g_L, 2, LUA_MULTRET, 0) == 0)
//             {
//                 int nRet = lua_gettop(g_L);
//                 if (nRet == 1 && lua_isboolean(g_L, -1) && lua_toboolean(g_L, -1)) { lua_pop(g_L, 1); return; }
//                 else if (nRet == 1 && lua_isstring(g_L, -1)) {
//                     strncpy(infobuffer, lua_tostring(g_L, -1), 255);
//                     lua_pop(g_L, 1);
//                 }
//                 else lua_pop(g_L, nRet);
//             }
//             else lua_pop(g_L, 1);
//         }
//         else lua_pop(g_L, 1);
//     }
//     chain->callNext(pPlayer, infobuffer);
// }

// /* 55. CSGameRules_PlayerKilled */
// void CSGameRules_PlayerKilled(IReGameHook_CSGameRules_PlayerKilled *chain, CBasePlayer *pPlayer, entvars_t *pevKiller, entvars_t *pevInflictor)
// {
//     if (g_L) 
//     {
//         lua_getglobal(g_L, "ReGame_CSGameRules_PlayerKilled");
//         if (lua_isfunction(g_L, -1)) 
//         {
//             edict_t* pVictim = pPlayer ? ENT(((CCSPlayer*)pPlayer)->pev) : NULL;
//             lua_pushentity(g_L, pVictim);

//             edict_t* pKiller = pevKiller ? ENT(pevKiller) : NULL;
//             lua_pushentity(g_L, pKiller);

//             edict_t* pInflictor = pevInflictor ? ENT(pevInflictor) : NULL;
//             lua_pushentity(g_L, pInflictor);

//             if (lua_pcall(g_L, 3, 1, 0) == 0)
//             {
//                 if (lua_isboolean(g_L, -1) && lua_toboolean(g_L, -1)) { lua_pop(g_L, 1); return; }
//                 lua_pop(g_L, 1);
//             }
//             else lua_pop(g_L, 1);
//         }
//         else lua_pop(g_L, 1);
//     }
//     chain->callNext(pPlayer, pevKiller, pevInflictor);
// }

// /* 56. CSGameRules_DeathNotice */
// void CSGameRules_DeathNotice(IReGameHook_CSGameRules_DeathNotice *chain, CBasePlayer *pPlayer, entvars_t *pevKiller, entvars_t *pevInflictor)
// {
//     if (g_L) 
//     {
//         lua_getglobal(g_L, "ReGame_CSGameRules_DeathNotice");
//         if (lua_isfunction(g_L, -1)) 
//         {
//             edict_t* pVictim = pPlayer ? ENT(((CCSPlayer*)pPlayer)->pev) : NULL;
//             lua_pushentity(g_L, pVictim);

//             edict_t* pKiller = pevKiller ? ENT(pevKiller) : NULL;
//             lua_pushentity(g_L, pKiller);

//             edict_t* pInflictor = pevInflictor ? ENT(pevInflictor) : NULL;
//             lua_pushentity(g_L, pInflictor);

//             if (lua_pcall(g_L, 3, 1, 0) == 0)
//             {
//                 if (lua_isboolean(g_L, -1) && lua_toboolean(g_L, -1)) { lua_pop(g_L, 1); return; }
//                 lua_pop(g_L, 1);
//             }
//             else lua_pop(g_L, 1);
//         }
//         else lua_pop(g_L, 1);
//     }
//     chain->callNext(pPlayer, pevKiller, pevInflictor);
// }

// /* 57. CSGameRules_CanHavePlayerItem */
// BOOL CSGameRules_CanHavePlayerItem(IReGameHook_CSGameRules_CanHavePlayerItem *chain, CBasePlayer *pPlayer, CBasePlayerItem *pItem)
// {
//     if (g_L) 
//     {
//         lua_getglobal(g_L, "ReGame_CSGameRules_CanHavePlayerItem");
//         if (lua_isfunction(g_L, -1)) 
//         {
//             edict_t* pEnt = pPlayer ? ENT(((CCSPlayer*)pPlayer)->pev) : NULL;
//             lua_pushentity(g_L, pEnt);

//             edict_t* pItemEnt = pItem ? ENT(((CCSPlayerItem*)pItem)->pev) : NULL;
//             lua_pushentity(g_L, pItemEnt);

//             if (lua_pcall(g_L, 2, 1, 0) == 0)
//             {
//                 if (lua_isboolean(g_L, -1) || lua_isnumber(g_L, -1)) {
//                     BOOL ret = (BOOL)lua_tointeger(g_L, -1);
//                     lua_pop(g_L, 1);
//                     return ret;
//                 }
//                 lua_pop(g_L, 1);
//             }
//             else lua_pop(g_L, 1);
//         }
//         else lua_pop(g_L, 1);
//     }
//     return chain->callNext(pPlayer, pItem);
// }

// /* 58. CSGameRules_DeadPlayerWeapons */
// int CSGameRules_DeadPlayerWeapons(IReGameHook_CSGameRules_DeadPlayerWeapons *chain, CBasePlayer *pPlayer)
// {
//     if (g_L) 
//     {
//         lua_getglobal(g_L, "ReGame_CSGameRules_DeadPlayerWeapons");
//         if (lua_isfunction(g_L, -1)) 
//         {
//             edict_t* pEnt = pPlayer ? ENT(((CCSPlayer*)pPlayer)->pev) : NULL;
//             lua_pushentity(g_L, pEnt);

//             if (lua_pcall(g_L, 1, 1, 0) == 0)
//             {
//                 if (lua_isnumber(g_L, -1)) {
//                     int ret = (int)lua_tointeger(g_L, -1);
//                     lua_pop(g_L, 1);
//                     return ret;
//                 }
//                 lua_pop(g_L, 1);
//             }
//             else lua_pop(g_L, 1);
//         }
//         else lua_pop(g_L, 1);
//     }
//     return chain->callNext(pPlayer);
// }

// /* 59. CSGameRules_ServerDeactivate */
// void CSGameRules_ServerDeactivate(IReGameHook_CSGameRules_ServerDeactivate *chain)
// {
//     if (g_L) 
//     {
//         lua_getglobal(g_L, "ReGame_CSGameRules_ServerDeactivate");
//         if (lua_isfunction(g_L, -1)) 
//         {
//             if (lua_pcall(g_L, 0, 1, 0) == 0)
//             {
//                 if (lua_isboolean(g_L, -1) && lua_toboolean(g_L, -1)) { lua_pop(g_L, 1); return; }
//                 lua_pop(g_L, 1);
//             }
//             else lua_pop(g_L, 1);
//         }
//         else lua_pop(g_L, 1);
//     }
//     chain->callNext();
// }

// /* 60. CSGameRules_CheckMapConditions */
// void CSGameRules_CheckMapConditions(IReGameHook_CSGameRules_CheckMapConditions *chain)
// {
//     if (g_L) 
//     {
//         lua_getglobal(g_L, "ReGame_CSGameRules_CheckMapConditions");
//         if (lua_isfunction(g_L, -1)) 
//         {
//             if (lua_pcall(g_L, 0, 1, 0) == 0)
//             {
//                 if (lua_isboolean(g_L, -1) && lua_toboolean(g_L, -1)) { lua_pop(g_L, 1); return; }
//                 lua_pop(g_L, 1);
//             }
//             else lua_pop(g_L, 1);
//         }
//         else lua_pop(g_L, 1);
//     }
//     chain->callNext();
// }

// /* 61. CSGameRules_CleanUpMap */
// void CSGameRules_CleanUpMap(IReGameHook_CSGameRules_CleanUpMap *chain)
// {
//     if (g_L) 
//     {
//         lua_getglobal(g_L, "ReGame_CSGameRules_CleanUpMap");
//         if (lua_isfunction(g_L, -1)) 
//         {
//             if (lua_pcall(g_L, 0, 1, 0) == 0)
//             {
//                 if (lua_isboolean(g_L, -1) && lua_toboolean(g_L, -1)) { lua_pop(g_L, 1); return; }
//                 lua_pop(g_L, 1);
//             }
//             else lua_pop(g_L, 1);
//         }
//         else lua_pop(g_L, 1);
//     }
//     chain->callNext();
// }

// /* 62. CSGameRules_RestartRound */
// void CSGameRules_RestartRound(IReGameHook_CSGameRules_RestartRound *chain)
// {
//     if (g_L) 
//     {
//         lua_getglobal(g_L, "ReGame_CSGameRules_RestartRound");
//         if (lua_isfunction(g_L, -1)) 
//         {
//             if (lua_pcall(g_L, 0, 1, 0) == 0)
//             {
//                 if (lua_isboolean(g_L, -1) && lua_toboolean(g_L, -1)) { lua_pop(g_L, 1); return; }
//                 lua_pop(g_L, 1);
//             }
//             else lua_pop(g_L, 1);
//         }
//         else lua_pop(g_L, 1);
//     }
//     chain->callNext();
// }

// /* 63. CSGameRules_CheckWinConditions */
// void CSGameRules_CheckWinConditions(IReGameHook_CSGameRules_CheckWinConditions *chain)
// {
//     if (g_L) 
//     {
//         lua_getglobal(g_L, "ReGame_CSGameRules_CheckWinConditions");
//         if (lua_isfunction(g_L, -1)) 
//         {
//             if (lua_pcall(g_L, 0, 1, 0) == 0)
//             {
//                 if (lua_isboolean(g_L, -1) && lua_toboolean(g_L, -1)) { lua_pop(g_L, 1); return; }
//                 lua_pop(g_L, 1);
//             }
//             else lua_pop(g_L, 1);
//         }
//         else lua_pop(g_L, 1);
//     }
//     chain->callNext();
// }

// /* 64. CSGameRules_RemoveGuns */
// void CSGameRules_RemoveGuns(IReGameHook_CSGameRules_RemoveGuns *chain)
// {
//     if (g_L) 
//     {
//         lua_getglobal(g_L, "ReGame_CSGameRules_RemoveGuns");
//         if (lua_isfunction(g_L, -1)) 
//         {
//             if (lua_pcall(g_L, 0, 1, 0) == 0)
//             {
//                 if (lua_isboolean(g_L, -1) && lua_toboolean(g_L, -1)) { lua_pop(g_L, 1); return; }
//                 lua_pop(g_L, 1);
//             }
//             else lua_pop(g_L, 1);
//         }
//         else lua_pop(g_L, 1);
//     }
//     chain->callNext();
// }

// /* 65. CSGameRules_GiveC4 */
// void CSGameRules_GiveC4(IReGameHook_CSGameRules_GiveC4 *chain)
// {
//     if (g_L) 
//     {
//         lua_getglobal(g_L, "ReGame_CSGameRules_GiveC4");
//         if (lua_isfunction(g_L, -1)) 
//         {
//             if (lua_pcall(g_L, 0, 1, 0) == 0)
//             {
//                 if (lua_isboolean(g_L, -1) && lua_toboolean(g_L, -1)) { lua_pop(g_L, 1); return; }
//                 lua_pop(g_L, 1);
//             }
//             else lua_pop(g_L, 1);
//         }
//         else lua_pop(g_L, 1);
//     }
//     chain->callNext();
// }

// /* 66. CSGameRules_ChangeLevel */
// void CSGameRules_ChangeLevel(IReGameHook_CSGameRules_ChangeLevel *chain)
// {
//     if (g_L) 
//     {
//         lua_getglobal(g_L, "ReGame_CSGameRules_ChangeLevel");
//         if (lua_isfunction(g_L, -1)) 
//         {
//             if (lua_pcall(g_L, 0, 1, 0) == 0)
//             {
//                 if (lua_isboolean(g_L, -1) && lua_toboolean(g_L, -1)) { lua_pop(g_L, 1); return; }
//                 lua_pop(g_L, 1);
//             }
//             else lua_pop(g_L, 1);
//         }
//         else lua_pop(g_L, 1);
//     }
//     chain->callNext();
// }

// /* 67. CSGameRules_GoToIntermission */
// void CSGameRules_GoToIntermission(IReGameHook_CSGameRules_GoToIntermission *chain)
// {
//     if (g_L) 
//     {
//         lua_getglobal(g_L, "ReGame_CSGameRules_GoToIntermission");
//         if (lua_isfunction(g_L, -1)) 
//         {
//             if (lua_pcall(g_L, 0, 1, 0) == 0)
//             {
//                 if (lua_isboolean(g_L, -1) && lua_toboolean(g_L, -1)) { lua_pop(g_L, 1); return; }
//                 lua_pop(g_L, 1);
//             }
//             else lua_pop(g_L, 1);
//         }
//         else lua_pop(g_L, 1);
//     }
//     chain->callNext();
// }

// /* 68. CSGameRules_BalanceTeams */
// void CSGameRules_BalanceTeams(IReGameHook_CSGameRules_BalanceTeams *chain)
// {
//     if (g_L) 
//     {
//         lua_getglobal(g_L, "ReGame_CSGameRules_BalanceTeams");
//         if (lua_isfunction(g_L, -1)) 
//         {
//             if (lua_pcall(g_L, 0, 1, 0) == 0)
//             {
//                 if (lua_isboolean(g_L, -1) && lua_toboolean(g_L, -1)) { lua_pop(g_L, 1); return; }
//                 lua_pop(g_L, 1);
//             }
//             else lua_pop(g_L, 1);
//         }
//         else lua_pop(g_L, 1);
//     }
//     chain->callNext();
// }

// /* 69. CSGameRules_OnRoundFreezeEnd */
// void CSGameRules_OnRoundFreezeEnd(IReGameHook_CSGameRules_OnRoundFreezeEnd *chain)
// {
//     if (g_L) 
//     {
//         lua_getglobal(g_L, "ReGame_CSGameRules_OnRoundFreezeEnd");
//         if (lua_isfunction(g_L, -1)) 
//         {
//             if (lua_pcall(g_L, 0, 1, 0) == 0)
//             {
//                 if (lua_isboolean(g_L, -1) && lua_toboolean(g_L, -1)) { lua_pop(g_L, 1); return; }
//                 lua_pop(g_L, 1);
//             }
//             else lua_pop(g_L, 1);
//         }
//         else lua_pop(g_L, 1);
//     }
//     chain->callNext();
// }

// /* 70. PM_UpdateStepSound */
// void PM_UpdateStepSound(IReGameHook_PM_UpdateStepSound *chain)
// {
//     if (g_L) 
//     {
//         lua_getglobal(g_L, "ReGame_PM_UpdateStepSound");
//         if (lua_isfunction(g_L, -1)) 
//         {
//             if (lua_pcall(g_L, 0, 1, 0) == 0)
//             {
//                 if (lua_isboolean(g_L, -1) && lua_toboolean(g_L, -1)) { lua_pop(g_L, 1); return; }
//                 lua_pop(g_L, 1);
//             }
//             else lua_pop(g_L, 1);
//         }
//         else lua_pop(g_L, 1);
//     }
//     chain->callNext();
// }

// /* 71. CBasePlayer::StartDeathCam */
// void CBasePlayer_StartDeathCam(IReGameHook_CBasePlayer_StartDeathCam *chain, CBasePlayer *pthis)
// {
//     if (g_L) 
//     {
//         lua_getglobal(g_L, "ReGame_CBasePlayer_StartDeathCam");
//         if (lua_isfunction(g_L, -1)) 
//         {
//             edict_t* pEnt = pthis ? ENT(((CCSPlayer*)pthis)->pev) : NULL;
//             lua_pushentity(g_L, pEnt);

//             if (lua_pcall(g_L, 1, 1, 0) == 0)
//             {
//                 if (lua_isboolean(g_L, -1) && lua_toboolean(g_L, -1)) { lua_pop(g_L, 1); return; }
//                 lua_pop(g_L, 1);
//             }
//             else lua_pop(g_L, 1);
//         }
//         else lua_pop(g_L, 1);
//     }
//     chain->callNext(pthis);
// }

// /* 72. CBasePlayer::SwitchTeam */
// void CBasePlayer_SwitchTeam(IReGameHook_CBasePlayer_SwitchTeam *chain, CBasePlayer *pthis)
// {
//     if (g_L) 
//     {
//         lua_getglobal(g_L, "ReGame_CBasePlayer_SwitchTeam");
//         if (lua_isfunction(g_L, -1)) 
//         {
//             edict_t* pEnt = pthis ? ENT(((CCSPlayer*)pthis)->pev) : NULL;
//             lua_pushentity(g_L, pEnt);

//             if (lua_pcall(g_L, 1, 1, 0) == 0)
//             {
//                 if (lua_isboolean(g_L, -1) && lua_toboolean(g_L, -1)) { lua_pop(g_L, 1); return; }
//                 lua_pop(g_L, 1);
//             }
//             else lua_pop(g_L, 1);
//         }
//         else lua_pop(g_L, 1);
//     }
//     chain->callNext(pthis);
// }

// /* 73. CBasePlayer::CanSwitchTeam */
// bool CBasePlayer_CanSwitchTeam(IReGameHook_CBasePlayer_CanSwitchTeam *chain, CBasePlayer *pthis, TeamName team)
// {
//     int _team = (int)team;

//     if (g_L) 
//     {
//         lua_getglobal(g_L, "ReGame_CBasePlayer_CanSwitchTeam");
//         if (lua_isfunction(g_L, -1)) 
//         {
//             edict_t* pEnt = pthis ? ENT(((CCSPlayer*)pthis)->pev) : NULL;
//             lua_pushentity(g_L, pEnt);

//             lua_pushinteger(g_L, _team);

//             if (lua_pcall(g_L, 2, LUA_MULTRET, 0) == 0)
//             {
//                 int nRet = lua_gettop(g_L);
//                 if (nRet == 1) {
//                     if (lua_isboolean(g_L, -1)) {
//                         bool ret = (bool)lua_toboolean(g_L, -1);
//                         lua_pop(g_L, 1);
//                         return ret;
//                     }
//                     if (lua_isnumber(g_L, -1)) _team = (int)lua_tointeger(g_L, -1);
//                     lua_pop(g_L, 1);
//                 }
//                 else lua_pop(g_L, nRet);
//             }
//             else lua_pop(g_L, 1);
//         }
//         else lua_pop(g_L, 1);
//     }
//     return chain->callNext(pthis, (TeamName)_team);
// }

// /* 74. CBasePlayer::ThrowGrenade */
// CGrenade *CBasePlayer_ThrowGrenade(IReGameHook_CBasePlayer_ThrowGrenade *chain, CBasePlayer *pthis, CBasePlayerWeapon *pWeapon, Vector &vecOrigin, Vector &vecVelocity, float time, unsigned short type)
// {
//     float _time = time;
//     unsigned short _type = type;

//     if (g_L) 
//     {
//         lua_getglobal(g_L, "ReGame_CBasePlayer_ThrowGrenade");
//         if (lua_isfunction(g_L, -1)) 
//         {
//             edict_t* pEnt = pthis ? ENT(((CCSPlayer*)pthis)->pev) : NULL;
//             lua_pushentity(g_L, pEnt);

//             edict_t* pWeaponEnt = pWeapon ? ENT(((CCSPlayerWeapon*)pWeapon)->pev) : NULL;
//             lua_pushentity(g_L, pWeaponEnt);

//             lua_pushlightuserdata(g_L, &vecOrigin);
//             lua_pushlightuserdata(g_L, &vecVelocity);
//             lua_pushnumber(g_L, _time);
//             lua_pushinteger(g_L, _type);

//             if (lua_pcall(g_L, 6, LUA_MULTRET, 0) == 0)
//             {
//                 int nRet = lua_gettop(g_L);
//                 // 单值返回 -> Supercede (Expects Grenade Entity or Nil)
//                 if (nRet == 1) {
//                     if (lua_isnil(g_L, -1) || lua_isuserdata(g_L, -1)) {
//                         // ... Conversion logic ...
//                         lua_pop(g_L, 1);
//                         return NULL;
//                     }
//                     lua_pop(g_L, 1);
//                 }
//                 // 双值返回 -> Update (time, type) - Vectors are skipped for update
//                 else if (nRet == 2) {
//                     if (lua_isnumber(g_L, -2)) _time = (float)lua_tonumber(g_L, -2);
//                     if (lua_isnumber(g_L, -1)) _type = (unsigned short)lua_tointeger(g_L, -1);
//                     lua_pop(g_L, 2);
//                 }
//                 else lua_pop(g_L, nRet);
//             }
//             else lua_pop(g_L, 1);
//         }
//         else lua_pop(g_L, 1);
//     }
//     return chain->callNext(pthis, pWeapon, vecOrigin, vecVelocity, _time, _type);
// }

// /* 75. CSGameRules::CanPlayerHearPlayer */
// bool CSGameRules_CanPlayerHearPlayer(IReGameHook_CSGameRules_CanPlayerHearPlayer *chain, CBasePlayer *pListener, CBasePlayer *pSender)
// {
//     if (g_L) 
//     {
//         lua_getglobal(g_L, "ReGame_CSGameRules_CanPlayerHearPlayer");
//         if (lua_isfunction(g_L, -1)) 
//         {
//             edict_t* pListenerEnt = pListener ? ENT(((CCSPlayer*)pListener)->pev) : NULL;
//             lua_pushentity(g_L, pListenerEnt);

//             edict_t* pSenderEnt = pSender ? ENT(((CCSPlayer*)pSender)->pev) : NULL;
//             lua_pushentity(g_L, pSenderEnt);

//             if (lua_pcall(g_L, 2, 1, 0) == 0)
//             {
//                 if (lua_isboolean(g_L, -1)) {
//                     bool ret = (bool)lua_toboolean(g_L, -1);
//                     lua_pop(g_L, 1);
//                     return ret;
//                 }
//                 lua_pop(g_L, 1);
//             }
//             else lua_pop(g_L, 1);
//         }
//         else lua_pop(g_L, 1);
//     }
//     return chain->callNext(pListener, pSender);
// }

// /* 76. CWeaponBox::SetModel */
// void CWeaponBox_SetModel(IReGameHook_CWeaponBox_SetModel *chain, CWeaponBox *pthis, const char *pszModelName)
// {
//     const char* _pszModelName = pszModelName;
//     static char szModelBuf[64];

//     if (g_L) 
//     {
//         lua_getglobal(g_L, "ReGame_CWeaponBox_SetModel");
//         if (lua_isfunction(g_L, -1)) 
//         {
//             edict_t* pEnt = pthis ? ENT(((CCSWeaponBox*)pthis)->pev) : NULL;
//             lua_pushentity(g_L, pEnt);

//             lua_pushstring(g_L, _pszModelName);

//             if (lua_pcall(g_L, 2, LUA_MULTRET, 0) == 0)
//             {
//                 int nRet = lua_gettop(g_L);
//                 if (nRet == 1) {
//                     if (lua_isboolean(g_L, -1) && lua_toboolean(g_L, -1)) { lua_pop(g_L, 1); return; }
//                     if (lua_isstring(g_L, -1)) {
//                         strncpy(szModelBuf, lua_tostring(g_L, -1), 63);
//                         szModelBuf[63] = 0;
//                         _pszModelName = szModelBuf;
//                     }
//                     lua_pop(g_L, 1);
//                 }
//                 else lua_pop(g_L, nRet);
//             }
//             else lua_pop(g_L, 1);
//         }
//         else lua_pop(g_L, 1);
//     }
//     chain->callNext(pthis, _pszModelName);
// }

// /* 77. CGrenade::DefuseBombStart */
// void CGrenade_DefuseBombStart(IReGameHook_CGrenade_DefuseBombStart *chain, CGrenade *pthis, CBasePlayer *pPlayer)
// {
//     if (g_L) 
//     {
//         lua_getglobal(g_L, "ReGame_CGrenade_DefuseBombStart");
//         if (lua_isfunction(g_L, -1)) 
//         {
//             edict_t* pGrenadeEnt = pthis ? ENT(((CCSGrenade*)pthis)->pev) : NULL;
//             lua_pushentity(g_L, pGrenadeEnt);

//             edict_t* pPlayerEnt = pPlayer ? ENT(((CCSPlayer*)pPlayer)->pev) : NULL;
//             lua_pushentity(g_L, pPlayerEnt);

//             if (lua_pcall(g_L, 2, 1, 0) == 0)
//             {
//                 if (lua_isboolean(g_L, -1) && lua_toboolean(g_L, -1)) { lua_pop(g_L, 1); return; }
//                 lua_pop(g_L, 1);
//             }
//             else lua_pop(g_L, 1);
//         }
//         else lua_pop(g_L, 1);
//     }
//     chain->callNext(pthis, pPlayer);
// }

// /* 78. CGrenade::DefuseBombEnd */
// void CGrenade_DefuseBombEnd(IReGameHook_CGrenade_DefuseBombEnd *chain, CGrenade *pthis, CBasePlayer *pPlayer, bool bDefused)
// {
//     bool _bDefused = bDefused;

//     if (g_L) 
//     {
//         lua_getglobal(g_L, "ReGame_CGrenade_DefuseBombEnd");
//         if (lua_isfunction(g_L, -1)) 
//         {
//             edict_t* pGrenadeEnt = pthis ? ENT(((CCSGrenade*)pthis)->pev) : NULL;
//             lua_pushentity(g_L, pGrenadeEnt);

//             edict_t* pPlayerEnt = pPlayer ? ENT(((CCSPlayer*)pPlayer)->pev) : NULL;
//             lua_pushentity(g_L, pPlayerEnt);

//             lua_pushboolean(g_L, _bDefused);

//             if (lua_pcall(g_L, 3, LUA_MULTRET, 0) == 0)
//             {
//                 int nRet = lua_gettop(g_L);
//                 if (nRet == 1) {
//                     if (lua_isboolean(g_L, -1)) {
//                         // Ambiguous: Block or Update Arg? Assuming Block if single return
//                         if (lua_toboolean(g_L, -1)) { lua_pop(g_L, 1); return; }
//                     }
//                     lua_pop(g_L, 1);
//                 }
//                 else lua_pop(g_L, nRet);
//             }
//             else lua_pop(g_L, 1);
//         }
//         else lua_pop(g_L, 1);
//     }
//     chain->callNext(pthis, pPlayer, _bDefused);
// }

// /* 79. CGrenade::ExplodeHeGrenade */
// void CGrenade_ExplodeHeGrenade(IReGameHook_CGrenade_ExplodeHeGrenade *chain, CGrenade *pthis, TraceResult *pTrace, int bitsDamageType)
// {
//     int _bits = bitsDamageType;

//     if (g_L) 
//     {
//         lua_getglobal(g_L, "ReGame_CGrenade_ExplodeHeGrenade");
//         if (lua_isfunction(g_L, -1)) 
//         {
//             edict_t* pGrenadeEnt = pthis ? ENT(((CCSGrenade*)pthis)->pev) : NULL;
//             lua_pushentity(g_L, pGrenadeEnt);

//             lua_pushlightuserdata(g_L, pTrace);
//             lua_pushinteger(g_L, _bits);

//             if (lua_pcall(g_L, 3, LUA_MULTRET, 0) == 0)
//             {
//                 int nRet = lua_gettop(g_L);
//                 if (nRet == 1) {
//                     if (lua_isboolean(g_L, -1) && lua_toboolean(g_L, -1)) { lua_pop(g_L, 1); return; }
//                     if (lua_isnumber(g_L, -1)) _bits = (int)lua_tointeger(g_L, -1);
//                     lua_pop(g_L, 1);
//                 }
//                 else lua_pop(g_L, nRet);
//             }
//             else lua_pop(g_L, 1);
//         }
//         else lua_pop(g_L, 1);
//     }
//     chain->callNext(pthis, pTrace, _bits);
// }

// /* 80. CGrenade::ExplodeFlashbang */
// void CGrenade_ExplodeFlashbang(IReGameHook_CGrenade_ExplodeFlashbang *chain, CGrenade *pthis, TraceResult *pTrace, int bitsDamageType)
// {
//     int _bits = bitsDamageType;

//     if (g_L) 
//     {
//         lua_getglobal(g_L, "ReGame_CGrenade_ExplodeFlashbang");
//         if (lua_isfunction(g_L, -1)) 
//         {
//             edict_t* pGrenadeEnt = pthis ? ENT(((CCSGrenade*)pthis)->pev) : NULL;
//             lua_pushentity(g_L, pGrenadeEnt);

//             lua_pushlightuserdata(g_L, pTrace);
//             lua_pushinteger(g_L, _bits);

//             if (lua_pcall(g_L, 3, LUA_MULTRET, 0) == 0)
//             {
//                 int nRet = lua_gettop(g_L);
//                 if (nRet == 1) {
//                     if (lua_isboolean(g_L, -1) && lua_toboolean(g_L, -1)) { lua_pop(g_L, 1); return; }
//                     if (lua_isnumber(g_L, -1)) _bits = (int)lua_tointeger(g_L, -1);
//                     lua_pop(g_L, 1);
//                 }
//                 else lua_pop(g_L, nRet);
//             }
//             else lua_pop(g_L, 1);
//         }
//         else lua_pop(g_L, 1);
//     }
//     chain->callNext(pthis, pTrace, _bits);
// }

// /* 81. CGrenade::ExplodeSmokeGrenade */
// void CGrenade_ExplodeSmokeGrenade(IReGameHook_CGrenade_ExplodeSmokeGrenade *chain, CGrenade *pthis)
// {
//     if (g_L) 
//     {
//         lua_getglobal(g_L, "ReGame_CGrenade_ExplodeSmokeGrenade");
//         if (lua_isfunction(g_L, -1)) 
//         {
//             edict_t* pGrenadeEnt = pthis ? ENT(((CCSGrenade*)pthis)->pev) : NULL;
//             lua_pushentity(g_L, pGrenadeEnt);

//             if (lua_pcall(g_L, 1, 1, 0) == 0)
//             {
//                 if (lua_isboolean(g_L, -1) && lua_toboolean(g_L, -1)) { lua_pop(g_L, 1); return; }
//                 lua_pop(g_L, 1);
//             }
//             else lua_pop(g_L, 1);
//         }
//         else lua_pop(g_L, 1);
//     }
//     chain->callNext(pthis);
// }

// /* 82. CGrenade::ExplodeBomb */
// void CGrenade_ExplodeBomb(IReGameHook_CGrenade_ExplodeBomb *chain, CGrenade *pthis, TraceResult *pTrace, int bitsDamageType)
// {
//     int _bits = bitsDamageType;

//     if (g_L) 
//     {
//         lua_getglobal(g_L, "ReGame_CGrenade_ExplodeBomb");
//         if (lua_isfunction(g_L, -1)) 
//         {
//             edict_t* pGrenadeEnt = pthis ? ENT(((CCSGrenade*)pthis)->pev) : NULL;
//             lua_pushentity(g_L, pGrenadeEnt);

//             lua_pushlightuserdata(g_L, pTrace);
//             lua_pushinteger(g_L, _bits);

//             if (lua_pcall(g_L, 3, LUA_MULTRET, 0) == 0)
//             {
//                 int nRet = lua_gettop(g_L);
//                 if (nRet == 1) {
//                     if (lua_isboolean(g_L, -1) && lua_toboolean(g_L, -1)) { lua_pop(g_L, 1); return; }
//                     if (lua_isnumber(g_L, -1)) _bits = (int)lua_tointeger(g_L, -1);
//                     lua_pop(g_L, 1);
//                 }
//                 else lua_pop(g_L, nRet);
//             }
//             else lua_pop(g_L, 1);
//         }
//         else lua_pop(g_L, 1);
//     }
//     chain->callNext(pthis, pTrace, _bits);
// }

// /* 83. ThrowHeGrenade */
// CGrenade *ThrowHeGrenade(IReGameHook_ThrowHeGrenade *chain, entvars_t *pevOwner, Vector &vecOrigin, Vector &vecVelocity, float time, int iDamage, unsigned short type)
// {
//     float _time = time;
//     int _dmg = iDamage;
//     unsigned short _type = type;

//     if (g_L) 
//     {
//         lua_getglobal(g_L, "ReGame_ThrowHeGrenade");
//         if (lua_isfunction(g_L, -1)) 
//         {
//             edict_t* pOwner = pevOwner ? ENT(pevOwner) : NULL;
//             lua_pushentity(g_L, pOwner);

//             lua_pushlightuserdata(g_L, &vecOrigin);
//             lua_pushlightuserdata(g_L, &vecVelocity);
//             lua_pushnumber(g_L, _time);
//             lua_pushinteger(g_L, _dmg);
//             lua_pushinteger(g_L, _type);

//             if (lua_pcall(g_L, 6, LUA_MULTRET, 0) == 0)
//             {
//                 int nRet = lua_gettop(g_L);
//                 // 1 Ret = Supercede (return entity/nil)
//                 if (nRet == 1) {
//                     if (lua_isuserdata(g_L, -1) || lua_isnil(g_L, -1)) { lua_pop(g_L, 1); return NULL; }
//                     lua_pop(g_L, 1);
//                 }
//                 // 3 Rets = Update (time, dmg, type)
//                 else if (nRet == 3) {
//                     if (lua_isnumber(g_L, -3)) _time = (float)lua_tonumber(g_L, -3);
//                     if (lua_isnumber(g_L, -2)) _dmg = (int)lua_tointeger(g_L, -2);
//                     if (lua_isnumber(g_L, -1)) _type = (unsigned short)lua_tointeger(g_L, -1);
//                     lua_pop(g_L, 3);
//                 }
//                 else lua_pop(g_L, nRet);
//             }
//             else lua_pop(g_L, 1);
//         }
//         else lua_pop(g_L, 1);
//     }
//     return chain->callNext(pevOwner, vecOrigin, vecVelocity, _time, _dmg, _type);
// }

// /* 84. ThrowFlashbang */
// CGrenade *ThrowFlashbang(IReGameHook_ThrowFlashbang *chain, entvars_t *pevOwner, Vector &vecOrigin, Vector &vecVelocity, float time)
// {
//     float _time = time;

//     if (g_L) 
//     {
//         lua_getglobal(g_L, "ReGame_ThrowFlashbang");
//         if (lua_isfunction(g_L, -1)) 
//         {
//             edict_t* pOwner = pevOwner ? ENT(pevOwner) : NULL;
//             lua_pushentity(g_L, pOwner);

//             lua_pushlightuserdata(g_L, &vecOrigin);
//             lua_pushlightuserdata(g_L, &vecVelocity);
//             lua_pushnumber(g_L, _time);

//             if (lua_pcall(g_L, 4, LUA_MULTRET, 0) == 0)
//             {
//                 int nRet = lua_gettop(g_L);
//                 if (nRet == 1) {
//                     if (lua_isuserdata(g_L, -1) || lua_isnil(g_L, -1)) { lua_pop(g_L, 1); return NULL; }
//                     if (lua_isnumber(g_L, -1)) _time = (float)lua_tonumber(g_L, -1);
//                     lua_pop(g_L, 1);
//                 }
//                 else lua_pop(g_L, nRet);
//             }
//             else lua_pop(g_L, 1);
//         }
//         else lua_pop(g_L, 1);
//     }
//     return chain->callNext(pevOwner, vecOrigin, vecVelocity, _time);
// }

// /* 85. ThrowSmokeGrenade */
// CGrenade *ThrowSmokeGrenade(IReGameHook_ThrowSmokeGrenade *chain, entvars_t *pevOwner, Vector &vecOrigin, Vector &vecVelocity, float time, unsigned short type)
// {
//     float _time = time;
//     unsigned short _type = type;

//     if (g_L) 
//     {
//         lua_getglobal(g_L, "ReGame_ThrowSmokeGrenade");
//         if (lua_isfunction(g_L, -1)) 
//         {
//             edict_t* pOwner = pevOwner ? ENT(pevOwner) : NULL;
//             lua_pushentity(g_L, pOwner);

//             lua_pushlightuserdata(g_L, &vecOrigin);
//             lua_pushlightuserdata(g_L, &vecVelocity);
//             lua_pushnumber(g_L, _time);
//             lua_pushinteger(g_L, _type);

//             if (lua_pcall(g_L, 5, LUA_MULTRET, 0) == 0)
//             {
//                 int nRet = lua_gettop(g_L);
//                 if (nRet == 1) {
//                     if (lua_isuserdata(g_L, -1) || lua_isnil(g_L, -1)) { lua_pop(g_L, 1); return NULL; }
//                     lua_pop(g_L, 1);
//                 }
//                 else if (nRet == 2) {
//                     if (lua_isnumber(g_L, -2)) _time = (float)lua_tonumber(g_L, -2);
//                     if (lua_isnumber(g_L, -1)) _type = (unsigned short)lua_tointeger(g_L, -1);
//                     lua_pop(g_L, 2);
//                 }
//                 else lua_pop(g_L, nRet);
//             }
//             else lua_pop(g_L, 1);
//         }
//         else lua_pop(g_L, 1);
//     }
//     return chain->callNext(pevOwner, vecOrigin, vecVelocity, _time, _type);
// }

// /* 86. PlantBomb */
// CGrenade *PlantBomb(IReGameHook_PlantBomb *chain, entvars_t *pevOwner, Vector &vecOrigin, Vector &vecVelocity)
// {
//     if (g_L) 
//     {
//         lua_getglobal(g_L, "ReGame_PlantBomb");
//         if (lua_isfunction(g_L, -1)) 
//         {
//             edict_t* pOwner = pevOwner ? ENT(pevOwner) : NULL;
//             lua_pushentity(g_L, pOwner);

//             lua_pushlightuserdata(g_L, &vecOrigin);
//             lua_pushlightuserdata(g_L, &vecVelocity);

//             if (lua_pcall(g_L, 3, 1, 0) == 0)
//             {
//                 if (lua_isuserdata(g_L, -1) || lua_isnil(g_L, -1)) { lua_pop(g_L, 1); return NULL; }
//                 lua_pop(g_L, 1);
//             }
//             else lua_pop(g_L, 1);
//         }
//         else lua_pop(g_L, 1);
//     }
//     return chain->callNext(pevOwner, vecOrigin, vecVelocity);
// }

// /* 87. CBasePlayer::RemoveSpawnProtection */
// void CBasePlayer_RemoveSpawnProtection(IReGameHook_CBasePlayer_RemoveSpawnProtection *chain, CBasePlayer *pthis)
// {
//     if (g_L) 
//     {
//         lua_getglobal(g_L, "ReGame_CBasePlayer_RemoveSpawnProtection");
//         if (lua_isfunction(g_L, -1)) 
//         {
//             edict_t* pEnt = pthis ? ENT(((CCSPlayer*)pthis)->pev) : NULL;
//             lua_pushentity(g_L, pEnt);

//             if (lua_pcall(g_L, 1, 1, 0) == 0)
//             {
//                 if (lua_isboolean(g_L, -1) && lua_toboolean(g_L, -1)) { lua_pop(g_L, 1); return; }
//                 lua_pop(g_L, 1);
//             }
//             else lua_pop(g_L, 1);
//         }
//         else lua_pop(g_L, 1);
//     }
//     chain->callNext(pthis);
// }

// /* 88. CBasePlayer::SetSpawnProtection */
// void CBasePlayer_SetSpawnProtection(IReGameHook_CBasePlayer_SetSpawnProtection *chain, CBasePlayer *pthis, float time)
// {
//     float _time = time;

//     if (g_L) 
//     {
//         lua_getglobal(g_L, "ReGame_CBasePlayer_SetSpawnProtection");
//         if (lua_isfunction(g_L, -1)) 
//         {
//             edict_t* pEnt = pthis ? ENT(((CCSPlayer*)pthis)->pev) : NULL;
//             lua_pushentity(g_L, pEnt);

//             lua_pushnumber(g_L, _time);

//             if (lua_pcall(g_L, 2, LUA_MULTRET, 0) == 0)
//             {
//                 int nRet = lua_gettop(g_L);
//                 if (nRet == 1) {
//                     if (lua_isboolean(g_L, -1) && lua_toboolean(g_L, -1)) { lua_pop(g_L, 1); return; }
//                     if (lua_isnumber(g_L, -1)) _time = (float)lua_tonumber(g_L, -1);
//                     lua_pop(g_L, 1);
//                 }
//                 else lua_pop(g_L, nRet);
//             }
//             else lua_pop(g_L, 1);
//         }
//         else lua_pop(g_L, 1);
//     }
//     chain->callNext(pthis, _time);
// }

// /* 89. IsPenetrableEntity */
// bool IsPenetrableEntity(IReGameHook_IsPenetrableEntity *chain, Vector &vecSrc, Vector &vecDest, entvars_t *pevInflictor, edict_t *pEnt)
// {
//     if (g_L) 
//     {
//         lua_getglobal(g_L, "ReGame_IsPenetrableEntity");
//         if (lua_isfunction(g_L, -1)) 
//         {
//             lua_pushlightuserdata(g_L, &vecSrc);
//             lua_pushlightuserdata(g_L, &vecDest);
            
//             edict_t* pInflictor = pevInflictor ? ENT(pevInflictor) : NULL;
//             lua_pushentity(g_L, pInflictor);

//             lua_pushentity(g_L, pEnt);

//             if (lua_pcall(g_L, 4, 1, 0) == 0)
//             {
//                 if (lua_isboolean(g_L, -1)) {
//                     bool ret = (bool)lua_toboolean(g_L, -1);
//                     lua_pop(g_L, 1);
//                     return ret;
//                 }
//                 lua_pop(g_L, 1);
//             }
//             else lua_pop(g_L, 1);
//         }
//         else lua_pop(g_L, 1);
//     }
//     return chain->callNext(vecSrc, vecDest, pevInflictor, pEnt);
// }
// /* 1. Steam_NotifyClientConnect */
// qboolean Steam_NotifyClientConnect(IRehldsHook_Steam_NotifyClientConnect *chain, IGameClient *cl, const void *pvSteam2Key, unsigned int ucbSteam2Key)
// {
//     unsigned int _ucb = ucbSteam2Key;
//     if (g_L) 
//     {
//         lua_getglobal(g_L, "Rehlds_Steam_NotifyClientConnect");
//         if (lua_isfunction(g_L, -1)) 
//         {
//             edict_t* pEnt = (cl) ? cl->GetEdict() : NULL;
//             lua_pushentity(g_L, pEnt);
//             lua_pushlightuserdata(g_L, (void*)pvSteam2Key);
//             lua_pushinteger(g_L, _ucb);

//             if (lua_pcall(g_L, 3, LUA_MULTRET, 0) == 0)
//             {
//                 int nRet = lua_gettop(g_L);
//                 if (nRet == 1 && (lua_isboolean(g_L, -1) || lua_isnumber(g_L, -1))) {
//                     qboolean ret = (qboolean)lua_tointeger(g_L, -1);
//                     lua_pop(g_L, 1);
//                     return ret;
//                 }
//                 else if (nRet == 3) {
//                     if (lua_isnumber(g_L, -1)) _ucb = (unsigned int)lua_tointeger(g_L, -1);
//                     lua_pop(g_L, 3);
//                 }
//                 else lua_pop(g_L, nRet);
//             }
//             else lua_pop(g_L, 1);
//         }
//         else lua_pop(g_L, 1);
//     }
//     return chain->callNext(cl, pvSteam2Key, _ucb);
// }

// /* 2. SV_ConnectClient */
// void SV_ConnectClient(IRehldsHook_SV_ConnectClient *chain)
// {
//     if (g_L) 
//     {
//         lua_getglobal(g_L, "Rehlds_SV_ConnectClient");
//         if (lua_isfunction(g_L, -1)) 
//         {
//             if (lua_pcall(g_L, 0, 1, 0) == 0)
//             {
//                 if (lua_isboolean(g_L, -1) && lua_toboolean(g_L, -1)) { lua_pop(g_L, 1); return; }
//                 lua_pop(g_L, 1);
//             }
//             else lua_pop(g_L, 1);
//         }
//         else lua_pop(g_L, 1);
//     }
//     chain->callNext();
// }

// /* 3. SV_GetIDString */
// char* SV_GetIDString(IRehldsHook_SV_GetIDString *chain, USERID_t *id)
// {
//     static char szStaticRet[256];
//     if (g_L) 
//     {
//         lua_getglobal(g_L, "Rehlds_SV_GetIDString");
//         if (lua_isfunction(g_L, -1)) 
//         {
//             lua_pushlightuserdata(g_L, id);

//             if (lua_pcall(g_L, 1, 1, 0) == 0)
//             {
//                 if (lua_isstring(g_L, -1)) {
//                     strncpy(szStaticRet, lua_tostring(g_L, -1), 255);
//                     szStaticRet[255] = 0;
//                     lua_pop(g_L, 1);
//                     return szStaticRet;
//                 }
//                 lua_pop(g_L, 1);
//             }
//             else lua_pop(g_L, 1);
//         }
//         else lua_pop(g_L, 1);
//     }
//     return chain->callNext(id);
// }

// /* 4. SV_SendServerinfo */
// void SV_SendServerinfo(IRehldsHook_SV_SendServerinfo *chain, sizebuf_t *msg, IGameClient *cl)
// {
//     if (g_L) 
//     {
//         lua_getglobal(g_L, "Rehlds_SV_SendServerinfo");
//         if (lua_isfunction(g_L, -1)) 
//         {
//             lua_pushlightuserdata(g_L, msg);
//             edict_t* pEnt = (cl) ? cl->GetEdict() : NULL;
//             lua_pushentity(g_L, pEnt);

//             if (lua_pcall(g_L, 2, 1, 0) == 0)
//             {
//                 if (lua_isboolean(g_L, -1) && lua_toboolean(g_L, -1)) { lua_pop(g_L, 1); return; }
//                 lua_pop(g_L, 1);
//             }
//             else lua_pop(g_L, 1);
//         }
//         else lua_pop(g_L, 1);
//     }
//     chain->callNext(msg, cl);
// }

// /* 5. SV_CheckProtocol */
// int SV_CheckProtocol(IRehldsHook_SV_CheckProtocol *chain, netadr_t *adr, int protocol)
// {
//     int _proto = protocol;
//     if (g_L) 
//     {
//         lua_getglobal(g_L, "Rehlds_SV_CheckProtocol");
//         if (lua_isfunction(g_L, -1)) 
//         {
//             lua_pushlightuserdata(g_L, adr);
//             lua_pushinteger(g_L, _proto);

//             if (lua_pcall(g_L, 2, LUA_MULTRET, 0) == 0)
//             {
//                 int nRet = lua_gettop(g_L);
//                 if (nRet == 1 && lua_isnumber(g_L, -1)) {
//                     int ret = (int)lua_tointeger(g_L, -1);
//                     lua_pop(g_L, 1);
//                     return ret;
//                 }
//                 else if (nRet == 2) {
//                     if (lua_isnumber(g_L, -1)) _proto = (int)lua_tointeger(g_L, -1);
//                     lua_pop(g_L, 2);
//                 }
//                 else lua_pop(g_L, nRet);
//             }
//             else lua_pop(g_L, 1);
//         }
//         else lua_pop(g_L, 1);
//     }
//     return chain->callNext(adr, _proto);
// }

// /* 6. SVC_GetChallenge_mod */
// void SVC_GetChallenge_mod(IRehldsHook_SVC_GetChallenge_mod *chain, char *buffer, int clientChallenge)
// {
//     int _chal = clientChallenge;
//     if (g_L) 
//     {
//         lua_getglobal(g_L, "Rehlds_SVC_GetChallenge_mod");
//         if (lua_isfunction(g_L, -1)) 
//         {
//             lua_pushstring(g_L, buffer);
//             lua_pushinteger(g_L, _chal);

//             if (lua_pcall(g_L, 2, LUA_MULTRET, 0) == 0)
//             {
//                 int nRet = lua_gettop(g_L);
//                 if (nRet == 1 && lua_isboolean(g_L, -1) && lua_toboolean(g_L, -1)) {
//                     lua_pop(g_L, 1);
//                     return;
//                 }
//                 else if (nRet == 2) {
//                     if (lua_isstring(g_L, -2)) strcpy(buffer, lua_tostring(g_L, -2));
//                     if (lua_isnumber(g_L, -1)) _chal = (int)lua_tointeger(g_L, -1);
//                     lua_pop(g_L, 2);
//                 }
//                 else lua_pop(g_L, nRet);
//             }
//             else lua_pop(g_L, 1);
//         }
//         else lua_pop(g_L, 1);
//     }
//     chain->callNext(buffer, _chal);
// }

// /* 7. SV_CheckKeyInfo */
// int SV_CheckKeyInfo(IRehldsHook_SV_CheckKeyInfo *chain, netadr_t *adr, char *protinfo, uint16 *port, int *player_idx, char *authinfo, char *server_cdkey)
// {
//     if (g_L) 
//     {
//         lua_getglobal(g_L, "Rehlds_SV_CheckKeyInfo");
//         if (lua_isfunction(g_L, -1)) 
//         {
//             lua_pushlightuserdata(g_L, adr);
//             lua_pushstring(g_L, protinfo);
//             lua_pushlightuserdata(g_L, port);
//             lua_pushlightuserdata(g_L, player_idx);
//             lua_pushstring(g_L, authinfo);
//             lua_pushstring(g_L, server_cdkey);

//             if (lua_pcall(g_L, 6, LUA_MULTRET, 0) == 0)
//             {
//                 int nRet = lua_gettop(g_L);
//                 if (nRet == 1 && lua_isnumber(g_L, -1)) {
//                     int ret = (int)lua_tointeger(g_L, -1);
//                     lua_pop(g_L, 1);
//                     return ret;
//                 }
//                 else if (nRet == 6) {
//                     if (lua_isstring(g_L, -5)) strcpy(protinfo, lua_tostring(g_L, -5));
//                     if (lua_isstring(g_L, -2)) strcpy(authinfo, lua_tostring(g_L, -2));
//                     if (lua_isstring(g_L, -1)) strcpy(server_cdkey, lua_tostring(g_L, -1));
//                     lua_pop(g_L, 6);
//                 }
//                 else lua_pop(g_L, nRet);
//             }
//             else lua_pop(g_L, 1);
//         }
//         else lua_pop(g_L, 1);
//     }
//     return chain->callNext(adr, protinfo, port, player_idx, authinfo, server_cdkey);
// }

// /* 8. SV_CheckIPRestrictions */
// int SV_CheckIPRestrictions(IRehldsHook_SV_CheckIPRestrictions *chain, netadr_t *adr, int protocol)
// {
//     int _proto = protocol;
//     if (g_L) 
//     {
//         lua_getglobal(g_L, "Rehlds_SV_CheckIPRestrictions");
//         if (lua_isfunction(g_L, -1)) 
//         {
//             lua_pushlightuserdata(g_L, adr);
//             lua_pushinteger(g_L, _proto);

//             if (lua_pcall(g_L, 2, LUA_MULTRET, 0) == 0)
//             {
//                 int nRet = lua_gettop(g_L);
//                 if (nRet == 1 && lua_isnumber(g_L, -1)) {
//                     int ret = (int)lua_tointeger(g_L, -1);
//                     lua_pop(g_L, 1);
//                     return ret;
//                 }
//                 else if (nRet == 2) {
//                     if (lua_isnumber(g_L, -1)) _proto = (int)lua_tointeger(g_L, -1);
//                     lua_pop(g_L, 2);
//                 }
//                 else lua_pop(g_L, nRet);
//             }
//             else lua_pop(g_L, 1);
//         }
//         else lua_pop(g_L, 1);
//     }
//     return chain->callNext(adr, _proto);
// }

// /* 9. SV_FinishCertificateCheck */
// int SV_FinishCertificateCheck(IRehldsHook_SV_FinishCertificateCheck *chain, netadr_t *adr, int protocol, char *authinfo, char *server_cdkey)
// {
//     int _proto = protocol;
//     if (g_L) 
//     {
//         lua_getglobal(g_L, "Rehlds_SV_FinishCertificateCheck");
//         if (lua_isfunction(g_L, -1)) 
//         {
//             lua_pushlightuserdata(g_L, adr);
//             lua_pushinteger(g_L, _proto);
//             lua_pushstring(g_L, authinfo);
//             lua_pushstring(g_L, server_cdkey);

//             if (lua_pcall(g_L, 4, LUA_MULTRET, 0) == 0)
//             {
//                 int nRet = lua_gettop(g_L);
//                 if (nRet == 1 && lua_isnumber(g_L, -1)) {
//                     int ret = (int)lua_tointeger(g_L, -1);
//                     lua_pop(g_L, 1);
//                     return ret;
//                 }
//                 else if (nRet == 4) {
//                     if (lua_isnumber(g_L, -3)) _proto = (int)lua_tointeger(g_L, -3);
//                     if (lua_isstring(g_L, -2)) strcpy(authinfo, lua_tostring(g_L, -2));
//                     if (lua_isstring(g_L, -1)) strcpy(server_cdkey, lua_tostring(g_L, -1));
//                     lua_pop(g_L, 4);
//                 }
//                 else lua_pop(g_L, nRet);
//             }
//             else lua_pop(g_L, 1);
//         }
//         else lua_pop(g_L, 1);
//     }
//     return chain->callNext(adr, _proto, authinfo, server_cdkey);
// }

// /* 10. Steam_NotifyBotConnect */
// qboolean Steam_NotifyBotConnect(IRehldsHook_Steam_NotifyBotConnect *chain, IGameClient *cl)
// {
//     if (g_L) 
//     {
//         lua_getglobal(g_L, "Rehlds_Steam_NotifyBotConnect");
//         if (lua_isfunction(g_L, -1)) 
//         {
//             edict_t* pEnt = (cl) ? cl->GetEdict() : NULL;
//             lua_pushentity(g_L, pEnt);

//             if (lua_pcall(g_L, 1, 1, 0) == 0)
//             {
//                 if (lua_isboolean(g_L, -1) || lua_isnumber(g_L, -1)) {
//                     qboolean ret = (qboolean)lua_tointeger(g_L, -1);
//                     lua_pop(g_L, 1);
//                     return ret;
//                 }
//                 lua_pop(g_L, 1);
//             }
//             else lua_pop(g_L, 1);
//         }
//         else lua_pop(g_L, 1);
//     }
//     return chain->callNext(cl);
// }

// /* 11. SerializeSteamId */
// void SerializeSteamId(IRehldsHook_SerializeSteamId *chain, USERID_t *id1, USERID_t *id2)
// {
//     if (g_L) 
//     {
//         lua_getglobal(g_L, "Rehlds_SerializeSteamId");
//         if (lua_isfunction(g_L, -1)) 
//         {
//             lua_pushlightuserdata(g_L, id1);
//             lua_pushlightuserdata(g_L, id2);

//             if (lua_pcall(g_L, 2, 1, 0) == 0)
//             {
//                 if (lua_isboolean(g_L, -1) && lua_toboolean(g_L, -1)) { lua_pop(g_L, 1); return; }
//                 lua_pop(g_L, 1);
//             }
//             else lua_pop(g_L, 1);
//         }
//         else lua_pop(g_L, 1);
//     }
//     chain->callNext(id1, id2);
// }

// /* 12. SV_CompareUserID */
// qboolean SV_CompareUserID(IRehldsHook_SV_CompareUserID *chain, USERID_t *id1, USERID_t *id2)
// {
//     if (g_L) 
//     {
//         lua_getglobal(g_L, "Rehlds_SV_CompareUserID");
//         if (lua_isfunction(g_L, -1)) 
//         {
//             lua_pushlightuserdata(g_L, id1);
//             lua_pushlightuserdata(g_L, id2);

//             if (lua_pcall(g_L, 2, 1, 0) == 0)
//             {
//                 if (lua_isboolean(g_L, -1) || lua_isnumber(g_L, -1)) {
//                     qboolean ret = (qboolean)lua_tointeger(g_L, -1);
//                     lua_pop(g_L, 1);
//                     return ret;
//                 }
//                 lua_pop(g_L, 1);
//             }
//             else lua_pop(g_L, 1);
//         }
//         else lua_pop(g_L, 1);
//     }
//     return chain->callNext(id1, id2);
// }

// /* 13. Steam_NotifyClientDisconnect */
// void Steam_NotifyClientDisconnect(IRehldsHook_Steam_NotifyClientDisconnect *chain, IGameClient *cl)
// {
//     if (g_L) 
//     {
//         lua_getglobal(g_L, "Rehlds_Steam_NotifyClientDisconnect");
//         if (lua_isfunction(g_L, -1)) 
//         {
//             edict_t* pEnt = (cl) ? cl->GetEdict() : NULL;
//             lua_pushentity(g_L, pEnt);

//             if (lua_pcall(g_L, 1, 1, 0) == 0)
//             {
//                 if (lua_isboolean(g_L, -1) && lua_toboolean(g_L, -1)) { lua_pop(g_L, 1); return; }
//                 lua_pop(g_L, 1);
//             }
//             else lua_pop(g_L, 1);
//         }
//         else lua_pop(g_L, 1);
//     }
//     chain->callNext(cl);
// }

// /* 14. PreProcessPacket */
// bool PreProcessPacket(IRehldsHook_PreprocessPacket *chain, uint8 *data, unsigned int len, const netadr_t &adr)
// {
//     unsigned int _len = len;
//     if (g_L) 
//     {
//         lua_getglobal(g_L, "Rehlds_PreProcessPacket");
//         if (lua_isfunction(g_L, -1)) 
//         {
//             lua_pushlightuserdata(g_L, data);
//             lua_pushinteger(g_L, _len);
//             lua_pushlightuserdata(g_L, (void*)&adr);

//             if (lua_pcall(g_L, 3, LUA_MULTRET, 0) == 0)
//             {
//                 int nRet = lua_gettop(g_L);
//                 if (nRet == 1 && lua_isboolean(g_L, -1)) {
//                     bool ret = (bool)lua_toboolean(g_L, -1);
//                     lua_pop(g_L, 1);
//                     return ret;
//                 }
//                 else if (nRet == 3) {
//                     if (lua_isnumber(g_L, -2)) _len = (unsigned int)lua_tointeger(g_L, -2);
//                     lua_pop(g_L, 3);
//                 }
//                 else lua_pop(g_L, nRet);
//             }
//             else lua_pop(g_L, 1);
//         }
//         else lua_pop(g_L, 1);
//     }
//     return chain->callNext(data, _len, adr);
// }

// /* 15. ValidateCommand */
// bool ValidateCommand(IRehldsHook_ValidateCommand *chain, const char *cmd, cmd_source_t source, IGameClient *cl)
// {
//     const char* _cmd = cmd;
//     cmd_source_t _src = source;
//     static char szCmdBuf[512];

//     if (g_L) 
//     {
//         lua_getglobal(g_L, "Rehlds_ValidateCommand");
//         if (lua_isfunction(g_L, -1)) 
//         {
//             lua_pushstring(g_L, _cmd);
//             lua_pushinteger(g_L, (int)_src);
//             edict_t* pEnt = (cl) ? cl->GetEdict() : NULL;
//             lua_pushentity(g_L, pEnt);

//             if (lua_pcall(g_L, 3, LUA_MULTRET, 0) == 0)
//             {
//                 int nRet = lua_gettop(g_L);
//                 if (nRet == 1 && lua_isboolean(g_L, -1)) {
//                     bool ret = (bool)lua_toboolean(g_L, -1);
//                     lua_pop(g_L, 1);
//                     return ret;
//                 }
//                 else if (nRet == 3) {
//                     if (lua_isstring(g_L, -3)) {
//                         strncpy(szCmdBuf, lua_tostring(g_L, -3), 511);
//                         szCmdBuf[511] = 0;
//                         _cmd = szCmdBuf;
//                     }
//                     if (lua_isnumber(g_L, -2)) _src = (cmd_source_t)lua_tointeger(g_L, -2);
//                     lua_pop(g_L, 3);
//                 }
//                 else lua_pop(g_L, nRet);
//             }
//             else lua_pop(g_L, 1);
//         }
//         else lua_pop(g_L, 1);
//     }
//     return chain->callNext(_cmd, _src, cl);
// }

// /* 16. ExecuteServerStringCmd */
// void ExecuteServerStringCmd(IRehldsHook_ExecuteServerStringCmd *chain, const char *cmd, cmd_source_t source, IGameClient *cl)
// {
//     const char* _cmd = cmd;
//     cmd_source_t _src = source;
//     static char szCmdBuf[512];

//     if (g_L) 
//     {
//         lua_getglobal(g_L, "Rehlds_ExecuteServerStringCmd");
//         if (lua_isfunction(g_L, -1)) 
//         {
//             lua_pushstring(g_L, _cmd);
//             lua_pushinteger(g_L, (int)_src);
//             edict_t* pEnt = (cl) ? cl->GetEdict() : NULL;
//             lua_pushentity(g_L, pEnt);

//             if (lua_pcall(g_L, 3, LUA_MULTRET, 0) == 0)
//             {
//                 int nRet = lua_gettop(g_L);
//                 if (nRet == 1 && lua_isboolean(g_L, -1) && lua_toboolean(g_L, -1)) {
//                     lua_pop(g_L, 1);
//                     return;
//                 }
//                 else if (nRet == 3) {
//                     if (lua_isstring(g_L, -3)) {
//                         strncpy(szCmdBuf, lua_tostring(g_L, -3), 511);
//                         szCmdBuf[511] = 0;
//                         _cmd = szCmdBuf;
//                     }
//                     if (lua_isnumber(g_L, -2)) _src = (cmd_source_t)lua_tointeger(g_L, -2);
//                     lua_pop(g_L, 3);
//                 }
//                 else lua_pop(g_L, nRet);
//             }
//             else lua_pop(g_L, 1);
//         }
//         else lua_pop(g_L, 1);
//     }
//     chain->callNext(_cmd, _src, cl);
// }

// /* 17. ClientConnected */
// void ClientConnected(IRehldsHook_ClientConnected *chain, IGameClient *cl)
// {
//     if (g_L) 
//     {
//         lua_getglobal(g_L, "Rehlds_ClientConnected");
//         if (lua_isfunction(g_L, -1)) 
//         {
//             edict_t* pEnt = (cl) ? cl->GetEdict() : NULL;
//             lua_pushentity(g_L, pEnt);

//             if (lua_pcall(g_L, 1, 1, 0) == 0)
//             {
//                 if (lua_isboolean(g_L, -1) && lua_toboolean(g_L, -1)) { lua_pop(g_L, 1); return; }
//                 lua_pop(g_L, 1);
//             }
//             else lua_pop(g_L, 1);
//         }
//         else lua_pop(g_L, 1);
//     }
//     chain->callNext(cl);
// }

// /* 18. HandleNetCommand */
// void HandleNetCommand(IRehldsHook_HandleNetCommand *chain, IGameClient *cl, int8 cmd)
// {
//     int _cmd = (int)cmd;
//     if (g_L) 
//     {
//         lua_getglobal(g_L, "Rehlds_HandleNetCommand");
//         if (lua_isfunction(g_L, -1)) 
//         {
//             edict_t* pEnt = (cl) ? cl->GetEdict() : NULL;
//             lua_pushentity(g_L, pEnt);
//             lua_pushinteger(g_L, _cmd);

//             if (lua_pcall(g_L, 2, LUA_MULTRET, 0) == 0)
//             {
//                 int nRet = lua_gettop(g_L);
//                 if (nRet == 1 && lua_isboolean(g_L, -1) && lua_toboolean(g_L, -1)) {
//                     lua_pop(g_L, 1);
//                     return;
//                 }
//                 else if (nRet == 2) {
//                     if (lua_isnumber(g_L, -1)) _cmd = (int)lua_tointeger(g_L, -1);
//                     lua_pop(g_L, 2);
//                 }
//                 else lua_pop(g_L, nRet);
//             }
//             else lua_pop(g_L, 1);
//         }
//         else lua_pop(g_L, 1);
//     }
//     chain->callNext(cl, (int8)_cmd);
// }

// /* 19. Mod_LoadBrushModel */
// void Mod_LoadBrushModel(IRehldsHook_Mod_LoadBrushModel *chain, model_t *mod, void *buffer)
// {
//     if (g_L) 
//     {
//         lua_getglobal(g_L, "Rehlds_Mod_LoadBrushModel");
//         if (lua_isfunction(g_L, -1)) 
//         {
//             lua_pushlightuserdata(g_L, mod);
//             lua_pushlightuserdata(g_L, buffer);

//             if (lua_pcall(g_L, 2, 1, 0) == 0)
//             {
//                 if (lua_isboolean(g_L, -1) && lua_toboolean(g_L, -1)) { lua_pop(g_L, 1); return; }
//                 lua_pop(g_L, 1);
//             }
//             else lua_pop(g_L, 1);
//         }
//         else lua_pop(g_L, 1);
//     }
//     chain->callNext(mod, buffer);
// }

// /* 20. Mod_LoadStudioModel */
// void Mod_LoadStudioModel(IRehldsHook_Mod_LoadStudioModel *chain, model_t *mod, void *buffer)
// {
//     if (g_L) 
//     {
//         lua_getglobal(g_L, "Rehlds_Mod_LoadStudioModel");
//         if (lua_isfunction(g_L, -1)) 
//         {
//             lua_pushlightuserdata(g_L, mod);
//             lua_pushlightuserdata(g_L, buffer);

//             if (lua_pcall(g_L, 2, 1, 0) == 0)
//             {
//                 if (lua_isboolean(g_L, -1) && lua_toboolean(g_L, -1)) { lua_pop(g_L, 1); return; }
//                 lua_pop(g_L, 1);
//             }
//             else lua_pop(g_L, 1);
//         }
//         else lua_pop(g_L, 1);
//     }
//     chain->callNext(mod, buffer);
// }

// /* 21. SV_EmitEvents */
// void SV_EmitEvents(IRehldsHook_SV_EmitEvents *chain, IGameClient *cl, struct packet_entities_s *pack, sizebuf_t *msg)
// {
//     if (g_L) 
//     {
//         lua_getglobal(g_L, "Rehlds_SV_EmitEvents");
//         if (lua_isfunction(g_L, -1)) 
//         {
//             edict_t* pEnt = (cl) ? cl->GetEdict() : NULL;
//             lua_pushentity(g_L, pEnt);
//             lua_pushlightuserdata(g_L, pack);
//             lua_pushlightuserdata(g_L, msg);

//             if (lua_pcall(g_L, 3, 1, 0) == 0)
//             {
//                 if (lua_isboolean(g_L, -1) && lua_toboolean(g_L, -1)) { lua_pop(g_L, 1); return; }
//                 lua_pop(g_L, 1);
//             }
//             else lua_pop(g_L, 1);
//         }
//         else lua_pop(g_L, 1);
//     }
//     chain->callNext(cl, pack, msg);
// }

// /* 22. EV_PlayReliableEvent */
// void EV_PlayReliableEvent(IRehldsHook_EV_PlayReliableEvent *chain, IGameClient *cl, int entindex, unsigned short eventindex, float delay, struct event_args_s *pargs)
// {
//     int _entindex = entindex;
//     unsigned short _eventindex = eventindex;
//     float _delay = delay;

//     if (g_L) 
//     {
//         lua_getglobal(g_L, "Rehlds_EV_PlayReliableEvent");
//         if (lua_isfunction(g_L, -1)) 
//         {
//             edict_t* pEnt = (cl) ? cl->GetEdict() : NULL;
//             lua_pushentity(g_L, pEnt);
//             lua_pushinteger(g_L, _entindex);
//             lua_pushinteger(g_L, _eventindex);
//             lua_pushnumber(g_L, _delay);
//             lua_pushlightuserdata(g_L, pargs);

//             if (lua_pcall(g_L, 5, LUA_MULTRET, 0) == 0)
//             {
//                 int nRet = lua_gettop(g_L);
//                 if (nRet == 1 && lua_isboolean(g_L, -1) && lua_toboolean(g_L, -1)) {
//                     lua_pop(g_L, 1);
//                     return;
//                 }
//                 else if (nRet == 3) {
//                     if (lua_isnumber(g_L, -3)) _entindex = (int)lua_tointeger(g_L, -3);
//                     if (lua_isnumber(g_L, -2)) _eventindex = (unsigned short)lua_tointeger(g_L, -2);
//                     if (lua_isnumber(g_L, -1)) _delay = (float)lua_tonumber(g_L, -1);
//                     lua_pop(g_L, 3);
//                 }
//                 else lua_pop(g_L, nRet);
//             }
//             else lua_pop(g_L, 1);
//         }
//         else lua_pop(g_L, 1);
//     }
//     chain->callNext(cl, _entindex, _eventindex, _delay, pargs);
// }

// /* 23. SV_StartSound */
// void SV_StartSound(IRehldsHook_SV_StartSound *chain, int recipients, edict_t *entity, int channel, const char *sample, int volume, float attenuation, int flags, int pitch)
// {
//     int _recipients = recipients;
//     int _channel = channel;
//     const char* _sample = sample;
//     int _volume = volume;
//     float _attenuation = attenuation;
//     int _flags = flags;
//     int _pitch = pitch;
//     static char szSampleBuf[128];

//     if (g_L) 
//     {
//         lua_getglobal(g_L, "Rehlds_SV_StartSound");
//         if (lua_isfunction(g_L, -1)) 
//         {
//             lua_pushinteger(g_L, _recipients);
//             lua_pushentity(g_L, entity);
//             lua_pushinteger(g_L, _channel);
//             lua_pushstring(g_L, _sample);
//             lua_pushinteger(g_L, _volume);
//             lua_pushnumber(g_L, _attenuation);
//             lua_pushinteger(g_L, _flags);
//             lua_pushinteger(g_L, _pitch);

//             if (lua_pcall(g_L, 8, LUA_MULTRET, 0) == 0)
//             {
//                 int nRet = lua_gettop(g_L);
//                 if (nRet == 1 && lua_isboolean(g_L, -1) && lua_toboolean(g_L, -1)) {
//                     lua_pop(g_L, 1);
//                     return;
//                 }
//                 else if (nRet == 7) {
//                     if (lua_isnumber(g_L, -7)) _recipients = (int)lua_tointeger(g_L, -7);
//                     if (lua_isnumber(g_L, -6)) _channel = (int)lua_tointeger(g_L, -6);
//                     if (lua_isstring(g_L, -5)) { strncpy(szSampleBuf, lua_tostring(g_L, -5), 127); szSampleBuf[127]=0; _sample = szSampleBuf; }
//                     if (lua_isnumber(g_L, -4)) _volume = (int)lua_tointeger(g_L, -4);
//                     if (lua_isnumber(g_L, -3)) _attenuation = (float)lua_tonumber(g_L, -3);
//                     if (lua_isnumber(g_L, -2)) _flags = (int)lua_tointeger(g_L, -2);
//                     if (lua_isnumber(g_L, -1)) _pitch = (int)lua_tointeger(g_L, -1);
//                     lua_pop(g_L, 7);
//                 }
//                 else lua_pop(g_L, nRet);
//             }
//             else lua_pop(g_L, 1);
//         }
//         else lua_pop(g_L, 1);
//     }
//     chain->callNext(_recipients, entity, _channel, _sample, _volume, _attenuation, _flags, _pitch);
// }

// /* 24. PF_Remove_I */
// void PF_Remove_I(IRehldsHook_PF_Remove_I *chain, edict_t *edict)
// {
//     if (g_L) 
//     {
//         lua_getglobal(g_L, "Rehlds_PF_Remove_I");
//         if (lua_isfunction(g_L, -1)) 
//         {
//             lua_pushentity(g_L, edict);

//             if (lua_pcall(g_L, 1, 1, 0) == 0)
//             {
//                 if (lua_isboolean(g_L, -1) && lua_toboolean(g_L, -1)) { lua_pop(g_L, 1); return; }
//                 lua_pop(g_L, 1);
//             }
//             else lua_pop(g_L, 1);
//         }
//         else lua_pop(g_L, 1);
//     }
//     chain->callNext(edict);
// }

// /* 25. PF_BuildSoundMsg_I */
// void PF_BuildSoundMsg_I(IRehldsHook_PF_BuildSoundMsg_I *chain, edict_t *entity, int channel, const char *sample, float volume, float attenuation, int fFlags, int pitch, int msg_dest, int msg_type, const float *pOrigin, edict_t *ed)
// {
//     int _channel = channel;
//     const char* _sample = sample;
//     float _volume = volume;
//     float _attenuation = attenuation;
//     int _fFlags = fFlags;
//     int _pitch = pitch;
//     int _msg_dest = msg_dest;
//     int _msg_type = msg_type;
//     static char szSampleBuf[128];

//     if (g_L) 
//     {
//         lua_getglobal(g_L, "Rehlds_PF_BuildSoundMsg_I");
//         if (lua_isfunction(g_L, -1)) 
//         {
//             lua_pushentity(g_L, entity);
//             lua_pushinteger(g_L, _channel);
//             lua_pushstring(g_L, _sample);
//             lua_pushnumber(g_L, _volume);
//             lua_pushnumber(g_L, _attenuation);
//             lua_pushinteger(g_L, _fFlags);
//             lua_pushinteger(g_L, _pitch);
//             lua_pushinteger(g_L, _msg_dest);
//             lua_pushinteger(g_L, _msg_type);
//             lua_pushlightuserdata(g_L, (void*)pOrigin);
//             lua_pushentity(g_L, ed);

//             if (lua_pcall(g_L, 11, LUA_MULTRET, 0) == 0)
//             {
//                 int nRet = lua_gettop(g_L);
//                 if (nRet == 1 && lua_isboolean(g_L, -1) && lua_toboolean(g_L, -1)) {
//                     lua_pop(g_L, 1);
//                     return;
//                 }
//                 else if (nRet == 8) {
//                     if (lua_isnumber(g_L, -8)) _channel = (int)lua_tointeger(g_L, -8);
//                     if (lua_isstring(g_L, -7)) { strncpy(szSampleBuf, lua_tostring(g_L, -7), 127); szSampleBuf[127]=0; _sample = szSampleBuf; }
//                     if (lua_isnumber(g_L, -6)) _volume = (float)lua_tonumber(g_L, -6);
//                     if (lua_isnumber(g_L, -5)) _attenuation = (float)lua_tonumber(g_L, -5);
//                     if (lua_isnumber(g_L, -4)) _fFlags = (int)lua_tointeger(g_L, -4);
//                     if (lua_isnumber(g_L, -3)) _pitch = (int)lua_tointeger(g_L, -3);
//                     if (lua_isnumber(g_L, -2)) _msg_dest = (int)lua_tointeger(g_L, -2);
//                     if (lua_isnumber(g_L, -1)) _msg_type = (int)lua_tointeger(g_L, -1);
//                     lua_pop(g_L, 8);
//                 }
//                 else lua_pop(g_L, nRet);
//             }
//             else lua_pop(g_L, 1);
//         }
//         else lua_pop(g_L, 1);
//     }
//     chain->callNext(entity, _channel, _sample, _volume, _attenuation, _fFlags, _pitch, _msg_dest, _msg_type, pOrigin, ed);
// }

// /* 26. SV_WriteFullClientUpdate */
// void SV_WriteFullClientUpdate(IRehldsHook_SV_WriteFullClientUpdate *chain, IGameClient *cl, char *buf, size_t len, sizebuf_t *sb, IGameClient *receiver)
// {
//     if (g_L) 
//     {
//         lua_getglobal(g_L, "Rehlds_SV_WriteFullClientUpdate");
//         if (lua_isfunction(g_L, -1)) 
//         {
//             edict_t* pCl = (cl) ? cl->GetEdict() : NULL;
//             lua_pushentity(g_L, pCl);
//             lua_pushlightuserdata(g_L, buf);
//             lua_pushinteger(g_L, len);
//             lua_pushlightuserdata(g_L, sb);
//             edict_t* pRcvr = (receiver) ? receiver->GetEdict() : NULL;
//             lua_pushentity(g_L, pRcvr);

//             if (lua_pcall(g_L, 5, 1, 0) == 0)
//             {
//                 if (lua_isboolean(g_L, -1) && lua_toboolean(g_L, -1)) { lua_pop(g_L, 1); return; }
//                 lua_pop(g_L, 1);
//             }
//             else lua_pop(g_L, 1);
//         }
//         else lua_pop(g_L, 1);
//     }
//     chain->callNext(cl, buf, len, sb, receiver);
// }

// /* 27. SV_CheckConsistencyResponse */
// bool SV_CheckConsistencyResponse(IRehldsHook_SV_CheckConsistencyResponse *chain, IGameClient *cl, resource_t *res, uint32 check)
// {
//     uint32 _check = check;
//     if (g_L) 
//     {
//         lua_getglobal(g_L, "Rehlds_SV_CheckConsistencyResponse");
//         if (lua_isfunction(g_L, -1)) 
//         {
//             edict_t* pEnt = (cl) ? cl->GetEdict() : NULL;
//             lua_pushentity(g_L, pEnt);
//             lua_pushlightuserdata(g_L, res);
//             lua_pushinteger(g_L, _check);

//             if (lua_pcall(g_L, 3, LUA_MULTRET, 0) == 0)
//             {
//                 int nRet = lua_gettop(g_L);
//                 if (nRet == 1) {
//                     if (lua_isboolean(g_L, -1)) {
//                         bool ret = (bool)lua_toboolean(g_L, -1);
//                         lua_pop(g_L, 1);
//                         return ret;
//                     }
//                     lua_pop(g_L, 1);
//                 }
//                 else if (nRet == 2) {
//                     if (lua_isnumber(g_L, -1)) _check = (uint32)lua_tointeger(g_L, -1);
//                     lua_pop(g_L, 2);
//                 }
//                 else lua_pop(g_L, nRet);
//             }
//             else lua_pop(g_L, 1);
//         }
//         else lua_pop(g_L, 1);
//     }
//     return chain->callNext(cl, res, _check);
// }

// /* 28. SV_DropClient */
// void SV_DropClient(IRehldsHook_SV_DropClient *chain, IGameClient *cl, bool crash, const char *fmt)
// {
//     bool _crash = crash;
//     const char* _fmt = fmt;
//     static char szFmtBuf[256];

//     if (g_L) 
//     {
//         lua_getglobal(g_L, "Rehlds_SV_DropClient");
//         if (lua_isfunction(g_L, -1)) 
//         {
//             edict_t* pEnt = (cl) ? cl->GetEdict() : NULL;
//             lua_pushentity(g_L, pEnt);
//             lua_pushboolean(g_L, _crash);
//             lua_pushstring(g_L, _fmt);

//             if (lua_pcall(g_L, 3, LUA_MULTRET, 0) == 0)
//             {
//                 int nRet = lua_gettop(g_L);
//                 if (nRet == 1 && lua_isboolean(g_L, -1) && lua_toboolean(g_L, -1)) {
//                     lua_pop(g_L, 1);
//                     return;
//                 }
//                 else if (nRet == 2) {
//                     if (lua_isboolean(g_L, -2)) _crash = (bool)lua_toboolean(g_L, -2);
//                     if (lua_isstring(g_L, -1)) { strncpy(szFmtBuf, lua_tostring(g_L, -1), 255); szFmtBuf[255]=0; _fmt = szFmtBuf; }
//                     lua_pop(g_L, 2);
//                 }
//                 else lua_pop(g_L, nRet);
//             }
//             else lua_pop(g_L, 1);
//         }
//         else lua_pop(g_L, 1);
//     }
//     chain->callNext(cl, _crash, _fmt);
// }

// /* 29. SV_ActivateServer */
// void SV_ActivateServer(IRehldsHook_SV_ActivateServer *chain, int maxclients)
// {
//     int _max = maxclients;

//     if (g_L) 
//     {
//         lua_getglobal(g_L, "Rehlds_SV_ActivateServer");
//         if (lua_isfunction(g_L, -1)) 
//         {
//             lua_pushinteger(g_L, _max);

//             if (lua_pcall(g_L, 1, LUA_MULTRET, 0) == 0)
//             {
//                 int nRet = lua_gettop(g_L);
//                 if (nRet == 1) {
//                     if (lua_isboolean(g_L, -1) && lua_toboolean(g_L, -1)) { lua_pop(g_L, 1); return; }
//                     if (lua_isnumber(g_L, -1)) _max = (int)lua_tointeger(g_L, -1);
//                     lua_pop(g_L, 1);
//                 }
//                 else lua_pop(g_L, nRet);
//             }
//             else lua_pop(g_L, 1);
//         }
//         else lua_pop(g_L, 1);
//     }
//     chain->callNext(_max);
// }

// /* 30. SV_WriteVoiceCodec */
// void SV_WriteVoiceCodec(IRehldsHook_SV_WriteVoiceCodec *chain, sizebuf_t *sb)
// {
//     if (g_L) 
//     {
//         lua_getglobal(g_L, "Rehlds_SV_WriteVoiceCodec");
//         if (lua_isfunction(g_L, -1)) 
//         {
//             lua_pushlightuserdata(g_L, sb);

//             if (lua_pcall(g_L, 1, 1, 0) == 0)
//             {
//                 if (lua_isboolean(g_L, -1) && lua_toboolean(g_L, -1)) { lua_pop(g_L, 1); return; }
//                 lua_pop(g_L, 1);
//             }
//             else lua_pop(g_L, 1);
//         }
//         else lua_pop(g_L, 1);
//     }
//     chain->callNext(sb);
// }

// /* 31. Steam_GSGetSteamID */
// uint64 Steam_GSGetSteamID(IRehldsHook_Steam_GSGetSteamID *chain)
// {
//     if (g_L) 
//     {
//         lua_getglobal(g_L, "Rehlds_Steam_GSGetSteamID");
//         if (lua_isfunction(g_L, -1)) 
//         {
//             if (lua_pcall(g_L, 0, 1, 0) == 0)
//             {
//                 if (lua_isstring(g_L, -1)) {
//                     // Lua 5.1 不支持 64 位整数，通常以字符串传输。需自行实现 strtoull 或类似转换。
//                     // 这里为了兼容性，假设不修改或返回原值。若需修改需引入 64 位转换库。
//                     // uint64 ret = _strtoui64(lua_tostring(g_L, -1), NULL, 10);
//                     // lua_pop(g_L, 1);
//                     // return ret;
//                 }
//                 lua_pop(g_L, 1);
//             }
//             else lua_pop(g_L, 1);
//         }
//         else lua_pop(g_L, 1);
//     }
//     return chain->callNext();
// }

// /* 32. SV_TransferConsistencyInfo */
// int SV_TransferConsistencyInfo(IRehldsHook_SV_TransferConsistencyInfo *chain)
// {
//     if (g_L) 
//     {
//         lua_getglobal(g_L, "Rehlds_SV_TransferConsistencyInfo");
//         if (lua_isfunction(g_L, -1)) 
//         {
//             if (lua_pcall(g_L, 0, 1, 0) == 0)
//             {
//                 if (lua_isnumber(g_L, -1)) {
//                     int ret = (int)lua_tointeger(g_L, -1);
//                     lua_pop(g_L, 1);
//                     return ret;
//                 }
//                 lua_pop(g_L, 1);
//             }
//             else lua_pop(g_L, 1);
//         }
//         else lua_pop(g_L, 1);
//     }
//     return chain->callNext();
// }

// /* 33. Steam_GSBUpdateUserData */
// bool Steam_GSBUpdateUserData(IRehldsHook_Steam_GSBUpdateUserData *chain, uint64 steamID, const char *pchPlayerName, uint32 uScore)
// {
//     const char* _name = pchPlayerName;
//     uint32 _score = uScore;
//     static char szNameBuf[128];
//     static char szSteamID[32]; // For passing uint64 to Lua as string

//     if (g_L) 
//     {
//         lua_getglobal(g_L, "Rehlds_Steam_GSBUpdateUserData");
//         if (lua_isfunction(g_L, -1)) 
//         {
//             sprintf(szSteamID, "%llu", steamID);
//             lua_pushstring(g_L, szSteamID);
//             lua_pushstring(g_L, _name);
//             lua_pushinteger(g_L, _score);

//             if (lua_pcall(g_L, 3, LUA_MULTRET, 0) == 0)
//             {
//                 int nRet = lua_gettop(g_L);
//                 if (nRet == 1) {
//                     if (lua_isboolean(g_L, -1)) {
//                         bool ret = (bool)lua_toboolean(g_L, -1);
//                         lua_pop(g_L, 1);
//                         return ret;
//                     }
//                     lua_pop(g_L, 1);
//                 }
//                 else if (nRet == 2) {
//                     // Update name, score. SteamID ignored.
//                     if (lua_isstring(g_L, -2)) { strncpy(szNameBuf, lua_tostring(g_L, -2), 127); szNameBuf[127]=0; _name = szNameBuf; }
//                     if (lua_isnumber(g_L, -1)) _score = (uint32)lua_tointeger(g_L, -1);
//                     lua_pop(g_L, 2);
//                 }
//                 else lua_pop(g_L, nRet);
//             }
//             else lua_pop(g_L, 1);
//         }
//         else lua_pop(g_L, 1);
//     }
//     return chain->callNext(steamID, _name, _score);
// }

// /* 34. Cvar_DirectSet */
// void Cvar_DirectSet(IRehldsHook_Cvar_DirectSet *chain, struct cvar_s *var, const char *value)
// {
//     const char* _val = value;
//     static char szValBuf[256];

//     if (g_L) 
//     {
//         lua_getglobal(g_L, "Rehlds_Cvar_DirectSet");
//         if (lua_isfunction(g_L, -1)) 
//         {
//             lua_pushlightuserdata(g_L, var);
//             lua_pushstring(g_L, _val);

//             if (lua_pcall(g_L, 2, LUA_MULTRET, 0) == 0)
//             {
//                 int nRet = lua_gettop(g_L);
//                 if (nRet == 1 && lua_isboolean(g_L, -1) && lua_toboolean(g_L, -1)) {
//                     lua_pop(g_L, 1);
//                     return;
//                 }
//                 else if (nRet == 1 && lua_isstring(g_L, -1)) {
//                     strncpy(szValBuf, lua_tostring(g_L, -1), 255); szValBuf[255]=0; _val = szValBuf;
//                     lua_pop(g_L, 1);
//                 }
//                 else lua_pop(g_L, nRet);
//             }
//             else lua_pop(g_L, 1);
//         }
//         else lua_pop(g_L, 1);
//     }
//     chain->callNext(var, _val);
// }

// /* 35. SV_EstablishTimeBase */
// void SV_EstablishTimeBase(IRehldsHook_SV_EstablishTimeBase *chain, IGameClient *cl, struct usercmd_s *cmd, int funcs, int movevar, int vec)
// {
//     if (g_L) 
//     {
//         lua_getglobal(g_L, "Rehlds_SV_EstablishTimeBase");
//         if (lua_isfunction(g_L, -1)) 
//         {
//             edict_t* pEnt = (cl) ? cl->GetEdict() : NULL;
//             lua_pushentity(g_L, pEnt);
//             lua_pushlightuserdata(g_L, cmd);
//             lua_pushinteger(g_L, funcs);
//             lua_pushinteger(g_L, movevar);
//             lua_pushinteger(g_L, vec);

//             if (lua_pcall(g_L, 5, 1, 0) == 0)
//             {
//                 if (lua_isboolean(g_L, -1) && lua_toboolean(g_L, -1)) { lua_pop(g_L, 1); return; }
//                 lua_pop(g_L, 1);
//             }
//             else lua_pop(g_L, 1);
//         }
//         else lua_pop(g_L, 1);
//     }
//     chain->callNext(cl, cmd, funcs, movevar, vec);
// }

// /* 36. SV_Spawn_f */
// void SV_Spawn_f(IRehldsHook_SV_Spawn_f *chain)
// {
//     if (g_L) 
//     {
//         lua_getglobal(g_L, "Rehlds_SV_Spawn_f");
//         if (lua_isfunction(g_L, -1)) 
//         {
//             if (lua_pcall(g_L, 0, 1, 0) == 0)
//             {
//                 if (lua_isboolean(g_L, -1) && lua_toboolean(g_L, -1)) { lua_pop(g_L, 1); return; }
//                 lua_pop(g_L, 1);
//             }
//             else lua_pop(g_L, 1);
//         }
//         else lua_pop(g_L, 1);
//     }
//     chain->callNext();
// }

// /* 37. SV_CreatePacketEntities */
// int SV_CreatePacketEntities(IRehldsHook_SV_CreatePacketEntities *chain, enum sv_delta_s delta, IGameClient *cl, struct packet_entities_s *pack, struct sizebuf_s *msg)
// {
//     if (g_L) 
//     {
//         lua_getglobal(g_L, "Rehlds_SV_CreatePacketEntities");
//         if (lua_isfunction(g_L, -1)) 
//         {
//             lua_pushinteger(g_L, (int)delta);
//             edict_t* pEnt = (cl) ? cl->GetEdict() : NULL;
//             lua_pushentity(g_L, pEnt);
//             lua_pushlightuserdata(g_L, pack);
//             lua_pushlightuserdata(g_L, msg);

//             if (lua_pcall(g_L, 4, 1, 0) == 0)
//             {
//                 if (lua_isnumber(g_L, -1)) {
//                     int ret = (int)lua_tointeger(g_L, -1);
//                     lua_pop(g_L, 1);
//                     return ret;
//                 }
//                 lua_pop(g_L, 1);
//             }
//             else lua_pop(g_L, 1);
//         }
//         else lua_pop(g_L, 1);
//     }
//     return chain->callNext(delta, cl, pack, msg);
// }

// /* 38. SV_EmitSound2 */
// bool SV_EmitSound2(IRehldsHook_SV_EmitSound2 *chain, edict_t *entity, IGameClient *receiver, int channel, const char *sample, float volume, float attenuation, int flags, int pitch, int emitFlags, const float *pOrigin)
// {
//     int _channel = channel;
//     const char* _sample = sample;
//     float _volume = volume;
//     float _attenuation = attenuation;
//     int _flags = flags;
//     int _pitch = pitch;
//     int _emitFlags = emitFlags;
//     static char szSampleBuf[128];

//     if (g_L) 
//     {
//         lua_getglobal(g_L, "Rehlds_SV_EmitSound2");
//         if (lua_isfunction(g_L, -1)) 
//         {
//             lua_pushentity(g_L, entity);
//             edict_t* pRcvr = (receiver) ? receiver->GetEdict() : NULL;
//             lua_pushentity(g_L, pRcvr);
//             lua_pushinteger(g_L, _channel);
//             lua_pushstring(g_L, _sample);
//             lua_pushnumber(g_L, _volume);
//             lua_pushnumber(g_L, _attenuation);
//             lua_pushinteger(g_L, _flags);
//             lua_pushinteger(g_L, _pitch);
//             lua_pushinteger(g_L, _emitFlags);
//             lua_pushlightuserdata(g_L, (void*)pOrigin);

//             if (lua_pcall(g_L, 10, LUA_MULTRET, 0) == 0)
//             {
//                 int nRet = lua_gettop(g_L);
//                 if (nRet == 1 && lua_isboolean(g_L, -1)) {
//                     bool ret = (bool)lua_toboolean(g_L, -1);
//                     lua_pop(g_L, 1);
//                     return ret;
//                 }
//                 else if (nRet == 7) {
//                     if (lua_isnumber(g_L, -7)) _channel = (int)lua_tointeger(g_L, -7);
//                     if (lua_isstring(g_L, -6)) { strncpy(szSampleBuf, lua_tostring(g_L, -6), 127); szSampleBuf[127]=0; _sample = szSampleBuf; }
//                     if (lua_isnumber(g_L, -5)) _volume = (float)lua_tonumber(g_L, -5);
//                     if (lua_isnumber(g_L, -4)) _attenuation = (float)lua_tonumber(g_L, -4);
//                     if (lua_isnumber(g_L, -3)) _flags = (int)lua_tointeger(g_L, -3);
//                     if (lua_isnumber(g_L, -2)) _pitch = (int)lua_tointeger(g_L, -2);
//                     if (lua_isnumber(g_L, -1)) _emitFlags = (int)lua_tointeger(g_L, -1);
//                     lua_pop(g_L, 7);
//                 }
//                 else lua_pop(g_L, nRet);
//             }
//             else lua_pop(g_L, 1);
//         }
//         else lua_pop(g_L, 1);
//     }
//     return chain->callNext(entity, receiver, _channel, _sample, _volume, _attenuation, _flags, _pitch, _emitFlags, pOrigin);
// }

// /* 39. CreateFakeClient */
// edict_t *CreateFakeClient(IRehldsHook_CreateFakeClient *chain, const char *netname)
// {
//     const char* _netname = netname;
//     static char szNameBuf[128];

//     if (g_L) 
//     {
//         lua_getglobal(g_L, "Rehlds_CreateFakeClient");
//         if (lua_isfunction(g_L, -1)) 
//         {
//             lua_pushstring(g_L, _netname);

//             if (lua_pcall(g_L, 1, LUA_MULTRET, 0) == 0)
//             {
//                 int nRet = lua_gettop(g_L);
//                 if (nRet == 1) {
//                     if (lua_isstring(g_L, -1)) {
//                         strncpy(szNameBuf, lua_tostring(g_L, -1), 127); szNameBuf[127]=0; _netname = szNameBuf;
//                     } else if (lua_isuserdata(g_L, -1) || lua_isnil(g_L, -1)) {
//                         // Return Entity or Nil to supercede
//                         lua_pop(g_L, 1);
//                         return NULL; // Assuming conversion is handled if needed
//                     }
//                     lua_pop(g_L, 1);
//                 }
//                 else lua_pop(g_L, nRet);
//             }
//             else lua_pop(g_L, 1);
//         }
//         else lua_pop(g_L, 1);
//     }
//     return chain->callNext(_netname);
// }

// /* 40. SV_CheckConnectionLessRateLimits */
// bool SV_CheckConnectionLessRateLimits(IRehldsHook_SV_CheckConnectionLessRateLimits *chain, netadr_t &adr, const uint8_t *data, int len)
// {
//     if (g_L) 
//     {
//         lua_getglobal(g_L, "Rehlds_SV_CheckConnectionLessRateLimits");
//         if (lua_isfunction(g_L, -1)) 
//         {
//             lua_pushlightuserdata(g_L, (void*)&adr);
//             lua_pushlightuserdata(g_L, (void*)data);
//             lua_pushinteger(g_L, len);

//             if (lua_pcall(g_L, 3, 1, 0) == 0)
//             {
//                 if (lua_isboolean(g_L, -1)) {
//                     bool ret = (bool)lua_toboolean(g_L, -1);
//                     lua_pop(g_L, 1);
//                     return ret;
//                 }
//                 lua_pop(g_L, 1);
//             }
//             else lua_pop(g_L, 1);
//         }
//         else lua_pop(g_L, 1);
//     }
//     return chain->callNext(adr, data, len);
// }
// /* 41. SV_Frame */
// void SV_Frame(IRehldsHook_SV_Frame *chain)
// {
//     if (g_L) 
//     {
//         lua_getglobal(g_L, "Rehlds_SV_Frame");
//         if (lua_isfunction(g_L, -1)) 
//         {
//             if (lua_pcall(g_L, 0, 1, 0) == 0)
//             {
//                 if (lua_isboolean(g_L, -1) && lua_toboolean(g_L, -1)) { lua_pop(g_L, 1); return; }
//                 lua_pop(g_L, 1);
//             }
//             else lua_pop(g_L, 1);
//         }
//         else lua_pop(g_L, 1);
//     }
//     chain->callNext();
// }

// /* 42. SV_ShouldSendConsistencyList */
// bool SV_ShouldSendConsistencyList(IRehldsHook_SV_ShouldSendConsistencyList *chain, IGameClient *cl, bool bSend)
// {
//     bool _bSend = bSend;

//     if (g_L) 
//     {
//         lua_getglobal(g_L, "Rehlds_SV_ShouldSendConsistencyList");
//         if (lua_isfunction(g_L, -1)) 
//         {
//             edict_t* pEnt = (cl) ? cl->GetEdict() : NULL;
//             lua_pushentity(g_L, pEnt);
//             lua_pushboolean(g_L, _bSend);

//             if (lua_pcall(g_L, 2, LUA_MULTRET, 0) == 0)
//             {
//                 int nRet = lua_gettop(g_L);
//                 if (nRet == 1) {
//                     if (lua_isboolean(g_L, -1)) {
//                         bool ret = (bool)lua_toboolean(g_L, -1);
//                         lua_pop(g_L, 1);
//                         return ret;
//                     }
//                     lua_pop(g_L, 1);
//                 }
//                 else if (nRet == 2) {
//                     if (lua_isboolean(g_L, -1)) _bSend = (bool)lua_toboolean(g_L, -1);
//                     lua_pop(g_L, 2);
//                 }
//                 else lua_pop(g_L, nRet);
//             }
//             else lua_pop(g_L, 1);
//         }
//         else lua_pop(g_L, 1);
//     }
//     return chain->callNext(cl, _bSend);
// }

// /* 43. GetEntityInit */
// ENTITYINIT GetEntityInit(IRehldsHook_GetEntityInit *chain, char *pszClassName)
// {
//     // ENTITYINIT 是函数指针 typedef void (*ENTITYINIT)( entvars_t *pev );
//     // Lua 无法直接返回 C++ 函数指针，因此仅支持修改 ClassName 参数
//     if (g_L) 
//     {
//         lua_getglobal(g_L, "Rehlds_GetEntityInit");
//         if (lua_isfunction(g_L, -1)) 
//         {
//             lua_pushstring(g_L, pszClassName);

//             if (lua_pcall(g_L, 1, LUA_MULTRET, 0) == 0)
//             {
//                 int nRet = lua_gettop(g_L);
//                 if (nRet == 1) {
//                     if (lua_isstring(g_L, -1)) {
//                         strncpy(pszClassName, lua_tostring(g_L, -1), 63); // 假设缓冲区足够
//                     }
//                     // 如果返回 nil，可以考虑 return NULL (supercede)，但 ENTITYINIT 返回 NULL 可能会导致引擎崩溃或默认处理
//                     else if (lua_isnil(g_L, -1)) {
//                         lua_pop(g_L, 1);
//                         return NULL; 
//                     }
//                     lua_pop(g_L, 1);
//                 }
//                 else lua_pop(g_L, nRet);
//             }
//             else lua_pop(g_L, 1);
//         }
//         else lua_pop(g_L, 1);
//     }
//     return chain->callNext(pszClassName);
// }

// /* 44. SV_EmitPings */
// void SV_EmitPings(IRehldsHook_SV_EmitPings *chain, IGameClient *cl, sizebuf_t *msg)
// {
//     if (g_L) 
//     {
//         lua_getglobal(g_L, "Rehlds_SV_EmitPings");
//         if (lua_isfunction(g_L, -1)) 
//         {
//             edict_t* pEnt = (cl) ? cl->GetEdict() : NULL;
//             lua_pushentity(g_L, pEnt);
//             lua_pushlightuserdata(g_L, msg);

//             if (lua_pcall(g_L, 2, 1, 0) == 0)
//             {
//                 if (lua_isboolean(g_L, -1) && lua_toboolean(g_L, -1)) { lua_pop(g_L, 1); return; }
//                 lua_pop(g_L, 1);
//             }
//             else lua_pop(g_L, 1);
//         }
//         else lua_pop(g_L, 1);
//     }
//     chain->callNext(cl, msg);
// }

/* 45. ED_Alloc */
edict_t *ED_Alloc(IRehldsHook_ED_Alloc *chain)
{
    edict_t *ed = chain->callNext();
    if (g_L) 
    {
        lua_getglobal(g_L, "Rehlds_ED_Alloc");
        if (lua_isfunction(g_L, -1)) 
        {
            lua_pushentity(g_L, ed);
            if (lua_pcall(g_L, 1, 1, 0) == 0)
            {
                if (lua_isboolean(g_L, -1) && lua_toboolean(g_L, -1)) { lua_pop(g_L, 1); return ed; }
                lua_pop(g_L, 1);
            }
            else lua_pop(g_L, 1);
        }
        else lua_pop(g_L, 1);
    }
    return ed;
}

/* 46. ED_Free */
void ED_Free(IRehldsHook_ED_Free *chain, edict_t *ed)
{
    if (g_L) 
    {
        lua_getglobal(g_L, "Rehlds_ED_Free");
        if (lua_isfunction(g_L, -1)) 
        {
            lua_pushentity(g_L, ed);

            if (lua_pcall(g_L, 1, 1, 0) == 0)
            {
                if (lua_isboolean(g_L, -1) && lua_toboolean(g_L, -1)) { lua_pop(g_L, 1); return; }
                lua_pop(g_L, 1);
            }
            else lua_pop(g_L, 1);
        }
        else lua_pop(g_L, 1);
    }
    chain->callNext(ed);
}

// /* 47. Con_Printf */
// void Con_Printf(IRehldsHook_Con_Printf *chain, const char *fmt)
// {
//     const char* _fmt = fmt;
//     static char szFmtBuf[1024];

//     if (g_L) 
//     {
//         lua_getglobal(g_L, "Rehlds_Con_Printf");
//         if (lua_isfunction(g_L, -1)) 
//         {
//             lua_pushstring(g_L, _fmt);

//             if (lua_pcall(g_L, 1, LUA_MULTRET, 0) == 0)
//             {
//                 int nRet = lua_gettop(g_L);
//                 if (nRet == 1) {
//                     if (lua_isboolean(g_L, -1) && lua_toboolean(g_L, -1)) {
//                         lua_pop(g_L, 1);
//                         return;
//                     }
//                     if (lua_isstring(g_L, -1)) {
//                         strncpy(szFmtBuf, lua_tostring(g_L, -1), 1023); szFmtBuf[1023]=0;
//                         _fmt = szFmtBuf;
//                     }
//                     lua_pop(g_L, 1);
//                 }
//                 else lua_pop(g_L, nRet);
//             }
//             else lua_pop(g_L, 1);
//         }
//         else lua_pop(g_L, 1);
//     }
//     chain->callNext(_fmt);
// }

// /* 48. SV_CheckUserInfo */
// int SV_CheckUserInfo(IRehldsHook_SV_CheckUserInfo *chain, netadr_t *adr, char *infobuffer, qboolean nodedup, int duplicateIndex, char *szSteamID)
// {
//     qboolean _nodedup = nodedup;
//     int _dupIdx = duplicateIndex;

//     if (g_L) 
//     {
//         lua_getglobal(g_L, "Rehlds_SV_CheckUserInfo");
//         if (lua_isfunction(g_L, -1)) 
//         {
//             lua_pushlightuserdata(g_L, adr);
//             lua_pushstring(g_L, infobuffer);
//             lua_pushinteger(g_L, _nodedup);
//             lua_pushinteger(g_L, _dupIdx);
//             lua_pushstring(g_L, szSteamID);

//             if (lua_pcall(g_L, 5, LUA_MULTRET, 0) == 0)
//             {
//                 int nRet = lua_gettop(g_L);
//                 if (nRet == 1 && lua_isnumber(g_L, -1)) {
//                     int ret = (int)lua_tointeger(g_L, -1);
//                     lua_pop(g_L, 1);
//                     return ret;
//                 }
//                 else if (nRet == 4) {
//                     // Update args: infobuffer, nodedup, duplicateIndex, steamid
//                     if (lua_isstring(g_L, -4)) strncpy(infobuffer, lua_tostring(g_L, -4), 255);
//                     if (lua_isnumber(g_L, -3)) _nodedup = (qboolean)lua_tointeger(g_L, -3);
//                     if (lua_isnumber(g_L, -2)) _dupIdx = (int)lua_tointeger(g_L, -2);
//                     if (lua_isstring(g_L, -1)) strncpy(szSteamID, lua_tostring(g_L, -1), 63);
//                     lua_pop(g_L, 4);
//                 }
//                 else lua_pop(g_L, nRet);
//             }
//             else lua_pop(g_L, 1);
//         }
//         else lua_pop(g_L, 1);
//     }
//     return chain->callNext(adr, infobuffer, _nodedup, _dupIdx, szSteamID);
// }

// /* 49. PF_precache_generic_I */
// int PF_precache_generic_I(IRehldsHook_PF_precache_generic_I *chain, const char *s)
// {
//     const char* _s = s;
//     static char szBuf[128];

//     if (g_L) 
//     {
//         lua_getglobal(g_L, "Rehlds_PF_precache_generic_I");
//         if (lua_isfunction(g_L, -1)) 
//         {
//             lua_pushstring(g_L, _s);

//             if (lua_pcall(g_L, 1, LUA_MULTRET, 0) == 0)
//             {
//                 int nRet = lua_gettop(g_L);
//                 if (nRet == 1) {
//                     if (lua_isnumber(g_L, -1)) {
//                         int ret = (int)lua_tointeger(g_L, -1);
//                         lua_pop(g_L, 1);
//                         return ret;
//                     }
//                     if (lua_isstring(g_L, -1)) {
//                         strncpy(szBuf, lua_tostring(g_L, -1), 127); szBuf[127]=0; _s = szBuf;
//                     }
//                     lua_pop(g_L, 1);
//                 }
//                 else lua_pop(g_L, nRet);
//             }
//             else lua_pop(g_L, 1);
//         }
//         else lua_pop(g_L, 1);
//     }
//     return chain->callNext(_s);
// }

// /* 50. PF_precache_model_I */
// int PF_precache_model_I(IRehldsHook_PF_precache_model_I *chain, const char *s)
// {
//     const char* _s = s;
//     static char szBuf[128];

//     if (g_L) 
//     {
//         lua_getglobal(g_L, "Rehlds_PF_precache_model_I");
//         if (lua_isfunction(g_L, -1)) 
//         {
//             lua_pushstring(g_L, _s);

//             if (lua_pcall(g_L, 1, LUA_MULTRET, 0) == 0)
//             {
//                 int nRet = lua_gettop(g_L);
//                 if (nRet == 1) {
//                     if (lua_isnumber(g_L, -1)) {
//                         int ret = (int)lua_tointeger(g_L, -1);
//                         lua_pop(g_L, 1);
//                         return ret;
//                     }
//                     if (lua_isstring(g_L, -1)) {
//                         strncpy(szBuf, lua_tostring(g_L, -1), 127); szBuf[127]=0; _s = szBuf;
//                     }
//                     lua_pop(g_L, 1);
//                 }
//                 else lua_pop(g_L, nRet);
//             }
//             else lua_pop(g_L, 1);
//         }
//         else lua_pop(g_L, 1);
//     }
//     return chain->callNext(_s);
// }

// /* 51. PF_precache_sound_I */
// int PF_precache_sound_I(IRehldsHook_PF_precache_sound_I *chain, const char *s)
// {
//     const char* _s = s;
//     static char szBuf[128];

//     if (g_L) 
//     {
//         lua_getglobal(g_L, "Rehlds_PF_precache_sound_I");
//         if (lua_isfunction(g_L, -1)) 
//         {
//             lua_pushstring(g_L, _s);

//             if (lua_pcall(g_L, 1, LUA_MULTRET, 0) == 0)
//             {
//                 int nRet = lua_gettop(g_L);
//                 if (nRet == 1) {
//                     if (lua_isnumber(g_L, -1)) {
//                         int ret = (int)lua_tointeger(g_L, -1);
//                         lua_pop(g_L, 1);
//                         return ret;
//                     }
//                     if (lua_isstring(g_L, -1)) {
//                         strncpy(szBuf, lua_tostring(g_L, -1), 127); szBuf[127]=0; _s = szBuf;
//                     }
//                     lua_pop(g_L, 1);
//                 }
//                 else lua_pop(g_L, nRet);
//             }
//             else lua_pop(g_L, 1);
//         }
//         else lua_pop(g_L, 1);
//     }
//     return chain->callNext(_s);
// }

// /* 52. EV_Precache */
// unsigned short EV_Precache(IRehldsHook_EV_Precache *chain, int type, const char *psz)
// {
//     int _type = type;
//     const char* _psz = psz;
//     static char szBuf[128];

//     if (g_L) 
//     {
//         lua_getglobal(g_L, "Rehlds_EV_Precache");
//         if (lua_isfunction(g_L, -1)) 
//         {
//             lua_pushinteger(g_L, _type);
//             lua_pushstring(g_L, _psz);

//             if (lua_pcall(g_L, 2, LUA_MULTRET, 0) == 0)
//             {
//                 int nRet = lua_gettop(g_L);
//                 if (nRet == 1 && lua_isnumber(g_L, -1)) {
//                     unsigned short ret = (unsigned short)lua_tointeger(g_L, -1);
//                     lua_pop(g_L, 1);
//                     return ret;
//                 }
//                 else if (nRet == 2) {
//                     if (lua_isnumber(g_L, -2)) _type = (int)lua_tointeger(g_L, -2);
//                     if (lua_isstring(g_L, -1)) { strncpy(szBuf, lua_tostring(g_L, -1), 127); szBuf[127]=0; _psz = szBuf; }
//                     lua_pop(g_L, 2);
//                 }
//                 else lua_pop(g_L, nRet);
//             }
//             else lua_pop(g_L, 1);
//         }
//         else lua_pop(g_L, 1);
//     }
//     return chain->callNext(_type, _psz);
// }

// /* 53. SV_AddResource */
// void SV_AddResource(IRehldsHook_SV_AddResource *chain, resourcetype_t type, const char *name, int size, unsigned char flags, int index)
// {
//     int _type = (int)type;
//     const char* _name = name;
//     int _size = size;
//     int _flags = (int)flags;
//     int _index = index;
//     static char szNameBuf[128];

//     if (g_L) 
//     {
//         lua_getglobal(g_L, "Rehlds_SV_AddResource");
//         if (lua_isfunction(g_L, -1)) 
//         {
//             lua_pushinteger(g_L, _type);
//             lua_pushstring(g_L, _name);
//             lua_pushinteger(g_L, _size);
//             lua_pushinteger(g_L, _flags);
//             lua_pushinteger(g_L, _index);

//             if (lua_pcall(g_L, 5, LUA_MULTRET, 0) == 0)
//             {
//                 int nRet = lua_gettop(g_L);
//                 if (nRet == 1 && lua_isboolean(g_L, -1) && lua_toboolean(g_L, -1)) {
//                     lua_pop(g_L, 1);
//                     return;
//                 }
//                 else if (nRet == 5) {
//                     if (lua_isnumber(g_L, -5)) _type = (int)lua_tointeger(g_L, -5);
//                     if (lua_isstring(g_L, -4)) { strncpy(szNameBuf, lua_tostring(g_L, -4), 127); szNameBuf[127]=0; _name = szNameBuf; }
//                     if (lua_isnumber(g_L, -3)) _size = (int)lua_tointeger(g_L, -3);
//                     if (lua_isnumber(g_L, -2)) _flags = (int)lua_tointeger(g_L, -2);
//                     if (lua_isnumber(g_L, -1)) _index = (int)lua_tointeger(g_L, -1);
//                     lua_pop(g_L, 5);
//                 }
//                 else lua_pop(g_L, nRet);
//             }
//             else lua_pop(g_L, 1);
//         }
//         else lua_pop(g_L, 1);
//     }
//     chain->callNext((resourcetype_t)_type, _name, _size, (unsigned char)_flags, _index);
// }

// /* 54. SV_ClientPrintf */
// void SV_ClientPrintf(IRehldsHook_SV_ClientPrintf *chain, const char *fmt)
// {
//     const char* _fmt = fmt;
//     static char szBuf[1024];

//     if (g_L) 
//     {
//         lua_getglobal(g_L, "Rehlds_SV_ClientPrintf");
//         if (lua_isfunction(g_L, -1)) 
//         {
//             lua_pushstring(g_L, _fmt);

//             if (lua_pcall(g_L, 1, LUA_MULTRET, 0) == 0)
//             {
//                 int nRet = lua_gettop(g_L);
//                 if (nRet == 1) {
//                     if (lua_isboolean(g_L, -1) && lua_toboolean(g_L, -1)) {
//                         lua_pop(g_L, 1);
//                         return;
//                     }
//                     if (lua_isstring(g_L, -1)) {
//                         strncpy(szBuf, lua_tostring(g_L, -1), 1023); szBuf[1023]=0; _fmt = szBuf;
//                     }
//                     lua_pop(g_L, 1);
//                 }
//                 else lua_pop(g_L, nRet);
//             }
//             else lua_pop(g_L, 1);
//         }
//         else lua_pop(g_L, 1);
//     }
//     chain->callNext(_fmt);
// }

// /* 55. SV_AllowPhysent */
// bool SV_AllowPhysent(IRehldsHook_SV_AllowPhysent *chain, edict_t *pEdict, edict_t *pPhysent)
// {
//     if (g_L) 
//     {
//         lua_getglobal(g_L, "Rehlds_SV_AllowPhysent");
//         if (lua_isfunction(g_L, -1)) 
//         {
//             lua_pushentity(g_L, pEdict);
//             lua_pushentity(g_L, pPhysent);

//             if (lua_pcall(g_L, 2, 1, 0) == 0)
//             {
//                 if (lua_isboolean(g_L, -1)) {
//                     bool ret = (bool)lua_toboolean(g_L, -1);
//                     lua_pop(g_L, 1);
//                     return ret;
//                 }
//                 lua_pop(g_L, 1);
//             }
//             else lua_pop(g_L, 1);
//         }
//         else lua_pop(g_L, 1);
//     }
//     return chain->callNext(pEdict, pPhysent);
// }

// // /* 56. SV_SendResources */
// // void SV_SendResources(IRehldsHook_SV_SendResources *chain, sizebuf_t *msg)
// // {
// //     if (g_L) 
// //     {
// //         lua_getglobal(g_L, "Rehlds_SV_SendResources");
// //         if (lua_isfunction(g_L, -1)) 
// //         {
// //             lua_pushlightuserdata(g_L, msg);

// //             if (lua_pcall(g_L, 1, 1, 0) == 0)
//             {
//                 if (lua_isboolean(g_L, -1) && lua_toboolean(g_L, -1)) { lua_pop(g_L, 1); return; }
//                 lua_pop(g_L, 1);
//             }
//             else lua_pop(g_L, 1);
//         }
//         else lua_pop(g_L, 1);
//     }
//     chain->callNext(msg);
// }