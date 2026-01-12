// vim: set ts=4 sw=4 tw=99 noet:
//
// AMX Mod X, based on AMX Mod by Aleksander Naszko ("OLO").
// Copyright (C) The AMX Mod X Development Team.
//
// This software is licensed under the GNU General Public License, version 3 or higher.
// Additional exceptions apply. For full license details, see LICENSE.txt or visit:
//     https://alliedmods.net/amxmodx-license

//
// MySQL Module
//

#include "amxxmodule.h"
#include "mysql2_header.h"
#include "threading.h"

using namespace SourceMod;

MysqlDriver g_Mysql;
MainThreader g_Threader;
ThreadWorker *g_pWorker = NULL;
extern DLL_FUNCTIONS *g_pFunctionTable;
IMutex *g_QueueLock = NULL;
CStack<MysqlThread *> g_ThreadQueue;
CStack<MysqlThread *> g_FreeThreads;
float g_lasttime = 0.0f;

void ShutdownThreading()
{
	if (g_pWorker)
	{
		// Flush all the remaining job fast!
		g_pWorker->SetMaxThreadsPerFrame(8192);
		g_pWorker->Stop(true);
		delete g_pWorker;
		g_pWorker = NULL;
	}

	g_QueueLock->Lock();
	while (!g_ThreadQueue.empty())
	{
		delete g_ThreadQueue.front();
		g_ThreadQueue.pop();
	}
	while (!g_FreeThreads.empty())
	{
		delete g_FreeThreads.front();
		g_FreeThreads.pop();
	}
	g_QueueLock->Unlock();
	g_QueueLock->DestroyThis();

	FreeHandleTable();
}

//public QueryHandler(state, Handle:query, error[], errnum, data[], size)
//native SQL_ThreadQuery(Handle:cn_tuple, const handler[], const query[], const data[]="", dataSize=0);
// static cell AMX_NATIVE_CALL SQL_ThreadQuery(AMX *amx, cell *params)
// {
// 	if (!g_pWorker)
// 	{
// 		MF_LogError(amx, AMX_ERR_NATIVE, "Thread worker was unable to start.");
// 		return 0;
// 	}

// 	SQL_Connection *cn = (SQL_Connection *)GetHandle(params[1], Handle_Connection);
// 	if (!cn)
// 	{
// 		MF_LogError(amx, AMX_ERR_NATIVE, "Invalid info tuple handle: %d", params[1]);
// 		return 0;
// 	}

// 	int len;
// 	const char *handler = MF_GetAmxString(amx, params[2], 0, &len);
// 	int fwd = MF_RegisterSPForwardByName(amx, handler, FP_CELL, FP_CELL, FP_STRING, FP_CELL, FP_ARRAY, FP_CELL, FP_CELL, FP_DONE);
// 	if (fwd < 1)
// 	{
// 		MF_LogError(amx, AMX_ERR_NATIVE, "Function not found: %s", handler);
// 		return 0;
// 	}

// 	MysqlThread *kmThread;
// 	g_QueueLock->Lock();
// 	if (g_FreeThreads.empty())
// 	{
// 		kmThread = new MysqlThread();
// 	} 
// 	else 
// 	{
// 		kmThread = g_FreeThreads.front();
// 		g_FreeThreads.pop();
// 	}
// 	g_QueueLock->Unlock();

// 	kmThread->SetInfo(cn->host, cn->user, cn->pass, cn->db, cn->port, cn->max_timeout);
// 	kmThread->SetForward(fwd);
// 	kmThread->SetQuery(MF_GetAmxString(amx, params[3], 1, &len));
// 	kmThread->SetCellData(MF_GetAmxAddr(amx, params[4]), (ucell)params[5]);
// 	kmThread->SetCharacterSet(cn->charset);

// 	g_pWorker->MakeThread(kmThread);

