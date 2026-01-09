
#include "luaMngr.h"

// typedef ke::HashMap<ke::AString, int,int> g_FuncIdMap;
StringHashMap<int> g_LuaPawnFuncMap;

static int Lua_CallPawnFunction_Proxy(lua_State *L)
{
    if (!L) {
        MF_Log(  "Lua_CallPawnFunction_Proxy: Invalid Lua state.");
        return 0;
    }
    int forwardId = (int)lua_tointeger(L, lua_upvalueindex(1));
    cell pawn_ret = MF_ExecuteForward(forwardId, (cell)L);
    return pawn_ret;
}

cell AMX_NATIVE_CALL Native_LuaRegisterFunction(AMX *amx, cell *params)
{
    lua_State *L = (lua_State *)params[1];
    if (!L) {
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

static cell AMX_NATIVE_CALL n_lua_open(AMX *amx, cell *params)
{
    lua_State * L=luaL_newstate();
    luaL_openlibs(L);
    return reinterpret_cast<cell>(L);
}

static cell AMX_NATIVE_CALL n_lua_close(AMX *amx, cell *params)
{
    lua_State *L = (lua_State *)params[1];
    if (L)
        lua_close(L);
    return 0;
}

static cell AMX_NATIVE_CALL n_lua_dostring(AMX *amx, cell *params)
{
    lua_State *L = (lua_State *)params[1];
    if (!L) {
        MF_Log(  "n_lua_dostring: Invalid Lua state.");
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
    if (!(lua_State *)params[1]) {
        MF_Log(  "n_lua_gettop: Invalid Lua state.");
        return 0;
    }
    return lua_gettop((lua_State *)params[1]);
}

static cell AMX_NATIVE_CALL n_lua_settop(AMX *amx, cell *params)
{
    if (!(lua_State *)params[1]) {
        MF_Log(  "n_lua_settop: Invalid Lua state.");
        return 0;
    }
    lua_settop((lua_State *)params[1], params[2]);
    return 0;
}

static cell AMX_NATIVE_CALL n_lua_pop(AMX *amx, cell *params)
{
    if (!(lua_State *)params[1]) {
        MF_Log(  "n_lua_pop: Invalid Lua state.");
        return 0;
    }
    lua_pop((lua_State *)params[1], params[2]);
    return 0;
}

static cell AMX_NATIVE_CALL n_lua_type(AMX *amx, cell *params)
{
    if (!(lua_State *)params[1]) {
        MF_Log(  "n_lua_type: Invalid Lua state.");
        return 0;
    }
    return lua_type((lua_State *)params[1], params[2]);
}

static cell AMX_NATIVE_CALL n_lua_pushvalue(AMX *amx, cell *params)
{
    if (!(lua_State *)params[1]) {
        MF_Log(  "n_lua_pushvalue: Invalid Lua state.");
        return 0;
    }
    lua_pushvalue((lua_State *)params[1], params[2]);
    return 0;
}

static cell AMX_NATIVE_CALL n_lua_remove(AMX *amx, cell *params)
{
    if (!(lua_State *)params[1]) {
        MF_Log(  "n_lua_remove: Invalid Lua state.");
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
    if (!(lua_State *)params[1]) {
        MF_Log(  "n_lua_pushinteger: Invalid Lua state.");
        return 0;
    }
    lua_pushinteger((lua_State *)params[1], (lua_Integer)params[2]);
    return 0;
}

static cell AMX_NATIVE_CALL n_lua_pushnumber(AMX *amx, cell *params)
{
    if (!(lua_State *)params[1]) {
        MF_Log(  "n_lua_pushnumber: Invalid Lua state.");
        return 0;
    }
    lua_pushnumber((lua_State *)params[1], (lua_Number)amx_ctof(params[2]));
    return 0;
}

static cell AMX_NATIVE_CALL n_lua_pushstring(AMX *amx, cell *params)
{
    if (!(lua_State *)params[1]) {
        MF_Log(  "n_lua_pushstring: Invalid Lua state.");
        return 0;
    }
    lua_pushstring((lua_State *)params[1], MF_GetAmxString(amx, params[2], 0, NULL));
    return 0;
}

static cell AMX_NATIVE_CALL n_lua_pushboolean(AMX *amx, cell *params)
{
    if (!(lua_State *)params[1]) {
        MF_Log(  "n_lua_pushboolean: Invalid Lua state.");
        return 0;
    }
    lua_pushboolean((lua_State *)params[1], params[2]);
    return 0;
}

static cell AMX_NATIVE_CALL n_lua_pushnil(AMX *amx, cell *params)
{
    if (!(lua_State *)params[1]) {
        MF_Log(  "n_lua_pushnil: Invalid Lua state.");
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
    if (!(lua_State *)params[1]) {
        MF_Log(  "n_lua_tointeger: Invalid Lua state.");
        return 0;
    }
    return (cell)lua_tointeger((lua_State *)params[1], params[2]);
}

static cell AMX_NATIVE_CALL n_lua_tonumber(AMX *amx, cell *params)
{
    if (!(lua_State *)params[1]) {
        MF_Log(  "n_lua_tonumber: Invalid Lua state.");
        return 0;
    }
    float res = (float)lua_tonumber((lua_State *)params[1], params[2]);
    return amx_ftoc(res);
}

static cell AMX_NATIVE_CALL n_lua_tostring(AMX *amx, cell *params)
{
    if (!(lua_State *)params[1]) {
        MF_Log(  "n_lua_tostring: Invalid Lua state.");
        return 0;
    }
    const char *str = lua_tostring((lua_State *)params[1], params[2]);
    return MF_SetAmxStringUTF8Char(amx, params[3], str ? str : "",str?strlen(str):0,params[4]);
}

static cell AMX_NATIVE_CALL n_lua_toboolean(AMX *amx, cell *params)
{
    if (!(lua_State *)params[1]) {
        MF_Log(  "n_lua_toboolean: Invalid Lua state.");
        return 0;
    }
    return lua_toboolean((lua_State *)params[1], params[2]);
}

// ---------------------------------------------------------
// Native 实现: 表与变量操作
// ---------------------------------------------------------

static cell AMX_NATIVE_CALL n_lua_getglobal(AMX *amx, cell *params)
{
    if (!(lua_State *)params[1]) {
        MF_Log(  "n_lua_getglobal: Invalid Lua state.");
        return 0;
    }
    return lua_getglobal((lua_State *)params[1], MF_GetAmxString(amx, params[2], 0, NULL));
}

static cell AMX_NATIVE_CALL n_lua_setglobal(AMX *amx, cell *params)
{
    if (!(lua_State *)params[1]) {
        MF_Log(  "n_lua_setglobal: Invalid Lua state.");
        return 0;
    }
    lua_setglobal((lua_State *)params[1], MF_GetAmxString(amx, params[2], 0, NULL));
    return 0;
}

static cell AMX_NATIVE_CALL n_lua_getfield(AMX *amx, cell *params)
{
    if (!(lua_State *)params[1]) {
        MF_Log(  "n_lua_getfield: Invalid Lua state.");
        return 0;
    }
    return lua_getfield((lua_State *)params[1], params[2], MF_GetAmxString(amx, params[3], 0, NULL));
}

static cell AMX_NATIVE_CALL n_lua_setfield(AMX *amx, cell *params)
{
    if (!(lua_State *)params[1]) {
        MF_Log(  "n_lua_setfield: Invalid Lua state.");
        return 0;
    }
    lua_setfield((lua_State *)params[1], params[2], MF_GetAmxString(amx, params[3], 0, NULL));
    return 0;
}

static cell AMX_NATIVE_CALL n_lua_createtable(AMX *amx, cell *params)
{
    if (!(lua_State *)params[1]) {
        MF_Log(  "n_lua_createtable: Invalid Lua state.");
        return 0;
    }
    lua_createtable((lua_State *)params[1], params[2], params[3]);
    return 0;
}

static cell AMX_NATIVE_CALL n_lua_settable(AMX *amx, cell *params)
{
    if (!(lua_State *)params[1]) {
        MF_Log(  "n_lua_settable: Invalid Lua state.");
        return 0;
    }
    lua_settable((lua_State *)params[1], params[2]);
    return 0;
}

static cell AMX_NATIVE_CALL n_lua_gettable(AMX *amx, cell *params)
{
    if (!(lua_State *)params[1]) {
        MF_Log(  "n_lua_gettable: Invalid Lua state.");
        return 0;
    }
    return lua_gettable((lua_State *)params[1], params[2]);
}

static cell AMX_NATIVE_CALL n_lua_rawlen(AMX *amx, cell *params)
{
    if (!(lua_State *)params[1]) {
        MF_Log(  "n_lua_rawlen: Invalid Lua state.");
        return 0;
    }
    return (cell)lua_rawlen((lua_State *)params[1], params[2]);
}

cell AMX_NATIVE_CALL Native_LuaRef(AMX *amx, cell *params)
{
    if (!(lua_State *)params[1]) {
        MF_Log(  "Native_LuaRef: Invalid Lua state.");
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
    if (!(lua_State *)params[1]) {
        MF_Log(  "Native_LuaUnref: Invalid Lua state.");
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
    if (!(lua_State *)params[1]) {
        MF_Log(  "Native_LuaGetRef: Invalid Lua state.");
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
    if (!(lua_State *)params[1]) {
        MF_Log(  "n_lua_pcall: Invalid Lua state.");
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
void TrimString(char* str) {
    size_t len = strlen(str);
    while (len > 0 && (str[len - 1] == '\n' || str[len - 1] == '\r' || str[len - 1] == ' ')) {
        str[--len] = '\0';
    }
}
void OnPluginsLoaded(){
	// 构建路径 (建议使用 MF_BuildPathname 以兼容不同模组目录，但这里先用你指定的路径)
    const char* szFile = "cstrike/addons/amxmodx/luascripting/function.txt";
    
    // 获取完整路径 (推荐做法，防止 cstrike 目录名变化)
    // char fullPath[256];
    // MF_BuildPathname(fullPath, sizeof(fullPath), "%s", szFile);

    FILE* fp = fopen(szFile, "rt");
    if (!fp) {
        MF_Log("Error: Failed to open file %s", szFile);
        return;
    }
    char buffer[128];
    // 逐行读取
    while (fgets(buffer, sizeof(buffer), fp)) {
        // 1. 清理换行符
        TrimString(buffer);

        // 2. 跳过空行或注释 (# 或 //)
        if (buffer[0] == '\0' || buffer[0] == '#' || (buffer[0] == '/' && buffer[1] == '/')) {
            continue;
        }
        g_LuaPawnFuncMap.insert(buffer, MF_RegisterForward(buffer,ET_STOP,FP_CELL,FP_DONE));
        // MF_Log("Registered Pawn function '%s' for Lua", buffer);

    }

    fclose(fp);
    
}