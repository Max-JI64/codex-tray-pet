static int customStateTest(void){
    WCHAR saved[1024],dir[1100],path[1100];FILE *f,*fixtureFile;BYTE fixture[12+8*4100]={0};DWORD count=8,duration=200;int i,slot,failed=0;CustomFrame *pointer;DWORD started;
    wcscpy(saved,folder);_snwprintf(dir,1100,L"%ls\\custom-state-test",folder);CreateDirectoryW(dir,NULL);wcscpy(folder,dir);
    _snwprintf(path,1100,L"%ls\\custom-state-verification.txt",saved);f=_wfopen(path,L"wb");if(!f)return 1;
    memcpy(fixture,"CPETAN1",8);memcpy(fixture+8,&count,4);for(i=0;i<8;i++){memcpy(fixture+12+i*4100,&duration,4);fixture[16+i*4100+4*500+3]=255;}
    for(slot=-1;slot<=8;slot++){fixture[16+4*500]=(BYTE)(slot+20);customPath(slot,path);fixtureFile=_wfopen(path,L"wb");if(!fixtureFile){failed++;continue;}fwrite(fixture,sizeof(fixture),1,fixtureFile);fclose(fixtureFile);}
    customEnabled=animation=1;
    failed+=check(customSlot(WORK,1,1)==7&&customSlot(WORK,0,1)==8&&customSlot(QUESTION,0,0)==3,"quota, long work and ordinary states map to distinct slots",f);
    {char request[WEB_REQUEST_CAP];PetHttpClient client={0};int header;webPort=12345;strcpy(webToken,"012345678901234567890123456789ab");
        header=sprintf(request,"POST /%s/api/icon?slot=3 HTTP/1.1\r\nHost: 127.0.0.1:12345\r\nOrigin: http://127.0.0.1:12345\r\nX-Pet-Key: %s\r\n\r\n",webToken,webToken);fixture[16+4*500]=23;memcpy(request+header,fixture,sizeof(fixture));client.input=request;client.headerLength=header;client.bodyLength=sizeof(fixture);webDispatch(&client);
        failed+=check(client.output&&!strncmp(client.output,"HTTP/1.1 200",12)&&customFileMask()==1023,"state upload saves only selected slot and retains every other set",f);if(client.output)VirtualFree(client.output,0,MEM_RELEASE);client.output=NULL;
        header=sprintf(request,"POST /%s/api/icon?slot=9 HTTP/1.1\r\nHost: 127.0.0.1:12345\r\nOrigin: http://127.0.0.1:12345\r\nX-Pet-Key: %s\r\n\r\n",webToken,webToken);memcpy(request+header,fixture,sizeof(fixture));client.headerLength=header;webDispatch(&client);
        failed+=check(client.output&&!strncmp(client.output,"HTTP/1.1 400",12)&&customFileMask()==1023,"invalid state upload is rejected without modifying saved sets",f);if(client.output)VirtualFree(client.output,0,MEM_RELEASE);
    }
    for(slot=0;slot<=8;slot++){selectCustomSlot(slot);failed+=check(customLoadedSlot==slot&&customCount==8&&(customFrames[0].pixels[500]>>16&255)==slot+20,"state loads its own file without another state leaking",f);}
    pointer=customFrames;started=customStartedTick;selectCustomSlot(8);failed+=check(customFrames==pointer&&customStartedTick==started,"unchanged slot reuses active allocation and clock",f);
    selectCustomSlot(7);failed+=check(customDisplayIcon(STOPPED,0,1,0,0)!=NULL&&customCacheCount==8,"specific quota set keeps all eight frames",f);
    customPath(3,path);DeleteFileW(path);freeCustomFrames();selectCustomSlot(3);failed+=check(customLoadedSlot==-1&&customCount==8,"missing state falls back to common set",f);
    customPath(2,path);fixtureFile=_wfopen(path,L"wb");if(fixtureFile){fwrite("bad",3,1,fixtureFile);fclose(fixtureFile);}freeCustomFrames();selectCustomSlot(2);failed+=check(customLoadedSlot==-1,"invalid state falls back without adopting invalid pixels",f);
    customPath(-1,path);DeleteFileW(path);freeCustomFrames();selectCustomSlot(3);failed+=check(!customFrames&&!customCount&&customRequestedSlot==3,"no state or common leaves base cat fallback",f);
    failed+=check(sizeof(CustomFrame)*CUSTOM_MAX_FRAMES==32800,"resident custom set remains bounded at 32800 bytes",f);
    for(slot=-1;slot<=8;slot++){customPath(slot,path);DeleteFileW(path);}_snwprintf(path,1100,L"%ls\\pet-settings.txt",folder);DeleteFileW(path);freeCustomFrames();RemoveDirectoryW(dir);wcscpy(folder,saved);customEnabled=0;fclose(f);return failed?1:0;
}
