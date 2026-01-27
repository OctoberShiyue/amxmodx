// vim: set ts=4 sw=4 tw=99 noet:
//
// JSON Lua Bindings
// Adapted from AMX Mod X JSON Natives
//

#include <main.h> // 确保这里包含你的 JSONMngr 定义和 ke::UniquePtr
#include <cstring>

// 假设 JsonMngr 是全局的或者在此处可见
ke::UniquePtr<JSONMngr> JsonMngr;

// 辅助宏：检查 Handle 有效性
#define CHECK_HANDLE(L, idx) \
    JS_Handle handle = (JS_Handle)luaL_checkinteger(L, idx); \
    if (!JsonMngr->IsValidHandle(handle)) { \
        return luaL_error(L, "Invalid JSON handle: %d", handle); \
    }

// 辅助宏：检查 Handle 有效性并验证类型
#define CHECK_HANDLE_TYPE(L, idx, type) \
    JS_Handle handle = (JS_Handle)luaL_checkinteger(L, idx); \
    if (!JsonMngr->IsValidHandle(handle, type)) { \
        return luaL_error(L, "Invalid JSON handle (type mismatch): %d", handle); \
    }

// lua: json.parse(string, is_file=false, with_comments=false) -> handle or nil
static int lua_json_parse(lua_State *L)
{
    size_t len;
    const char *str = luaL_checklstring(L, 1, &len);
    bool is_file = lua_toboolean(L, 2);
    bool with_comments = lua_toboolean(L, 3);

    JS_Handle handle;
    // 注意：去除了 AMX 特有的 path 构建，直接传参
    bool result = JsonMngr->Parse(str, &handle, is_file, with_comments);

    if (result) {
        lua_pushinteger(L, handle);
        return 1;
    }
    
    lua_pushnil(L);
    return 1;
}

// lua: json.equals(handle1, handle2) -> bool
static int lua_json_equals(lua_State *L)
{
    JS_Handle v1 = (JS_Handle)luaL_checkinteger(L, 1);
    JS_Handle v2 = (JS_Handle)luaL_checkinteger(L, 2);

    if (v1 == -1 || v2 == -1) {
        lua_pushboolean(L, v1 == v2);
        return 1;
    }

    if (!JsonMngr->IsValidHandle(v1) || !JsonMngr->IsValidHandle(v2)) {
        lua_pushboolean(L, 0);
        return 1;
    }

    lua_pushboolean(L, JsonMngr->AreValuesEquals(v1, v2));
    return 1;
}

// lua: json.validate(schema, value) -> bool
static int lua_json_validate(lua_State *L)
{
    CHECK_HANDLE(L, 1);
    JS_Handle schema = handle;
    
    // Check second handle manually to avoid redefinition error of macro
    JS_Handle value = (JS_Handle)luaL_checkinteger(L, 2);
    if (!JsonMngr->IsValidHandle(value)) {
        return luaL_error(L, "Invalid JSON value handle: %d", value);
    }

    lua_pushboolean(L, JsonMngr->IsValueValid(schema, value));
    return 1;
}

// lua: json.get_parent(value) -> handle or nil
static int lua_json_get_parent(lua_State *L)
{
    CHECK_HANDLE(L, 1);
    JS_Handle parent;
    if (JsonMngr->GetValueParent(handle, &parent)) {
        lua_pushinteger(L, parent);
    } else {
        lua_pushnil(L);
    }
    return 1;
}

// lua: json.get_type(value) -> int (JSONType)
static int lua_json_get_type(lua_State *L)
{
    CHECK_HANDLE(L, 1);
    lua_pushinteger(L, JsonMngr->GetHandleJSONType(handle));
    return 1;
}

// lua: json.init_object() -> handle
static int lua_json_init_object(lua_State *L)
{
    JS_Handle handle;
    if (JsonMngr->InitObject(&handle)) {
        lua_pushinteger(L, handle);
    } else {
        lua_pushnil(L);
    }
    return 1;
}