// 	return 1;
// }
// 辅助函数：将 C++ 的 SQL_Connection 指针封装为 Lua Userdata
static int Lua_CreateConnection(lua_State *L) {
    const char *host = luaL_checkstring(L, 1);
    const char *user = luaL_checkstring(L, 2);
    const char *pass = luaL_checkstring(L, 3);
    const char *db   = luaL_checkstring(L, 4);
    int port         = (int)luaL_optinteger(L, 5, 3306);

    if (port <= 0 || port > 65535) {
        return luaL_error(L, "Invalid port: %d", port);
    }

    // 1. 分配 Userdata 内存
    void *storage = lua_newuserdata(L, sizeof(SQL_Connection));
    
    // 2. 使用 Placement New 在这块内存上构造对象 (触发 ke::AString 构造函数)
    // 这一步非常重要，它初始化了内部的 AString 成员
    SQL_Connection *cn = new (storage) SQL_Connection();

    // 3. 赋值 (ke::AString 会自动深拷贝字符串内容，这样即便 Lua 栈清空也安全)
    cn->host = host;
    cn->user = user;
    cn->pass = pass;
    cn->db = db;
    cn->charset = "utf8";
    cn->port = port;
    cn->max_timeout = 30;

    // 4. 设置元表 (用于类型检查和垃圾回收)
    luaL_getmetatable(L, "SQL_Connection");
    lua_setmetatable(L, -2);

    return 1; // 此时栈顶是这个构造好的 Userdata
}
static int Lua_SQL_ThreadQuery(lua_State *L)
{
    // 1. 检查参数
    // 假设你已经将 SQL_Connection 包装成了 userdata
    SQL_Connection *cn = (SQL_Connection *)lua_touserdata(L, 1);
    if (!lua_isfunction(L, 2)) return luaL_argerror(L, 2, "Expected function callback");
    const char *query = luaL_checkstring(L, 3);

    // 2. 引用回调函数和附加数据 (Table/String/etc)
    lua_pushvalue(L, 2);
    int cbRef = luaL_ref(L, LUA_REGISTRYINDEX);
    
    int dataRef = -1;
    if (lua_gettop(L) >= 4) {
        lua_pushvalue(L, 4);
        dataRef = luaL_ref(L, LUA_REGISTRYINDEX);
    }

    // 3. 获取/创建线程对象 (沿用你原本的缓存池逻辑)
    MysqlThread *kmThread;
    g_QueueLock->Lock();
    if (g_FreeThreads.empty()) {
        kmThread = new MysqlThread();
		printf("Lua_SQL_ThreadQuery: New MysqlThread created\n");
    } else {
        kmThread = g_FreeThreads.front();
        g_FreeThreads.pop();
    }
    g_QueueLock->Unlock();

    // 4. 设置属性
    kmThread->SetInfo(cn->host.chars(), cn->user.chars(), cn->pass.chars(), cn->db.chars(), cn->port, cn->max_timeout);
    kmThread->SetQuery(query);
    kmThread->SetCharacterSet(cn->charset.chars());
	kmThread->SetLuaState(L); // 传入 Lua 状态机指针
    kmThread->SetLuaCallback(cbRef, dataRef); // 传入 Lua 引用

    // 5. 启动异步任务
    g_pWorker->MakeThread(kmThread);

    lua_pushboolean(L, 1);
    return 1;
}
// 1. 析构函数包装器
static int Lua_Connection_GC(lua_State *L) {
    // 检查并获取 userdata 指针
    SQL_Connection *cn = (SQL_Connection *)luaL_checkudata(L, 1, "SQL_Connection");
    if (cn) {
        // 手动执行析构函数（释放 ke::AString 的内部内存）
        cn->~SQL_Connection(); 
    }
    return 0;
}

// 2. 注册元表
void RegisterConnection(lua_State *L) {
    luaL_newmetatable(L, "SQL_Connection");
    
    // 绑定 __gc 键到析构包装器函数
    lua_pushcfunction(L, Lua_Connection_GC);
    lua_setfield(L, -2, "__gc"); 
    
    lua_pop(L, 1);
}
void LuaInit(lua_State *L)
{
	RegisterConnection(L);
	lua_register(L, "mysql_connect", Lua_CreateConnection);
	lua_register(L, "mysql_query", Lua_SQL_ThreadQuery);
}

MysqlThread::MysqlThread()
{
	m_fwd = 0;
	m_data = NULL;
	m_datalen = 0;
	m_maxdatalen = 0;
	// 初始化 Lua 引用
    m_luaCallbackRef = -1; 
    m_luaDataRef = -1;
    m_L = NULL;
}
void MysqlThread::SetLuaCallback(int cbRef, int dataRef)
{
    m_luaCallbackRef = cbRef;
    m_luaDataRef = dataRef;
}

MysqlThread::~MysqlThread()
{
	if (m_fwd)
	{
		MF_UnregisterSPForward(m_fwd);
		m_fwd = 0;
	}
	
	delete [] m_data;
	m_data = NULL;
}

void MysqlThread::SetCellData(cell data[], ucell len)
{
	if (len > m_maxdatalen)
	{
		delete [] m_data;
		m_data = new cell[len];
		m_maxdatalen = len;
	}
	if (len)
	{
		m_datalen = len;
		memcpy(m_data, data, len*sizeof(cell));
	}
}

