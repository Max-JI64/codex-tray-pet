#define UNICODE
#define _UNICODE
#define _WIN32_WINNT 0x0600
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
typedef UINT_PTR SOCKET;
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>
#include <math.h>
typedef struct { DWORD cbSize; HWND hWnd; UINT uID,uFlags,uCallbackMessage; HICON hIcon; WCHAR szTip[128]; DWORD dwState,dwStateMask; WCHAR szInfo[256]; UINT uTimeout; WCHAR szInfoTitle[64]; DWORD dwInfoFlags; GUID guidItem; HICON hBalloonIcon; } NOTIFYICONDATAW;
__declspec(dllimport) BOOL WINAPI Shell_NotifyIconW(DWORD,NOTIFYICONDATAW*);
__declspec(dllimport) HINSTANCE WINAPI ShellExecuteW(HWND,LPCWSTR,LPCWSTR,LPCWSTR,LPCWSTR,int);
#define NIF_MESSAGE 1
#define NIF_ICON 2
#define NIF_TIP 4
#define NIM_ADD 0
#define NIM_MODIFY 1
#define NIM_DELETE 2

/* Single native process. No CLR, WMI, JSON object tree, image library, or worker thread. */
#define MAX_CHATS 256
#define IO_CAP 8192
#define WM_TRAY (WM_APP+1)
#define ID_SCAN 1
#define ID_ANIM 2
#define CP_UTF8 65001
__declspec(dllimport) int WINAPI MultiByteToWideChar(UINT,DWORD,LPCSTR,int,LPWSTR,int);
enum { IDLE, WORK, DONE, QUESTION, ERROR_STATE, STOPPED, UNKNOWN };
typedef struct { DWORD size,usage,pid; ULONG_PTR heap; DWORD module,threads,parent; LONG priority; DWORD flags; WCHAR exe[MAX_PATH]; } PROCENTRY;
__declspec(dllimport) HANDLE WINAPI CreateToolhelp32Snapshot(DWORD,DWORD);
__declspec(dllimport) BOOL WINAPI Process32FirstW(HANDLE,PROCENTRY*);
__declspec(dllimport) BOOL WINAPI Process32NextW(HANDLE,PROCENTRY*);
typedef BOOL (WINAPI *ImageNameFn)(HANDLE,DWORD,LPWSTR,PDWORD);
typedef struct { DWORD pid,parent; } Candidate;
typedef struct {
    WCHAR path[1024]; LONGLONG offset; int allowed,mood,ready,unknown;
    char turn[128], questionId[128]; int asyncQuestion;
    char id[64]; WCHAR title[256]; LONGLONG startedAt,latestAt,quotaAt; int finished,quotaBlocked,quotaUnread;
} Chat;
typedef struct {
    char type[48],ev[48],turn[128],name[128],id[128],role[32],origin[80],parent[128];
    char chatId[64],title[1024],timestamp[40];
    int sourceObject,willRetry;char errorCode[96],errorMessage[1024],toolOutput[4096];
} Record;
static HWND window;
static HANDLE singleton,stopEvent;
static DWORD desktopPid,sessionId,suppressedPid;
static ImageNameFn imageName;
static WCHAR folder[1024],sessions[1024];
static Chat *chats;
static int chatCount,overflow,mood=IDLE,manual=-1,animation=1,phase,iconsAdded;
static HICON icons[7][2];
static NOTIFYICONDATAW tray;
static UINT taskbarCreated;
static char *lineBuf;
static DWORD scanTick;
static int failure;
static unsigned long recordsRead;
static LONGLONG desktopStartedAt;
#include "language.h"
static WCHAR codexHome[1024];
static HWND panel;
static HFONT panelFont,headerFont;
static int settingsOpen,panelRows[MAX_CHATS],panelCount,panelOffset,panelPage=9,uiScale=100;
static char unreadIds[MAX_CHATS][64];
static int unreadCount,readStateOk;
static FILETIME indexStamp;
static int panelRowHeight=22,panelHeaderHeight=30,panelHover=-1;
static char lastNavigationId[64];
static INT_PTR lastNavigationResult;
static HANDLE navigationHelper;
static HICON extraIcons[2][4],badgeIcons[4];
static int badgeKey=-1,badgeBaseKey=-1,badgeFrames,finishedUnread,longWorkCount,motion,manualMotion=-1;
static int badgeStyle=1; /* Red is the default; preferences are loaded once at startup. */
static int customEnabled;
static int webPort;
static unsigned animationFrame;static LONGLONG completionPulseUntil,quotaLogAt,quotaAckAt;
static FILETIME unreadStateStamp,unreadAuthStamp;
static int quotaStateDirty;
static void updatePanelData(void);
static void repaintPanel(void);
static void togglePanel(void);
static int check(int condition,const char *name,FILE *file);
static void pollNavigation(void);
static const char *member(const char *p,const char *wanted);
static int quotaCode(const char *code);
static void extractError(const char *p,Record *r);
static int quotaMessage(const char *message);
static int quotaToolFailure(const char *message);
static void saveQuotaState(void);
static void loadQuotaState(void);
static void discoverSessionId(const char *id);
static void syncQuotaReadState(void);
static int sessionUrl(const char *id,WCHAR out[96]);
static void syncUnread(void);
static int isUnread(const char *id);
static int activeChat(const Chat *c);
static int latestChat(int i);
static void updateMotion(void);
static void pollQuotaLog(void);
static void discoverUnreadChats(void);
static void dbShutdown(void);
static int legacyProjectName(const char *id,WCHAR *out,int cap);
static void loadCustomFrames(void);
static void freeCustomFrames(void);
static void browserSettingsStop(void);
static void browserSettingsPoll(void);
static void openBrowserSettings(int iconSection);
static void applyCommand(int choice);
static void detectionSnapshot(void);
static LONGLONG nowSeconds(void) { FILETIME ft; ULARGE_INTEGER n;GetSystemTimeAsFileTime(&ft);n.LowPart=ft.dwLowDateTime;n.HighPart=ft.dwHighDateTime;return (n.QuadPart-116444736000000000LL)/10000000; }
static LONGLONG isoSeconds(const char *text) { SYSTEMTIME s={0};FILETIME ft;ULARGE_INTEGER n;unsigned y,mo,d,h,mi,se;if(sscanf(text,"%u-%u-%uT%u:%u:%u",&y,&mo,&d,&h,&mi,&se)!=6)return 0;s.wYear=y;s.wMonth=mo;s.wDay=d;s.wHour=h;s.wMinute=mi;s.wSecond=se;if(!SystemTimeToFileTime(&s,&ft))return 0;n.LowPart=ft.dwLowDateTime;n.HighPart=ft.dwHighDateTime;return(n.QuadPart-116444736000000000LL)/10000000;}

