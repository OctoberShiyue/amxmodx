#include <amxxmodule.h>
#include <parson.h>
#include <amtl/am-vector.h>
#include <amtl/am-autoptr.h>
#include <amtl/am-uniqueptr.h>
#include <amtl/am-deque.h>
#include <amtl/am-string.h>
#include <amtl/am-hashmap.h>
#include <sm_stringhashmap.h>
#include <resdk/mod_rehlds_api.h>
#include <resdk/mod_regamedll_api.h>

extern "C" {
#include <lualib/lua.c>
#include <lualib/lualib.h>
#include <lualib/lauxlib.h>
}
void StartFrame2();
void OnPluginsLoaded2();