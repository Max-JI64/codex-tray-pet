static void detectionSnapshot(void){WCHAR path[1100];FILE *f;int i,n=0;_snwprintf(path,1100,L"%ls\\detection-live.json",folder);f=_wfopen(path,L"wb");if(!f)return;
    fprintf(f,"{\"pid\":%lu,\"state\":%d,\"desktopStartedAt\":%lld,\"recordBufferBytes\":%d,\"rows\":[",GetCurrentProcessId(),mood,desktopStartedAt,LINE_CAP);
    for(i=0;i<chatCount;i++)if(latestChat(i)){Chat *c=&chats[i];if(n++)fprintf(f,",");fprintf(f,"{\"id\":\"%s\",\"state\":%d,\"active\":%s,\"question\":%s,\"unknown\":%s,\"finished\":%s,\"ready\":%s,\"startedAt\":%lld,\"latestAt\":%lld,\"readOffset\":%lld}",c->id,c->mood,activeChat(c)?"true":"false",askingChat(c)?"true":"false",c->unknown?"true":"false",c->finished?"true":"false",c->ready?"true":"false",c->startedAt,c->latestAt,c->offset);}
    fprintf(f,"]}");fclose(f);
}
static void fixturePadding(FILE *f,int size){char data[IO_CAP];memset(data,'x',sizeof(data));while(size>0){int take=size>(int)sizeof(data)?sizeof(data):size;fwrite(data,1,take,f);size-=take;}}
static int detectionTest(void){WCHAR path[1100],input[1100];FILE *f,*data;Chat c={0},pair[2];int failed=0;StreamRecord *stream;const char *example;int i;
    _snwprintf(path,1100,L"%ls\\detection-verification.txt",folder);f=_wfopen(path,L"wb");if(!f||!ensureChats())return 1;
    _snwprintf(input,1100,L"%ls\\detection-test-input.tmp",folder);wcscpy(c.path,input);data=_wfopen(input,L"wb");if(!data){fclose(f);return 1;}
    fprintf(data,"{\"type\":\"session_meta\",\"payload\":{\"id\":\"long-running\",\"originator\":\"Codex Desktop\",\"source\":\"vscode\"}}\n");
    fprintf(data,"{\"timestamp\":\"2026-10-03T06:39:16.108Z\",\"type\":\"event_msg\",\"payload\":{\"type\":\"task_started\",\"turn_id\":\"long-turn\"}}\n");
    fprintf(data,"{\"timestamp\":\"2026-10-03T06:40:00.000Z\",\"type\":\"compacted\",\"replacement_history\":[{\"content\":[{\"type\":\"input_image\",\"image_url\":\"data:image/png;base64,");fixturePadding(data,6*1024*1024);fprintf(data,"\"}]}]}\n");
    fprintf(data,"{\"timestamp\":\"2026-10-03T06:41:00.000Z\",\"type\":\"event_msg\",\"payload\":{\"type\":\"item_completed\",\"item\":{\"output\":\"task_complete is quoted text ");fixturePadding(data,400000);fprintf(data,"\"}}}\n");
    fprintf(data,"{\"timestamp\":\"2026-10-03T06:42:00.000Z\",\"type\":\"response_item\",\"payload\":{\"type\":\"function_call_output\",\"call_id\":\"tool\",\"output\":\"normal output ");fixturePadding(data,400000);fprintf(data,"\\uC138\\uC158 ");fixturePadding(data,100000);fprintf(data,"\"}}\n");fclose(data);
    readChatMetadata(&c);readChat(&c,1);
    failed+=check(c.allowed&&c.mood==WORK&&!c.unknown&&c.startedAt==isoSeconds("2026-10-03T06:39:16.108Z"),"boot recovers active start before 6MiB image and large tool output",f);
    data=_wfopen(input,L"ab");fprintf(data,"{\"timestamp\":\"2026-10-03T06:43:00.000Z\",\"type\":\"event_msg\",\"payload\":{\"type\":\"task_complete\",\"turn_id\":\"long-turn\"}}\n");fclose(data);readChat(&c,0);
    pair[0]=c;memset(&pair[1],0,sizeof(Chat));pair[1].allowed=1;pair[1].mood=IDLE;
    failed+=check(c.mood==DONE&&c.ready&&!c.unknown&&aggregate(pair,2,0)==DONE,"completion becomes green after large content instead of sticky gray",f);
    memset(&c,0,sizeof(c));wcscpy(c.path,input);readChatMetadata(&c);readChat(&c,1);failed+=check(c.finished&&!c.unknown&&c.mood==IDLE,"completed history restored without false running or unknown",f);
    data=_wfopen(input,L"ab");fprintf(data,"{\"type\":\"event_msg\",\"payload\":{\"type\":\"task_started\",\"turn_id\":\"question-turn\"}}\n{\"type\":\"response_item\",\"payload\":{\"type\":\"function_call\",\"name\":\"functions.request_user_input_async\",\"call_id\":\"ask\"}}\n");fclose(data);readChat(&c,0);failed+=check(c.mood==QUESTION&&askingChat(&c),"stream reader retains question session marker",f);
    data=_wfopen(input,L"ab");fprintf(data,"{\"type\":\"response_item\",\"payload\":{\"type\":\"function_call_output\",\"call_id\":\"ask\",\"output\":\"ack\"}}\n");fclose(data);readChat(&c,0);failed+=check(c.mood==QUESTION,"async acknowledgment preserves waiting state",f);
    data=_wfopen(input,L"ab");fprintf(data,"{\"type\":\"response_item\",\"payload\":{\"type\":\"message\",\"role\":\"user\"}}\n");fclose(data);readChat(&c,0);failed+=check(c.mood==WORK&&!askingChat(&c),"user reply removes question marker",f);
    stream=(StreamRecord*)lineBuf;memset(stream,0,sizeof(*stream));example="{\"type\":\"event_msg\",\"payload\":{\"type\":\"error\",\"willRetry\":true,\"error\":{\"codexErrorInfo\":\"UsageLimitExceeded\",\"message\":\"You've hit your usage limit\"}}}";for(i=0;example[i];i++)streamFeed(stream,example[i]);failed+=check(streamFinish(stream)&&stream->record.willRetry&&quotaCode(stream->record.errorCode)&&quotaMessage(stream->record.errorMessage),"structured retry and quota fields survive selective parsing",f);
    memset(stream,0,sizeof(*stream));example="{\"type\":\"session_meta\",\"payload\":{\"originator\":\"Codex Desktop\",\"source\":{\"subagent\":{\"other\":\"guardian\"}}}}";for(i=0;example[i];i++)streamFeed(stream,example[i]);failed+=check(streamFinish(stream)&&stream->record.sourceObject,"child source object remains distinguishable",f);
    failed+=check(LINE_CAP<32768,"record metadata storage is below 32KiB regardless of content length",f);
    DeleteFileW(input);fclose(f);freeChats();return failed?1:0;
}
