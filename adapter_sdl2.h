#ifndef _ADAPTER_SDL2_H_
#define _ADAPTER_SDL2_H_

#include <SDL2/SDL.h>
#include <assert.h>
#include <pthread.h>

#include "assert_out_ns_vim.h"
#include "begin_ns_vim.h"

enum adapter_event_type
{
    ADAPT_EVENT_BLINK,
    ADAPT_EVENT_TIMER,
    ADAPT_EVENT_KEYDOWN,
    ADAPT_EVENT_FLUSH,
    ADAPT_EVENT_REDRAW,
    ADAPT_EVENT_KILLFOCUS,
    ADAPT_EVENT_SETFOCUS,
    ADAPT_EVENT_TEXTAREA_RESIZE,
    ADAPT_EVENT_MOUSEBUTTON_DOWN,
    ADAPT_EVENT_MOUSEBUTTON_UP,
    ADAPT_EVENT_QUITALL,
    ADAPT_EVENT_SETCELLSIZE
};

typedef struct adapter_event_t
{
    int type;
    void (*func)(int type, void *ud0, void *ud1);
    void *user_data0;
    void *user_data1;
} adapter_event_t;

extern int adapter_has_event();
extern void adapter_push_event(adapter_event_t evnt);
extern int adapter_poll_event(adapter_event_t* pevnt); 
extern int adapter_get_event(adapter_event_t* pevnt); // blocking
extern void adapter_wait_for_event(); // blocking

typedef adapter_event_t VIM_Event;
/* Event queue consumed by vim core, working in the same thread as vim core.
 * Other threads can push events into this queue because this class is using
 * lock internally.
 */
class VimEventQueue //singlton
{
public:
    static VimEventQueue* get();

    VimEventQueue() {
        assert(mInstance==NULL); // user must not call this!
        mInstance=this;
    }

    bool hasEvent();
    void pushEvent(VIM_Event evnt);
    bool pollEvent(VIM_Event* evnt);
    void getEvent(VIM_Event* evnt);
    void waitForEvent();
private:
    void lock();
    void unlock();

    static VimEventQueue* mInstance;
};

extern int info_has_message();
extern void info_push_message(const char *msg);
extern void info_push_messagef(const char* msg, ...);
extern int info_poll_message(char *buf, int *len, int maxlen);
extern int info_message_number();

typedef struct Rect_f
{
    double x,y,w,h;
} Rect_f;

enum display_task_type
{
    DISP_TASK_TEXTOUT,
    DISP_TASK_SCROLL,
    DISP_TASK_HOLLOWCURSOR,
    DISP_TASK_PARTCURSOR,
    DISP_TASK_SETCOLOR,
    DISP_TASK_INVERTRECT,
    DISP_TASK_CLEARRECT,
    DISP_TASK_SETFONT,
    DISP_TASK_SETCELLSIZE,
    DISP_TASK_RESIZE,
    DISP_TASK_QUITVIM,
    DISP_TASK_DRAWLINE,
    DISP_TASK_BEEP,
    DISP_TASK_UNDERCURL,
    DISP_TASK_FLUSH,
    DISP_TASK_REQUIREESC,
    DISP_TASK_ADDTIMER, // for blink timer (blink_timer @ gui_w48.c)
    DISP_TASK_ADDTIMER2 // for wait timer (s_wait_timer @ gui_w48.c)
};

typedef struct disp_task_textout_t
{
    Uint32 type;
    char* text;
    int len;
    double x,y,w,h;
} disp_task_textout_t;

typedef struct disp_task_scroll_t
{
    Uint32 type;
    double distance; // negative for scrolling up. postive for scrolling down.
    Rect_f scroll_rect, clip_rect;
    int flags;
} disp_task_scroll_t;

typedef struct disp_task_hollowcursor_t
{
    Uint32 type;
    Rect_f rect;
    SDL_Color color;
} disp_task_hollowcursor_t;

typedef struct disp_task_partcursor_t
{
    Uint32 type;
    Rect_f rect;
    SDL_Color color;
} disp_task_partcursor_t;

enum 
{
    DISP_TASK_SETCOLORFG,
    DISP_TASK_SETCOLORBG,
    DISP_TASK_SETCOLORSP
};

