// vim: set ts=4 sw=4 tw=99 noet:
//
// AMX Mod X, based on AMX Mod by Aleksander Naszko ("OLO").
// Copyright (C) The AMX Mod X Development Team.
//
// This software is licensed under the GNU General Public License, version 3 or higher.
// Additional exceptions apply. For full license details, see LICENSE.txt or visit:
//     https://alliedmods.net/amxmodx-license

//
// Fakemeta Module
//

#ifndef FORWARDMACROS_H
#define FORWARDMACROS_H


#define SIMPLE_CONSTSTRING_HOOK_VOID(call) \
	const char* call () \
	{ \
		FM_ENG_HANDLE0(FM_##call, (Engine[FM_##call].at(i))); \
		LUA_SIMPLE_CONSTSTRING_HOOK_VOID(call, ) \
		RETURN_META_VALUE(mswi(lastFmRes), mlStringResult); \
	} \
	const char* call##_post () \
	{ \
		origStringRet = META_RESULT_ORIG_RET(const char *); \
		FM_ENG_HANDLE_POST0(FM_##call, (EnginePost[FM_##call].at(i))); \
		LUA_SIMPLE_CONSTSTRING_HOOK_VOID(call, POST) \
		RETURN_META_VALUE(MRES_IGNORED, mlStringResult); \
	}

#define SIMPLE_CONSTSTRING_HOOK_INT(call) \
	const char* call (int v) \
	{ \
		FM_ENG_HANDLE(FM_##call, (Engine[FM_##call].at(i),(cell)v)); \
		LUA_SIMPLE_CONSTSTRING_HOOK_INT(call, , v) \
		RETURN_META_VALUE(mswi(lastFmRes), mlStringResult); \
	} \
	const char* call##_post (int v) \
	{ \
		origStringRet = META_RESULT_ORIG_RET(const char *); \
		FM_ENG_HANDLE_POST(FM_##call, (EnginePost[FM_##call].at(i),(cell)v)); \
		LUA_SIMPLE_CONSTSTRING_HOOK_INT(call, POST, v) \
		RETURN_META_VALUE(MRES_IGNORED, mlStringResult); \
	}

#define SIMPLE_CONSTSTRING_HOOK_CONSTEDICT(call) \
	const char* call (const edict_t *e) \
	{ \
		FM_ENG_HANDLE(FM_##call, (Engine[FM_##call].at(i),(cell)ENTINDEX((edict_t*)e))); \
		LUA_SIMPLE_CONSTSTRING_HOOK_CONSTEDICT(call, , e) \
		RETURN_META_VALUE(mswi(lastFmRes), mlStringResult); \
	} \
	const char* call##_post (const edict_t *e) \
	{ \
		origStringRet = META_RESULT_ORIG_RET(const char *); \
		FM_ENG_HANDLE_POST(FM_##call, (EnginePost[FM_##call].at(i),(cell)ENTINDEX((edict_t*)e))); \
		LUA_SIMPLE_CONSTSTRING_HOOK_CONSTEDICT(call, POST, e) \
		RETURN_META_VALUE(MRES_IGNORED, mlStringResult); \
	}
#define SIMPLE_CONSTSTRING_HOOK_CONSTEDICT_CONSTSTRING(call) \
	const char* call (const edict_t *e, const char *c) \
	{ \
		FM_ENG_HANDLE(FM_##call, (Engine[FM_##call].at(i),(cell)ENTINDEX((edict_t*)e),c)); \
		LUA_SIMPLE_CONSTSTRING_HOOK_CONSTEDICT_CONSTSTRING(call, , e, c) \
		RETURN_META_VALUE(mswi(lastFmRes), mlStringResult); \
	} \
	const char* call##_post (const edict_t *e, const char *c) \
	{ \
		origStringRet = META_RESULT_ORIG_RET(const char *); \
		FM_ENG_HANDLE_POST(FM_##call, (EnginePost[FM_##call].at(i),(cell)ENTINDEX((edict_t*)e),c)); \
		LUA_SIMPLE_CONSTSTRING_HOOK_CONSTEDICT_CONSTSTRING(call, POST, e, c) \
		RETURN_META_VALUE(MRES_IGNORED, mlStringResult); \
	}
#define SIMPLE_VOID_HOOK_CONSTEDICT_INT_INT_INT_INT(call) \
	void call (const edict_t *e, int v, int vb, int vc, int vd) \
	{ \
		FM_ENG_HANDLE(FM_##call, (Engine[FM_##call].at(i),(cell)ENTINDEX((edict_t*)e),(cell)v,(cell)vb,(cell)vc,(cell)vd)); \
		LUA_SIMPLE_VOID_HOOK_CONSTEDICT_INT_INT_INT_INT(call, , e, v, vb, vc, vd) \
		RETURN_META(mswi(lastFmRes)); \
	} \
	void call##_post (const edict_t *e, int v, int vb, int vc, int vd) \
	{ \
		FM_ENG_HANDLE_POST(FM_##call, (EnginePost[FM_##call].at(i),(cell)ENTINDEX((edict_t*)e),(cell)v,(cell)vb,(cell)vc,(cell)vd)); \
		LUA_SIMPLE_VOID_HOOK_CONSTEDICT_INT_INT_INT_INT(call, POST, e, v, vb, vc, vd) \
		RETURN_META(MRES_IGNORED); \
	}
#define SIMPLE_VOID_HOOK_INT_STRING_STRING_STRING(call) \
	void call (int v,char *c, char *cb, char *cc) \
	{ \
		FM_ENG_HANDLE(FM_##call, (Engine[FM_##call].at(i),(cell)v,c,cb,cc)); \
		RETURN_META(mswi(lastFmRes)); \
	} \
	void call##_post (int v, char *c, char *cb, char *cc) \
	{ \
		FM_ENG_HANDLE_POST(FM_##call, (EnginePost[FM_##call].at(i),(cell)v,c,cb,cc)); \
		RETURN_META(MRES_IGNORED); \
	}

#define SIMPLE_VOID_HOOK_INT_STRING_CONSTSTRING_CONSTSTRING(call) \
        void call (int v,char *c, const char *cb, const char *cc) \
        { \
                FM_ENG_HANDLE(FM_##call, (Engine[FM_##call].at(i),(cell)v,c,cb,cc)); \
		LUA_SIMPLE_VOID_HOOK_INT_STRING_CONSTSTRING_CONSTSTRING_RW(call, , v, c, cb, cc) \
                RETURN_META(mswi(lastFmRes)); \
        } \
        void call##_post (int v, char *c, const char *cb, const char *cc) \
        { \
                FM_ENG_HANDLE_POST(FM_##call, (EnginePost[FM_##call].at(i),(cell)v,c,cb,cc)); \
		LUA_SIMPLE_VOID_HOOK_INT_STRING_CONSTSTRING_CONSTSTRING_RW(call, POST, v, c, cb, cc) \
                RETURN_META(MRES_IGNORED); \
        }


#define SIMPLE_VOID_HOOK_STRING_STRING_STRING(call) \
	void call (char *c, char *cb, char *cc) \
	{ \
		FM_ENG_HANDLE(FM_##call, (Engine[FM_##call].at(i),c,cb,cc)); \
		RETURN_META(mswi(lastFmRes)); \
	} \
	void call##_post (char *c, char *cb, char *cc) \
	{ \
		FM_ENG_HANDLE_POST(FM_##call, (EnginePost[FM_##call].at(i),c,cb,cc)); \
		RETURN_META(MRES_IGNORED); \
	}

#define SIMPLE_VOID_HOOK_STRING_CONSTSTRING_CONSTSTRING(call) \
        void call (char *c, const char *cb, const char *cc) \
        { \
                FM_ENG_HANDLE(FM_##call, (Engine[FM_##call].at(i),c,cb,cc)); \
		LUA_SIMPLE_VOID_HOOK_STRING_CONSTSTRING_CONSTSTRING_RW(call, , c, cb, cc) \
                RETURN_META(mswi(lastFmRes)); \
        } \
        void call##_post (char *c, const char *cb, const char *cc) \
        { \
                FM_ENG_HANDLE_POST(FM_##call, (EnginePost[FM_##call].at(i),c,cb,cc)); \
		LUA_SIMPLE_VOID_HOOK_STRING_CONSTSTRING_CONSTSTRING_RW(call, POST, c, cb, cc) \
                RETURN_META(MRES_IGNORED); \
        }

#define SIMPLE_STRING_HOOK_STRING_STRING(call) \
	char* call (char *c, char *cb) \
	{ \
		FM_ENG_HANDLE(FM_##call, (Engine[FM_##call].at(i),c,cb)); \
		RETURN_META_VALUE(mswi(lastFmRes), (char*)mlStringResult); \
	} \
	char* call##_post (char *c, char *cb) \
	{ \
		origStringRet = META_RESULT_ORIG_RET(char *); \
		FM_ENG_HANDLE_POST(FM_##call, (EnginePost[FM_##call].at(i),c,cb)); \
		RETURN_META_VALUE(MRES_IGNORED, (char*)mlStringResult); \
	}
#define SIMPLE_STRING_HOOK_STRING_CONSTSTRING(call) \
        char* call (char *c, const char *cb) \
        { \
                FM_ENG_HANDLE(FM_##call, (Engine[FM_##call].at(i),c,cb)); \
		LUA_SIMPLE_STRING_HOOK_STRING_CONSTSTRING_RW(call, , c, cb) \
                RETURN_META_VALUE(mswi(lastFmRes), (char*)mlStringResult); \
        } \
        char* call##_post (char *c, const char *cb) \
        { \
                origStringRet = META_RESULT_ORIG_RET(char *); \
                FM_ENG_HANDLE_POST(FM_##call, (EnginePost[FM_##call].at(i),c,cb)); \
		LUA_SIMPLE_STRING_HOOK_STRING_CONSTSTRING_RW(call, POST, c, cb) \
                RETURN_META_VALUE(MRES_IGNORED, (char*)mlStringResult); \
        }
#define SIMPLE_CONSTSTRING_HOOK_EDICT(call) \
	const char* call (edict_t *e) \
	{ \
		FM_ENG_HANDLE(FM_##call, (Engine[FM_##call].at(i),(cell)ENTINDEX(e))); \
		LUA_SIMPLE_CONSTSTRING_HOOK_EDICT(call, , e) \
		RETURN_META_VALUE(mswi(lastFmRes), mlStringResult); \
	} \
	const char* call##_post (edict_t *e) \
	{ \
		origStringRet = META_RESULT_ORIG_RET(const char *); \
		FM_ENG_HANDLE_POST(FM_##call, (EnginePost[FM_##call].at(i),(cell)ENTINDEX(e))); \
		LUA_SIMPLE_CONSTSTRING_HOOK_EDICT(call, POST, e) \
		RETURN_META_VALUE(MRES_IGNORED, mlStringResult); \
	}
#define SIMPLE_VOID_HOOK_CONSTEDICT_CONSTSTRING_CONSTSTRING(call) \
	void call (const edict_t *e, const char *c, const char *cb) \
	{ \
		FM_ENG_HANDLE(FM_##call, (Engine[FM_##call].at(i),(cell)ENTINDEX((edict_t*)e),c,cb)); \
		LUA_SIMPLE_VOID_HOOK_CONSTEDICT_CONSTSTRING_CONSTSTRING(call, , e, c, cb) \
		RETURN_META(mswi(lastFmRes)); \
	} \
	void call##_post (const edict_t *e, const char *c, const char *cb) \
	{ \
		FM_ENG_HANDLE_POST(FM_##call, (EnginePost[FM_##call].at(i),(cell)ENTINDEX((edict_t*)e),c,cb)); \
		LUA_SIMPLE_VOID_HOOK_CONSTEDICT_CONSTSTRING_CONSTSTRING(call, POST, e, c, cb) \
		RETURN_META(MRES_IGNORED); \
	}

		

#define SIMPLE_INT_HOOK_STRING(call) \
	int call (char *s) \
	{ \
		FM_ENG_HANDLE(FM_##call, (Engine[FM_##call].at(i), s)); \
		RETURN_META_VALUE(mswi(lastFmRes), (int)mlCellResult); \
	} \
	int call##_post (char *s) \
	{ \
		origCellRet = META_RESULT_ORIG_RET(int); \
		FM_ENG_HANDLE_POST(FM_##call, (EnginePost[FM_##call].at(i), s)); \
		RETURN_META_VALUE(MRES_IGNORED, (int)mlCellResult); \
	} 

#define SIMPLE_INT_HOOK_CONSTSTRING(call) \
	int call (const char *s) \
	{ \
		FM_ENG_HANDLE(FM_##call, (Engine[FM_##call].at(i), s)); \
		LUA_SIMPLE_INT_HOOK_CONSTSTRING(call, , s) \
		RETURN_META_VALUE(mswi(lastFmRes), (int)mlCellResult); \
	} \
	int call##_post (const char *s) \
	{ \
		origCellRet = META_RESULT_ORIG_RET(int); \
		FM_ENG_HANDLE_POST(FM_##call, (EnginePost[FM_##call].at(i), s)); \
		LUA_SIMPLE_INT_HOOK_CONSTSTRING(call, POST, s) \
		RETURN_META_VALUE(MRES_IGNORED, (int)mlCellResult); \
	}

#define SIMPLE_EDICT_HOOK_CONSTSTRING(call) \
	edict_t* call (const char *s) \
	{ \
		FM_ENG_HANDLE(FM_##call, (Engine[FM_##call].at(i), s)); \
		RETURN_META_VALUE(mswi(lastFmRes), TypeConversion.id_to_edict((int)mlCellResult)); \
	} \
	edict_t* call##_post (const char *s) \
	{ \
		origCellRet = ENTINDEX(META_RESULT_ORIG_RET(edict_t *)); \
		FM_ENG_HANDLE_POST(FM_##call, (EnginePost[FM_##call].at(i), s)); \
		RETURN_META_VALUE(MRES_IGNORED, TypeConversion.id_to_edict((int)mlCellResult)); \
	}
#define SIMPLE_CHAR_HOOK_STRING(call) \
	char call (char *s) \
	{ \
		FM_ENG_HANDLE(FM_##call, (Engine[FM_##call].at(i), s)); \
		RETURN_META_VALUE(mswi(lastFmRes), (char)mlCellResult); \
	} \
	char call##_post (char *s) \
	{ \
		origCellRet = META_RESULT_ORIG_RET(char); \
		FM_ENG_HANDLE_POST(FM_##call, (EnginePost[FM_##call].at(i), s)); \
		RETURN_META_VALUE(MRES_IGNORED, (char)mlCellResult); \
	}
#define SIMPLE_CHAR_HOOK_CONSTSTRING(call) \
        char call (const char *s) \
        { \
                FM_ENG_HANDLE(FM_##call, (Engine[FM_##call].at(i), s)); \
		LUA_SIMPLE_CHAR_HOOK_CONSTSTRING_RETURN(call, , s) \
                RETURN_META_VALUE(mswi(lastFmRes), (char)mlCellResult); \
        } \
        char call##_post (const char *s) \
        { \
                origCellRet = META_RESULT_ORIG_RET(char); \
                FM_ENG_HANDLE_POST(FM_##call, (EnginePost[FM_##call].at(i), s)); \
		LUA_SIMPLE_CHAR_HOOK_CONSTSTRING_RETURN(call, POST, s) \
                RETURN_META_VALUE(MRES_IGNORED, (char)mlCellResult); \
        }

#define SIMPLE_VOID_HOOK_CONSTSTRING(call) \
	void call (const char *s) \
	{ \
		FM_ENG_HANDLE(FM_##call, (Engine[FM_##call].at(i), s)); \
		LUA_SIMPLE_VOID_HOOK_CONSTSTRING(call, , s) \
		RETURN_META(mswi(lastFmRes)); \
	} \
	void call##_post (const char *s) \
	{ \
		FM_ENG_HANDLE_POST(FM_##call, (EnginePost[FM_##call].at(i), s)); \
		LUA_SIMPLE_VOID_HOOK_CONSTSTRING(call, POST, s) \
		RETURN_META(MRES_IGNORED); \
	}

#define SIMPLE_VOID_HOOK_STRING_STRING(call) \
	void call (char *s, char *sb) \
	{ \
		FM_ENG_HANDLE(FM_##call, (Engine[FM_##call].at(i), s, sb)); \
		RETURN_META(mswi(lastFmRes)); \
	} \
	void call##_post (char *s, char *sb) \
	{ \
		FM_ENG_HANDLE_POST(FM_##call, (EnginePost[FM_##call].at(i), s, sb)); \
		RETURN_META(MRES_IGNORED); \
	}

#define SIMPLE_VOID_HOOK_INT_CONSTSTRING(call) \
	void call (int v, const char *s) \
	{ \
		FM_ENG_HANDLE(FM_##call, (Engine[FM_##call].at(i), (cell)v, s)); \
		RETURN_META(mswi(lastFmRes)); \
	} \
	void call##_post (int v,const char *s) \
	{ \
		FM_ENG_HANDLE_POST(FM_##call, (EnginePost[FM_##call].at(i), (cell)v, s)); \
		RETURN_META(MRES_IGNORED); \
	}

#define SIMPLE_VOID_HOOK_CONSTSTRING_FLOAT(call) \
	void call (const char *s, float f) \
	{ \
		FM_ENG_HANDLE(FM_##call, (Engine[FM_##call].at(i), s, f)); \
		LUA_SIMPLE_VOID_HOOK_CONSTSTRING_FLOAT(call, , s, f) \
		RETURN_META(mswi(lastFmRes)); \
	} \
	void call##_post (const char *s, float f) \
	{ \
		FM_ENG_HANDLE_POST(FM_##call, (EnginePost[FM_##call].at(i), s, f)); \
		LUA_SIMPLE_VOID_HOOK_CONSTSTRING_FLOAT(call, POST, s, f) \
		RETURN_META(MRES_IGNORED); \
	}

#define SIMPLE_INT_HOOK_CONSTSTRING_INT(call) \
	int call (const char *s, int v) \
	{ \
		FM_ENG_HANDLE(FM_##call, (Engine[FM_##call].at(i), s, (cell)v)); \
		RETURN_META_VALUE(mswi(lastFmRes),(int)mlCellResult); \
	} \
	int call##_post (const char *s, int v) \
	{ \
		origCellRet = META_RESULT_ORIG_RET(int); \
		FM_ENG_HANDLE_POST(FM_##call, (EnginePost[FM_##call].at(i), s, (cell)v)); \
		RETURN_META_VALUE(MRES_IGNORED,(int)mlCellResult); \
	}

#define SIMPLE_VOID_HOOK_CONSTSTRING_CONSTSTRING(call) \
	void call (const char *s,const char *sb) \
	{ \
		FM_ENG_HANDLE(FM_##call, (Engine[FM_##call].at(i), s, sb)); \
		LUA_SIMPLE_VOID_HOOK_CONSTSTRING_CONSTSTRING(call, , s, sb) \
		RETURN_META(mswi(lastFmRes)); \
	} \
	void call##_post (const char *s, const char *sb) \
	{ \
		FM_ENG_HANDLE_POST(FM_##call, (EnginePost[FM_##call].at(i), s, sb)); \
		LUA_SIMPLE_VOID_HOOK_CONSTSTRING_CONSTSTRING(call, POST, s, sb) \
		RETURN_META(MRES_IGNORED); \
	}


#define SIMPLE_USHORT_HOOK_INT_CONSTSTRING(call) \
	unsigned short call (int v, const char *s) \
	{ \
		FM_ENG_HANDLE(FM_##call, (Engine[FM_##call].at(i), (cell)v, s)); \
		LUA_SIMPLE_USHORT_HOOK_INT_CONSTSTRING(call, , v, s) \
		RETURN_META_VALUE(mswi(lastFmRes),(unsigned short)mlCellResult); \
	} \
	unsigned short call##_post (int v,const char *s) \
	{ \
		origCellRet = META_RESULT_ORIG_RET(unsigned short); \
		FM_ENG_HANDLE_POST(FM_##call, (EnginePost[FM_##call].at(i),(cell)v, s)); \
		LUA_SIMPLE_USHORT_HOOK_INT_CONSTSTRING(call, POST, v, s) \
		RETURN_META_VALUE(MRES_IGNORED,(unsigned short)mlCellResult); \
	}

#define SIMPLE_VOID_HOOK_INT_STRING(call) \
	void call (int v, char *s) \
	{ \
		FM_ENG_HANDLE(FM_##call, (Engine[FM_##call].at(i), (cell)v, s)); \
		LUA_SIMPLE_VOID_HOOK_INT_CONSTSTRING(call, , v, s) \
		RETURN_META(mswi(lastFmRes)); \
	} \
	void call##_post (int v,char *s) \
	{ \
		FM_ENG_HANDLE_POST(FM_##call, (EnginePost[FM_##call].at(i), (cell)v, s)); \
		LUA_SIMPLE_VOID_HOOK_INT_CONSTSTRING(call, POST, v, s) \
		RETURN_META(MRES_IGNORED); \
	}

#define SIMPLE_VOID_HOOK_INT(call) \
	void call (int v) \
	{ \
		FM_ENG_HANDLE(FM_##call, (Engine[FM_##call].at(i), (cell)v)); \
		LUA_SIMPLE_VOID_HOOK_INT(call, , v) \
		RETURN_META(mswi(lastFmRes)); \
	} \
	void call##_post (int v) \
	{ \
		FM_ENG_HANDLE_POST(FM_##call, (EnginePost[FM_##call].at(i), (cell)v)); \
		LUA_SIMPLE_VOID_HOOK_INT(call, POST, v) \
		RETURN_META(MRES_IGNORED); \
	}

#define SIMPLE_VOID_HOOK_FLOAT(call) \
	void call (float v) \
	{ \
		FM_ENG_HANDLE(FM_##call, (Engine[FM_##call].at(i), v)); \
		LUA_SIMPLE_VOID_HOOK_FLOAT(call, , v) \
		RETURN_META(mswi(lastFmRes)); \
	} \
	void call##_post (float v) \
	{ \
		FM_ENG_HANDLE_POST(FM_##call, (EnginePost[FM_##call].at(i), v)); \
		LUA_SIMPLE_VOID_HOOK_FLOAT(call, POST, v) \
		RETURN_META(MRES_IGNORED); \
	}

#define SIMPLE_VOID_HOOK_CONSTEDICT(call) \
	void call (const edict_t *ent) \
	{ \
	FM_ENG_HANDLE(FM_##call, (Engine[FM_##call].at(i),  (cell)ENTINDEX(ent))); \
	LUA_SIMPLE_VOID_HOOK_CONSTEDICT(call, , ent) \
	RETURN_META(mswi(lastFmRes)); \
	} \
	void call##_post (const edict_t *ent) \
	{ \
	FM_ENG_HANDLE_POST(FM_##call, (EnginePost[FM_##call].at(i),  (cell)ENTINDEX(ent))); \
	LUA_SIMPLE_VOID_HOOK_CONSTEDICT(call, POST, ent) \
	RETURN_META(MRES_IGNORED); \
	}

#define SIMPLE_VOID_HOOK_CONSTEDICT_FLOAT(call) \
	void call (const edict_t *ent, float blah) \
	{ \
		FM_ENG_HANDLE(FM_##call, (Engine[FM_##call].at(i),  (cell)ENTINDEX((edict_t*)ent), blah)); \
		LUA_SIMPLE_VOID_HOOK_CONSTEDICT_FLOAT(call, , ent, blah) \
		RETURN_META(mswi(lastFmRes)); \
	} \
	void call##_post (const edict_t *ent, float blah) \
	{ \
		FM_ENG_HANDLE_POST(FM_##call, (EnginePost[FM_##call].at(i),  (cell)ENTINDEX((edict_t*)ent), blah)); \
		LUA_SIMPLE_VOID_HOOK_CONSTEDICT_FLOAT(call, POST, ent, blah) \
		RETURN_META(MRES_IGNORED); \
	} 
#define SIMPLE_VOID_HOOK_CONSTEDICT_FLOAT_FLOAT(call) \
	void call (const edict_t *ent, float blah, float blahb) \
	{ \
		FM_ENG_HANDLE(FM_##call, (Engine[FM_##call].at(i),  (cell)ENTINDEX((edict_t*)ent), blah, blahb)); \
		LUA_SIMPLE_VOID_HOOK_CONSTEDICT_FLOAT_FLOAT(call, , ent, blah, blahb) \
		RETURN_META(mswi(lastFmRes)); \
	} \
	void call##_post (const edict_t *ent, float blah, float blahb) \
	{ \
		FM_ENG_HANDLE_POST(FM_##call, (EnginePost[FM_##call].at(i),  (cell)ENTINDEX((edict_t*)ent), blah, blahb)); \
		LUA_SIMPLE_VOID_HOOK_CONSTEDICT_FLOAT_FLOAT(call, POST, ent, blah, blahb) \
		RETURN_META(MRES_IGNORED); \
	} 

#define LUA_HOOK_LOOKUP(call, post) \
	if (g_L) { \
		lua_getglobal(g_L, "fakemeta_"#call#post); \
		if (lua_isfunction(g_L, -1)) {

#define LUA_HOOK_PUSH_INT(v) \
			lua_pushinteger(g_L, (lua_Integer)(v));

#define LUA_HOOK_PUSH_FLOAT(v) \
			lua_pushnumber(g_L, (lua_Number)(v));

#define LUA_HOOK_PUSH_EDICT(ent) \
			lua_pushinteger(g_L, (lua_Integer)ENTINDEX((edict_t *)(ent)));

#define LUA_HOOK_PUSH_STRING(str) \
			lua_pushstring(g_L, (str));

#define LUA_HOOK_PUSH_VECTOR(vec) \
			lua_pushnumber(g_L, (lua_Number)(vec[0])); \
			lua_pushnumber(g_L, (lua_Number)(vec[1])); \
			lua_pushnumber(g_L, (lua_Number)(vec[2]));

#define LUA_HOOK_CALL(nargs) \
			if (lua_pcall(g_L, (nargs), 1, 0) == 0) { \
				if (lua_isnumber(g_L, -1) || lua_isboolean(g_L, -1)) { \
					fmres = (int)lua_tointeger(g_L, -1); \
					lua_pop(g_L, 1); \
					if (fmres >= lastFmRes) \
					{ \
						if (retType == FMV_STRING) \
							mlStringResult = mStringResult; \
						else if (retType == FMV_CELL) \
							mlCellResult = mCellResult; \
						else if (retType == FMV_FLOAT) \
							mlFloatResult = mFloatResult; \
						lastFmRes = fmres; \
					} \
				} else { \
					lua_pop(g_L, 1); \
				} \
			} else { \
				lua_pop(g_L, 1); \
			} \
		} else { \
			lua_pop(g_L, 1); \
		} \
	}

#define LUA_HOOK_APPLY_RESULT(idx) \
			if (lua_isnumber(g_L, (idx)) || lua_isboolean(g_L, (idx))) { \
				fmres = (int)lua_tointeger(g_L, (idx)); \
				if (fmres >= lastFmRes) \
				{ \
					if (retType == FMV_STRING) \
						mlStringResult = mStringResult; \
					else if (retType == FMV_CELL) \
						mlCellResult = mCellResult; \
					else if (retType == FMV_FLOAT) \
						mlFloatResult = mFloatResult; \
					lastFmRes = fmres; \
				} \
			}

#define LUA_HOOK_COPYBACK_STRING(idx, buf, maxlen) \
			if (lua_isstring(g_L, (idx))) { \
				ke::SafeStrcpy((buf), (maxlen), lua_tostring(g_L, (idx))); \
			}

#define LUA_SIMPLE_CHAR_HOOK_CONSTSTRING_RETURN(call, post, s) \
	LUA_HOOK_LOOKUP(call, post) \
	LUA_HOOK_PUSH_STRING(s) \
			if (lua_pcall(g_L, 1, 2, 0) == 0) { \
				LUA_HOOK_APPLY_RESULT(-2) \
				if (lua_isstring(g_L, -1)) { \
					mlCellResult = (cell)lua_tostring(g_L, -1)[0]; \
				} else if (lua_isnumber(g_L, -1) || lua_isboolean(g_L, -1)) { \
					mlCellResult = (cell)lua_tointeger(g_L, -1); \
				} \
				lua_pop(g_L, 2); \
			} else { \
				lua_pop(g_L, 1); \
			} \
		} else { \
			lua_pop(g_L, 1); \
		} \
	}

#define LUA_SIMPLE_STRING_HOOK_STRING_CONSTSTRING_RW(call, post, s, sb) \
	LUA_HOOK_LOOKUP(call, post) \
	LUA_HOOK_PUSH_STRING(s) \
	LUA_HOOK_PUSH_STRING(sb) \
			if (lua_pcall(g_L, 2, 2, 0) == 0) { \
				LUA_HOOK_APPLY_RESULT(-2) \
				if (lua_isstring(g_L, -1)) { \
					mlStringResult = lua_tostring(g_L, -1); \
				} \
				lua_pop(g_L, 2); \
			} else { \
				lua_pop(g_L, 1); \
			} \
		} else { \
			lua_pop(g_L, 1); \
		} \
	}

#define LUA_SIMPLE_BOOL_HOOK_EDICT_CONSTSTRING_CONSTSTRING_STRING128_RW(call, post, ent, s, sb, outbuf) \
	LUA_HOOK_LOOKUP(call, post) \
	LUA_HOOK_PUSH_EDICT(ent) \
	LUA_HOOK_PUSH_STRING(s) \
	LUA_HOOK_PUSH_STRING(sb) \
	LUA_HOOK_PUSH_STRING(outbuf) \
			if (lua_pcall(g_L, 4, 2, 0) == 0) { \
				LUA_HOOK_APPLY_RESULT(-2) \
				LUA_HOOK_COPYBACK_STRING(-1, outbuf, 128) \
				lua_pop(g_L, 2); \
			} else { \
				lua_pop(g_L, 1); \
			} \
		} else { \
			lua_pop(g_L, 1); \
		} \
	}

#define LUA_SIMPLE_VOID_HOOK_STRING_CONSTSTRING_CONSTSTRING_RW(call, post, buf, s, sb) \
	LUA_HOOK_LOOKUP(call, post) \
	LUA_HOOK_PUSH_STRING(buf) \
	LUA_HOOK_PUSH_STRING(s) \
	LUA_HOOK_PUSH_STRING(sb) \
			if (lua_pcall(g_L, 3, 2, 0) == 0) { \
				LUA_HOOK_APPLY_RESULT(-2) \
				if (lua_isstring(g_L, -1)) { \
					ke::SafeStrcpy((buf), 1024, lua_tostring(g_L, -1)); \
				} \
				lua_pop(g_L, 2); \
			} else { \
				lua_pop(g_L, 1); \
			} \
		} else { \
			lua_pop(g_L, 1); \
		} \
	}

#define LUA_SIMPLE_VOID_HOOK_INT_STRING_CONSTSTRING_CONSTSTRING_RW(call, post, v, buf, s, sb) \
	LUA_HOOK_LOOKUP(call, post) \
	LUA_HOOK_PUSH_INT(v) \
	LUA_HOOK_PUSH_STRING(buf) \
	LUA_HOOK_PUSH_STRING(s) \
	LUA_HOOK_PUSH_STRING(sb) \
			if (lua_pcall(g_L, 4, 2, 0) == 0) { \
				LUA_HOOK_APPLY_RESULT(-2) \
				if (lua_isstring(g_L, -1)) { \
					ke::SafeStrcpy((buf), 1024, lua_tostring(g_L, -1)); \
				} \
				lua_pop(g_L, 2); \
			} else { \
				lua_pop(g_L, 1); \
			} \
		} else { \
			lua_pop(g_L, 1); \
		} \
	}

#define LUA_SIMPLE_VOID_HOOK_VOID(call, post) \
	LUA_HOOK_LOOKUP(call, post) \
	LUA_HOOK_CALL(0)

#define LUA_SIMPLE_VOID_HOOK_INT(call, post, v) \
	LUA_HOOK_LOOKUP(call, post) \
	LUA_HOOK_PUSH_INT(v) \
	LUA_HOOK_CALL(1)

#define LUA_SIMPLE_VOID_HOOK_FLOAT(call, post, v) \
	LUA_HOOK_LOOKUP(call, post) \
	LUA_HOOK_PUSH_FLOAT(v) \
	LUA_HOOK_CALL(1)

#define LUA_SIMPLE_VOID_HOOK_EDICT(call, post, ent) \
	LUA_HOOK_LOOKUP(call, post) \
	LUA_HOOK_PUSH_EDICT(ent) \
	LUA_HOOK_CALL(1)

#define LUA_SIMPLE_VOID_HOOK_EDICT_EDICT(call, post, ent, entb) \
	LUA_HOOK_LOOKUP(call, post) \
	LUA_HOOK_PUSH_EDICT(ent) \
	LUA_HOOK_PUSH_EDICT(entb) \
	LUA_HOOK_CALL(2)

#define LUA_SIMPLE_VOID_HOOK_CONSTSTRING(call, post, s) \
	LUA_HOOK_LOOKUP(call, post) \
	LUA_HOOK_PUSH_STRING(s) \
	LUA_HOOK_CALL(1)

#define LUA_SIMPLE_VOID_HOOK_INT_CONSTSTRING(call, post, v, s) \
	LUA_HOOK_LOOKUP(call, post) \
	LUA_HOOK_PUSH_INT(v) \
	LUA_HOOK_PUSH_STRING(s) \
	LUA_HOOK_CALL(2)

#define LUA_SIMPLE_VOID_HOOK_CONSTSTRING_CONSTSTRING(call, post, s, sb) \
	LUA_HOOK_LOOKUP(call, post) \
	LUA_HOOK_PUSH_STRING(s) \
	LUA_HOOK_PUSH_STRING(sb) \
	LUA_HOOK_CALL(2)

#define LUA_SIMPLE_VOID_HOOK_CONSTSTRING_FLOAT(call, post, s, f) \
	LUA_HOOK_LOOKUP(call, post) \
	LUA_HOOK_PUSH_STRING(s) \
	LUA_HOOK_PUSH_FLOAT(f) \
	LUA_HOOK_CALL(2)

#define LUA_SIMPLE_VOID_HOOK_INT_INT(call, post, v, vb) \
	LUA_HOOK_LOOKUP(call, post) \
	LUA_HOOK_PUSH_INT(v) \
	LUA_HOOK_PUSH_INT(vb) \
	LUA_HOOK_CALL(2)

#define LUA_SIMPLE_INT_HOOK_VOID(call, post) \
	LUA_SIMPLE_VOID_HOOK_VOID(call, post)

#define LUA_SIMPLE_INT_HOOK_INT(call, post, v) \
	LUA_SIMPLE_VOID_HOOK_INT(call, post, v)

#define LUA_SIMPLE_INT_HOOK_EDICT(call, post, ent) \
	LUA_SIMPLE_VOID_HOOK_EDICT(call, post, ent)

#define LUA_SIMPLE_INT_HOOK_CONSTEDICT(call, post, ent) \
	LUA_SIMPLE_VOID_HOOK_EDICT(call, post, ent)

#define LUA_SIMPLE_FLOAT_HOOK_VOID(call, post) \
	LUA_SIMPLE_VOID_HOOK_VOID(call, post)

#define LUA_SIMPLE_FLOAT_HOOK_CONSTSTRING(call, post, s) \
	LUA_SIMPLE_VOID_HOOK_CONSTSTRING(call, post, s)

#define LUA_SIMPLE_CONSTSTRING_HOOK_VOID(call, post) \
	LUA_SIMPLE_VOID_HOOK_VOID(call, post)

#define LUA_SIMPLE_CONSTSTRING_HOOK_CONSTSTRING(call, post, s) \
	LUA_SIMPLE_VOID_HOOK_CONSTSTRING(call, post, s)

#define LUA_SIMPLE_CONSTSTRING_HOOK_VOID(call, post) \
	LUA_SIMPLE_VOID_HOOK_VOID(call, post)

#define LUA_SIMPLE_CONSTSTRING_HOOK_INT(call, post, v) \
	LUA_SIMPLE_VOID_HOOK_INT(call, post, v)

#define LUA_SIMPLE_CONSTSTRING_HOOK_CONSTEDICT(call, post, ent) \
	LUA_SIMPLE_VOID_HOOK_EDICT(call, post, ent)

#define LUA_SIMPLE_CONSTSTRING_HOOK_CONSTEDICT_CONSTSTRING(call, post, ent, s) \
	LUA_HOOK_LOOKUP(call, post) \
	LUA_HOOK_PUSH_EDICT(ent) \
	LUA_HOOK_PUSH_STRING(s) \
	LUA_HOOK_CALL(2)

#define LUA_SIMPLE_STRING_HOOK_STRING_STRING(call, post, s, sb) \
	LUA_HOOK_LOOKUP(call, post) \
	LUA_HOOK_PUSH_STRING(s) \
	LUA_HOOK_PUSH_STRING(sb) \
	LUA_HOOK_CALL(2)

#define LUA_SIMPLE_STRING_HOOK_STRING_CONSTSTRING(call, post, s, sb) \
	LUA_HOOK_LOOKUP(call, post) \
	LUA_HOOK_PUSH_STRING(s) \
	LUA_HOOK_PUSH_STRING(sb) \
	LUA_HOOK_CALL(2)

#define LUA_SIMPLE_CHAR_HOOK_STRING(call, post, s) \
	LUA_SIMPLE_VOID_HOOK_CONSTSTRING(call, post, s)

#define LUA_SIMPLE_CHAR_HOOK_CONSTSTRING(call, post, s) \
	LUA_SIMPLE_VOID_HOOK_CONSTSTRING(call, post, s)

#define LUA_SIMPLE_EDICT_HOOK_VOID(call, post) \
	LUA_SIMPLE_VOID_HOOK_VOID(call, post)

#define LUA_SIMPLE_EDICT_HOOK_INT(call, post, v) \
	LUA_SIMPLE_VOID_HOOK_INT(call, post, v)

#define LUA_SIMPLE_EDICT_HOOK_EDICT(call, post, ent) \
	LUA_SIMPLE_VOID_HOOK_EDICT(call, post, ent)

#define LUA_SIMPLE_EDICT_HOOK_CONSTSTRING(call, post, s) \
	LUA_SIMPLE_VOID_HOOK_CONSTSTRING(call, post, s)

#define LUA_SIMPLE_CONSTSTRING_HOOK_EDICT(call, post, ent) \
	LUA_SIMPLE_VOID_HOOK_EDICT(call, post, ent)

#define LUA_SIMPLE_VOID_HOOK_CONSTEDICT(call, post, ent) \
	LUA_SIMPLE_VOID_HOOK_EDICT(call, post, ent)

#define LUA_SIMPLE_VOID_HOOK_CONSTEDICT_FLOAT(call, post, ent, f) \
	LUA_HOOK_LOOKUP(call, post) \
	LUA_HOOK_PUSH_EDICT(ent) \
	LUA_HOOK_PUSH_FLOAT(f) \
	LUA_HOOK_CALL(2)

#define LUA_SIMPLE_VOID_HOOK_CONSTEDICT_FLOAT_FLOAT(call, post, ent, f, fb) \
	LUA_HOOK_LOOKUP(call, post) \
	LUA_HOOK_PUSH_EDICT(ent) \
	LUA_HOOK_PUSH_FLOAT(f) \
	LUA_HOOK_PUSH_FLOAT(fb) \
	LUA_HOOK_CALL(3)

#define LUA_SIMPLE_VOID_HOOK_CONSTEDICT_CONSTEDICT(call, post, ent, entb) \
	LUA_SIMPLE_VOID_HOOK_EDICT_EDICT(call, post, ent, entb)

#define LUA_SIMPLE_VOID_HOOK_CONSTEDICT_INT_INT_INT_INT(call, post, ent, v, vb, vc, vd) \
	LUA_HOOK_LOOKUP(call, post) \
	LUA_HOOK_PUSH_EDICT(ent) \
	LUA_HOOK_PUSH_INT(v) \
	LUA_HOOK_PUSH_INT(vb) \
	LUA_HOOK_PUSH_INT(vc) \
	LUA_HOOK_PUSH_INT(vd) \
	LUA_HOOK_CALL(5)

#define LUA_SIMPLE_VOID_HOOK_CONSTEDICT_CONSTSTRING_CONSTSTRING(call, post, ent, s, sb) \
	LUA_HOOK_LOOKUP(call, post) \
	LUA_HOOK_PUSH_EDICT(ent) \
	LUA_HOOK_PUSH_STRING(s) \
	LUA_HOOK_PUSH_STRING(sb) \
	LUA_HOOK_CALL(3)

#define LUA_SIMPLE_UINT_HOOK_EDICT(call, post, ent) \
	LUA_SIMPLE_VOID_HOOK_EDICT(call, post, ent)

#define LUA_SIMPLE_BOOL_HOOK_EDICT_CONSTSTRING_CONSTSTRING_STRING128(call, post, ent, s, sb, outbuf) \
	LUA_HOOK_LOOKUP(call, post) \
	LUA_HOOK_PUSH_EDICT(ent) \
	LUA_HOOK_PUSH_STRING(s) \
	LUA_HOOK_PUSH_STRING(sb) \
	LUA_HOOK_PUSH_STRING(outbuf) \
	LUA_HOOK_CALL(4)

#define LUA_SIMPLE_VOID_HOOK_EDICT_INT_CONSTSTRING_FLOAT_FLOAT_INT_INT(call, post, ent, v, s, f, fb, vb, vc) \
	LUA_HOOK_LOOKUP(call, post) \
	LUA_HOOK_PUSH_EDICT(ent) \
	LUA_HOOK_PUSH_INT(v) \
	LUA_HOOK_PUSH_STRING(s) \
	LUA_HOOK_PUSH_FLOAT(f) \
	LUA_HOOK_PUSH_FLOAT(fb) \
	LUA_HOOK_PUSH_INT(vb) \
	LUA_HOOK_PUSH_INT(vc) \
	LUA_HOOK_CALL(7)

#define LUA_SIMPLE_VOID_HOOK_EDICT_VECT_CONSTSTRING_FLOAT_FLOAT_INT_INT(call, post, ent, vec, s, f, fb, vb, vc) \
	LUA_HOOK_LOOKUP(call, post) \
	LUA_HOOK_PUSH_EDICT(ent) \
	LUA_HOOK_PUSH_VECTOR(vec) \
	LUA_HOOK_PUSH_STRING(s) \
	LUA_HOOK_PUSH_FLOAT(f) \
	LUA_HOOK_PUSH_FLOAT(fb) \
	LUA_HOOK_PUSH_INT(vb) \
	LUA_HOOK_PUSH_INT(vc) \
	LUA_HOOK_CALL(10)

#define LUA_SIMPLE_EDICT_HOOK_EDICT_CONSTVECT_FLOAT(call, post, ent, vec, fla) \
	LUA_HOOK_LOOKUP(call, post) \
	LUA_HOOK_PUSH_EDICT(ent) \
	LUA_HOOK_PUSH_VECTOR(vec) \
	LUA_HOOK_PUSH_FLOAT(fla) \
	LUA_HOOK_CALL(5)

#define LUA_HOOK_PUSH_EDICT_OR_NEG1(ent) \
			lua_pushinteger(g_L, (lua_Integer)((ent) ? ENTINDEX((edict_t *)(ent)) : -1));

#define LUA_SIMPLE_HOOK_PLAYBACK_EVENT(call, post, v, e, eb, f, vec, vecb, fb, fc, vb, vc, vd, ve) \
	LUA_HOOK_LOOKUP(call, post) \
	LUA_HOOK_PUSH_INT(v) \
	LUA_HOOK_PUSH_EDICT_OR_NEG1(e) \
	LUA_HOOK_PUSH_INT(eb) \
	LUA_HOOK_PUSH_FLOAT(f) \
	LUA_HOOK_PUSH_VECTOR(vec) \
	LUA_HOOK_PUSH_VECTOR(vecb) \
	LUA_HOOK_PUSH_FLOAT(fb) \
	LUA_HOOK_PUSH_FLOAT(fc) \
	LUA_HOOK_PUSH_INT(vb) \
	LUA_HOOK_PUSH_INT(vc) \
	LUA_HOOK_PUSH_INT(vd) \
	LUA_HOOK_PUSH_INT(ve) \
	LUA_HOOK_CALL(12)

#define LUA_SIMPLE_INT_HOOK_STRING(call, post, s) \
	LUA_SIMPLE_VOID_HOOK_CONSTSTRING(call, post, s)

#define LUA_SIMPLE_INT_HOOK_CONSTSTRING(call, post, s) \
	LUA_SIMPLE_VOID_HOOK_CONSTSTRING(call, post, s)

#define LUA_SIMPLE_INT_HOOK_CONSTSTRING_INT(call, post, s, v) \
	LUA_HOOK_LOOKUP(call, post) \
	LUA_HOOK_PUSH_STRING(s) \
	LUA_HOOK_PUSH_INT(v) \
	LUA_HOOK_CALL(2)

#define LUA_SIMPLE_USHORT_HOOK_INT_CONSTSTRING(call, post, v, s) \
	LUA_SIMPLE_VOID_HOOK_INT_CONSTSTRING(call, post, v, s)

#define LUA_SIMPLE_FLOAT_HOOK_CONSTVECT(call, post, vec) \
	LUA_HOOK_LOOKUP(call, post) \
	LUA_HOOK_PUSH_VECTOR(vec) \
	LUA_HOOK_CALL(3)

#define LUA_SIMPLE_INT_HOOK_CONSTVECT(call, post, vec) \
	LUA_HOOK_LOOKUP(call, post) \
	LUA_HOOK_PUSH_VECTOR(vec) \
	LUA_HOOK_CALL(3)

#define LUA_SIMPLE_VOID_HOOK_CONSTVECT(call, post, vec) \
	LUA_HOOK_LOOKUP(call, post) \
	LUA_HOOK_PUSH_VECTOR(vec) \
	LUA_HOOK_CALL(3)

#define LUA_SIMPLE_VOID_HOOK_EDICT_CONSTVECT(call, post, ent, vec) \
	LUA_HOOK_LOOKUP(call, post) \
	LUA_HOOK_PUSH_EDICT(ent) \
	LUA_HOOK_PUSH_VECTOR(vec) \
	LUA_HOOK_CALL(4)

#define LUA_SIMPLE_VOID_HOOK_EDICT_FLOAT_VECT(call, post, ent, f, vec) \
	LUA_HOOK_LOOKUP(call, post) \
	LUA_HOOK_PUSH_EDICT(ent) \
	LUA_HOOK_PUSH_FLOAT(f) \
	LUA_HOOK_PUSH_VECTOR(vec) \
	LUA_HOOK_CALL(5)

#define LUA_SIMPLE_VOID_HOOK_EDICT_CONSTVECT_CONSTVECT(call, post, ent, vec, vecb) \
	LUA_HOOK_LOOKUP(call, post) \
	LUA_HOOK_PUSH_EDICT(ent) \
	LUA_HOOK_PUSH_VECTOR(vec) \
	LUA_HOOK_PUSH_VECTOR(vecb) \
	LUA_HOOK_CALL(7)

#define LUA_SIMPLE_VOID_HOOK_CONSTVECT_CONSTVECT_FLOAT_FLOAT(call, post, vec, vecb, fla, flb) \
	LUA_HOOK_LOOKUP(call, post) \
	LUA_HOOK_PUSH_VECTOR(vec) \
	LUA_HOOK_PUSH_VECTOR(vecb) \
	LUA_HOOK_PUSH_FLOAT(fla) \
	LUA_HOOK_PUSH_FLOAT(flb) \
	LUA_HOOK_CALL(8)

#define LUA_SIMPLE_VOID_HOOK_CONSTVECT_VECT_VECT_VECT(call, post, vec, vecb, vecc, vecd) \
	LUA_HOOK_LOOKUP(call, post) \
	LUA_HOOK_PUSH_VECTOR(vec) \
	LUA_HOOK_PUSH_VECTOR(vecb) \
	LUA_HOOK_PUSH_VECTOR(vecc) \
	LUA_HOOK_PUSH_VECTOR(vecd) \
	LUA_HOOK_CALL(12)

#define LUA_SIMPLE_INT_HOOK_EDICT_EDICT(call, post, ent, entb) \
	LUA_SIMPLE_VOID_HOOK_EDICT_EDICT(call, post, ent, entb)

#define LUA_SIMPLE_INT_HOOK_EDICT_FLOAT_FLOAT_INT(call, post, ent, f, fb, v) \
	LUA_HOOK_LOOKUP(call, post) \
	LUA_HOOK_PUSH_EDICT(ent) \
	LUA_HOOK_PUSH_FLOAT(f) \
	LUA_HOOK_PUSH_FLOAT(fb) \
	LUA_HOOK_PUSH_INT(v) \
	LUA_HOOK_CALL(4)

#define LUA_SIMPLE_BOOL_HOOK_INT_INT(call, post, v, vb) \
	LUA_SIMPLE_VOID_HOOK_INT_INT(call, post, v, vb)

#define LUA_SIMPLE_BOOL_HOOK_INT_INT_BOOL(call, post, v, vb, bah) \
	LUA_HOOK_LOOKUP(call, post) \
	LUA_HOOK_PUSH_INT(v) \
	LUA_HOOK_PUSH_INT(vb) \
	LUA_HOOK_PUSH_INT((bah) > 0 ? 1 : 0) \
	LUA_HOOK_CALL(3)

#define FM_MakeLuaForward(_0, ...) FM_ExecuteCurrentLuaForwardArgs(__VA_ARGS__)

#define FM_ENG_HANDLE0(pfnCall, pfnArgs) \
	register unsigned int i = 0; \
	clfm(); \
	int fmres = FMRES_IGNORED; \
	int lastFmRes = FMRES_IGNORED; \
	for (i=0; i<Engine[pfnCall].length(); i++) \
	{ \
		fmres = MF_ExecuteForward pfnArgs; \
		if (fmres >= lastFmRes) { \
			if (retType == FMV_STRING) \
				mlStringResult = mStringResult; \
			else if (retType == FMV_CELL) \
				mlCellResult = mCellResult; \
			else if (retType == FMV_FLOAT) \
				mlFloatResult = mFloatResult; \
			lastFmRes = fmres; \
		} \
	} \
	FM_MakeLuaForwardName(#pfnCall + 3, false); \
	fmres = FM_MakeLuaForward pfnArgs; \
	if (fmres >= lastFmRes) { \
		if (retType == FMV_STRING) \
			mlStringResult = mStringResult; \
		else if (retType == FMV_CELL) \
			mlCellResult = mCellResult; \
		else if (retType == FMV_FLOAT) \
			mlFloatResult = mFloatResult; \
		lastFmRes = fmres; \
	}

#define LUA_SIMPLE_HOOK_DISABLED(...) 

#undef LUA_SIMPLE_CHAR_HOOK_CONSTSTRING_RETURN
#define LUA_SIMPLE_CHAR_HOOK_CONSTSTRING_RETURN(...) LUA_SIMPLE_HOOK_DISABLED(__VA_ARGS__)
#undef LUA_SIMPLE_STRING_HOOK_STRING_CONSTSTRING_RW
#define LUA_SIMPLE_STRING_HOOK_STRING_CONSTSTRING_RW(...) LUA_SIMPLE_HOOK_DISABLED(__VA_ARGS__)
#undef LUA_SIMPLE_BOOL_HOOK_EDICT_CONSTSTRING_CONSTSTRING_STRING128_RW
#define LUA_SIMPLE_BOOL_HOOK_EDICT_CONSTSTRING_CONSTSTRING_STRING128_RW(...) LUA_SIMPLE_HOOK_DISABLED(__VA_ARGS__)
#undef LUA_SIMPLE_VOID_HOOK_STRING_CONSTSTRING_CONSTSTRING_RW
#define LUA_SIMPLE_VOID_HOOK_STRING_CONSTSTRING_CONSTSTRING_RW(...) LUA_SIMPLE_HOOK_DISABLED(__VA_ARGS__)
#undef LUA_SIMPLE_VOID_HOOK_INT_STRING_CONSTSTRING_CONSTSTRING_RW
#define LUA_SIMPLE_VOID_HOOK_INT_STRING_CONSTSTRING_CONSTSTRING_RW(...) LUA_SIMPLE_HOOK_DISABLED(__VA_ARGS__)
#undef LUA_SIMPLE_VOID_HOOK_VOID
#define LUA_SIMPLE_VOID_HOOK_VOID(...) LUA_SIMPLE_HOOK_DISABLED(__VA_ARGS__)
#undef LUA_SIMPLE_VOID_HOOK_INT
#define LUA_SIMPLE_VOID_HOOK_INT(...) LUA_SIMPLE_HOOK_DISABLED(__VA_ARGS__)
#undef LUA_SIMPLE_VOID_HOOK_FLOAT
#define LUA_SIMPLE_VOID_HOOK_FLOAT(...) LUA_SIMPLE_HOOK_DISABLED(__VA_ARGS__)
#undef LUA_SIMPLE_VOID_HOOK_EDICT
#define LUA_SIMPLE_VOID_HOOK_EDICT(...) LUA_SIMPLE_HOOK_DISABLED(__VA_ARGS__)
#undef LUA_SIMPLE_VOID_HOOK_EDICT_EDICT
#define LUA_SIMPLE_VOID_HOOK_EDICT_EDICT(...) LUA_SIMPLE_HOOK_DISABLED(__VA_ARGS__)
#undef LUA_SIMPLE_VOID_HOOK_CONSTSTRING
#define LUA_SIMPLE_VOID_HOOK_CONSTSTRING(...) LUA_SIMPLE_HOOK_DISABLED(__VA_ARGS__)
#undef LUA_SIMPLE_VOID_HOOK_INT_CONSTSTRING
#define LUA_SIMPLE_VOID_HOOK_INT_CONSTSTRING(...) LUA_SIMPLE_HOOK_DISABLED(__VA_ARGS__)
#undef LUA_SIMPLE_VOID_HOOK_CONSTSTRING_CONSTSTRING
#define LUA_SIMPLE_VOID_HOOK_CONSTSTRING_CONSTSTRING(...) LUA_SIMPLE_HOOK_DISABLED(__VA_ARGS__)
#undef LUA_SIMPLE_VOID_HOOK_CONSTSTRING_FLOAT
#define LUA_SIMPLE_VOID_HOOK_CONSTSTRING_FLOAT(...) LUA_SIMPLE_HOOK_DISABLED(__VA_ARGS__)
#undef LUA_SIMPLE_VOID_HOOK_INT_INT
#define LUA_SIMPLE_VOID_HOOK_INT_INT(...) LUA_SIMPLE_HOOK_DISABLED(__VA_ARGS__)
#undef LUA_SIMPLE_INT_HOOK_VOID
#define LUA_SIMPLE_INT_HOOK_VOID(...) LUA_SIMPLE_HOOK_DISABLED(__VA_ARGS__)
#undef LUA_SIMPLE_INT_HOOK_INT
#define LUA_SIMPLE_INT_HOOK_INT(...) LUA_SIMPLE_HOOK_DISABLED(__VA_ARGS__)
#undef LUA_SIMPLE_INT_HOOK_EDICT
#define LUA_SIMPLE_INT_HOOK_EDICT(...) LUA_SIMPLE_HOOK_DISABLED(__VA_ARGS__)
#undef LUA_SIMPLE_INT_HOOK_CONSTEDICT
#define LUA_SIMPLE_INT_HOOK_CONSTEDICT(...) LUA_SIMPLE_HOOK_DISABLED(__VA_ARGS__)
#undef LUA_SIMPLE_FLOAT_HOOK_VOID
#define LUA_SIMPLE_FLOAT_HOOK_VOID(...) LUA_SIMPLE_HOOK_DISABLED(__VA_ARGS__)
#undef LUA_SIMPLE_FLOAT_HOOK_CONSTSTRING
#define LUA_SIMPLE_FLOAT_HOOK_CONSTSTRING(...) LUA_SIMPLE_HOOK_DISABLED(__VA_ARGS__)
#undef LUA_SIMPLE_CONSTSTRING_HOOK_VOID
#define LUA_SIMPLE_CONSTSTRING_HOOK_VOID(...) LUA_SIMPLE_HOOK_DISABLED(__VA_ARGS__)
#undef LUA_SIMPLE_CONSTSTRING_HOOK_CONSTSTRING
#define LUA_SIMPLE_CONSTSTRING_HOOK_CONSTSTRING(...) LUA_SIMPLE_HOOK_DISABLED(__VA_ARGS__)
#undef LUA_SIMPLE_CONSTSTRING_HOOK_INT
#define LUA_SIMPLE_CONSTSTRING_HOOK_INT(...) LUA_SIMPLE_HOOK_DISABLED(__VA_ARGS__)
#undef LUA_SIMPLE_CONSTSTRING_HOOK_CONSTEDICT
#define LUA_SIMPLE_CONSTSTRING_HOOK_CONSTEDICT(...) LUA_SIMPLE_HOOK_DISABLED(__VA_ARGS__)
#undef LUA_SIMPLE_CONSTSTRING_HOOK_CONSTEDICT_CONSTSTRING
#define LUA_SIMPLE_CONSTSTRING_HOOK_CONSTEDICT_CONSTSTRING(...) LUA_SIMPLE_HOOK_DISABLED(__VA_ARGS__)
#undef LUA_SIMPLE_STRING_HOOK_STRING_STRING
#define LUA_SIMPLE_STRING_HOOK_STRING_STRING(...) LUA_SIMPLE_HOOK_DISABLED(__VA_ARGS__)
#undef LUA_SIMPLE_STRING_HOOK_STRING_CONSTSTRING
#define LUA_SIMPLE_STRING_HOOK_STRING_CONSTSTRING(...) LUA_SIMPLE_HOOK_DISABLED(__VA_ARGS__)
#undef LUA_SIMPLE_CHAR_HOOK_STRING
#define LUA_SIMPLE_CHAR_HOOK_STRING(...) LUA_SIMPLE_HOOK_DISABLED(__VA_ARGS__)
#undef LUA_SIMPLE_CHAR_HOOK_CONSTSTRING
#define LUA_SIMPLE_CHAR_HOOK_CONSTSTRING(...) LUA_SIMPLE_HOOK_DISABLED(__VA_ARGS__)
#undef LUA_SIMPLE_EDICT_HOOK_VOID
#define LUA_SIMPLE_EDICT_HOOK_VOID(...) LUA_SIMPLE_HOOK_DISABLED(__VA_ARGS__)
#undef LUA_SIMPLE_EDICT_HOOK_INT
#define LUA_SIMPLE_EDICT_HOOK_INT(...) LUA_SIMPLE_HOOK_DISABLED(__VA_ARGS__)
#undef LUA_SIMPLE_EDICT_HOOK_EDICT
#define LUA_SIMPLE_EDICT_HOOK_EDICT(...) LUA_SIMPLE_HOOK_DISABLED(__VA_ARGS__)
#undef LUA_SIMPLE_EDICT_HOOK_CONSTSTRING
#define LUA_SIMPLE_EDICT_HOOK_CONSTSTRING(...) LUA_SIMPLE_HOOK_DISABLED(__VA_ARGS__)
#undef LUA_SIMPLE_CONSTSTRING_HOOK_EDICT
#define LUA_SIMPLE_CONSTSTRING_HOOK_EDICT(...) LUA_SIMPLE_HOOK_DISABLED(__VA_ARGS__)
#undef LUA_SIMPLE_VOID_HOOK_CONSTEDICT
#define LUA_SIMPLE_VOID_HOOK_CONSTEDICT(...) LUA_SIMPLE_HOOK_DISABLED(__VA_ARGS__)
#undef LUA_SIMPLE_VOID_HOOK_CONSTEDICT_FLOAT
#define LUA_SIMPLE_VOID_HOOK_CONSTEDICT_FLOAT(...) LUA_SIMPLE_HOOK_DISABLED(__VA_ARGS__)
#undef LUA_SIMPLE_VOID_HOOK_CONSTEDICT_FLOAT_FLOAT
#define LUA_SIMPLE_VOID_HOOK_CONSTEDICT_FLOAT_FLOAT(...) LUA_SIMPLE_HOOK_DISABLED(__VA_ARGS__)
#undef LUA_SIMPLE_VOID_HOOK_CONSTEDICT_CONSTEDICT
#define LUA_SIMPLE_VOID_HOOK_CONSTEDICT_CONSTEDICT(...) LUA_SIMPLE_HOOK_DISABLED(__VA_ARGS__)
#undef LUA_SIMPLE_VOID_HOOK_CONSTEDICT_INT_INT_INT_INT
#define LUA_SIMPLE_VOID_HOOK_CONSTEDICT_INT_INT_INT_INT(...) LUA_SIMPLE_HOOK_DISABLED(__VA_ARGS__)
#undef LUA_SIMPLE_VOID_HOOK_CONSTEDICT_CONSTSTRING_CONSTSTRING
#define LUA_SIMPLE_VOID_HOOK_CONSTEDICT_CONSTSTRING_CONSTSTRING(...) LUA_SIMPLE_HOOK_DISABLED(__VA_ARGS__)
#undef LUA_SIMPLE_UINT_HOOK_EDICT
#define LUA_SIMPLE_UINT_HOOK_EDICT(...) LUA_SIMPLE_HOOK_DISABLED(__VA_ARGS__)
#undef LUA_SIMPLE_BOOL_HOOK_EDICT_CONSTSTRING_CONSTSTRING_STRING128
#define LUA_SIMPLE_BOOL_HOOK_EDICT_CONSTSTRING_CONSTSTRING_STRING128(...) LUA_SIMPLE_HOOK_DISABLED(__VA_ARGS__)
#undef LUA_SIMPLE_VOID_HOOK_EDICT_INT_CONSTSTRING_FLOAT_FLOAT_INT_INT
#define LUA_SIMPLE_VOID_HOOK_EDICT_INT_CONSTSTRING_FLOAT_FLOAT_INT_INT(...) LUA_SIMPLE_HOOK_DISABLED(__VA_ARGS__)
#undef LUA_SIMPLE_VOID_HOOK_EDICT_VECT_CONSTSTRING_FLOAT_FLOAT_INT_INT
#define LUA_SIMPLE_VOID_HOOK_EDICT_VECT_CONSTSTRING_FLOAT_FLOAT_INT_INT(...) LUA_SIMPLE_HOOK_DISABLED(__VA_ARGS__)
#undef LUA_SIMPLE_EDICT_HOOK_EDICT_CONSTVECT_FLOAT
#define LUA_SIMPLE_EDICT_HOOK_EDICT_CONSTVECT_FLOAT(...) LUA_SIMPLE_HOOK_DISABLED(__VA_ARGS__)
#undef LUA_SIMPLE_HOOK_PLAYBACK_EVENT
#define LUA_SIMPLE_HOOK_PLAYBACK_EVENT(...) LUA_SIMPLE_HOOK_DISABLED(__VA_ARGS__)
#undef LUA_SIMPLE_INT_HOOK_STRING
#define LUA_SIMPLE_INT_HOOK_STRING(...) LUA_SIMPLE_HOOK_DISABLED(__VA_ARGS__)
#undef LUA_SIMPLE_INT_HOOK_CONSTSTRING
#define LUA_SIMPLE_INT_HOOK_CONSTSTRING(...) LUA_SIMPLE_HOOK_DISABLED(__VA_ARGS__)
#undef LUA_SIMPLE_INT_HOOK_CONSTSTRING_INT
#define LUA_SIMPLE_INT_HOOK_CONSTSTRING_INT(...) LUA_SIMPLE_HOOK_DISABLED(__VA_ARGS__)
#undef LUA_SIMPLE_USHORT_HOOK_INT_CONSTSTRING
#define LUA_SIMPLE_USHORT_HOOK_INT_CONSTSTRING(...) LUA_SIMPLE_HOOK_DISABLED(__VA_ARGS__)
#undef LUA_SIMPLE_FLOAT_HOOK_CONSTVECT
#define LUA_SIMPLE_FLOAT_HOOK_CONSTVECT(...) LUA_SIMPLE_HOOK_DISABLED(__VA_ARGS__)
#undef LUA_SIMPLE_INT_HOOK_CONSTVECT
#define LUA_SIMPLE_INT_HOOK_CONSTVECT(...) LUA_SIMPLE_HOOK_DISABLED(__VA_ARGS__)
#undef LUA_SIMPLE_VOID_HOOK_CONSTVECT
#define LUA_SIMPLE_VOID_HOOK_CONSTVECT(...) LUA_SIMPLE_HOOK_DISABLED(__VA_ARGS__)
#undef LUA_SIMPLE_VOID_HOOK_EDICT_CONSTVECT
#define LUA_SIMPLE_VOID_HOOK_EDICT_CONSTVECT(...) LUA_SIMPLE_HOOK_DISABLED(__VA_ARGS__)
#undef LUA_SIMPLE_VOID_HOOK_EDICT_FLOAT_VECT
#define LUA_SIMPLE_VOID_HOOK_EDICT_FLOAT_VECT(...) LUA_SIMPLE_HOOK_DISABLED(__VA_ARGS__)
#undef LUA_SIMPLE_VOID_HOOK_EDICT_CONSTVECT_CONSTVECT
#define LUA_SIMPLE_VOID_HOOK_EDICT_CONSTVECT_CONSTVECT(...) LUA_SIMPLE_HOOK_DISABLED(__VA_ARGS__)
#undef LUA_SIMPLE_VOID_HOOK_CONSTVECT_CONSTVECT_FLOAT_FLOAT
#define LUA_SIMPLE_VOID_HOOK_CONSTVECT_CONSTVECT_FLOAT_FLOAT(...) LUA_SIMPLE_HOOK_DISABLED(__VA_ARGS__)
#undef LUA_SIMPLE_VOID_HOOK_CONSTVECT_VECT_VECT_VECT
#define LUA_SIMPLE_VOID_HOOK_CONSTVECT_VECT_VECT_VECT(...) LUA_SIMPLE_HOOK_DISABLED(__VA_ARGS__)
#undef LUA_SIMPLE_INT_HOOK_EDICT_EDICT
#define LUA_SIMPLE_INT_HOOK_EDICT_EDICT(...) LUA_SIMPLE_HOOK_DISABLED(__VA_ARGS__)
#undef LUA_SIMPLE_INT_HOOK_EDICT_FLOAT_FLOAT_INT
#define LUA_SIMPLE_INT_HOOK_EDICT_FLOAT_FLOAT_INT(...) LUA_SIMPLE_HOOK_DISABLED(__VA_ARGS__)
#undef LUA_SIMPLE_BOOL_HOOK_INT_INT
#define LUA_SIMPLE_BOOL_HOOK_INT_INT(...) LUA_SIMPLE_HOOK_DISABLED(__VA_ARGS__)
#undef LUA_SIMPLE_BOOL_HOOK_INT_INT_BOOL
#define LUA_SIMPLE_BOOL_HOOK_INT_INT_BOOL(...) LUA_SIMPLE_HOOK_DISABLED(__VA_ARGS__)

#define SIMPLE_VOID_HOOK_EDICT(call) \
	void call (edict_t *ent) \
	{ \
		FM_ENG_HANDLE(FM_##call, (Engine[FM_##call].at(i),  (cell)ENTINDEX(ent))); \
		LUA_SIMPLE_VOID_HOOK_EDICT(call, , ent) \
		RETURN_META(mswi(lastFmRes)); \
	} \
	void call##_post (edict_t *ent) \
	{ \
		FM_ENG_HANDLE_POST(FM_##call, (EnginePost[FM_##call].at(i),  (cell)ENTINDEX(ent))); \
		LUA_SIMPLE_VOID_HOOK_EDICT(call, POST, ent) \
		RETURN_META(MRES_IGNORED); \
	} 
#define SIMPLE_EDICT_HOOK_VOID(call) \
	edict_t* call () \
	{ \
		FM_ENG_HANDLE0(FM_##call, (Engine[FM_##call].at(i))); \
		LUA_SIMPLE_EDICT_HOOK_VOID(call, ) \
		RETURN_META_VALUE(mswi(lastFmRes),TypeConversion.id_to_edict((int)mlCellResult)); \
	} \
	edict_t* call##_post () \
	{ \
		origCellRet = ENTINDEX(META_RESULT_ORIG_RET(edict_t *)); \
		FM_ENG_HANDLE_POST0(FM_##call, (EnginePost[FM_##call].at(i))); \
		LUA_SIMPLE_EDICT_HOOK_VOID(call, POST) \
		RETURN_META_VALUE(MRES_IGNORED,TypeConversion.id_to_edict((int)mlCellResult)); \
	} 
#define SIMPLE_EDICT_HOOK_INT(call) \
	edict_t* call (int v) \
	{ \
		FM_ENG_HANDLE(FM_##call, (Engine[FM_##call].at(i),(cell)v)); \
		LUA_SIMPLE_EDICT_HOOK_INT(call, , v) \
		RETURN_META_VALUE(mswi(lastFmRes),TypeConversion.id_to_edict((int)mlCellResult)); \
	} \
	edict_t* call##_post (int v) \
	{ \
		origCellRet = ENTINDEX(META_RESULT_ORIG_RET(edict_t *)); \
		FM_ENG_HANDLE_POST(FM_##call, (EnginePost[FM_##call].at(i),(cell)v)); \
		LUA_SIMPLE_EDICT_HOOK_INT(call, POST, v) \
		RETURN_META_VALUE(MRES_IGNORED,TypeConversion.id_to_edict((int)mlCellResult)); \
	} 

#define SIMPLE_EDICT_HOOK_EDICT(call) \
	edict_t* call (edict_t *e) \
	{ \
		FM_ENG_HANDLE(FM_##call, (Engine[FM_##call].at(i),(cell)ENTINDEX(e))); \
		LUA_SIMPLE_EDICT_HOOK_EDICT(call, , e) \
		RETURN_META_VALUE(mswi(lastFmRes),TypeConversion.id_to_edict((int)mlCellResult)); \
	} \
	edict_t* call##_post (edict_t *e) \
	{ \
		origCellRet = ENTINDEX(META_RESULT_ORIG_RET(edict_t *)); \
		FM_ENG_HANDLE_POST(FM_##call, (EnginePost[FM_##call].at(i),(cell)ENTINDEX(e))); \
		LUA_SIMPLE_EDICT_HOOK_EDICT(call, POST, e) \
		RETURN_META_VALUE(MRES_IGNORED,TypeConversion.id_to_edict((int)mlCellResult)); \
	} 

#define SIMPLE_EDICT_HOOK_EDICT_CONSTVECT_FLOAT(call) \
	edict_t* call (edict_t *ed, const float *vec, float fla) \
	{ \
		PREPARE_VECTOR(vec); \
		FM_ENG_HANDLE(FM_##call, (Engine[FM_##call].at(i),  (cell)ENTINDEX(ed), p_vec, fla)); \
		LUA_SIMPLE_EDICT_HOOK_EDICT_CONSTVECT_FLOAT(call, , ed, vec, fla) \
		RETURN_META_VALUE(mswi(lastFmRes), TypeConversion.id_to_edict((int)mlCellResult)); \
	} \
	edict_t* call##_post (edict_t *ed, const float *vec, float fla) \
	{ \
		PREPARE_VECTOR(vec); \
		origCellRet = ENTINDEX(META_RESULT_ORIG_RET(edict_t *)); \
		FM_ENG_HANDLE_POST(FM_##call, (EnginePost[FM_##call].at(i),  (cell)ENTINDEX(ed), p_vec, fla)); \
		LUA_SIMPLE_EDICT_HOOK_EDICT_CONSTVECT_FLOAT(call, POST, ed, vec, fla) \
		RETURN_META_VALUE(MRES_IGNORED, TypeConversion.id_to_edict((int)mlCellResult)); \
	}


#define SIMPLE_VOID_HOOK_EDICT_EDICT(call) \
	void call (edict_t *ent,edict_t *entb) \
	{ \
		FM_ENG_HANDLE(FM_##call, (Engine[FM_##call].at(i),  (cell)ENTINDEX(ent), (cell)ENTINDEX(entb))); \
		LUA_SIMPLE_VOID_HOOK_EDICT_EDICT(call, , ent, entb) \
		RETURN_META(mswi(lastFmRes)); \
	} \
	void call##_post (edict_t *ent,edict_t *entb) \
	{ \
		FM_ENG_HANDLE_POST(FM_##call, (EnginePost[FM_##call].at(i),  (cell)ENTINDEX(ent), (cell)ENTINDEX(entb))); \
		LUA_SIMPLE_VOID_HOOK_EDICT_EDICT(call, POST, ent, entb) \
		RETURN_META(MRES_IGNORED); \
	} 

#define SIMPLE_VOID_HOOK_CONSTEDICT_CONSTEDICT(call) \
	void call (const edict_t *ent,const edict_t *entb) \
	{ \
		FM_ENG_HANDLE(FM_##call, (Engine[FM_##call].at(i), (cell)ENTINDEX((edict_t*)ent), (cell)ENTINDEX((edict_t*)entb))); \
		LUA_SIMPLE_VOID_HOOK_CONSTEDICT_CONSTEDICT(call, , ent, entb) \
		RETURN_META(mswi(lastFmRes)); \
	} \
	void call##_post (const edict_t *ent,const edict_t *entb) \
	{ \
		FM_ENG_HANDLE_POST(FM_##call, (EnginePost[FM_##call].at(i),  (cell)ENTINDEX((edict_t*)ent), (cell)ENTINDEX((edict_t*)entb))); \
		LUA_SIMPLE_VOID_HOOK_CONSTEDICT_CONSTEDICT(call, POST, ent, entb) \
		RETURN_META(MRES_IGNORED); \
	} 

#define SIMPLE_VOID_HOOK_VOID(call) \
	void call (void) \
	{ \
		FM_ENG_HANDLE0(FM_##call, (Engine[FM_##call].at(i))); \
		LUA_SIMPLE_VOID_HOOK_VOID(call, ) \
		RETURN_META(mswi(lastFmRes)); \
	} \
	void call##_post (void) \
	{ \
		FM_ENG_HANDLE_POST0(FM_##call, (EnginePost[FM_##call].at(i))); \
		LUA_SIMPLE_VOID_HOOK_VOID(call, POST) \
		RETURN_META(MRES_IGNORED); \
	} 

#define SIMPLE_INT_HOOK_EDICT(call) \
	int call (edict_t *pent) \
	{ \
		FM_ENG_HANDLE(FM_##call, (Engine[FM_##call].at(i),  (cell)ENTINDEX(pent))); \
		LUA_SIMPLE_INT_HOOK_EDICT(call, , pent) \
		RETURN_META_VALUE(mswi(lastFmRes), (int)mlCellResult); \
	} \
	int call##_post (edict_t *pent) \
	{ \
		origCellRet = META_RESULT_ORIG_RET(int); \
		FM_ENG_HANDLE_POST(FM_##call, (EnginePost[FM_##call].at(i),  (cell)ENTINDEX(pent))); \
		LUA_SIMPLE_INT_HOOK_EDICT(call, POST, pent) \
		RETURN_META_VALUE(MRES_IGNORED, (int)mlCellResult); \
	}
#define SIMPLE_UINT_HOOK_EDICT(call) \
	unsigned int call (edict_t *pent) \
	{ \
		FM_ENG_HANDLE(FM_##call, (Engine[FM_##call].at(i),  (cell)ENTINDEX(pent))); \
		LUA_SIMPLE_UINT_HOOK_EDICT(call, , pent) \
		RETURN_META_VALUE(mswi(lastFmRes), (unsigned int)mlCellResult); \
	} \
	unsigned int call##_post (edict_t *pent) \
	{ \
		origCellRet = META_RESULT_ORIG_RET(unsigned int); \
		FM_ENG_HANDLE_POST(FM_##call, (EnginePost[FM_##call].at(i),  (cell)ENTINDEX(pent))); \
		LUA_SIMPLE_UINT_HOOK_EDICT(call, POST, pent) \
		RETURN_META_VALUE(MRES_IGNORED, (unsigned int)mlCellResult); \
	}

#define SIMPLE_INT_HOOK_CONSTEDICT(call) \
	int call (const edict_t *pent) \
	{ \
		FM_ENG_HANDLE(FM_##call, (Engine[FM_##call].at(i),  (cell)ENTINDEX((edict_t*)pent))); \
		LUA_SIMPLE_INT_HOOK_CONSTEDICT(call, , pent) \
		RETURN_META_VALUE(mswi(lastFmRes), (int)mlCellResult); \
	} \
	int call##_post (const edict_t *pent) \
	{ \
		origCellRet = META_RESULT_ORIG_RET(int); \
		FM_ENG_HANDLE_POST(FM_##call, (EnginePost[FM_##call].at(i),  (cell)ENTINDEX((edict_t*)pent))); \
		LUA_SIMPLE_INT_HOOK_CONSTEDICT(call, POST, pent) \
		RETURN_META_VALUE(MRES_IGNORED, (int)mlCellResult); \
	}

#define SIMPLE_INT_HOOK_INT(call) \
	int call (int v) \
	{ \
		FM_ENG_HANDLE(FM_##call, (Engine[FM_##call].at(i),  (cell)v)); \
		LUA_SIMPLE_INT_HOOK_INT(call, , v) \
		RETURN_META_VALUE(mswi(lastFmRes), (int)mlCellResult); \
	} \
	int call##_post (int v) \
	{ \
		origCellRet = META_RESULT_ORIG_RET(int); \
		FM_ENG_HANDLE_POST(FM_##call, (EnginePost[FM_##call].at(i),  (cell)v)); \
		LUA_SIMPLE_INT_HOOK_INT(call, POST, v) \
		RETURN_META_VALUE(MRES_IGNORED, (int)mlCellResult); \
	}

#define SIMPLE_VOID_HOOK_INT_INT(call) \
	void call (int v, int vb) \
	{ \
		FM_ENG_HANDLE(FM_##call, (Engine[FM_##call].at(i), (cell)v, (cell)vb)); \
		LUA_SIMPLE_VOID_HOOK_INT_INT(call, , v, vb) \
		RETURN_META(mswi(lastFmRes)); \
	} \
	void call##_post (int v, int vb) \
	{ \
		FM_ENG_HANDLE_POST(FM_##call, (EnginePost[FM_##call].at(i), (cell)v, (cell)vb)); \
		LUA_SIMPLE_VOID_HOOK_INT_INT(call, POST, v, vb) \
		RETURN_META(MRES_IGNORED); \
	}

#define SIMPLE_BOOL_HOOK_INT_INT(call) \
	qboolean call (int v, int vb) \
	{ \
		FM_ENG_HANDLE(FM_##call, (Engine[FM_##call].at(i),  (cell)v, (cell)vb)); \
		LUA_SIMPLE_BOOL_HOOK_INT_INT(call, , v, vb) \
		RETURN_META_VALUE(mswi(lastFmRes), (int)mlCellResult > 0 ? 1 : 0); \
	} \
	qboolean call##_post (int v, int vb) \
	{ \
		origCellRet = META_RESULT_ORIG_RET(qboolean); \
		FM_ENG_HANDLE_POST(FM_##call, (EnginePost[FM_##call].at(i),  (cell)v, (cell)vb)); \
		LUA_SIMPLE_BOOL_HOOK_INT_INT(call, POST, v, vb) \
		RETURN_META_VALUE(MRES_IGNORED, (int)mlCellResult > 0 ? 1 : 0); \
	}

#define SIMPLE_BOOL_HOOK_INT_INT_BOOL(call) \
	qboolean call (int v, int vb, qboolean bah) \
	{ \
		FM_ENG_HANDLE(FM_##call, (Engine[FM_##call].at(i), (cell) v, (cell)vb, (cell)(bah > 0 ? 1 : 0))); \
		LUA_SIMPLE_BOOL_HOOK_INT_INT_BOOL(call, , v, vb, bah) \
		RETURN_META_VALUE(mswi(lastFmRes), (int)mlCellResult > 0 ? 1 : 0); \
	} \
	qboolean call##_post (int v, int vb, qboolean bah) \
	{ \
		origCellRet = META_RESULT_ORIG_RET(qboolean); \
		FM_ENG_HANDLE_POST(FM_##call, (EnginePost[FM_##call].at(i), (cell)v, (cell)vb, (cell)(bah > 0 ? 1 : 0))); \
		LUA_SIMPLE_BOOL_HOOK_INT_INT_BOOL(call, POST, v, vb, bah) \
		RETURN_META_VALUE(MRES_IGNORED, (int)mlCellResult > 0 ? 1 : 0); \
	}

#define SIMPLE_INT_HOOK_VOID(call) \
	int call () \
	{ \
		FM_ENG_HANDLE0(FM_##call, (Engine[FM_##call].at(i))); \
		LUA_SIMPLE_INT_HOOK_VOID(call, ) \
		RETURN_META_VALUE(mswi(lastFmRes), (int)mlCellResult); \
	} \
	int call##_post () \
	{ \
		origCellRet = META_RESULT_ORIG_RET(int); \
		FM_ENG_HANDLE_POST0(FM_##call, (EnginePost[FM_##call].at(i))); \
		LUA_SIMPLE_INT_HOOK_VOID(call, POST) \
		RETURN_META_VALUE(MRES_IGNORED, (int)mlCellResult); \
	}
#define SIMPLE_FLOAT_HOOK_VOID(call) \
	float call () \
	{ \
		FM_ENG_HANDLE0(FM_##call, (Engine[FM_##call].at(i))); \
		LUA_SIMPLE_FLOAT_HOOK_VOID(call, ) \
		RETURN_META_VALUE(mswi(lastFmRes), (float)mFloatResult); \
	} \
	float call##_post () \
	{ \
		origFloatRet = META_RESULT_ORIG_RET(float); \
		FM_ENG_HANDLE_POST0(FM_##call, (EnginePost[FM_##call].at(i))); \
		LUA_SIMPLE_FLOAT_HOOK_VOID(call, POST) \
		RETURN_META_VALUE(MRES_IGNORED, (float)mFloatResult); \
	}


#define SIMPLE_INT_HOOK_CONSTVECT(call) \
	int call (const float *vec) \
	{ \
		PREPARE_VECTOR(vec); \
		FM_ENG_HANDLE(FM_##call, (Engine[FM_##call].at(i),  p_vec)); \
		LUA_SIMPLE_INT_HOOK_CONSTVECT(call, , vec) \
		RETURN_META_VALUE(mswi(lastFmRes), (int)mlCellResult); \
	} \
	int call##_post (const float *vec) \
	{ \
		PREPARE_VECTOR(vec); \
		origCellRet = META_RESULT_ORIG_RET(int); \
		FM_ENG_HANDLE_POST(FM_##call, (EnginePost[FM_##call].at(i),  p_vec)); \
		LUA_SIMPLE_INT_HOOK_CONSTVECT(call, POST, vec) \
		RETURN_META_VALUE(MRES_IGNORED, (int)mlCellResult); \
	}

#define SIMPLE_VOID_HOOK_EDICT_CONSTVECT(call) \
	void call (edict_t *e, const float *vec) \
	{ \
		PREPARE_VECTOR(vec); \
		FM_ENG_HANDLE(FM_##call, (Engine[FM_##call].at(i),  (cell)ENTINDEX(e), p_vec)); \
		LUA_SIMPLE_VOID_HOOK_EDICT_CONSTVECT(call, , e, vec) \
		RETURN_META(mswi(lastFmRes)); \
	} \
	void call##_post (edict_t *e, const float *vec) \
	{ \
		PREPARE_VECTOR(vec); \
		FM_ENG_HANDLE_POST(FM_##call, (EnginePost[FM_##call].at(i),  (cell)ENTINDEX(e), p_vec)); \
		LUA_SIMPLE_VOID_HOOK_EDICT_CONSTVECT(call, POST, e, vec) \
		RETURN_META(MRES_IGNORED); \
	}

#define SIMPLE_VOID_HOOK_EDICT_FLOAT_VECT(call) \
	void call (edict_t *e, float f, float *vec) \
	{ \
		PREPARE_VECTOR(vec); \
		FM_ENG_HANDLE(FM_##call, (Engine[FM_##call].at(i),  (cell)ENTINDEX(e), f, p_vec)); \
		LUA_SIMPLE_VOID_HOOK_EDICT_FLOAT_VECT(call, , e, f, vec) \
		RETURN_META(mswi(lastFmRes)); \
	} \
	void call##_post (edict_t *e, float f, float *vec) \
	{ \
		PREPARE_VECTOR(vec); \
		FM_ENG_HANDLE_POST(FM_##call, (EnginePost[FM_##call].at(i),  (cell)ENTINDEX(e), f, p_vec)); \
		LUA_SIMPLE_VOID_HOOK_EDICT_FLOAT_VECT(call, POST, e, f, vec) \
		RETURN_META(MRES_IGNORED); \
	}


#define SIMPLE_VOID_HOOK_CONSTVECT(call) \
	void call (const float *vec) \
	{ \
		PREPARE_VECTOR(vec); \
		FM_ENG_HANDLE(FM_##call, (Engine[FM_##call].at(i),  p_vec)); \
		LUA_SIMPLE_VOID_HOOK_CONSTVECT(call, , vec) \
		RETURN_META(mswi(lastFmRes)); \
	} \
	void call##_post (const float *vec) \
	{ \
		PREPARE_VECTOR(vec); \
		FM_ENG_HANDLE_POST(FM_##call, (EnginePost[FM_##call].at(i),  p_vec)); \
		LUA_SIMPLE_VOID_HOOK_CONSTVECT(call, POST, vec) \
		RETURN_META(MRES_IGNORED); \
	}

#define SIMPLE_VOID_HOOK_CONSTVECT_VECT_VECT_VECT(call) \
	void call (const float *vec, float *vecb, float *vecc, float *vecd) \
	{ \
		PREPARE_VECTOR(vec); \
		PREPARE_VECTOR(vecb); \
		PREPARE_VECTOR(vecc); \
		PREPARE_VECTOR(vecd); \
		FM_ENG_HANDLE(FM_##call, (Engine[FM_##call].at(i),  p_vec, p_vecb, p_vecc, p_vecd)); \
		LUA_SIMPLE_VOID_HOOK_CONSTVECT_VECT_VECT_VECT(call, , vec, vecb, vecc, vecd) \
		RETURN_META(mswi(lastFmRes)); \
	} \
	void call##_post (const float *vec, float *vecb, float *vecc, float *vecd) \
	{ \
		PREPARE_VECTOR(vec); \
		PREPARE_VECTOR(vecb); \
		PREPARE_VECTOR(vecc); \
		PREPARE_VECTOR(vecd); \
		FM_ENG_HANDLE_POST(FM_##call, (EnginePost[FM_##call].at(i),  p_vec, p_vecb, p_vecc, p_vecd)); \
		LUA_SIMPLE_VOID_HOOK_CONSTVECT_VECT_VECT_VECT(call, POST, vec, vecb, vecc, vecd) \
		RETURN_META(MRES_IGNORED); \
	}

#define SIMPLE_VOID_HOOK_EDICT_CONSTVECT_CONSTVECT(call) \
	void call (edict_t *e, const float *vec, const float *vecb) \
	{ \
		PREPARE_VECTOR(vec); \
		PREPARE_VECTOR(vecb); \
		FM_ENG_HANDLE(FM_##call, (Engine[FM_##call].at(i), (cell)ENTINDEX(e), p_vec, p_vecb)); \
		LUA_SIMPLE_VOID_HOOK_EDICT_CONSTVECT_CONSTVECT(call, , e, vec, vecb) \
		RETURN_META(mswi(lastFmRes)); \
	} \
	void call##_post (edict_t *e, const float *vec, const float *vecb) \
	{ \
		PREPARE_VECTOR(vec); \
		PREPARE_VECTOR(vecb); \
		FM_ENG_HANDLE_POST(FM_##call, (EnginePost[FM_##call].at(i), (cell)ENTINDEX(e), p_vec, p_vecb)); \
		LUA_SIMPLE_VOID_HOOK_EDICT_CONSTVECT_CONSTVECT(call, POST, e, vec, vecb) \
		RETURN_META(MRES_IGNORED); \
	}

#define SIMPLE_FLOAT_HOOK_CONSTVECT(call) \
	float call (const float *vec) \
	{ \
		PREPARE_VECTOR(vec); \
		FM_ENG_HANDLE(FM_##call, (Engine[FM_##call].at(i),  p_vec)); \
		LUA_SIMPLE_FLOAT_HOOK_CONSTVECT(call, , vec) \
		RETURN_META_VALUE(mswi(lastFmRes),mlFloatResult); \
	} \
	float call##_post (const float *vec) \
	{ \
		PREPARE_VECTOR(vec); \
		origFloatRet = META_RESULT_ORIG_RET(float); \
		FM_ENG_HANDLE_POST(FM_##call, (EnginePost[FM_##call].at(i),  p_vec)); \
		LUA_SIMPLE_FLOAT_HOOK_CONSTVECT(call, POST, vec) \
		RETURN_META_VALUE(MRES_IGNORED,mlFloatResult); \
	}
#define SIMPLE_FLOAT_HOOK_CONSTSTRING(call) \
	float call (const char *s) \
	{ \
		FM_ENG_HANDLE(FM_##call, (Engine[FM_##call].at(i),  s)); \
		LUA_SIMPLE_FLOAT_HOOK_CONSTSTRING(call, , s) \
		RETURN_META_VALUE(mswi(lastFmRes),mlFloatResult); \
	} \
	float call##_post (const char *s) \
	{ \
		origFloatRet = META_RESULT_ORIG_RET(float); \
		FM_ENG_HANDLE_POST(FM_##call, (EnginePost[FM_##call].at(i),  s)); \
		LUA_SIMPLE_FLOAT_HOOK_CONSTSTRING(call, POST, s) \
		RETURN_META_VALUE(MRES_IGNORED,mlFloatResult); \
	}

#define SIMPLE_CONSTSTRING_HOOK_CONSTSTRING(call) \
	const char* call (const char *s) \
	{ \
		FM_ENG_HANDLE(FM_##call, (Engine[FM_##call].at(i),  s)); \
		LUA_SIMPLE_CONSTSTRING_HOOK_CONSTSTRING(call, , s) \
		RETURN_META_VALUE(mswi(lastFmRes),mlStringResult); \
	} \
	const char* call##_post (const char *s) \
	{ \
		origStringRet = META_RESULT_ORIG_RET(const char *); \
		FM_ENG_HANDLE_POST(FM_##call, (EnginePost[FM_##call].at(i),  s)); \
		LUA_SIMPLE_CONSTSTRING_HOOK_CONSTSTRING(call, POST, s) \
		RETURN_META_VALUE(MRES_IGNORED,mlStringResult); \
	}

#define SIMPLE_VOID_HOOK_INT_INT_CONSTVECT_EDICT(call) \
	void call (int v, int vb, const float *vec, edict_t *e) \
	{ \
		const float b[3]={0.0,0.0,0.0}; \
		if (vec == nullptr) { \
			vec = b; \
		} \
		PREPARE_VECTOR(vec); \
		FM_ENG_HANDLE(FM_##call, (Engine[FM_##call].at(i),  (cell)v, (cell)vb, p_vec, (cell)ENTINDEX(e))); \
		RETURN_META(mswi(lastFmRes)); \
	} \
	void call##_post (int v, int vb, const float *vec, edict_t *e) \
	{ \
		if (vec) { \
			PREPARE_VECTOR(vec); \
			FM_ENG_HANDLE_POST(FM_##call, (EnginePost[FM_##call].at(i),  (cell)v, (cell)vb, p_vec, (cell)ENTINDEX(e))); \
		} else { \
			const float b[3]={0.0,0.0,0.0}; \
			PREPARE_VECTOR(b); \
			FM_ENG_HANDLE_POST(FM_##call, (EnginePost[FM_##call].at(i),  (cell)v, (cell)vb, p_b, (cell)ENTINDEX(e))); \
		} \
		RETURN_META(MRES_IGNORED); \
	}
#define SIMPLE_BOOL_HOOK_EDICT_CONSTSTRING_CONSTSTRING_STRING128(call) \
	qboolean call (edict_t *e, const char *sza, const char *szb, char blah[128]) \
	{ \
		FM_ENG_HANDLE(FM_##call, (Engine[FM_##call].at(i),  (cell)ENTINDEX(e), sza, szb, blah)); \
		LUA_SIMPLE_BOOL_HOOK_EDICT_CONSTSTRING_CONSTSTRING_STRING128_RW(call, , e, sza, szb, blah) \
		RETURN_META_VALUE(mswi(lastFmRes),(int)mlCellResult > 0 ? 0 : 1); \
	} \
	qboolean call##_post (edict_t *e, const char *sza, const char *szb, char blah[128]) \
	{ \
		origCellRet = META_RESULT_ORIG_RET(qboolean); \
		FM_ENG_HANDLE_POST(FM_##call, (EnginePost[FM_##call].at(i),  (cell)ENTINDEX(e), sza, szb, blah)); \
		LUA_SIMPLE_BOOL_HOOK_EDICT_CONSTSTRING_CONSTSTRING_STRING128_RW(call, POST, e, sza, szb, blah) \
		RETURN_META_VALUE(MRES_IGNORED,(int)mlCellResult > 0 ? 0 : 1); \
	}
#define SIMPLE_VOID_HOOK_EDICT_INT_CONSTSTRING_FLOAT_FLOAT_INT_INT(call) \
	void call (edict_t *e, int v, const char *sz, float f, float fb, int vb, int vc) \
	{ \
		FM_ENG_HANDLE(FM_##call, (Engine[FM_##call].at(i),  (cell)ENTINDEX(e), (cell)v, sz, f, fb, (cell)vb, (cell)vc)); \
		LUA_SIMPLE_VOID_HOOK_EDICT_INT_CONSTSTRING_FLOAT_FLOAT_INT_INT(call, , e, v, sz, f, fb, vb, vc) \
		RETURN_META(mswi(lastFmRes)); \
	} \
	void call##_post (edict_t *e, int v, const char *sz, float f, float fb, int vb, int vc) \
	{ \
		FM_ENG_HANDLE_POST(FM_##call, (EnginePost[FM_##call].at(i),  (cell)ENTINDEX(e), (cell)v, sz, f, fb, (cell)vb, (cell)vc)); \
		LUA_SIMPLE_VOID_HOOK_EDICT_INT_CONSTSTRING_FLOAT_FLOAT_INT_INT(call, POST, e, v, sz, f, fb, vb, vc) \
		RETURN_META(MRES_IGNORED); \
	}

#define SIMPLE_VOID_HOOK_EDICT_VECT_CONSTSTRING_FLOAT_FLOAT_INT_INT(call) \
	void call (edict_t *e, float *vec, const char *sz, float f, float fb, int vb, int vc) \
	{ \
		PREPARE_VECTOR(vec); \
		FM_ENG_HANDLE(FM_##call, (Engine[FM_##call].at(i),  (cell)ENTINDEX(e), p_vec, sz, f, fb, (cell)vb, (cell)vc)); \
		LUA_SIMPLE_VOID_HOOK_EDICT_VECT_CONSTSTRING_FLOAT_FLOAT_INT_INT(call, , e, vec, sz, f, fb, vb, vc) \
		RETURN_META(mswi(lastFmRes)); \
	} \
	void call##_post (edict_t *e, float *vec, const char *sz, float f, float fb, int vb, int vc) \
	{ \
		PREPARE_VECTOR(vec); \
		FM_ENG_HANDLE_POST(FM_##call, (EnginePost[FM_##call].at(i),  (cell)ENTINDEX(e), p_vec, sz, f, fb, (cell)vb, (cell)vc)); \
		LUA_SIMPLE_VOID_HOOK_EDICT_VECT_CONSTSTRING_FLOAT_FLOAT_INT_INT(call, POST, e, vec, sz, f, fb, vb, vc) \
		RETURN_META(MRES_IGNORED); \
	}
//int flags, const edict_t *pInvoker, unsigned short eventindex, float delay, float *origin, float *angles, float fparam1, float fparam2, int iparam1, int iparam2, int bparam1, int bparam2 );
#define HOOK_PLAYBACK_EVENT(call) \
	void call (int v, const edict_t *e, unsigned short eb, float f, float *vec, float *vecb, float fb, float fc, int vb, int vc, int vd, int ve) \
	{ \
		PREPARE_VECTOR(vec); \
		PREPARE_VECTOR(vecb); \
		FM_ENG_HANDLE(FM_##call, (Engine[FM_##call].at(i),  (cell)v, e ? (cell)ENTINDEX((edict_t*)e) : -1, (cell)eb, f, p_vec, p_vecb, fb, fc, (cell)vb, (cell)vc, (cell)vd, (cell)ve)); \
		LUA_SIMPLE_HOOK_PLAYBACK_EVENT(call, , v, e, eb, f, vec, vecb, fb, fc, vb, vc, vd, ve) \
		RETURN_META(mswi(lastFmRes)); \
	} \
	void call##_post (int v, const edict_t *e, unsigned short eb, float f, float *vec, float *vecb, float fb, float fc, int vb, int vc, int vd, int ve) \
	{ \
		PREPARE_VECTOR(vec); \
		PREPARE_VECTOR(vecb); \
		FM_ENG_HANDLE_POST(FM_##call, (EnginePost[FM_##call].at(i), (cell)v, e ? (cell)ENTINDEX((edict_t*)e) : -1, (cell)eb, f, p_vec, p_vecb, fb, fc, (cell)vb, (cell)vc, (cell)vd, (cell)ve)); \
		LUA_SIMPLE_HOOK_PLAYBACK_EVENT(call, POST, v, e, eb, f, vec, vecb, fb, fc, vb, vc, vd, ve) \
		RETURN_META(MRES_IGNORED); \
	} 


#define SIMPLE_VOID_HOOK_CONSTVECT_CONSTVECT_FLOAT_FLOAT(call) \
	void call (const float *vec,const float *vecb, float fla, float flb) \
	{ \
		PREPARE_VECTOR(vec); \
		PREPARE_VECTOR(vecb); \
		FM_ENG_HANDLE(FM_##call, (Engine[FM_##call].at(i),  p_vec, p_vecb, fla, flb)); \
		LUA_SIMPLE_VOID_HOOK_CONSTVECT_CONSTVECT_FLOAT_FLOAT(call, , vec, vecb, fla, flb) \
		RETURN_META(mswi(lastFmRes)); \
	} \
	void call##_post (const float *vec,const float *vecb, float fla, float flb) \
	{ \
		PREPARE_VECTOR(vec); \
		PREPARE_VECTOR(vecb); \
		FM_ENG_HANDLE_POST(FM_##call, (EnginePost[FM_##call].at(i),  p_vec, p_vecb, fla, flb)); \
		LUA_SIMPLE_VOID_HOOK_CONSTVECT_CONSTVECT_FLOAT_FLOAT(call, POST, vec, vecb, fla, flb) \
		RETURN_META(MRES_IGNORED); \
	}

#define SIMPLE_INT_HOOK_EDICT_EDICT(call) \
	int call (edict_t *pent,edict_t *pentb) \
	{ \
		FM_ENG_HANDLE(FM_##call, (Engine[FM_##call].at(i),  (cell)ENTINDEX(pent), (cell)ENTINDEX(pentb))); \
		LUA_SIMPLE_INT_HOOK_EDICT_EDICT(call, , pent, pentb) \
		RETURN_META_VALUE(mswi(lastFmRes), (int)mlCellResult); \
	} \
	int call##_post (edict_t *pent,edict_t *pentb) \
	{ \
		origCellRet = META_RESULT_ORIG_RET(int); \
		FM_ENG_HANDLE_POST(FM_##call, (EnginePost[FM_##call].at(i),  (cell)ENTINDEX(pent), (cell)ENTINDEX(pentb))); \
		LUA_SIMPLE_INT_HOOK_EDICT_EDICT(call, POST, pent, pentb) \
		RETURN_META_VALUE(MRES_IGNORED, (int)mlCellResult); \
	}

#define SIMPLE_INT_HOOK_EDICT_FLOAT_FLOAT_INT(call) \
	int call (edict_t *pent, float f, float fb, int v) \
	{ \
		FM_ENG_HANDLE(FM_##call, (Engine[FM_##call].at(i), (cell)ENTINDEX(pent), f, fb, (cell)v)); \
		LUA_SIMPLE_INT_HOOK_EDICT_FLOAT_FLOAT_INT(call, , pent, f, fb, v) \
		RETURN_META_VALUE(mswi(lastFmRes), (int)mlCellResult); \
	} \
	int call##_post (edict_t *pent, float f, float fb, int v) \
	{ \
		origCellRet = META_RESULT_ORIG_RET(int); \
		FM_ENG_HANDLE_POST(FM_##call, (EnginePost[FM_##call].at(i), (cell)ENTINDEX(pent), f, fb, (cell)v)); \
		LUA_SIMPLE_INT_HOOK_EDICT_FLOAT_FLOAT_INT(call, POST, pent, f, fb, v) \
		RETURN_META_VALUE(MRES_IGNORED, (int)mlCellResult); \
	} \

#define ENGHOOK(pfnCall) \
	if (post) \
	{ \
		EngineAddrsPost[FM_##pfnCall] = &engtable->pfn##pfnCall; \
		if (engtable->pfn##pfnCall == NULL) \
			engtable->pfn##pfnCall = pfnCall##_post; \
	} \
	else \
	{ \
		EngineAddrs[FM_##pfnCall] = &engtable->pfn##pfnCall; \
		if (engtable->pfn##pfnCall == NULL) \
			engtable->pfn##pfnCall = pfnCall; \
	} 

#define DLLHOOK(pfnCall) \
	if (post) \
	{ \
		EngineAddrsPost[FM_##pfnCall] = &dlltable->pfn##pfnCall; \
		if (dlltable->pfn##pfnCall == NULL) \
			dlltable->pfn##pfnCall = pfnCall##_post; \
	} \
	else \
	{ \
		EngineAddrs[FM_##pfnCall] = &dlltable->pfn##pfnCall; \
		if (dlltable->pfn##pfnCall == NULL) \
			dlltable->pfn##pfnCall = pfnCall; \
	} 
#define NEWDLLHOOK(pfnCall) \
	if (post) \
	{ \
		if (newdlltable->pfn##pfnCall == NULL) \
			newdlltable->pfn##pfnCall = pfnCall##_post; \
	} \
	else \
	{ \
		if (newdlltable->pfn##pfnCall == NULL) \
			newdlltable->pfn##pfnCall = pfnCall; \
	}

#define PREPARE_VECTOR(vector_name) \
	cell vector_name##_cell[3] = {amx_ftoc(vector_name[0]), amx_ftoc(vector_name[1]), amx_ftoc(vector_name[2])}; \
	cell p_##vector_name = MF_PrepareCellArray(vector_name##_cell, 3) \

#define PREPARE_FLOAT(float_name) \
	cell c_##float_name = amx_ftoc(float_name);

#define BYREF_FLOAT(float_name) \
	float_name = amx_ctof(c_##float_name);


#define FM_ENG_HANDLE(pfnCall, pfnArgs) \
	register unsigned int i = 0; \
	clfm(); \
	int fmres = FMRES_IGNORED; \
	int lastFmRes = FMRES_IGNORED; \
	for (i=0; i<Engine[pfnCall].length(); i++) \
	{ \
		fmres = MF_ExecuteForward pfnArgs; \
		if (fmres >= lastFmRes) { \
			if (retType == FMV_STRING) \
				mlStringResult = mStringResult; \
			else if (retType == FMV_CELL) \
				mlCellResult = mCellResult; \
			else if (retType == FMV_FLOAT) \
				mlFloatResult = mFloatResult; \
			lastFmRes = fmres; \
		} \
	} \
	FM_MakeLuaForwardName(#pfnCall + 3, false); \
	fmres = FM_MakeLuaForward pfnArgs; \
	if (fmres >= lastFmRes) { \
		if (retType == FMV_STRING) \
			mlStringResult = mStringResult; \
		else if (retType == FMV_CELL) \
			mlCellResult = mCellResult; \
		else if (retType == FMV_FLOAT) \
			mlFloatResult = mFloatResult; \
		lastFmRes = fmres; \
	}
#define FM_ENG_HANDLE_POST(pfnCall, pfnArgs) \
	register unsigned int i = 0; \
	clfm(); \
	int fmres = FMRES_IGNORED; \
	int lastFmRes = FMRES_IGNORED; \
	for (i=0; i<EnginePost[pfnCall].length(); i++) \
	{ \
		fmres = MF_ExecuteForward pfnArgs; \
		if (fmres >= lastFmRes) { \
			if (retType == FMV_STRING) \
				mlStringResult = mStringResult; \
			else if (retType == FMV_CELL) \
				mlCellResult = mCellResult; \
			else if (retType == FMV_FLOAT) \
				mlFloatResult = mFloatResult; \
			lastFmRes = fmres; \
		} \
	} \
	FM_MakeLuaForwardName(#pfnCall + 3, true); \
	fmres = FM_MakeLuaForward pfnArgs; \
	if (fmres >= lastFmRes) { \
		if (retType == FMV_STRING) \
			mlStringResult = mStringResult; \
		else if (retType == FMV_CELL) \
			mlCellResult = mCellResult; \
		else if (retType == FMV_FLOAT) \
			mlFloatResult = mFloatResult; \
		lastFmRes = fmres; \
	} \
	origCellRet = 0; \
	origFloatRet = 0.0; \
	origStringRet = "";

#define FM_ENG_HANDLE_POST0(pfnCall, pfnArgs) \
	register unsigned int i = 0; \
	clfm(); \
	int fmres = FMRES_IGNORED; \
	int lastFmRes = FMRES_IGNORED; \
	for (i=0; i<EnginePost[pfnCall].length(); i++) \
	{ \
		fmres = MF_ExecuteForward pfnArgs; \
		if (fmres >= lastFmRes) { \
			if (retType == FMV_STRING) \
				mlStringResult = mStringResult; \
			else if (retType == FMV_CELL) \
				mlCellResult = mCellResult; \
			else if (retType == FMV_FLOAT) \
				mlFloatResult = mFloatResult; \
			lastFmRes = fmres; \
		} \
	} \
	FM_MakeLuaForwardName(#pfnCall + 3, true); \
	fmres = FM_MakeLuaForward pfnArgs; \
	if (fmres >= lastFmRes) { \
		if (retType == FMV_STRING) \
			mlStringResult = mStringResult; \
		else if (retType == FMV_CELL) \
			mlCellResult = mCellResult; \
		else if (retType == FMV_FLOAT) \
			mlFloatResult = mFloatResult; \
		lastFmRes = fmres; \
	} \
	origCellRet = 0; \
	origFloatRet = 0.0; \
	origStringRet = "";



#endif // FORWARDMACROS_H
