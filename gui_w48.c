/* vi:set ts=8 sts=4 sw=4:
 *
 * VIM - Vi IMproved    by Bram Moolenaar
 *
 * Do ":help uganda"  in Vim to read copying and usage conditions.
 * Do ":help credits" in Vim to see a list of people who contributed.
 * See README.txt for an overview of the Vim source code.
 */
/*
 * gui_w48.c
 * rewrite of original gui_w48.c
 * changing a lot of things to make it work with sdl2 api.
 */

#include <SDL2/sdl.h>
#include <math.h>
#include <string.h>
#include <stdarg.h>
#include "adapter_sdl2.h"
#define Warn(msg) \
     do { \
        char _errMsg[200]; \
        snprintf(_errMsg, 200, "Warn:\n%s\nFile: %s\nLine: %d\nFunc:%s\n", \
                msg, __FILE__, __LINE__, __FUNCTION__); \
        MessageBox(s_hwnd,_errMsg, "Warn", MB_OK); \
    } while (0);
#define Error(msg) \
     do { \
        char _errMsg[200]; \
        snprintf(_errMsg, 200, "Error:\n%s\nFile: %s\nLine: %d\nFunc:%s\n", \
                msg, __FILE__, __LINE__, __FUNCTION__); \
        MessageBox(s_hwnd,_errMsg, "Error", MB_OK); \
        exit(13); \
    } while (0);
#define NotFinished() Error("Not Finished.")

/* vi:set ts=8 sts=4 sw=4:
 *
 * VIM - Vi IMproved		by Bram Moolenaar
 *				GUI support by Robert Webb
 *
 * Do ":help uganda"  in Vim to read copying and usage conditions.
 * Do ":help credits" in Vim to see a list of people who contributed.
 * See README.txt for an overview of the Vim source code.
 */
/*
 * gui_w48.c:  This file is included in gui_w16.c and gui_w32.c.
 *
 * GUI support for Microsoft Windows (Win16 + Win32 = Win48 :-)
 *
 * The combined efforts of:
 * George V. Reilly <george@reilly.org>
 * Robert Webb
 * Vince Negri
 * ...and contributions from many others
 *
 */

//#error gui-w48 in use.
#include "vim.h"
#include <pthread.h>

#include "version.h"	/* used by dialog box routine for default title */
#ifdef DEBUG
# include <tchar.h>
#endif

/* cproto fails on missing include files */
#ifndef PROTO

#ifndef __MINGW32__
# include <shellapi.h>
#endif
#if defined(FEAT_TOOLBAR) || defined(FEAT_BEVAL) || defined(FEAT_GUI_TABLINE)
# include <commctrl.h>
#endif
#ifdef WIN16
# include <commdlg.h>
# include <shellapi.h>
# ifdef WIN16_3DLOOK
#  include <ctl3d.h>
# endif
#endif
#include <windowsx.h>

#ifdef GLOBAL_IME
# include "glbl_ime.h"
#endif

#endif /* PROTO */

#ifdef FEAT_MENU
# define MENUHINTS		/* show menu hints in command line */
#endif

/* Some parameters for dialog boxes.  All in pixels. */
#define DLG_PADDING_X		10
#define DLG_PADDING_Y		10
#define DLG_OLD_STYLE_PADDING_X	5
#define DLG_OLD_STYLE_PADDING_Y	5
#define DLG_VERT_PADDING_X	4	/* For vertical buttons */
#define DLG_VERT_PADDING_Y	4
#define DLG_ICON_WIDTH		34
#define DLG_ICON_HEIGHT		34
#define DLG_MIN_WIDTH		150
#define DLG_FONT_NAME		"MS Sans Serif"
#define DLG_FONT_POINT_SIZE	8
#define DLG_MIN_MAX_WIDTH	400
#define DLG_MIN_MAX_HEIGHT	400

#define DLG_NONBUTTON_CONTROL	5000	/* First ID of non-button controls */

#ifndef WM_XBUTTONDOWN /* For Win2K / winME ONLY */
//#error no WM_XBUTTONDOWN
# define WM_XBUTTONDOWN		0x020B
# define WM_XBUTTONUP		0x020C
# define WM_XBUTTONDBLCLK	0x020D
# define MK_XBUTTON1		0x0020
# define MK_XBUTTON2		0x0040
#endif


#ifndef GET_X_LPARAM
# define GET_X_LPARAM(lp) ((int)(short)LOWORD(lp))
#endif

static void _OnPaint( HWND hwnd);
static int gui_mswin_get_menu_height(int fix_window);

static WORD		s_dlgfntheight;		/* height of the dialog font */
static WORD		s_dlgfntwidth;		/* width of the dialog font */


/* Flag that is set while processing a message that must not be interrupted by
 * processing another message. */
static int		s_busy_processing = FALSE;

static int		destroying = FALSE;	/* call DestroyWindow() ourselves */


static HINSTANCE	s_hinst = NULL;
#if !defined(FEAT_SNIFF) && !defined(FEAT_GUI)
static
#endif
HWND			s_hwnd = NULL;
static HDC		s_hdc = NULL;
static HBRUSH	s_brush = NULL;

#ifdef FEAT_TOOLBAR
static HWND		s_toolbarhwnd = NULL;
static WNDPROC		s_toolbar_wndproc = NULL;
#endif

#ifdef FEAT_GUI_TABLINE
//#error FEAT_GUI_TABLINE
static HWND		s_tabhwnd = NULL;
static WNDPROC		s_tabline_wndproc = NULL;
static int		showing_tabline = 0;
#endif

static WPARAM		s_wParam = 0;
static LPARAM		s_lParam = 0;

static HWND		s_textArea = NULL;
static UINT		s_uMsg = 0;

static char_u		*s_textfield; /* Used by dialogs to pass back strings */

static int		s_need_activate = FALSE;

/* This variable is set when waiting for an event, which is the only moment
 * scrollbar dragging can be done directly.  It's not allowed while commands
 * are executed, because it may move the cursor and that may cause unexpected
 * problems (e.g., while ":s" is working).
 */
static int allow_scrollbar = FALSE;

extern int current_font_height;	    /* this is in os_mswin.c */


