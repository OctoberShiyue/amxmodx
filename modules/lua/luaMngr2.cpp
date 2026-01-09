#include "luaMngr.h"
lua_State *g_lLuaL;
AMX *g_amx = NULL;
#define STKMARGIN ((cell)(16 * sizeof(cell)))

static int l_CallNative(lua_State *L)
{
	int n = lua_gettop(L);
	if (n < 1)
	{
		return luaL_error(L, "CallNative expects at least native name");
	}
	ke::AString nativeName;
	nativeName = luaL_checkstring(L, 1);
	if (nativeName.length() == 0)
	{
		luaL_error(L, "CallNative: empty native name");
	}

	int numArgs = n - 1;
	cell newparams[16];
	newparams[0] = numArgs * sizeof(cell);
	cell numcells, err;
	for (int arg = 0; arg < numArgs; arg++)
	{
		cell val_addr;
		cell *addr;
		int luaIdx = arg + 2;
		int paramIdx = arg + 1;
		numcells += 8 * sizeof(cell);
		if (lua_isinteger(L, luaIdx))
		{
			if (g_amx->hea + STKMARGIN > g_amx->stk)
			{
				AMX_HEADER *hdr;
				unsigned char *data;
				hdr = (AMX_HEADER *)g_amx->base;
				data=(g_amx->data!=NULL) ? g_amx->data : g_amx->base+(int)hdr->dat;
				g_amx->stk-=sizeof(cell);
				// g_amx->paramcount+=1;
				*(cell *)(data+(int)g_amx->stk)=(cell)lua_tointeger(L, luaIdx);
				newparams[paramIdx] = *(cell *)(data+(int)g_amx->stk);
			}
			// // numcells+= 64*sizeof(cell);
			// err=MF_AmxAllot(g_amx,numcells,&val_addr,&addr);
			// if (err==AMX_ERR_NONE)
			// 	*addr=(cell)lua_tointeger(L, luaIdx);
			// cell *addr = MF_GetAmxAddr(g_amx, numcells);
			// *addr=(cell)lua_tointeger(L, luaIdx);
		}
		else if (lua_isboolean(L, luaIdx))
		{
			// numcells+= 64*sizeof(cell);
			// err=MF_AmxAllot(g_amx,numcells,&val_addr,&addr);
			// if (err==AMX_ERR_NONE)
			// 	*addr = lua_toboolean(L, luaIdx) ? 1 : 0;
		}
		else if (lua_isnumber(L, luaIdx))
		{
			// numcells+= 64*sizeof(cell);
			// err=MF_AmxAllot(g_amx,numcells,&val_addr,&addr);
			// if (err==AMX_ERR_NONE)
			// 	*addr = amx_ftoc((REAL)lua_tonumber(L, luaIdx));
		}
		else if (lua_isstring(L, luaIdx))
		{
			ke::AString str;
			str = lua_tostring(L, luaIdx);
			numcells += str.length() + 1;
			err = MF_AmxAllot(g_amx, numcells, &val_addr, &addr);
			if (err == AMX_ERR_NONE)
				MF_SetAmxString(g_amx, val_addr, str.chars(), str.length() + 1);

			newparams[paramIdx] = val_addr;
		}
		else
		{
			luaL_error(L, "CallNative: unsupported param type for param %d", arg + 1);
		}
	}

	cell result = UTIL_ExecNative(g_amx, nativeName.chars(), newparams);
	lua_pushinteger(L, result);
	// 缺少返回值处理，因为可能有多种类型，暂时只返回整数，整数，浮点
	return 1;
}

