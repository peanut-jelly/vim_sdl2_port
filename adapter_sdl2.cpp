#include "adapter_sdl2.h"
#include <stdarg.h>
#include <stdio.h>
#include <queue>
#include <string>
#include <pthread.h>
#include "utf8.h"
#include "mock.h"
extern "C" 
{
#include "iVim.h"
}

using std::queue;
using std::string;

static std::queue<adapter_event_t> q_event;
static pthread_mutex_t q_mutex=PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t q_cond = PTHREAD_COND_INITIALIZER;
#define lock_q() pthread_mutex_lock(&q_mutex)
#define unlock_q() pthread_mutex_unlock(&q_mutex)
int adapter_has_event()
{
int r=0;
lock_q();
r= ! q_event.empty();
unlock_q();
return r;
}

void adapter_push_event(adapter_event_t evnt)
{
lock_q();
q_event.push(evnt);
unlock_q();
pthread_cond_signal(&q_cond);
}

void adapter_wait_for_event()
{
lock_q();
if (q_event.size()==0)
    pthread_cond_wait(&q_cond, &q_mutex);
unlock_q();
}

int adapter_get_event(adapter_event_t* pevnt)
{
lock_q();
if (q_event.size()==0)
    pthread_cond_wait(&q_cond, &q_mutex);
*pevnt=q_event.front();
q_event.pop();
unlock_q();
return 1;
}

int adapter_poll_event(adapter_event_t* pevnt)
{
lock_q();
if (q_event.empty())
    {
    unlock_q();
    return 0;
    }
*pevnt=q_event.front();
q_event.pop();
unlock_q();
return 1;
}


static queue<string> q_msg;
static pthread_mutex_t m_mutex =PTHREAD_MUTEX_INITIALIZER;
#define lock_m() pthread_mutex_lock(&m_mutex)
#define unlock_m() pthread_mutex_unlock(&m_mutex)
int info_has_message()
{
int r=0;
lock_m();
r= ! q_msg.empty();
unlock_m();
return r;
}

void info_push_message(const char* msg)
{
lock_m();
q_msg.push(msg);
iVim_log(msg);
unlock_m();
}
void info_push_messagef(const char* msg, ...)
{
static char buf[200];
va_list vl;
va_start(vl, msg);
vsnprintf(buf,200,  msg, vl);
va_end(vl);
info_push_message(buf);
}

int info_poll_message(char *buf, int * len, int maxlen)
{
lock_m();
if (buf==NULL || maxlen<=0)
    {
    q_msg.pop();
    unlock_m();
    return 1;
    }
if (q_msg.empty())
    {
    unlock_m();
    return 0;
    }
int ll=q_msg.front().size();
strncpy(buf, q_msg.front().c_str(), maxlen);
if (ll>=maxlen)
    {
    if(len!=NULL) *len=maxlen-1;
    buf[maxlen-1]=0;
    }
else
    {
    if (len!=NULL) *len=ll;
    buf[ll]=0;
    }
q_msg.pop();
unlock_m();
return 1;
}

int info_message_number()
{
lock_m();
int n=q_msg.size();
unlock_m();
return n;
}



static queue<disp_task_t> q_display;
static pthread_mutex_t d_mutex=PTHREAD_MUTEX_INITIALIZER;
#define lock_d() pthread_mutex_lock(&d_mutex)
#define unlock_d() pthread_mutex_unlock(&d_mutex)
int display_has_task()
{
lock_d();
int r=! q_display.empty();
unlock_d();
return r;
}


void display_fill_textout(disp_task_textout_t *textout,
        double x, double y, double w, double h,
        const char* text, int len)
{
//the test is an ansi string, with number of bytes 'len'.
//need to convert it to utf-8
/*
static Uint16 buf_u16[400];
int len_u16=MultiByteToWideChar(CP_ACP, MB_PRECOMPOSED, test, len,
        buf_u16, 400);
static Uint8 buf_u8[400];
int len_u8=WideCharToMultiByte(CP_UTF8, WC_COMPOSITECHECK, buf_u16, len_u16,
        buf_u8, 400, NULL, NULL);
*/
// text is utf-8
char* buf=(char*)malloc(len);
if (buf==NULL)
    fnError("not enough memory");
memcpy(buf, text, len);
disp_task_textout_t dtask={DISP_TASK_TEXTOUT, 
    buf, len, 
    x,y,w,h};
*textout=dtask;
}

void display_push_task(const disp_task_t *task)
{
lock_d();
q_display.push(*task);
unlock_d();
}

int display_poll_task(disp_task_t *task)
{
lock_d();
if (q_display.empty())
    {
    unlock_d();
    return 0;
    }
*task= q_display.front();
q_display.pop();
unlock_d();
return 1;
}

void display_task_cleanup(disp_task_t *task)
{
switch (task->type)
    {
    case DISP_TASK_TEXTOUT:
        free(task->textout.text);
    }
}

void display_extract_textout(disp_task_textout_t* textout,
        double* x, double* y, double* w, double* h, 
        char* buf, int* len, int maxlen)
{
if (x) *x=textout->x;
if (y) *y=textout->y;
if (w) *w=textout->w;
if (h) *h=textout->h;

if (buf==NULL || maxlen<=0)
    {
    return;
    }
int ll=textout->len;
strncpy(buf, textout->text, maxlen);
if (ll>=maxlen)
    {
    if(len!=NULL) *len=maxlen-1;
    buf[maxlen-1]=0;
    }
else
    {
    if (len!=NULL) *len=ll;
    buf[ll]=0;
    }
}

void display_set_font(Uint32 which, Uint32 flags)
{
if ((which!=DISP_TASK_SETFONTNORMAL) &&
        (which!=DISP_TASK_SETFONTWIDE)
   )
    fnWarn("error setting font for sdl display");
disp_task_setfont_t setfont=(disp_task_setfont_t)
    {DISP_TASK_SETFONT, which, flags};
display_push_task((disp_task_t*)&setfont);
}


int is_valid_utf8(const char* start, const char* end)
{
if (utf8::find_invalid(start, end)==end)
    return 1;
else
    return 0;
}

int utf8_distance(const char* start, const char* end)
{
return utf8::distance(start, end);
}

extern "C" int mb_string2cells(unsigned char* p, int len);
int num_string_cells(char* start, char* end)
{
// call vim routine.
return mb_string2cells((unsigned char*)start, end-start);
}

int num_utf8_chars(const char* start, const char* end)
{
return utf8_distance(start, end);
}

int num_ascii_chars(const char* start, const char* end)
{
const char *ch=start;
int n=0;
while (ch<end)
    {
    if (*ch>0) n++;
    ch++;
    }
return n;
}

extern "C" int utf_ptr2char(unsigned char* p);
int utf8_ptr2char(char* ptr)
{
// call vim routine
return utf_ptr2char((unsigned char*)ptr);
}

