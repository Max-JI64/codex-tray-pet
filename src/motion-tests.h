static void exportIcon(HICON icon,const WCHAR *name) {
    DWORD pixels[1024];BITMAPFILEHEADER file={0};BITMAPINFOHEADER info={0};WCHAR path[1100];FILE *f;
    if(!copyIconPixels(icon,pixels))return;file.bfType=0x4d42;file.bfOffBits=sizeof(file)+sizeof(info);file.bfSize=file.bfOffBits+4096;
    info.biSize=sizeof(info);info.biWidth=32;info.biHeight=-32;info.biPlanes=1;info.biBitCount=32;
    _snwprintf(path,1100,L"%ls\\%ls",folder,name);f=_wfopen(path,L"wb");if(!f)return;fwrite(&file,sizeof(file),1,f);fwrite(&info,sizeof(info),1,f);fwrite(pixels,4096,1,f);fclose(f);
}
static int badgeSamples(void) {
    WCHAR path[1100],name[256];HICON base,icon;int state,style,n,counts[]={1,3,10},failed=0;
    _snwprintf(path,1100,L"%ls\\badge-samples",folder);CreateDirectoryW(path,NULL);
    for(state=0;state<9;state++){
        base=state<7?makeIcon(state,0):makeMotionIcon(state-7,0);if(!base){failed++;continue;}
        for(style=0;style<6;style++)for(n=0;n<3;n++){
            icon=coloredBadgeIcon(base,counts[n],style,0);if(!icon){failed++;continue;}
            _snwprintf(name,256,L"badge-samples\\s%d-c%d-n%d.bmp",state,style,n);exportIcon(icon,name);DestroyIcon(icon);
        }DestroyIcon(base);
    }return failed?1:0;
}
static int customIconTest(void){
    WCHAR path[1100];FILE *f;BYTE fixture[12+8*4100]={0};DWORD count=8,duration=150;int i,failed=0;DWORD pixels[1024];
    _snwprintf(path,1100,L"%ls\\custom-verification.txt",folder);f=_wfopen(path,L"wb");if(!f)return 1;
    memcpy(fixture,"CPETAN1",8);memcpy(fixture+8,&count,4);for(i=0;i<8;i++){memcpy(fixture+12+i*4100,&duration,4);fixture[16+i*4100+4*500]=220;fixture[16+i*4100+4*500+3]=255;}
    failed+=check(animationValid(fixture,sizeof(fixture))&&adoptAnimation(fixture,sizeof(fixture)),"8 resized frames accepted without original decoder",f);
    failed+=check(!animationValid(fixture,sizeof(fixture)-1),"truncated frame rejected",f);fixture[8]=9;failed+=check(!animationValid(fixture,sizeof(fixture)),"ninth frame rejected",f);fixture[8]=8;
    fixture[12]=100;failed+=check(!animationValid(fixture,sizeof(fixture)),"too-fast frame rejected",f);fixture[12]=150;
    animation=1;failed+=check(customFrameAt(149)==0&&customFrameAt(150)==1&&customFrameAt(1200)==0,"elapsed clock handles custom frames and wrapping",f);
    animation=0;failed+=check(customFrameAt(150)==0,"disabled motion freezes first frame",f);animation=1;
    for(i=0;i<30;i++){HICON icon=customDisplayIcon(i%7,3,0,0,0);failed+=check(icon&&customCacheCount==8,"state switches keep only eight cached icons",f);}
    {HICON question=customDisplayIcon(QUESTION,0,0,0,0);copyIconPixels(question,pixels);failed+=check(pixels[25*32+4]==0xff17233b,"custom icon retains question mark",f);}
    {HICON plain=makeCustomFrame(0,WORK,0,0,0,0);int x,transparent=1;copyIconPixels(plain,pixels);for(x=7;x<25;x++)if(pixels[30*32+x])transparent=0;failed+=check(transparent,"custom icon has no bottom status bar",f);DestroyIcon(plain);}
    {HICON warning=customDisplayIcon(WORK,3,1,0,0);failed+=check(warning&&customCacheCount==4,"usage warning remains four urgent frames",f);}
    freeCustomFrames();failed+=check(!customFrames&&!customCount&&!customCacheCount,"custom pixels and handles released",f);
    webPort=12345;strcpy(webToken,"012345678901234567890123456789ab");
    failed+=check(validHttpOrigin("POST / HTTP/1.1\r\nHost: 127.0.0.1:12345\r\nOrigin: http://127.0.0.1:12345\r\nX-Pet-Key: 012345678901234567890123456789ab\r\n\r\n",160,1),"same-origin settings request allowed",f);
    failed+=check(!validHttpOrigin("POST / HTTP/1.1\r\nHost: 127.0.0.1:12345\r\nOrigin: https://example.com\r\nX-Pet-Key: 012345678901234567890123456789ab\r\n\r\n",160,1),"foreign origin cannot change local settings",f);
    failed+=check(!validHttpOrigin("POST / HTTP/1.1\r\nHost: 127.0.0.1:12345\r\n\r\n",100,1),"missing access key rejected",f);
    fclose(f);return failed?1:0;
}
static int motionTest(void) {
    WCHAR path[1100];FILE *f;int failed=0,i,j;Chat sample[4]={{0}};Record r;LONGLONG now=nowSeconds();HICON icon;
    _snwprintf(path,1100,L"%ls\\motion-verification.txt",folder);f=_wfopen(path,L"wb");if(!f)return 1;
    failed+=check(quotaCode("usage_limit_reached")&&quotaCode("UsageLimitExceeded")&&quotaCode("insufficient_quota"),"exact quota codes",f);
    failed+=check(!quotaCode("rate_limit_exceeded")&&!quotaCode("ContextWindowExceeded")&&!quotaCode("HttpConnectionFailed"),"retry and context errors are not exhausted usage",f);
    failed+=check(quotaMessage("startup websocket prewarm setup failed: You\xe2\x80\x99ve hit your usage limit. Upgrade to Pro"),"real Windows curly apostrophe quota log",f);
    chats=sample;chatCount=4;readStateOk=1;unreadCount=1;strcpy(unreadIds[0],"done");
    for(i=0;i<4;i++){sample[i].allowed=1;sample[i].latestAt=now;sample[i].startedAt=now-590;}
    strcpy(sample[0].id,"active");sample[0].mood=WORK;strcpy(sample[1].id,"done");sample[1].finished=1;sample[1].mood=IDLE;
    strcpy(sample[2].id,"done");sample[2].finished=1;sample[2].mood=IDLE;sample[2].latestAt=now-1;
    strcpy(sample[3].id,"old");sample[3].mood=WORK;sample[3].latestAt=now-3600;desktopStartedAt=now-1000;
    updateMotion();failed+=check(finishedUnread==1&&motion==0&&aggregate(chats,chatCount,0)==WORK,"one unread completion coalesced while blue active",f);
    sample[0].startedAt=now-600;updateMotion();failed+=check(longWorkCount==1&&motion==2,"600 second long work threshold",f);
    for(i=0;i<7;i++){manual=i;manualMotion=-1;updateMotion();failed+=check(motion==0,"ordinary manual state preview overrides real long-work motion",f);}manual=manualMotion=-1;updateMotion();failed+=check(motion==2,"automatic mode restores real long-work motion",f);
    sample[0].mood=QUESTION;updateMotion();failed+=check(longWorkCount==0&&motion==0,"waiting question is not long working motion",f);
    sample[0].mood=IDLE;unreadCount=0;updateMotion();failed+=check(finishedUnread==0&&aggregate(chats,chatCount,0)==IDLE,"reading clears badge and stale work cannot keep blue",f);
    sample[0].mood=WORK;sample[0].quotaBlocked=1;sample[0].quotaAt=now;updateMotion();failed+=check(motion==1,"quota warning wins over long work",f);
    manual=DONE;updateMotion();failed+=check(motion==0,"explicit completion preview overrides real quota alert",f);manual=manualMotion=-1;updateMotion();failed+=check(motion==1,"returning to automatic restores real quota alert",f);
    sample[0].mood=STOPPED;failed+=check(visibleChat(&sample[0])&&!isUnread(sample[0].id),"read quota-blocked session remains listed",f);
    quotaAckAt=now;updateMotion();failed+=check(motion!=1&&sample[0].quotaBlocked&&visibleChat(&sample[0]),"acknowledging flashing keeps resume marker",f);quotaAckAt=0;
    sample[0].quotaUnread=1;syncQuotaReadState();failed+=check(!sample[0].quotaBlocked&&!visibleChat(&sample[0]),"Codex unread-to-read change clears quota marker without starting a turn",f);
    sample[0].quotaBlocked=1;sample[0].quotaAt=now;
    parse("{\"type\":\"event_msg\",\"payload\":{\"type\":\"task_started\",\"turn_id\":\"new\"}}",&r);fold(&sample[0],&r,0);failed+=check(!sample[0].quotaBlocked&&sample[0].mood==WORK,"new turn clears previous quota marker",f);
    parse("{\"type\":\"event_msg\",\"payload\":{\"type\":\"error\",\"willRetry\":true,\"error\":{\"codexErrorInfo\":\"UsageLimitExceeded\"}}}",&r);fold(&sample[0],&r,0);failed+=check(sample[0].mood==WORK,"retry notification cannot mark terminal stop",f);
    parse("{\"type\":\"event_msg\",\"payload\":{\"type\":\"error\",\"error\":{\"codexErrorInfo\":\"UsageLimitExceeded\"}}}",&r);fold(&sample[0],&r,0);failed+=check(sample[0].quotaBlocked&&sample[0].mood==STOPPED,"nested quota event marks stopped",f);
    parse("{\"type\":\"event_msg\",\"payload\":{\"type\":\"task_complete\",\"turn_id\":\"new\"}}",&r);fold(&sample[0],&r,0);failed+=check(!sample[0].quotaBlocked&&sample[0].finished,"successful completion clears earlier quota error without new start",f);
    parse("{\"type\":\"response_item\",\"payload\":{\"type\":\"message\",\"role\":\"assistant\",\"content\":[{\"text\":\"You've hit your usage limit\"}]}}",&r);sample[0].quotaBlocked=0;sample[0].mood=WORK;fold(&sample[0],&r,0);failed+=check(!sample[0].quotaBlocked&&sample[0].mood==WORK,"quoted assistant text does not set quota",f);
    failed+=check(quotaToolFailure("Script error:\nexec_command failed: CreateProcess { message: \"Rejected(Automatic approval review failed: You've hit your usage limit")&&!quotaToolFailure("A report quotes Automatic approval review failed: You've hit your usage limit"),"specific tool quota failure recognized without quoted-text false positive",f);
    {RECT parent={300,50,580,200},work={0,0,1000,1000};failed+=check(projectTipX(&parent,&work,220)==580,"project tip prefers right",f);parent.left=720;parent.right=1000;failed+=check(projectTipX(&parent,&work,220)==500,"project tip falls back left at edge",f);}
    for(i=0;i<2;i++)for(j=0;j<4;j++){icon=makeMotionIcon(i,j);failed+=check(icon!=NULL,"cached alert frame",f);if(icon){if(j==0)exportIcon(icon,i?L"preview-long.bmp":L"preview-quota.bmp");DestroyIcon(icon);}}
    icon=makeIcon(WORK,0);if(icon){HICON badge=badgeIcon(icon,3,0),many=badgeIcon(icon,10,0);failed+=check(badge&&many,"single number and 9+ overlay",f);if(badge){exportIcon(badge,L"preview-badge.bmp");DestroyIcon(badge);}if(many){exportIcon(many,L"preview-badge-many.bmp");DestroyIcon(many);}DestroyIcon(icon);}
    {int x,y,left=16,right=-1,top=16,bottom=-1;makeBadgeNumber(3);for(y=0;y<16;y++)for(x=0;x<16;x++)if(badgeNumberMask[y*16+x]>10){if(x<left)left=x;if(x>right)right=x;if(y<top)top=y;if(y>bottom)bottom=y;}
        failed+=check(abs(left+right-15)<=1&&abs(top+bottom-15)<=1,"Segoe UI number ink centered in badge",f);}
    chats=NULL;chatCount=unreadCount=0;desktopStartedAt=0;fclose(f);return failed?1:0;
}
static int quotaStateTest(void) {
    WCHAR savedFolder[1024],testFolder[1100],path[1100];Chat sample={0};FILE *f;int failed=0;Record r;
    wcscpy(savedFolder,folder);_snwprintf(testFolder,1100,L"%ls\\quota-state-test",folder);CreateDirectoryW(testFolder,NULL);wcscpy(folder,testFolder);
    chats=&sample;chatCount=1;sample.allowed=1;strcpy(sample.id,"00000000-0000-0000-0000-000000000001");wcscpy(sample.path,L"fixture.jsonl");sample.startedAt=100;sample.latestAt=200;sample.quotaBlocked=1;sample.quotaAt=200;sample.mood=STOPPED;quotaAckAt=150;quotaStateDirty=1;saveQuotaState();
    failed+=!quotaStateDirty?0:1;sample.quotaBlocked=0;sample.quotaAt=0;quotaAckAt=0;loadQuotaState();
    _snwprintf(path,1100,L"%ls\\quota-state-verification.txt",savedFolder);f=_wfopen(path,L"wb");if(!f){wcscpy(folder,savedFolder);chats=NULL;chatCount=0;return 1;}
    failed+=check(sample.quotaBlocked&&sample.quotaAt==200&&quotaAckAt==150,"quota marker and alert acknowledgement survive watcher restart",f);
    parse("{\"type\":\"event_msg\",\"timestamp\":\"2026-10-03T05:00:00Z\",\"payload\":{\"type\":\"task_started\",\"turn_id\":\"resume\"}}",&r);fold(&sample,&r,0);saveQuotaState();sample.quotaBlocked=0;loadQuotaState();failed+=check(!sample.quotaBlocked,"resuming removes persisted quota marker",f);
    _snwprintf(path,1100,L"%ls\\quota-blocked.txt",folder);DeleteFileW(path);wcscpy(folder,savedFolder);chats=NULL;chatCount=0;quotaAckAt=0;fclose(f);return failed?1:0;
}