static struct
{
    UINT    key_sym;
    char_u  vim_code0;
    char_u  vim_code1;
} special_keys_sdl[] =
{
#define MM(k, c1, c2) {SDL_SCANCODE_##k, c1, c2}
    MM(UP, 'k', 'u'),
    MM(DOWN, 'k', 'd'),
    MM(LEFT, 'k', 'l'),
    MM(RIGHT, 'k', 'r'),

    MM(F1,		'k', '1'),
    MM(F2,		'k', '2'),
    MM(F3,		'k', '3'),
    MM(F4,		'k', '4'),
    MM(F5,		'k', '5'),
    MM(F6,		'k', '6'),
    MM(F7,		'k', '7'),
    MM(F8,		'k', '8'),
    MM(F9,		'k', '9'),
    MM(F10,		'k', ';'),

    MM(F11,		'F', '1'),
    MM(F12,		'F', '2'),
    MM(F13,		'F', '3'),
    MM(F14,		'F', '4'),
    MM(F15,		'F', '5'),
    MM(F16,		'F', '6'),
    MM(F17,		'F', '7'),
    MM(F18,		'F', '8'),
    MM(F19,		'F', '9'),
    MM(F20,		'F', 'A'),

    MM(F21,		'F', 'B'),
#ifdef FEAT_NETBEANS_INTG
    MM(PAUSE,		'F', 'B'),	/* Pause == F21 (see gui_gtk_x11.c) */
#endif
    MM(F22,		'F', 'C'),
    MM(F23,		'F', 'D'),
    /* winuser.h defines up to F24 */
    // SDL_scancode.h defines up to F24, too.
    MM(F24,		'F', 'E'),	

    MM(HELP,		'%', '1'),
    MM(BACKSPACE,		'k', 'b'),
    MM(INSERT,		'k', 'I'),
    MM(DELETE,		'k', 'D'),
    MM(HOME,		'k', 'h'),
    MM(END,		'@', '7'),
    MM(PAGEUP,		'k', 'P'),
    MM(PAGEDOWN,		'k', 'N'),
    // no print key on my keyboard
    //MM(PRINT,		'%', '9'), 
    MM(KP_PLUS,		'K', '6'),
    MM(KP_MINUS,	'K', '7'),
    MM(KP_DIVIDE,		'K', '8'),
    MM(KP_MULTIPLY,	'K', '9'),
    // vk_separator /* Keypad Enter */
    MM(KP_ENTER,	'K', 'A'),	
    // FIXME what is a deciaml key?
    //MM(DECIMAL,	'K', 'B'),

    MM(KP_0,	'K', 'C'),
    MM(KP_1,	'K', 'D'),
    MM(KP_2,	'K', 'E'),
    MM(KP_3,	'K', 'F'),
    MM(KP_4,	'K', 'G'),
    MM(KP_5,	'K', 'H'),
    MM(KP_6,	'K', 'I'),
    MM(KP_7,	'K', 'J'),
    MM(KP_8,	'K', 'K'),
    MM(KP_9,	'K', 'L'),

    /* Keys that we want to be able to use any modifier with: */
    MM(SPACE,		' ', NUL),
    MM(TAB,		TAB, NUL),
    MM(ESCAPE,		ESC, NUL),
    // the following 2 items should not appear here
    // because i am dealing with sdl scancodes.
    //{NL,		NL, NUL},
    //{CAR,		CAR, NUL},
    MM(RETURN, NL, NUL),

    /* End of list marker: */
    {0,			0, 0}
#undef MM
};

/* Local variables */
static int		s_button_pending = -1;

/* s_getting_focus is set when we got focus but didn't see mouse-up event yet,
 * so don't reset s_button_pending. */
static int		s_getting_focus = FALSE;

static int		s_x_pending;
static int		s_y_pending;
static UINT		s_kFlags_pending;
static UINT     s_my_modifiers_pending;
static UINT		s_wait_timer = 0;   /* Timer for get char from user */
static int		s_timed_out = FALSE;
static int		dead_key = 0;	/* 0 - no dead key, 1 - dead key pressed */

#ifdef WIN3264
static OSVERSIONINFO os_version;    /* like it says.  Init in gui_mch_init() */
#endif


#if 0
#ifdef DEBUG_PRINT_ERROR
/*
 * Print out the last Windows error message
 */
    static void
print_windows_error(void)
{
    LPVOID  lpMsgBuf;

    FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM,
		  NULL, GetLastError(),
		  MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
		  (LPTSTR) &lpMsgBuf, 0, NULL);
    TRACE1("Error: %s\n", lpMsgBuf);
    LocalFree(lpMsgBuf);
}
#endif
#endif // 0

void fnError(const char *msg)
{
Error(msg);
}
void fnWarn(const char *msg)
{
Warn(msg);
}
void fnWarnf(const char* fmt, ...)
{
static char buf[200];
va_list vl;
va_start(vl, fmt);
vsnprintf(buf,200, fmt, vl);
va_end(vl);
Warn(buf);
}
void fnErrorf(const char* fmt, ...)
{
static char buf[200];
va_list vl;
va_start(vl, fmt);
vsnprintf(buf,200, fmt, vl);
va_end(vl);
Error(buf);
}

void fnError2(const char* msg1, const char* msg2)
{
static char mmm[200];
snprintf(mmm, 200, "%s %s", msg1, msg2);
Error(mmm);
}
void fnWarn2(const char* msg1, const char* msg2)
{
static char mmm[200];
snprintf(mmm, 200, "%s %s", msg1, msg2);
Warn(mmm);
}


/*
 * Cursor blink functions.
 *
 * This is a simple state machine:
 * BLINK_NONE	not blinking at all
 * BLINK_OFF	blinking, cursor is not shown
 * BLINK_ON	blinking, cursor is shown
 */

#define BLINK_NONE  0
#define BLINK_OFF   1
#define BLINK_ON    2

static int		blink_state = BLINK_NONE;
static long_u		blink_waittime = 700;
static long_u		blink_ontime = 400;
static long_u		blink_offtime = 250;
static UINT		blink_timer = 0;

    void
gui_mch_set_blinking(long wait, long on, long off)
{
    blink_waittime = wait;
    blink_ontime = on;
    blink_offtime = off;
}

static void 
myBlinkCallback_sdl(int tt, void* ud0, void* ud1);

static Uint32 
_OnBlinkTimer_sdl(Uint32 interval, void *ud)
{
adapter_event_t evnt;
evnt.type=ADAPT_EVENT_BLINK;
evnt.func=myBlinkCallback_sdl;
evnt.user_data0 = evnt.user_data1 = 0;
adapter_push_event(evnt);
return 0; // stop current timer.
}


// args unused.
static void 
myBlinkCallback_sdl(int tt, void* ud0, void* ud1)
{
    if (blink_state == BLINK_ON)
        {
        gui_undraw_cursor();
        blink_state = BLINK_OFF;
        blink_timer = SDL_AddTimer(blink_offtime, _OnBlinkTimer_sdl, 0);
        }
    else
        {
        gui_update_cursor(TRUE, FALSE);
        blink_state = BLINK_ON;
        blink_timer = SDL_AddTimer(blink_ontime, _OnBlinkTimer_sdl, 0);
        }
}


    static void
gui_sdl_rm_blink_timer(void)
{
if (blink_timer!=0)
    {
    SDL_RemoveTimer(blink_timer);
    blink_timer=0;
    }

}

/*
 * Stop the cursor blinking.  Show the cursor if it wasn't shown.
 */
    void
gui_mch_stop_blink(void)
{
    gui_sdl_rm_blink_timer();
    if (blink_state == BLINK_OFF)
	gui_update_cursor(TRUE, FALSE);
    blink_state = BLINK_NONE;
}

/*
 * Start the cursor blinking.  If it was already blinking, this restarts the
 * waiting time and shows the cursor.
 */
    void
gui_mch_start_blink(void)
{
    if (blink_waittime && blink_ontime && blink_offtime && gui.in_focus)
    {
    blink_timer=SDL_AddTimer(blink_waittime, _OnBlinkTimer_sdl, 0);
    blink_state=BLINK_ON;
    gui_update_cursor(TRUE,FALSE);
    }
}

static void
myTimerCallback(int tt, void* ud0, void* ud1)
{
s_timed_out=TRUE;
s_wait_timer=0;
}

static int
_OnTimer_sdl(Uint32 interval, void* ud)
{
adapter_event_t evnt;
evnt.type=ADAPT_EVENT_TIMER;
evnt.func=myTimerCallback;
evnt.user_data0 = evnt.user_data1 = 0;
adapter_push_event(evnt);
return 0;
}

/* original requirements are: */
/*
 * Convert Unicode character "ch" to bytes in "string[slen]".
 * When "had_alt" is TRUE the ALT key was included in "ch".
 * Return the length.
 */
/* i am using a normal keyboard, so everything is in 1 byte */
    static int
char_to_string(int ch, char_u *string, int slen, int had_alt)
{
// if ch<0 then the least significant byte is the actual ch.
if (ch>0)
    {
    string[0]=ch;
    return 1;
    }

//else  ch has control flag.
ch &= 0xff;
// ctrl-key will ignore case if it is a letter.
if ('a'<=ch && ch<='z')
    ch=ch-'a'+'A';

if ('A'<=ch && ch<='Z')
    ch=ch-'A'+1; // see ctrl-key mappings by vim command :digraph
else
    {
    switch (ch)
        {
#define mm(cc, val) case cc: ch=val; break;
        mm('@', 0);
        mm('[', 27);
        mm('\\' , 28);
        mm(']', 29);
        mm('^', 30);
        mm('_', 31);
#undef mm
        }
    }
string[0]=ch;
return 1;
}