// lua: json.init_array() -> handle
static int lua_json_init_array(lua_State *L)
{
    JS_Handle handle;
    if (JsonMngr->InitArray(&handle)) {
        lua_pushinteger(L, handle);
    } else {
        lua_pushnil(L);
    }
    return 1;
}

// lua: json.init_string(value) -> handle
static int lua_json_init_string(lua_State *L)
{
    const char* val = luaL_checkstring(L, 1);
    JS_Handle handle;
    if (JsonMngr->InitString(val, &handle)) {
        lua_pushinteger(L, handle);
    } else {
        lua_pushnil(L);
    }
    return 1;
}

// lua: json.init_number(int_val) -> handle
static int lua_json_init_number(lua_State *L)
{
    int val = (int)luaL_checkinteger(L, 1);
    JS_Handle handle;
    if (JsonMngr->InitNum(val, &handle)) {
        lua_pushinteger(L, handle);
    } else {
        lua_pushnil(L);
    }
    return 1;
}

// lua: json.init_real(float_val) -> handle
static int lua_json_init_real(lua_State *L)
{
    float val = (float)luaL_checknumber(L, 1);
    JS_Handle handle;
    if (JsonMngr->InitNum(val, &handle)) {
        lua_pushinteger(L, handle);
    } else {
        lua_pushnil(L);
    }
    return 1;
}

// lua: json.init_bool(bool_val) -> handle
static int lua_json_init_bool(lua_State *L)
{
    bool val = lua_toboolean(L, 1);
    JS_Handle handle;
    if (JsonMngr->InitBool(val, &handle)) {
        lua_pushinteger(L, handle);
    } else {
        lua_pushnil(L);
    }
    return 1;
}

// lua: json.init_null() -> handle
static int lua_json_init_null(lua_State *L)
{
    JS_Handle handle;
    if (JsonMngr->InitNull(&handle)) {
        lua_pushinteger(L, handle);
    } else {
        lua_pushnil(L);
    }
    return 1;
}

// lua: json.deep_copy(value) -> handle
static int lua_json_deep_copy(lua_State *L)
{
    CHECK_HANDLE(L, 1);
    JS_Handle cloned;
    if (JsonMngr->DeepCopyValue(handle, &cloned)) {
        lua_pushinteger(L, cloned);
    } else {
        lua_pushnil(L);
    }
    return 1;
}

// lua: json.free(handle) -> bool
static int lua_json_free(lua_State *L)
{
    JS_Handle handle = (JS_Handle)luaL_checkinteger(L, 1);
    if (!JsonMngr->IsValidHandle(handle)) {
        lua_pushboolean(L, 0);
        return 1;
    }
    JsonMngr->Free(handle);
    lua_pushboolean(L, 1);
    return 1;
}

// lua: json.get_string(value) -> string
static int lua_json_get_string(lua_State *L)
{
    CHECK_HANDLE(L, 1);
    const char* str = JsonMngr->ValueToString(handle);
    lua_pushstring(L, str);
    return 1;
}

// lua: json.get_number(value) -> int
static int lua_json_get_number(lua_State *L)
{
    CHECK_HANDLE(L, 1);
    lua_pushinteger(L, (lua_Integer)JsonMngr->ValueToNum(handle));
    return 1;
}

// lua: json.get_real(value) -> float
static int lua_json_get_real(lua_State *L)
{
    CHECK_HANDLE(L, 1);
    lua_pushnumber(L, (lua_Number)JsonMngr->ValueToNum(handle));
    return 1;
}

// lua: json.get_bool(value) -> bool
static int lua_json_get_bool(lua_State *L)
{
    CHECK_HANDLE(L, 1);
    lua_pushboolean(L, JsonMngr->ValueToBool(handle));
    return 1;
}

// lua: json.array_get_value(array, index) -> handle
static int lua_json_array_get_value(lua_State *L)
{
    CHECK_HANDLE_TYPE(L, 1, Handle_Array);
    int index = (int)luaL_checkinteger(L, 2);

    JS_Handle resHandle;
    if (JsonMngr->ArrayGetValue(handle, index, &resHandle)) {
        lua_pushinteger(L, resHandle);
    } else {
        lua_pushnil(L);
    }
    return 1;
}

