static int quotaCode(const char *code) {
    char normalized[96];int i,n=0;for(i=0;code[i]&&n<95;i++){char c=code[i];if(c>='A'&&c<='Z')c+=32;if(c!='_'&&c!='-'&&c!=' ')normalized[n++]=c;}normalized[n]=0;
    return !strcmp(normalized,"usagelimitexceeded")||!strcmp(normalized,"usagelimitreached")||!strcmp(normalized,"insufficientquota");
}
static void extractError(const char *p,Record *r) {
    const char *v=member(p,"codexErrorInfo");if(!v)v=member(p,"codex_error_info");if(!v)v=member(p,"code");
    if(v&&*v=='"')stringValue(v,r->errorCode,sizeof(r->errorCode));
    v=member(p,"message");if(v&&*v=='"')stringValue(v,r->errorMessage,sizeof(r->errorMessage));
}
static int quotaMessage(const char *message) {
    return !strncmp(message,"You've hit your usage limit",27)||!strncmp(message,"Turn error: You've hit your usage limit",39)||
        !strncmp(message,"You\xe2\x80\x99ve hit your usage limit",29)||
        strstr(message,"startup websocket prewarm setup failed: You\xe2\x80\x99ve hit your usage limit.")!=NULL||
        strstr(message,"Turn error: You\xe2\x80\x99ve hit your usage limit.")!=NULL||
        strstr(message,"\"code\":\"usage_limit_reached\"")!=NULL||strstr(message,"\"code\":\"usage_limit_exceeded\"")!=NULL||strstr(message,"codexErrorInfo: UsageLimitExceeded")!=NULL;
}
static int quotaToolFailure(const char *message) {
    const char *start="Script error:\nexec_command failed: CreateProcess { message: \"Rejected(";
    return !strncmp(message,start,strlen(start))&&strstr(message,"Automatic approval review failed:")&&
        (strstr(message,"You\xe2\x80\x99ve hit your usage limit")||strstr(message,"You've hit your usage limit"));
}
static void saveQuotaState(void) {
    WCHAR path[1100],temp[1100];FILE *f;int i,ok;
    if(!quotaStateDirty||!chats)return;_snwprintf(path,1100,L"%ls\\quota-blocked.txt",folder);_snwprintf(temp,1100,L"%ls\\quota-blocked.tmp",folder);
    f=_wfopen(temp,L"wb");if(!f)return;fprintf(f,"ack %lld\n",quotaAckAt);
    for(i=0;i<chatCount;i++)if(latestChat(i)&&chats[i].quotaBlocked)fprintf(f,"%s %lld %d\n",chats[i].id,chats[i].quotaAt,chats[i].quotaUnread);
    ok=!ferror(f);if(fclose(f)!=0)ok=0;if(ok&&MoveFileExW(temp,path,MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH))quotaStateDirty=0;
}
static void loadQuotaState(void) {
    WCHAR path[1100],url[96];FILE *f;char id[64],record[160];LONGLONG at;int i,n=0,wasUnread=0;
    _snwprintf(path,1100,L"%ls\\quota-blocked.txt",folder);f=_wfopen(path,L"rb");if(!f)return;
    if(fgets(record,sizeof(record),f)&&sscanf(record,"ack %lld",&at)==1&&at>=0)quotaAckAt=at;
    while(n++<MAX_CHATS&&fgets(record,sizeof(record),f)){wasUnread=0;if(sscanf(record,"%63s %lld %d",id,&at,&wasUnread)>=2&&at>0&&sessionUrl(id,url)){
        discoverSessionId(id);for(i=0;i<chatCount;i++)if(latestChat(i)&&!strcmp(chats[i].id,id)&&chats[i].path[0]){
            if(chats[i].startedAt>at||chats[i].finished&&chats[i].latestAt>at){quotaStateDirty=1;break;}
            if(readStateOk&&wasUnread&&!isUnread(id)){quotaStateDirty=1;break;}
            chats[i].quotaBlocked=1;chats[i].quotaAt=at;chats[i].quotaUnread=wasUnread||isUnread(id);chats[i].mood=STOPPED;chats[i].finished=chats[i].ready=0;break;
        }
    }}fclose(f);
}
static void syncQuotaReadState(void) {
    int i;if(!readStateOk)return;
    for(i=0;i<chatCount;i++)if(latestChat(i)&&chats[i].quotaBlocked){int unread=isUnread(chats[i].id);
        if(chats[i].quotaUnread&&!unread){chats[i].quotaBlocked=chats[i].quotaUnread=0;chats[i].quotaAt=0;if(chats[i].mood==STOPPED)chats[i].mood=IDLE;quotaStateDirty=1;}
        else if(!chats[i].quotaUnread&&unread){chats[i].quotaUnread=1;quotaStateDirty=1;}
    }
}
static void applyQuotaLog(const char *id,LONGLONG at) {
    int i;if(!id||at<=quotaAckAt)return;
    for(i=0;i<chatCount;i++)if(latestChat(i)&&!strcmp(chats[i].id,id)&&chats[i].startedAt<=at&&(activeChat(&chats[i])||isUnread(id))){
        if(chats[i].finished&&chats[i].latestAt>at)break;
        if(!chats[i].quotaBlocked||chats[i].quotaAt!=at)quotaStateDirty=1;chats[i].quotaBlocked=1;chats[i].quotaAt=at;chats[i].quotaUnread=isUnread(id);chats[i].mood=STOPPED;chats[i].finished=chats[i].ready=0;if(at>quotaLogAt)quotaLogAt=at;break;}
}
static void pollQuotaLog(void) {
    PetDb *db;PetStmt *s=NULL;int i,rows=0;LONGLONG previous=quotaCursor;
    quotaDbOk=0;db=dbRead(L"logs_2.sqlite");if(!db)return;
    if(!quotaBootDone){
        /* Indexed per-thread startup recovery catches quota errors even after quiet time. */
        if(sqlPrepare(db,"SELECT ts,target,feedback_log_body FROM logs WHERE thread_id=? AND ts>=? ORDER BY ts DESC,ts_nanos DESC,id DESC LIMIT 128",-1,&s,NULL)==0){
            for(i=0;i<chatCount;i++)if(latestChat(i)&&(activeChat(&chats[i])||isUnread(chats[i].id))){int n;sqlFinalize(s);s=NULL;
                if(sqlPrepare(db,"SELECT ts,target,feedback_log_body FROM (SELECT ts,ts_nanos,id,level,target,feedback_log_body FROM logs WHERE thread_id=? AND ts>=? ORDER BY ts DESC,ts_nanos DESC,id DESC LIMIT 128) WHERE level IN ('WARN','ERROR')",-1,&s,NULL)!=0)break;
                sqlBindText(s,1,chats[i].id,-1,NULL);sqlBindInt(s,2,chats[i].startedAt?chats[i].startedAt:desktopStartedAt);
                for(n=0;n<128&&sqlStep(s)==100;n++){const char *target=(const char*)sqlText(s,1),*body=(const char*)sqlText(s,2);if(target&&!strncmp(target,"codex_core",10)&&body&&quotaMessage(body)){applyQuotaLog(chats[i].id,sqlInt(s,0));break;}}
            }quotaBootDone=1;
        }if(s)sqlFinalize(s);s=NULL;
    }
    if(!quotaCursor){if(sqlPrepare(db,"SELECT max(id) FROM logs",-1,&s,NULL)==0&&sqlStep(s)==100)quotaCursor=sqlInt(s,0)>256?sqlInt(s,0)-256:0;if(s)sqlFinalize(s);s=NULL;}
    if(sqlPrepare(db,"SELECT id,ts,level,target,feedback_log_body,thread_id FROM logs WHERE id>? ORDER BY id LIMIT 512",-1,&s,NULL)==0) {
        sqlBindInt(s,1,quotaCursor);
        while(sqlStep(s)==100){const char *level=(const char*)sqlText(s,2),*target=(const char*)sqlText(s,3),*body=(const char*)sqlText(s,4),*id=(const char*)sqlText(s,5);LONGLONG at=sqlInt(s,1);quotaCursor=sqlInt(s,0);rows++;
            if(at<desktopStartedAt||!level||strcmp(level,"ERROR")&&strcmp(level,"WARN")||!target||strncmp(target,"codex_core",10)||!body||!id||!quotaMessage(body))continue;
            applyQuotaLog(id,at);
        }quotaDbOk=1;
    }
    if(s)sqlFinalize(s);sqlClose(db);
    /* Busy databases are retried at the same cursor; no full-file scanning. */
    if(!quotaDbOk)quotaCursor=previous;
}
static HICON iconPixels(DWORD *pixels) {
    BITMAPINFO bi={0};DWORD *dest;HBITMAP color,mask;ICONINFO ii={0};HICON result;BYTE bits[128]={0};
    bi.bmiHeader.biSize=sizeof(BITMAPINFOHEADER);bi.bmiHeader.biWidth=32;bi.bmiHeader.biHeight=-32;bi.bmiHeader.biPlanes=1;bi.bmiHeader.biBitCount=32;
    color=CreateDIBSection(NULL,&bi,DIB_RGB_COLORS,(void**)&dest,NULL,0);if(!color)return NULL;memcpy(dest,pixels,4096);mask=CreateBitmap(32,32,1,1,bits);ii.fIcon=TRUE;ii.hbmColor=color;ii.hbmMask=mask;result=CreateIconIndirect(&ii);DeleteObject(color);DeleteObject(mask);return result;
}
static int copyIconPixels(HICON icon,DWORD pixels[1024]) {
    ICONINFO ii;BITMAPINFO bi={0};HDC dc;int ok;if(!GetIconInfo(icon,&ii))return 0;bi.bmiHeader.biSize=sizeof(BITMAPINFOHEADER);bi.bmiHeader.biWidth=32;bi.bmiHeader.biHeight=-32;bi.bmiHeader.biPlanes=1;bi.bmiHeader.biBitCount=32;dc=GetDC(NULL);ok=GetDIBits(dc,ii.hbmColor,0,32,pixels,&bi,DIB_RGB_COLORS)!=0;ReleaseDC(NULL,dc);DeleteObject(ii.hbmColor);DeleteObject(ii.hbmMask);return ok;
}
static HICON makeMotionIcon(int mode,int frame) {
    DWORD source[1024],dest[1024]={0};HICON base=makeIcon(WORK,0);int x,y,shift=frame==1?-2:frame==3?2:0;DWORD tint=mode==0?(frame&1?0xffffbbbb:0xffff293f):0xff6489ef;
    if(!base)return NULL;if(!copyIconPixels(base,source)){DestroyIcon(base);return NULL;}DestroyIcon(base);
    for(y=0;y<32;y++)for(x=0;x<32;x++)if(source[y*32+x])pixel(dest,x,y+shift,source[y*32+x]==0xff161f2d?source[y*32+x]:tint);
    if(mode==1){for(x=2;x<=10;x++){pixel(dest,x,20,0xff17233b);pixel(dest,x,30,0xff17233b);}for(y=21;y<30;y++){int width=abs(y-25);for(x=6-width;x<=6+width;x++)pixel(dest,x,y,0xffffcb55);}for(y=21;y<30;y++){pixel(dest,2+((y-21)<4?y-21:29-y),y,0xff17233b);pixel(dest,10-((y-21)<4?y-21:29-y),y,0xff17233b);}}
    return iconPixels(dest);
}
static void clearBadgeIcons(void){int i;for(i=0;i<4;i++){if(badgeIcons[i])DestroyIcon(badgeIcons[i]);badgeIcons[i]=NULL;}badgeKey=badgeBaseKey=-1;badgeFrames=0;}
static const DWORD badgeColors[]={0x2f3038,0xe5484d,0x2979ff,0x168b5b,0x8054d9,0xffffff};
static void loadBadgeStyle(void){
    WCHAR path[1100];FILE *f;char text[64];int value;
    badgeStyle=1;animation=1;customEnabled=0;_snwprintf(path,1100,L"%ls\\pet-settings.txt",folder);f=_wfopen(path,L"rb");if(!f)return;
    while(fgets(text,sizeof(text),f)){if(sscanf(text,"badgeColor=%d",&value)==1&&value>=0&&value<6)badgeStyle=value;
        else if(sscanf(text,"animation=%d",&value)==1&&(value==0||value==1))animation=value;
        else if(sscanf(text,"customIcon=%d",&value)==1&&(value==0||value==1))customEnabled=value;}
    fclose(f);
}
static int savePetPreferences(int value,int moving,int custom){
    WCHAR path[1100],temporary[1100];FILE *f;int ok;
    if(value<0||value>=6||moving<0||moving>1||custom<0||custom>1)return 0;
    _snwprintf(path,1100,L"%ls\\pet-settings.txt",folder);_snwprintf(temporary,1100,L"%ls\\pet-settings.tmp",folder);
    f=_wfopen(temporary,L"wb");if(!f)return 0;ok=fprintf(f,"badgeColor=%d\nanimation=%d\ncustomIcon=%d\n",value,moving,custom)>0;if(fclose(f))ok=0;
    if(!ok||!MoveFileExW(temporary,path,MOVEFILE_REPLACE_EXISTING)){DeleteFileW(temporary);return 0;}
    return 1;
}
static int setBadgeStyle(int value){if(!savePetPreferences(value,animation,customEnabled))return 0;if(badgeStyle!=value){badgeStyle=value;clearBadgeIcons();}return 1;}
static BYTE badgeNumberMask[256];static int badgeNumberKey=-1;
/* Rasterize only on cache changes, then keep a 256-byte grayscale mask. */
static int makeBadgeNumber(int count) {
    BITMAPINFO bi={0};DWORD *p;HBITMAP bitmap;HDC dc;HFONT font;HGDIOBJ oldBitmap,oldFont;RECT r={0,0,64,64};WCHAR text[8];
    int x,y,sx,sy,left=64,right=-1,top=64,bottom=-1,shiftX,shiftY;
    if(badgeNumberKey==count)return 1;bi.bmiHeader.biSize=sizeof(BITMAPINFOHEADER);bi.bmiHeader.biWidth=64;bi.bmiHeader.biHeight=-64;bi.bmiHeader.biPlanes=1;bi.bmiHeader.biBitCount=32;
    bitmap=CreateDIBSection(NULL,&bi,DIB_RGB_COLORS,(void**)&p,NULL,0);if(!bitmap)return 0;memset(p,0,64*64*4);
    dc=CreateCompatibleDC(NULL);font=CreateFontW(-48,0,0,0,FW_NORMAL,FALSE,FALSE,FALSE,DEFAULT_CHARSET,OUT_TT_ONLY_PRECIS,CLIP_DEFAULT_PRECIS,ANTIALIASED_QUALITY,DEFAULT_PITCH,L"Segoe UI");
    if(!dc||!font){if(dc)DeleteDC(dc);if(font)DeleteObject(font);DeleteObject(bitmap);return 0;}
    oldBitmap=SelectObject(dc,bitmap);oldFont=SelectObject(dc,font);SetBkMode(dc,OPAQUE);SetBkColor(dc,RGB(0,0,0));SetTextColor(dc,RGB(255,255,255));
    if(count>=10)wcscpy(text,L"9+");else _snwprintf(text,8,L"%d",count);
    DrawTextW(dc,text,-1,&r,DT_SINGLELINE|DT_CENTER|DT_VCENTER|DT_NOPREFIX);GdiFlush();
    for(y=0;y<64;y++)for(x=0;x<64;x++)if((p[y*64+x]&255)>0){if(x<left)left=x;if(x>right)right=x;if(y<top)top=y;if(y>bottom)bottom=y;}
    shiftX=(64-left-right-1)/2;shiftY=(64-top-bottom-1)/2;
    memset(badgeNumberMask,0,sizeof(badgeNumberMask));
    if(right>=left)for(y=0;y<16;y++)for(x=0;x<16;x++){int sum=0;for(sy=0;sy<4;sy++)for(sx=0;sx<4;sx++){int xx=x*4+sx-shiftX,yy=y*4+sy-shiftY;if(xx>=0&&xx<64&&yy>=0&&yy<64)sum+=p[yy*64+xx]&255;}badgeNumberMask[y*16+x]=(BYTE)((sum+8)/16);}
    SelectObject(dc,oldFont);SelectObject(dc,oldBitmap);DeleteObject(font);DeleteDC(dc);DeleteObject(bitmap);badgeNumberKey=count;return right>=left;
}
static HICON coloredBadgeIcon(HICON base,int count,int style,int bright) {
    DWORD p[1024],fill=badgeColors[style],ink=style==5?0x25262d:0xffffff;int x,y,sx,sy;
    if(!copyIconPixels(base,p)||!makeBadgeNumber(count))return NULL;
    if(bright&&style!=5)fill=((fill&0xfefefe)>>1)+0x505050;
    for(y=0;y<16;y++)for(x=0;x<16;x++){
        int covered=0,inner=0,alpha,text;DWORD old=p[y*32+x+16],color=fill,result;int channel;
        for(sy=0;sy<4;sy++)for(sx=0;sx<4;sx++){int dx=x*8+sx*2+1-64,dy=y*8+sy*2+1-64;if(dx*dx+dy*dy<=4096)covered++;if(dx*dx+dy*dy<=3481)inner++;}
        if(!covered)continue;alpha=(covered*255+8)/16;text=badgeNumberMask[y*16+x];
        if(style==5&&inner<covered){int weight=inner*255/covered;DWORD edge=0xd1d3d9;color=0;for(channel=0;channel<3;channel++){int shift=channel*8;color|=((((fill>>shift)&255)*weight+((edge>>shift)&255)*(255-weight)+127)/255)<<shift;}}
        result=(alpha+((old>>24)*(255-alpha)+127)/255)<<24;
        for(channel=0;channel<3;channel++){int shift=channel*8,straight=(((color>>shift)&255)*(255-text)+((ink>>shift)&255)*text+127)/255;result|=((straight*alpha+((old>>shift)&255)*(255-alpha)+127)/255)<<shift;}
        p[y*32+x+16]=result;
    }
    return iconPixels(p);
}
static HICON badgeIcon(HICON base,int count,int bright){return coloredBadgeIcon(base,count,badgeStyle,bright);}
#include "custom-icon.h"
static void updateMotion(void) {
    int i,quota=0;LONGLONG now=nowSeconds();finishedUnread=longWorkCount=0;
    for(i=0;i<chatCount;i++)if(latestChat(i)){Chat *c=&chats[i];if(c->quotaBlocked&&c->quotaAt>quotaAckAt)quota=1;if(c->finished&&!activeChat(c)&&isUnread(c->id))finishedUnread++;if(c->mood==WORK&&activeChat(c)&&c->startedAt>0&&now-c->startedAt>=600)longWorkCount++;}
    motion=manualMotion>=0?manualMotion:manual>=0?0:quota?1:longWorkCount&&aggregate(chats,chatCount,overflow)==WORK?2:0;
}
static HICON displayIcon(int shown) {
    int count=manualMotion==3?3:finishedUnread,key=count>9?10:count,kind=motion==1?7:motion==2?8:shown,frames=kind>=7?4:2,frame=animation?(kind==8?(animationFrame/2)%4:kind==7?animationFrame%4:phase):0,pulse=animation&&nowSeconds()<completionPulseUntil;
    HICON base=kind>=7?extraIcons[kind-7][frame]:icons[shown][frame];int cacheKey=kind+10*pulse;
    if(customEnabled){selectCustomSlot(customSlot(shown,motion==1,motion==2));if(customCount){HICON custom=customDisplayIcon(shown,count,motion==1,motion==2,pulse);if(custom)return custom;}}
    if(!count){if(badgeFrames)clearBadgeIcons();return base;}
    if(badgeKey!=key||badgeBaseKey!=cacheKey){int i;clearBadgeIcons();badgeKey=key;badgeBaseKey=cacheKey;badgeFrames=frames;for(i=0;i<frames;i++)badgeIcons[i]=badgeIcon(kind>=7?extraIcons[kind-7][i]:icons[shown][i],key,pulse&&(i&1));}
    return badgeIcons[frame]?badgeIcons[frame]:base;
}
