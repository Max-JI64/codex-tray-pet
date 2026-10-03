/* Documentation renderer: synthetic sessions, no windows, tray registration,
   account reads, screen captures, or calls to the running Codex application. */
#define WinMain PetApplicationEntry
#include "../src/CodexPetLite.c"
#undef WinMain

static int writeBitmap(const WCHAR *name,int width,int height,const DWORD *pixels){
    BITMAPFILEHEADER file={0};BITMAPINFOHEADER info={0};WCHAR path[1100];FILE *f;
    file.bfType=0x4d42;file.bfOffBits=sizeof(file)+sizeof(info);file.bfSize=file.bfOffBits+width*height*4;
    info.biSize=sizeof(info);info.biWidth=width;info.biHeight=-height;info.biPlanes=1;info.biBitCount=32;
    _snwprintf(path,1100,L"%ls\\%ls",folder,name);f=_wfopen(path,L"wb");if(!f)return 0;
    fwrite(&file,sizeof(file),1,f);fwrite(&info,sizeof(info),1,f);fwrite(pixels,width*height*4,1,f);return fclose(f)==0;
}
static int renderSessionExample(int example,int hovered){
    Chat sample[3]={{0}};BITMAPINFO bi={0};HBITMAP bitmap;HDC dc;DWORD *pixels;HGDIOBJ old;RECT bounds;WCHAR name[80];int i,ok;
    chats=sample;chatCount=panelCount=2;panelOffset=0;panelHover=hovered?0:-1;readStateOk=1;desktopStartedAt=0;
    for(i=0;i<2;i++){sample[i].allowed=1;sample[i].latestAt=nowSeconds();sample[i].startedAt=nowSeconds()-(i?724:83);panelRows[i]=i;}
    wcscpy(sample[0].title,L"Build documentation");wcscpy(sample[1].title,L"Review changes");
    sample[0].mood=sample[1].mood=example==0?WORK:example==1?DONE:example==2?QUESTION:example==3?ERROR_STATE:STOPPED;
    if(example==4)sample[0].quotaBlocked=sample[1].quotaBlocked=1;
    bounds.left=bounds.top=0;bounds.right=px(280);bounds.bottom=panelHeight();
    bi.bmiHeader.biSize=sizeof(BITMAPINFOHEADER);bi.bmiHeader.biWidth=bounds.right;bi.bmiHeader.biHeight=-bounds.bottom;bi.bmiHeader.biPlanes=1;bi.bmiHeader.biBitCount=32;
    dc=CreateCompatibleDC(NULL);bitmap=CreateDIBSection(dc,&bi,DIB_RGB_COLORS,(void**)&pixels,NULL,0);if(!dc||!bitmap)return 0;
    old=SelectObject(dc,bitmap);drawPanel(dc,bounds);
    /* Same hand cursor selected by panelProc for a hovered session title. */
    if(hovered)DrawIconEx(dc,px(158),px(panelHeaderHeight+2),(HICON)LoadCursor(NULL,IDC_HAND),px(22),px(22),0,NULL,DI_NORMAL);
    GdiFlush();
    if(hovered)wcscpy(name,L"panel-hover.bmp");else _snwprintf(name,80,L"panel-%d.bmp",example);
    ok=writeBitmap(name,bounds.right,bounds.bottom,pixels);
    SelectObject(dc,old);DeleteObject(bitmap);DeleteDC(dc);chats=NULL;chatCount=0;return ok;
}
int main(void){
    int state,frame,failed=0;WCHAR name[80],*slash;HICON icon,withBadge;DWORD pixels[1024];POINT point={0};
    GetModuleFileNameW(NULL,folder,1024);slash=wcsrchr(folder,L'\\');if(slash)*slash=0;
    menuStyle(point);badgeStyle=1;
    for(state=0;state<9;state++)for(frame=0;frame<4;frame++){
        icon=state<7?makeIcon(state,frame%2):makeMotionIcon(state-7,frame);
        if(!icon){failed++;continue;}
        if(!copyIconPixels(icon,pixels))failed++;else{_snwprintf(name,80,L"icon-%d-%d.bmp",state,frame);failed+=!writeBitmap(name,32,32,pixels);}
        if(state==WORK){withBadge=badgeIcon(icon,3,0);if(!withBadge)failed++;else{copyIconPixels(withBadge,pixels);_snwprintf(name,80,L"badge-%d.bmp",frame);failed+=!writeBitmap(name,32,32,pixels);DestroyIcon(withBadge);}}
        DestroyIcon(icon);
    }
    icon=makeIcon(WORK,0);
    if(!icon)failed++;else{
        for(state=0;state<6;state++){
            withBadge=coloredBadgeIcon(icon,3,state,0);
            if(!withBadge)failed++;else{
                if(!copyIconPixels(withBadge,pixels))failed++;else{
                    _snwprintf(name,80,L"badge-color-%d.bmp",state);failed+=!writeBitmap(name,32,32,pixels);
                }
                DestroyIcon(withBadge);
            }
        }
        DestroyIcon(icon);
    }
    for(state=0;state<5;state++)failed+=!renderSessionExample(state,0);
    failed+=!renderSessionExample(0,1);
    DeleteObject(panelFont);DeleteObject(headerFont);return failed?1:0;
}