static void 
_my_OnMouseEvent(int button, int x, int y, int repeated_click, 
        int vim_modifiers)
{
    s_getting_focus = FALSE;
    gui_send_mouse_event(button, x, y, repeated_click, vim_modifiers);
}

int sdl_kmod_to_vim_modifiers(Uint32 kmod)
{
int  vmod= 0;
if (kmod & KMOD_SHIFT) vmod |= MOUSE_SHIFT;
if (kmod & KMOD_CTRL) vmod |=MOUSE_CTRL;
if (kmod & KMOD_ALT) vmod |=  MOUSE_ALT;
return vmod;
}

void _on_mousebuttondown_callback(int tt, void* ud0, void* ud1)
{
// p is allocated in the caller (malloc). Must free it when done with it
// in this function
const SDL_MouseButtonEvent *p=(SDL_MouseButtonEvent*)ud0;
// p->clicks not used here, because vim is not relying on OS to
// translate double-click.
int x=p->x, y=p->y;

Uint32 sdlModKey = (Uint32)ud1; // SDL_Keymod
int modifiers = sdl_kmod_to_vim_modifiers(sdlModKey);
int keyFlags = 0;

    static LONG	s_prevTime = 0;
    LONG    currentTime = SDL_GetTicks();
    int	    button = -1;
    int	    repeated_click;

switch (p->button)
    {
    case SDL_BUTTON_LEFT:
        button=MOUSE_LEFT;
        break;
    case SDL_BUTTON_MIDDLE:
        button=MOUSE_MIDDLE;
        break;
    case SDL_BUTTON_RIGHT:
        button=MOUSE_RIGHT;
        break;
    case SDL_BUTTON_X1:
        button=MOUSE_X1;
        break;
    case SDL_BUTTON_X2:
        button=MOUSE_X2;
        break;
    }

    if (button >= 0)
        {
        repeated_click = ((int)(currentTime - s_prevTime) < p_mouset);

        /*
         * Holding down the left and right buttons simulates pushing the middle
         * button.
         */
        if (repeated_click
                && ((button == MOUSE_LEFT && s_button_pending == MOUSE_RIGHT)
                    || (button == MOUSE_RIGHT
                        && s_button_pending == MOUSE_LEFT)))
            {
            /*
             * Hmm, gui.c will ignore more than one button down at a time, so
             * pretend we let go of it first.
             */
            gui_send_mouse_event(MOUSE_RELEASE, x, y, FALSE, 0x0);
            button = MOUSE_MIDDLE;
            repeated_click = FALSE;
            s_button_pending = -1;
            _my_OnMouseEvent(button, x, y, repeated_click, modifiers);
            }
        else if ((repeated_click)
                || (mouse_model_popup() && (button == MOUSE_RIGHT)))
            {
            if (s_button_pending > -1)
                {
                _my_OnMouseEvent(s_button_pending, x, y, FALSE, modifiers);
                s_button_pending = -1;
                }
            /* TRACE("Button down at x %d, y %d\n", x, y); */
            _my_OnMouseEvent(button, x, y, repeated_click, modifiers);
            }
        else
            {
            /*
             * If this is the first press (i.e. not a multiple click) don't
             * action immediately, but store and wait for:
             * i) button-up
             * ii) mouse move
             * iii) another button press
             * before using it.
             * This enables us to make left+right simulate middle button,
             * without left or right being actioned first.  The side-effect is
             * that if you click and hold the mouse without dragging, the
             * cursor doesn't move until you release the button. In practice
             * this is hardly a problem.
             */
            s_button_pending = button;
            s_x_pending = x;
            s_y_pending = y;
            s_my_modifiers_pending = modifiers;
            //s_kFlags_pending = keyFlags;
            }

        s_prevTime = currentTime;
        }
    free(p);
}


void
_on_mouse_move_or_release_callback(int tt, void* ud0, void* ud1)
{
SDL_Event *p = (SDL_Event*)ud0;
int x=p->motion.x, y=p->motion.y;
int button;
int keyFlags=0;

s_getting_focus = FALSE;
if (s_button_pending > -1)
    {
    /* Delayed action for mouse down event */
    _my_OnMouseEvent(s_button_pending, s_x_pending,
            s_y_pending, FALSE, s_my_modifiers_pending);
    s_button_pending = -1;
    }

if (p->type==SDL_MOUSEMOTION)
    {
    /*
     * It's only a MOUSE_DRAG if one or more mouse buttons are being held
     * down.
     */
    if (!(p->motion.state & 
                (SDL_BUTTON_LMASK | SDL_BUTTON_MMASK | SDL_BUTTON_RMASK |
                 SDL_BUTTON_X1MASK | SDL_BUTTON_X2MASK)))
        {
        gui_mouse_moved(x, y);
        return;
        }

    button = MOUSE_DRAG;
    }
else
    {
    button = MOUSE_RELEASE;
    }

_my_OnMouseEvent(button, x, y, FALSE, keyFlags);
free(p);
}




/*
 * Called when the foreground or background color has been changed.
 */
    void
gui_mch_new_colors(void)
{
    /* nothing to do? */
}

/*
 * Set the colors to their default values.
 */
    void
gui_mch_def_colors()
{
    gui.norm_pixel = myRGB(0,0,0);
    gui.back_pixel = myRGB(0xff, 0xff, 0xff);
    gui.def_norm_pixel = gui.norm_pixel;
    gui.def_back_pixel = gui.back_pixel;
}

/*
 * Open the GUI window which was created by a call to gui_mch_init().
 */
    int
gui_mch_open(void)
{
return OK;
}

/*
 * Get the position of the top left corner of the window.
 */
    int
gui_mch_get_winpos(int *x, int *y)
{
*x = *y = 0;
return OK;
}

/*
 * Set the position of the top left corner of the window to the given
 * coordinates.
 */
    void
gui_mch_set_winpos(int x, int y)
{
// do nothing
}
    void
gui_mch_set_text_area_pos(int x, int y, int w, int h)
{
// do nothing
}


/*
 * Scrollbar stuff:
 */

// no use now.
    void
gui_mch_enable_scrollbar(
    scrollbar_T     *sb,
    int		    flag)
{
// do nothing
}

// no use now.
    void
gui_mch_set_scrollbar_pos(
    scrollbar_T *sb,
    int		x,
    int		y,
    int		w,
    int		h)
{
// not using this.
}

    void
gui_mch_create_scrollbar(
    scrollbar_T *sb,
    int		orient)	/* SBAR_VERT or SBAR_HORIZ */
{
// do nothing
}


static int sdl_cell_pix_width = 10,
           sdl_cell_pix_height = 15;
void adpt_set_cell_size_callback(int tt, void* ud0, void* ud1)
{
gui.char_width = sdl_cell_pix_width = (int)ud0;
gui.char_height = sdl_cell_pix_height = (int)ud1;
}

/*
 * Get the character size of a font.
 */
    static void
GetFontSize(GuiFont font)
{
// use the size provided from sdl
gui.char_width = sdl_cell_pix_width;
gui.char_height = sdl_cell_pix_height;
}

/*
 * Adjust gui.char_height (after 'linespace' was changed).
 */
    int
gui_mch_adjust_charheight(void)
{
    GetFontSize(gui.norm_font);
    return OK;
}


    int
