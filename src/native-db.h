/* Windows supplied SQLite, read-only connections, bounded pager cache, no mmap. */
typedef void PetDb;
typedef void PetStmt;
static HMODULE sqlModule;
static int (*sqlOpen)(const char*,PetDb**,int,const char*);
static int (*sqlClose)(PetDb*);
static int (*sqlPrepare)(PetDb*,const char*,int,PetStmt**,const char**);
static int (*sqlStep)(PetStmt*);
static int (*sqlFinalize)(PetStmt*);
static int (*sqlBindText)(PetStmt*,int,const char*,int,void(*)(void*));
static int (*sqlBindInt)(PetStmt*,int,LONGLONG);
static const unsigned char *(*sqlText)(PetStmt*,int);
static LONGLONG (*sqlInt)(PetStmt*,int);
static int (*sqlExec)(PetDb*,const char*,void*,void*,char**);
static int (*sqlBusy)(PetDb*,int);
static LONGLONG quotaCursor;
static int quotaDbOk;
static int quotaBootDone;
static LONGLONG threadCursor;
__declspec(dllimport) int WINAPI WideCharToMultiByte(UINT,DWORD,LPCWSTR,int,LPSTR,int,LPCSTR,LPBOOL);
static int dbInit(void) {
    WCHAR path[MAX_PATH];if(sqlModule)return 1;
    if(!GetSystemDirectoryW(path,MAX_PATH-20))return 0;wcscat(path,L"\\winsqlite3.dll");sqlModule=LoadLibraryW(path);if(!sqlModule)return 0;
#define SQL_FN(v,n) v=(void*)GetProcAddress(sqlModule,n);if(!v)goto fail
    SQL_FN(sqlOpen,"sqlite3_open_v2");SQL_FN(sqlClose,"sqlite3_close");SQL_FN(sqlPrepare,"sqlite3_prepare_v2");SQL_FN(sqlStep,"sqlite3_step");SQL_FN(sqlFinalize,"sqlite3_finalize");SQL_FN(sqlBindText,"sqlite3_bind_text");SQL_FN(sqlBindInt,"sqlite3_bind_int64");SQL_FN(sqlText,"sqlite3_column_text");SQL_FN(sqlInt,"sqlite3_column_int64");SQL_FN(sqlExec,"sqlite3_exec");SQL_FN(sqlBusy,"sqlite3_busy_timeout");
#undef SQL_FN
    return 1;
fail: FreeLibrary(sqlModule);sqlModule=NULL;return 0;
}
static void dbShutdown(void){if(sqlModule)FreeLibrary(sqlModule);sqlModule=NULL;quotaCursor=threadCursor=0;quotaDbOk=quotaBootDone=0;}
static PetDb *dbRead(const WCHAR *name) {
    WCHAR path[1100];char utf8[4400];PetDb *db=NULL;
    if(!dbInit())return NULL;_snwprintf(path,1100,L"%ls\\%ls",codexHome,name);
    if(!WideCharToMultiByte(CP_UTF8,0,path,-1,utf8,sizeof(utf8),NULL,NULL))return NULL;
    if(sqlOpen(utf8,&db,1,NULL)!=0){if(db)sqlClose(db);return NULL;}
    sqlBusy(db,30);
    if(sqlExec(db,"PRAGMA cache_size=-64; PRAGMA mmap_size=0; PRAGMA query_only=ON",NULL,NULL,NULL)!=0){sqlClose(db);return NULL;}
    return db;
}
static int discoverIndexedSession(const char *id){PetDb *db=dbRead(L"state_5.sqlite");PetStmt *s=NULL;WCHAR path[1024];int found=0;if(!db)return 0;
    if(sqlPrepare(db,"SELECT rollout_path FROM threads WHERE id=?",-1,&s,NULL)==0){sqlBindText(s,1,id,-1,NULL);if(sqlStep(s)==100){const char *text=(const char*)sqlText(s,0);if(text&&MultiByteToWideChar(CP_UTF8,0,text,-1,path,1024)){discoverRollout(path);found=1;}}}
    if(s)sqlFinalize(s);sqlClose(db);return found;
}
static void pollUpdatedThreads(void){PetDb *db=dbRead(L"state_5.sqlite");PetStmt *s=NULL;WCHAR path[1024];LONGLONG last;int count=0;if(!db)return;
    if(!threadCursor)threadCursor=nowSeconds()-7*86400;last=threadCursor;
    if(sqlPrepare(db,"SELECT rollout_path,updated_at FROM threads WHERE updated_at>=? ORDER BY updated_at LIMIT 256",-1,&s,NULL)==0){sqlBindInt(s,1,threadCursor);
        while(count++<256&&sqlStep(s)==100){const char *text=(const char*)sqlText(s,0);LONGLONG at=sqlInt(s,1);if(text&&MultiByteToWideChar(CP_UTF8,0,text,-1,path,1024))discoverRollout(path);if(at>last)last=at;}
        threadCursor=last>threadCursor?last-1:threadCursor;
    }
    if(s)sqlFinalize(s);sqlClose(db);
}
static int projectName(const char *id,WCHAR *out,int cap) {
    PetDb *db;PetStmt *s=NULL;int found=0;out[0]=0;if(legacyProjectName(id,out,cap))return 1;db=dbRead(L"state_5.sqlite");if(!db)return 0;
    if(sqlPrepare(db,"SELECT p.name FROM threads t JOIN projects p ON p.id=t.project_id WHERE t.id=?",-1,&s,NULL)==0){
        sqlBindText(s,1,id,-1,NULL);
        if(sqlStep(s)==100){const char *text=(const char*)sqlText(s,0);if(text&&text[0])found=MultiByteToWideChar(CP_UTF8,0,text,-1,out,cap)>0;}
    }
    if(s)sqlFinalize(s);sqlClose(db);return found;
}