// lua: json.array_get_string(array, index) -> string
static int lua_json_array_get_string(lua_State *L)
{
    CHECK_HANDLE_TYPE(L, 1, Handle_Array);
    int index = (int)luaL_checkinteger(L, 2);
    
    const char* str = JsonMngr->ArrayGetString(handle, index);
    if (str) lua_pushstring(L, str);
    else lua_pushnil(L);
    return 1;
}

// lua: json.array_get_number(array, index) -> int
static int lua_json_array_get_number(lua_State *L)
{
    CHECK_HANDLE_TYPE(L, 1, Handle_Array);
    int index = (int)luaL_checkinteger(L, 2);
    lua_pushinteger(L, (lua_Integer)JsonMngr->ArrayGetNum(handle, index));
    return 1;
}

// lua: json.array_get_real(array, index) -> float
static int lua_json_array_get_real(lua_State *L)
{
    CHECK_HANDLE_TYPE(L, 1, Handle_Array);
    int index = (int)luaL_checkinteger(L, 2);
    lua_pushnumber(L, (lua_Number)JsonMngr->ArrayGetNum(handle, index));
    return 1;
}

// lua: json.array_get_bool(array, index) -> bool
static int lua_json_array_get_bool(lua_State *L)
{
    CHECK_HANDLE_TYPE(L, 1, Handle_Array);
    int index = (int)luaL_checkinteger(L, 2);
    lua_pushboolean(L, JsonMngr->ArrayGetBool(handle, index));
    return 1;
}

// lua: json.array_get_count(array) -> int
static int lua_json_array_get_count(lua_State *L)
{
    CHECK_HANDLE_TYPE(L, 1, Handle_Array);
    lua_pushinteger(L, JsonMngr->ArrayGetCount(handle));
    return 1;
}

// lua: json.array_replace_value(array, index, value) -> bool
static int lua_json_array_replace_value(lua_State *L)
{
    CHECK_HANDLE_TYPE(L, 1, Handle_Array);
    int index = (int)luaL_checkinteger(L, 2);
    JS_Handle val = (JS_Handle)luaL_checkinteger(L, 3);
    
    if (!JsonMngr->IsValidHandle(val)) {
         return luaL_error(L, "Invalid replacement value handle");
    }

    lua_pushboolean(L, JsonMngr->ArrayReplaceValue(handle, index, val));
    return 1;
}

// lua: json.array_replace_string(array, index, string) -> bool
static int lua_json_array_replace_string(lua_State *L)
{
    CHECK_HANDLE_TYPE(L, 1, Handle_Array);
    int index = (int)luaL_checkinteger(L, 2);
    const char* str = luaL_checkstring(L, 3);

    lua_pushboolean(L, JsonMngr->ArrayReplaceString(handle, index, str));
    return 1;
}

// lua: json.array_replace_number(array, index, int) -> bool
static int lua_json_array_replace_number(lua_State *L)
{
    CHECK_HANDLE_TYPE(L, 1, Handle_Array);
    int index = (int)luaL_checkinteger(L, 2);
    int val = (int)luaL_checkinteger(L, 3);

    lua_pushboolean(L, JsonMngr->ArrayReplaceNum(handle, index, val));
    return 1;
}

// lua: json.array_replace_real(array, index, float) -> bool
static int lua_json_array_replace_real(lua_State *L)
{
    CHECK_HANDLE_TYPE(L, 1, Handle_Array);
    int index = (int)luaL_checkinteger(L, 2);
    float val = (float)luaL_checknumber(L, 3);

    lua_pushboolean(L, JsonMngr->ArrayReplaceNum(handle, index, val));
    return 1;
}

// lua: json.array_replace_bool(array, index, bool) -> bool
static int lua_json_array_replace_bool(lua_State *L)
{
    CHECK_HANDLE_TYPE(L, 1, Handle_Array);
    int index = (int)luaL_checkinteger(L, 2);
    bool val = lua_toboolean(L, 3);

    lua_pushboolean(L, JsonMngr->ArrayReplaceBool(handle, index, val));
    return 1;
}