pixels_to_points(int pixels, int vertical)
{
    int		points;
    HWND	hwnd;
    HDC		hdc;

    hwnd = GetDesktopWindow();
    hdc = GetWindowDC(hwnd);

    points = MulDiv(pixels, 72,
		    GetDeviceCaps(hdc, vertical ? LOGPIXELSY : LOGPIXELSX));

    ReleaseDC(hwnd, hdc);

    return points;
}

// this is used in many places.
// not sure whether simply returning 1 suffices.
    GuiFont
gui_mch_get_font(
    char_u	*name,
    int		giveErrorIfMissing)
{
return (GuiFont)1;
}

#if defined(FEAT_EVAL) || defined(PROTO)
/*
 * Return the name of font "font" in allocated memory.
 * Don't know how to get the actual name, thus use the provided name.
 */
/*ARGSUSED*/
    char_u *
gui_mch_get_fontname(font, name)
    GuiFont font;
    char_u  *name;
{
    if (name == NULL)
	return NULL;
    return vim_strsave(name);
}
#endif

    void
gui_mch_free_font(GuiFont font)
{
// do nothing
}

    static int
hex_digit(int c)
{
    if (VIM_ISDIGIT(c))
	return c - '0';
    c = TOLOWER_ASC(c);
    if (c >= 'a' && c <= 'f')
	return c - 'a' + 10;
    return -1000;
}
/*
 * Return the Pixel value (color) for the given color name.
 * Return INVALCOLOR for error.
 */
    guicolor_T
gui_mch_get_color(char_u *name)
{
    typedef struct guicolor_tTable
    {
	char	    *name;
	COLORREF    color;
    } guicolor_tTable;

    static guicolor_tTable table[] =
    {
	{"Black",		myRGB(0x00, 0x00, 0x00)},
	{"DarkGray",		myRGB(0xA9, 0xA9, 0xA9)},
	{"DarkGrey",		myRGB(0xA9, 0xA9, 0xA9)},
	{"Gray",		myRGB(0xC0, 0xC0, 0xC0)},
	{"Grey",		myRGB(0xC0, 0xC0, 0xC0)},
	{"LightGray",		myRGB(0xD3, 0xD3, 0xD3)},
	{"LightGrey",		myRGB(0xD3, 0xD3, 0xD3)},
	{"Gray10",		myRGB(0x1A, 0x1A, 0x1A)},
	{"Grey10",		myRGB(0x1A, 0x1A, 0x1A)},
	{"Gray20",		myRGB(0x33, 0x33, 0x33)},
	{"Grey20",		myRGB(0x33, 0x33, 0x33)},
	{"Gray30",		myRGB(0x4D, 0x4D, 0x4D)},
	{"Grey30",		myRGB(0x4D, 0x4D, 0x4D)},
	{"Gray40",		myRGB(0x66, 0x66, 0x66)},
	{"Grey40",		myRGB(0x66, 0x66, 0x66)},
	{"Gray50",		myRGB(0x7F, 0x7F, 0x7F)},
	{"Grey50",		myRGB(0x7F, 0x7F, 0x7F)},
	{"Gray60",		myRGB(0x99, 0x99, 0x99)},
	{"Grey60",		myRGB(0x99, 0x99, 0x99)},
	{"Gray70",		myRGB(0xB3, 0xB3, 0xB3)},
	{"Grey70",		myRGB(0xB3, 0xB3, 0xB3)},
	{"Gray80",		myRGB(0xCC, 0xCC, 0xCC)},
	{"Grey80",		myRGB(0xCC, 0xCC, 0xCC)},
	{"Gray90",		myRGB(0xE5, 0xE5, 0xE5)},
	{"Grey90",		myRGB(0xE5, 0xE5, 0xE5)},
	{"White",		myRGB(0xFF, 0xFF, 0xFF)},
	{"DarkRed",		myRGB(0x80, 0x00, 0x00)},
	{"Red",			myRGB(0xFF, 0x00, 0x00)},
	{"LightRed",		myRGB(0xFF, 0xA0, 0xA0)},
	{"DarkBlue",		myRGB(0x00, 0x00, 0x80)},
	{"Blue",		myRGB(0x00, 0x00, 0xFF)},
	{"LightBlue",		myRGB(0xAD, 0xD8, 0xE6)},
	{"DarkGreen",		myRGB(0x00, 0x80, 0x00)},
	{"Green",		myRGB(0x00, 0xFF, 0x00)},
	{"LightGreen",		myRGB(0x90, 0xEE, 0x90)},
	{"DarkCyan",		myRGB(0x00, 0x80, 0x80)},
	{"Cyan",		myRGB(0x00, 0xFF, 0xFF)},
	{"LightCyan",		myRGB(0xE0, 0xFF, 0xFF)},
	{"DarkMagenta",		myRGB(0x80, 0x00, 0x80)},
	{"Magenta",		myRGB(0xFF, 0x00, 0xFF)},
	{"LightMagenta",	myRGB(0xFF, 0xA0, 0xFF)},
	{"Brown",		myRGB(0x80, 0x40, 0x40)},
	{"Yellow",		myRGB(0xFF, 0xFF, 0x00)},
	{"LightYellow",		myRGB(0xFF, 0xFF, 0xE0)},
	{"DarkYellow",		myRGB(0xBB, 0xBB, 0x00)},
	{"SeaGreen",		myRGB(0x2E, 0x8B, 0x57)},
	{"Orange",		myRGB(0xFF, 0xA5, 0x00)},
	{"Purple",		myRGB(0xA0, 0x20, 0xF0)},
	{"SlateBlue",		myRGB(0x6A, 0x5A, 0xCD)},
	{"Violet",		myRGB(0xEE, 0x82, 0xEE)},
    };

    typedef struct SysColorTable
    {
	char	    *name;
	int	    color;
    } SysColorTable;

#if 0
    static SysColorTable sys_table[] =
        {
#ifdef WIN3264
            {"SYS_3DDKSHADOW", COLOR_3DDKSHADOW},
            {"SYS_3DHILIGHT", COLOR_3DHILIGHT},
#ifndef __MINGW32__
#error aiefj
            {"SYS_3DHIGHLIGHT", COLOR_3DHIGHLIGHT},
#endif
            {"SYS_BTNHILIGHT", COLOR_BTNHILIGHT},
            {"SYS_BTNHIGHLIGHT", COLOR_BTNHIGHLIGHT},
            {"SYS_3DLIGHT", COLOR_3DLIGHT},
            {"SYS_3DSHADOW", COLOR_3DSHADOW},
            {"SYS_DESKTOP", COLOR_DESKTOP},
            {"SYS_INFOBK", COLOR_INFOBK},
            {"SYS_INFOTEXT", COLOR_INFOTEXT},
            {"SYS_3DFACE", COLOR_3DFACE},
#endif
            {"SYS_BTNFACE", COLOR_BTNFACE},
            {"SYS_BTNSHADOW", COLOR_BTNSHADOW},
            {"SYS_ACTIVEBORDER", COLOR_ACTIVEBORDER},
            {"SYS_ACTIVECAPTION", COLOR_ACTIVECAPTION},
            {"SYS_APPWORKSPACE", COLOR_APPWORKSPACE},
            {"SYS_BACKGROUND", COLOR_BACKGROUND},
            {"SYS_BTNTEXT", COLOR_BTNTEXT},
            {"SYS_CAPTIONTEXT", COLOR_CAPTIONTEXT},
            {"SYS_GRAYTEXT", COLOR_GRAYTEXT},
            {"SYS_HIGHLIGHT", COLOR_HIGHLIGHT},
            {"SYS_HIGHLIGHTTEXT", COLOR_HIGHLIGHTTEXT},
            {"SYS_INACTIVEBORDER", COLOR_INACTIVEBORDER},
            {"SYS_INACTIVECAPTION", COLOR_INACTIVECAPTION},
            {"SYS_INACTIVECAPTIONTEXT", COLOR_INACTIVECAPTIONTEXT},
            {"SYS_MENU", COLOR_MENU},
            {"SYS_MENUTEXT", COLOR_MENUTEXT},
            {"SYS_SCROLLBAR", COLOR_SCROLLBAR},
            {"SYS_WINDOW", COLOR_WINDOW},
            {"SYS_WINDOWFRAME", COLOR_WINDOWFRAME},
            {"SYS_WINDOWTEXT", COLOR_WINDOWTEXT}
        };
#endif // 0

    int		    r, g, b;
    int		    i;

    if (name[0] == '#' && strlen(name) == 7)
    {
	/* Name is in "#rrggbb" format */
	r = hex_digit(name[1]) * 16 + hex_digit(name[2]);
	g = hex_digit(name[3]) * 16 + hex_digit(name[4]);
	b = hex_digit(name[5]) * 16 + hex_digit(name[6]);
	if (r < 0 || g < 0 || b < 0)
	    return INVALCOLOR;
	return myRGB(r, g, b);
    }
    else
    {
	/* Check if the name is one of the colors we know */
	for (i = 0; i < sizeof(table) / sizeof(table[0]); i++)
	    if (STRICMP(name, table[i].name) == 0)
		return table[i].color;
    }

    // do not use system color.
    /*
     * Try to look up a system colour.
     */
    /*
    for (i = 0; i < sizeof(sys_table) / sizeof(sys_table[0]); i++)
	if (STRICMP(name, sys_table[i].name) == 0)
	    return GetSysColor(sys_table[i].color);
        */

    /*
     * Last attempt. Look in the file "$VIMRUNTIME/rgb.txt".
     */
    {
#define LINE_LEN 100
	FILE	*fd;
	char	line[LINE_LEN];
	char_u	*fname;

	fname = expand_env_save((char_u *)"$VIMRUNTIME/rgb.txt");
	if (fname == NULL)
	    return INVALCOLOR;

	fd = mch_fopen((char *)fname, "rt");
	vim_free(fname);
	if (fd == NULL)
	    return INVALCOLOR;

	while (!feof(fd))
	{
	    int	    len;
	    int	    pos;
	    char    *color;

	    fgets(line, LINE_LEN, fd);
	    len = (int)STRLEN(line);

	    if (len <= 1 || line[len-1] != '\n')
		continue;

	    line[len-1] = '\0';

	    i = sscanf(line, "%d %d %d %n", &r, &g, &b, &pos);
	    if (i != 3)
		continue;

	    color = line + pos;

	    if (STRICMP(color, name) == 0)
	    {
		fclose(fd);
		return (guicolor_T) myRGB(r, g, b);
	    }
	}

	fclose(fd);
    }

    return INVALCOLOR;
}
/*
 * Return OK if the key with the termcap name "name" is supported.
 */
    int