/* JSON reader copies only relevant direct fields. Nested content is skipped, never materialized. */
static const char *ws(const char *p) { while(*p && (unsigned char)*p<=32) p++; return p; }
static const char *stringValue(const char *p,char *out,int cap) {
    int n=0; if(*p!='"') return NULL; p++;
    while(*p && *p!='"') {
        char c=*p++;
        if(c=='\\') {
            if(!*p) return NULL; c=*p++;
            if(c=='u') {
                unsigned value=0;int j;char bytes[4];int count;
                for(j=0;j<4;j++){char h=*p++;if(h>='0'&&h<='9')value=value*16+h-'0';else if(h>='a'&&h<='f')value=value*16+h-'a'+10;else if(h>='A'&&h<='F')value=value*16+h-'A'+10;else return NULL;}
                if(value>=0xd800&&value<=0xdbff){unsigned low=0;if(p[0]!='\\'||p[1]!='u')return NULL;p+=2;for(j=0;j<4;j++){char h=*p++;if(h>='0'&&h<='9')low=low*16+h-'0';else if(h>='a'&&h<='f')low=low*16+h-'a'+10;else if(h>='A'&&h<='F')low=low*16+h-'A'+10;else return NULL;}if(low<0xdc00||low>0xdfff)return NULL;value=0x10000+((value-0xd800)<<10)+low-0xdc00;}
                if(value<128){bytes[0]=value;count=1;}else if(value<2048){bytes[0]=0xc0|(value>>6);bytes[1]=0x80|(value&63);count=2;}else if(value<65536){bytes[0]=0xe0|(value>>12);bytes[1]=0x80|((value>>6)&63);bytes[2]=0x80|(value&63);count=3;}else{bytes[0]=0xf0|(value>>18);bytes[1]=0x80|((value>>12)&63);bytes[2]=0x80|((value>>6)&63);bytes[3]=0x80|(value&63);count=4;}
                if(out&&n+count<cap){for(j=0;j<count;j++)out[n++]=bytes[j];}continue;
            }
            if(c=='n')c='\n';else if(c=='r')c='\r';else if(c=='t')c='\t';else if(c=='b')c='\b';else if(c=='f')c='\f';
        }
        if(out && n<cap-1) out[n++]=c;
    }
    if(*p!='"') return NULL;
    if(out) out[n]=0;
    return p+1;
}
static const char *skipValue(const char *p,int depth) {
    char close; p=ws(p); if(depth>64 || !*p) return NULL;
    if(*p=='"') return stringValue(p,NULL,0);
    if(*p=='{' || *p=='[') {
        close=*p=='{' ? '}' : ']'; p=ws(p+1);
        while(*p && *p!=close) {
            if(close=='}') { p=stringValue(p,NULL,0); if(!p) return NULL; p=ws(p); if(*p++!=':') return NULL; }
            p=skipValue(p,depth+1); if(!p) return NULL; p=ws(p);
            if(*p==',') p=ws(p+1); else if(*p!=close) return NULL;
        }
        return *p==close ? p+1 : NULL;
    }
    while(*p && *p!=',' && *p!=']' && *p!='}' && (unsigned char)*p>32) p++;
    return p;
}
static const char *payload(const char *p,Record *r) {
    char key[64],*target; int cap;
    p=ws(p); if(*p!='{') return NULL; p=ws(p+1);
    while(*p && *p!='}') {
        p=stringValue(p,key,sizeof(key)); if(!p) return NULL; p=ws(p); if(*p++!=':') return NULL; p=ws(p);
        target=NULL; cap=0;
#define FIELD(k,f) if(strcmp(key,k)==0) { target=r->f; cap=sizeof(r->f); }
        FIELD("type",ev) else FIELD("turn_id",turn) else FIELD("name",name) else FIELD("call_id",id)
        else FIELD("role",role) else FIELD("originator",origin) else FIELD("parent_thread_id",parent) else FIELD("id",chatId)
        else FIELD("codexErrorInfo",errorCode) else FIELD("codex_error_info",errorCode) else FIELD("errorInfo",errorCode) else FIELD("code",errorCode) else FIELD("message",errorMessage) else FIELD("output",toolOutput)
#undef FIELD
        if(strcmp(key,"source")==0 && *p=='{') r->sourceObject=1;
        if(strcmp(key,"willRetry")==0&&strncmp(p,"true",4)==0)r->willRetry=1;
        if(strcmp(key,"error")==0&&*p=='{')extractError(p,r);
        if(target && *p=='"') p=stringValue(p,target,cap); else p=skipValue(p,0);
        if(!p) return NULL; p=ws(p); if(*p==',') p=ws(p+1); else if(*p!='}') return NULL;
    }
    return *p=='}' ? p+1 : NULL;
}
static int parse(const char *p,Record *r) {
    char key[64]; memset(r,0,sizeof(*r)); p=ws(p); if(*p!='{') return 0; p=ws(p+1);
    while(*p && *p!='}') {
        p=stringValue(p,key,sizeof(key)); if(!p) return 0; p=ws(p); if(*p++!=':') return 0; p=ws(p);
        if(strcmp(key,"type")==0) p=stringValue(p,r->type,sizeof(r->type));
        else if(strcmp(key,"timestamp")==0) p=stringValue(p,r->timestamp,sizeof(r->timestamp));
        else if(strcmp(key,"id")==0) p=stringValue(p,r->chatId,sizeof(r->chatId));
        else if(strcmp(key,"thread_name")==0) p=stringValue(p,r->title,sizeof(r->title));
        else if(strcmp(key,"payload")==0) p=payload(p,r); else p=skipValue(p,0);
        if(!p) return 0; p=ws(p); if(*p==',') p=ws(p+1); else if(*p!='}') return 0;
    }
    return *p=='}';
}
#include "stream-record.h"
#define LINE_CAP ((int)sizeof(StreamRecord))
static void fold(Chat *c,Record *r,int boot) {
    if(strcmp(r->type,"session_meta")==0) {
        c->allowed=strcmp(r->origin,"Codex Desktop")==0 && !r->parent[0] && !r->sourceObject;
        strncpy(c->id,r->chatId,63); return;
    }
    if(!c->allowed) return;
    if(r->timestamp[0]) {LONGLONG at=isoSeconds(r->timestamp);if(at>c->latestAt)c->latestAt=at;}
    if(strcmp(r->type,"event_msg")==0) {
        if(strcmp(r->ev,"task_started")==0) {
            strncpy(c->turn,r->turn,127); c->mood=WORK; c->ready=0; c->unknown=0; c->questionId[0]=0;
            c->startedAt=isoSeconds(r->timestamp);
            if(c->quotaBlocked)quotaStateDirty=1;c->finished=c->quotaBlocked=c->quotaUnread=0;c->quotaAt=0;
        } else if(!c->turn[0] || !r->turn[0] || strcmp(c->turn,r->turn)==0) {
            if(strcmp(r->ev,"task_complete")==0 || strcmp(r->ev,"turn_aborted")==0 || strcmp(r->ev,"task_failed")==0) {
                if(!strcmp(r->ev,"task_complete")&&!quotaCode(r->errorCode)&&c->quotaBlocked){c->quotaBlocked=c->quotaUnread=0;c->quotaAt=0;quotaStateDirty=1;}
                c->quotaBlocked|=quotaCode(r->errorCode);
                if(c->quotaBlocked){c->quotaAt=c->latestAt?c->latestAt:nowSeconds();c->quotaUnread=isUnread(c->id);quotaStateDirty=1;}
                c->finished=strcmp(r->ev,"task_complete")==0&&!c->quotaBlocked;
                c->mood=c->quotaBlocked?STOPPED:strcmp(r->ev,"task_complete")==0 ? (boot ? IDLE : DONE) : strcmp(r->ev,"turn_aborted")==0 ? STOPPED : ERROR_STATE;
                c->ready=c->mood==DONE; c->turn[0]=c->questionId[0]=0; c->unknown=0;
                if(!boot&&c->finished)completionPulseUntil=nowSeconds()+12;
            } else if(strcmp(r->ev,"error")==0&&!r->willRetry) {
                c->quotaBlocked=quotaCode(r->errorCode)||quotaMessage(r->errorMessage);
                if(c->quotaBlocked){c->quotaAt=c->latestAt?c->latestAt:nowSeconds();c->quotaUnread=isUnread(c->id);quotaStateDirty=1;}
                c->mood=c->quotaBlocked?STOPPED:ERROR_STATE;c->finished=c->ready=0;c->questionId[0]=0;
            }
        }
    } else if(strcmp(r->type,"response_item")==0) {
        if((!strcmp(r->ev,"function_call_output")||!strcmp(r->ev,"custom_tool_call_output"))&&quotaToolFailure(r->toolOutput)){
            c->quotaBlocked=1;c->quotaAt=c->latestAt?c->latestAt:nowSeconds();c->quotaUnread=isUnread(c->id);c->mood=STOPPED;c->finished=c->ready=0;quotaStateDirty=1;return;
        }
        if(strcmp(r->ev,"message")==0 && strcmp(r->role,"user")==0 && c->questionId[0]) { c->questionId[0]=0; c->mood=WORK; }
        if((strcmp(r->ev,"function_call")==0 || strcmp(r->ev,"custom_tool_call")==0) && strstr(r->name,"request_user_input")) {
            strncpy(c->questionId,r->id,127); c->asyncQuestion=strstr(r->name,"async")!=NULL; c->mood=QUESTION;
        }
        if((strcmp(r->ev,"function_call_output")==0 || strcmp(r->ev,"custom_tool_call_output")==0) &&
           c->questionId[0] && !c->asyncQuestion && strcmp(c->questionId,r->id)==0) { c->questionId[0]=0; c->mood=WORK; }
    }
}
static int aggregate(Chat *list,int count,int capExceeded) {
    int i,w=0,d=0,s=0,u=capExceeded,e=0,q=0;
    for(i=0;i<count;i++) if(list[i].allowed) {
        if(list==chats&&!latestChat(i))continue;
        if(desktopStartedAt&&(list[i].mood==WORK||list[i].mood==QUESTION)&&!activeChat(&list[i]))continue;
        int m=list[i].mood; q|=m==QUESTION; e|=m==ERROR_STATE; w|=m==WORK; d|=list[i].ready; s|=m==STOPPED; u|=m==UNKNOWN || list[i].unknown;
        if(list==chats)d|=list[i].finished&&isUnread(list[i].id)&&!activeChat(&list[i]);
    }
    return q ? QUESTION : e ? ERROR_STATE : w ? WORK : u ? UNKNOWN : d ? DONE : s ? STOPPED : IDLE;
}
static int readChat(Chat *c,int boot) {
    HANDLE file; LARGE_INTEGER size,pos; char buffer[IO_CAP]; DWORD got;StreamRecord *record=(StreamRecord*)lineBuf;
    LONGLONG committed,at;
    file=CreateFileW(c->path,GENERIC_READ,FILE_SHARE_READ|FILE_SHARE_WRITE|FILE_SHARE_DELETE,NULL,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,NULL);
    if(file==INVALID_HANDLE_VALUE) { if(GetLastError()!=ERROR_FILE_NOT_FOUND) c->unknown=1; return 0; }
    if(!GetFileSizeEx(file,&size)) { CloseHandle(file); c->unknown=1; return 0; }
    if(size.QuadPart<c->offset) { c->offset=0; c->mood=UNKNOWN; c->questionId[0]=0; }
    if(c->offset==size.QuadPart) { CloseHandle(file); return 1; }
    pos.QuadPart=c->offset;
    if(!SetFilePointerEx(file,pos,NULL,FILE_BEGIN)) { CloseHandle(file); c->unknown=1; return 0; }
    at=committed=c->offset;memset(record,0,sizeof(*record));
    while(ReadFile(file,buffer,sizeof(buffer),&got,NULL) && got) {
        DWORD j;
        for(j=0;j<got;j++) {
            char b=buffer[j]; at++;
            if(b=='\n') {
                if(streamFinish(record)) {fold(c,&record->record,boot);recordsRead++;}else c->unknown=1;
                committed=at;memset(record,0,sizeof(*record));
            } else streamFeed(record,(unsigned char)b);
        }
    }
    c->offset=committed; CloseHandle(file); return 1;
}
static void readChatMetadata(Chat *c){HANDLE file;char buffer[IO_CAP];DWORD got,j;StreamRecord *record=(StreamRecord*)lineBuf;
    file=CreateFileW(c->path,GENERIC_READ,FILE_SHARE_READ|FILE_SHARE_WRITE|FILE_SHARE_DELETE,NULL,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,NULL);if(file==INVALID_HANDLE_VALUE)return;memset(record,0,sizeof(*record));
    while(ReadFile(file,buffer,sizeof(buffer),&got,NULL)&&got){for(j=0;j<got;j++)if(buffer[j]=='\n'){if(streamFinish(record))fold(c,&record->record,1);CloseHandle(file);return;}else streamFeed(record,(unsigned char)buffer[j]);}CloseHandle(file);
}
static void freeChats(void) { dbShutdown();if(chats) VirtualFree(chats,0,MEM_RELEASE); if(lineBuf) VirtualFree(lineBuf,0,MEM_RELEASE); chats=NULL; lineBuf=NULL; chatCount=overflow=0; unreadCount=panelCount=0; finishedUnread=longWorkCount=motion=0;completionPulseUntil=quotaLogAt=quotaAckAt=0;manualMotion=-1;readStateOk=0;memset(&indexStamp,0,sizeof(indexStamp));memset(&unreadStateStamp,0,sizeof(unreadStateStamp));memset(&unreadAuthStamp,0,sizeof(unreadAuthStamp)); }
static int ensureChats(void) {
    if(!chats) chats=VirtualAlloc(NULL,sizeof(Chat)*MAX_CHATS,MEM_RESERVE|MEM_COMMIT,PAGE_READWRITE);
    if(!lineBuf) lineBuf=VirtualAlloc(NULL,LINE_CAP,MEM_RESERVE|MEM_COMMIT,PAGE_READWRITE);
    return chats && lineBuf;
}
static void discover(void) {
    FILETIME ft; ULARGE_INTEGER time; int day;
    GetSystemTimeAsFileTime(&ft); time.LowPart=ft.dwLowDateTime; time.HighPart=ft.dwHighDateTime;
    for(day=-1;day<7;day++) {
        ULARGE_INTEGER then=time; SYSTEMTIME date; WCHAR dir[1024],pattern[1100]; WIN32_FIND_DATAW data; HANDLE find;
        then.QuadPart-=((LONGLONG)day)*864000000000LL; ft.dwLowDateTime=then.LowPart; ft.dwHighDateTime=then.HighPart; FileTimeToSystemTime(&ft,&date);
        _snwprintf(dir,1024,L"%ls\\%04u\\%02u\\%02u",sessions,date.wYear,date.wMonth,date.wDay);
        _snwprintf(pattern,1100,L"%ls\\rollout-*.jsonl",dir); find=FindFirstFileW(pattern,&data);
        if(find==INVALID_HANDLE_VALUE) continue;
        do {
            WCHAR path[1024]; int i; Chat *c;
            if(_snwprintf(path,1024,L"%ls\\%ls",dir,data.cFileName)<0) { overflow=1; continue; }
            for(i=0;i<chatCount;i++) if(wcscmp(chats[i].path,path)==0) break;
            if(i<chatCount) continue;
            if(chatCount==MAX_CHATS) { overflow=1; continue; }
            c=&chats[chatCount++]; memset(c,0,sizeof(*c)); wcscpy(c->path,path); c->mood=IDLE;
            /* Read metadata before seeking tail, so child/guardian agents are excluded. */
            readChatMetadata(c);
            if(c->allowed) readChat(c,1);
        } while(FindNextFileW(find,&data));
        FindClose(find);
    }
}
static DWORD probeDesktop(int *ok) {
    HANDLE snap; PROCENTRY pe; Candidate candidates[128]; int n=0,i,j; DWORD current=0;
    *ok=0; snap=CreateToolhelp32Snapshot(2,0); if(snap==INVALID_HANDLE_VALUE) return 0;
    memset(&pe,0,sizeof(pe)); pe.size=sizeof(pe);
    if(Process32FirstW(snap,&pe)) do {
        if(_wcsicmp(pe.exe,L"ChatGPT.exe")==0) {
            DWORD sess=0,len=1024; WCHAR path[1024]; HANDLE p;
            if(!ProcessIdToSessionId(pe.pid,&sess) || sess!=sessionId) continue;
            p=OpenProcess(0x1000,FALSE,pe.pid);
            if(!p) { *ok=-1; continue; }
            if(imageName(p,0,path,&len) && wcsstr(path,L"\\WindowsApps\\OpenAI.Codex_") &&
               len>16 && _wcsicmp(path+len-16,L"\\app\\ChatGPT.exe")==0 && n<128) { candidates[n].pid=pe.pid; candidates[n++].parent=pe.parent; }
            CloseHandle(p);
        }
    } while(Process32NextW(snap,&pe));
    CloseHandle(snap);
    for(i=0;i<n;i++) { for(j=0;j<n;j++) if(candidates[j].pid==candidates[i].parent) break; if(j==n) { current=candidates[i].pid; break; } }
    if(*ok!=-1 || current) *ok=1;
    return current;
}
static void pixel(DWORD *p,int x,int y,DWORD color) { if(x>=0 && x<32 && y>=0 && y<32) p[y*32+x]=color; }
static HICON makeIcon(int m,int f) {
    DWORD colors[]={0xff9ba8b8,0xff49aaff,0xff45d896,0xffffbe41,0xffff6474,0xffb58bee,0xff828282};
    DWORD ink=0xff161f2d,col=colors[m]; int x,y,bob=(m==WORK && f)?2:0; BITMAPINFO bi; DWORD *pixels; HBITMAP color,mask; ICONINFO ii; HICON icon;
    if(f && (m==WORK || m==QUESTION || m==ERROR_STATE)) col=0xff000000|(((col>>16&255)+255)/2<<16)|(((col>>8&255)+255)/2<<8)|((col&255)+255)/2;
    memset(&bi,0,sizeof(bi)); bi.bmiHeader.biSize=sizeof(BITMAPINFOHEADER); bi.bmiHeader.biWidth=32; bi.bmiHeader.biHeight=-32; bi.bmiHeader.biPlanes=1; bi.bmiHeader.biBitCount=32;
    color=CreateDIBSection(NULL,&bi,DIB_RGB_COLORS,(void**)&pixels,NULL,0); if(!color) return NULL; memset(pixels,0,4096);
    for(y=0;y<32;y++) for(x=0;x<32;x++) {
        int yy=y-bob;
        if(((x-16)*(x-16)*100+(yy-19)*(yy-19)*169<=16900 && yy>=9) ||
           (yy>=3 && yy<=14 && ((x>=4 && x<=4+(yy-3)) || (x<=27 && x>=27-(yy-3))))) pixel(pixels,x,y,col);
    }
    for(x=0;x<4;x++) {
        int ey=(m==DONE)?16+(x==0||x==3):17;
        pixel(pixels,9+x,ey+bob,ink); pixel(pixels,19+x,ey+bob,ink);
        if(m==WORK || m==QUESTION || m==ERROR_STATE) { pixel(pixels,10,18+bob,ink); pixel(pixels,20,18+bob,ink); }
    }
    pixel(pixels,14,22+bob,ink); pixel(pixels,15,23+bob,ink); pixel(pixels,16,23+bob,ink); pixel(pixels,17,22+bob,ink);
    if(m==QUESTION || m==ERROR_STATE || m==STOPPED || m==UNKNOWN) {
        for(y=21;y<32;y++) for(x=21;x<32;x++) if((x-26)*(x-26)+(y-26)*(y-26)<31) pixel(pixels,x,y,ink);
        if(m==QUESTION) { pixel(pixels,25,23,0xffffffff); pixel(pixels,26,23,0xffffffff); pixel(pixels,27,24,0xffffffff); pixel(pixels,26,25,0xffffffff); pixel(pixels,25,26,0xffffffff); pixel(pixels,25,29,0xffffffff); }
        else if(m==STOPPED) { for(y=24;y<29;y++) {pixel(pixels,24,y,0xffffffff);pixel(pixels,27,y,0xffffffff);} }
        else {for(y=23;y<27;y++) pixel(pixels,26,y,0xffffffff);pixel(pixels,26,29,0xffffffff);}
    }
    { BYTE maskBits[128]={0}; mask=CreateBitmap(32,32,1,1,maskBits); }
    memset(&ii,0,sizeof(ii)); ii.fIcon=TRUE; ii.hbmColor=color; ii.hbmMask=mask;
    icon=CreateIconIndirect(&ii); DeleteObject(color); DeleteObject(mask); return icon;
}
#include "native-db.h"
#include "pet-motion.h"
static void destroyIcons(void) { int m,f; for(m=0;m<7;m++) for(f=0;f<2;f++) {if(icons[m][f]) DestroyIcon(icons[m][f]); icons[m][f]=NULL;}for(m=0;m<2;m++)for(f=0;f<4;f++){if(extraIcons[m][f])DestroyIcon(extraIcons[m][f]);extraIcons[m][f]=NULL;}clearBadgeIcons();freeCustomFrames(); }
static void report(void) {
    WCHAR path[1100]; FILE *file; int allowed=0,i; for(i=0;i<chatCount;i++) allowed+=chats[i].allowed!=0;
    _snwprintf(path,1100,L"%ls\\lite-status.json",folder); file=_wfopen(path,L"wb"); if(!file) return;
    fprintf(file,"{\"pid\":%lu,\"desktopPid\":%lu,\"tray\":%s,\"state\":%d,\"chats\":%d,\"failure\":%d,\"recordBufferBytes\":%d,\"iconFrames\":%d,\"recordsRead\":%lu,\"tick\":%lu,\"navigationId\":\"%s\",\"navigationResult\":%lld,\"navigationPending\":%s,\"motion\":%d,\"finishedUnread\":%d,\"longWorkCount\":%d,\"badgeFrames\":%d,\"quotaDbOk\":%s,\"quotaLogAt\":%lld,\"badgeColor\":%d,\"customIcon\":%d,\"customFrames\":%d,\"customCacheFrames\":%d,\"settingsPort\":%d,\"language\":%d}",GetCurrentProcessId(),desktopPid,iconsAdded?"true":"false",motion==1?STOPPED:manual>=0?manual:mood,allowed,failure,LINE_CAP,iconsAdded?22:0,recordsRead,GetTickCount(),lastNavigationId,(LONGLONG)lastNavigationResult,navigationHelper?"true":"false",motion,finishedUnread,longWorkCount,badgeFrames,quotaDbOk?"true":"false",quotaLogAt,badgeStyle,customEnabled,customCount,customCacheCount,webPort,uiLanguage); fclose(file);
}
static void refresh(int add) {
    int shown=manual>=0?manual:mood;
    tray.uFlags=NIF_ICON|NIF_TIP|NIF_MESSAGE; tray.hIcon=displayIcon(shown);
    _snwprintf(tray.szTip,128,tr(TXT_tooltip),motion==1?tr(TXT_quota):stateName(shown),manual>=0||manualMotion>=0?tr(TXT_previewSuffix):L"",finishedUnread,longWorkCount?tr(TXT_longSuffix):L"");
    if(add) iconsAdded=Shell_NotifyIconW(NIM_ADD,&tray)!=0;
    else if(iconsAdded && !Shell_NotifyIconW(NIM_MODIFY,&tray)) iconsAdded=0;
}
static void hidePet(void) {
    browserSettingsStop();
    if(panel) DestroyWindow(panel);
    KillTimer(window,ID_ANIM); if(iconsAdded) Shell_NotifyIconW(NIM_DELETE,&tray); iconsAdded=0;
    destroyIcons(); freeChats(); manual=-1; mood=IDLE;
}
static void showPet(void) {
    int m,f; if(!ensureChats()) { failure=ERROR_NOT_ENOUGH_MEMORY; return; }
    for(m=0;m<7;m++) for(f=0;f<2;f++) icons[m][f]=makeIcon(m,f);
    for(m=0;m<2;m++)for(f=0;f<4;f++)extraIcons[m][f]=makeMotionIcon(m,f);
    loadCustomFrames();
    discover();syncUnread();discoverUnreadChats();loadQuotaState();updateMotion();mood=aggregate(chats,chatCount,overflow); refresh(1); SetTimer(window,ID_ANIM,customEnabled&&customCount>1?150:300,NULL);
}
static void scan(void) {
    DWORD next; int ok,i; if(WaitForSingleObject(stopEvent,0)==WAIT_OBJECT_0) {DestroyWindow(window);return;}
    next=probeDesktop(&ok); if(ok!=1) {failure=GetLastError()?GetLastError():5;report();return;}
    failure=0;
    if(next!=desktopPid) {
        HANDLE process; FILETIME created,exit,kernel,user;ULARGE_INTEGER start;
        hidePet();desktopPid=next;suppressedPid=0;desktopStartedAt=0;
        if(desktopPid){process=OpenProcess(0x1000,FALSE,desktopPid);if(process){if(GetProcessTimes(process,&created,&exit,&kernel,&user)){start.LowPart=created.dwLowDateTime;start.HighPart=created.dwHighDateTime;desktopStartedAt=(start.QuadPart-116444736000000000LL)/10000000;}CloseHandle(process);}showPet();}report();
    }
    if(!desktopPid || suppressedPid==desktopPid) return;
    if(!iconsAdded) { if(!icons[0][0]) showPet(); else refresh(1); }
    if(!ensureChats()) return;
    if(++scanTick%3==0) discover();
    for(i=0;i<chatCount;i++) if(chats[i].allowed && chats[i].path[0]) readChat(&chats[i],0);
    syncUnread();discoverUnreadChats();syncQuotaReadState();pollQuotaLog();saveQuotaState();updateMotion();
    { int old=mood; mood=aggregate(chats,chatCount,overflow); if(mood!=old) refresh(0); }
    if(panel && IsWindowVisible(panel)) {updatePanelData();repaintPanel();}
    if(scanTick%15==0) report();
    refresh(0);
}
static void menu(void) {
    HMENU root=CreatePopupMenu(),preview=CreatePopupMenu(),colors=CreatePopupMenu(),languages=CreatePopupMenu(); POINT pt; int i,choice; WCHAR title[80];
    const int colorKeys[]={TXT_charcoal,TXT_red,TXT_blue,TXT_green,TXT_purple,TXT_white};
    for(i=0;i<5;i++)AppendMenuW(languages,MF_STRING|(i==uiLanguage?MF_CHECKED:0),100+i,languageNames[i]);AppendMenuW(root,MF_POPUP,(UINT_PTR)languages,tr(TXT_language));AppendMenuW(root,MF_SEPARATOR,0,NULL);
    _snwprintf(title,80,tr(TXT_status),motion==1?tr(TXT_quota):stateName(manual>=0?manual:mood)); AppendMenuW(root,MF_STRING|MF_GRAYED,0,title);
    AppendMenuW(root,MF_STRING,10,tr(TXT_auto)); AppendMenuW(root,MF_STRING,11,tr(TXT_ack));
    for(i=0;i<7;i++) AppendMenuW(preview,MF_STRING,30+i,stateName(i)); AppendMenuW(root,MF_POPUP,(UINT_PTR)preview,tr(TXT_preview));
    AppendMenuW(preview,MF_SEPARATOR,0,NULL);AppendMenuW(preview,MF_STRING,70,tr(TXT_quotaPreview));AppendMenuW(preview,MF_STRING,71,tr(TXT_longPreview));AppendMenuW(preview,MF_STRING,72,tr(TXT_unreadPreview));
    AppendMenuW(root,MF_STRING,15,tr(TXT_ackQuota));
    for(i=0;i<6;i++)AppendMenuW(colors,MF_STRING|(i==badgeStyle?MF_CHECKED:0),80+i,tr(colorKeys[i]));
    AppendMenuW(root,MF_POPUP,(UINT_PTR)colors,tr(TXT_badgeMenu));
    AppendMenuW(root,MF_STRING|(animation?MF_CHECKED:0),12,tr(TXT_animate));
    AppendMenuW(root,MF_STRING,16,tr(TXT_changeIcon));
    AppendMenuW(root,MF_STRING,17,tr(TXT_browserSettings));
    AppendMenuW(root,MF_SEPARATOR,0,NULL); AppendMenuW(root,MF_STRING,13,tr(TXT_hide)); AppendMenuW(root,MF_STRING,14,tr(TXT_exit));
    GetCursorPos(&pt); settingsOpen=1; SetForegroundWindow(panel?panel:window); choice=TrackPopupMenu(root,TPM_RETURNCMD|TPM_RIGHTBUTTON,pt.x,pt.y,0,panel?panel:window,NULL); settingsOpen=0; DestroyMenu(root);
    applyCommand(choice);
}
static void applyCommand(int choice){int i;
    if(choice>=100&&choice<105){if(!setUiLanguage(choice-100))MessageBoxW(panel?panel:window,tr(TXT_saveFailed),tr(TXT_app),MB_OK|MB_ICONERROR);}
    else if(choice==10) manual=manualMotion=-1;
    else if(choice==11) {for(i=0;i<chatCount;i++) {chats[i].ready=0;if(chats[i].mood==DONE||chats[i].mood==ERROR_STATE||chats[i].mood==STOPPED)chats[i].mood=IDLE;}manual=-1;mood=aggregate(chats,chatCount,overflow);}
    else if(choice==12) {if(savePetPreferences(badgeStyle,!animation,customEnabled))animation=!animation;}
    else if(choice==16||choice==17){openBrowserSettings(choice==16);return;}
    else if(choice==15){quotaAckAt=nowSeconds();quotaStateDirty=1;saveQuotaState();manual=manualMotion=-1;}
    else if(choice==13) {suppressedPid=desktopPid;hidePet();}
    else if(choice==14) {DestroyWindow(window);return;}
    else if(choice>=30 && choice<37){manual=choice-30;manualMotion=-1;}
    else if(choice>=70&&choice<=72){manual=WORK;manualMotion=choice==70?1:choice==71?2:3;}
    else if(choice>=80&&choice<86){if(!setBadgeStyle(choice-80))MessageBoxW(panel?panel:window,tr(TXT_saveFailed),tr(TXT_app),MB_OK|MB_ICONERROR);}
    updateMotion();refresh(0); report(); PostMessageW(window,WM_NULL,0,0);if(panel)InvalidateRect(panel,NULL,FALSE);
}
#include "session-panel.h"
#include "browser-settings.h"
static LRESULT CALLBACK wndProc(HWND h,UINT msg,WPARAM w,LPARAM l) {
    if(msg==taskbarCreated && desktopPid && suppressedPid!=desktopPid) {iconsAdded=0;refresh(1);return 0;}
    if(msg==WM_TIMER) {if(w==99){DestroyWindow(h);return 0;}if(w==ID_SCAN){pollNavigation();scan();browserSettingsPoll();}else if(w==ID_ANIM){int shown=manual>=0?manual:mood;animationFrame++;phase^=1;if(animation&&iconsAdded&&(customEnabled&&customCount>1||motion==1||motion==2||shown==WORK||shown==QUESTION||shown==ERROR_STATE||nowSeconds()<completionPulseUntil))refresh(0);}return 0;}
    if(msg==WM_APP+23){browserSocketEvent((SOCKET)w,l);return 0;}
    if(msg==WM_APP+24){applyCommand((int)w);return 0;}
    if(msg==WM_APP+25){openBrowserSettings((int)w);return 0;}
    if(msg==WM_APP+26){scan();detectionSnapshot();report();return 0;}
    if(msg==WM_TRAY && (l==WM_LBUTTONUP||l==WM_RBUTTONUP)) {togglePanel();return 0;}
    if(msg==WM_CLOSE) {DestroyWindow(h);return 0;}
    if(msg==WM_APP+20) {togglePanel();return 0;}
    if(msg==WM_APP+21) {writePanelSnapshot();return 0;}
    if(msg==WM_DESTROY) {hidePet();desktopPid=0;report();PostQuitMessage(0);return 0;}
    return DefWindowProcW(h,msg,w,l);
}
static int check(int condition,const char *name,FILE *file) {fprintf(file,"%s %s\n",condition?"PASS":"FAIL",name);return !condition;}
static int selfTest(void) {
    WCHAR path[1100]; FILE *f; int failed=0; Record r; Chat c={0},pair[2]; HICON icon; int m,p;
    _snwprintf(path,1100,L"%ls\\lite-verification.txt",folder);f=_wfopen(path,L"wb");if(!f)return 1;
    failed+=check(parse("{\"type\":\"event_msg\",\"payload\":{\"type\":\"task_started\",\"turn_id\":\"a\"}}",&r),"JSON record",f);
    c.allowed=1;fold(&c,&r,0);failed+=check(c.mood==WORK,"work start",f);
    parse("{\"type\":\"response_item\",\"payload\":{\"type\":\"message\",\"role\":\"assistant\",\"content\":[{\"text\":\"task_complete and request_user_input are just quoted text\"}]}}",&r);fold(&c,&r,0);
    failed+=check(c.mood==WORK,"nested text cannot fake events",f);
    parse("{\"type\":\"response_item\",\"payload\":{\"type\":\"function_call\",\"name\":\"request_user_input_async\",\"call_id\":\"q\",\"arguments\":\"{\\\"escaped\\\":true}\"}}",&r);fold(&c,&r,0);failed+=check(c.mood==QUESTION,"async question",f);
    parse("{\"type\":\"response_item\",\"payload\":{\"type\":\"function_call_output\",\"call_id\":\"q\"}}",&r);fold(&c,&r,0);failed+=check(c.mood==QUESTION,"acknowledgment is not answer",f);
    parse("{\"type\":\"response_item\",\"payload\":{\"type\":\"message\",\"role\":\"user\"}}",&r);fold(&c,&r,0);failed+=check(c.mood==WORK,"user answer",f);
    parse("{\"type\":\"event_msg\",\"payload\":{\"type\":\"task_complete\",\"turn_id\":\"old\"}}",&r);fold(&c,&r,0);failed+=check(c.mood==WORK,"turn mismatch ignored",f);
    strcpy(r.turn,"a");fold(&c,&r,0);failed+=check(c.mood==DONE && c.ready,"completion",f);
    pair[0]=c;memset(&pair[1],0,sizeof(Chat));pair[1].allowed=1;pair[1].mood=WORK;
    failed+=check(aggregate(pair,2,0)==WORK,"working chat wins over completion",f);
    pair[1].mood=IDLE;failed+=check(aggregate(pair,2,0)==DONE,"all finished",f);
    failed+=check(aggregate(pair,2,1)==UNKNOWN,"bounded-capacity warning",f);
    parse("{\"type\":\"session_meta\",\"payload\":{\"originator\":\"Codex Desktop\",\"source\":{\"subagent\":{\"other\":\"guardian\"}}}}",&r);fold(&c,&r,1);failed+=check(!c.allowed,"child excluded",f);
    for(m=0;m<7;m++)for(p=0;p<2;p++){icon=makeIcon(m,p);failed+=check(icon!=NULL,"icon frame",f);if(icon)DestroyIcon(icon);}
    fclose(f);return failed?1:0;
}
static int streamTest(void) {
    WCHAR testPath[1100]; FILE *f; Chat c={0}; HANDLE data; DWORD wrote; int failed=0;
    const char *start="{\"type\":\"event_msg\",\"payload\":{\"type\":\"task_started\",\"turn_id\":\"stream\"}}\n";
    const char *done="{\"type\":\"event_msg\",\"payload\":{\"type\":\"task_complete\",\"turn_id\":\"stream\"}}\n";
    _snwprintf(testPath,1100,L"%ls\\lite-test-input.tmp",folder); wcscpy(c.path,testPath); c.allowed=1;
    if(!ensureChats())return 1;
    data=CreateFileW(testPath,GENERIC_WRITE,FILE_SHARE_READ,NULL,CREATE_ALWAYS,FILE_ATTRIBUTE_NORMAL,NULL);if(data==INVALID_HANDLE_VALUE)return 1;
    WriteFile(data,start,(DWORD)strlen(start),&wrote,NULL);CloseHandle(data);
    readChat(&c,0);if(c.mood!=WORK)failed++;
    data=CreateFileW(testPath,FILE_APPEND_DATA,FILE_SHARE_READ,NULL,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,NULL);
    WriteFile(data,done,(DWORD)strlen(done)-1,&wrote,NULL);CloseHandle(data);
    readChat(&c,0);if(c.mood!=WORK)failed++;
    data=CreateFileW(testPath,FILE_APPEND_DATA,FILE_SHARE_READ,NULL,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,NULL);WriteFile(data,"\n",1,&wrote,NULL);CloseHandle(data);
    readChat(&c,0);if(c.mood!=DONE || !c.ready)failed++;
    DeleteFileW(testPath);freeChats();
    _snwprintf(testPath,1100,L"%ls\\lite-stream-verification.txt",folder);f=_wfopen(testPath,L"wb");if(f){fprintf(f,"%s: append-only streaming, incomplete JSON line deferred, completion after newline, buffers released.\n",failed?"FAIL":"PASS");fclose(f);}return failed?1:0;
}
static int lifecycleTest(void) {
    WCHAR path[1100]; FILE *f; int i,failed=0; DWORD startHandles,endHandles;
    typedef BOOL (WINAPI *HandleCountFn)(HANDLE,PDWORD);
    typedef DWORD (WINAPI *GuiCountFn)(HANDLE,DWORD);
    HandleCountFn handles=(HandleCountFn)GetProcAddress(GetModuleHandleW(L"kernel32.dll"),"GetProcessHandleCount");
    GuiCountFn gui=(GuiCountFn)GetProcAddress(GetModuleHandleW(L"user32.dll"),"GetGuiResources");
    DWORD gdiBefore,userBefore,gdiAfter,userAfter;
    /* Warm shell API loading before testing repeated release. No Codex process is stopped. */
    for(i=0;i<5;i++){showPet();hidePet();}handles(GetCurrentProcess(),&startHandles);gdiBefore=gui(GetCurrentProcess(),0);userBefore=gui(GetCurrentProcess(),1);
    for(i=0;i<10;i++) {showPet();if(!iconsAdded||!chats||!lineBuf)failed++;hidePet();if(iconsAdded||chats||lineBuf||icons[0][0])failed++;}
    handles(GetCurrentProcess(),&endHandles);gdiAfter=gui(GetCurrentProcess(),0);userAfter=gui(GetCurrentProcess(),1);
    if(endHandles>startHandles+2||gdiAfter>gdiBefore||userAfter>userBefore)failed++;
    _snwprintf(path,1100,L"%ls\\lite-lifecycle-verification.txt",folder);f=_wfopen(path,L"wb");if(f){fprintf(f,"%s: 10 active/inactive cycles, tray re-added, icon/chat/buffer release; handles %lu -> %lu; GDI %lu -> %lu; USER %lu -> %lu. Actual Codex Exit was not invoked.\n",failed?"FAIL":"PASS",startHandles,endHandles,gdiBefore,gdiAfter,userBefore,userAfter);fclose(f);}return failed?1:0;
}
#include "motion-tests.h"
#include "detection-tests.h"
#include "custom-state-tests.h"
#include "language-tests.h"
int WINAPI WinMain(HINSTANCE instance,HINSTANCE prev,LPSTR command,int show) {
    MSG msg; WNDCLASSW wc; WCHAR home[1024],*slash; int probeOk; DWORD probe;
    enableDpiAwareness();
    GetModuleFileNameW(NULL,folder,1024);slash=wcsrchr(folder,L'\\');if(slash)*slash=0;
    loadBadgeStyle();
    if(!GetEnvironmentVariableW(L"CODEX_HOME",home,1024)){if(!GetEnvironmentVariableW(L"USERPROFILE",home,1000))return 3;wcscat(home,L"\\.codex");}wcscpy(codexHome,home);
    ProcessIdToSessionId(GetCurrentProcessId(),&sessionId);
    imageName=(ImageNameFn)GetProcAddress(GetModuleHandleW(L"kernel32.dll"),"QueryFullProcessImageNameW");if(!imageName)return 2;
    if(strstr(command,"--language-test"))return languageTest();
    if(strstr(command,"--self-test"))return selfTest();
    if(strstr(command,"--motion-test"))return motionTest();
    if(strstr(command,"--custom-test"))return customIconTest();
    if(strstr(command,"--custom-state-test"))return customStateTest();
    if(strstr(command,"--quota-state-test"))return quotaStateTest();
    if(strstr(command,"--badge-samples"))return badgeSamples();
    if(strstr(command,"--panel-test"))return panelTest();
    if(strstr(command,"--stream-test"))return streamTest();
    if(strstr(command,"--detection-test"))return detectionTest();
    if(strstr(command,"--diagnose")){HWND h=FindWindowW(L"CodexPetLiteWindow",L"Codex Pet Lite");if(h){PostMessageW(h,WM_APP+26,0,0);return 0;}return 1;}
    if(strstr(command,"--open-settings-url ")){char narrow[256]={0};WCHAR url[256];sscanf(strstr(command,"--open-settings-url ")+20,"\"%255[^\"]",narrow);if(strncmp(narrow,"http://127.0.0.1:",17)||strpbrk(narrow," \r\n\\"))return 2;MultiByteToWideChar(CP_UTF8,0,narrow,-1,url,256);return launchSettingsBrowser(url)?0:1;}
    if(strstr(command,"--navigate ")){char id[64]={0};WCHAR url[96];sscanf(strstr(command,"--navigate ")+11,"%63s",id);if(!sessionUrl(id,url))return 2;return (int)(INT_PTR)ShellExecuteW(NULL,L"open",url,NULL,NULL,SW_SHOWNORMAL);}
    if(strstr(command,"--stop")){HANDLE e=OpenEventW(EVENT_MODIFY_STATE,FALSE,L"Local\\CodexPetLiteStop");if(e){SetEvent(e);CloseHandle(e);return 0;}return 1;}
    if(strstr(command,"--show-panel")){HWND h=FindWindowW(L"CodexPetLiteWindow",NULL);if(h){PostMessageW(h,WM_APP+20,0,0);return 0;}return 1;}
    if(strstr(command,"--settings")){HWND h=FindWindowW(L"CodexPetLiteWindow",L"Codex Pet Lite");if(h){PostMessageW(h,WM_APP+25,strstr(command,"--settings-icon")?1:0,0);return 0;}return 1;}
    singleton=CreateMutexW(NULL,FALSE,(strstr(command,"--lifecycle-test")||strstr(command,"--panel-check")||strstr(command,"--warning-preview"))?L"Local\\CodexPetLiteTest":L"Local\\CodexPetLite");if(!singleton || GetLastError()==ERROR_ALREADY_EXISTS){if(singleton)CloseHandle(singleton);return 0;}
    stopEvent=CreateEventW(NULL,FALSE,FALSE,L"Local\\CodexPetLiteStop");
    if(!GetEnvironmentVariableW(L"CODEX_HOME",home,1024)){if(!GetEnvironmentVariableW(L"USERPROFILE",home,1000))return 3;wcscat(home,L"\\.codex");}
    wcscpy(codexHome,home);
    _snwprintf(sessions,1024,L"%ls\\sessions",home);
    memset(&wc,0,sizeof(wc));wc.lpfnWndProc=wndProc;wc.hInstance=instance;wc.lpszClassName=L"CodexPetLiteWindow";RegisterClassW(&wc);
    window=CreateWindowExW(WS_EX_TOOLWINDOW,wc.lpszClassName,L"Codex Pet Lite",0,0,0,0,0,NULL,NULL,instance,NULL);if(!window)return 4;
    memset(&tray,0,sizeof(tray));tray.cbSize=sizeof(tray);tray.hWnd=window;tray.uID=1;tray.uCallbackMessage=WM_TRAY;
    taskbarCreated=RegisterWindowMessageW(L"TaskbarCreated");
    if(strstr(command,"--warning-preview")){int i;const WCHAR *title[]={L"예제 작업 A",L"예제 작업 B",L"예제 작업 C",L"예제 작업 D"};
        ensureChats();desktopPid=1;togglePanel();chatCount=4;readStateOk=1;unreadCount=1;strcpy(unreadIds[0],"preview-3");
        for(i=0;i<4;i++){memset(&chats[i],0,sizeof(Chat));chats[i].allowed=1;sprintf(chats[i].id,"preview-%d",i);wcscpy(chats[i].title,title[i]);chats[i].mood=i<2?STOPPED:i==2?QUESTION:IDLE;chats[i].quotaBlocked=i<2;chats[i].latestAt=nowSeconds();chats[i].startedAt=nowSeconds()-130;}
        refreshPanelRows(0);repaintPanel();SetTimer(window,99,10000,NULL);
        while(GetMessageW(&msg,NULL,0,0)>0){TranslateMessage(&msg);DispatchMessageW(&msg);}CloseHandle(stopEvent);CloseHandle(singleton);return 0;
    }
    if(strstr(command,"--lifecycle-test")){int result=lifecycleTest();CloseHandle(stopEvent);CloseHandle(singleton);return result;}
    if(strstr(command,"--panel-check")){int result=panelCheck();CloseHandle(stopEvent);CloseHandle(singleton);return result;}
    scan();SetTimer(window,ID_SCAN,2000,NULL);
    while(GetMessageW(&msg,NULL,0,0)>0){TranslateMessage(&msg);DispatchMessageW(&msg);}
    if(navigationHelper)CloseHandle(navigationHelper);CloseHandle(stopEvent);CloseHandle(singleton);return 0;
}