cell UTIL_ExecNative(AMX *amx, const char *Nativename, cell *params)
{
	char blah[64];
	int NativeIndex;
	int i = 0;
	strncpy(blah, Nativename, 63);
	if (MF_AmxFindNative(amx, blah, &NativeIndex) == AMX_ERR_NONE)
	{

		AMX_HEADER *hdr = (AMX_HEADER *)amx->base;
		AMX_FUNCSTUBNT *natives = (AMX_FUNCSTUBNT *)(amx->base + hdr->natives);
		AMX_NATIVE func = (AMX_NATIVE)natives[NativeIndex].address;
		return func(amx, params);
	}
	return 0;
}
static cell AMX_NATIVE_CALL lua_ini(AMX *amx, cell *params)
{
	if (g_lLuaL)
	{
		lua_close(g_lLuaL);
		g_lLuaL = NULL;
	}
	g_lLuaL = luaL_newstate(); /* create state */
	if (g_lLuaL == NULL)
	{
		return 0;
	}
	g_amx = amx;
	printf("####################################################\n");
	luaL_openlibs(g_lLuaL);

	// =======================================================
	// [START] 新增路径逻辑: addons\amxmodx\luascripting
	// =======================================================
	lua_getglobal(g_lLuaL, "package");					  // 1. 将 package 表压入栈 (索引 -1)
	lua_getfield(g_lLuaL, -1, "path");					  // 2. 获取 package.path 压入栈 (索引 -1，package 变 -2)
	const char *current_path = lua_tostring(g_lLuaL, -1); // 3. 读取当前路径字符串

	// 4. 拼接新路径。
	// 注意：
	// 1. 使用分号 ; 分隔。
	// 2. 必须加上 /?.lua，这样 require "xx" 才会查找 "luascripting/xx.lua"。
	// 3. 建议使用正斜杠 /，Lua 在 Windows 下也能识别，且避免 C++ 转义字符烦恼。
	lua_pushfstring(g_lLuaL, "%s;cstrike\\addons\\amxmodx\\luascripting\\?.lua", current_path);

	lua_setfield(g_lLuaL, -3, "path"); // 5. 设置 package.path = 新字符串 (package 表在索引 -3)
	lua_pop(g_lLuaL, 2);			   // 6. 弹出栈顶的 package 表和旧 path 字符串，恢复堆栈平衡

	// =======================================================
	// [END] 新增路径逻辑
	// =======================================================
	lua_register(g_lLuaL, "CallNative", l_CallNative);

	lua_gc(g_lLuaL, LUA_GCGEN, 0, 0);
	lua_getglobal(g_lLuaL, "require");
	lua_pushstring(g_lLuaL, "lua_main");

	// 注意：docall 函数在你提供的片段中未定义，假设它在你的项目中已存在
	int r_status = docall(g_lLuaL, 1, 1);

	if (r_status != LUA_OK)
		printf("lua_main_fail\n");

	// printf("get_maxplayers=%d\n", UTIL_ExecNative(amx, "get_maxplayers", nullptr));

	// 传递整数
	// cell fmt_addr = params[0];
	// cell val_addr = 32*sizeof(cell); // 预留空间存放整数
	// printf("fmt_addr=%d,val_addr=%d\n", fmt_addr, val_addr);
	// cell *addr = MF_GetAmxAddr(amx, val_addr); // 获取地址
	// *addr = 1;						   // 存入值
	// MF_SetAmxString(amx, fmt_addr, "test: %d", strlen("test: %d") + 1);
	// cell newparams[3];
	// newparams[0] = 2 * sizeof(cell); // 2个参数
	// newparams[1] = fmt_addr;		 // 格式字符串地址
	// newparams[2] = val_addr;		 // 格式字符串地址
	// printf("log_amx: %d\n", UTIL_ExecNative(amx, "log_amx", newparams));

	// // 传递浮点
	// cell fmt_addr = params[0];
	// cell val_addr = 32*sizeof(cell); // 预留空间存放整数
	// cell *addr = MF_GetAmxAddr(amx, val_addr); // 获取地址
	// *addr = amx_ftoc(3.1415926);						   // 存入值
	// MF_SetAmxString(amx, fmt_addr, "test: %.3f", strlen("test: %.3f") + 1);
	// cell newparams[3];
	// newparams[0] = 2 * sizeof(cell); // 2个参数
	// newparams[1] = fmt_addr;		 // 格式字符串地址
	// newparams[2] = val_addr;		 // 格式字符串地址
	// printf("log_amx: %d\n", UTIL_ExecNative(amx, "log_amx", newparams));

	// // 传递字符串
	// cell fmt_addr = 32 * sizeof(cell);
	// cell val_addr = 32 * sizeof(cell);
	// cell *addr1 = MF_GetAmxAddr(amx, fmt_addr); // 获取地址
	// cell *addr2 = MF_GetAmxAddr(amx, val_addr); // 获取地址
	// ke::AString str1;
	// ke::AString str2;
	// str1 = "aaaaa=%s";
	// str2 = "bbbbbbbb";
	// strncopy(addr1, str1.chars(), str1.length() + 1);
	// strncopy(addr2, str2.chars(), str2.length() + 1);
	// cell newparams[3];
	// newparams[0] = 2 * sizeof(cell); // 2个参数
	// newparams[1] = fmt_addr;		 // 格式字符串地址
	// newparams[2] = val_addr;		 // 格式字符串地址
	// printf("log_amx: %d\n", UTIL_ExecNative(amx, "log_amx", newparams));

	// 传递字符串
	// cell fmt_addr = 32 * sizeof(cell);
	// cell val_addr = 64 * sizeof(cell);
	// cell *addr1 = MF_GetAmxAddr(amx, fmt_addr); // 获取地址
	// cell *addr2 = MF_GetAmxAddr(amx, val_addr); // 获取地址
	// ke::AString str1;
	// ke::AString str2;
	// str1 = "aaaaa=%s";
	// str2 = "bbbbbbbb";
	// strncopy(addr1, str1.chars(), str1.length() + 1);
	// strncopy(addr2, str2.chars(), str2.length() + 1);
	// cell newparams[3];
	// newparams[0] = 2 * sizeof(cell); // 2个参数
	// newparams[1] = fmt_addr;		 // 格式字符串地址
	// newparams[2] = val_addr;		 // 格式字符串地址
	// printf("log_amx: %d\n", UTIL_ExecNative(amx, "log_amx", newparams));
	// printf("======%d=%d\n",fmt_addr,val_addr);

	printf("####################################################\n");
	return true;
}

AMX_NATIVE_INFO LuaNatives[] = {
	{"lua_ini", lua_ini},
	// {"lua_test_printf", lua_test_printf},
	{nullptr, nullptr}};

void OnAmxxAttach()
{
	MF_AddNatives(LuaNatives);
}