void MysqlThread::SetForward(int forward)
{
	m_fwd = forward;
}

void MysqlThread::SetInfo(const char *host, const char *user, const char *pass, const char *db, int port, unsigned int max_timeout)
{
	m_host = host;
	m_user = user;
	m_pass = pass;
	m_db = db;
	m_max_timeout = max_timeout;
	m_port = port;
	m_qrInfo.queue_time = gpGlobals->time;
}

void MysqlThread::SetCharacterSet(const char *charset)
{
	m_charset = charset;
}

void MysqlThread::SetQuery(const char *query)
{
	m_query = query;
}

void MysqlThread::RunThread(IThreadHandle *pHandle)
{
	DatabaseInfo info;

	info.database = m_db.chars();
	info.pass = m_pass.chars();
	info.user = m_user.chars();
	info.host = m_host.chars();
	info.port = m_port;
	info.max_timeout = m_max_timeout;
	info.charset = m_charset.chars();

	float save_time = m_qrInfo.queue_time;

	memset(static_cast<void *>(&m_qrInfo), 0, sizeof(m_qrInfo));

	m_qrInfo.queue_time = save_time;

	IDatabase *pDatabase = g_Mysql.Connect2(&info, &m_qrInfo.amxinfo.info.errorcode, m_qrInfo.amxinfo.error, 254);
	IQuery *pQuery = NULL;
	if (!pDatabase)
	{
		m_qrInfo.connect_success = false;
		m_qrInfo.query_success = false;
	} 
	else 
	{
		m_qrInfo.connect_success = true;
		pQuery = pDatabase->PrepareQuery(m_query.chars());
		if (!pQuery->Execute2(&m_qrInfo.amxinfo.info, m_qrInfo.amxinfo.error, 254))
		{
			m_qrInfo.query_success = false;
		} 
		else 
		{
			m_qrInfo.query_success = true;
		}
	}

	if (m_qrInfo.query_success && m_qrInfo.amxinfo.info.rs)
	{
		m_atomicResult.CopyFrom(m_qrInfo.amxinfo.info.rs);
		m_qrInfo.amxinfo.info.rs = &m_atomicResult;
	}

	if (pQuery)
	{
		pQuery->FreeHandle();
		pQuery = NULL;
	} 

	m_qrInfo.amxinfo.pQuery = NULL;
	m_qrInfo.amxinfo.opt_ptr = new char[m_query.length() + 1];
	strcpy(m_qrInfo.amxinfo.opt_ptr, m_query.chars());

	if (pDatabase)
	{
		pDatabase->FreeHandle();
		pDatabase = NULL;
	}
}

void MysqlThread::Invalidate()
{
	m_atomicResult.FreeHandle();
	// 如果任务被取消，没进入 Execute，也需要在这里清理 Lua 引用
    if (m_L) {
        if (m_luaCallbackRef != -1) {
            luaL_unref(m_L, LUA_REGISTRYINDEX, m_luaCallbackRef);
            m_luaCallbackRef = -1;
        }
        if (m_luaDataRef != -1) {
            luaL_unref(m_L, LUA_REGISTRYINDEX, m_luaDataRef);
            m_luaDataRef = -1;
        }
    }
}

void MysqlThread::OnTerminate(IThreadHandle *pHandle, bool cancel)
{
	if (cancel)
	{
		Invalidate();
		g_QueueLock->Lock();
		g_FreeThreads.push(this);
		g_QueueLock->Unlock();
	} else {
		g_QueueLock->Lock();
		g_ThreadQueue.push(this);
		g_QueueLock->Unlock();
	}
}

void NullFunc(void *ptr, unsigned int num)
{
}

void PushAtomicResultToLua(lua_State *L, AtomicResult *res) {
    lua_newtable(L); // 创建结果数组
    
    unsigned int rows = res->RowCount();
    unsigned int fields = res->FieldCount();

    for (unsigned int i = 1; i <= rows; i++) {
        lua_pushinteger(L, i);
        lua_newtable(L); // 创建这一行的 table

        for (unsigned int f = 0; f < fields; f++) {
            const char* name = res->FieldNumToName(f);
            const char* val = res->GetString(f);
            
            lua_pushstring(L, name);
            lua_pushstring(L, val ? val : "");
            lua_settable(L, -3);
        }
        
        lua_settable(L, -3);
        res->NextRow();
    }
    res->Rewind(); // 重置指针供下次使用或清理
}

