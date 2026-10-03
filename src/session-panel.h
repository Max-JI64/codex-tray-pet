/* Native popup; no WebView or writes to Codex state. */
static const char *member(const char *p,const char *wanted) {
    char key[160];p=ws(p);if(*p!='{')return NULL;p=ws(p+1);
    while(*p && *p!='}') {p=stringValue(p,key,sizeof(key));if(!p)return NULL;p=ws(p);if(*p++!=':')return NULL;p=ws(p);if(strcmp(key,wanted)==0)return p;p=skipValue(p,0);if(!p)return NULL;p=ws(p);if(*p==',')p=ws(p+1);else if(*p!='}')return NULL;}return NULL;
}
static char *textFile(const WCHAR *path) {
    HANDLE f; LARGE_INTEGER size;DWORD got;char *text;
    f=CreateFileW(path,GENERIC_READ,FILE_SHARE_READ|FILE_SHARE_WRITE|FILE_SHARE_DELETE,NULL,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,NULL);if(f==INVALID_HANDLE_VALUE)return NULL;
    if(!GetFileSizeEx(f,&size)||size.QuadPart<2||size.QuadPart>8*1024*1024){CloseHandle(f);return NULL;}
    text=VirtualAlloc(NULL,(SIZE_T)size.QuadPart+1,MEM_RESERVE|MEM_COMMIT,PAGE_READWRITE);
    if(!text){CloseHandle(f);return NULL;}
    if(!ReadFile(f,text,(DWORD)size.QuadPart,&got,NULL)||got!=size.QuadPart){VirtualFree(text,0,MEM_RELEASE);CloseHandle(f);return NULL;}
    text[got]=0;CloseHandle(f);return text;
}
static int valueString(const char *object,const char *key,char *out,int cap) {const char *p=object?member(object,key):NULL;return p&&stringValue(p,out,cap)!=NULL;}
static int legacyProjectName(const char *id,WCHAR *out,int cap) {
    WCHAR path[1100];char *state,kind[32],projectId[96],name[2048],host[160];const char *assign,*entry,*projects,*project,*hosts;int ok=0;
    _snwprintf(path,1100,L"%ls\\.codex-global-state.json",codexHome);state=textFile(path);if(!state)return 0;
    assign=member(state,"thread-project-assignments");entry=assign?member(assign,id):NULL;hosts=member(state,"thread-project-membership-host-ids");
    if(hosts&&valueString(hosts,id,host,sizeof(host))&&strcmp(host,"local")&&strncmp(host,"local:",6))goto end;
    if(!entry||!valueString(entry,"projectKind",kind,sizeof(kind))||strcmp(kind,"local")||!valueString(entry,"projectId",projectId,sizeof(projectId)))goto end;
    projects=member(state,"local-projects");project=projects?member(projects,projectId):NULL;
    if(project&&valueString(project,"name",name,sizeof(name)))ok=MultiByteToWideChar(CP_UTF8,0,name,-1,out,cap)>0;
end: VirtualFree(state,0,MEM_RELEASE);return ok;
}
static int safeId(const char *text){int i;if(!text[0])return 0;for(i=0;text[i];i++)if(!(text[i]>='a'&&text[i]<='z')&&!(text[i]>='A'&&text[i]<='Z')&&!(text[i]>='0'&&text[i]<='9')&&text[i]!='-'&&text[i]!='_')return 0;return 1;}
static int shaIdentity(const char *account,const char *user,char hash[65]) {
    typedef BOOL(WINAPI *AcquireFn)(ULONG_PTR*,LPCWSTR,LPCWSTR,DWORD,DWORD);
    typedef BOOL(WINAPI *CreateFn)(ULONG_PTR,DWORD,ULONG_PTR,DWORD,ULONG_PTR*);
    typedef BOOL(WINAPI *DataFn)(ULONG_PTR,const BYTE*,DWORD,DWORD);
    typedef BOOL(WINAPI *GetFn)(ULONG_PTR,DWORD,BYTE*,DWORD*,DWORD);
    typedef BOOL(WINAPI *DestroyFn)(ULONG_PTR);typedef BOOL(WINAPI *ReleaseFn)(ULONG_PTR,DWORD);
    HMODULE lib;AcquireFn acquire;CreateFn create;DataFn data;GetFn get;DestroyFn destroy;ReleaseFn release;ULONG_PTR provider=0,digest=0;BYTE bytes[32];DWORD length=32;char input[512];int ok=0,i;
    if(!safeId(account)||!safeId(user))return 0;
    lib=LoadLibraryW(L"advapi32.dll");if(!lib)return 0;
    acquire=(AcquireFn)GetProcAddress(lib,"CryptAcquireContextW");create=(CreateFn)GetProcAddress(lib,"CryptCreateHash");data=(DataFn)GetProcAddress(lib,"CryptHashData");get=(GetFn)GetProcAddress(lib,"CryptGetHashParam");destroy=(DestroyFn)GetProcAddress(lib,"CryptDestroyHash");release=(ReleaseFn)GetProcAddress(lib,"CryptReleaseContext");
    if(!acquire||!create||!data||!get||!destroy||!release)goto end;
    if(!acquire(&provider,NULL,NULL,24,0xf0000000)||!create(provider,0x800c,0,0,&digest))goto end;
    _snprintf(input,sizeof(input),"[\"chatgpt\",\"%s\",\"%s\"]",account,user);
    if(!data(digest,(const BYTE*)input,(DWORD)strlen(input),0)||!get(digest,2,bytes,&length,0))goto end;
    for(i=0;i<32;i++)sprintf(hash+i*2,"%02x",bytes[i]);hash[64]=0;ok=1;
end: if(digest&&destroy)destroy(digest);if(provider&&release)release(provider,0);FreeLibrary(lib);return ok;
}
static int currentIdentity(char hash[65]) {
    WCHAR path[1100];char *auth;const char *tokens,*claims;char token[16384],decoded[12288],account[128],user[128];int i,n=0,bits=0,value=0,ok=0;
    _snwprintf(path,1100,L"%ls\\auth.json",codexHome);auth=textFile(path);if(!auth)return 0;
    tokens=member(auth,"tokens");if(!tokens||!valueString(tokens,"id_token",token,sizeof(token)))goto end;
    {char *p=strchr(token,'.');if(!p)goto end;p++;for(i=0;p[i]&&p[i]!='.';i++) {int x;char c=p[i];if(c>='A'&&c<='Z')x=c-'A';else if(c>='a'&&c<='z')x=c-'a'+26;else if(c>='0'&&c<='9')x=c-'0'+52;else if(c=='-'||c=='+')x=62;else if(c=='_'||c=='/')x=63;else if(c=='=')break;else goto end;value=((value<<6)|x)&65535;bits+=6;if(bits>=8){bits-=8;if(n>=sizeof(decoded)-1)goto end;decoded[n++]=(char)((value>>bits)&255);}}decoded[n]=0;}
    claims=member(decoded,"https://api.openai.com/auth");if(!claims)goto end;
    if(!valueString(tokens,"account_id",account,sizeof(account))&&!valueString(claims,"chatgpt_account_id",account,sizeof(account)))goto end;
    if(!valueString(claims,"chatgpt_user_id",user,sizeof(user))&&!valueString(claims,"user_id",user,sizeof(user)))goto end;
    ok=shaIdentity(account,user,hash);
end: memset(token,0,sizeof(token));memset(decoded,0,sizeof(decoded));memset(account,0,sizeof(account));memset(user,0,sizeof(user));memset(auth,0,strlen(auth));VirtualFree(auth,0,MEM_RELEASE);return ok;
}
static int unreadFrom(const char *json,const char *identity,const char *host) {
    const char *state,*map,*bucket,*ids,*p;char id[64];int n=0;
    state=member(json,"electron-thread-read-state-v1");map=state?member(state,"unreadByIdentity"):NULL;bucket=map?member(map,identity):NULL;
    if(!state||!map)return 0;
    /* A valid current account without a bucket has no unread chats. Never union old accounts. */
    if(!bucket){unreadCount=0;return 1;}
    ids=member(bucket,host);if(!ids){unreadCount=0;return 1;}p=ws(ids);if(*p!='[')return 0;p=ws(p+1);
    while(*p&&*p!=']'){p=stringValue(p,id,sizeof(id));if(!p||n==MAX_CHATS)return 0;strcpy(unreadIds[n++],id);p=ws(p);if(*p==',')p=ws(p+1);else if(*p!=']')return 0;}
    if(*p!=']')return 0;unreadCount=n;return 1;
}
static void syncUnread(void) {
    WCHAR path[1100],authPath[1100];WIN32_FILE_ATTRIBUTE_DATA stateAttr,authAttr;char identity[65],host[160],*state;const char *read,*migration,*hosts;
    _snwprintf(path,1100,L"%ls\\.codex-global-state.json",codexHome);_snwprintf(authPath,1100,L"%ls\\auth.json",codexHome);
    if(!GetFileAttributesExW(path,GetFileExInfoStandard,&stateAttr)||!GetFileAttributesExW(authPath,GetFileExInfoStandard,&authAttr)){readStateOk=0;return;}
    if(readStateOk&&CompareFileTime(&unreadStateStamp,&stateAttr.ftLastWriteTime)==0&&CompareFileTime(&unreadAuthStamp,&authAttr.ftLastWriteTime)==0)return;
    readStateOk=0;if(!currentIdentity(identity))return;
    _snwprintf(path,1100,L"%ls\\.codex-global-state.json",codexHome);state=textFile(path);if(!state)return;
    read=member(state,"electron-thread-read-state-v1");migration=read?member(read,"legacyMigration"):NULL;hosts=migration?member(migration,"adoptedHostIds"):NULL;
    if(hosts&&valueString(hosts,"local",host,sizeof(host)))readStateOk=unreadFrom(state,identity,host);
    else {
        const char *map=read?member(read,"unreadByIdentity"):NULL,*bucket=map?member(map,identity):NULL,*p=bucket;int count=0;
        if(bucket){p=ws(bucket+1);while(*p&&*p!='}'){char key[160];p=stringValue(p,key,sizeof(key));if(!p)break;p=ws(p);if(*p++!=':')break;p=ws(p);if(strncmp(key,"local:",6)==0){strcpy(host,key);count++;}p=skipValue(p,0);if(!p)break;p=ws(p);if(*p==',')p=ws(p+1);else if(*p!='}')break;}}
        if(count==1)readStateOk=unreadFrom(state,identity,host);else if(map&&!bucket){unreadCount=0;readStateOk=1;}
    }
    VirtualFree(state,0,MEM_RELEASE);if(readStateOk){unreadStateStamp=stateAttr.ftLastWriteTime;unreadAuthStamp=authAttr.ftLastWriteTime;}
}
static int isUnread(const char *id){int i;if(!readStateOk)return 0;for(i=0;i<unreadCount;i++)if(strcmp(unreadIds[i],id)==0)return 1;return 0;}
static int activeChat(const Chat *c){return (c->mood==WORK||c->mood==QUESTION)&&(!desktopStartedAt||c->latestAt>=desktopStartedAt);}
static int askingChat(const Chat *c){return activeChat(c)&&c->mood==QUESTION;}
static int latestChat(int i){int j;if(!chats[i].allowed||!chats[i].id[0])return 0;for(j=0;j<chatCount;j++)if(j!=i&&chats[j].allowed&&strcmp(chats[i].id,chats[j].id)==0&&(chats[j].latestAt>chats[i].latestAt||(chats[j].latestAt==chats[i].latestAt&&j>i)))return 0;return 1;}
static HWND projectTip;
static WCHAR hoveredProject[512];
static char hoveredProjectId[64];
static void hideProjectTip(void){if(projectTip)DestroyWindow(projectTip);projectTip=NULL;hoveredProjectId[0]=0;hoveredProject[0]=0;}
static int visibleChat(const Chat *c){return activeChat(c)||isUnread(c->id)||c->quotaBlocked;}
static void discoverSessionId(const char *id) {
        int i;Chat *c;char digits[13];WCHAR wideId[64],pattern[1100];ULARGE_INTEGER ft;FILETIME fileTime;SYSTEMTIME date;WIN32_FIND_DATAW found;HANDLE find;
        for(i=0;i<chatCount;i++)if(chats[i].allowed&&strcmp(chats[i].id,id)==0)break;if(i<chatCount)return;
        if(chatCount==MAX_CHATS){overflow=1;return;}
        c=&chats[chatCount++];memset(c,0,sizeof(*c));strncpy(c->id,id,63);c->allowed=1;c->mood=IDLE;
        /* UUIDv7 creation date locates older unread rollouts without a recursive disk scan. */
        if(strlen(id)<13)return;memcpy(digits,id,8);memcpy(digits+8,id+9,4);digits[12]=0;
        ft.QuadPart=_strtoui64(digits,NULL,16)*10000+116444736000000000LL;fileTime.dwLowDateTime=ft.LowPart;fileTime.dwHighDateTime=ft.HighPart;
        if(!FileTimeToSystemTime(&fileTime,&date)||!MultiByteToWideChar(CP_UTF8,0,id,-1,wideId,64))return;
        _snwprintf(pattern,1100,L"%ls\\%04u\\%02u\\%02u\\rollout-*-%ls.jsonl",sessions,date.wYear,date.wMonth,date.wDay,wideId);
        find=FindFirstFileW(pattern,&found);if(find==INVALID_HANDLE_VALUE)return;
        _snwprintf(c->path,1024,L"%ls\\%04u\\%02u\\%02u\\%ls",sessions,date.wYear,date.wMonth,date.wDay,found.cFileName);FindClose(find);readChat(c,1);
}
static void discoverUnreadChats(void){int u;if(!readStateOk)return;for(u=0;u<unreadCount;u++)discoverSessionId(unreadIds[u]);}
static void syncTitles(void) {
    WCHAR path[1100];WIN32_FILE_ATTRIBUTE_DATA attr;FILE *f;char record[8192];int i;
    _snwprintf(path,1100,L"%ls\\session_index.jsonl",codexHome);
    if(!GetFileAttributesExW(path,GetFileExInfoStandard,&attr))return;
    if(CompareFileTime(&indexStamp,&attr.ftLastWriteTime)==0){int missing=0;for(i=0;i<chatCount;i++)if(chats[i].allowed&&!chats[i].title[0])missing=1;if(!missing)return;}
    f=_wfopen(path,L"rb");if(!f)return;
    while(fgets(record,sizeof(record),f)){Record r;if(!strchr(record,'\n')&&!feof(f)){int ch;while((ch=fgetc(f))!=EOF&&ch!='\n');continue;}if(!parse(record,&r)||!r.chatId[0]||!r.title[0])continue;for(i=0;i<chatCount;i++)if(strcmp(chats[i].id,r.chatId)==0)MultiByteToWideChar(CP_UTF8,0,r.title,-1,chats[i].title,256);}
    fclose(f);indexStamp=attr.ftLastWriteTime;
}
static void refreshPanelRows(int preserveOrder) {
    int candidates[MAX_CHATS],next[MAX_CHATS],count=0,n=0,i,j;
    for(i=0;i<chatCount;i++)if(latestChat(i)&&visibleChat(&chats[i]))candidates[count++]=i;
    if(!preserveOrder) {
        /* Sort once when opening. Record activity must not move an open row. */
        for(i=1;i<count;i++){int row=candidates[i];j=i;while(j>0){Chat *a=&chats[row],*b=&chats[candidates[j-1]];if(activeChat(a)<activeChat(b)||(activeChat(a)==activeChat(b)&&a->latestAt<=b->latestAt))break;candidates[j]=candidates[j-1];j--;}candidates[j]=row;}
    } else {
        /* Match by session ID so a newer rollout for the same chat keeps its slot. */
        for(i=0;i<panelCount;i++)for(j=0;j<count;j++)if(candidates[j]>=0&&strcmp(chats[panelRows[i]].id,chats[candidates[j]].id)==0){next[n++]=candidates[j];candidates[j]=-1;break;}
    }
    for(i=0;i<count;i++)if(candidates[i]>=0)next[n++]=candidates[i];
    memcpy(panelRows,next,n*sizeof(int));panelCount=n;
    if(panelOffset>panelCount-panelPage)panelOffset=panelCount>panelPage?panelCount-panelPage:0;
}
static void updatePanelData(void) {
    syncUnread();discoverUnreadChats();syncQuotaReadState();saveQuotaState();syncTitles();refreshPanelRows(panel!=NULL);
    if(projectTip&&(panelHover<0||panelHover>=panelCount||strcmp(hoveredProjectId,chats[panelRows[panelHover]].id)))hideProjectTip();
}
static int px(int value){return MulDiv(value,uiScale,100);}
static void enableDpiAwareness(void) {
    typedef BOOL(WINAPI *AwareFn)(HANDLE);
    AwareFn aware=(AwareFn)GetProcAddress(GetModuleHandleW(L"user32.dll"),"SetProcessDpiAwarenessContext");
    if(aware)aware((HANDLE)-4);
}
static void menuStyle(POINT pt) {
    typedef UINT(WINAPI *DpiFn)(HWND);
    typedef BOOL(WINAPI *MetricsFn)(UINT,UINT,PVOID,UINT,UINT);
    struct {NONCLIENTMETRICSW metrics;int paddedBorderWidth;} n;
    HMODULE user=GetModuleHandleW(L"user32.dll");DpiFn getDpi=(DpiFn)GetProcAddress(user,"GetDpiForWindow");MetricsFn getMetrics=(MetricsFn)GetProcAddress(user,"SystemParametersInfoForDpi");
    UINT dpi=96;LOGFONTW font;int ok=0;
    if(window){SetWindowPos(window,NULL,pt.x,pt.y,0,0,SWP_NOSIZE|SWP_NOACTIVATE|SWP_NOZORDER);if(getDpi)dpi=getDpi(window);}
    if(!dpi)dpi=96;uiScale=MulDiv(dpi,100,96);
    memset(&n,0,sizeof(n));n.metrics.cbSize=sizeof(n);
    if(getMetrics)ok=getMetrics(SPI_GETNONCLIENTMETRICS,sizeof(n),&n,0,dpi);
    else ok=SystemParametersInfoW(SPI_GETNONCLIENTMETRICS,sizeof(n),&n,0);
    memset(&font,0,sizeof(font));
    if(ok)font=n.metrics.lfMenuFont;
    else {font.lfHeight=-MulDiv(12,dpi,96);font.lfWeight=FW_NORMAL;font.lfCharSet=DEFAULT_CHARSET;wcscpy(font.lfFaceName,L"Malgun Gothic");}
    font.lfQuality=CLEARTYPE_NATURAL_QUALITY;font.lfOutPrecision=OUT_TT_ONLY_PRECIS;
    panelFont=CreateFontIndirectW(&font);headerFont=CreateFontIndirectW(&font);
    panelRowHeight=ok?MulDiv(n.metrics.iMenuHeight,96,dpi)+3:22;if(panelRowHeight<22)panelRowHeight=22;
    panelHeaderHeight=panelRowHeight+8;if(panelHeaderHeight<30)panelHeaderHeight=30;
}
static int panelHeight(void){return px(panelHeaderHeight+panelRowHeight*(panelCount<panelPage?(panelCount?panelCount:1):panelPage)+12);}
static int panelRowAt(int x,int y) {
    int row;if(x<px(8)||x>=px(272)||y<px(panelHeaderHeight)||y>=px(panelHeaderHeight+panelRowHeight*panelPage))return -1;
    row=panelOffset+(y-px(panelHeaderHeight))/px(panelRowHeight);return row<panelCount?row:-1;
}
static int sessionUrl(const char *id,WCHAR out[96]) {
    int i;WCHAR wide[64];if(strlen(id)!=36)return 0;
    for(i=0;i<36;i++){int hyphen=i==8||i==13||i==18||i==23;char c=id[i];if(hyphen?c!='-':!((c>='0'&&c<='9')||(c>='a'&&c<='f')||(c>='A'&&c<='F')))return 0;}
    if(!MultiByteToWideChar(CP_UTF8,0,id,-1,wide,64))return 0;
    _snwprintf(out,96,L"codex://threads/%ls",wide);return 1;
}
static void openPanelSession(int row) {
    WCHAR url[96],executable[1024],command[1200],wideId[64];char id[64];STARTUPINFOW startup={0};PROCESS_INFORMATION child={0};
    if(row<0||row>=panelCount||!desktopPid)return;
    strcpy(id,chats[panelRows[row]].id);if(!sessionUrl(id,url))return;
    if(panel)DestroyWindow(panel);
    strcpy(lastNavigationId,id);lastNavigationResult=0;
    GetModuleFileNameW(NULL,executable,1024);MultiByteToWideChar(CP_UTF8,0,id,-1,wideId,64);_snwprintf(command,1200,L"\"%ls\" --navigate %ls",executable,wideId);
    startup.cb=sizeof(startup);startup.dwFlags=STARTF_USESHOWWINDOW;startup.wShowWindow=SW_HIDE;
    if(navigationHelper){CloseHandle(navigationHelper);navigationHelper=NULL;}
    /* Windows URI activation loads COM DLLs; release them with a click-only process. */
    if(CreateProcessW(executable,command,NULL,NULL,FALSE,CREATE_NO_WINDOW,NULL,folder,&startup,&child)) {
        CloseHandle(child.hThread);navigationHelper=child.hProcess;
    } else MessageBoxW(window,L"Codex 세션 링크를 실행할 수 없습니다.",L"코덱스 펫",MB_OK|MB_ICONERROR);
    report();
}
static void pollNavigation(void) {
    DWORD result;if(!navigationHelper||WaitForSingleObject(navigationHelper,0)!=WAIT_OBJECT_0)return;
    if(!GetExitCodeProcess(navigationHelper,&result))result=0;CloseHandle(navigationHelper);navigationHelper=NULL;lastNavigationResult=result;
    if(result<=32)MessageBoxW(window,L"Codex 세션 링크를 열 수 없습니다. Codex 앱의 URL 연결을 확인해 주세요.",L"코덱스 펫",MB_OK|MB_ICONERROR);
}
static void roundPanel(HWND h) {
    typedef HRESULT(WINAPI *AttributeFn)(HWND,DWORD,LPCVOID,DWORD);HMODULE lib=LoadLibraryW(L"dwmapi.dll");int rounded=2;COLORREF border=RGB(218,218,218);
    if(lib){AttributeFn attribute=(AttributeFn)GetProcAddress(lib,"DwmSetWindowAttribute");if(attribute){attribute(h,33,&rounded,sizeof(rounded));attribute(h,34,&border,sizeof(border));}FreeLibrary(lib);}
}
static LRESULT CALLBACK projectTipProc(HWND h,UINT msg,WPARAM w,LPARAM l) {
    if(msg==WM_MOUSEACTIVATE)return MA_NOACTIVATE;
    if(msg==WM_ERASEBKGND)return 1;
    if(msg==WM_PAINT){PAINTSTRUCT ps;HDC dc=BeginPaint(h,&ps);RECT r;HBRUSH bg=CreateSolidBrush(RGB(249,249,249));HGDIOBJ old;
        GetClientRect(h,&r);FillRect(dc,&r,bg);DeleteObject(bg);SetBkMode(dc,OPAQUE);SetBkColor(dc,RGB(249,249,249));old=SelectObject(dc,panelFont);
        r.left=px(12);r.right-=px(12);SetTextColor(dc,RGB(24,24,24));DrawTextW(dc,hoveredProject,-1,&r,DT_SINGLELINE|DT_VCENTER|DT_END_ELLIPSIS|DT_NOPREFIX);SelectObject(dc,old);EndPaint(h,&ps);return 0;}
    return DefWindowProcW(h,msg,w,l);
}
static int projectTipX(const RECT *parent,const RECT *work,int width){return parent->right+width<=work->right?parent->right:parent->left-width>=work->left?parent->left-width:work->left;}
static void showProjectTip(int row) {
    WNDCLASSW wc={0};RECT parent;MONITORINFO info;POINT anchor;SIZE textSize={0};HDC dc;HGDIOBJ old;int x,y,width,height=px(panelRowHeight);const char *id;
    hideProjectTip();if(!panel||row<0||row>=panelCount)return;id=chats[panelRows[row]].id;
    if(!projectName(id,hoveredProject,512))return;strcpy(hoveredProjectId,id);
    dc=GetDC(panel);old=SelectObject(dc,panelFont);GetTextExtentPoint32W(dc,hoveredProject,(int)wcslen(hoveredProject),&textSize);SelectObject(dc,old);ReleaseDC(panel,dc);
    width=textSize.cx+px(24)+2;if(width<px(64))width=px(64);if(width>px(220))width=px(220);
    wc.style=CS_DROPSHADOW;wc.lpfnWndProc=projectTipProc;wc.hInstance=GetModuleHandleW(NULL);wc.lpszClassName=L"CodexPetProjectTip";wc.hCursor=LoadCursor(NULL,IDC_ARROW);RegisterClassW(&wc);
    GetWindowRect(panel,&parent);info.cbSize=sizeof(info);GetMonitorInfoW(MonitorFromWindow(panel,MONITOR_DEFAULTTONEAREST),&info);x=projectTipX(&parent,&info.rcWork,width);
    anchor.x=0;anchor.y=px(panelHeaderHeight+(row-panelOffset)*panelRowHeight);ClientToScreen(panel,&anchor);y=anchor.y;if(y+height>info.rcWork.bottom)y=info.rcWork.bottom-height;if(y<info.rcWork.top)y=info.rcWork.top;
    projectTip=CreateWindowExW(WS_EX_TOOLWINDOW|WS_EX_TOPMOST|WS_EX_NOACTIVATE,wc.lpszClassName,L"코덱스 프로젝트",WS_POPUP|WS_BORDER,x,y,width,height,panel,NULL,wc.hInstance,NULL);
    if(projectTip){roundPanel(projectTip);ShowWindow(projectTip,SW_SHOWNOACTIVATE);}
}
static void elapsedText(LONGLONG started,LONGLONG now,WCHAR out[40]) {
    LONGLONG seconds=now>started?now-started:0;
    if(started<=0){wcscpy(out,L"—");return;}
    if(seconds>=3600)_snwprintf(out,40,L"%lld:%02lld:%02lld",seconds/3600,(seconds/60)%60,seconds%60);
    else _snwprintf(out,40,L"%02lld:%02lld",seconds/60,seconds%60);
}
static void repaintPanel(void){if(panel){RECT r;int height=panelHeight();GetWindowRect(panel,&r);if(r.bottom-r.top!=height){hideProjectTip();panelHover=-1;SetWindowPos(panel,NULL,r.left,r.bottom-height,px(280),height,SWP_NOACTIVATE|SWP_NOZORDER);}InvalidateRect(panel,NULL,FALSE);}}
static void drawPanel(HDC dc,RECT bounds) {
    RECT r;HBRUSH background=CreateSolidBrush(RGB(249,249,249));HPEN line=CreatePen(PS_SOLID,1,RGB(218,218,218));HGDIOBJ oldPen,oldFont;int row,y;
    r=bounds;FillRect(dc,&r,background);DeleteObject(background);SetBkMode(dc,OPAQUE);SetBkColor(dc,RGB(249,249,249));
    oldFont=SelectObject(dc,headerFont);SetTextColor(dc,RGB(24,24,24));r.left=px(12);r.top=px(4);r.right=px(200);r.bottom=px(panelHeaderHeight-4);DrawTextW(dc,L"세션",-1,&r,DT_SINGLELINE|DT_VCENTER);
    r.left=px(216);r.right=px(268);DrawTextW(dc,L"설정",-1,&r,DT_SINGLELINE|DT_VCENTER|DT_CENTER);
    SelectObject(dc,panelFont);oldPen=SelectObject(dc,line);MoveToEx(dc,px(8),px(panelHeaderHeight-2),NULL);LineTo(dc,px(272),px(panelHeaderHeight-2));
    if(!panelCount){r.left=px(12);r.top=px(panelHeaderHeight);r.right=px(268);r.bottom=px(panelHeaderHeight+panelRowHeight);SetTextColor(dc,GetSysColor(COLOR_GRAYTEXT));DrawTextW(dc,L"표시할 세션이 없습니다.",-1,&r,DT_SINGLELINE|DT_VCENTER);}
    for(row=panelOffset;row<panelCount&&row<panelOffset+panelPage;row++) {
        Chat *c=&chats[panelRows[row]];WCHAR elapsed[40];y=px(panelHeaderHeight+(row-panelOffset)*panelRowHeight);
        if(row==panelHover){HBRUSH hover=CreateSolidBrush(RGB(235,235,235));r.left=px(6);r.top=y;r.right=px(274);r.bottom=y+px(panelRowHeight);FillRect(dc,&r,hover);DeleteObject(hover);SetBkColor(dc,RGB(235,235,235));}else SetBkColor(dc,RGB(249,249,249));
        r.left=px(12);r.top=y;r.right=px(188);r.bottom=y+px(panelRowHeight);SetTextColor(dc,RGB(24,24,24));
        DrawTextW(dc,c->title[0]?c->title:L"제목 확인 중",-1,&r,DT_SINGLELINE|DT_VCENTER|DT_END_ELLIPSIS|DT_NOPREFIX);
        if(c->quotaBlocked){int side=px(13),left=px(252),top=y+(px(panelRowHeight)-side)/2;HPEN warning=CreatePen(PS_SOLID,px(1),RGB(235,99,91));HGDIOBJ priorPen=SelectObject(dc,warning),priorBrush=SelectObject(dc,GetStockObject(NULL_BRUSH));
            Ellipse(dc,left,top,left+side,top+side);SelectObject(dc,priorBrush);SelectObject(dc,priorPen);DeleteObject(warning);
            r.left=left;r.right=left+side;r.top=top-px(1);r.bottom=top+side;SetTextColor(dc,RGB(235,99,91));SetBkMode(dc,TRANSPARENT);DrawTextW(dc,L"i",-1,&r,DT_SINGLELINE|DT_CENTER|DT_VCENTER|DT_NOPREFIX);SetBkMode(dc,OPAQUE);
        }else if(activeChat(c)){int asking=askingChat(c);elapsedText(c->startedAt,nowSeconds(),elapsed);r.left=px(194);r.right=px(asking?246:268);SetTextColor(dc,GetSysColor(COLOR_GRAYTEXT));DrawTextW(dc,elapsed,-1,&r,DT_SINGLELINE|DT_VCENTER|DT_RIGHT);
            if(asking){int side=px(13),left=px(252),top=y+(px(panelRowHeight)-side)/2;HPEN question=CreatePen(PS_SOLID,px(1),RGB(165,108,0));HGDIOBJ priorPen=SelectObject(dc,question),priorBrush=SelectObject(dc,GetStockObject(NULL_BRUSH));
                Ellipse(dc,left,top,left+side,top+side);SelectObject(dc,priorBrush);SelectObject(dc,priorPen);DeleteObject(question);
                r.left=left;r.right=left+side;r.top=top;r.bottom=top+side;SetTextColor(dc,RGB(165,108,0));SetBkMode(dc,TRANSPARENT);DrawTextW(dc,L"?",-1,&r,DT_SINGLELINE|DT_CENTER|DT_VCENTER|DT_NOPREFIX);SetBkMode(dc,OPAQUE);}
        }
        else {int dotY=y+(px(panelRowHeight)-px(8))/2;HBRUSH dot=CreateSolidBrush(RGB(52,132,255));HGDIOBJ oldBrush=SelectObject(dc,dot),dotPen=SelectObject(dc,GetStockObject(NULL_PEN));Ellipse(dc,px(257),dotY,px(265),dotY+px(8));SelectObject(dc,dotPen);SelectObject(dc,oldBrush);DeleteObject(dot);}
    }
    SetBkColor(dc,RGB(249,249,249));r=bounds;r.left=px(12);r.top=r.bottom-px(12);r.right-=px(12);SetTextColor(dc,GetSysColor(COLOR_GRAYTEXT));
    if(!readStateOk)DrawTextW(dc,L"읽음 상태를 동기화할 수 없습니다.",-1,&r,DT_SINGLELINE|DT_VCENTER);
    else if(panelCount>panelPage){WCHAR count[80];_snwprintf(count,80,L"%d–%d / %d  ·  휠로 보기",panelOffset+1,panelOffset+panelPage<panelCount?panelOffset+panelPage:panelCount,panelCount);DrawTextW(dc,count,-1,&r,DT_SINGLELINE|DT_VCENTER);}
    SelectObject(dc,oldFont);SelectObject(dc,oldPen);DeleteObject(line);
}
static void paintPanel(HWND h) {PAINTSTRUCT ps;RECT bounds;HDC dc=BeginPaint(h,&ps);GetClientRect(h,&bounds);drawPanel(dc,bounds);EndPaint(h,&ps);}
static LRESULT CALLBACK panelProc(HWND h,UINT msg,WPARAM w,LPARAM l) {
    if(msg==WM_PAINT){paintPanel(h);return 0;}
    if(msg==WM_ERASEBKGND)return 1;
    if(msg==WM_TIMER){repaintPanel();return 0;}
    if(msg==WM_LBUTTONUP){int x=(short)LOWORD(l),y=(short)HIWORD(l);if(x>=px(216)&&x<px(268)&&y>=px(4)&&y<px(panelHeaderHeight-4))menu();else openPanelSession(panelRowAt(x,y));return 0;}
    if(msg==WM_MOUSEMOVE){int hit=panelRowAt((short)LOWORD(l),(short)HIWORD(l));TRACKMOUSEEVENT track={sizeof(track),TME_LEAVE,h,0};TrackMouseEvent(&track);if(hit!=panelHover){panelHover=hit;showProjectTip(hit);InvalidateRect(h,NULL,FALSE);}SetCursor(LoadCursor(NULL,hit>=0?IDC_HAND:IDC_ARROW));return 0;}
    if(msg==WM_MOUSELEAVE){hideProjectTip();panelHover=-1;InvalidateRect(h,NULL,FALSE);return 0;}
    if(msg==WM_KEYDOWN&&w==VK_F10){menu();return 0;}
    if(msg==WM_MOUSEWHEEL){int delta=(short)HIWORD(w);hideProjectTip();panelHover=-1;panelOffset+=delta<0?1:-1;if(panelOffset<0)panelOffset=0;if(panelOffset>panelCount-panelPage)panelOffset=panelCount>panelPage?panelCount-panelPage:0;repaintPanel();return 0;}
    if(msg==WM_ACTIVATE&&LOWORD(w)==WA_INACTIVE&&!settingsOpen){DestroyWindow(h);return 0;}
    if(msg==WM_KEYDOWN&&w==VK_ESCAPE){DestroyWindow(h);return 0;}
    if(msg==WM_CLOSE){DestroyWindow(h);return 0;}
    if(msg==WM_DESTROY){hideProjectTip();KillTimer(h,3);if(panelFont)DeleteObject(panelFont);if(headerFont)DeleteObject(headerFont);panelFont=headerFont=NULL;panelHover=-1;panel=NULL;return 0;}
    return DefWindowProcW(h,msg,w,l);
}
static void togglePanel(void) {
    WNDCLASSW wc;POINT pt;HMONITOR monitor;MONITORINFO info;int height,x,y;
    if(panel){DestroyWindow(panel);return;}
    if(!desktopPid||!chats)return;
    updatePanelData();panelOffset=0;
    GetCursorPos(&pt);menuStyle(pt);
    memset(&wc,0,sizeof(wc));wc.style=CS_DROPSHADOW;wc.lpfnWndProc=panelProc;wc.hInstance=GetModuleHandleW(NULL);wc.lpszClassName=L"CodexPetSessionPanel";wc.hCursor=LoadCursor(NULL,IDC_ARROW);RegisterClassW(&wc);
    height=panelHeight();monitor=MonitorFromPoint(pt,MONITOR_DEFAULTTONEAREST);info.cbSize=sizeof(info);GetMonitorInfoW(monitor,&info);
    x=pt.x-px(280);y=pt.y-height-px(10);if(x<info.rcWork.left)x=info.rcWork.left;if(x+px(280)>info.rcWork.right)x=info.rcWork.right-px(280);if(y<info.rcWork.top)y=info.rcWork.top;
    panel=CreateWindowExW(WS_EX_TOOLWINDOW|WS_EX_TOPMOST,wc.lpszClassName,L"코덱스 세션",WS_POPUP|WS_BORDER,x,y,px(280),height,window,NULL,wc.hInstance,NULL);
    if(!panel){DeleteObject(panelFont);DeleteObject(headerFont);panelFont=headerFont=NULL;return;}
    roundPanel(panel);
    ShowWindow(panel,SW_SHOWNORMAL);SetForegroundWindow(panel);SetTimer(panel,3,1000,NULL);
}
static int panelOrderTest(FILE *f) {
    Chat sample[4]={{0}},*savedChats=chats;int savedCount=chatCount,failed=0,i;
    chats=sample;chatCount=2;panelCount=0;readStateOk=1;unreadCount=0;
    for(i=0;i<4;i++){sample[i].allowed=1;sample[i].mood=WORK;sample[i].latestAt=20-i*10;}
    strcpy(sample[0].id,"a");strcpy(sample[1].id,"b");strcpy(sample[2].id,"c");strcpy(sample[3].id,"b");
    refreshPanelRows(0);failed+=check(panelCount==2&&panelRows[0]==0&&panelRows[1]==1,"initial session order",f);
    sample[1].latestAt=40;refreshPanelRows(1);failed+=check(panelRows[0]==0&&panelRows[1]==1,"new activity cannot reorder open sessions",f);
    sample[0].mood=DONE;strcpy(unreadIds[0],"a");unreadCount=1;refreshPanelRows(1);failed+=check(panelRows[0]==0&&panelRows[1]==1,"completion keeps its existing position",f);
    sample[2].latestAt=100;chatCount=3;refreshPanelRows(1);failed+=check(panelCount==3&&panelRows[0]==0&&panelRows[1]==1&&panelRows[2]==2,"new session appended below existing rows",f);
    unreadCount=0;refreshPanelRows(1);failed+=check(panelCount==2&&panelRows[0]==1&&panelRows[1]==2,"reading a completion removes only that row",f);
    sample[3].latestAt=120;chatCount=4;refreshPanelRows(1);failed+=check(panelCount==2&&panelRows[0]==3&&panelRows[1]==2,"replacement rollout retains session position",f);
    sample[2].latestAt=200;refreshPanelRows(0);failed+=check(panelCount==2&&panelRows[0]==2&&panelRows[1]==3,"reopening can sort the current session snapshot",f);
    chats=savedChats;chatCount=savedCount;panelCount=0;return failed;
}
static int panelTest(void) {
    WCHAR path[1100],time[40];FILE *f;int failed=0;Chat c={0};char utf8[80];
    const char *before="{\"electron-thread-read-state-v1\":{\"unreadByIdentity\":{\"current\":{\"local:test\":[\"finished\"]},\"old-account\":{\"local:test\":[\"other\"]}}}}";
    const char *after="{\"electron-thread-read-state-v1\":{\"unreadByIdentity\":{\"current\":{\"local:test\":[]},\"old-account\":{\"local:test\":[\"finished\"]}}}}";
    _snwprintf(path,1100,L"%ls\\panel-verification.txt",folder);f=_wfopen(path,L"wb");if(!f)return 1;
    readStateOk=unreadFrom(before,"current","local:test");c.allowed=1;c.mood=DONE;strcpy(c.id,"finished");
    failed+=check(visibleChat(&c)&&isUnread("finished"),"unread completion has a blue dot",f);
    failed+=check(!isUnread("other"),"other accounts excluded",f);
    readStateOk=unreadFrom(after,"current","local:test");failed+=check(!visibleChat(&c),"Codex read removes completion even if another account is unread",f);
    c.mood=WORK;c.latestAt=nowSeconds();failed+=check(visibleChat(&c),"read active chat remains visible",f);
    failed+=check(!askingChat(&c),"working session has no question marker",f);c.mood=QUESTION;failed+=check(visibleChat(&c)&&askingChat(&c),"waiting question remains visible with a question marker",f);c.mood=WORK;failed+=check(!askingChat(&c),"answering question removes its marker",f);
    c.mood=IDLE;failed+=check(!visibleChat(&c),"read idle chat omitted",f);
    elapsedText(100,165,time);failed+=check(wcscmp(time,L"01:05")==0,"minute elapsed timer",f);
    elapsedText(100,3765,time);failed+=check(wcscmp(time,L"1:01:05")==0,"hour elapsed timer",f);
    stringValue("\"\\uC138\\uC158\"",utf8,sizeof(utf8));failed+=check(strcmp(utf8,"세션")==0,"escaped Korean title",f);
    failed+=check(shaIdentity("test-account","test-user",utf8),"Windows SHA256 identity fingerprint",f);
    failed+=panelOrderTest(f);
    {WCHAR url[96];failed+=check(sessionUrl("00000000-0000-0000-0000-000000000001",url)&&wcscmp(url,L"codex://threads/00000000-0000-0000-0000-000000000001")==0,"canonical local thread link",f);failed+=check(!sessionUrl("new",url)&&!sessionUrl("../../settings",url),"navigation accepts only a session UUID",f);}
    uiScale=100;panelHeaderHeight=30;panelRowHeight=22;panelCount=2;panelOffset=0;
    failed+=check(panelRowAt(20,29)==-1&&panelRowAt(20,30)==0&&panelRowAt(20,52)==1&&panelRowAt(20,74)==-1,"title click respects header and row boundaries",f);panelCount=0;
    fclose(f);return failed?1:0;
}
static int writePanelSnapshot(void) {
    WCHAR path[1100];FILE *f;int i;LOGFONTW font={0};if(panelFont)GetObjectW(panelFont,sizeof(font),&font);
    _snwprintf(path,1100,L"%ls\\panel-live-verification.json",folder);f=_wfopen(path,L"wb");if(!f)return 1;
    fprintf(f,"{\"readStateOk\":%s,\"unreadCount\":%d,\"uiScale\":%d,\"headerHeight\":%d,\"rowHeight\":%d,\"fontHeight\":%ld,\"fontQuality\":%u,\"rows\":[",readStateOk?"true":"false",unreadCount,uiScale,panelHeaderHeight,panelRowHeight,font.lfHeight,font.lfQuality);
    for(i=0;i<panelCount;i++){Chat *c=&chats[panelRows[i]];if(i)fprintf(f,",");fprintf(f,"{\"id\":\"%s\",\"active\":%s,\"question\":%s,\"unread\":%s,\"hasTitle\":%s,\"startedAt\":%lld,\"quotaBlocked\":%s,\"quotaAt\":%lld}",c->id,activeChat(c)?"true":"false",activeChat(c)&&c->mood==QUESTION?"true":"false",isUnread(c->id)?"true":"false",c->title[0]?"true":"false",c->startedAt,c->quotaBlocked?"true":"false",c->quotaAt);}
    fprintf(f,"]}");fclose(f);return readStateOk?0:1;
}
static int panelCheck(void) {int result;showPet();updatePanelData();result=writePanelSnapshot();hidePet();return result;}