gui_mch_haskey(char_u *name)
{
    int i;

    for (i = 0; special_keys_sdl[i].vim_code1 != NUL; i++)
	if (name[0] == special_keys_sdl[i].vim_code0 &&
					 name[1] == special_keys_sdl[i].vim_code1)
	    return OK;
    return FAIL;
#if 0
    int i;

    for (i = 0; special_keys[i].vim_code1 != NUL; i++)
	if (name[0] == special_keys[i].vim_code0 &&
					 name[1] == special_keys[i].vim_code1)
	    return OK;
    return FAIL;
#endif // 0
}

    void
gui_mch_beep(void)
{
disp_task_beep_t beep={DISP_TASK_BEEP};
display_push_task(&beep);
}
/*
 * Invert a rectangle from row r, column c, for nr rows and nc columns.
 */
    void
gui_mch_invert_rectangle(
    int	    r,
    int	    c,
    int	    nr,
    int	    nc)
{
disp_task_invertrect_t irect;
irect.type=DISP_TASK_INVERTRECT;
irect.rect=(Rect_f)
    {
    (double)c/Columns, (double)r/Rows,
        (double)nc/Columns, (double)nr/Rows
    };
display_push_task(&irect);
}

/*
 * Iconify the GUI window.
 */
    void
gui_mch_iconify(void)
{
}

/*
 * Draw a cursor without focus.
 */
    void
gui_mch_draw_hollow_cursor(guicolor_T color)
{
    // inform sdl to draw a hollow cursor.
    disp_task_hollowcursor_t hollow;
    hollow.type=DISP_TASK_HOLLOWCURSOR;
    hollow.color=(SDL_Color){myGetRValue(color), myGetGValue(color), 
        myGetBValue(color), 0xff};
    int ncell=1;
    if (mb_lefthalve(gui.row, gui.col))
        ncell=2;
    hollow.rect=(Rect_f)
        {
        (double)gui.col/Columns,
            (double)gui.row/Rows,
            (double)ncell/Columns,
            (double)1/Rows
        };
    display_push_task(&hollow);
}
/*
 * Draw part of a cursor, "w" pixels wide, and "h" pixels high, using
 * color "color".
 */
    void
gui_mch_draw_part_cursor(
    int		w,
    int		h,
    guicolor_T	color)
{

    // inform sdl to draw part cursor
    disp_task_partcursor_t part;
    part.type=DISP_TASK_PARTCURSOR;
    part.color=(SDL_Color){myGetRValue(color), myGetGValue(color), 
        myGetBValue(color), 0xff};
    part.rect=(Rect_f)
        {
        (double)gui.col/Columns,
            (double)(gui.row+(double)(gui.char_height-h)/gui.char_height)
                /Rows,
            (double)w/gui.char_width/Columns,
            (double)h/gui.char_height/Rows
        };
    display_push_task(&part);

}


static int
adjust_key_pad(int scode, int mod_sdl)
{
/*
const Uint8* ks=SDL_GetKeyboardState(NULL);
if (ks[SDL_SCANCODE_NUMLOCKCLEAR])
    info_push_messagef("numlock down t%d", SDL_GetTicks());
    */
#define MM(k) SDL_SCANCODE_##k
// if not a keypad key then return it directly
if (!((MM(KP_1)<=scode && scode<=MM(KP_0))
            || scode==MM(KP_PERIOD)
     )
   )
    return scode;

if ((KMOD_NUM & mod_sdl)) // !! numlock on.
    return scode;
// numlock is not on.
// only 11 keys need to adjust
if (!(KMOD_NUM & mod_sdl))
    {
    switch(scode)
        {
#define MT(kpk, tk) case SDL_SCANCODE_##kpk: return SDL_SCANCODE_##tk;
        MT(KP_0, INSERT);
        MT(KP_1, END);
        MT(KP_2, DOWN);
        MT(KP_3, PAGEDOWN);
        MT(KP_4, LEFT);
        // vk_clear is not considered as a special_key
        // vim seems to ignore vk_clear.
        MT(KP_5, CLEAR); 
        MT(KP_6, RIGHT);
        MT(KP_7, HOME);
        MT(KP_8, UP);
        MT(KP_9, PAGEUP);
        MT(KP_PERIOD, DELETE);
#undef MT
        }
    }
#undef MM
}

