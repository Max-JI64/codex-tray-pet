static int languageApiCase(const char *body,int expectedCode,int expectedLanguage){
    char request[1024];PetHttpClient client={0};int failed=0;
    sprintf(request,"POST /%s/api/language HTTP/1.1\r\nHost: 127.0.0.1:12345\r\nOrigin: http://127.0.0.1:12345\r\nX-Pet-Key: %s\r\nContent-Length: %d\r\n\r\n%s",webToken,webToken,(int)strlen(body),body);
    client.input=request;client.headerLength=(int)(strstr(request,"\r\n\r\n")-request)+4;client.bodyLength=(int)strlen(body);
    webDispatch(&client);if(!client.output)return 1;
    failed=atoi(client.output+9)!=expectedCode||uiLanguage!=expectedLanguage;
    if(expectedCode==200){char wanted[32];sprintf(wanted,"\"language\":%d",expectedLanguage);failed+=strstr(client.output,wanted)==NULL;}
    VirtualFree(client.output,0,MEM_RELEASE);return failed;
}
static int languageTest(void){
    WCHAR savedFolder[1024],testFolder[1100],path[1100];int i,j,failed=0;FILE *f;
    wcscpy(savedFolder,folder);_snwprintf(testFolder,1100,L"%ls\\language-test",folder);CreateDirectoryW(testFolder,NULL);wcscpy(folder,testFolder);
    loadBadgeStyle();failed+=uiLanguage!=0;
    webPort=12345;strcpy(webToken,"012345678901234567890123456789ab");
    for(i=0;i<5;i++){
        failed+=!setUiLanguage(i);loadBadgeStyle();failed+=uiLanguage!=i;
        failed+=!savePetPreferences(2,0,0);loadBadgeStyle();failed+=uiLanguage!=i||badgeStyle!=2||animation!=0;
        for(j=0;j<sizeof(uiText)/sizeof(uiText[0]);j++)failed+=!uiText[j][i]||!uiText[j][i][0];
        failed+=wcscmp(stateName(WORK),uiText[TXT_working][i])!=0;
        {char body[40];sprintf(body,"{\"language\":%d}",i);failed+=languageApiCase(body,200,i);loadBadgeStyle();failed+=uiLanguage!=i;}
    }
    failed+=setUiLanguage(-1)||setUiLanguage(5)||uiLanguage!=4;
    failed+=languageApiCase("{\"language\":5}",400,4);
    failed+=languageApiCase("{\"language\":-1}",400,4);
    failed+=languageApiCase("{\"language\":1.5}",400,4);
    failed+=languageApiCase("{\"language\":\"ko\"}",400,4);
    _snwprintf(path,1100,L"%ls\\pet-settings.txt",folder);f=_wfopen(path,L"wb");if(f){fputs("badgeColor=1\nanimation=1\ncustomIcon=0\nlanguage=99\n",f);fclose(f);}else failed++;
    loadBadgeStyle();failed+=uiLanguage!=0;DeleteFileW(path);_snwprintf(path,1100,L"%ls\\lite-status.json",folder);DeleteFileW(path);RemoveDirectoryW(testFolder);wcscpy(folder,savedFolder);uiLanguage=0;
    _snwprintf(path,1100,L"%ls\\language-verification.txt",folder);f=_wfopen(path,L"wb");if(f){fprintf(f,"%s: five languages, English default, restart persistence, preference preservation, invalid-language rejection.\n",failed?"FAIL":"PASS");fclose(f);}else failed++;
    return failed?1:0;
}