typedef struct disp_task_setcolor_t
{
    Uint32 type;
    Uint32 which;
    SDL_Color color;
} disp_task_setcolor_t;

typedef struct disp_task_invertrect_t
{
    Uint32 type;
    Rect_f rect;
} disp_task_invertrect_t;

typedef struct disp_task_clearrect_t
{
    Uint32 type;
    Rect_f rect;
} disp_task_clearrect_t;

enum {DISP_TASK_SETFONTNORMAL, DISP_TASK_SETFONTWIDE};
enum { // font style. synchronized with sdl_ttf's font style
    DISP_TASK_SETFONT_NOSTYLE = 0,
    DISP_TASK_SETFONT_BOLD = 1, 
    DISP_TASK_SETFONT_ITALIC =2
};
typedef struct disp_task_setfont_t
{
    Uint32 type;
    Uint32 which;
    Uint32 flags;
} disp_task_setfont_t;

typedef struct disp_task_setcellsize_t
{
    Uint32 type;
    int cell_width, cell_height;
} disp_task_setcellsize_t;

typedef struct disp_task_resize_t
{
    Uint32 type;
    int w, h;
} disp_task_resize_t;

typedef struct disp_task_quitvim_t
{
    Uint32 type;
} disp_task_quitvim_t;

typedef struct disp_task_drawline_t
{
    Uint32 type;
    double x1, y1, x2, y2;
} disp_task_drawline_t;

typedef struct disp_task_beep_t
{
    Uint32 type;
} disp_task_beep_t;

typedef struct disp_task_undercurl_t
{
    Uint32 type;
    double x,y,w;
} disp_task_undercurl_t;

typedef struct disp_task_flush_t
{
    Uint32 type;
} disp_task_flush_t;

// only used for quitting sync between vim thread and sdl thread.
typedef struct disp_task_requireesc_t
{
    Uint32 type;
} disp_task_requireesc_t;

typedef struct disp_task_addtimer_t
{
    Uint32 type;
    int time_to_delay;
    void* ud;
    SDL_TimerCallback callback;
} disp_task_addtimer_t;

typedef union disp_task_t
{
    Uint32 type;
    disp_task_textout_t textout;
    disp_task_scroll_t scroll;
    disp_task_hollowcursor_t hollowcursor;
    disp_task_partcursor_t partcursor;
    disp_task_setcolor_t setcolor;
    disp_task_invertrect_t invertrect;
    disp_task_clearrect_t clearrect;
    disp_task_setfont_t setfont;
    disp_task_setcellsize_t setcellsize;
    disp_task_resize_t resize;
    disp_task_quitvim_t quitvim;
    disp_task_drawline_t drawline;
    disp_task_beep_t beep;
    disp_task_undercurl_t undercurl;
    disp_task_flush_t flush;
    disp_task_requireesc_t requireesc;
    disp_task_addtimer_t addtimer;
} disp_task_t;

extern int display_has_task();
extern void display_push_task(const disp_task_t *task);
extern int display_poll_task(disp_task_t* task);
extern void display_task_cleanup(disp_task_t *task);

// this function is called from vim core thread, so this cannot call SDL_*
// functions.
extern int display_getTicks();

// helper initializers.
extern void 
display_fill_textout(disp_task_textout_t *textout, 
        double x, double y, double w, double h,
        const char* text, int len);
extern void 
display_extract_textout(disp_task_textout_t *textout,
        double* x, double* y, double* w, double* h,
        char* buf, int* len, int maxlen);
extern void
display_set_font(Uint32 which, Uint32 flags);

extern bool is_valid_utf8(const char* start, const char* end);

extern int utf8_distance(const char* start, const char* end);

// num_utf8_chars is the same as utf8_distance.
extern int num_utf8_chars(const char* start, const char* end);

extern int utf8_ptr2char(char* ptr);
extern int num_string_cells(char* start, char* end);
#define myRGB(r,g,b) ((((r)&0xff)<<16)|(((g)&0xff)<<8)|((b)&0xff))
#define myGetRValue(c) (((c)>>16)&0xff)
#define myGetGValue(c) (((c)>>8)&0xff)
#define myGetBValue(c) ((c)&0xff)


#include "end_ns_vim.h"
#endif //_ADAPTER_SDL2_H_

