static void detectionSnapshot(void){WCHAR path[1100];FILE *f;int i,n=0;_snwprintf(path,1100,L"%ls\\detection-live.json",folder);f=_wfopen(path,L"wb");if(!f)return;
    {int paused=0;for(i=0;i<chatCount;i++)paused+=chats[i].paused!=0;
    fprintf(f,"{\"pid\":%lu,\"state\":%d,\"desktopStartedAt\":%lld,\"recordBufferBytes\":%d,\"trackedSessions\":%d,\"capacityExceeded\":%s,\"cachedRollouts\":%d,\"pausedSessions\":%d,\"chatCommittedBytes\":%llu,\"rolloutCacheBytes\":%u,\"logOpens\":%lu,\"pausedSkips\":%lu,\"rows\":[",GetCurrentProcessId(),mood,desktopStartedAt,LINE_CAP,chatCount,overflow?"true":"false",seenRolloutCount,paused,(ULONGLONG)chatCommitted,(unsigned)sizeof(seenRollouts),logOpens,pausedSkips);}
    for(i=0;i<chatCount;i++)if(latestChat(i)){Chat *c=&chats[i];if(n++)fprintf(f,",");fprintf(f,"{\"id\":\"%s\",\"state\":%d,\"active\":%s,\"question\":%s,\"unknown\":%s,\"finished\":%s,\"ready\":%s,\"startedAt\":%lld,\"latestAt\":%lld,\"readOffset\":%lld}",c->id,c->mood,activeChat(c)?"true":"false",askingChat(c)?"true":"false",c->unknown?"true":"false",c->finished?"true":"false",c->ready?"true":"false",c->startedAt,c->latestAt,c->offset);}
    fprintf(f,"]}");fclose(f);
}
static void fixturePadding(FILE *f,int size){char data[IO_CAP];memset(data,'x',sizeof(data));while(size>0){int take=size>(int)sizeof(data)?sizeof(data):size;fwrite(data,1,take,f);size-=take;}}
static int terminalCleanupTest(FILE *f){WCHAR path[1100];FILE *data;Chat c={0};int failed=0,i;unsigned long opens,records;SIZE_T before;freeChats();if(!ensureChats())return 1;readStateOk=1;
    _snwprintf(path,1100,L"%ls\\cleanup-test.tmp",folder);data=_wfopen(path,L"wb");if(!data)return 1;
    fputs("{\"timestamp\":\"2026-10-05T00:00:00Z\",\"type\":\"session_meta\",\"payload\":{\"id\":\"cleanup-session\",\"originator\":\"Codex Desktop\",\"source\":\"vscode\"}}\n{\"timestamp\":\"2026-10-05T00:01:00Z\",\"type\":\"event_msg\",\"payload\":{\"type\":\"task_started\",\"turn_id\":\"first\"}}\n{\"timestamp\":\"2026-10-05T00:02:00Z\",\"type\":\"event_msg\",\"payload\":{\"type\":\"task_complete\",\"turn_id\":\"first\"}}\n",data);fclose(data);
    unreadCount=1;strcpy(unreadIds[0],"cleanup-session");discoverRollout(path);
    failed+=check(chatCount==1&&chats[0].paused&&chats[0].finished&&!chats[0].turn[0]&&!chats[0].questionId[0]&&visibleChat(&chats[0]),"completion clears turn details and retains unread result",f);
    opens=logOpens;for(i=0;i<20;i++)readChat(&chats[0],0);
    failed+=check(logOpens==opens&&pausedSkips>=20,"unchanged completed log is not reopened or parsed",f);
    unreadCount=0;completionPulseUntil=0;pruneChats();
    failed+=check(chatCount==0&&seenRollout(path),"reading completion releases its session slot and retains only a change checkpoint",f);
    unreadCount=1;strcpy(unreadIds[0],"cleanup-session");discoverRollout(path);
    failed+=check(chatCount==1&&chats[0].finished&&chats[0].paused,"late unread-state update restores the completion badge from its checkpoint",f);
    unreadCount=0;pruneChats();records=recordsRead;
    data=_wfopen(path,L"ab");if(!data)return 1;fputs("{\"timestamp\":\"2026-10-05T00:03:00Z\",\"type\":\"event_msg\",\"payload\":{\"type\":\"task_started\",\"turn_id\":\"second\"}}\n",data);fclose(data);discoverRollout(path);
    failed+=check(chatCount==1&&activeChat(&chats[0])&&!chats[0].paused&&!strcmp(chats[0].turn,"second")&&recordsRead==records+1,"same-file restart resumes from the checkpoint without replaying history",f);
    data=_wfopen(path,L"ab");if(!data)return 1;fputs("{\"timestamp\":\"2026-10-05T00:04:00Z\",\"type\":\"event_msg\",\"payload\":{\"type\":\"task_failed\",\"turn_id\":\"second\"}}\n",data);fclose(data);readChat(&chats[0],0);pruneChats();opens=logOpens;readChat(&chats[0],0);
    failed+=check(chatCount==1&&chats[0].mood==ERROR_STATE&&chats[0].paused&&!chats[0].turn[0]&&logOpens==opens,"ordinary error stops detailed tracking but remains until acknowledged",f);
    chats[0].mood=IDLE;pruneChats();failed+=check(chatCount==0,"acknowledging an error releases its slot",f);
    data=_wfopen(path,L"ab");if(!data)return 1;fputs("{\"timestamp\":\"2026-10-05T00:05:00Z\",\"type\":\"event_msg\",\"payload\":{\"type\":\"task_started\",\"turn_id\":\"quota-turn\"}}\n{\"timestamp\":\"2026-10-05T00:06:00Z\",\"type\":\"event_msg\",\"payload\":{\"type\":\"task_failed\",\"turn_id\":\"quota-turn\",\"error\":{\"codex_error_info\":\"usage_limit_exceeded\"}}}\n",data);fclose(data);
    unreadCount=1;strcpy(unreadIds[0],"cleanup-session");discoverRollout(path);opens=logOpens;readChat(&chats[0],0);pruneChats();
    failed+=check(chatCount==1&&chats[0].quotaBlocked&&chats[0].paused&&visibleChat(&chats[0])&&logOpens==opens,"usage-limit interruption keeps its warning without reopening the unchanged log",f);
    quotaAckAt=nowSeconds();updateMotion();failed+=check(chats[0].quotaBlocked&&visibleChat(&chats[0])&&motion==0,"acknowledging quota flashing retains the resume marker",f);
    unreadCount=0;syncQuotaReadState();pruneChats();failed+=check(chatCount==0,"Codex read acknowledgement clears the quota marker and releases its slot",f);
    data=_wfopen(path,L"wb");if(!data)return 1;fputs("{\"timestamp\":\"2026-10-05T00:07:00Z\",\"type\":\"session_meta\",\"payload\":{\"id\":\"cleanup-session\",\"originator\":\"Codex Desktop\",\"source\":\"vscode\"}}\n{\"timestamp\":\"2026-10-05T00:07:01Z\",\"type\":\"event_msg\",\"payload\":{\"type\":\"task_started\",\"turn_id\":\"replacement\"}}\n",data);fclose(data);discoverRollout(path);
    failed+=check(chatCount==1&&activeChat(&chats[0])&&!strcmp(chats[0].turn,"replacement"),"truncated or replaced log recovers its current task",f);
    before=chatCommitted;ensureChatCapacity(MAX_CHATS);before=chatCommitted;chats[0].mood=IDLE;chats[0].ready=chats[0].unknown=0;pruneChats();
    failed+=check(chatCount==0&&chatCommitted<before&&chatCommitted==chatPages(0),"unused chat pages are decommitted after terminal cleanup",f);
    DeleteFileW(path);freeChats();return failed;
}
static int discoveryCapacityTest(FILE *f){WCHAR path[1100],oldPath[1100],newPath[1100];FILE *data;int i,failed=0;Chat candidate={0};
    for(i=0;i<MAX_CHATS+44;i++){
        _snwprintf(path,1100,L"%ls\\excluded-rollout-%d.tmp",folder,i);data=_wfopen(path,L"wb");if(!data)return 1;
        fprintf(data,"{\"timestamp\":\"2026-10-04T10:00:00.000Z\",\"type\":\"session_meta\",\"payload\":{\"id\":\"child-%d\",\"originator\":\"Codex Desktop\",\"source\":{\"subagent\":{\"other\":\"guardian\"}}}}\n",i);fclose(data);
        discoverRollout(path);DeleteFileW(path);
    }
    failed+=check(chatCount==0&&!overflow&&seenRolloutCount==MAX_CHATS+44,"300 excluded agents consume no session slots; metadata cache stays bounded",f);
    for(i=0;i<MAX_CHATS;i++){
        _snwprintf(path,1100,L"%ls\\root-rollout-%d.tmp",folder,i);data=_wfopen(path,L"wb");if(!data)return 1;
        fprintf(data,"{\"timestamp\":\"2026-10-04T10:00:00.100Z\",\"type\":\"session_meta\",\"payload\":{\"id\":\"root-%d\",\"originator\":\"Codex Desktop\",\"source\":\"vscode\"}}\n",i);
        if(!i)fprintf(data,"{\"timestamp\":\"2026-10-04T10:01:00Z\",\"type\":\"event_msg\",\"payload\":{\"type\":\"task_started\",\"turn_id\":\"old-turn\"}}\n{\"timestamp\":\"2026-10-04T10:01:01Z\",\"type\":\"event_msg\",\"payload\":{\"type\":\"turn_aborted\",\"turn_id\":\"old-turn\"}}\n");
        fclose(data);discoverRollout(path);if(!i)wcscpy(oldPath,path);else DeleteFileW(path);
    }
    failed+=check(chatCount==MAX_CHATS&&!overflow&&chats[0].mood==STOPPED,"all 256 slots remain available for desktop sessions",f);
    wcscpy(chats[0].title,L"Retained title");panelCount=1;panelRows[0]=0;
    _snwprintf(newPath,1100,L"%ls\\resumed-root-rollout.tmp",folder);data=_wfopen(newPath,L"wb");if(!data)return 1;
    fprintf(data,"{\"timestamp\":\"2026-10-04T10:00:00.900Z\",\"type\":\"session_meta\",\"payload\":{\"id\":\"root-0\",\"originator\":\"Codex Desktop\",\"source\":\"vscode\"}}\n{\"timestamp\":\"2026-10-04T10:02:00Z\",\"type\":\"event_msg\",\"payload\":{\"type\":\"task_started\",\"turn_id\":\"resumed-turn\"}}\n");fclose(data);discoverRollout(newPath);
    failed+=check(chatCount==MAX_CHATS&&!overflow&&chats[0].mood==WORK&&!wcscmp(chats[0].path,newPath)&&!wcscmp(chats[0].title,L"Retained title")&&panelRows[0]==0,"resumed rollout replaces stopped history at full capacity and retains row/title",f);
    seenRolloutCount=seenRolloutNext=0;discoverRollout(oldPath);
    failed+=check(!wcscmp(chats[0].path,newPath)&&aggregate(chats,chatCount,0)==WORK,"older rollout cannot override current work even after cache eviction",f);
    data=_wfopen(newPath,L"ab");if(!data)return 1;fprintf(data,"{\"timestamp\":\"2026-10-04T10:03:00Z\",\"type\":\"event_msg\",\"payload\":{\"type\":\"task_complete\",\"turn_id\":\"resumed-turn\"}}\n");fclose(data);readChat(&chats[0],0);
    failed+=check(chats[0].finished&&chats[0].ready&&aggregate(chats,chatCount,0)==DONE,"completion of resumed work becomes green rather than capacity gray",f);
    candidate.allowed=1;strcpy(candidate.id,"beyond-capacity");failed+=check(rolloutSlot(&candidate)==-2,"true capacity exhaustion is distinguished from excluded metadata",f);
    _snwprintf(path,1100,L"%ls\\partial-rollout.tmp",folder);data=_wfopen(path,L"wb");if(!data)return 1;fputs("{\"type\":\"session_meta\"",data);fclose(data);discoverRollout(path);
    failed+=check(!seenRollout(path),"partial metadata is retried instead of permanently ignored",f);
    DeleteFileW(path);DeleteFileW(oldPath);DeleteFileW(newPath);panelCount=0;return failed;
}
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
    DeleteFileW(input);failed+=discoveryCapacityTest(f);failed+=terminalCleanupTest(f);fclose(f);freeChats();return failed?1:0;
}