// lua: json.array_replace_null(array, index) -> bool
static int lua_json_array_replace_null(lua_State *L)
{
    CHECK_HANDLE_TYPE(L, 1, Handle_Array);
    int index = (int)luaL_checkinteger(L, 2);
    lua_pushboolean(L, JsonMngr->ArrayReplaceNull(handle, index));
    return 1;
}

// lua: json.array_append_value(array, value) -> bool
static int lua_json_array_append_value(lua_State *L)
{
    CHECK_HANDLE_TYPE(L, 1, Handle_Array);
    JS_Handle val = (JS_Handle)luaL_checkinteger(L, 2);
    if (!JsonMngr->IsValidHandle(val)) return luaL_error(L, "Invalid value handle");

    lua_pushboolean(L, JsonMngr->ArrayAppendValue(handle, val));
    return 1;
}

// lua: json.array_append_string(array, string) -> bool
static int lua_json_array_append_string(lua_State *L)
{
    CHECK_HANDLE_TYPE(L, 1, Handle_Array);
    const char* str = luaL_checkstring(L, 2);
    lua_pushboolean(L, JsonMngr->ArrayAppendString(handle, str));
    return 1;
}

// lua: json.array_append_number(array, int) -> bool
static int lua_json_array_append_number(lua_State *L)
{
    CHECK_HANDLE_TYPE(L, 1, Handle_Array);
    int val = (int)luaL_checkinteger(L, 2);
    lua_pushboolean(L, JsonMngr->ArrayAppendNum(handle, val));
    return 1;
}

// lua: json.array_append_real(array, float) -> bool
static int lua_json_array_append_real(lua_State *L)
{
    CHECK_HANDLE_TYPE(L, 1, Handle_Array);
    float val = (float)luaL_checknumber(L, 2);
    lua_pushboolean(L, JsonMngr->ArrayAppendNum(handle, val));
    return 1;
}

// lua: json.array_append_bool(array, bool) -> bool
static int lua_json_array_append_bool(lua_State *L)
{
    CHECK_HANDLE_TYPE(L, 1, Handle_Array);
    bool val = lua_toboolean(L, 2);
    lua_pushboolean(L, JsonMngr->ArrayAppendBool(handle, val));
    return 1;
}

// lua: json.array_append_null(array) -> bool
static int lua_json_array_append_null(lua_State *L)
{
    CHECK_HANDLE_TYPE(L, 1, Handle_Array);
    lua_pushboolean(L, JsonMngr->ArrayAppendNull(handle));
    return 1;
}

// lua: json.array_remove(array, index) -> bool
static int lua_json_array_remove(lua_State *L)
{
    CHECK_HANDLE_TYPE(L, 1, Handle_Array);
    int index = (int)luaL_checkinteger(L, 2);
    lua_pushboolean(L, JsonMngr->ArrayRemove(handle, index));
    return 1;
}

// lua: json.array_clear(array) -> bool
static int lua_json_array_clear(lua_State *L)
{
    CHECK_HANDLE_TYPE(L, 1, Handle_Array);
    lua_pushboolean(L, JsonMngr->ArrayClear(handle));
    return 1;
}

// lua: json.object_get_value(object, name, dotfunc=false) -> handle
static int lua_json_object_get_value(lua_State *L)
{
    CHECK_HANDLE_TYPE(L, 1, Handle_Object);
    const char* name = luaL_checkstring(L, 2);
    bool dotfunc = lua_toboolean(L, 3);

    JS_Handle resHandle;
    if (JsonMngr->ObjectGetValue(handle, name, &resHandle, dotfunc)) {
        lua_pushinteger(L, resHandle);
    } else {
        lua_pushnil(L);
    }
    return 1;
}

// lua: json.object_get_string(object, name, dotfunc=false) -> string
static int lua_json_object_get_string(lua_State *L)
{
    CHECK_HANDLE_TYPE(L, 1, Handle_Object);
    const char* name = luaL_checkstring(L, 2);
    bool dotfunc = lua_toboolean(L, 3); // 原 AMX 中 buffer/maxlen 在 params 3,4，所以 dotfunc 是 params[5]，这里是 3

    const char* res = JsonMngr->ObjectGetString(handle, name, dotfunc);
    if (res) lua_pushstring(L, res);
    else lua_pushnil(L);
    return 1;
}

