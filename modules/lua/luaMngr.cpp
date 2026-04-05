
#include "luaMngr.h"
bool HasReHlds;
bool HasReGameDll;


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

// ---------------------------------------------------------
// 辅助函数：从 Lua table 提取 {x, y, z} 坐标
// ---------------------------------------------------------
static void get_vec_from_table(lua_State *L, int idx, float *vec)
{
    if (lua_istable(L, idx))
    {
        for (int i = 0; i < 3; i++)
        {
            lua_rawgeti(L, idx, i + 1);
            vec[i] = (float)lua_tonumber(L, -1);
            lua_pop(L, 1);
        }
    }
    else
    {
        vec[0] = vec[1] = vec[2] = 0.0f;
    }
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

bool is_ent_valid(int iEnt)
{
	if (iEnt < 1 || iEnt > gpGlobals->maxEntities) 
		return false;

	if (iEnt <= gpGlobals->maxClients)
	{
		if (!MF_IsPlayerIngame(iEnt))
		{
			return false;
		}
	} else {
		if (FNullEnt(INDEXENT(iEnt)))
		{
			return false;
		}
	}

	return true;
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
static int L_is_nullent(lua_State* L)
{
    lua_pushboolean(L, !is_ent_valid(lua_tointeger(L, 1)));
    return 1;
}
// 查找指定半径内的玩家，并通过回调函数返回
static int L_entity_players_range(lua_State *L)
{
    int entCenter = (int)luaL_checkinteger(L, 1);
    float radius  = (float)luaL_checknumber(L, 2);

    if (!lua_isfunction(L, 3)) 
    {
        return luaL_error(L, "Argument 3 must be a callback function");
    }

    edict_t *pCenter = INDEXENT(entCenter);
    if ( !is_ent_valid(entCenter) || !pCenter)
    {
        return 0;
    }

    for (int i = 1; i <= gpGlobals->maxClients; ++i)
    {
        edict_t *pOther = INDEXENT(i);

        if (!is_ent_valid(i) || !pOther || pOther->free) 
        {
            continue;
        }
        float dist = (pCenter->v.origin - pOther->v.origin).Length();
        if (dist <= radius)
        {
            lua_pushvalue(L, 3); 
            lua_pushinteger(L, i);
            lua_pushnumber(L, dist);
            if (lua_pcall(L, 2, 0, 0) != LUA_OK)
            {
                printf("[Lua Error] in entity_players_range: %s\n", lua_tostring(L, -1));
                lua_pop(L, 1); 
            }
        }
    }

    return 0; 
}
// 查找指定半径内的所有实体，并通过回调函数返回
static int L_entity_all_range(lua_State *L)
{
    int entCenter = (int)luaL_checkinteger(L, 1);
    float radius  = (float)luaL_checknumber(L, 2);

    if (!lua_isfunction(L, 3)) 
    {
        return luaL_error(L, "Argument 3 must be a callback function");
    }

    edict_t *pCenter = INDEXENT(entCenter);
    if (!is_ent_valid(entCenter) || !pCenter)
    {
        return 0; 
    }

    for (int i = 1; i < gpGlobals->maxEntities; ++i)
    {
        edict_t *pOther = INDEXENT(i);

        if (!is_ent_valid(i) || !pOther || pOther->free) 
        {
            continue;
        }

        float dist = (pCenter->v.origin - pOther->v.origin).Length();

        if (dist <= radius)
        {
            lua_pushvalue(L, 3); 
            
            lua_pushinteger(L, i);
            lua_pushnumber(L, dist);

            if (lua_pcall(L, 2, 0, 0) != LUA_OK)
            {
                printf("[Lua Error] in entity_all_range: %s\n", lua_tostring(L, -1));
                lua_pop(L, 1); 
            }
        }
    }

    return 0; 
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
// ---------------------------------------------------------
// 引擎消息函数 Lua 绑定
// ---------------------------------------------------------
static int L_message_begin(lua_State *L)
{
    int msg_dest = (int)luaL_checkinteger(L, 1);
    int msg_type = (int)luaL_checkinteger(L, 2);

    float origin[3] = {0.0f, 0.0f, 0.0f};
    float *pOrigin = nullptr;

    if (lua_istable(L, 3))
    {
        get_vec_from_table(L, 3, origin);
        pOrigin = origin;
    }

    edict_t *ed = nullptr;
    if (lua_isnumber(L, 4))
    {
        int entIndex = (int)lua_tointeger(L, 4);
        if (entIndex > 0) ed = INDEXENT(entIndex);
    }

    // 调用 SDK 宏
    MESSAGE_BEGIN(msg_dest, msg_type, pOrigin, ed);
    return 0;
}

static int L_message_end(lua_State *L)
{
    MESSAGE_END();
    return 0;
}

static int L_write_byte(lua_State *L) { WRITE_BYTE((int)luaL_checkinteger(L, 1)); return 0; }
static int L_write_char(lua_State *L) { WRITE_CHAR((int)luaL_checkinteger(L, 1)); return 0; }
static int L_write_short(lua_State *L) { WRITE_SHORT((int)luaL_checkinteger(L, 1)); return 0; }
static int L_write_long(lua_State *L) { WRITE_LONG((int)luaL_checkinteger(L, 1)); return 0; }
static int L_write_angle(lua_State *L) { WRITE_ANGLE((float)luaL_checknumber(L, 1)); return 0; }
static int L_write_coord(lua_State *L) { WRITE_COORD((float)luaL_checknumber(L, 1)); return 0; }
static int L_write_string(lua_State *L) { WRITE_STRING(luaL_checkstring(L, 1)); return 0; }
static int L_write_entity(lua_State *L) { WRITE_ENTITY((int)luaL_checkinteger(L, 1)); return 0; }

// ---------------------------------------------------------
// 引擎工具函数绑定
// ---------------------------------------------------------

static int L_RandomLong(lua_State *L)
{
    int32_t lLow = (int32_t)luaL_checkinteger(L, 1);
    int32_t lHigh = (int32_t)luaL_checkinteger(L, 2);
    lua_pushinteger(L, RANDOM_LONG(lLow, lHigh));
    return 1;
}

static int L_RandomFloat(lua_State *L)
{
    float flLow = (float)luaL_checknumber(L, 1);
    float flHigh = (float)luaL_checknumber(L, 2);
    lua_pushnumber(L, RANDOM_FLOAT(flLow, flHigh));
    return 1;
}

static int L_Time(lua_State *L)
{
    // HL 引擎没有直接的 TIME 宏，时间通常存储在 gpGlobals 里
    if (gpGlobals) {
        lua_pushnumber(L, gpGlobals->time);
    } else {
        lua_pushnumber(L, 0.0);
    }
    return 1;
}

static int L_BuildSoundMsg(lua_State *L)
{
    int entIndex = (int)luaL_checkinteger(L, 1);
    edict_t *entity = (entIndex > 0) ? INDEXENT(entIndex) : nullptr;
    
    int channel = (int)luaL_checkinteger(L, 2);
    const char *sample = luaL_checkstring(L, 3);
    float volume = (float)luaL_checknumber(L, 4);
    float attenuation = (float)luaL_checknumber(L, 5);
    int fFlags = (int)luaL_checkinteger(L, 6);
    int pitch = (int)luaL_checkinteger(L, 7);
    int msg_dest = (int)luaL_checkinteger(L, 8);
    int msg_type = (int)luaL_checkinteger(L, 9);

    float origin[3];
    float *pOrigin = nullptr;
    if (lua_istable(L, 10))
    {
        get_vec_from_table(L, 10, origin);
        pOrigin = origin;
    }

    int edIndex = (int)luaL_optinteger(L, 11, 0);
    edict_t *ed = (edIndex > 0) ? INDEXENT(edIndex) : nullptr;

    BUILD_SOUND_MSG(entity, channel, sample, volume, attenuation, fFlags, pitch, msg_dest, msg_type, pOrigin, ed);
    return 0;
}

static int L_receiver_is_null(lua_State *L)
{
    if (lua_isnil(L, 1))
    {
        lua_pushboolean(L, 1);
        return 1;
    }
    if (!lua_islightuserdata(L, 1))
    {
        lua_pushboolean(L, 0);
        return 1;
    }
    void *p = lua_touserdata(L, 1);
    lua_pushboolean(L, p == nullptr);
    return 1;
}

static int L_receiver_equal(lua_State *L)
{
    void *a = nullptr;
    void *b = nullptr;

    if (!lua_isnil(L, 1) && lua_islightuserdata(L, 1)) a = lua_touserdata(L, 1);
    if (!lua_isnil(L, 2) && lua_islightuserdata(L, 2)) b = lua_touserdata(L, 2);

    lua_pushboolean(L, a == b);
    return 1;
}

static int L_receiver_id(lua_State *L)
{
    if (lua_isnil(L, 1))
    {
        return 0;
    }
    if (!lua_islightuserdata(L, 1))
    {
        return 0;
    }
    IGameClient *receiver = reinterpret_cast<IGameClient *>(lua_touserdata(L, 1));
    lua_pushinteger(L, receiver ? receiver->GetId() : 0);
    return 1;
}

static int L_receiver_to(lua_State *L)
{
    if (lua_isnil(L, 1))
    {
        return 0;
    }
    if (!lua_isnumber(L, 1))
    {
        return 0;
    }
    lua_pushlightuserdata(L, RehldsSvs->GetClient(luaL_checkinteger(L, 1)));
    return 1;
}


// Direct call of RehldsFuncs->SV_EmitSound2 for Lua.
// Lua receiver should be the lightuserdata value passed into the Rehlds_SV_EmitSound2 callback.
static int L_SV_EmitSound2_call(lua_State *L)
{
    if (!HasReHlds || !RehldsFuncs || !RehldsFuncs->SV_EmitSound2)
    {
        lua_pushboolean(L, 0);
        return 1;
    }

    int entIndex = (int)luaL_checkinteger(L, 1);
    edict_t *entity = (entIndex > 0) ? INDEXENT(entIndex) : nullptr;

    IGameClient *receiver = nullptr;
    if (!lua_isnil(L, 2))
    {
        if (!lua_islightuserdata(L, 2))
        {
            return luaL_error(L, "receiver must be lightuserdata or nil");
        }
        receiver = reinterpret_cast<IGameClient *>(lua_touserdata(L, 2));
    }

    int channel = (int)luaL_checkinteger(L, 3);
    const char *sample = luaL_checkstring(L, 4);
    float volume = (float)luaL_checknumber(L, 5);
    float attenuation = (float)luaL_checknumber(L, 6);
    int flags = (int)luaL_checkinteger(L, 7);
    int pitch = (int)luaL_checkinteger(L, 8);
    int emitFlags = (int)luaL_checkinteger(L, 9);

    float origin[3] = {0.0f, 0.0f, 0.0f};
    const float *pOrigin = nullptr;
    if (lua_istable(L, 10))
    {
        get_vec_from_table(L, 10, origin);
        pOrigin = origin;
    }

    bool res = RehldsFuncs->SV_EmitSound2(entity, receiver, channel, sample, volume, attenuation,
                                          flags, pitch, emitFlags, pOrigin);
    lua_pushboolean(L, res ? 1 : 0);
    return 1;
}

static int L_PrecacheModel(lua_State *L)
{
    lua_pushinteger(L, PRECACHE_MODEL(luaL_checkstring(L, 1)));
    return 1;
}

static int L_PrecacheSound(lua_State *L)
{
    lua_pushinteger(L, PRECACHE_SOUND(luaL_checkstring(L, 1)));
    return 1;
}

static int L_SetModel(lua_State *L)
{
    int entIndex = (int)luaL_checkinteger(L, 1);
    edict_t *e = (entIndex > 0) ? INDEXENT(entIndex) : nullptr;
    if (e) SET_MODEL(e, luaL_checkstring(L, 2));
    return 0;
}

static int L_ModelIndex(lua_State *L)
{
    lua_pushinteger(L, MODEL_INDEX(luaL_checkstring(L, 1)));
    return 1;
}

static int L_ModelFrames(lua_State *L)
{
    lua_pushinteger(L, MODEL_FRAMES((int)luaL_checkinteger(L, 1)));
    return 1;
}

static int L_SetSize(lua_State *L)
{
    int entIndex = (int)luaL_checkinteger(L, 1);
    edict_t *e = (entIndex > 0) ? INDEXENT(entIndex) : nullptr;
    
    float min[3], max[3];
    get_vec_from_table(L, 2, min);
    get_vec_from_table(L, 3, max);

    if (e) SET_SIZE(e, min, max);
    return 0;
}

static int L_ChangeLevel(lua_State *L)
{
    CHANGE_LEVEL(luaL_checkstring(L, 1), luaL_optstring(L, 2, ""));
    return 0;
}

static int L_GetSpawnParms(lua_State *L)
{
    int entIndex = (int)luaL_checkinteger(L, 1);
    edict_t *ent = (entIndex > 0) ? INDEXENT(entIndex) : nullptr;
    if (ent) GET_SPAWN_PARMS(ent);
    return 0;
}

static int L_SaveSpawnParms(lua_State *L)
{
    int entIndex = (int)luaL_checkinteger(L, 1);
    edict_t *ent = (entIndex > 0) ? INDEXENT(entIndex) : nullptr;
    if (ent) SAVE_SPAWN_PARMS(ent);
    return 0;
}

static int L_VecToYaw(lua_State *L)
{
    float vec[3];
    get_vec_from_table(L, 1, vec);
    lua_pushnumber(L, VEC_TO_YAW(vec));
    return 1;
}

static int L_VecToAngles(lua_State *L)
{
    float vecIn[3], vecOut[3];
    get_vec_from_table(L, 1, vecIn);
    VEC_TO_ANGLES(vecIn, vecOut);

    lua_newtable(L);
    for (int i = 0; i < 3; i++)
    {
        lua_pushnumber(L, vecOut[i]);
        lua_rawseti(L, -2, i + 1);
    }
    return 1;
}

static int L_MoveToOrigin(lua_State *L)
{
    int entIndex = (int)luaL_checkinteger(L, 1);
    edict_t *ent = (entIndex > 0) ? INDEXENT(entIndex) : nullptr;
    
    float goal[3];
    get_vec_from_table(L, 2, goal);
    
    float dist = (float)luaL_checknumber(L, 3);
    int moveType = (int)luaL_checkinteger(L, 4);

    if (ent) MOVE_TO_ORIGIN(ent, goal, dist, moveType);
    return 0;
}

static int L_ChangeYaw(lua_State *L)
{
    int entIndex = (int)luaL_checkinteger(L, 1);
    edict_t *ent = (entIndex > 0) ? INDEXENT(entIndex) : nullptr;
    // 宏定义中写的是 oldCHANGE_YAW，这里直接使用原指针最安全
    if (ent) g_engfuncs.pfnChangeYaw(ent); 
    return 0;
}

static int L_ChangePitch(lua_State *L)
{
    int entIndex = (int)luaL_checkinteger(L, 1);
    edict_t *ent = (entIndex > 0) ? INDEXENT(entIndex) : nullptr;
    if (ent) CHANGE_PITCH(ent);
    return 0;
}

static int L_FindEntityByString(lua_State *L)
{
    int startEntIdx = (int)luaL_checkinteger(L, 1);
    edict_t *pStart = (startEntIdx > 0) ? INDEXENT(startEntIdx) : nullptr;
    
    edict_t *result = FIND_ENTITY_BY_STRING(pStart, luaL_checkstring(L, 2), luaL_checkstring(L, 3));
    
    if (result && !result->free) lua_pushinteger(L, ENTINDEX(result));
    else lua_pushnil(L);
    return 1;
}

static int L_GetEntityIllum(lua_State *L)
{
    int entIndex = (int)luaL_checkinteger(L, 1);
    edict_t *ent = (entIndex > 0) ? INDEXENT(entIndex) : nullptr;
    lua_pushinteger(L, ent ? GETENTITYILLUM(ent) : 0);
    return 1;
}

static int L_FindEntityInSphere(lua_State *L)
{
    int startEntIdx = (int)luaL_checkinteger(L, 1);
    edict_t *pStart = (startEntIdx > 0) ? INDEXENT(startEntIdx) : nullptr;
    
    float org[3];
    get_vec_from_table(L, 2, org);

    edict_t *result = FIND_ENTITY_IN_SPHERE(pStart, org, (float)luaL_checknumber(L, 3));
    
    if (result && !result->free) lua_pushinteger(L, ENTINDEX(result));
    else lua_pushnil(L);
    return 1;
}

static int L_FindClientInPVS(lua_State *L)
{
    int entIndex = (int)luaL_checkinteger(L, 1);
    edict_t *ent = (entIndex > 0) ? INDEXENT(entIndex) : nullptr;

    if (ent) {
        edict_t *result = FIND_CLIENT_IN_PVS(ent);
        if (result && !result->free) {
            lua_pushinteger(L, ENTINDEX(result));
            return 1;
        }
    }
    lua_pushnil(L);
    return 1;
}

static int L_EntitiesInPVS(lua_State *L)
{
    int entIndex = (int)luaL_checkinteger(L, 1);
    edict_t *ent = (entIndex > 0) ? INDEXENT(entIndex) : nullptr;

    if (ent && g_engfuncs.pfnEntitiesInPVS) {
        edict_t *result = g_engfuncs.pfnEntitiesInPVS(ent);
        if (result && !result->free) {
            lua_pushinteger(L, ENTINDEX(result));
            return 1;
        }
    }
    lua_pushnil(L);
    return 1;
}

static int L_RemoveEntity(lua_State *L)
{
    int entIndex = (int)luaL_checkinteger(L, 1);
    edict_t *ent = (entIndex > 0) ? INDEXENT(entIndex) : nullptr;
    if (ent) REMOVE_ENTITY(ent);
    return 0;
}

static int L_GetGameDir(lua_State *L)
{
    char szGameDir[256] = {0};
    GET_GAME_DIR(szGameDir);
    lua_pushstring(L, szGameDir);
    return 1;
}

static int L_trace_line(lua_State *L)
{
    float v1[3], v2[3];
    get_vec_from_table(L, 1, v1);
    get_vec_from_table(L, 2, v2);
    int noMonsters = (int)luaL_checkinteger(L, 3);
    
    edict_t *pSkip = nullptr;
    if (lua_isnumber(L, 4))
    {
        int entIndex = (int)lua_tointeger(L, 4);
        if (entIndex > 0) pSkip = INDEXENT(entIndex);
    }

    TraceResult tr; 
    
    TRACE_LINE(v1, v2, noMonsters, pSkip, &tr);

    lua_newtable(L);
    
    lua_pushstring(L, "fAllSolid");
    lua_pushboolean(L, tr.fAllSolid);
    lua_settable(L, -3);
    
    lua_pushstring(L, "fStartSolid");
    lua_pushboolean(L, tr.fStartSolid);
    lua_settable(L, -3);
    
    lua_pushstring(L, "fInOpen");
    lua_pushboolean(L, tr.fInOpen);
    lua_settable(L, -3);
    
    lua_pushstring(L, "fInWater");
    lua_pushboolean(L, tr.fInWater);
    lua_settable(L, -3);
    
    lua_pushstring(L, "flFraction");
    lua_pushnumber(L, tr.flFraction);
    lua_settable(L, -3);
    
    lua_pushstring(L, "vecEndPos");
    lua_newtable(L);
    for (int i = 0; i < 3; i++) {
        lua_pushnumber(L, tr.vecEndPos[i]);
        lua_rawseti(L, -2, i + 1);
    }
    lua_settable(L, -3);
    
    lua_pushstring(L, "flPlaneDist");
    lua_pushnumber(L, tr.flPlaneDist);
    lua_settable(L, -3);
    
    lua_pushstring(L, "vecPlaneNormal");
    lua_newtable(L);
    for (int i = 0; i < 3; i++) {
        lua_pushnumber(L, tr.vecPlaneNormal[i]);
        lua_rawseti(L, -2, i + 1);
    }
    lua_settable(L, -3);
    
    lua_pushstring(L, "pHit");
    if (tr.pHit) lua_pushinteger(L, ENTINDEX(tr.pHit));
    else lua_pushinteger(L, 0);
    lua_settable(L, -3);
    
    lua_pushstring(L, "iHitgroup");
    lua_pushinteger(L, tr.iHitgroup);
    lua_settable(L, -3);

    return 1;
}

static int L_trace_toss(lua_State *L)
{
    int pentIndex = (int)luaL_checkinteger(L, 1);
    int pentToIgnoreIndex = (int)luaL_optinteger(L, 2, -1);
    
    edict_t *pent = INDEXENT(pentIndex);
    edict_t *pentToIgnore = (pentToIgnoreIndex > 0) ? INDEXENT(pentToIgnoreIndex) : NULL;
    
    TraceResult tr;
    TRACE_TOSS(pent, pentToIgnore, &tr);

    lua_newtable(L);
    
    lua_pushstring(L, "fAllSolid");
    lua_pushboolean(L, tr.fAllSolid);
    lua_settable(L, -3);
    
    lua_pushstring(L, "fStartSolid");
    lua_pushboolean(L, tr.fStartSolid);
    lua_settable(L, -3);
    
    lua_pushstring(L, "fInOpen");
    lua_pushboolean(L, tr.fInOpen);
    lua_settable(L, -3);
    
    lua_pushstring(L, "fInWater");
    lua_pushboolean(L, tr.fInWater);
    lua_settable(L, -3);
    
    lua_pushstring(L, "flFraction");
    lua_pushnumber(L, tr.flFraction);
    lua_settable(L, -3);
    
    lua_pushstring(L, "vecEndPos");
    lua_newtable(L);
    for (int i = 0; i < 3; i++) {
        lua_pushnumber(L, tr.vecEndPos[i]);
        lua_rawseti(L, -2, i + 1);
    }
    lua_settable(L, -3);
    
    lua_pushstring(L, "flPlaneDist");
    lua_pushnumber(L, tr.flPlaneDist);
    lua_settable(L, -3);
    
    lua_pushstring(L, "vecPlaneNormal");
    lua_newtable(L);
    for (int i = 0; i < 3; i++) {
        lua_pushnumber(L, tr.vecPlaneNormal[i]);
        lua_rawseti(L, -2, i + 1);
    }
    lua_settable(L, -3);
    
    lua_pushstring(L, "pHit");
    if (tr.pHit) lua_pushinteger(L, ENTINDEX(tr.pHit));
    else lua_pushinteger(L, 0);
    lua_settable(L, -3);
    
    lua_pushstring(L, "iHitgroup");
    lua_pushinteger(L, tr.iHitgroup);
    lua_settable(L, -3);

    return 1;
}

static int L_trace_monster_hull(lua_State *L)
{
    int pentIndex = (int)luaL_checkinteger(L, 1);
    float v1[3], v2[3];
    get_vec_from_table(L, 2, v1);
    get_vec_from_table(L, 3, v2);
    int noMonsters = (int)luaL_checkinteger(L, 4);
    int pentToSkipIndex = (int)luaL_optinteger(L, 5, 0);
    
    edict_t *pent = INDEXENT(pentIndex);
    edict_t *pentToSkip = (pentToSkipIndex > 0) ? INDEXENT(pentToSkipIndex) : NULL;
    
    TraceResult tr;
    TRACE_MONSTER_HULL(pent, v1, v2, noMonsters, pentToSkip, &tr);

    lua_newtable(L);
    
    lua_pushstring(L, "fAllSolid");
    lua_pushboolean(L, tr.fAllSolid);
    lua_settable(L, -3);
    
    lua_pushstring(L, "fStartSolid");
    lua_pushboolean(L, tr.fStartSolid);
    lua_settable(L, -3);
    
    lua_pushstring(L, "fInOpen");
    lua_pushboolean(L, tr.fInOpen);
    lua_settable(L, -3);
    
    lua_pushstring(L, "fInWater");
    lua_pushboolean(L, tr.fInWater);
    lua_settable(L, -3);
    
    lua_pushstring(L, "flFraction");
    lua_pushnumber(L, tr.flFraction);
    lua_settable(L, -3);
    
    lua_pushstring(L, "vecEndPos");
    lua_newtable(L);
    for (int i = 0; i < 3; i++) {
        lua_pushnumber(L, tr.vecEndPos[i]);
        lua_rawseti(L, -2, i + 1);
    }
    lua_settable(L, -3);
    
    lua_pushstring(L, "flPlaneDist");
    lua_pushnumber(L, tr.flPlaneDist);
    lua_settable(L, -3);
    
    lua_pushstring(L, "vecPlaneNormal");
    lua_newtable(L);
    for (int i = 0; i < 3; i++) {
        lua_pushnumber(L, tr.vecPlaneNormal[i]);
        lua_rawseti(L, -2, i + 1);
    }
    lua_settable(L, -3);
    
    lua_pushstring(L, "pHit");
    if (tr.pHit) lua_pushinteger(L, ENTINDEX(tr.pHit));
    else lua_pushinteger(L, 0);
    lua_settable(L, -3);
    
    lua_pushstring(L, "iHitgroup");
    lua_pushinteger(L, tr.iHitgroup);
    lua_settable(L, -3);

    return 1;
}

static int L_trace_hull(lua_State *L)
{
    float v1[3], v2[3];
    get_vec_from_table(L, 1, v1);
    get_vec_from_table(L, 2, v2);
    int noMonsters = (int)luaL_checkinteger(L, 3);
    int hullNumber = (int)luaL_checkinteger(L, 4);
    int pentToSkipIndex = (int)luaL_optinteger(L, 5, 0);
    
    edict_t *pentToSkip = (pentToSkipIndex > 0) ? INDEXENT(pentToSkipIndex) : NULL;
    
    TraceResult tr;
    TRACE_HULL(v1, v2, noMonsters, hullNumber, pentToSkip, &tr);

    lua_newtable(L);
    
    lua_pushstring(L, "fAllSolid");
    lua_pushboolean(L, tr.fAllSolid);
    lua_settable(L, -3);
    
    lua_pushstring(L, "fStartSolid");
    lua_pushboolean(L, tr.fStartSolid);
    lua_settable(L, -3);
    
    lua_pushstring(L, "fInOpen");
    lua_pushboolean(L, tr.fInOpen);
    lua_settable(L, -3);
    
    lua_pushstring(L, "fInWater");
    lua_pushboolean(L, tr.fInWater);
    lua_settable(L, -3);
    
    lua_pushstring(L, "flFraction");
    lua_pushnumber(L, tr.flFraction);
    lua_settable(L, -3);
    
    lua_pushstring(L, "vecEndPos");
    lua_newtable(L);
    for (int i = 0; i < 3; i++) {
        lua_pushnumber(L, tr.vecEndPos[i]);
        lua_rawseti(L, -2, i + 1);
    }
    lua_settable(L, -3);
    
    lua_pushstring(L, "flPlaneDist");
    lua_pushnumber(L, tr.flPlaneDist);
    lua_settable(L, -3);
    
    lua_pushstring(L, "vecPlaneNormal");
    lua_newtable(L);
    for (int i = 0; i < 3; i++) {
        lua_pushnumber(L, tr.vecPlaneNormal[i]);
        lua_rawseti(L, -2, i + 1);
    }
    lua_settable(L, -3);
    
    lua_pushstring(L, "pHit");
    if (tr.pHit) lua_pushinteger(L, ENTINDEX(tr.pHit));
    else lua_pushinteger(L, 0);
    lua_settable(L, -3);
    
    lua_pushstring(L, "iHitgroup");
    lua_pushinteger(L, tr.iHitgroup);
    lua_settable(L, -3);

    return 1;
}

// ---------------------------------------------------------
// Entity State get/set 函数
// ---------------------------------------------------------
static int L_get_es(lua_State *L)
{
    entity_state_t *es = reinterpret_cast<entity_state_t *>(lua_touserdata(L, 1));
    int member = (int)luaL_checkinteger(L, 2);

    switch(member)
    {
    case ES_EntityType:
        lua_pushinteger(L, es->entityType);
        return 1;
    case ES_Number:
        lua_pushinteger(L, es->number);
        return 1;
    case ES_MsgTime:
        lua_pushnumber(L, es->msg_time);
        return 1;
    case ES_MessageNum:
        lua_pushinteger(L, es->messagenum);
        return 1;
    case ES_Origin:
        lua_newtable(L);
        lua_pushnumber(L, es->origin.x);
        lua_rawseti(L, -2, 1);
        lua_pushnumber(L, es->origin.y);
        lua_rawseti(L, -2, 2);
        lua_pushnumber(L, es->origin.z);
        lua_rawseti(L, -2, 3);
        return 1;
    case ES_Angles:
        lua_newtable(L);
        lua_pushnumber(L, es->angles.x);
        lua_rawseti(L, -2, 1);
        lua_pushnumber(L, es->angles.y);
        lua_rawseti(L, -2, 2);
        lua_pushnumber(L, es->angles.z);
        lua_rawseti(L, -2, 3);
        return 1;
    case ES_ModelIndex:
        lua_pushinteger(L, es->modelindex);
        return 1;
    case ES_Sequence:
        lua_pushinteger(L, es->sequence);
        return 1;
    case ES_Frame:
        lua_pushnumber(L, es->frame);
        return 1;
    case ES_ColorMap:
        lua_pushinteger(L, es->colormap);
        return 1;
    case ES_Skin:
        lua_pushinteger(L, es->skin);
        return 1;
    case ES_Solid:
        lua_pushinteger(L, es->solid);
        return 1;
    case ES_Effects:
        lua_pushinteger(L, es->effects);
        return 1;
    case ES_Scale:
        lua_pushnumber(L, es->scale);
        return 1;
    case ES_eFlags:
        lua_pushinteger(L, es->eflags);
        return 1;
    case ES_RenderMode:
        lua_pushinteger(L, es->rendermode);
        return 1;
    case ES_RenderAmt:
        lua_pushinteger(L, es->renderamt);
        return 1;
    case ES_RenderColor:
        lua_newtable(L);
        lua_pushinteger(L, es->rendercolor.r);
        lua_rawseti(L, -2, 1);
        lua_pushinteger(L, es->rendercolor.g);
        lua_rawseti(L, -2, 2);
        lua_pushinteger(L, es->rendercolor.b);
        lua_rawseti(L, -2, 3);
        return 1;
    case ES_RenderFx:
        lua_pushinteger(L, es->renderfx);
        return 1;
    case ES_MoveType:
        lua_pushinteger(L, es->movetype);
        return 1;
    case ES_AnimTime:
        lua_pushnumber(L, es->animtime);
        return 1;
    case ES_FrameRate:
        lua_pushnumber(L, es->framerate);
        return 1;
    case ES_Body:
        lua_pushinteger(L, es->body);
        return 1;
    case ES_Controller:
        lua_newtable(L);
        for (int i = 0; i < 4; i++) {
            lua_pushinteger(L, es->controller[i]);
            lua_rawseti(L, -2, i + 1);
        }
        return 1;
    case ES_Blending:
        lua_newtable(L);
        for (int i = 0; i < 4; i++) {
            lua_pushnumber(L, es->blending[i]);
            lua_rawseti(L, -2, i + 1);
        }
        return 1;
    case ES_Velocity:
        lua_newtable(L);
        lua_pushnumber(L, es->velocity.x);
        lua_rawseti(L, -2, 1);
        lua_pushnumber(L, es->velocity.y);
        lua_rawseti(L, -2, 2);
        lua_pushnumber(L, es->velocity.z);
        lua_rawseti(L, -2, 3);
        return 1;
    case ES_Mins:
        lua_newtable(L);
        lua_pushnumber(L, es->mins.x);
        lua_rawseti(L, -2, 1);
        lua_pushnumber(L, es->mins.y);
        lua_rawseti(L, -2, 2);
        lua_pushnumber(L, es->mins.z);
        lua_rawseti(L, -2, 3);
        return 1;
    case ES_Maxs:
        lua_newtable(L);
        lua_pushnumber(L, es->maxs.x);
        lua_rawseti(L, -2, 1);
        lua_pushnumber(L, es->maxs.y);
        lua_rawseti(L, -2, 2);
        lua_pushnumber(L, es->maxs.z);
        lua_rawseti(L, -2, 3);
        return 1;
    case ES_AimEnt:
        lua_pushinteger(L, es->aiment);
        return 1;
    case ES_Owner:
        lua_pushinteger(L, es->owner);
        return 1;
    case ES_Friction:
        lua_pushnumber(L, es->friction);
        return 1;
    case ES_Gravity:
        lua_pushnumber(L, es->gravity);
        return 1;
    case ES_Team:
        lua_pushinteger(L, es->team);
        return 1;
    case ES_PlayerClass:
        lua_pushinteger(L, es->playerclass);
        return 1;
    case ES_Health:
        lua_pushinteger(L, es->health);
        return 1;
    case ES_Spectator:
        lua_pushinteger(L, es->spectator);
        return 1;
    case ES_WeaponModel:
        lua_pushinteger(L, es->weaponmodel);
        return 1;
    case ES_GaitSequence:
        lua_pushinteger(L, es->gaitsequence);
        return 1;
    case ES_BaseVelocity:
        lua_newtable(L);
        lua_pushnumber(L, es->basevelocity.x);
        lua_rawseti(L, -2, 1);
        lua_pushnumber(L, es->basevelocity.y);
        lua_rawseti(L, -2, 2);
        lua_pushnumber(L, es->basevelocity.z);
        lua_rawseti(L, -2, 3);
        return 1;
    case ES_UseHull:
        lua_pushinteger(L, es->usehull);
        return 1;
    case ES_OldButtons:
        lua_pushinteger(L, es->oldbuttons);
        return 1;
    case ES_OnGround:
        lua_pushinteger(L, es->onground);
        return 1;
    case ES_iStepLeft:
        lua_pushinteger(L, es->iStepLeft);
        return 1;
    case ES_flFallVelocity:
        lua_pushnumber(L, es->flFallVelocity);
        return 1;
    case ES_FOV:
        lua_pushinteger(L, es->fov);
        return 1;
    case ES_WeaponAnim:
        lua_pushinteger(L, es->weaponanim);
        return 1;
    case ES_StartPos:
        lua_newtable(L);
        lua_pushnumber(L, es->startpos.x);
        lua_rawseti(L, -2, 1);
        lua_pushnumber(L, es->startpos.y);
        lua_rawseti(L, -2, 2);
        lua_pushnumber(L, es->startpos.z);
        lua_rawseti(L, -2, 3);
        return 1;
    case ES_EndPos:
        lua_newtable(L);
        lua_pushnumber(L, es->endpos.x);
        lua_rawseti(L, -2, 1);
        lua_pushnumber(L, es->endpos.y);
        lua_rawseti(L, -2, 2);
        lua_pushnumber(L, es->endpos.z);
        lua_rawseti(L, -2, 3);
        return 1;
    case ES_ImpactTime:
        lua_pushnumber(L, es->impacttime);
        return 1;
    case ES_StartTime:
        lua_pushnumber(L, es->starttime);
        return 1;
    case ES_iUser1:
        lua_pushinteger(L, es->iuser1);
        return 1;
    case ES_iUser2:
        lua_pushinteger(L, es->iuser2);
        return 1;
    case ES_iUser3:
        lua_pushinteger(L, es->iuser3);
        return 1;
    case ES_iUser4:
        lua_pushinteger(L, es->iuser4);
        return 1;
    case ES_fUser1:
        lua_pushnumber(L, es->fuser1);
        return 1;
    case ES_fUser2:
        lua_pushnumber(L, es->fuser2);
        return 1;
    case ES_fUser3:
        lua_pushnumber(L, es->fuser3);
        return 1;
    case ES_fUser4:
        lua_pushnumber(L, es->fuser4);
        return 1;
    case ES_vUser1:
        lua_newtable(L);
        lua_pushnumber(L, es->vuser1.x);
        lua_rawseti(L, -2, 1);
        lua_pushnumber(L, es->vuser1.y);
        lua_rawseti(L, -2, 2);
        lua_pushnumber(L, es->vuser1.z);
        lua_rawseti(L, -2, 3);
        return 1;
    case ES_vUser2:
        lua_newtable(L);
        lua_pushnumber(L, es->vuser2.x);
        lua_rawseti(L, -2, 1);
        lua_pushnumber(L, es->vuser2.y);
        lua_rawseti(L, -2, 2);
        lua_pushnumber(L, es->vuser2.z);
        lua_rawseti(L, -2, 3);
        return 1;
    case ES_vUser3:
        lua_newtable(L);
        lua_pushnumber(L, es->vuser3.x);
        lua_rawseti(L, -2, 1);
        lua_pushnumber(L, es->vuser3.y);
        lua_rawseti(L, -2, 2);
        lua_pushnumber(L, es->vuser3.z);
        lua_rawseti(L, -2, 3);
        return 1;
    case ES_vUser4:
        lua_newtable(L);
        lua_pushnumber(L, es->vuser4.x);
        lua_rawseti(L, -2, 1);
        lua_pushnumber(L, es->vuser4.y);
        lua_rawseti(L, -2, 2);
        lua_pushnumber(L, es->vuser4.z);
        lua_rawseti(L, -2, 3);
        return 1;
    default:
        luaL_error(L, "Unknown EntityState member: %d", member);
        return 0;
    }
}


static int L_set_es(lua_State *L)
{
    entity_state_t *es = reinterpret_cast<entity_state_t *>(lua_touserdata(L, 1));
    int member = (int)luaL_checkinteger(L, 2);

    if (lua_isnumber(L, 3))
    {
        lua_Number num = lua_tonumber(L, 3);
        int intValue = (int)num;
        float floatValue = (float)num;
        
        switch(member)
        {
        case ES_EntityType:
            es->entityType = intValue;
            return 1;
        case ES_Number:
            es->number = intValue;
            return 1;
        case ES_MsgTime:
            es->msg_time = floatValue;
            return 1;
        case ES_MessageNum:
            es->messagenum = intValue;
            return 1;
        case ES_ModelIndex:
            es->modelindex = intValue;
            return 1;
        case ES_Sequence:
            es->sequence = intValue;
            return 1;
        case ES_Frame:
            es->frame = floatValue;
            return 1;
        case ES_ColorMap:
            es->colormap = intValue;
            return 1;
        case ES_Skin:
            es->skin = intValue;
            return 1;
        case ES_Solid:
            es->solid = intValue;
            return 1;
        case ES_Effects:
            es->effects = intValue;
            return 1;
        case ES_Scale:
            es->scale = floatValue;
            return 1;
        case ES_eFlags:
            es->eflags = intValue;
            return 1;
        case ES_RenderMode:
            es->rendermode = intValue;
            return 1;
        case ES_RenderAmt:
            es->renderamt = intValue;
            return 1;
        case ES_RenderFx:
            es->renderfx = intValue;
            return 1;
        case ES_MoveType:
            es->movetype = intValue;
            return 1;
        case ES_AnimTime:
            es->animtime = floatValue;
            return 1;
        case ES_FrameRate:
            es->framerate = floatValue;
            return 1;
        case ES_Body:
            es->body = intValue;
            return 1;
        case ES_AimEnt:
            es->aiment = intValue;
            return 1;
        case ES_Owner:
            es->owner = intValue;
            return 1;
        case ES_Friction:
            es->friction = floatValue;
            return 1;
        case ES_Gravity:
            es->gravity = floatValue;
            return 1;
        case ES_Team:
            es->team = intValue;
            return 1;
        case ES_PlayerClass:
            es->playerclass = intValue;
            return 1;
        case ES_Health:
            es->health = intValue;
            return 1;
        case ES_Spectator:
            es->spectator = intValue;
            return 1;
        case ES_WeaponModel:
            es->weaponmodel = intValue;
            return 1;
        case ES_GaitSequence:
            es->gaitsequence = intValue;
            return 1;
        case ES_UseHull:
            es->usehull = intValue;
            return 1;
        case ES_OldButtons:
            es->oldbuttons = intValue;
            return 1;
        case ES_OnGround:
            es->onground = intValue;
            return 1;
        case ES_iStepLeft:
            es->iStepLeft = intValue;
            return 1;
        case ES_flFallVelocity:
            es->flFallVelocity = floatValue;
            return 1;
        case ES_FOV:
            es->fov = intValue;
            return 1;
        case ES_WeaponAnim:
            es->weaponanim = intValue;
            return 1;
        case ES_ImpactTime:
            es->impacttime = floatValue;
            return 1;
        case ES_StartTime:
            es->starttime = floatValue;
            return 1;
        case ES_iUser1:
            es->iuser1 = intValue;
            return 1;
        case ES_iUser2:
            es->iuser2 = intValue;
            return 1;
        case ES_iUser3:
            es->iuser3 = intValue;
            return 1;
        case ES_iUser4:
            es->iuser4 = intValue;
            return 1;
        case ES_fUser1:
            es->fuser1 = floatValue;
            return 1;
        case ES_fUser2:
            es->fuser2 = floatValue;
            return 1;
        case ES_fUser3:
            es->fuser3 = floatValue;
            return 1;
        case ES_fUser4:
            es->fuser4 = floatValue;
            return 1;
        default:
            break;
        }
    }

    if (lua_istable(L, 3))
    {
        float vec[3];
        get_vec_from_table(L, 3, vec);
        
        switch(member)
        {
        case ES_Origin:
            es->origin.x = vec[0];
            es->origin.y = vec[1];
            es->origin.z = vec[2];
            return 1;
        case ES_Angles:
            es->angles.x = vec[0];
            es->angles.y = vec[1];
            es->angles.z = vec[2];
            return 1;
        case ES_Velocity:
            es->velocity.x = vec[0];
            es->velocity.y = vec[1];
            es->velocity.z = vec[2];
            return 1;
        case ES_Mins:
            es->mins.x = vec[0];
            es->mins.y = vec[1];
            es->mins.z = vec[2];
            return 1;
        case ES_Maxs:
            es->maxs.x = vec[0];
            es->maxs.y = vec[1];
            es->maxs.z = vec[2];
            return 1;
        case ES_BaseVelocity:
            es->basevelocity.x = vec[0];
            es->basevelocity.y = vec[1];
            es->basevelocity.z = vec[2];
            return 1;
        case ES_StartPos:
            es->startpos.x = vec[0];
            es->startpos.y = vec[1];
            es->startpos.z = vec[2];
            return 1;
        case ES_EndPos:
            es->endpos.x = vec[0];
            es->endpos.y = vec[1];
            es->endpos.z = vec[2];
            return 1;
        case ES_vUser1:
            es->vuser1.x = vec[0];
            es->vuser1.y = vec[1];
            es->vuser1.z = vec[2];
            return 1;
        case ES_vUser2:
            es->vuser2.x = vec[0];
            es->vuser2.y = vec[1];
            es->vuser2.z = vec[2];
            return 1;
        case ES_vUser3:
            es->vuser3.x = vec[0];
            es->vuser3.y = vec[1];
            es->vuser3.z = vec[2];
            return 1;
        case ES_vUser4:
            es->vuser4.x = vec[0];
            es->vuser4.y = vec[1];
            es->vuser4.z = vec[2];
            return 1;
        case ES_RenderColor:
            es->rendercolor.r = (byte)vec[0];
            es->rendercolor.g = (byte)vec[1];
            es->rendercolor.b = (byte)vec[2];
            return 1;
        case ES_Controller:
            for (int i = 0; i < 4; i++)
            {
                lua_rawgeti(L, 3, i + 1);
                es->controller[i] = lua_tointeger(L, -1);
                lua_pop(L, 1);
            }
            return 1;
        case ES_Blending:
            for (int i = 0; i < 4; i++)
            {
                lua_rawgeti(L, 3, i + 1);
                es->blending[i] = (float)lua_tonumber(L, -1);
                lua_pop(L, 1);
            }
            return 1;
        default:
            break;
        }
    }

    luaL_error(L, "Unknown or unsupported EntityState member: %d", member);
    return 0;
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
    lua_register(L, "amxx_get_gametime", L_get_gametime);
    lua_register(L, "amxx_random_num", L_random_num);
    lua_register(L, "amxx2_is_nullent", L_is_nullent);
    lua_register(L, "amxx2_entity_players_range", L_entity_players_range);
    lua_register(L, "amxx2_entity_all_range", L_entity_all_range);
    // 注册消息系统 API
    lua_register(L, "amxx2_message_begin", L_message_begin);
    lua_register(L, "amxx2_message_end", L_message_end);
    lua_register(L, "amxx2_write_byte", L_write_byte);
    lua_register(L, "amxx2_write_char", L_write_char);
    lua_register(L, "amxx2_write_short", L_write_short);
    lua_register(L, "amxx2_write_long", L_write_long);
    lua_register(L, "amxx2_write_angle", L_write_angle);
    lua_register(L, "amxx2_write_coord", L_write_coord);
    lua_register(L, "amxx2_write_string", L_write_string);
    lua_register(L, "amxx2_write_entity", L_write_entity);
    lua_register(L, "amxx2_random_long", L_RandomLong);
    lua_register(L, "amxx2_random_float", L_RandomFloat);
    lua_register(L, "amxx2_time", L_Time); 
    lua_register(L, "amxx2_build_sound_msg", L_BuildSoundMsg);
    lua_register(L, "amxx2_sv_emit_sound2", L_SV_EmitSound2_call);
    lua_register(L, "amxx2_receiver_is_null", L_receiver_is_null);
    lua_register(L, "amxx2_receiver_equal", L_receiver_equal);
    lua_register(L, "amxx2_receiver_id", L_receiver_id);
    lua_register(L, "amxx2_receiver_to", L_receiver_to);
    lua_register(L, "amxx2_precache_model", L_PrecacheModel);
    lua_register(L, "amxx2_precache_sound", L_PrecacheSound);
    lua_register(L, "amxx2_set_model", L_SetModel);
    lua_register(L, "amxx2_model_index", L_ModelIndex);
    lua_register(L, "amxx2_model_frames", L_ModelFrames);
    lua_register(L, "amxx2_set_size", L_SetSize);
    lua_register(L, "amxx2_change_level", L_ChangeLevel);
    lua_register(L, "amxx2_get_spawn_parms", L_GetSpawnParms);
    lua_register(L, "amxx2_save_spawn_parms", L_SaveSpawnParms);
    lua_register(L, "amxx2_vec_to_yaw", L_VecToYaw);
    lua_register(L, "amxx2_vec_to_angles", L_VecToAngles);
    lua_register(L, "amxx2_move_to_origin", L_MoveToOrigin);
    lua_register(L, "amxx2_change_yaw", L_ChangeYaw);
    lua_register(L, "amxx2_change_pitch", L_ChangePitch);
    lua_register(L, "amxx2_find_entity_by_string", L_FindEntityByString);
    lua_register(L, "amxx2_get_entity_illum", L_GetEntityIllum);
    lua_register(L, "amxx2_find_entity_in_sphere", L_FindEntityInSphere);
    lua_register(L, "amxx2_find_client_in_pvs", L_FindClientInPVS);
    lua_register(L, "amxx2_entities_in_pvs", L_EntitiesInPVS);
    lua_register(L, "amxx2_remove_entity", L_RemoveEntity);
    lua_register(L, "amxx2_get_game_dir", L_GetGameDir);
    lua_register(L, "amxx2_trace_line", L_trace_line);
    lua_register(L, "amxx2_trace_toss", L_trace_toss);
    lua_register(L, "amxx2_trace_monster_hull", L_trace_monster_hull);
    lua_register(L, "amxx2_trace_hull", L_trace_hull);
    lua_register(L, "amxx2_get_es", L_get_es);
    lua_register(L, "amxx2_set_es", L_set_es);
}
static cell AMX_NATIVE_CALL n_lua_open(AMX *amx, cell *params)
{   
    lua_State *L = luaL_newstate();
    luaL_openlibs(L);
    InitLuaAPI(L);
    LuaInit(L);
    g_L = L;
    // luaopen_json(L);
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
    lua_getglobal((lua_State *)params[1], MF_GetAmxString(amx, params[2], 0, NULL));
    return lua_type((lua_State *)params[1], -1);
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
    lua_getfield((lua_State *)params[1], params[2], MF_GetAmxString(amx, params[3], 0, NULL));
    return lua_type((lua_State *)params[1], -1);
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
    lua_gettable((lua_State *)params[1], params[2]);
    return lua_type((lua_State *)params[1], -1);
}

static cell AMX_NATIVE_CALL n_lua_rawlen(AMX *amx, cell *params)
{
    if (!(lua_State *)params[1])
    {
        MF_Log("n_lua_rawlen: Invalid Lua state.");
        return 0;
    }
    return (cell)lua_objlen((lua_State *)params[1], params[2]);
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

    RehldsHookchains->ED_Alloc()->registerHook(&ED_Alloc, HC_PRIORITY_LOW);
    RehldsHookchains->ED_Free()->registerHook(&ED_Free, HC_PRIORITY_HIGH);
    RehldsHookchains->SV_EmitSound2()->registerHook(&SV_EmitSound2, HC_PRIORITY_HIGH);
    
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

/* 47. SV_EmitSound2 */
bool SV_EmitSound2(IRehldsHook_SV_EmitSound2 *chain,
                    edict_t *entity,
                    IGameClient *receiver,
                    int channel,
                    const char *sample,
                    float volume,
                    float attenuation,
                    int flags,
                    int pitch,
                    int emitFlags,
                    const float *pOrigin)
{
    if (g_L)
    {
        lua_getglobal(g_L, "Rehlds_SV_EmitSound2");
        if (lua_isfunction(g_L, -1))
        {
            lua_pushentity(g_L, entity);

            // IGameClient* receiver has no direct edict conversion here.
            // Expose it as lightuserdata so Lua can do identity comparisons.
            lua_pushlightuserdata(g_L, receiver);

            lua_pushinteger(g_L, channel);
            lua_pushstring(g_L, sample ? sample : "");
            lua_pushnumber(g_L, (lua_Number)volume);
            lua_pushnumber(g_L, (lua_Number)attenuation);
            lua_pushinteger(g_L, flags);
            lua_pushinteger(g_L, pitch);
            lua_pushinteger(g_L, emitFlags);

            lua_newtable(g_L);
            float origin[3] = {0.0f, 0.0f, 0.0f};
            if (pOrigin)
            {
                origin[0] = pOrigin[0];
                origin[1] = pOrigin[1];
                origin[2] = pOrigin[2];
            }
            for (int i = 0; i < 3; ++i)
            {
                lua_pushnumber(g_L, (lua_Number)origin[i]);
                lua_rawseti(g_L, -2, i + 1);
            }

            // Lua returns boolean: true => handled (skip next), false => continue chain.
            if (lua_pcall(g_L, 10, 1, 0) == 0)
            {
                if (lua_isboolean(g_L, -1))
                {
                    if (lua_toboolean(g_L, -1))
                    {
                        lua_pop(g_L, 1);
                        return chain->callNext(entity, receiver, channel, sample, volume, attenuation, flags, pitch, emitFlags, pOrigin);
                    }else{
                        lua_pop(g_L, 1);
                        return false;
                    }
                }
                lua_pop(g_L, 1);
            }
            else
            {
                lua_pop(g_L, 1);
            }
        }
        else
        {
            lua_pop(g_L, 1);
        }
    }

    return chain->callNext(entity, receiver, channel, sample, volume, attenuation, flags, pitch, emitFlags, pOrigin);
}


void DispatchTouch(edict_t *pentTouched, edict_t *pentOther) 
{
    if (g_L) 
    {
        lua_getglobal(g_L, "Meta_DispatchTouch"); 
        if (lua_isfunction(g_L, -1)) 
        {
            lua_pushentity(g_L, pentTouched);
            lua_pushentity(g_L, pentOther);
            if (lua_pcall(g_L, 2, 1, 0) == 0)
            {
                if (lua_isnumber(g_L, -1)) 
                {
                    int meta_res = lua_tointeger(g_L, -1);
                    lua_pop(g_L, 1); 
                    
                    RETURN_META((META_RES)meta_res);
                }
                lua_pop(g_L, 1);
            }
            else 
            {
                lua_pop(g_L, 1);
            }
        }
        else 
        {
            lua_pop(g_L, 1);
        }
    }
    RETURN_META(MRES_IGNORED);
}
void DispatchTouch_Post(edict_t *pentTouched, edict_t *pentOther) {
    if (g_L) {
        lua_getglobal(g_L, "Meta_DispatchTouch_Post");
        if (lua_isfunction(g_L, -1)) {
            lua_pushentity(g_L, pentTouched);
            lua_pushentity(g_L, pentOther);
            if (lua_pcall(g_L, 2, 1, 0) == 0) {
                if (lua_isnumber(g_L, -1)) {
                    int meta_res = lua_tointeger(g_L, -1);
                    lua_pop(g_L, 1);
                    RETURN_META((META_RES)meta_res);
                }
                lua_pop(g_L, 1);
            } else {
                lua_pop(g_L, 1);
            }
        } else {
            lua_pop(g_L, 1);
        }
    }
    RETURN_META(MRES_IGNORED);
}

int AddToFullPack(struct entity_state_s *state, int e, edict_t *ent, edict_t *host, int hostflags, int player, unsigned char *pSet)
{
    if (g_L) {
        lua_getglobal(g_L, "Meta_AddToFullPack");
        if (lua_isfunction(g_L, -1)) {
            lua_pushlightuserdata(g_L, state);
            lua_pushinteger(g_L, e);
            lua_pushentity(g_L, ent);
            lua_pushentity(g_L, host);
            lua_pushinteger(g_L, hostflags);
            lua_pushinteger(g_L, player);
            lua_pushlightuserdata(g_L, pSet);
            if (lua_pcall(g_L, 7, 1, 0) == 0) {
                if (lua_isnumber(g_L, -1)) {
                    int meta_res = lua_tointeger(g_L, -1);
                    lua_pop(g_L, 1);
                    RETURN_META_VALUE(MRES_IGNORED,(META_RES)meta_res);
                }
                lua_pop(g_L, 1);
            } else {
                lua_pop(g_L, 1);
            }
        } else {
            lua_pop(g_L, 1);
        }
    }
    RETURN_META_VALUE(MRES_IGNORED,0);
}
int AddToFullPack_Post(struct entity_state_s *state, int e, edict_t *ent, edict_t *host, int hostflags, int player, unsigned char *pSet)
{
    if (g_L) {
        lua_getglobal(g_L, "Meta_AddToFullPack_Post");
        if (lua_isfunction(g_L, -1)) {
            lua_pushlightuserdata(g_L, state);
            lua_pushinteger(g_L, e);
            lua_pushentity(g_L, ent);
            lua_pushentity(g_L, host);
            lua_pushinteger(g_L, hostflags);
            lua_pushinteger(g_L, player);
            lua_pushlightuserdata(g_L, pSet);
            if (lua_pcall(g_L, 7, 1, 0) == 0) {
                if (lua_isnumber(g_L, -1)) {
                    int meta_res = lua_tointeger(g_L, -1);
                    lua_pop(g_L, 1);
                    RETURN_META_VALUE(MRES_IGNORED,(META_RES)meta_res);
                }
                lua_pop(g_L, 1);
            } else {
                lua_pop(g_L, 1);
            }
        } else {
            lua_pop(g_L, 1);
        }
    }
    RETURN_META_VALUE(MRES_IGNORED,0);
}

int ShouldCollide(edict_t *pentTouched, edict_t *pentOther) {

    if (g_L) {
        lua_getglobal(g_L, "Meta_ShouldCollide");
        if (lua_isfunction(g_L, -1)) {
            lua_pushentity(g_L, pentTouched);
            lua_pushentity(g_L, pentOther);
            if (lua_pcall(g_L, 2, 1, 0) == 0) {
                if (lua_isnumber(g_L, -1)) {
                    int meta_res = lua_tointeger(g_L, -1);
                    lua_pop(g_L, 1);
                    RETURN_META_VALUE(MRES_SUPERCEDE,(META_RES)meta_res);
                }
                lua_pop(g_L, 1);
            } else {
                lua_pop(g_L, 1);
            }
        } else {
            lua_pop(g_L, 1);
        }
    }
    RETURN_META_VALUE(MRES_IGNORED,1);
}

void PM_Move(struct playermove_s *ppmove, int server ) {
    edict_t* ent = INDEXENT(ppmove->player_index+1);
	if (ppmove->deadflag != DEAD_NO || ppmove->dead || ppmove->spectator || (ent && ent->v.deadflag != DEAD_NO && ent->v.health<=0))
		return;

	int j, numphysent = -1;
	for (j = numphysent; j < ppmove->numphysent; ++j)
	{
		int entTarget = ppmove->physents[j].player;
		if (!entTarget)
		{
			ppmove->physents[numphysent++] = ppmove->physents[j];
			continue;
		}

        if (g_L) {
            lua_getglobal(g_L, "Meta_PM_Move_Player");
            if (lua_isfunction(g_L, -1)) {
                lua_pushinteger(g_L, ENTINDEX(ent));
                lua_pushinteger(g_L, entTarget);
                lua_pushnumber(g_L, GET_DISTANCE(ent->v.origin, INDEXENT(entTarget)->v.origin));
                if (lua_pcall(g_L, 3, 1, 0) == 0) {
                    if (lua_isboolean(g_L, -1)) {
                        if (lua_toboolean(g_L, -1))
                        {
                            lua_pop(g_L, 1);
                            ppmove->physents[numphysent++] = ppmove->physents[j];
			                continue;
                        }else{
                            lua_pop(g_L, 1);
                            continue;
                        }
                    }else{
                        lua_pop(g_L, 1);
                        ppmove->physents[numphysent++] = ppmove->physents[j];
			            continue;
                    }
                } else {
                    lua_pop(g_L, 1);
                    ppmove->physents[numphysent++] = ppmove->physents[j];
			        continue;
                }
            } else {
                lua_pop(g_L, 1);
                ppmove->physents[numphysent++] = ppmove->physents[j];
			    continue;
            }
        }
	}
	ppmove->numphysent = numphysent;
}

void CmdStart(const edict_t *player, const struct usercmd_s *_cmd, unsigned int random_seed) {
    if (g_L) {
        lua_getglobal(g_L, "Meta_CmdStart");
        if (lua_isfunction(g_L, -1)) {
            lua_pushentity(g_L, (edict_t*)player);
            lua_pushinteger(g_L, _cmd->buttons);
            lua_pushinteger(g_L, random_seed);
            if (lua_pcall(g_L, 3, 1, 0) == 0) {
                if (lua_isnumber(g_L, -1)) {
                    int meta_res = lua_tointeger(g_L, -1);
                    lua_pop(g_L, 1);
                    RETURN_META((META_RES)meta_res);
                }
                lua_pop(g_L, 1);
            } else {
                lua_pop(g_L, 1);
            }
        } else {
            lua_pop(g_L, 1);
        }
    }
    RETURN_META(MRES_IGNORED);
}

void CmdStart_Post(const edict_t *player, const struct usercmd_s *cmd, unsigned int random_seed) {
    if (g_L) {
        lua_getglobal(g_L, "Meta_CmdStart_Post");
        if (lua_isfunction(g_L, -1)) {
            lua_pushentity(g_L, (edict_t*)player);
            lua_pushinteger(g_L, cmd->buttons);
            lua_pushinteger(g_L, random_seed);
            if (lua_pcall(g_L, 3, 1, 0) == 0) {
                if (lua_isnumber(g_L, -1)) {
                    int meta_res = lua_tointeger(g_L, -1);
                    lua_pop(g_L, 1);
                    RETURN_META((META_RES)meta_res);
                }
                lua_pop(g_L, 1);
            } else {
                lua_pop(g_L, 1);
            }
        } else {
            lua_pop(g_L, 1);
        }
    }
    RETURN_META(MRES_IGNORED);
}

void CmdEnd(const edict_t *player) {
    if (g_L) {
        lua_getglobal(g_L, "Meta_CmdEnd");
        if (lua_isfunction(g_L, -1)) {
            lua_pushentity(g_L, (edict_t*)player);
            if (lua_pcall(g_L, 1, 1, 0) == 0) {
                if (lua_isnumber(g_L, -1)) {
                    int meta_res = lua_tointeger(g_L, -1);
                    lua_pop(g_L, 1);
                    RETURN_META((META_RES)meta_res);
                }
                lua_pop(g_L, 1);
            } else {
                lua_pop(g_L, 1);
            }
        } else {
            lua_pop(g_L, 1);
        }
    }
    RETURN_META(MRES_IGNORED);
}

void CmdEnd_Post(const edict_t *player) {
    if (g_L) {
        lua_getglobal(g_L, "Meta_CmdEnd_Post");
        if (lua_isfunction(g_L, -1)) {
            lua_pushentity(g_L, (edict_t*)player);
            if (lua_pcall(g_L, 1, 1, 0) == 0) {
                if (lua_isnumber(g_L, -1)) {
                    int meta_res = lua_tointeger(g_L, -1);
                    lua_pop(g_L, 1);
                    RETURN_META((META_RES)meta_res);
                }
                lua_pop(g_L, 1);
            } else {
                lua_pop(g_L, 1);
            }
        } else {
            lua_pop(g_L, 1);
        }
    }
    RETURN_META(MRES_IGNORED);
}
