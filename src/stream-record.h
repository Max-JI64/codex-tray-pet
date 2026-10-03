/* Incremental JSON envelope reader. Large content is validated/skipped, never retained. */
typedef struct {char container,key[80];int phase,kind;} JsonScope;
typedef struct {
    Record record;JsonScope scope[64];char text[4120];
    int depth,started,done,bad,mode,keyString,capture,used,safe,escaped,unicode,truncated;
} StreamRecord;
static int envelopeField(int kind,const char *key){
    if(kind==1)return !strcmp(key,"type")||!strcmp(key,"timestamp")||!strcmp(key,"id")||!strcmp(key,"thread_name");
    if(kind==2)return !strcmp(key,"type")||!strcmp(key,"turn_id")||!strcmp(key,"name")||!strcmp(key,"call_id")||!strcmp(key,"role")||!strcmp(key,"originator")||!strcmp(key,"parent_thread_id")||!strcmp(key,"id")||!strcmp(key,"codexErrorInfo")||!strcmp(key,"codex_error_info")||!strcmp(key,"errorInfo")||!strcmp(key,"code")||!strcmp(key,"message")||!strcmp(key,"output");
    return kind==3&&(!strcmp(key,"codexErrorInfo")||!strcmp(key,"codex_error_info")||!strcmp(key,"code")||!strcmp(key,"message"));
}
static void envelopeString(StreamRecord *s,JsonScope *scope){
    Record *r=&s->record;char *out=NULL;int cap=0;const char *key=scope->key;
#define STREAM_FIELD(k,f) if(!strcmp(key,k)){out=r->f;cap=sizeof(r->f);}
    if(scope->kind==1){STREAM_FIELD("type",type)else STREAM_FIELD("timestamp",timestamp)else STREAM_FIELD("id",chatId)else STREAM_FIELD("thread_name",title)}
    else if(scope->kind==2){STREAM_FIELD("type",ev)else STREAM_FIELD("turn_id",turn)else STREAM_FIELD("name",name)else STREAM_FIELD("call_id",id)else STREAM_FIELD("role",role)else STREAM_FIELD("originator",origin)else STREAM_FIELD("parent_thread_id",parent)else STREAM_FIELD("id",chatId)else STREAM_FIELD("codexErrorInfo",errorCode)else STREAM_FIELD("codex_error_info",errorCode)else STREAM_FIELD("errorInfo",errorCode)else STREAM_FIELD("code",errorCode)else STREAM_FIELD("message",errorMessage)else STREAM_FIELD("output",toolOutput)}
    else if(scope->kind==3){STREAM_FIELD("codexErrorInfo",errorCode)else STREAM_FIELD("codex_error_info",errorCode)else STREAM_FIELD("code",errorCode)else STREAM_FIELD("message",errorMessage)}
#undef STREAM_FIELD
    if(out&&!stringValue(s->text,out,cap))s->bad=1;
}
static void streamValueDone(StreamRecord *s){if(s->depth)s->scope[s->depth-1].phase=3;else s->done=1;}
static int expectingValue(JsonScope *scope){return scope->container=='{'?scope->phase==2:scope->phase==0||scope->phase==2;}
static void streamPrimitiveDone(StreamRecord *s){char *end;JsonScope *scope=s->depth?&s->scope[s->depth-1]:NULL;
    s->text[s->used]=0;
    if(strcmp(s->text,"true")&&strcmp(s->text,"false")&&strcmp(s->text,"null")){
        if(!((s->text[0]>='0'&&s->text[0]<='9')||s->text[0]=='-'))s->bad=1;
        else {strtod(s->text,&end);if(!s->used||*end)s->bad=1;}
    }
    if(scope&&scope->kind==2&&!strcmp(scope->key,"willRetry")&&!strcmp(s->text,"true"))s->record.willRetry=1;
    s->mode=0;streamValueDone(s);
}
static void streamFeed(StreamRecord *s,unsigned char c){JsonScope *scope;
    if(s->bad)return;
    if(s->mode==2){if(c>32&&c!=','&&c!=']'&&c!='}'){
            if(s->used<(int)sizeof(s->text)-1)s->text[s->used++]=c;else s->bad=1;return;}
        streamPrimitiveDone(s);if(s->bad)return;}
    if(s->mode==1){
        if(!s->escaped&&!s->unicode&&c=='"'){
            if(s->capture){s->used=s->safe;s->text[s->used++]='"';s->text[s->used]=0;}
            scope=&s->scope[s->depth-1];
            if(s->keyString){scope->key[0]=0;if(s->capture&&!stringValue(s->text,scope->key,sizeof(scope->key)))s->bad=1;scope->phase=1;}
            else {if(s->capture)envelopeString(s,scope);streamValueDone(s);}s->mode=0;return;
        }
        if(c<32){s->bad=1;return;}
        if(s->capture&&!s->truncated){if(s->used<(int)sizeof(s->text)-8)s->text[s->used++]=c;else s->truncated=1;}
        if(s->unicode){if(!((c>='0'&&c<='9')||(c>='a'&&c<='f')||(c>='A'&&c<='F'))){s->bad=1;return;}s->unicode--;}
        else if(s->escaped){s->escaped=0;if(c=='u')s->unicode=4;else if(!strchr("\"\\/bfnrt",c)){s->bad=1;return;}}
        else if(c=='\\')s->escaped=1;
        if(!s->escaped&&!s->unicode&&!s->truncated)s->safe=s->used;return;
    }
    if(c<=32)return;
    if(s->done){s->bad=1;return;}
    if(!s->started){if(c!='{'){s->bad=1;return;}s->started=1;s->depth=1;s->scope[0].container='{';s->scope[0].kind=1;return;}
    if(!s->depth){s->bad=1;return;}scope=&s->scope[s->depth-1];
    if(c==':'&&scope->container=='{'&&scope->phase==1){scope->phase=2;return;}
    if(c==','&&scope->phase==3){scope->phase=scope->container=='{'?4:2;return;}
    if(c=='}'||c==']'){
        if(c!=(scope->container=='{'?'}':']')||(scope->phase!=0&&scope->phase!=3)){s->bad=1;return;}
        s->depth--;streamValueDone(s);return;
    }
    if(c=='"'){
        s->keyString=scope->container=='{'&&(scope->phase==0||scope->phase==4);
        if(!s->keyString&&!expectingValue(scope)){s->bad=1;return;}
        s->capture=s->keyString?scope->kind!=0:envelopeField(scope->kind,scope->key);s->used=s->safe=1;s->text[0]='"';s->escaped=s->unicode=s->truncated=0;s->mode=1;return;
    }
    if(!expectingValue(scope)){s->bad=1;return;}
    if(c=='{'||c=='['){int kind=0;
        if(s->depth==64){s->bad=1;return;}
        if(c=='{'&&scope->kind==1&&!strcmp(scope->key,"payload"))kind=2;
        if(c=='{'&&scope->kind==2&&!strcmp(scope->key,"error"))kind=3;
        if(c=='{'&&scope->kind==2&&!strcmp(scope->key,"source"))s->record.sourceObject=1;
        scope=&s->scope[s->depth++];memset(scope,0,sizeof(*scope));scope->container=c;scope->kind=kind;return;
    }
    s->mode=2;s->used=1;s->text[0]=c;
}
static int streamFinish(StreamRecord *s){if(s->mode==2)streamPrimitiveDone(s);return s->started&&s->done&&!s->bad&&!s->mode&&!s->depth;}
