/* Only resized frames are resident. Image decoding happens in the settings browser. */
#define CUSTOM_MAX_FRAMES 8
typedef struct {DWORD duration,pixels[1024];} CustomFrame;
static CustomFrame *customFrames;
static int customCount,customCacheKey=-1,customCacheCount;
static HICON customIcons[CUSTOM_MAX_FRAMES];
static DWORD customStartedTick;
/* Disk has one optional set per state; only the current/fallback set is resident. */
static int customRequestedSlot=-2,customLoadedSlot=-2;
static int customSlot(int shown,int urgent,int longWork){return urgent?7:longWork?8:shown;}
static int customPath(int slot,WCHAR path[1100]){if(slot < -1||slot>8)return 0;if(slot==-1)_snwprintf(path,1100,L"%ls\\custom-animation.bin",folder);else _snwprintf(path,1100,L"%ls\\custom-animation-%d.bin",folder,slot);return 1;}
static void clearCustomIcons(void){int i;for(i=0;i<CUSTOM_MAX_FRAMES;i++){if(customIcons[i])DestroyIcon(customIcons[i]);customIcons[i]=NULL;}customCacheKey=-1;customCacheCount=0;}
static void freeCustomFrames(void){clearCustomIcons();if(customFrames)VirtualFree(customFrames,0,MEM_RELEASE);customFrames=NULL;customCount=0;customRequestedSlot=customLoadedSlot=-2;}
static int animationValid(const BYTE *data,int length){
    DWORD count,i;int alpha=0;
    if(length<12||memcmp(data,"CPETAN1",8))return 0;memcpy(&count,data+8,4);
    if(count<1||count>CUSTOM_MAX_FRAMES||length!=12+(int)count*4100)return 0;
    for(i=0;i<count;i++){DWORD duration;int p;memcpy(&duration,data+12+i*4100,4);if(duration<150||duration>2000)return 0;
        for(p=3;p<4096;p+=4)if(data[16+i*4100+p])alpha=1;}
    return alpha;
}
static int adoptAnimation(const BYTE *data,int length){
    CustomFrame *frames;DWORD count,i;int p;
    if(!animationValid(data,length))return 0;memcpy(&count,data+8,4);
    frames=VirtualAlloc(NULL,count*sizeof(CustomFrame),MEM_RESERVE|MEM_COMMIT,PAGE_READWRITE);if(!frames)return 0;
    for(i=0;i<count;i++){const BYTE *rgba=data+16+i*4100;memcpy(&frames[i].duration,data+12+i*4100,4);
        for(p=0;p<1024;p++){DWORD a=rgba[p*4+3],r=(rgba[p*4]*a+127)/255,g=(rgba[p*4+1]*a+127)/255,b=(rgba[p*4+2]*a+127)/255;frames[i].pixels[p]=(a<<24)|(r<<16)|(g<<8)|b;}}
    freeCustomFrames();customFrames=frames;customCount=count;customStartedTick=GetTickCount();return 1;
}
static int readCustomSlot(int slot){
    WCHAR path[1100];FILE *f;BYTE *data;long length;
    int ok=0;if(!customPath(slot,path))return 0;f=_wfopen(path,L"rb");if(!f)return 0;
    if(fseek(f,0,SEEK_END)||((length=ftell(f))<12)||length>12+CUSTOM_MAX_FRAMES*4100||fseek(f,0,SEEK_SET)){fclose(f);return 0;}
    data=VirtualAlloc(NULL,length,MEM_RESERVE|MEM_COMMIT,PAGE_READWRITE);if(data){if(fread(data,1,length,f)==length)ok=adoptAnimation(data,length);VirtualFree(data,0,MEM_RELEASE);}fclose(f);if(ok)customLoadedSlot=slot;return ok;
}
static void selectCustomSlot(int slot){if(!customEnabled||slot==customRequestedSlot)return;freeCustomFrames();if(!readCustomSlot(slot))readCustomSlot(-1);customRequestedSlot=slot;}
static void loadCustomFrames(void){freeCustomFrames();if(customEnabled)selectCustomSlot(customSlot(manual>=0?manual:mood,motion==1,motion==2));}
static int customFrameAt(DWORD elapsed){DWORD total=0;int i;if(!animation)return 0;for(i=0;i<customCount;i++)total+=customFrames[i].duration;if(!total)return 0;elapsed%=total;for(i=0;i<customCount;i++){if(elapsed<customFrames[i].duration)return i;elapsed-=customFrames[i].duration;}return 0;}
static HICON makeCustomFrame(int frame,int shown,int count,int urgent,int longWork,int bright){
    DWORD dest[1024]={0};DWORD colors[]={0xff9ba8b8,0xff49aaff,0xff45d896,0xffffbe41,0xffff6474,0xffb58bee,0xff828282};
    int fallbackUrgent=urgent&&customLoadedSlot!=7;
    DWORD bar=urgent?(frame&1?0xffffbbbb:0xffff293f):longWork?0xff6489ef:colors[shown];int x,y,shift=fallbackUrgent?(frame%4==1?-2:frame%4==3?2:0):0;HICON base,withBadge;
    for(y=0;y<32;y++)for(x=0;x<32;x++){DWORD p=customFrames[fallbackUrgent?0:frame].pixels[y*32+x];if(fallbackUrgent&&p>>24){DWORD a=p>>24;p=(a<<24)|((((p>>16&255)+a)/2)<<16)|(((p>>8&255)/2)<<8)|((p&255)/2);}pixel(dest,x,y+shift,p);}
    if(shown==QUESTION||urgent||shown==ERROR_STATE||shown==UNKNOWN){for(y=20;y<30;y++)for(x=0;x<10;x++)if((x-5)*(x-5)+(y-25)*(y-25)<=25)pixel(dest,x,y,bar);
        if(shown==QUESTION&&!urgent){pixel(dest,4,22,0xff17233b);pixel(dest,5,22,0xff17233b);pixel(dest,6,23,0xff17233b);pixel(dest,5,24,0xff17233b);pixel(dest,4,25,0xff17233b);pixel(dest,4,28,0xff17233b);}
        else{for(y=22;y<26;y++)pixel(dest,5,y,0xff17233b);pixel(dest,5,28,0xff17233b);}}
    if(longWork&&!urgent){for(x=0;x<9;x++){pixel(dest,x,20,0xff17233b);pixel(dest,x,28,0xff17233b);}for(y=21;y<28;y++){int width=abs(y-24);for(x=4-width;x<=4+width;x++)pixel(dest,x,y,0xffffcb55);}}
    base=iconPixels(dest);if(base&&count){withBadge=coloredBadgeIcon(base,count,badgeStyle,bright);if(withBadge){DestroyIcon(base);return withBadge;}}return base;
}
static HICON customDisplayIcon(int shown,int count,int urgent,int longWork,int pulse){
    int i,fallbackUrgent=urgent&&customLoadedSlot!=7,frames=fallbackUrgent?4:customCount,key=shown+8*urgent+16*longWork+32*pulse+64*badgeStyle+512*(count>9?10:count),frame;
    if(customCacheKey!=key){clearCustomIcons();customCacheKey=key;for(i=0;i<frames;i++){customIcons[i]=makeCustomFrame(i,shown,count,urgent,longWork,pulse&&(i&1));if(customIcons[i])customCacheCount++;}}
    frame=fallbackUrgent?(animation?animationFrame%4:0):customFrameAt(GetTickCount()-customStartedTick);return customIcons[frame];
}
