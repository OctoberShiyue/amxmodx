#include <amxxmodule.h>
#include <parson.h>
#include "JsonMngr.h"
#include <amtl/am-vector.h>
#include <amtl/am-autoptr.h>
#include <amtl/am-uniqueptr.h>
#include <amtl/am-deque.h>
#include <amtl/am-string.h>
#include <amtl/am-hashmap.h>
#include <sm_stringhashmap.h>
#include <resdk/mod_rehlds_api.h>
#include <resdk/mod_regamedll_api.h>
#include "lualib/lua.hpp"
#define CURL_STATICLIB  // 必须在 include 之前
#include "curl/curl.h"
#include <string>

void StartFrame2();
void OnPluginsLoaded2();
void LuaInit(lua_State *L);

extern "C" int luaopen_json(lua_State *L);