// the scode passed in can not be special_keys.
// aka. keys not in main keyboard area are all special keys.
// keypad keys are special even after adjustment.
static int mysdl_KeydownToChar(int scode, int mod_sdl)
{
#define MM(ss) SDL_SCANCODE_##ss
int ret_ch=0;
if (!(KMOD_SHIFT & mod_sdl)) // shift is not down
    {
    if (SDL_SCANCODE_1<= scode && scode<=SDL_SCANCODE_0)
        {
        if (MM(1)<=scode && scode<=MM(9))
            ret_ch=scode-MM(1)+'1';
        else ret_ch='0';
        goto the_end;
        }
    if  (MM(A)<=scode && scode<=MM(Z))
        {
        if ((mod_sdl & KMOD_CAPS)) // capslock is on shift is not down
            ret_ch=scode-MM(A)+'A';
        else // capslock is off, shift is not down
            ret_ch=scode-MM(A)+'a';
        goto the_end;
        }
    switch (scode)
        {
#define MT(sc, ch) case SDL_SCANCODE_##sc: ret_ch=ch; goto the_end;
        MT(EQUALS,'=');
        MT(COMMA,',');
        MT(MINUS,'-');
        MT(PERIOD,'.');
        MT(SEMICOLON,';'); // ';'
        MT(SLASH,'/'); // '/'
        MT(GRAVE,'`'); // '`' aka. backquote
        MT(LEFTBRACKET,'['); // '['
        MT(BACKSLASH, '\\'); // '\'
        MT(RIGHTBRACKET,']'); // ']'
        MT(APOSTROPHE,'\''); // ''' the quote mark itself
#undef MT
        }
    }
else // shift is pressed
    {
    if  (MM(A)<=scode && scode<=MM(Z))
        {
        if ((mod_sdl & KMOD_CAPS)) // capslock is on , shift is down
            ret_ch=scode-MM(A)+'a';
        else
            ret_ch=scode-MM(A)+'A';
        goto the_end;
        }
    switch (scode)
        {
#define MNU(nu, ch) case SDL_SCANCODE_##nu: ret_ch=ch; goto the_end;
        MNU(1,'!');
        MNU(2,'@');
        MNU(3,'#');
        MNU(4,'$');
        MNU(5,'%');
        MNU(6,'^');
        MNU(7,'&');
        MNU(8,'*');
        MNU(9,'(');
        MNU(0,')');
#undef MNU
        }
    switch (scode)
        {
#define MT2(vvk, ch) case SDL_SCANCODE_##vvk:  ret_ch=ch; goto the_end;
        MT2(EQUALS,'+');
        MT2(COMMA,'<');
        MT2(MINUS,'_');
        MT2(PERIOD,'>');
        MT2(SEMICOLON,':'); // ';'
        MT2(SLASH,'?'); // '/'
        MT2(GRAVE,'~'); // '`' aka. backquote
        MT2(LEFTBRACKET,'{'); // '['
        MT2(BACKSLASH,'|'); // '\'
        MT2(RIGHTBRACKET,'}'); // ']'
        MT2(APOSTROPHE,'"'); // ''' the quote mark itself
#undef MT2
        }
    }

char msg[100];
snprintf(msg, 100, "mysdl_KeydownToChar non captured scancode"
        "=%d(0x%x) mod=%d(0x%x)",scode,scode, mod_sdl, mod_sdl);
Error(msg);

the_end:
if (mod_sdl & KMOD_CTRL) // control key is down(left or right)
    {
    ret_ch|=0x80000000;
    }
return ret_ch;
#undef MM
}


void _adapter_on_sdl_keydown(int tt, void* ud0, void* ud1)
{
process_sdl_key((int)ud0, (int)ud1);
}

void process_sdl_key(int scode, int mod_sdl)
{

UINT	vk = 0;		/* Virtual key */
char_u	string[40];
int		i;
int		modifiers = 0;
int		key;

// adjust keypad keydown
vk = adjust_key_pad(scode, mod_sdl);

for (i = 0; special_keys_sdl[i].key_sym != 0; i++)
    {
    /* ignore VK_SPACE when ALT key pressed: system menu */
    if (special_keys_sdl[i].key_sym == vk
            && (vk != SDL_SCANCODE_SPACE || !(mod_sdl & KMOD_ALT)))
        {
        if (mod_sdl&KMOD_SHIFT)
            {
            modifiers |= MOD_MASK_SHIFT;
            }
        /*
         * Don't use caps-lock as shift, because these are special keys
         * being considered here, and we only want letters to get
         * shifted -- webb
         */
        /*
           if (vimGetModKeyState(VK_CAPITAL) & 0x0001)
           modifiers ^= MOD_MASK_SHIFT;
           */
        if (mod_sdl & KMOD_CTRL)
            {
            modifiers |= MOD_MASK_CTRL;
            }
        if (mod_sdl & KMOD_ALT)
            {
            modifiers |= MOD_MASK_ALT;
            }

        if (special_keys_sdl[i].vim_code1 == NUL)
            key = special_keys_sdl[i].vim_code0;
        else
            key = TO_SPECIAL(special_keys_sdl[i].vim_code0,
                    special_keys_sdl[i].vim_code1);
        /*
        fnWarnf("c0=%c c1=%c key=%d", special_keys_sdl[i].vim_code0,
                special_keys_sdl[i].vim_code1,
                key);
                */
        key = simplify_key(key, &modifiers);
        /*
        fnWarnf("c0=%c c1=%c simplifykey=%d", special_keys_sdl[i].vim_code0,
                special_keys_sdl[i].vim_code1,
                key);
                */
        if (key == CSI)
            key = K_CSI;

        if (modifiers)
            {
            string[0] = CSI;
            string[1] = KS_MODIFIER;
            string[2] = modifiers;
            add_to_input_buf(string, 3);
            }

        if (IS_SPECIAL(key))
            {
            string[0] = CSI;
            string[1] = K_SECOND(key);
            string[2] = K_THIRD(key);
            add_to_input_buf(string, 3);
            }
        else
            {
            int	len;

            /* Handle "key" as a Unicode character. */
            len = char_to_string(key, string, 40, FALSE);
            add_to_input_buf(string, len);
            }
        break;
        }
    }
if (special_keys_sdl[i].key_sym == 0)
    {
    //info_push_messagef("process_sdl_key scode=%d", scode);
    /*
    if (vk==VK_SHIFT || vk==VK_CONTROL || vk==VK_MENU ||
            vk==VK_LWIN || vk==VK_RWIN || vk==VK_APPS)
        // i am doing my own 'translatemessage', and that
        // those can not be translated into chars
        return; 
        */
#define MM(k) SDL_SCANCODE_##k
    if (vk==MM(LSHIFT) || vk==MM(RSHIFT)
            || vk==MM(LCTRL) || vk==MM(RCTRL)
            || vk==MM(LALT) || vk==MM(RALT)
            || vk==MM(APPLICATION)
            || vk==MM(LGUI) || vk==MM(RGUI)
            || vk==MM(CLEAR)
            || vk==MM(NUMLOCKCLEAR)
            || vk==MM(CAPSLOCK)
       )
        return;
#undef MM
    char_u string[40];
    int len=0;
    // ch is normal char if 'control' key is not down.
    // otherwise the most significant bit is set to 1
    int ch=mysdl_KeydownToChar(vk, mod_sdl);
    len = char_to_string(ch, string, 40, FALSE);
    if (len == 1 && string[0] == Ctrl_C && ctrl_c_interrupts)
        {
        trash_input_buf();
        got_int = TRUE;
        }

    add_to_input_buf(string, len);
    }
}

/*
 * Process a single Windows message.
 * If one is not available we hang until one is.
 */
    static void
process_message(void)
{
    // adapter events.
    adapter_event_t evt;
    adapter_wait_for_event();
    while(adapter_poll_event(&evt))
        {
        void (*fn)(int, void*, void*) = evt.func;
        int tt=evt.type;
        void *ud0=evt.user_data0;
        void *ud1=evt.user_data1;
        if (fn!=NULL)
            (*fn)(tt, ud0, ud1);
        }
}

/*
 * Catch up with any queued events.  This may put keyboard input into the
 * input buffer, call resize call-backs, trigger timers etc.  If there is
 * nothing in the event queue (& no timers pending), then we return
 * immediately.
 */
    void
gui_mch_update(void)
{
    if (!s_busy_processing)
        {
        while (adapter_has_event() && !vim_is_input_buf_full)
            process_message();
        }
}

