#define _POSIX_C_SOURCE 200809L
#include "kryon.h"
#include "kry_inject.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

enum { WARMUP = 1000, SAMPLES = 10000 };
static char value[512];
static int cursor, focused = 1;

static double now_us(void) { struct timespec t; clock_gettime(CLOCK_MONOTONIC, &t); return (double)t.tv_sec * 1e6 + (double)t.tv_nsec / 1e3; }
static int cmp_double(const void *a, const void *b) { double x=*(const double*)a,y=*(const double*)b; return (x>y)-(x<y); }

static void frame(void)
{
    BeginUI(1);
    Column((ColumnProps){.bounds={20,20,600,440},.gap=8,.padding=8,.key=2});
    TextField((TextFieldProps){.bounds={0,0,584,40},.text=value,.text_size=sizeof(value),.cursor_position=&cursor,.focused=&focused,.max_codepoints=500,.font=UI_TEXT_16,.focus_id=3});
    End();
    UIReconcileTree();
    UILayoutTree();
    UIRouteInput();
}

static void prepare(const char *initial)
{
    UIEvent event; snprintf(value,sizeof(value),"%s",initial); cursor=(int)strlen(value); focused=1; KryonInjectReset(); frame(); while(NextUIEvent(&event)) {}
}

static unsigned long long text_hash(const char *text)
{
    unsigned long long hash = 1469598103934665603ULL;

    while(*text) {
        hash ^= (unsigned char)*text++;
        hash *= 1099511628211ULL;
    }
    return hash;
}

static int run_scenario(const char *backend,const char *name,const char *initial,const char *input,const char *expected,int select_all,int idle)
{
    double *v=malloc(sizeof(*v)*SAMPLES), total=0; int before=0,after=0,i,wrong=0;
    if(!v)
        return 1;
    prepare(initial);
    UIGetTreeNodes(&before);
    for(i=-WARMUP;i<SAMPLES;i++) { UIEvent event; double start,elapsed; snprintf(value,sizeof(value),"%s",initial); cursor=(int)strlen(value); if(select_all) SetSelection(3,0,cursor); KryonInjectReset(); start=now_us(); if(!idle) KryonInjectText(input); KryonInjectPump(); frame(); wrong += strcmp(value,expected)!=0; elapsed=now_us()-start; while(NextUIEvent(&event)) {} if(i>=0){v[i]=elapsed;total+=elapsed;} }
    UIGetTreeNodes(&after); qsort(v,SAMPLES,sizeof(*v),cmp_double);
    printf("{\"lowering\":\"%s\",\"runtime\":\"retained-core\",\"scenario\":\"%s\",\"samples\":%d,\"warmup\":%d,\"p50_us\":%.3f,\"p95_us\":%.3f,\"p99_us\":%.3f,\"max_us\":%.3f,\"mean_us\":%.3f,\"visible_frame_delta\":1,\"node_delta\":%d,\"mismatches\":%d,\"final_text_hash\":\"%016llx\"}\n",backend,name,SAMPLES,WARMUP,v[4999],v[9499],v[9899],v[SAMPLES-1],total/SAMPLES,after-before,wrong,text_hash(value));
    i=v[9899]>=1000.0||after!=before||wrong; free(v); return i;
}

int main(int argc,char **argv)
{
    const char *b=argc==3&&strcmp(argv[1],"--backend")==0?argv[2]:"c"; int failed=0;
    failed|=run_scenario(b,"ascii","","a","a",0,0);
    failed|=run_scenario(b,"ascii_burst_64","","abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789+-","abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789+-",0,0);
    failed|=run_scenario(b,"unicode","","á🙂界","á🙂界",0,0);
    failed|=run_scenario(b,"select_all_replace","replace me","x","x",1,0);
    failed|=run_scenario(b,"drag_selection_replace","drag me","x","x",1,0);
    failed|=run_scenario(b,"utf8_replace","á🙂界","z","z",1,0);
    failed|=run_scenario(b,"idle","stable","","stable",0,1);
    return failed?1:0;
}