//public QueryHandler(state, Handle:query, error[], errnum, data[], size)
void MysqlThread::Execute()
{
	// 如果没有 Lua 回调，可以走原有的 Pawn 逻辑或直接返回
    if (m_luaCallbackRef == -1 || !m_L) 
        return;

    // 1. 获取回调函数
    lua_rawgeti(m_L, LUA_REGISTRYINDEX, m_luaCallbackRef);

    // 2. 准备参数 (state, results, error, data)
    int state = 0;
    if (!m_qrInfo.connect_success) state = -2;
    else if (!m_qrInfo.query_success) state = -1;
    
    lua_pushinteger(m_L, state); // 参数 1: state

    // 参数 2: results (这里将 AtomicResult 包装成 table 或 userdata)
    // 简单起见，这里假设你有一个转换函数。如果没有，先传 nil
    PushAtomicResultToLua(m_L, &m_atomicResult); 

    // 参数 3: error
    lua_pushstring(m_L, m_qrInfo.amxinfo.error);

    // 参数 4: data (用户传入的附加数据)
    if (m_luaDataRef != -1) {
        lua_rawgeti(m_L, LUA_REGISTRYINDEX, m_luaDataRef);
    } else {
        lua_pushnil(m_L);
    }

    // 3. 执行 Lua 回调 (4 个参数, 0 个返回值)
    if (lua_pcall(m_L, 4, 0, 0) != LUA_OK) {
        // 报错处理
        const char* err = lua_tostring(m_L, -1);
        printf("Lua SQL Callback Error: %s\n", err);
        lua_pop(m_L, 1);
    }

    // 4. 重要：释放引用，防止内存泄漏
    luaL_unref(m_L, LUA_REGISTRYINDEX, m_luaCallbackRef);
    if (m_luaDataRef != -1) {
        luaL_unref(m_L, LUA_REGISTRYINDEX, m_luaDataRef);
    }
    m_luaCallbackRef = -1;
    m_luaDataRef = -1;

    // 清理原有的 opt_ptr 逻辑
    if (m_qrInfo.amxinfo.opt_ptr) {
        delete [] m_qrInfo.amxinfo.opt_ptr;
        m_qrInfo.amxinfo.opt_ptr = NULL;
    }
}

/*****************
 * METAMOD STUFF *
 *****************/

void OnPluginsLoaded2(void)
{
	if (g_pWorker)
	{
		return;
	}

	if (!g_QueueLock)
	{
		g_QueueLock = g_Threader.MakeMutex();
	}

	g_pWorker = new ThreadWorker(&g_Threader, DEFAULT_THINK_TIME_MS);
	if (!g_pWorker->Start())
	{
		delete g_pWorker;
		g_pWorker = NULL;
	}
	g_pFunctionTable->pfnSpawn = NULL;

	g_lasttime = 0.0f;

	return;
}

void StartFrame2()
{
	if (g_pWorker && (g_lasttime < gpGlobals->time))
	{
		g_lasttime = gpGlobals->time + 0.025f;
		g_QueueLock->Lock();
		size_t remaining = g_ThreadQueue.size();
		if (remaining)
		{
			MysqlThread *kmThread;
			do 
			{
				kmThread = g_ThreadQueue.front();
				g_ThreadQueue.pop();
				g_QueueLock->Unlock();
				kmThread->Execute();
				kmThread->Invalidate();
				g_FreeThreads.push(kmThread);
				g_QueueLock->Lock();
			} while (!g_ThreadQueue.empty());
		}

		g_QueueLock->Unlock();
	}

	RETURN_META(MRES_IGNORED);
}

void OnPluginsUnloading()
{
	if (!g_pWorker)
	{
		return;
	}

	// Flush all the remaining job fast!
	g_pWorker->SetMaxThreadsPerFrame(8192);
	g_pWorker->Stop(false);
	delete g_pWorker;
	g_pWorker = NULL;

	g_QueueLock->Lock();
	size_t remaining = g_ThreadQueue.size();
	if (remaining)
	{
		MysqlThread *kmThread;
		do 
		{
			kmThread = g_ThreadQueue.front();
			g_ThreadQueue.pop();
			g_QueueLock->Unlock();
			kmThread->SetLuaState(nullptr);
			kmThread->Execute();
			kmThread->Invalidate();
			g_FreeThreads.push(kmThread);
			g_QueueLock->Lock();
		} while (!g_ThreadQueue.empty());
	}

	g_QueueLock->Unlock();
}

/***********************
 * ATOMIC RESULT STUFF *
 ***********************/

AtomicResult::AtomicResult()
{
	m_IsFree = true;
	m_CurRow = 1;
	m_RowCount = 0;
	m_Table = NULL;
	m_AllocSize = 0;
}