// lua: json.object_get_number(object, name, dotfunc=false) -> int
static int lua_json_object_get_number(lua_State *L)
{
    CHECK_HANDLE_TYPE(L, 1, Handle_Object);
    const char* name = luaL_checkstring(L, 2);
    bool dotfunc = lua_toboolean(L, 3);
    lua_pushinteger(L, (lua_Integer)JsonMngr->ObjectGetNum(handle, name, dotfunc));
    return 1;
}

// lua: json.object_get_real(object, name, dotfunc=false) -> float
static int lua_json_object_get_real(lua_State *L)
{
    CHECK_HANDLE_TYPE(L, 1, Handle_Object);
    const char* name = luaL_checkstring(L, 2);
    bool dotfunc = lua_toboolean(L, 3);
    lua_pushnumber(L, (lua_Number)JsonMngr->ObjectGetNum(handle, name, dotfunc));
    return 1;
}

// lua: json.object_get_bool(object, name, dotfunc=false) -> bool
static int lua_json_object_get_bool(lua_State *L)
{
    CHECK_HANDLE_TYPE(L, 1, Handle_Object);
    const char* name = luaL_checkstring(L, 2);
    bool dotfunc = lua_toboolean(L, 3);
    lua_pushboolean(L, JsonMngr->ObjectGetBool(handle, name, dotfunc));
    return 1;
}

// lua: json.object_get_count(object) -> int
static int lua_json_object_get_count(lua_State *L)
{
    CHECK_HANDLE_TYPE(L, 1, Handle_Object);
    lua_pushinteger(L, JsonMngr->ObjectGetCount(handle));
    return 1;
}

// lua: json.object_get_name(object, index) -> string
static int lua_json_object_get_name(lua_State *L)
{
    CHECK_HANDLE_TYPE(L, 1, Handle_Object);
    int index = (int)luaL_checkinteger(L, 2);
    const char* name = JsonMngr->ObjectGetName(handle, index);
    if (name) lua_pushstring(L, name);
    else lua_pushnil(L);
    return 1;
}

// lua: json.object_get_value_at(object, index) -> handle
static int lua_json_object_get_value_at(lua_State *L)
{
    CHECK_HANDLE_TYPE(L, 1, Handle_Object);
    int index = (int)luaL_checkinteger(L, 2);
    
    JS_Handle resHandle;
    if (JsonMngr->ObjectGetValueAt(handle, index, &resHandle)) {
        lua_pushinteger(L, resHandle);
    } else {
        lua_pushnil(L);
    }
    return 1;
}

// lua: json.object_has_value(object, name, type=JSONError, dot_not=false) -> bool
static int lua_json_object_has_value(lua_State *L)
{
    CHECK_HANDLE_TYPE(L, 1, Handle_Object);
    const char* name = luaL_checkstring(L, 2);
    int type = lua_isnoneornil(L, 3) ? (int)JSONError : (int)lua_tointeger(L, 3);
    bool dot_not = lua_toboolean(L, 4);

    lua_pushboolean(L, JsonMngr->ObjectHasValue(handle, name, (JSONType)type, dot_not));
    return 1;
}

// lua: json.object_set_value(object, name, value, dotfunc=false) -> bool
static int lua_json_object_set_value(lua_State *L)
{
    CHECK_HANDLE_TYPE(L, 1, Handle_Object);
    const char* name = luaL_checkstring(L, 2);
    JS_Handle val = (JS_Handle)luaL_checkinteger(L, 3);
    bool dotfunc = lua_toboolean(L, 4);

    if (!JsonMngr->IsValidHandle(val)) return luaL_error(L, "Invalid value handle");

    lua_pushboolean(L, JsonMngr->ObjectSetValue(handle, name, val, dotfunc));
    return 1;
}

