/* A loopback settings connection is created only when requested. No worker/runtime. */
#define WEB_CLIENTS 4
#define WEB_REQUEST_CAP 36864
#define BAD_SOCKET ((SOCKET)~(UINT_PTR)0)
typedef struct {short family;unsigned short port;DWORD address;char zero[8];} PetAddress;
typedef struct {SOCKET socket;char *input,*output;int used,headerLength,bodyLength,sent,outputLength;DWORD tick;} PetHttpClient;
static HMODULE webLibrary;
static SOCKET webListener=BAD_SOCKET;
static PetHttpClient webClients[WEB_CLIENTS];
static int webClosing,webStartupDone;
static DWORD webTouched;
static char webToken[33];
static int(WINAPI *petWSAStartup)(WORD,void*);
static int(WINAPI *petWSACleanup)(void);
static SOCKET(WINAPI *petSocket)(int,int,int);
static int(WINAPI *petBind)(SOCKET,const void*,int);
static int(WINAPI *petListen)(SOCKET,int);
static SOCKET(WINAPI *petAccept)(SOCKET,void*,int*);
static int(WINAPI *petCloseSocket)(SOCKET);
static int(WINAPI *petRecv)(SOCKET,char*,int,int);
static int(WINAPI *petSend)(SOCKET,const char*,int,int);
static int(WINAPI *petAsyncSelect)(SOCKET,HWND,unsigned int,long);
static int(WINAPI *petSocketName)(SOCKET,void*,int*);
static int(WINAPI *petSocketError)(void);
static void closeWebClient(PetHttpClient *c){
    if(c->socket!=BAD_SOCKET&&petCloseSocket)petCloseSocket(c->socket);
    if(c->input)VirtualFree(c->input,0,MEM_RELEASE);if(c->output)VirtualFree(c->output,0,MEM_RELEASE);
    memset(c,0,sizeof(*c));c->socket=BAD_SOCKET;
}
static void browserSettingsStop(void){int i;
    if(!webLibrary)return;for(i=0;i<WEB_CLIENTS;i++)closeWebClient(&webClients[i]);
    if(webListener!=BAD_SOCKET)petCloseSocket(webListener);webListener=BAD_SOCKET;webPort=0;webToken[0]=0;webClosing=0;
    if(webStartupDone&&petWSACleanup)petWSACleanup();webStartupDone=0;FreeLibrary(webLibrary);webLibrary=NULL;
}
static int makeWebToken(void){typedef BOOLEAN(WINAPI *RandomFn)(void*,ULONG);HMODULE lib=LoadLibraryW(L"advapi32.dll");RandomFn random;BYTE bytes[16];int i,ok=0;if(!lib)return 0;
    random=(RandomFn)GetProcAddress(lib,"SystemFunction036");if(random&&random(bytes,sizeof(bytes))){for(i=0;i<16;i++)sprintf(webToken+i*2,"%02x",bytes[i]);webToken[32]=0;ok=1;}FreeLibrary(lib);return ok;
}
static int browserSettingsStart(void){
    BYTE data[512];PetAddress address={0};int size=sizeof(address),i;
    if(webLibrary&&webListener!=BAD_SOCKET)return 1;
    webLibrary=LoadLibraryW(L"ws2_32.dll");if(!webLibrary)return 0;
#define WEB_FN(field,name) field=(void*)GetProcAddress(webLibrary,name);if(!field)goto failed;
    WEB_FN(petWSAStartup,"WSAStartup") WEB_FN(petWSACleanup,"WSACleanup") WEB_FN(petSocket,"socket") WEB_FN(petBind,"bind") WEB_FN(petListen,"listen") WEB_FN(petAccept,"accept") WEB_FN(petCloseSocket,"closesocket") WEB_FN(petRecv,"recv") WEB_FN(petSend,"send") WEB_FN(petAsyncSelect,"WSAAsyncSelect") WEB_FN(petSocketName,"getsockname") WEB_FN(petSocketError,"WSAGetLastError")
#undef WEB_FN
    for(i=0;i<WEB_CLIENTS;i++)webClients[i].socket=BAD_SOCKET;
    if(petWSAStartup(0x0202,data))goto failed;webStartupDone=1;if(!makeWebToken())goto failed;
    webListener=petSocket(2,1,6);if(webListener==BAD_SOCKET)goto failed;
    address.family=2;address.address=0x0100007f;
    if(petBind(webListener,&address,sizeof(address))||petListen(webListener,4)||petSocketName(webListener,&address,&size)||petAsyncSelect(webListener,window,WM_APP+23,8|32))goto failed;
    webPort=((address.port&255)<<8)|(address.port>>8);webTouched=GetTickCount();webClosing=0;return 1;
failed:browserSettingsStop();return 0;
}
static int httpHeader(const char *request,int length,const char *name,char *out,int cap){
    const char *p=strstr(request,"\r\n");int n=(int)strlen(name);if(!p)return 0;p+=2;
    while(p<request+length&&p[0]!='\r'){const char *end=strstr(p,"\r\n"),*value;if(!end||end>request+length)return 0;
        if(end-p>n&&p[n]==':'&&!_strnicmp(p,name,n)){int size;value=p+n+1;while(value<end&&*value==' ')value++;size=(int)(end-value);if(size>=cap)return 0;memcpy(out,value,size);out[size]=0;return 1;}p=end+2;}
    return 0;
}
static int validHttpOrigin(const char *request,int length,int post){char host[128],origin[128],expected[128],key[64];
    sprintf(expected,"127.0.0.1:%d",webPort);if(!httpHeader(request,length,"Host",host,sizeof(host))||strcmp(host,expected))return 0;
    if(httpHeader(request,length,"Origin",origin,sizeof(origin))){sprintf(expected,"http://127.0.0.1:%d",webPort);if(strcmp(origin,expected))return 0;}
    if(post&&(!httpHeader(request,length,"X-Pet-Key",key,sizeof(key))||strcmp(key,webToken)))return 0;
    return 1;
}
static void webReply(PetHttpClient *c,int code,const char *type,const void *body,int length){
    char header[1024];int h;const char *reason=code==200?"OK":code==400?"Bad Request":code==403?"Forbidden":code==404?"Not Found":"Internal Server Error";
    h=_snprintf(header,sizeof(header),"HTTP/1.1 %d %s\r\nContent-Type: %s\r\nContent-Length: %d\r\nConnection: close\r\nCache-Control: no-store\r\nX-Content-Type-Options: nosniff\r\nReferrer-Policy: no-referrer\r\nContent-Security-Policy: default-src 'self'; script-src 'self' 'unsafe-inline'; style-src 'self' 'unsafe-inline'; img-src 'self' blob: data:; connect-src 'self'; frame-ancestors 'none'\r\n\r\n",code,reason,type,length);
    if(h<0){closeWebClient(c);return;}c->output=VirtualAlloc(NULL,h+length,MEM_RESERVE|MEM_COMMIT,PAGE_READWRITE);if(!c->output){closeWebClient(c);return;}
    memcpy(c->output,header,h);if(length)memcpy(c->output+h,body,length);c->outputLength=h+length;c->sent=0;
}
static void webError(PetHttpClient *c,int code,const char *message){webReply(c,code,"text/plain; charset=utf-8",message,(int)strlen(message));}
static int jsonInt(const char *text,const char *name,int *out){const char *v=member(text,name),*end;char *parsed;long value;if(!v)return 0;value=strtol(v,&parsed,10);end=ws(parsed);if(parsed==v||(*end!=','&&*end!='}'))return 0;*out=(int)value;return value>=-2147483647&&value<=2147483647;}
static int customFileMask(void){WCHAR path[1100];WIN32_FILE_ATTRIBUTE_DATA attr;int slot,mask=0;for(slot=-1;slot<=8;slot++){customPath(slot,path);if(GetFileAttributesExW(path,GetFileExInfoStandard,&attr)&&!(attr.dwFileAttributes&FILE_ATTRIBUTE_DIRECTORY)&&attr.nFileSizeHigh==0&&attr.nFileSizeLow>=4112&&attr.nFileSizeLow<=32812)mask|=1<<(slot+1);}return mask;}
static int customFileAvailable(void){return customFileMask()!=0;}
static void webState(PetHttpClient *c){char json[512];int n=_snprintf(json,sizeof(json),"{\"language\":%d,\"badgeColor\":%d,\"animation\":%d,\"customIcon\":%d,\"customAvailable\":%s,\"customFrames\":%d,\"customPixelBytes\":%d,\"customSlots\":%d,\"customLoadedSlot\":%d,\"state\":%d,\"motion\":%d,\"finishedUnread\":%d,\"manual\":%d,\"manualMotion\":%d}",uiLanguage,badgeStyle,animation,customEnabled,customFileAvailable()?"true":"false",customCount,customCount*4096,customFileMask(),customLoadedSlot,mood,motion,finishedUnread,manual,manualMotion);webReply(c,200,"application/json; charset=utf-8",json,n);}
static void webRenderedPreview(PetHttpClient *c){
    BYTE bytes[12+8*4100]={0};DWORD pixels[1024],frames,duration;int i,j,shown=manual>=0?manual:mood,count=manualMotion==3?3:finishedUnread,custom=customEnabled&&customCount;
    int fallbackUrgent=motion==1&&customLoadedSlot!=7;frames=custom?(fallbackUrgent?4:customCount):motion==1||motion==2?4:2;if(!animation)frames=1;memcpy(bytes,"CPETAN1",8);memcpy(bytes+8,&frames,4);
    for(i=0;i<frames;i++){HICON base,icon;int owned=0;duration=custom&&!fallbackUrgent?customFrames[i].duration:motion==2?600:300;memcpy(bytes+12+i*4100,&duration,4);
        if(custom){icon=makeCustomFrame(i,shown,count,motion==1,motion==2,0);owned=1;}
        else {base=motion==1?extraIcons[0][i]:motion==2?extraIcons[1][i]:icons[shown][i];icon=count?badgeIcon(base,count,0):base;owned=count!=0;}
        if(!icon||!copyIconPixels(icon,pixels)){if(icon&&owned)DestroyIcon(icon);webError(c,500,"Preview failed");return;}if(owned)DestroyIcon(icon);
        for(j=0;j<1024;j++){DWORD p=pixels[j],a=p>>24;BYTE *rgba=bytes+16+i*4100+j*4;rgba[0]=a?((p>>16&255)*255+a/2)/a:0;rgba[1]=a?((p>>8&255)*255+a/2)/a:0;rgba[2]=a?((p&255)*255+a/2)/a:0;rgba[3]=a;}
    }webReply(c,200,"application/octet-stream",bytes,12+frames*4100);
}
static int writeOwnFile(const WCHAR *name,const BYTE *data,int length){WCHAR path[1100],temporary[1100];FILE *f;int ok;_snwprintf(path,1100,L"%ls\\%ls",folder,name);_snwprintf(temporary,1100,L"%ls\\%ls.tmp",folder,name);f=_wfopen(temporary,L"wb");if(!f)return 0;ok=fwrite(data,1,length,f)==length;if(fclose(f))ok=0;if(!ok||!MoveFileExW(temporary,path,MOVEFILE_REPLACE_EXISTING)){DeleteFileW(temporary);return 0;}return 1;}
static void webDispatch(PetHttpClient *c){
    char method[8],path[256],version[16],prefix[40];char *route,*body=c->input+c->headerLength;int post,bodyLength=c->bodyLength;
    if(sscanf(c->input,"%7s %255s %15s",method,path,version)!=3||strcmp(version,"HTTP/1.1")){webError(c,400,"Invalid request");return;}
    post=!strcmp(method,"POST");if(strcmp(method,"GET")&&!post){webError(c,400,"Unsupported method");return;}
    sprintf(prefix,"/%s/",webToken);if(strncmp(path,prefix,strlen(prefix))||!validHttpOrigin(c->input,c->headerLength,post)){webError(c,403,"Settings access denied");return;}
    route=path+strlen(prefix);webTouched=GetTickCount();
    if(!post&&!route[0]){WCHAR file[1100];char *html;_snwprintf(file,1100,L"%ls\\settings.html",folder);html=textFile(file);if(!html){webError(c,500,"Settings page missing");return;}if(strlen(html)>65536){VirtualFree(html,0,MEM_RELEASE);webError(c,500,"Settings page too large");return;}webReply(c,200,"text/html; charset=utf-8",html,(int)strlen(html));VirtualFree(html,0,MEM_RELEASE);return;}
    if(!post&&!strcmp(route,"localization.js")){WCHAR file[1100];char *js;_snwprintf(file,1100,L"%ls\\localization.js",folder);js=textFile(file);if(!js){webError(c,500,"Localization missing");return;}if(strlen(js)>262144){VirtualFree(js,0,MEM_RELEASE);webError(c,500,"Localization too large");return;}webReply(c,200,"text/javascript; charset=utf-8",js,(int)strlen(js));VirtualFree(js,0,MEM_RELEASE);return;}
    if(post&&!strcmp(route,"api/language")){int language;if(!jsonInt(body,"language",&language)||!setUiLanguage(language)){webError(c,400,"Language could not be saved");return;}refresh(0);report();if(panel)InvalidateRect(panel,NULL,FALSE);webState(c);return;}
    if(!post&&!strcmp(route,"api/state")){webState(c);return;}
    if(!post&&!strcmp(route,"api/rendered")){webRenderedPreview(c);return;}
    if(!post&&!strncmp(route,"api/preview",11)){DWORD pixels[1024];BYTE rgba[4096];HICON icon;int custom=0,state=manual>=0?manual:mood,count=manualMotion==3?3:finishedUnread,i;
        if(strstr(route,"custom=1"))custom=1;if(strstr(route,"state=")){state=atoi(strstr(route,"state=")+6);if(state<0||state>6){webError(c,400,"Invalid state");return;}}
        if(custom&&customCount)icon=makeCustomFrame(0,state,count,motion==1,motion==2,0);else icon=makeIcon(state,0);
        if(!icon||!copyIconPixels(icon,pixels)){if(icon)DestroyIcon(icon);webError(c,500,"Preview failed");return;}DestroyIcon(icon);
        for(i=0;i<1024;i++){DWORD p=pixels[i],a=p>>24;rgba[i*4]=a?((p>>16&255)*255+a/2)/a:0;rgba[i*4+1]=a?((p>>8&255)*255+a/2)/a:0;rgba[i*4+2]=a?((p&255)*255+a/2)/a:0;rgba[i*4+3]=a;}webReply(c,200,"application/octet-stream",rgba,4096);return;}
    if(!post&&!strcmp(route,"api/frames")){WCHAR path[1100];FILE *f;BYTE *data;long length;_snwprintf(path,1100,L"%ls\\custom-animation.bin",folder);f=_wfopen(path,L"rb");if(!f){webError(c,404,"No custom icon");return;}fseek(f,0,SEEK_END);length=ftell(f);fseek(f,0,SEEK_SET);if(length<12||length>32812){fclose(f);webError(c,400,"Invalid saved animation");return;}data=VirtualAlloc(NULL,length,MEM_RESERVE|MEM_COMMIT,PAGE_READWRITE);if(!data){fclose(f);webError(c,500,"Memory unavailable");return;}if(fread(data,1,length,f)!=length||!animationValid(data,length))webError(c,400,"Invalid saved animation");else webReply(c,200,"application/octet-stream",data,length);VirtualFree(data,0,MEM_RELEASE);fclose(f);return;}
    if(post&&!strcmp(route,"api/settings")){int color,moving,custom;if(!jsonInt(body,"badgeColor",&color)||!jsonInt(body,"animation",&moving)||!jsonInt(body,"customIcon",&custom)||custom&&!customFileAvailable()||!savePetPreferences(color,moving,custom)){webError(c,400,"Settings could not be saved");return;}
        badgeStyle=color;animation=moving;customEnabled=custom;clearBadgeIcons();loadCustomFrames();SetTimer(window,ID_ANIM,customEnabled&&customCount>1?150:300,NULL);refresh(0);report();webState(c);return;}
    if(post&&(!strcmp(route,"api/icon")||!strncmp(route,"api/icon?slot=",14))){int slot=-1;WCHAR name[64];char *end;if(strcmp(route,"api/icon")){long value=strtol(route+14,&end,10);if(end==route+14||*end||value < -1||value>8){webError(c,400,"Invalid icon state");return;}slot=(int)value;}if(!animationValid((BYTE*)body,bodyLength)){webError(c,400,"Use 1-8 frames per state, 32x32 RGBA, 150-2000ms per frame");return;}
        if(slot==-1)wcscpy(name,L"custom-animation.bin");else _snwprintf(name,64,L"custom-animation-%d.bin",slot);
        if(!writeOwnFile(name,(BYTE*)body,bodyLength)||!savePetPreferences(badgeStyle,animation,1)){webError(c,500,"Icon could not be saved");return;}customEnabled=1;loadCustomFrames();refresh(0);SetTimer(window,ID_ANIM,customCount>1?150:300,NULL);report();webState(c);return;}
    if(post&&!strcmp(route,"api/action")){int choice;if(!jsonInt(body,"command",&choice)||!(choice==10||choice==11||choice==13||choice==14||choice==15||choice>=30&&choice<37||choice>=70&&choice<=72)){webError(c,400,"Invalid action");return;}webReply(c,200,"application/json", "{\"ok\":true}",11);PostMessageW(window,WM_APP+24,choice,0);return;}
    if(post&&!strcmp(route,"api/close")){webReply(c,200,"application/json","{\"ok\":true}",11);webClosing=1;return;}
    webError(c,404,"Page not found");
}
static void webFlush(PetHttpClient *c){while(c->socket!=BAD_SOCKET&&c->output&&c->sent<c->outputLength){int sent=petSend(c->socket,c->output+c->sent,c->outputLength-c->sent,0);if(sent>0)c->sent+=sent;else{if(petSocketError()!=10035)closeWebClient(c);return;}}if(c->socket!=BAD_SOCKET&&c->output&&c->sent==c->outputLength)closeWebClient(c);}
static void webReceive(PetHttpClient *c){int received;
    if(c->output){webFlush(c);return;}while(c->used<WEB_REQUEST_CAP-1){received=petRecv(c->socket,c->input+c->used,WEB_REQUEST_CAP-1-c->used,0);if(received<=0){if(!received||petSocketError()!=10035)closeWebClient(c);return;}c->used+=received;c->input[c->used]=0;
        if(!c->headerLength){char *end=strstr(c->input,"\r\n\r\n");if(end){char sizeText[24],encoding[64],*tail;long size=0;c->headerLength=(int)(end-c->input)+4;if(c->headerLength>4096||httpHeader(c->input,c->headerLength,"Transfer-Encoding",encoding,sizeof(encoding))){webError(c,400,"Invalid headers");break;}
            if(httpHeader(c->input,c->headerLength,"Content-Length",sizeText,sizeof(sizeText))){size=strtol(sizeText,&tail,10);if(*tail||size<0||size>32812){webError(c,400,"Invalid length");break;}}c->bodyLength=(int)size;}
            else if(c->used>4096){webError(c,400,"Headers too large");break;}}
        if(c->headerLength&&c->used>=c->headerLength+c->bodyLength){c->input[c->headerLength+c->bodyLength]=0;webDispatch(c);break;}}
    if(c->socket!=BAD_SOCKET&&c->used>=WEB_REQUEST_CAP-1&&!c->output)webError(c,400,"Request too large");if(c->socket!=BAD_SOCKET&&c->output)webFlush(c);
}
static void browserSocketEvent(SOCKET socket,LPARAM event){int i;if(!webLibrary)return;
    if(socket==webListener){if(LOWORD(event)==8&&!HIWORD(event)){PetAddress address;int size=sizeof(address);SOCKET accepted=petAccept(webListener,&address,&size);if(accepted==BAD_SOCKET)return;
            if(address.address!=0x0100007f){petCloseSocket(accepted);return;}for(i=0;i<WEB_CLIENTS;i++)if(webClients[i].socket==BAD_SOCKET)break;if(i==WEB_CLIENTS){petCloseSocket(accepted);return;}
            webClients[i].socket=accepted;webClients[i].tick=GetTickCount();webClients[i].input=VirtualAlloc(NULL,WEB_REQUEST_CAP,MEM_RESERVE|MEM_COMMIT,PAGE_READWRITE);if(!webClients[i].input||petAsyncSelect(accepted,window,WM_APP+23,1|2|32))closeWebClient(&webClients[i]);}return;}
    for(i=0;i<WEB_CLIENTS;i++)if(webClients[i].socket==socket){if(HIWORD(event)||LOWORD(event)==32)closeWebClient(&webClients[i]);else if(LOWORD(event)==1)webReceive(&webClients[i]);else if(LOWORD(event)==2)webFlush(&webClients[i]);break;}
}
static void browserSettingsPoll(void){int i,open=0;if(!webLibrary)return;for(i=0;i<WEB_CLIENTS;i++)if(webClients[i].socket!=BAD_SOCKET){if(GetTickCount()-webClients[i].tick>10000)closeWebClient(&webClients[i]);else open++;}
    if(webClosing&&!open||GetTickCount()-webTouched>600000){browserSettingsStop();report();}
}
static int launchSettingsBrowser(const WCHAR *url){WCHAR chrome[1024],root[900],parameters[400];int found=0;
    if(GetEnvironmentVariableW(L"PROGRAMFILES",root,900)){_snwprintf(chrome,1024,L"%ls\\Google\\Chrome\\Application\\chrome.exe",root);found=GetFileAttributesW(chrome)!=INVALID_FILE_ATTRIBUTES;}
    if(!found&&GetEnvironmentVariableW(L"LOCALAPPDATA",root,900)){_snwprintf(chrome,1024,L"%ls\\Google\\Chrome\\Application\\chrome.exe",root);found=GetFileAttributesW(chrome)!=INVALID_FILE_ATTRIBUTES;}
    if(!found&&GetEnvironmentVariableW(L"PROGRAMFILES(X86)",root,900)){_snwprintf(chrome,1024,L"%ls\\Google\\Chrome\\Application\\chrome.exe",root);found=GetFileAttributesW(chrome)!=INVALID_FILE_ATTRIBUTES;}
    if(found){_snwprintf(parameters,400,L"--new-tab \"%ls\"",url);return (INT_PTR)ShellExecuteW(NULL,L"open",chrome,parameters,NULL,SW_SHOWNORMAL)>32;}return (INT_PTR)ShellExecuteW(NULL,L"open",url,NULL,NULL,SW_SHOWNORMAL)>32;
}
static void openBrowserSettings(int iconSection){WCHAR url[256],executable[1024],command[1500];STARTUPINFOW startup={0};PROCESS_INFORMATION child={0};
    if(!browserSettingsStart()){MessageBoxW(panel?panel:window,tr(TXT_browserFailed),tr(TXT_app),MB_OK|MB_ICONERROR);return;}
    webTouched=GetTickCount();_snwprintf(url,256,L"http://127.0.0.1:%d/%hs/%ls",webPort,webToken,iconSection?L"#icon":L"");
    GetModuleFileNameW(NULL,executable,1024);_snwprintf(command,1500,L"\"%ls\" --open-settings-url \"%ls\"",executable,url);startup.cb=sizeof(startup);startup.dwFlags=STARTF_USESHOWWINDOW;startup.wShowWindow=SW_HIDE;
    /* URI/Chrome activation DLLs belong to this short-lived helper. */
    if(CreateProcessW(executable,command,NULL,NULL,FALSE,CREATE_NO_WINDOW,NULL,folder,&startup,&child)){CloseHandle(child.hThread);CloseHandle(child.hProcess);}else MessageBoxW(panel?panel:window,tr(TXT_launchBrowserFailed),tr(TXT_app),MB_OK|MB_ICONERROR);
    report();
}