AtomicResult::~AtomicResult()
{
	if (!m_IsFree)
	{
		FreeHandle();
	}

	for (size_t i=0; i<=m_AllocSize; i++)
	{
		delete m_Table[i];
	}

	delete [] m_Table;

	m_Table = NULL;
	m_IsFree = true;
}

unsigned int AtomicResult::RowCount()
{
	return m_RowCount;
}

bool AtomicResult::IsNull(unsigned int columnId)
{
	return (GetString(columnId) == NULL);
}

unsigned int AtomicResult::FieldCount()
{
	return m_FieldCount;
}

bool AtomicResult::FieldNameToNum(const char *name, unsigned int *columnId)
{
	for (unsigned int i=0; i<m_FieldCount; i++)
	{
		assert(m_Table[i] != NULL);
		if (strcmp(m_Table[i]->chars(), name) == 0)
		{
			if (columnId)
			{
				*columnId = i;
			}
			return true;
		}
	}

	return false;
}

const char *AtomicResult::FieldNumToName(unsigned int num)
{
	if (num >= m_FieldCount)
		return NULL;

	assert(m_Table[num] != NULL);

	return m_Table[num]->chars();
}

double AtomicResult::GetDouble(unsigned int columnId)
{
	return atof(GetStringSafe(columnId));
}

float AtomicResult::GetFloat(unsigned int columnId)
{
	return atof(GetStringSafe(columnId));
}

int AtomicResult::GetInt(unsigned int columnId)
{
	return atoi(GetStringSafe(columnId));
}

const char *AtomicResult::GetRaw(unsigned int columnId, size_t *length)
{
	//we don't support this yet...
	*length = 0;
	return "";
}

const char *AtomicResult::GetStringSafe(unsigned int columnId)
{
	const char *str = GetString(columnId);
	
	return str ? str : "";
}

const char *AtomicResult::GetString(unsigned int columnId)
{
	if (columnId >= m_FieldCount)
		return NULL;

	size_t idx = (m_CurRow * m_FieldCount) + columnId;

	assert(m_Table[idx] != NULL);

	return m_Table[idx]->chars();
}

IResultRow *AtomicResult::GetRow()
{
	return static_cast<IResultRow *>(this);
}

bool AtomicResult::IsDone()
{
	if (m_CurRow > m_RowCount)
		return true;

	return false;
}

void AtomicResult::NextRow()
{
	m_CurRow++;
}

void AtomicResult::_InternalClear()
{
	if (m_IsFree)
		return;

	m_IsFree = true;
}

void AtomicResult::FreeHandle()
{
	_InternalClear();
}

void AtomicResult::CopyFrom(IResultSet *rs)
{
	if (!m_IsFree)
	{
		_InternalClear();
	}

	m_IsFree = false;

	m_FieldCount = rs->FieldCount();
	m_RowCount = rs->RowCount();
	m_CurRow = 1;

	size_t newTotal = (m_RowCount * m_FieldCount) + m_FieldCount;
	if (newTotal > m_AllocSize)
	{
		ke::AString **table = new ke::AString *[newTotal];
		memset(table, 0, newTotal * sizeof(ke::AString *));
		if (m_Table)
		{
			memcpy(table, m_Table, m_AllocSize * sizeof(ke::AString *));
			delete [] m_Table;
		}
		m_Table = table;
		m_AllocSize = newTotal;
	}

	for (unsigned int i=0; i<m_FieldCount; i++)
	{
		if (m_Table[i])
		{
			*m_Table[i] = rs->FieldNumToName(i);
		} else {
			const char* string = rs->FieldNumToName(i);
			m_Table[i] = new ke::AString(string ? string : "");
		}
	}

	IResultRow *row;
	unsigned int idx = m_FieldCount;
	while (!rs->IsDone())
	{
		row = rs->GetRow();
		for (unsigned int i=0; i<m_FieldCount; i++,idx++)
		{
			if (m_Table[idx])
			{
				*m_Table[idx] = row->GetString(i);
			} else {
				const char* string = row->GetString(i);
				m_Table[idx] = new ke::AString(string ? string : "");
			}
		}
		rs->NextRow();
	}
}

void AtomicResult::Rewind()
{
	m_CurRow = 1;
}

bool AtomicResult::NextResultSet()
{
	return false;
}

// AMX_NATIVE_INFO g_ThreadSqlNatives[] =
// {
// 	{"SQL_ThreadQuery",			SQL_ThreadQuery},
// 	{NULL,						NULL},
// };

