/* Read-only desktop state subscription. No ownership, decisions, or task mutation. */
typedef struct {
    char type[32],method[80],client[80],id[64],host[32],change[24],path[80],field[80];
    int version,statusTouched,approval,userInput,requestTouched,requestApproval,requestInput,resync;
    LONGLONG revision,baseRevision;
} ApprovalMessage;
static HANDLE approvalPipe=INVALID_HANDLE_VALUE;
static StreamRecord *approvalReader;
static ApprovalMessage approvalMessage;
static char approvalClient[80];
static DWORD approvalLength,approvalRead,approvalHeadBytes,approvalRetryAt;
static BYTE approvalHead[4];
static unsigned long approvalFrames,approvalReconnects;
static int approvalProtocolOk;
static int approvalConnected(void){return approvalPipe!=INVALID_HANDLE_VALUE&&approvalProtocolOk;}
static unsigned long approvalFrameCount(void){return approvalFrames;}
enum { AP_TOP=100,AP_PARAMS,AP_CHANGE,AP_SNAPSHOT,AP_STATUS,AP_FLAGS,AP_PATCHES,AP_PATCH,AP_PATH,AP_REQUESTS,AP_REQUEST,AP_RESULT };
static int permissionMethod(const char *name){return !strcmp(name,"item/commandExecution/requestApproval")||!strcmp(name,"item/fileChange/requestApproval")||!strcmp(name,"item/permissions/requestApproval")||!strcmp(name,"execCommandApproval")||!strcmp(name,"applyPatchApproval");}
static void approvalObserve(StreamRecord *reader,JsonScope *scope,int event,const char *value){
    ApprovalMessage *m=&approvalMessage;JsonScope *parent=reader->depth>1?&reader->scope[reader->depth-2]:NULL;char text[160];const char *key=scope->key;
    if(event==1){int kind=parent?parent->kind:0;
        if(kind==AP_TOP&&!strcmp(parent->key,"params"))scope->kind=AP_PARAMS;
        else if(kind==AP_TOP&&!strcmp(parent->key,"result"))scope->kind=AP_RESULT;
        else if(kind==AP_PARAMS&&!strcmp(parent->key,"change"))scope->kind=AP_CHANGE;
        else if(kind==AP_CHANGE&&!strcmp(parent->key,"conversationState"))scope->kind=AP_SNAPSHOT;
        else if(kind==AP_SNAPSHOT&&!strcmp(parent->key,"threadRuntimeStatus"))scope->kind=AP_STATUS;
        else if(kind==AP_STATUS&&!strcmp(parent->key,"activeFlags")){scope->kind=AP_FLAGS;m->statusTouched=1;m->approval=m->userInput=0;}
        else if(kind==AP_CHANGE&&!strcmp(parent->key,"patches"))scope->kind=AP_PATCHES;
        else if(kind==AP_PATCHES){scope->kind=AP_PATCH;m->path[0]=m->field[0]=0;}
        else if(kind==AP_PATCH&&!strcmp(parent->key,"path"))scope->kind=AP_PATH;
        else if(kind==AP_PATCH&&!strcmp(parent->key,"value")&&!strcmp(m->path,"threadRuntimeStatus")){
            if(!m->field[0])scope->kind=AP_STATUS;
            else if(!strcmp(m->field,"activeFlags")){scope->kind=AP_FLAGS;m->statusTouched=1;m->approval=m->userInput=0;}
        }
        else if((kind==AP_SNAPSHOT&&!strcmp(parent->key,"requests"))||(kind==AP_PATCH&&!strcmp(parent->key,"value")&&!strcmp(m->path,"requests")&&!m->field[0])){
            scope->kind=AP_REQUESTS;m->requestTouched=1;m->requestApproval=m->requestInput=0;
        }else if(kind==AP_REQUESTS)scope->kind=AP_REQUEST;
        return;
    }
    if(event==2){if(scope->kind==AP_PATCH&&!strcmp(m->path,"requests")&&m->field[0])m->resync=1;return;}
    if(!value)return;if(*value=='"'){if(!stringValue(value,text,sizeof(text)))return;}else {strncpy(text,value,159);text[159]=0;}
#define AP_COPY(k,f) if(!strcmp(key,k)){strncpy(m->f,text,sizeof(m->f)-1);}
    if(scope->kind==AP_TOP){AP_COPY("type",type)else AP_COPY("method",method)else if(!strcmp(key,"version"))m->version=atoi(text);}
    else if(scope->kind==AP_RESULT){AP_COPY("clientId",client)}
    else if(scope->kind==AP_PARAMS){AP_COPY("conversationId",id)else AP_COPY("hostId",host)}
    else if(scope->kind==AP_CHANGE){AP_COPY("type",change)else if(!strcmp(key,"revision"))m->revision=_strtoi64(text,NULL,10);else if(!strcmp(key,"baseRevision"))m->baseRevision=_strtoi64(text,NULL,10);}
#undef AP_COPY
    else if(scope->kind==AP_PATH){if(scope->index==0)strncpy(m->path,text,79);else if(scope->index==1)strncpy(m->field,text,79);}
    else if(scope->kind==AP_STATUS&&!strcmp(key,"type")&&strcmp(text,"active")){m->statusTouched=1;m->approval=m->userInput=0;}
    else if(scope->kind==AP_FLAGS){m->statusTouched=1;if(!strcmp(text,"waitingOnApproval"))m->approval=1;if(!strcmp(text,"waitingOnUserInput"))m->userInput=1;}
    else if(scope->kind==AP_REQUEST&&!strcmp(key,"method")){m->requestApproval|=permissionMethod(text);m->requestInput|=!strcmp(text,"item/tool/requestUserInput");}
    else if(scope->kind==AP_PATCH&&!strcmp(key,"value")&&!strcmp(m->path,"threadRuntimeStatus")&&(!strcmp(m->field,"type")&&strcmp(text,"active")||!strcmp(text,"null"))){m->statusTouched=1;m->approval=m->userInput=0;}
}
static int approvalWrite(const char *json){DWORD got,bytes=(DWORD)strlen(json);char frame[1200];if(approvalPipe==INVALID_HANDLE_VALUE||bytes>sizeof(frame)-4)return 0;memcpy(frame,&bytes,4);memcpy(frame+4,json,bytes);return WriteFile(approvalPipe,frame,bytes+4,&got,NULL)&&got==bytes+4;}
static int approvalFollow(const Chat *c,int follow){char json[800];if(!approvalClient[0])return 0;
    _snprintf(json,sizeof(json),"{\"type\":\"broadcast\",\"method\":\"thread-stream-following-changed\",\"sourceClientId\":\"%s\",\"version\":1,\"params\":{\"conversationId\":\"%s\",\"hostId\":\"local\",\"following\":%s}}",approvalClient,c->id,follow?"true":"false");return approvalWrite(json);
}
static void approvalForget(const Chat *c){if(c->ipcSubscribed)approvalFollow(c,0);}
static void approvalStop(void){int i;
    if(approvalPipe!=INVALID_HANDLE_VALUE){for(i=0;i<chatCount;i++)approvalForget(&chats[i]);CloseHandle(approvalPipe);approvalPipe=INVALID_HANDLE_VALUE;}
    if(approvalReader)VirtualFree(approvalReader,0,MEM_RELEASE);approvalReader=NULL;approvalClient[0]=0;approvalHeadBytes=approvalLength=approvalRead=0;approvalProtocolOk=0;
    for(i=0;i<chatCount;i++){Chat *c=&chats[i];c->approvalPending=c->liveUserInput=c->ipcSubscribed=c->ipcFlagApproval=c->ipcRequestApproval=0;c->ipcRevision=0;}
}
static void approvalCommit(void){ApprovalMessage *m=&approvalMessage;int i;if(!strcmp(m->type,"response")&&!strcmp(m->method,"initialize")&&m->client[0]){
        if(!strpbrk(m->client,"\"\\\r\n")){strncpy(approvalClient,m->client,79);approvalProtocolOk=1;}return;
    }
    if(strcmp(m->type,"broadcast")||strcmp(m->method,"thread-stream-state-changed")||strcmp(m->host,"local")||m->version!=11)return;
    for(i=0;i<chatCount;i++)if(!strcmp(chats[i].id,m->id)){Chat *c=&chats[i];if(!c->ipcSubscribed)return;
        if(!strcmp(m->change,"patches")&&m->baseRevision!=c->ipcRevision){c->ipcSubscribed=0;return;}
        if(!strcmp(m->change,"snapshot")){c->ipcFlagApproval=c->ipcRequestApproval=c->liveUserInput=0;}
        else if(strcmp(m->change,"patches"))return;
        if(m->statusTouched){c->ipcFlagApproval=m->approval;c->liveUserInput=m->userInput;}
        if(m->requestTouched){c->ipcRequestApproval=m->requestApproval;c->liveUserInput|=m->requestInput;}
        c->approvalPending=c->ipcFlagApproval||c->ipcRequestApproval;c->ipcRevision=m->revision;
        if(m->resync)c->ipcSubscribed=0;return;
    }
}
static void approvalFeed(const BYTE *data,DWORD size){DWORD i;for(i=0;i<size;i++){
    if(!approvalLength){approvalHead[approvalHeadBytes++]=data[i];if(approvalHeadBytes==4){memcpy(&approvalLength,approvalHead,4);approvalHeadBytes=0;approvalRead=0;memset(approvalReader,0,sizeof(*approvalReader));approvalReader->observer=approvalObserve;memset(&approvalMessage,0,sizeof(approvalMessage));if(!approvalLength||approvalLength>268435456){approvalStop();return;}}continue;}
    streamFeed(approvalReader,data[i]);if(++approvalRead==approvalLength){if(streamFinish(approvalReader)){approvalFrames++;approvalCommit();}approvalLength=approvalRead=0;}
}}
static void approvalPoll(void){BYTE data[IO_CAP];DWORD available,got,total=0,start=GetTickCount();int i;
    if(approvalPipe==INVALID_HANDLE_VALUE){if((LONG)(start-approvalRetryAt)<0)return;approvalRetryAt=start+10000;
        approvalPipe=CreateFileW(L"\\\\.\\pipe\\codex-ipc",GENERIC_READ|GENERIC_WRITE,0,NULL,OPEN_EXISTING,0,NULL);if(approvalPipe==INVALID_HANDLE_VALUE)return;
        approvalReader=VirtualAlloc(NULL,sizeof(StreamRecord),MEM_RESERVE|MEM_COMMIT,PAGE_READWRITE);if(!approvalReader){approvalStop();return;}approvalReconnects++;
        if(!approvalWrite("{\"type\":\"request\",\"requestId\":\"tray-pet-init\",\"sourceClientId\":\"initializing-client\",\"version\":0,\"method\":\"initialize\",\"params\":{\"clientType\":\"desktop\"}}")){approvalStop();return;}
    }
    while(total<4*1024*1024&&GetTickCount()-start<50){
        if(!PeekNamedPipe(approvalPipe,NULL,0,NULL,&available,NULL)){approvalStop();return;}if(!available)break;
        if(!ReadFile(approvalPipe,data,available>sizeof(data)?sizeof(data):available,&got,NULL)||!got){approvalStop();return;}total+=got;approvalFeed(data,got);if(approvalPipe==INVALID_HANDLE_VALUE)return;
    }
    if(approvalClient[0])for(i=0;i<chatCount;i++){Chat *c=&chats[i];int follow=(c->mood==WORK||c->mood==QUESTION||c->approvalPending||c->liveUserInput)&&(!desktopStartedAt||c->latestAt>=desktopStartedAt);
        if(follow&&!c->ipcSubscribed){if(!approvalFollow(c,1)){approvalStop();return;}c->ipcSubscribed=1;}
        else if(!follow&&c->ipcSubscribed){approvalForget(c);c->ipcSubscribed=0;c->approvalPending=c->liveUserInput=c->ipcFlagApproval=c->ipcRequestApproval=0;}
    }
}
#include "approval-tests.h"