// lua: json.object_set_string(object, name, string, dotfunc=false) -> bool
static int lua_json_object_set_string(lua_State *L)
{
    CHECK_HANDLE_TYPE(L, 1, Handle_Object);
    const char* name = luaL_checkstring(L, 2);
    const char* str = luaL_checkstring(L, 3);
    bool dotfunc = lua_toboolean(L, 4);

    lua_pushboolean(L, JsonMngr->ObjectSetString(handle, name, str, dotfunc));
    return 1;
}

// lua: json.object_set_number(object, name, int, dotfunc=false) -> bool
static int lua_json_object_set_number(lua_State *L)
{
    CHECK_HANDLE_TYPE(L, 1, Handle_Object);
    const char* name = luaL_checkstring(L, 2);
    int val = (int)luaL_checkinteger(L, 3);
    bool dotfunc = lua_toboolean(L, 4);

    lua_pushboolean(L, JsonMngr->ObjectSetNum(handle, name, val, dotfunc));
    return 1;
}

// lua: json.object_set_real(object, name, float, dotfunc=false) -> bool
static int lua_json_object_set_real(lua_State *L)
{
    CHECK_HANDLE_TYPE(L, 1, Handle_Object);
    const char* name = luaL_checkstring(L, 2);
    float val = (float)luaL_checknumber(L, 3);
    bool dotfunc = lua_toboolean(L, 4);

    lua_pushboolean(L, JsonMngr->ObjectSetNum(handle, name, val, dotfunc));
    return 1;
}

// lua: json.object_set_bool(object, name, bool, dotfunc=false) -> bool
static int lua_json_object_set_bool(lua_State *L)
{
    CHECK_HANDLE_TYPE(L, 1, Handle_Object);
    const char* name = luaL_checkstring(L, 2);
    bool val = lua_toboolean(L, 3);
    bool dotfunc = lua_toboolean(L, 4);

    lua_pushboolean(L, JsonMngr->ObjectSetBool(handle, name, val, dotfunc));
    return 1;
}

// lua: json.object_set_null(object, name, dotfunc=false) -> bool
static int lua_json_object_set_null(lua_State *L)
{
    CHECK_HANDLE_TYPE(L, 1, Handle_Object);
    const char* name = luaL_checkstring(L, 2);
    bool dotfunc = lua_toboolean(L, 3);

    lua_pushboolean(L, JsonMngr->ObjectSetNull(handle, name, dotfunc));
    return 1;
}

// lua: json.object_remove(object, name, dotfunc=false) -> bool
static int lua_json_object_remove(lua_State *L)
{
    CHECK_HANDLE_TYPE(L, 1, Handle_Object);
    const char* name = luaL_checkstring(L, 2);
    bool dotfunc = lua_toboolean(L, 3);

    lua_pushboolean(L, JsonMngr->ObjectRemove(handle, name, dotfunc));
    return 1;
}

// lua: json.object_clear(object) -> bool
static int lua_json_object_clear(lua_State *L)
{
    CHECK_HANDLE_TYPE(L, 1, Handle_Object);
    lua_pushboolean(L, JsonMngr->ObjectClear(handle));
    return 1;
}

// lua: json.serial_size(value, pretty=false, with_null_byte=false) -> int
static int lua_json_serial_size(lua_State *L)
{
    CHECK_HANDLE(L, 1);
    bool pretty = lua_toboolean(L, 2);
    bool with_null = lua_toboolean(L, 3);

    size_t size = JsonMngr->SerialSize(handle, pretty);
    lua_pushinteger(L, with_null ? size : size - 1);
    return 1;
}

// lua: json.serial_to_string(value, pretty=false) -> string
static int lua_json_serial_to_string(lua_State *L)
{
    CHECK_HANDLE(L, 1);
    bool pretty = lua_toboolean(L, 2);

    char* result = JsonMngr->SerialToString(handle, pretty);
    if (result) {
        lua_pushstring(L, result);
        JsonMngr->FreeString(result);
    } else {
        lua_pushnil(L);
    }
    return 1;
}