/*
 * GUI input routine called by gui_wait_for_chars().  Waits for a character
 * from the keyboard.
 *  wtime == -1	    Wait forever.
 *  wtime == 0	    This should never happen.
 *  wtime > 0	    Wait wtime milliseconds for a character.
 * Returns OK if a character was found to be available within the given time,
 * or FAIL otherwise.
 */
// FUNC_WAIT FIXME
    int
gui_mch_wait_for_chars(int wtime)
{
    MSG		msg;
    int		focus;

    s_timed_out = FALSE;

    if (wtime > 0)
    {
	/* Don't do anything while processing a (scroll) message. */
	if (s_busy_processing)
	    return FAIL;
	s_wait_timer = SDL_AddTimer(wtime, _OnTimer_sdl, 0);
    }

    allow_scrollbar = TRUE;

    focus = gui.in_focus;
    // busy wait
    while (!s_timed_out)
    {
	    /* Stop or start blinking when focus changes */
	    if (gui.in_focus != focus)
	    {
	        if (gui.in_focus)
	    	gui_mch_start_blink();
	        else
	    	gui_mch_stop_blink();
	        focus = gui.in_focus;
	    }

	    if (s_need_activate)
	    {
        // do nothing and clear that flag
	        s_need_activate = FALSE;
	    }

	    /*
	     * Don't use gui_mch_update() because then we will spin-lock until a
	     * char arrives, instead we use GetMessage() to hang until an
	     * event arrives.  No need to check for input_buf_full because we are
	     * returning as soon as it contains a single char -- webb
	     */
	    process_message();

	    if (input_available())
            {
            if (s_wait_timer != 0 && !s_timed_out)
                {
                SDL_RemoveTimer(s_wait_timer);
                s_wait_timer = 0;
                }
            allow_scrollbar = FALSE;

            /* Clear pending mouse button, the release event may have been
             * taken by the dialog window.  But don't do this when getting
             * focus, we need the mouse-up event then. */
            if (!s_getting_focus)
                s_button_pending = -1;

            return OK;
            }
    }
    allow_scrollbar = FALSE;
    return FAIL;
}

/*
 * Clear a rectangular region of the screen from text pos (row1, col1) to
 * (row2, col2) inclusive.
 */
    void
gui_mch_clear_block(
    int		row1,
    int		col1,
    int		row2,
    int		col2)
{
    // inform sdl to change color.
    disp_task_setcolor_t scolor;
    scolor.type=DISP_TASK_SETCOLOR;
    scolor.which=DISP_TASK_SETCOLORBG;
    scolor.color=(SDL_Color)
        {
        myGetRValue(gui.back_pixel),
            myGetGValue(gui.back_pixel),
            myGetBValue(gui.back_pixel),
            0xff //GetAValue(gui.back_pixel)
        };
    display_push_task(&scolor);
    // inform sdl to do clearing.
    disp_task_clearrect_t clrect;
    clrect.type=DISP_TASK_CLEARRECT;
    clrect.rect=(Rect_f)
        {
        (double)col1/Columns, (double)row1/Rows, 
            (double)(abs(col2-col1)+1)/Columns, 
            (double)(abs(row2-row1)+1)/Rows
        };
    display_push_task(&clrect);
}

/*
 * Clear the whole text window.
 */
    void
gui_mch_clear_all(void)
{
    // inform sdl to change color.
    disp_task_setcolor_t scolor;
    scolor.type=DISP_TASK_SETCOLOR;
    scolor.which=DISP_TASK_SETCOLORBG;
    scolor.color=(SDL_Color)
        {
        myGetRValue(gui.back_pixel),
            myGetGValue(gui.back_pixel),
            myGetBValue(gui.back_pixel),
            0xff //GetAValue(gui.back_pixel)
        };
    display_push_task(&scolor);
    // inform sdl to do clearing.
    disp_task_clearrect_t clrect;
    clrect.type=DISP_TASK_CLEARRECT;
    clrect.rect=(Rect_f){ 0.0 , 0.0, 1.0, 1.0};
    display_push_task(&clrect);
}

    void
gui_mch_enable_menu(int flag)
{
// do nothing
}

/*ARGSUSED*/
    void
gui_mch_set_menu_pos(
    int	    x,
    int	    y,
    int	    w,
    int	    h)
{
    /* It will be in the right place anyway */
}



/*
 * Return the myRGB value of a pixel as a long.
 */
    long_u
gui_mch_get_rgb(guicolor_T pixel)
{
    return (myGetRValue(pixel) << 16) + (myGetGValue(pixel) << 8)
							   + myGetBValue(pixel);
}


void
gui_mch_activate_window(void)
{
// do nothing
}


/*
 * ":simalt" command.
 */
    void
ex_simalt(exarg_T *eap)
{
// not using menu, so leave it out.
#if 0
    char_u *keys = eap->arg;

    PostMessage(s_hwnd, WM_SYSCOMMAND, (WPARAM)SC_KEYMENU, (LPARAM)0);
    while (*keys)
    {
	if (*keys == '~')
	    *keys = ' ';	    /* for showing system menu */
	PostMessage(s_hwnd, WM_CHAR, (WPARAM)*keys, (LPARAM)0);
	keys++;
    }
#endif // 0
}

    static void
set_window_title(HWND hwnd, char *title)
{
// do nothing
}

    void
gui_mch_find_dialog(exarg_T *eap)
{
// do nothing
}


    void
gui_mch_replace_dialog(exarg_T *eap)
{
// do nothing
}


/*
 * Set visibility of the pointer.
 */
    void
gui_mch_mousehide(int hide)
{
// Although i can simulate this by SDL_ShowCursor,
// it interferes too much with outside world.
// so don't do it.
}

void force_redraw()
{
out_flush();

// copied from gui_redraw @ gui.c with modifications.
(void)gui_redraw_block(0,0,Rows,Columns, GUI_MON_NOCLEAR);

if (gui.row == gui.cursor_row)
    gui_update_cursor(TRUE, TRUE);

}



// used with sdl resize event.
int screen_w_pending =0,
    screen_h_pending =0;
static void
_on_textarea_resize_from_sdl(int w, int h)
{
screen_w_pending =w+gui_get_base_width();
screen_h_pending = h + gui_get_base_height();

	gui_resize_shell(w+gui_get_base_width(),
            h+ gui_get_base_height());
}

void
_on_textarea_resize_callback(int tt, void* ud0, void* ud1)
{
int w=(int)ud0, h=(int)ud1;
_on_textarea_resize_from_sdl(w,h);
}


void 
_on_set_focus_callback(int tt, void* ud0, void* ud1)
{
    gui_focus_change(TRUE);
    s_getting_focus = TRUE;
}


void
_on_kill_focus_callback(int tt, void* ud0, void* ud1)
{
gui_focus_change(FALSE);
s_getting_focus=FALSE;
}


#if defined(FEAT_WINDOWS) || defined(PROTO)
    void
gui_mch_destroy_scrollbar(scrollbar_T *sb)
{
// do nothing
}
#endif

/*
 * Get current mouse coordinates in text window.
 */
    void
gui_mch_getmouse(int *x, int *y)
{
Warn("gui_mch_getmouse is called");
int xx, yy;
SDL_GetMouseState(&xx, &yy);
*x=xx;
*y=yy;
}

/*
 * Move mouse pointer to character at (x, y).
 */
    void
gui_mch_setmouse(int x, int y)
{
// do nothing
}


    void
gui_mch_flash(int msec)
{
disp_task_invertrect_t irect;
irect.type=DISP_TASK_INVERTRECT;
irect.rect=(Rect_f) {0.0, 0.0, 1.0, 1.0};
display_push_task(&irect);
ui_delay((long)msec, TRUE);	/* wait for a few msec */
display_push_task(&irect);
}

