/* Hidden loopback-only test harness; never activates a tray icon or Codex. */
#define WinMain PetApplicationEntry
#include "../src/CodexPetLite.c"
#undef WinMain
int main(void){
    WCHAR base[1024],file[1100],*slash;WNDCLASSW wc={0};MSG msg;FILE *f;DWORD start;
    GetModuleFileNameW(NULL,base,1024);slash=wcsrchr(base,L'\\');if(slash)*slash=0;
    _snwprintf(folder,1024,L"%ls\\settings-http-test",base);CreateDirectoryW(folder,NULL);
    _snwprintf(codexHome,1024,L"%ls\\empty-codex-home",folder);
    uiLanguage=0;window=NULL;wc.lpfnWndProc=wndProc;wc.hInstance=GetModuleHandleW(NULL);wc.lpszClassName=L"CodexPetSettingsTest";RegisterClassW(&wc);
    window=CreateWindowW(wc.lpszClassName,L"",WS_OVERLAPPED,0,0,0,0,NULL,NULL,wc.hInstance,NULL);
    if(!window||!browserSettingsStart())return 1;
    _snwprintf(file,1100,L"%ls\\connection.json",folder);f=_wfopen(file,L"wb");if(!f)return 1;
    fprintf(f,"{\"port\":%d,\"key\":\"%s\"}",webPort,webToken);fclose(f);start=GetTickCount();
    while(GetTickCount()-start<30000){while(PeekMessageW(&msg,NULL,0,0,PM_REMOVE)){TranslateMessage(&msg);DispatchMessageW(&msg);}browserSettingsPoll();Sleep(5);}
    browserSettingsStop();DestroyWindow(window);DeleteFileW(file);return 0;
}