// lua: json.serial_to_file(value, file_path, pretty=false) -> bool
static int lua_json_serial_to_file(lua_State *L)
{
    CHECK_HANDLE(L, 1);
    const char* file = luaL_checkstring(L, 2);
    bool pretty = lua_toboolean(L, 3);

    // 去除了 AMX BuildPathnameR，直接使用传入的文件路径
    lua_pushboolean(L, JsonMngr->SerialToFile(handle, file, pretty));
    return 1;
}

static const luaL_Reg json_lib[] = {
    { "parse",                     lua_json_parse },
    { "equals",                    lua_json_equals },
    { "validate",                  lua_json_validate },
    { "get_parent",                lua_json_get_parent },
    { "get_type",                  lua_json_get_type },
    { "init_object",               lua_json_init_object },
    { "init_array",                lua_json_init_array },
    { "init_string",               lua_json_init_string },
    { "init_number",               lua_json_init_number },
    { "init_real",                 lua_json_init_real },
    { "init_bool",                 lua_json_init_bool },
    { "init_null",                 lua_json_init_null },
    { "deep_copy",                 lua_json_deep_copy },
    { "free",                      lua_json_free },
    { "get_string",                lua_json_get_string },
    { "get_number",                lua_json_get_number },
    { "get_real",                  lua_json_get_real },
    { "get_bool",                  lua_json_get_bool },
    { "array_get_value",           lua_json_array_get_value },
    { "array_get_string",          lua_json_array_get_string },
    { "array_get_count",           lua_json_array_get_count },
    { "array_get_number",          lua_json_array_get_number },
    { "array_get_real",            lua_json_array_get_real },
    { "array_get_bool",            lua_json_array_get_bool },
    { "array_replace_value",       lua_json_array_replace_value },
    { "array_replace_string",      lua_json_array_replace_string },
    { "array_replace_number",      lua_json_array_replace_number },
    { "array_replace_real",        lua_json_array_replace_real },
    { "array_replace_bool",        lua_json_array_replace_bool },
    { "array_replace_null",        lua_json_array_replace_null },
    { "array_append_value",        lua_json_array_append_value },
    { "array_append_string",       lua_json_array_append_string },
    { "array_append_number",       lua_json_array_append_number },
    { "array_append_real",         lua_json_array_append_real },
    { "array_append_bool",         lua_json_array_append_bool },
    { "array_append_null",         lua_json_array_append_null },
    { "array_remove",              lua_json_array_remove },
    { "array_clear",               lua_json_array_clear },
    { "object_get_value",          lua_json_object_get_value },
    { "object_get_string",         lua_json_object_get_string },
    { "object_get_number",         lua_json_object_get_number },
    { "object_get_real",           lua_json_object_get_real },
    { "object_get_bool",           lua_json_object_get_bool },
    { "object_get_count",          lua_json_object_get_count },
    { "object_get_name",           lua_json_object_get_name },
    { "object_get_value_at",       lua_json_object_get_value_at },
    { "object_has_value",          lua_json_object_has_value },
    { "object_set_value",          lua_json_object_set_value },
    { "object_set_string",         lua_json_object_set_string },
    { "object_set_number",         lua_json_object_set_number },
    { "object_set_real",           lua_json_object_set_real },
    { "object_set_bool",           lua_json_object_set_bool },
    { "object_set_null",           lua_json_object_set_null },
    { "object_remove",             lua_json_object_remove },
    { "object_clear",              lua_json_object_clear },
    { "serial_size",               lua_json_serial_size },
    { "serial_to_string",          lua_json_serial_to_string },
    { "serial_to_file",            lua_json_serial_to_file },
    { NULL, NULL }
};

// 注册函数
// 在你的主模块初始化代码中调用此函数 (例如 luaopen_module)
// 确保包含这个初始化检查
extern "C" int luaopen_json(lua_State *L) {
    // 1. 如果指针为空，先分配内存
    if (!JsonMngr) {
        JsonMngr = ke::MakeUnique<JSONMngr>();
    }

    // 2. 注册库
    luaL_register(L, "json", json_lib); 
    return 1;
}