/*
 * Return flags used for scrolling.
 * The SW_INVALIDATE is required when part of the window is covered or
 * off-screen. Refer to MS KB Q75236.
 */
// called by insert_lines and delete_lines
    static int
get_scroll_flags(void)
{
// origin function returns only 0 or SW_INVALIDATE
// SW_INVALIDATE for partly draw, 0 for a whole draw
// i am not using the invalidate rects so i just return 0.
return 0;
}

/*
 * Delete the given number of lines from the given row, scrolling up any
 * text further down within the scroll region.
 */
    void
gui_mch_delete_lines(
    int	    row,
    int	    num_lines)
{
    // inform sdl to scroll the display window.
    disp_task_scroll_t scro;
    scro.type=DISP_TASK_SCROLL;
    scro.distance = -(double)num_lines/Rows;
    scro.scroll_rect= (Rect_f) 
        {
        (double)gui.scroll_region_left/Columns,
        (double)row/Rows,
        (double)(gui.scroll_region_right+1-gui.scroll_region_left)/Columns,
        (double)(gui.scroll_region_bot+1-row)/Rows
        };
    scro.clip_rect=scro.scroll_rect;
    scro.flags=get_scroll_flags();
    display_push_task(&scro);

    gui_clear_block(gui.scroll_region_bot - num_lines + 1,
            gui.scroll_region_left,
            gui.scroll_region_bot, gui.scroll_region_right);

}

/*
 * Insert the given number of lines before the given row, scrolling down any
 * following text within the scroll region.
 */
    void
gui_mch_insert_lines(
    int		row,
    int		num_lines)
{
    disp_task_scroll_t scro;
    scro.type=DISP_TASK_SCROLL;
    scro.distance=(double)num_lines/Rows;
    scro.scroll_rect= (Rect_f) 
        {
        (double)gui.scroll_region_left/Columns,
        (double)row/Rows,
        (double)(gui.scroll_region_right+1-gui.scroll_region_left)/Columns,
        (double)(gui.scroll_region_bot+1-row)/Rows
        };
    scro.clip_rect=scro.scroll_rect;
    scro.flags=get_scroll_flags();
    display_push_task(&scro);
    gui_clear_block(row, gui.scroll_region_left,
				row + num_lines - 1, gui.scroll_region_right);

}


/*ARGSUSED*/
// the actuall exit(..) is in os_mswin.c mch_exit
// here is just some cleanning-up.
    void
gui_mch_exit(int rc)
{
// do nothing
}


#ifdef FEAT_MBYTE
/*
 * Handler of gui.wide_font (p_guifontwide) changed notification.
 */
    void
gui_mch_wide_font_changed()
{
gui.wide_font = (GuiFont)11;
gui.wide_ital_font = (GuiFont)12;
gui.wide_bold_font = (GuiFont)13;
gui.wide_boldital_font = (GuiFont)14;
}
#endif

/*
 * Initialise vim to use the font with the given name.
 * Return FAIL if the font could not be loaded, OK otherwise.
 */
/*ARGSUSED*/
    int
gui_mch_init_font(char_u *font_name, int fontset)
{
gui.norm_font = (GuiFont)1;
gui.ital_font =(GuiFont)2;
gui.bold_font =(GuiFont)3;
gui.boldital_font =(GuiFont)4;

GetFontSize(0);
return OK;
}

#ifndef WPF_RESTORETOMAXIMIZED
# define WPF_RESTORETOMAXIMIZED 2   /* just in case someone doesn't have it */
#endif

/*
 * Return TRUE if the GUI window is maximized, filling the whole screen.
 */
    int
gui_mch_maximized()
{
// never maximized
return 0;
}

/*
 * Called when the font changed while the window is maximized.  Compute the
 * new Rows and Columns.  This is like resizing the window.
 */
    void
gui_mch_newfont()
{
// no use.
}

/*
 * Set the window title
 */
/*ARGSUSED*/
    void
gui_mch_settitle(
    char_u  *title,
    char_u  *icon)
{
// do nothing
}


// _OnScroll is called for scroll bar dragging and mouse wheel.
// maybe will come to use if need to support mouse wheel.
#if 0
/*ARGSUSED*/
    static int
_OnScroll(
    HWND hwnd,
    HWND hwndCtl,
    UINT code,
    int pos)
{
    static UINT	prev_code = 0;   /* code of previous call */
    scrollbar_T *sb, *sb_info;
    long	val;
    int		dragging = FALSE;
    int		dont_scroll_save = dont_scroll;
#ifndef WIN3264
    int		nPos;
#else
    SCROLLINFO	si;

    si.cbSize = sizeof(si);
    si.fMask = SIF_POS;
#endif

    sb = gui_mswin_find_scrollbar(hwndCtl);
    if (sb == NULL)
	return 0;

    if (sb->wp != NULL)		/* Left or right scrollbar */
    {
	/*
	 * Careful: need to get scrollbar info out of first (left) scrollbar
	 * for window, but keep real scrollbar too because we must pass it to
	 * gui_drag_scrollbar().
	 */
	sb_info = &sb->wp->w_scrollbars[0];
    }
    else	    /* Bottom scrollbar */
	sb_info = sb;
    val = sb_info->value;

    switch (code)
    {
	case SB_THUMBTRACK:
	    val = pos;
	    dragging = TRUE;
	    if (sb->scroll_shift > 0)
		val <<= sb->scroll_shift;
	    break;
	case SB_LINEDOWN:
	    val++;
	    break;
	case SB_LINEUP:
	    val--;
	    break;
	case SB_PAGEDOWN:
	    val += (sb_info->size > 2 ? sb_info->size - 2 : 1);
	    break;
	case SB_PAGEUP:
	    val -= (sb_info->size > 2 ? sb_info->size - 2 : 1);
	    break;
	case SB_TOP:
	    val = 0;
	    break;
	case SB_BOTTOM:
	    val = sb_info->max;
	    break;
	case SB_ENDSCROLL:
	    if (prev_code == SB_THUMBTRACK)
	    {
		/*
		 * "pos" only gives us 16-bit data.  In case of large file,
		 * use GetScrollPos() which returns 32-bit.  Unfortunately it
		 * is not valid while the scrollbar is being dragged.
		 */
		val = GetScrollPos(hwndCtl, SB_CTL);
		if (sb->scroll_shift > 0)
		    val <<= sb->scroll_shift;
	    }
	    break;

	default:
	    /* TRACE("Unknown scrollbar event %d\n", code); */
	    return 0;
    }
    prev_code = code;

#ifdef WIN3264
    si.nPos = (sb->scroll_shift > 0) ? val >> sb->scroll_shift : val;
    SetScrollInfo(hwndCtl, SB_CTL, &si, TRUE);
#else
    nPos = (sb->scroll_shift > 0) ? val >> sb->scroll_shift : val;
    SetScrollPos(hwndCtl, SB_CTL, nPos, TRUE);
#endif

    /*
     * When moving a vertical scrollbar, move the other vertical scrollbar too.
     */
    if (sb->wp != NULL)
    {
	scrollbar_T *sba = sb->wp->w_scrollbars;
	HWND    id = sba[ (sb == sba + SBAR_LEFT) ? SBAR_RIGHT : SBAR_LEFT].id;

#ifdef WIN3264
	SetScrollInfo(id, SB_CTL, &si, TRUE);
#else
	SetScrollPos(id, SB_CTL, nPos, TRUE);
#endif
    }

    /* Don't let us be interrupted here by another message. */
    s_busy_processing = TRUE;

    /* When "allow_scrollbar" is FALSE still need to remember the new
     * position, but don't actually scroll by setting "dont_scroll". */
    dont_scroll = !allow_scrollbar;

    gui_drag_scrollbar(sb, val, dragging);

    s_busy_processing = FALSE;
    dont_scroll = dont_scroll_save;

    return 0;
}

#endif // 0

