/* vi:set ts=8 sts=4 sw=4:
 *
 * VIM - Vi IMproved    by Bram Moolenaar
 *
 * Do ":help uganda"  in Vim to read copying and usage conditions.
 * Do ":help credits" in Vim to see a list of people who contributed.
 * See README.txt for an overview of the Vim source code.
 */
/*
 * gui_sdl2.c
 * sdl2 helper routines
 * not used now.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <SDL2/sdl.h>




#include "assert_out_ns_vim.h"
#include "begin_ns_vim.h"



#define _DisplayWarningMessage(heading, msg) \
    do { \
        char __buf_Error[ERROR_LENGTH]; \
        snprintf(__buf_Error, ERROR_LENGTH, heading ":\n%s\nFile: %s\nLine: %d\nFunc:%s\n",  \
                msg, __FILE__, __LINE__, __FUNCTION__); \
        fprintf(stderr, "%s", __buf_Error); \
    } while (0)
#define Warn(msg) \
    _DisplayWarningMessage("Warn", msg)
#define Error(msg) \
    do { \
        _DisplayWarningMessage("Error", msg); \
        exit(2); \
    } while (0)

#define NotFinished() Error("Not Finished.")

static int sAlreadyInited=0;
static const unsigned char *sKeyboardState=0;
SDL_Rect sTextAreaRect={0,0,0,0};

static struct
{
    UINT    key_sym;
    char_u  vim_code0;
    char_u  vim_code1;
} special_keys[] =
{
    {VK_UP,		'k', 'u'},
    {VK_DOWN,		'k', 'd'},
    {VK_LEFT,		'k', 'l'},
    {VK_RIGHT,		'k', 'r'},

    {VK_F1,		'k', '1'},
    {VK_F2,		'k', '2'},
    {VK_F3,		'k', '3'},
    {VK_F4,		'k', '4'},
    {VK_F5,		'k', '5'},
    {VK_F6,		'k', '6'},
    {VK_F7,		'k', '7'},
    {VK_F8,		'k', '8'},
    {VK_F9,		'k', '9'},
    {VK_F10,		'k', ';'},

    {VK_F11,		'F', '1'},
    {VK_F12,		'F', '2'},
    {VK_F13,		'F', '3'},
    {VK_F14,		'F', '4'},
    {VK_F15,		'F', '5'},
    {VK_F16,		'F', '6'},
    {VK_F17,		'F', '7'},
    {VK_F18,		'F', '8'},
    {VK_F19,		'F', '9'},
    {VK_F20,		'F', 'A'},

    {VK_F21,		'F', 'B'},
#ifdef FEAT_NETBEANS_INTG
    {VK_PAUSE,		'F', 'B'},	/* Pause == F21 (see gui_gtk_x11.c) */
#endif
    {VK_F22,		'F', 'C'},
    {VK_F23,		'F', 'D'},
    {VK_F24,		'F', 'E'},	/* winuser.h defines up to F24 */

    {VK_HELP,		'%', '1'},
    {VK_BACK,		'k', 'b'},
    {VK_INSERT,		'k', 'I'},
    {VK_DELETE,		'k', 'D'},
    {VK_HOME,		'k', 'h'},
    {VK_END,		'@', '7'},
    {VK_PRIOR,		'k', 'P'},
    {VK_NEXT,		'k', 'N'},
    {VK_PRINT,		'%', '9'},
    {VK_ADD,		'K', '6'},
    {VK_SUBTRACT,	'K', '7'},
    {VK_DIVIDE,		'K', '8'},
    {VK_MULTIPLY,	'K', '9'},
    // separator is not keypad enter. keypad enter is VK_RETURN.
    {VK_SEPARATOR,	'K', 'A'},	/* Keypad Enter */
    {VK_DECIMAL,	'K', 'B'},

    {VK_NUMPAD0,	'K', 'C'},
    {VK_NUMPAD1,	'K', 'D'},
    {VK_NUMPAD2,	'K', 'E'},
    {VK_NUMPAD3,	'K', 'F'},
    {VK_NUMPAD4,	'K', 'G'},
    {VK_NUMPAD5,	'K', 'H'},
    {VK_NUMPAD6,	'K', 'I'},
    {VK_NUMPAD7,	'K', 'J'},
    {VK_NUMPAD8,	'K', 'K'},
    {VK_NUMPAD9,	'K', 'L'},

    /* Keys that we want to be able to use any modifier with: */
    {VK_SPACE,		' ', NUL},
    {VK_TAB,		TAB, NUL},
    {VK_ESCAPE,		ESC, NUL},
    {NL,		NL, NUL},
    {CAR,		CAR, NUL},

    /* End of list marker: */
    {0,			0, 0}
};


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
static UINT		blink_last_check_time = 0;
static UINT     blink_state_time_left=0;
    void
gui_mch_set_blinking(long wait, long on, long off)
{
    blink_waittime = wait;
    blink_ontime = on;
    blink_offtime = off;
}

void updataCursorBlink(int delta_msec)
{
if (blink_state==BLINK_NONE) return;
blink_state_time_left-=delta_msec;
if (blink_state_time_left>0) return;
blink_state_time_left=0; // in case it becomes negative
if (blink_state==BLINK_ON)
    {
    gui_undraw_cursor();
    blink_state=BLINK_OFF;
    blink_state_time_left=blink_off_time;
    }
else
    {
    gui_update_cursor(TRUE, FALSE);
    blink_state = BLINK_ON;
    blink_state_time_left=blink_on_time;
    }
}

void gui_mch_stop_blink()
{
if (blink_state == BLINK_OFF)
    gui_update_cursor(TRUE, FALSE);
blink_state=BLINK_NONE;
}

void gui_mch_start_blink()
{
if (blink_waittime && blink_ontime && blink_offtime && gui.in_focus)
    {
    blink_state=BLINK_ON;
    blink_state_time_left=blink_on_time;
    gui_update_cursor(TRUE, FALSE);
    }
}

static int char_to_string(int ch, char_u *string, int slen, int had_alt)
{
Assert(0<=ch && ch<=0xff);
string[0]=(ch_u)ch;
string[1]=0;
return 1;
}

void mySleep(int ms)
{
// TODO this has strong dependency on platform.
Sleep(ms);
}

static void
_OnMouseEvent(
    int button,
    int x,
    int y,
    int repeated_click,
    UINT keyFlags)
{
    int vim_modifiers = 0x0;

    s_getting_focus = FALSE;

    if (keyFlags & MK_SHIFT)
	vim_modifiers |= MOUSE_SHIFT;
    if (keyFlags & MK_CONTROL)
	vim_modifiers |= MOUSE_CTRL;
    if (GetKeyState(VK_MENU) & 0x8000)
	vim_modifiers |= MOUSE_ALT;

    gui_send_mouse_event(button, x, y, repeated_click, vim_modifiers);
}


static int sdl2_keydown_event_to_vim_vk(SDL_Event kevnt)
{
}


static int myMessageBox(const char *msg, const char *title, int type)
{
if (type!=MB_OK)
    Error("MessageBox types other than MB_OK is not supported yet.");
NotFinished();
}

static void myError(char *msg)
{
fprintf(stderr, msg);
exit(3);
}

int gui_mch_init()
{
if (sAlreadyInited) return 0;
sKeyboardState=SDL_GetKeyboardState(NULL);
notFinished();
}

// gui_w48.c 
// HandleMouseHide(uMsg, lParam)
void handleMouseHide(SDL_Event evnt)
{
if (evnt.type==SDL_MOUSEMOTION) return;
switch (evnt.type)
    {
    case SDL_KEYDOWN:
    case SDL_KEYUP:
        gui_mch_mousehide(FALSE);
        break;
    }
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
    // RGB is a macro(windef.h) to map rgb to a COLORREF.
    // COLORREF is typedef'ed as DWORD.
    gui.norm_pixel = RGB(0,0,0); //GetSysColor(COLOR_WINDOWTEXT);
    gui.back_pixel = RGB(0xff, 0xff, 0xff); //GetSysColor(COLOR_WINDOW);
    gui.def_norm_pixel = gui.norm_pixel;
    gui.def_back_pixel = gui.back_pixel;
}

int gui_mch_open()
{
//nothing to do.
return OK;
}

//ignore window position.
int gui_mch_get_winpos(int *x, int *y)
{
*x=0;
*y=0;
return OK;
}

void gui_mch_set_winpos(int x, int y)
{
}

// original one is only referenced by gui.c gui_position_components,
// which i intend to ignore.
void gui_mch_set_text_area_pos(int x, int y, int w, int h)
{
// need to deal with tabline, scroll bar, .etc
// ignore them for now.
}

    void
gui_mch_enable_scrollbar(
    scrollbar_T     *sb,
    int		    flag)
{
    //ShowScrollBar(sb->id, SB_CTL, flag);

    /* TODO: When the window is maximized, the size of the window stays the
     * same, thus the size of the text area changes.  On Win98 it's OK, on Win
     * NT 4.0 it's not... */
}

    void
gui_mch_set_scrollbar_pos(
    scrollbar_T *sb,
    int		x,
    int		y,
    int		w,
    int		h)
{
    //SetWindowPos(sb->id, NULL, x, y, w, h, SWP_NOZORDER | SWP_NOACTIVATE | SWP_SHOWWINDOW);
}

    void
gui_mch_create_scrollbar(
    scrollbar_T *sb,
    int		orient)	/* SBAR_VERT or SBAR_HORIZ */
{
/*
    sb->id = CreateWindow(
	"SCROLLBAR", "Scrollbar",
	WS_CHILD | ((orient == SBAR_VERT) ? SBS_VERT : SBS_HORZ), 0, 0,
	10,				// Any value will do for now 
	10,				// Any value will do for now 
	s_hwnd, NULL,
	s_hinst, NULL);
    */
}

/*
 * Find the scrollbar with the given hwnd.
 */
	 static scrollbar_T *
gui_mswin_find_scrollbar(HWND hwnd)
{
/*
    win_T	*wp;

    if (gui.bottom_sbar.id == hwnd)
	return &gui.bottom_sbar;
    FOR_ALL_WINDOWS(wp)
    {
	if (wp->w_scrollbars[SBAR_LEFT].id == hwnd)
	    return &wp->w_scrollbars[SBAR_LEFT];
	if (wp->w_scrollbars[SBAR_RIGHT].id == hwnd)
	    return &wp->w_scrollbars[SBAR_RIGHT];
    }
    */
    return NULL;
}

static void GetFontSize(GuiFont font)
{
int w, h;
TTF_Font *ff=(TTF_Font*)font;
gui.char_height=TTF_FontHeight(ff);
int r=TTF_SizeText(ff, "x", &w, &h);
gui.char_width=w;
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

GuiFont gui_mch_get_font(char_u *name, int giveErrorIfMissing)
{
TTF_Font* font=TTF_Open(name, 12);
if (font==NULL) 
    {
    Warn("font not loaded");
	EMSG2(_(e_font), name);
    }
return (GuiFont)font;
}

    char_u *
gui_mch_get_fontname(font, name)
    GuiFont font;
    char_u  *name;
{
    if (name == NULL)
	return NULL;
    return vim_strsave(name);
}

    void
gui_mch_free_font(GuiFont font)
{
if (font)
    TTF_CloseFont((TTF_Font*)font);
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
	{"Black",		RGB(0x00, 0x00, 0x00)},
	{"DarkGray",		RGB(0xA9, 0xA9, 0xA9)},
	{"DarkGrey",		RGB(0xA9, 0xA9, 0xA9)},
	{"Gray",		RGB(0xC0, 0xC0, 0xC0)},
	{"Grey",		RGB(0xC0, 0xC0, 0xC0)},
	{"LightGray",		RGB(0xD3, 0xD3, 0xD3)},
	{"LightGrey",		RGB(0xD3, 0xD3, 0xD3)},
	{"Gray10",		RGB(0x1A, 0x1A, 0x1A)},
	{"Grey10",		RGB(0x1A, 0x1A, 0x1A)},
	{"Gray20",		RGB(0x33, 0x33, 0x33)},
	{"Grey20",		RGB(0x33, 0x33, 0x33)},
	{"Gray30",		RGB(0x4D, 0x4D, 0x4D)},
	{"Grey30",		RGB(0x4D, 0x4D, 0x4D)},
	{"Gray40",		RGB(0x66, 0x66, 0x66)},
	{"Grey40",		RGB(0x66, 0x66, 0x66)},
	{"Gray50",		RGB(0x7F, 0x7F, 0x7F)},
	{"Grey50",		RGB(0x7F, 0x7F, 0x7F)},
	{"Gray60",		RGB(0x99, 0x99, 0x99)},
	{"Grey60",		RGB(0x99, 0x99, 0x99)},
	{"Gray70",		RGB(0xB3, 0xB3, 0xB3)},
	{"Grey70",		RGB(0xB3, 0xB3, 0xB3)},
	{"Gray80",		RGB(0xCC, 0xCC, 0xCC)},
	{"Grey80",		RGB(0xCC, 0xCC, 0xCC)},
	{"Gray90",		RGB(0xE5, 0xE5, 0xE5)},
	{"Grey90",		RGB(0xE5, 0xE5, 0xE5)},
	{"White",		RGB(0xFF, 0xFF, 0xFF)},
	{"DarkRed",		RGB(0x80, 0x00, 0x00)},
	{"Red",			RGB(0xFF, 0x00, 0x00)},
	{"LightRed",		RGB(0xFF, 0xA0, 0xA0)},
	{"DarkBlue",		RGB(0x00, 0x00, 0x80)},
	{"Blue",		RGB(0x00, 0x00, 0xFF)},
	{"LightBlue",		RGB(0xAD, 0xD8, 0xE6)},
	{"DarkGreen",		RGB(0x00, 0x80, 0x00)},
	{"Green",		RGB(0x00, 0xFF, 0x00)},
	{"LightGreen",		RGB(0x90, 0xEE, 0x90)},
	{"DarkCyan",		RGB(0x00, 0x80, 0x80)},
	{"Cyan",		RGB(0x00, 0xFF, 0xFF)},
	{"LightCyan",		RGB(0xE0, 0xFF, 0xFF)},
	{"DarkMagenta",		RGB(0x80, 0x00, 0x80)},
	{"Magenta",		RGB(0xFF, 0x00, 0xFF)},
	{"LightMagenta",	RGB(0xFF, 0xA0, 0xFF)},
	{"Brown",		RGB(0x80, 0x40, 0x40)},
	{"Yellow",		RGB(0xFF, 0xFF, 0x00)},
	{"LightYellow",		RGB(0xFF, 0xFF, 0xE0)},
	{"DarkYellow",		RGB(0xBB, 0xBB, 0x00)},
	{"SeaGreen",		RGB(0x2E, 0x8B, 0x57)},
	{"Orange",		RGB(0xFF, 0xA5, 0x00)},
	{"Purple",		RGB(0xA0, 0x20, 0xF0)},
	{"SlateBlue",		RGB(0x6A, 0x5A, 0xCD)},
	{"Violet",		RGB(0xEE, 0x82, 0xEE)},
    };

    typedef struct SysColorTable
    {
	char	    *name;
	int	    color;
    } SysColorTable;

    static SysColorTable sys_table[] =
    {
#ifdef WIN3264
	{"SYS_3DDKSHADOW", COLOR_3DDKSHADOW},
	{"SYS_3DHILIGHT", COLOR_3DHILIGHT},
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
	return RGB(r, g, b);
    }
    else
    {
	/* Check if the name is one of the colors we know */
	for (i = 0; i < sizeof(table) / sizeof(table[0]); i++)
	    if (STRICMP(name, table[i].name) == 0)
		return table[i].color;
    }

    /*
     * Try to look up a system colour.
     */
    for (i = 0; i < sizeof(sys_table) / sizeof(sys_table[0]); i++)
	if (STRICMP(name, sys_table[i].name) == 0)
	    return GetSysColor(sys_table[i].color);

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
#undef LINE_LEN
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
		return (guicolor_T) RGB(r, g, b);
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

    for (i = 0; special_keys[i].vim_code1 != NUL; i++)
	if (name[0] == special_keys[i].vim_code0 &&
					 name[1] == special_keys[i].vim_code1)
	    return OK;
    return FAIL;
}

    void
gui_mch_beep(void)
{
}


/*
 * Iconify the GUI window.
 */
    void
gui_mch_iconify(void)
{
}

static inline void PutPixel32_nolock(SDL_Surface * surface, 
        int x, int y, Uint32 color)
{
    Uint8 * pixel = (Uint8*)surface->pixels;
    pixel += (y * surface->pitch) + (x * sizeof(Uint32));
    *((Uint32*)pixel) = color;
}
/*
 * Draw a cursor without focus.
 */
    void
gui_mch_draw_hollow_cursor(guicolor_T color)
{
int x=FILL_X(gui.col),
    y=FILL_Y(gui.row),
    w=gui.char_width,
    h=gui.char_height;
int r=GetRValue(color),
    g=GetGValue(color),
    b=GetBvalue(color);
SDL_Surface *surface=lockTextArea();
Assert(surface->format->BitsPerPixel == 32);
Uint32 new_color=SDL_MapRGBA(surface->format, r,g,b,0xff);
for (int i=0; i<w; i++)
    {
    PutPixel32_nolock(surface, x+i, y, new_color);
    PutPixel32_nolock(surface, x+i, y+h, new_color);
    }
for (int j=0; j<h; j++)
    {
    PutPixel32_nolock(surface, x, y+j, new_color);
    PutPixel32_nolock(surface, x+w, y+j, new_color);
    }
unlockTextArea();
}

    void
gui_mch_draw_part_cursor(
    int		w,
    int		h,
    guicolor_T	color)
{
int x=FILL_X(gui.col),
    y=FILL_Y(gui.row),
int r=GetRValue(color),
    g=GetGValue(color),
    b=GetBvalue(color);
SDL_Rect rect={x,y,w,h};
SDL_Surface *surface=getTextArea();
Assert(surface->format->BitsPerPixel == 32);
Uint32 new_color=SDL_MapRGBA(surface->format, r,g,b,0xff);
SDL_FillRect(surface, &rect, new_color);
}

void gui_mch_update()
{
}

int 
gui_mch_wait_for_chars(int wtime)
{
NotFinished();
}

static void clear_rect(SDL_Rect *prect)
{
Uint32 color=gui.back_pixel;
int r=GetRValue(color),
    g=GetGValue(color),
    b=GetBvalue(color);

SDL_Surface *surface=getTextArea();
Uint32 new_color=SDL_MapRGBA(surface->format, r,g,b,0xff);
SDL_FillRect(surface, prect, new_color);
}

/*
 * Clear a rectangular region of the screen from text pos (row1, col1) to
 * (row2, col2) inclusive.
 */
    void
gui_mch_clear_block(int	row1, int col1, int row2, int col2)
{
int x=FILL_X(col1),
    y=FILL_Y(row1),
    w=FILL_X(col2)-x,
    h=FILL_Y(row2)-y;
SDL_Rect rect={x,y,w,h};
clear_rect(&rect);
}

/*
 * Clear the whole text window.
 */
    void
gui_mch_clear_all(void)
{
int x=0,
    y=0,
    w=Columns*gui.char_width,
    h=Rows*gui.char_height;
SDL_Rect rect={x,y,w,h};
clear_rect(&rect);
}

    void
gui_mch_enable_menu(int flag)
{
NotFinished();
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
 * Return the RGB value of a pixel as a long.
 */
    long_u
gui_mch_get_rgb(guicolor_T pixel)
{
    return (GetRValue(pixel) << 16) + (GetGValue(pixel) << 8)
							   + GetBValue(pixel);
}

void
gui_mch_activate_window(void)
{
    (void)SetActiveWindow(s_hwnd);
}

/*
 * ":simalt" command.
 */
    void
ex_simalt(exarg_T *eap)
{
}

int sdl_key_scancode_to_vim_vk(int scode)
{
#define MT(vvk, sc) case SDL_SCANCODE_##sc: return VK_##vvk;
switch (scode)
    {
    MT(BACK, BACKSPACE);
    MT(TAB, TAB);
    MT(CLEAR, CLEAR);
    MT(RETURN, RETURN);
    MT(PAUSE, PAUSE);
    MT(CAPITAL, CAPSLOCK);
    MT(SCROLL, SCROLLLOCK);
    MT(ESCAPE, ESCAPE);
    MT(SPACE, SPACE);
    MT(PRIOR, PAGEUP);
    MT(NEXT, PAGEDOWN);
    MT(END, END);
    MT(HOME, HOME);
    MT(LEFT, LEFT);
    MT(RIGHT, RIGHT);
    MT(UP, UP);
    MT(DOWN, DOWN);
    MT(SELECT, SELECT);
    MT(SNAPSHOT, PRINTSCREEN);
    MT(EXECUTE, EXECUTE);
    MT(INSERT, INSERT);
    MT(DELETE, DELETE);
    MT(HELP, HELP);
    MT(NUMLOCK, NUMLOCKCLEAR);
    MT(LWIN, LGUI);
    MT(RWIN, RGUI);
    // map left and right to the single compound virtual key.
    MT(SHIFT, LSHIFT);
    MT(SHIFT, RSHIFT);
    MT(CONTROL, LCTRL);
    MT(CONTROL, RCTRL);
    MT(MENU, LALT);
    MT(MENU, RALT);
    MT(OEM_PLUS, EQUALS);
    MT(OEM_COMMA, COMMA);
    MT(OEM_MINUS, MINUS);
    MT(OEM_PERIOD, PERIOD);
    MT(OEM_1, SEMICOLON); // ';'
    MT(OEM_2, SLASH); // '/'
    MT(OEM_3, GRAVE); // '`' aka. backquote
    MT(OEM_4, LEFTBRACKET); // '['
    MT(OEM_5, BACKSLASH); // '\'
    MT(OEM_6, RIGHTBRACKET); // ']'
    MT(OEM_7, APOSTROPHE); // ''' the quote mark itself

    // keypad. keys whose behavior do not change by Numlock.
    MT(MULTIPLY, KP_MULTIPLY); // the <shift>-8 is not a direct multiply.
    MT(DIVIDE, KP_DIVIDE);
    MT(ADD, KP_PLUS);
    MT(SUBTRACT, KP_MINUS);
    MT(RETURN, KP_ENTER);
    // seems there is no separator key on my keyboard.
    case SDL_SCANCODE_SEPARATOR:
        NotFinished();
    default:
        (void)0;
    }
if (SDL_SCANCODE_A<=scode && scode<=SDL_SCANCODE_Z)
    return scode-SDL_SCANCODE_A+'A';
if (SDL_SCANCODE_1<=scode && scode<=SDL_SCANCODE_0)
    {
    if (SDL_SCANCODE_0==scode) return '0';
    else return scode-SDL_SCANCODE_1+'1';
    }
if (SDL_SCANCODE_F1<=scode && scode<=SDL_SCANCODE_F12)
    return scode-SDL_SCANCODE_F1+VK_F1;
if (SDL_SCANCODE_F13<=scode && scode<=SDL_SCANCODE_F24)
    return scode-SDL_SCANCODE_F13+VK_F13;
// keypad.
// if numlock is not on, the press of '7' on keypad will generate a keydown of
// 'VK_HOME', if numlock is on, then press of the same key will generate a 
// keydown of 'VK_NUMPAD7'.
// the keypad5 key generates a VK_CLEAR if Numlock is not on.
// Note that 'numlock', 'divide', 'multiply', 'minus', 'plus', and 'enter' 
// are already handled in previous section.
if (!sKeyboardState[SDL_SCANCODE_NUMLOCK]) // numlock is not on.
    case (scode)
        {
#define MKP(vvk, sc) case SDL_SCANCODE_KP_##sc: return VK_##vvk
        MKP(INSERT,0);
        MKP(END,1);
        MKP(DOWN,2);
        MKP(NEXT,3);
        MKP(LEFT,4);
        MKP(CLEAR,5);
        MKP(RIGHT,6);
        MKP(HOME,7);
        MKP(UP,8);
        MKP(PRIOR,9);
        MKP(DELETE,DECIMAL);
#undef MKP
        default: Error("non-captured key");
        }
else // numlock is on
    {
    if (SDL_SCANCODE_KP_1<=scode && scode<=SDL_SCANCODE_KP_9)
        return scode-SDL_SCANCODE_KP_1+VK_NUMPAD1;
    else if (SDL_SCANCODE_KP_0==scode)
        return VK_NUMPAD0;
    else if(SDL_SCANCODE_KP_DECIMAL==scode)
        return VK_DECIMAL;
    else Error("non-captured key");
    }
}





int vim_vk_to_sdl_key_scancode(int vim_vk)
{
#define MT(vk,sc) case (VK_##vk) : return (SDL_SCANCODE_##sc); break
//fprintf(stderr, "converting vim_vk to sdl_key_scancode\n");
//MessageBox(s_hwnd, errMsg, "Error", MB_OK); 
char mms[200];
snprintf(mms, 200, "vim_vk=%d(0x%x)", vim_vk, vim_vk);
//Warn(mms);
switch (vim_vk)
    {
    MT(BACK, BACKSPACE);
    MT(TAB, TAB);
    MT(CLEAR, CLEAR);
    MT(RETURN, RETURN);
    MT(PAUSE, PAUSE);
    MT(CAPITAL, CAPSLOCK);
    MT(SCROLL, SCROLLLOCK);
    MT(ESCAPE, ESCAPE);
    MT(SPACE, SPACE);
    MT(PRIOR, PAGEUP);
    MT(NEXT, PAGEDOWN);
    MT(END, END);
    MT(HOME, HOME);
    MT(LEFT, LEFT);
    MT(RIGHT, RIGHT);
    MT(UP, UP);
    MT(DOWN, DOWN);
    MT(SELECT, SELECT);
    MT(SNAPSHOT, PRINTSCREEN);
    MT(EXECUTE, EXECUTE);
    MT(INSERT, INSERT);
    MT(DELETE, DELETE);
    MT(HELP, HELP);
    MT(LWIN, LGUI);
    MT(RWIN, RGUI);
    MT(NUMLOCK, NUMLOCKCLEAR);
    MT(LSHIFT, LSHIFT);
    MT(RSHIFT, RSHIFT);
    MT(LCONTROL, LCTRL);
    MT(RCONTROL, RCTRL);
    MT(LMENU, LALT);
    MT(RMENU, RALT);
    MT(OEM_PLUS, EQUALS);
    MT(OEM_COMMA, COMMA);
    MT(OEM_MINUS, MINUS);
    MT(OEM_PERIOD, PERIOD);
    MT(OEM_1, SEMICOLON); // ';'
    MT(OEM_2, SLASH); // '/'
    MT(OEM_3, GRAVE); // '`' aka. backquote
    MT(OEM_4, LEFTBRACKET); // '['
    MT(OEM_5, BACKSLASH); // '\'
    MT(OEM_6, RIGHTBRACKET); // ']'
    MT(OEM_7, APOSTROPHE); // ''' the quote mark itself


    // the following 3 items indicates any of the two keys.
    case VK_SHIFT:
    case VK_CONTROL:
    case VK_MENU:
        return -1;
    MT(MULTIPLY, KP_MULTIPLY); // the <shift>-8 is not a direct multiply.
    MT(DIVIDE, KP_DIVIDE);
    MT(ADD, KP_PLUS);
    MT(SUBTRACT, KP_MINUS);
    MT(DECIMAL, KP_PERIOD);
    // seems there is no separator key on my keyboard.
    case VK_SEPARATOR:
        NotFinished();
    default:
        (void)0;
    }
// key 0,1,...,9
if (0x30<=vim_vk && vim_vk<=0x39)
    {
    if (0x31<=vim_vk && vim_vk<=0x39)// key 1..9
        return vim_vk-0x31+SDL_SCANCODE_1;
    else // key 0
        return SDL_SCANCODE_0;
    }
// key A..Z
if (0x41<=vim_vk && vim_vk<=0x5A)
    return vim_vk-0x41+SDL_SCANCODE_A;
// keypad 0,1,..,9
if (VK_NUMPAD0<=vim_vk && vim_vk<=VK_NUMPAD9)
    {
    if (VK_NUMPAD1<=vim_vk && vim_vk<=VK_NUMPAD9)// keypad 1..9
        return vim_vk-VK_NUMPAD1+SDL_SCANCODE_KP_1;
    else // keypad 0
        return SDL_SCANCODE_KP_0;
    }
// key F1 .. F24
if (VK_F1<=vim_vk && vim_vk<=VK_F24)
    {
    if (VK_F1<=vim_vk && vim_vk<=VK_F12) // F1 .. F12
        return vim_vk-VK_F1 + SDL_SCANCODE_F1;
    else // F13 .. F24
        return vim_vk - VK_F13 + SDL_SCANCODE_F13;
    }

char err[200];
snprintf(err, 200, "non-captured key: %d(%x)", vim_vk, vim_vk);
Error(err);
#undef MT
}
int vimGetModKeyState(int vim_vk)
{
#define kk(x) (sKeyboardState[SDL_SCANCODE_##x])
switch (vim_vk)
    {
    case VK_SHIFT:
        return kk(LSHIFT) | kk(RSHIFT);
    case VK_CONTROL:
        return kk(LCTRL) | kk(RCTRL);
    case VK_MENU:
        return kk(LMENU) | kk(RMENU);
    default:
        Error("keys other than mod keys used here.");
    }
#undef kk
}

// serves as TranslateMessage
static int myKeydownToChar(int vim_vk)
{
if (!vimGetModKeyState(VK_SHIFT))
    {
    if ('0'<=vim_vk && vim_vk<='9')
        return vim_vk;
    if  ('A'<=vim_vk && vim_vk<='Z')
        return vim_vk-'A'+'a';
    switch (vim_vk)
        {
#define MT(vvk, ch) case VK_##vvk: return ch;
        MT(OEM_PLUS,'=');
        MT(OEM_COMMA,',');
        MT(OEM_MINUS,'-');
        MT(OEM_PERIOD,'.');
        MT(OEM_1,';'); // ';'
        MT(OEM_2,'/'); // '/'
        MT(OEM_3,'`'); // '`' aka. backquote
        MT(OEM_4,'['); // '['
        MT(OEM_5, '\\'); // '\'
        MT(OEM_6,']'); // ']'
        MT(OEM_7,'\''); // ''' the quote mark itself
#undef MT
        }
    Error("non-captured key");
    }
else // shift is pressed
    {
    if ('A'<=vim_vk && vim_vk<='Z')
        return vim_vk;
    switch (vim_vk)
        {
#define MNU(nu, ch) case nu: return ch
        MNU('1','!');
        MNU('2','@');
        MNU('3','#');
        MNU('4','$');
        MNU('5','%');
        MNU('6','^');
        MNU('7','&');
        MNU('8','*');
        MNU('9','(');
        MNU('0',')');
#undef MNU
        }
    switch (vim_vk)
        {
#define MT2(vvk, ch) case vvk: return ch
        MT2(OEM_PLUS,'+');
        MT2(OEM_COMMA,'<');
        MT2(OEM_MINUS,'_');
        MT2(OEM_PERIOD,'>');
        MT2(OEM_1,':'); // ';'
        MT2(OEM_2,'?'); // '/'
        MT2(OEM_3,'~'); // '`' aka. backquote
        MT2(OEM_4,'{'); // '['
        MT2(OEM_5,'|'); // '\'
        MT2(OEM_6,'}'); // ']'
        MT2(OEM_7,'"'); // ''' the quote mark itself
#undef MT2
        }
    Error("non-captured key");
    }
}

// gui_w48.c 
// process_message() and _OnChar(hwnd, ch, cRepeat)
// This is the keyboard input handler.
// The original process_message() is the main input handler&dispatcher.
// _OnChar receives chars from process_message.
// Here I extracted the keyboard-related parts of the two functions
// and put them together.
static void process_vim_vk(int vim_vk)
{
UINT	vk = 0;		/* Virtual key */
char_u	string[40];
int		i;
int		modifiers = 0;
int		key;
/*
 * Check if it's a special key that we recognise.  If not, call
 * TranslateMessage().
 */
//vk = (int) msg.wParam;
vk=vim_vk;

/* Check for CTRL-BREAK */
if (vk == VK_CANCEL)
    {
    trash_input_buf();
    got_int = TRUE;
    string[0] = Ctrl_C;
    add_to_input_buf(string, 1);
    }
for (i = 0; special_keys[i].key_sym != 0; i++)
    {
    /* ignore VK_SPACE when ALT key pressed: system menu */
    if (special_keys[i].key_sym == vk
            && (vk != VK_SPACE || !(vimGetModKeyState(VK_MENU))))
        {
        if (vimGetModKeyState(VK_SHIFT))
            modifiers |= MOD_MASK_SHIFT;
        /*
         * Don't use caps-lock as shift, because these are special keys
         * being considered here, and we only want letters to get
         * shifted -- webb
         */
        /*
           if (vimGetModKeyState(VK_CAPITAL) & 0x0001)
           modifiers ^= MOD_MASK_SHIFT;
           */
        if (vimGetModKeyState(VK_CONTROL))
            modifiers |= MOD_MASK_CTRL;
        if (vimGetModKeyState(VK_MENU))
            modifiers |= MOD_MASK_ALT;

        if (special_keys[i].vim_code1 == NUL)
            key = special_keys[i].vim_code0;
        else
            key = TO_SPECIAL(special_keys[i].vim_code0,
                    special_keys[i].vim_code1);
        key = simplify_key(key, &modifiers);
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
if (special_keys[i].key_sym == 0)
    {
    char_u string[40];
    int len=0;
    int ch=myKeydownToChar(vim_vk);
    len = char_to_string(ch, string, 40, FALSE);
    if (len == 1 && string[0] == Ctrl_C && ctrl_c_interrupts)
        {
        trash_input_buf();
        got_int = TRUE;
        }

    add_to_input_buf(string, len);
    /* Some keys need C-S- where they should only need C-.
     * Ignore 0xff, Windows XP sends it when NUMLOCK has changed since
     * system startup (Helmut Stiegler, 2003 Oct 3). */
    
    /*
     * *comment*
    if (vk != 0xff
            && (vimGetModKeyState(VK_CONTROL))
            && !(vimGetModKeyState(VK_SHIFT))
            && !(vimGetModKeyState(VK_MENU)))
        {
        // CTRL-6 is '^'; Japanese keyboard maps '^' to vk == 0xDE 
        if (vk == '6' || MapVirtualKey(vk, 2) == (UINT)'^')
            {
            string[0] = Ctrl_HAT;
            add_to_input_buf(string, 1);
            }
        // vk == 0xBD AZERTY for CTRL-'-', but CTRL-[ for * QWERTY! 
        else if (vk == 0xBD)	// QWERTY for CTRL-'-' 
            {
            string[0] = Ctrl__;
            add_to_input_buf(string, 1);
            }
        // CTRL-2 is '@'; Japanese keyboard maps '@' to vk == 0xC0 
        
        else if (vk == '2' || MapVirtualKey(vk, 2) == (UINT)'@')
            {
            string[0] = Ctrl_AT;
            add_to_input_buf(string, 1);
            }
            
        else
            MyTranslateMessage(&msg);
        }
    else
        MyTranslateMessage(&msg);
    */
    }
}


// gui_w48.c 
// _TextAreaWndProc(hwnd, uMsg, wParam, lParam)
// is used to be registered as the event handler of text area.
// unprocessed msg goes to DefWindowProc if there is no FEAT_MBYTE
// otherwise goes to vim_WindowProc, which only checks whether should use
// the wide version of DefWindowProc.
int handleMouseEvent(SDL_Event evnt)
{
handleMouseHide(evnt);
// all mouse event types are listed below.
switch (evnt.type)
    {
    case SDL_MOUSEMOTION:
    case SDL_MOUSEBUTTONUP:
        _OnMouseMoveOrRelease(evnt);
        break;
    case SDL_MOUSEBUTTONDOWN:
        _OnMouseButtonDown(evnt);
        break;
    case SDL_MOUSEWHEEL:
        _OnMouseWheel(evnt);
        break;
    }
return 0;
}

static void _OnMouseButtonDown(SDL_Event evnt)
{
    static LONG	s_prevTime = 0;

    LONG    currentTime = SDL_GetTicks();
    int	    button = -1;
    int	    repeated_click;


    if (evnt.button==SDL_BUTTON_LEFT)
	button = MOUSE_LEFT;
    else if (evnt.button==SDL_BUTTON_MIDDLE)
	button = MOUSE_MIDDLE;
    else if (evnt.button==SDL_BUTTON_RIGHT)
	button = MOUSE_RIGHT;
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
	    _OnMouseEvent(button, x, y, repeated_click, keyFlags);
	}
	else if ((repeated_click)
		|| (mouse_model_popup() && (button == MOUSE_RIGHT)))
	{
	    if (s_button_pending > -1)
	    {
		    _OnMouseEvent(s_button_pending, x, y, FALSE, keyFlags);
		    s_button_pending = -1;
	    }
	    /* TRACE("Button down at x %d, y %d\n", x, y); */
	    _OnMouseEvent(button, x, y, repeated_click, keyFlags);
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
	    s_kFlags_pending = keyFlags;
	}

	s_prevTime = currentTime;
    }
}

static void _OnMouseMoveOrRelease(SDL_Event evnt)
{
int button;

s_getting_focus = FALSE;
if (s_button_pending > -1)
    {
    /* Delayed action for mouse down event */
    _OnMouseEvent(s_button_pending, s_x_pending,
            s_y_pending, FALSE, s_kFlags_pending);
    s_button_pending = -1;
    }
if (evnt.motion.type==SDL_MOUSEMOTION)
    {
    /*
     * It's only a MOUSE_DRAG if one or more mouse buttons are being held
     * down.
     */
    if (!(keyFlags & (MK_LBUTTON | MK_MBUTTON | MK_RBUTTON
                    | MK_XBUTTON1 | MK_XBUTTON2)))
        {
        gui_mouse_moved(x, y);
        return;
        }

    /*
     * While button is down, keep grabbing mouse move events when
     * the mouse goes outside the window
     */
    SetCapture(s_textArea); // TODO
    button = MOUSE_DRAG;
    /* TRACE("  move at x %d, y %d\n", x, y); */
    }
else
    {
    ReleaseCapture(); // TODO
    button = MOUSE_RELEASE;
    /* TRACE("  up at x %d, y %d\n", x, y); */
    }

_OnMouseEvent(button, x, y, FALSE, keyFlags);
}


static int myShouldPaint()
{
return 1; // always return 1 for now. TODO optimize it.
}

// typedef long guicolor_T
void gui_mch_set_fg_color(guicolor_T color)
{
gui.currFgColor=color;
}
void gui_mch_set_bg_color(guicolor_T color)
{
gui.currBgColor=color;
}
void gui_mch_set_sp_color(guicolor_T color)
{
gui.currSpColor=color;
}
// typedef long_u GuiFont;
void gui_mch_set_font(GuiFont font)
{
gui.currFont=font;
}

/* flags :
 *     DRAW_UNDERL // under line
 *               C // under curl
 *          TRANSP // draw transparently
 *          BOLD // bold
 *          CURSOR // for cursor only.
 */
int gui_sdl2_draw_string(int row, int col, char *s, int len, int flags)
{
NotFinished();
}

void gui_mch_invert_rectangle(int r, int c, int nr, int nc)
{
int x=FILL_X(c),
    y=FILL_Y(r),
    w=gui.char_width * nc,
    h=gui.char_height* nr;
SDL_LockSurface(sVimSurface);
NotFinished();
SDL_UnlockSurface(sVimSurface);
}

// gui.c
// int gui_outstr_nowrap(s,len,flags,fg,bg,back)
int gui_outstr_nowrap(char_u *s, int len, int flags, 
        guicolor_T fg, guicolor_T bg, int back)
{
    long_u	highlight_mask;
    long_u	hl_mask_todo;
    guicolor_T	fg_color;
    guicolor_T	bg_color;
    guicolor_T	sp_color;
    GuiFont	font = NOFONT;
    GuiFont	wide_font = NOFONT;
    attrentry_T	*aep = NULL;
    int		draw_flags;
    int		col = gui.col;
#ifdef FEAT_SIGN_ICONS
    int		draw_sign = FALSE;
# ifdef FEAT_NETBEANS_INTG
    int		multi_sign = FALSE;
# endif
#endif
if (len<=0) len=strlen(s);
if (len==0) return OK;

if (gui.highlight_mask > HL_ALL)
    {
    aep = syn_gui_attr2entry(gui.highlight_mask);
    if (aep == NULL)	    /* highlighting not set */
        highlight_mask = 0;
    else
        highlight_mask = aep->ae_attr;
    }
else
    highlight_mask = gui.highlight_mask;
hl_mask_todo = highlight_mask;

/* Set the font */
if (aep != NULL && aep->ae_u.gui.font != NOFONT)
    font = aep->ae_u.gui.font;
else
    {
    if (hl_mask_todo & (HL_BOLD | HL_STANDOUT))
        {
        if ((hl_mask_todo & HL_ITALIC) && gui.boldital_font != NOFONT)
            {
            font = gui.boldital_font;
            hl_mask_todo &= ~(HL_BOLD | HL_STANDOUT | HL_ITALIC);
            }
        else if (gui.bold_font != NOFONT)
            {
            font = gui.bold_font;
            hl_mask_todo &= ~(HL_BOLD | HL_STANDOUT);
            }
        else
            font = gui.norm_font;
        }
    else if ((hl_mask_todo & HL_ITALIC) && gui.ital_font != NOFONT)
        {
        font = gui.ital_font;
        hl_mask_todo &= ~HL_ITALIC;
        }
    else
        font = gui.norm_font;

# ifdef FEAT_MBYTE
    /*
     * Choose correct wide_font by font.  wide_font should be set with font
     * at same time in above block.  But it will make many "ifdef" nasty
     * blocks.  So we do it here.
     */
    if (font == gui.boldital_font && gui.wide_boldital_font)
        wide_font = gui.wide_boldital_font;
    else if (font == gui.bold_font && gui.wide_bold_font)
        wide_font = gui.wide_bold_font;
    else if (font == gui.ital_font && gui.wide_ital_font)
        wide_font = gui.wide_ital_font;
    else if (font == gui.norm_font && gui.wide_font)
        wide_font = gui.wide_font;
# endif

    }
gui_mch_set_font(font);

draw_flags = 0;

/* Set the color */
bg_color = gui.back_pixel;
if ((flags & GUI_MON_IS_CURSOR) && gui.in_focus)
    {
    draw_flags |= DRAW_CURSOR;
    fg_color = fg;
    bg_color = bg;
    sp_color = fg;
    }
else if (aep != NULL)
    {
    fg_color = aep->ae_u.gui.fg_color;
    if (fg_color == INVALCOLOR)
        fg_color = gui.norm_pixel;
    bg_color = aep->ae_u.gui.bg_color;
    if (bg_color == INVALCOLOR)
        bg_color = gui.back_pixel;
    sp_color = aep->ae_u.gui.sp_color;
    if (sp_color == INVALCOLOR)
        sp_color = fg_color;
    }
else
    {
    fg_color = gui.norm_pixel;
    sp_color = fg_color;
    }

if (highlight_mask & (HL_INVERSE | HL_STANDOUT))
    {
    gui_mch_set_fg_color(bg_color);
    gui_mch_set_bg_color(fg_color);
    }
else
    {
    gui_mch_set_fg_color(fg_color);
    gui_mch_set_bg_color(bg_color);
    }
gui_mch_set_sp_color(sp_color);

/* Clear the selection if we are about to write over it */
if (!(flags & GUI_MON_NOCLEAR))
    clip_may_clear_selection(gui.row, gui.row);


#ifndef MSWIN16_FASTTEXT
/* If there's no bold font, then fake it */
if (hl_mask_todo & (HL_BOLD | HL_STANDOUT))
    draw_flags |= DRAW_BOLD;
#endif

/*
 * When drawing bold or italic characters the spill-over from the left
 * neighbor may be destroyed.  Let the caller backup to start redrawing
 * just after a blank.
 */
if (back != 0 && ((draw_flags & DRAW_BOLD) || (highlight_mask & HL_ITALIC)))
    return FAIL;

/* Do we underline the text? */
if ((hl_mask_todo & HL_UNDERLINE)
# ifndef MSWIN16_FASTTEXT
        || (hl_mask_todo & HL_ITALIC)
# endif
   )
    draw_flags |= DRAW_UNDERL;
/* Do we undercurl the text? */
if (hl_mask_todo & HL_UNDERCURL)
    draw_flags |= DRAW_UNDERC;

/* Do we draw transparently? */
if (flags & GUI_MON_TRS_CURSOR)
    draw_flags |= DRAW_TRANSP;

/* The value returned is the length in display cells */
len = gui_sdl2_draw_string(gui.row, col, s, len, draw_flags);
if (!(flags & (GUI_MON_IS_CURSOR | GUI_MON_TRS_CURSOR)))
    gui.col = col + len;

/* May need to invert it when it's part of the selection. */
if (flags & GUI_MON_NOCLEAR)
    clip_may_redraw_selection(gui.row, col, len);

if (!(flags & (GUI_MON_IS_CURSOR | GUI_MON_TRS_CURSOR)))
    {
    /* Invalidate the old physical cursor position if we wrote over it */
    if (gui.cursor_row == gui.row
            && gui.cursor_col >= col
            && gui.cursor_col < col + len)
        gui.cursor_is_valid = FALSE;
    }
return OK;
}

int myGetTextAreaWidth()
{
return sTextAreaRect.w;
}

// gui_w48.c
// void _OnPaint(hwnd)
static void _OnPaint()
{
if (!myShouldPaint()) return;
#define GG(name) sTextAreaRect.##name
gui_redraw(GG(x), GG(y), GG(w), GG(h));
#undef GG
}

// gui_w48.c
// static void gui_mswin_get_valid_dimensions(w,h,valid_w, valid_h)
// gui_w32.c
// static int _DuringSizing(fwSide, lprc)
// _DuringSizing is the resize handler of _WndProc.
// It adjusts delta width and delta height to multiples of gui.char_width
// and gui.char_height by invoking gui_mswin_get_valid_dimensions.
static void _OnResize(int new_w, int new_h)
{
// the delta of w and h should be multiples of gui.char_width 
// and gui.char_height
// TODO from gui.c gui_resize_shell i can see that the following adjustment
// to round down to multiples of character size is no help, because that 
// function already does this itself when updating gui.num_cols and 
// gui.num_rows
sTextAreaRealWidth=new_w;
sTextAreaRealHeight=new_h;
int draw_w=sTextAreaRect.w,
    draw_h=sTextAreaRect.h;
new_w=draw_w+(new_w-draw_w)/gui.char_width*gui.char_width;
new_h=draw_h+(new_h-draw_h)/gui.char_height*gui.char_height;
gui_resize_shell(new_w, new_h);
}

static void _OnSetFocus()
{
gui_focus_change(TRUE);
s_getting_focus=TRUE;
}

static void _OnKillFocus()
{
gui_focus_change(FALSE);
s_getting_focus=FALSE;
}

static void _OnScroll()
{
NotFinished();
}

int get_cmd_args(char *prog, char *cmdline, char ***argvp, char **tofree)
{
int argc=1;
char **argv=NULL;
argv=(char**)malloc(2*sizeof(char*));
argv[0]="vim sdl2 port";
argv[1]=NULL;
*argvp=argv;
return argc;
}


// get mouse position relative to text window.
void gui_mch_getmouse(int *x, int *y)
{
int mx, my;
SDL_GetMouseState(&mx, &my);
mx-=sTextAreaRect.x;
my-=sTextAreaRect.y;
//  mx and my need not to be greater than zero.
//  see gui.c gui_mouse_correct()
//Assert(mx>=0&&my>=0);
*x=mx;
*y=my;
}

void mySetState(int state, int msec)
{
// TODO 
// sState is a vector where sState[s]>0 means there are as  many miliseconds
// to wait before quitting it.
// sState[s]==0 means not currently in that.
// sState[s]<= means forever in that until another call removes it.
sState[state]=msec;
}

void gui_mch_flash(int msec)
{
mySetState(STATE_INVERT, msec);
}


void gui_mch_delete_lines(int row, int num_lines)
{
//do nothing.
}
void gui_mch_insert_lines(int row, int num_lines)
{
// do nothing.
}

void gui_mch_exit(int rc)
{
}

void gui_mch_wide_font_changed()
{
/* 
 * TODO
    gui_mch_free_font(gui.wide_ital_font);
    gui.wide_ital_font = NOFONT;
    gui_mch_free_font(gui.wide_bold_font);
    gui.wide_bold_font = NOFONT;
    gui_mch_free_font(gui.wide_boldital_font);
    gui.wide_boldital_font = NOFONT;

    if(gui.wide_font)
        {
        //update the fonts deleted previously.
        }
        */
}


int gui_mch_init_font(char_u *font_name, int fontset)
{
NotFinished();
}

int gui_mch_maximized()
{
return 0;
}

// gui_w48.c
// void gui_mch_newfont()
// the original function is used when window is maximized and font changed.
// I am not using maximized mode so this is no use.
void gui_mch_newfont()
{
}

void gui_mch_settitle(char_u *title, char_u *icon)
{
}

void mch_set_mouse_shape(int shape)
{
Warn("set mouse shape");
}


// gui_w48.c
// void gui_mch_setmouse(x,y)
// original function takes into consideration gui.border_offset
void gui_mch_setmouse(int x, int y)
{
Error("setting mouse position");
}


// gui_w48.c
// void gui_mch_set_text_area_pos(x,y,w,h)
void gui_mch_set_text_area(int x, int y, int w, int h)
{
}


// gui_w48.c
// copy&paste
    static char_u *
convert_filter(char_u *s)
{
    char_u	*res;
    unsigned	s_len = (unsigned)STRLEN(s);
    unsigned	i;

    res = alloc(s_len + 3);
    if (res != NULL)
    {
	for (i = 0; i < s_len; ++i)
	    if (s[i] == '\t' || s[i] == '\n')
		res[i] = '\0';
	    else
		res[i] = s[i];
	res[s_len] = NUL;
	/* Add two extra NULs to make sure it's properly terminated. */
	res[s_len + 1] = NUL;
	res[s_len + 2] = NUL;
    }
    return res;
}

/*
 * Select a directory.
 */
    char_u *
gui_mch_browsedir(char_u *title, char_u *initdir)
{
    /* We fake this: Use a filter that doesn't select anything and a default
     * file name that won't be used. */
    return gui_mch_browse(0, title, (char_u *)_("Not Used"), NULL,
			      initdir, (char_u *)_("Directory\t*.nothing\n"));
}

/*
 * Pop open a file browser and return the file selected, in allocated memory,
 * or NULL if Cancel is hit.
 *  saving  - TRUE if the file will be saved to, FALSE if it will be opened.
 *  title   - Title message for the file browser dialog.
 *  dflt    - Default name of file.
 *  ext     - Default extension to be added to files without extensions.
 *  initdir - directory in which to open the browser (NULL = current dir)
 *  filter  - Filter for matched files to choose from.
 *
 * Keep in sync with gui_mch_browseW() above!
 */
    char_u *
gui_mch_browse(
	int saving,
	char_u *title,
	char_u *dflt,
	char_u *ext,
	char_u *initdir,
	char_u *filter)
{
    OPENFILENAME	fileStruct;
    char_u		fileBuf[MAXPATHL];
    char_u		*initdirp = NULL;
    char_u		*filterp;
    char_u		*p;

# if defined(FEAT_MBYTE) && defined(WIN3264)
    if (os_version.dwPlatformId == VER_PLATFORM_WIN32_NT)
	return gui_mch_browseW(saving, title, dflt, ext, initdir, filter);
# endif

    if (dflt == NULL)
	fileBuf[0] = NUL;
    else
	vim_strncpy(fileBuf, dflt, MAXPATHL - 1);

    /* Convert the filter to Windows format. */
    filterp = convert_filter(filter);

    vim_memset(&fileStruct, 0, sizeof(OPENFILENAME));
#ifdef OPENFILENAME_SIZE_VERSION_400
    /* be compatible with Windows NT 4.0 */
    fileStruct.lStructSize = OPENFILENAME_SIZE_VERSION_400;
#else
    fileStruct.lStructSize = sizeof(fileStruct);
#endif

    fileStruct.lpstrTitle = title;
    fileStruct.lpstrDefExt = ext;

    fileStruct.lpstrFile = fileBuf;
    fileStruct.nMaxFile = MAXPATHL;
    fileStruct.lpstrFilter = filterp;
    fileStruct.hwndOwner = s_hwnd;		/* main Vim window is owner*/
    /* has an initial dir been specified? */
    if (initdir != NULL && *initdir != NUL)
    {
	/* Must have backslashes here, no matter what 'shellslash' says */
	initdirp = vim_strsave(initdir);
	if (initdirp != NULL)
	    for (p = initdirp; *p != NUL; ++p)
		if (*p == '/')
		    *p = '\\';
	fileStruct.lpstrInitialDir = initdirp;
    }

    /*
     * TODO: Allow selection of multiple files.  Needs another arg to this
     * function to ask for it, and need to use OFN_ALLOWMULTISELECT below.
     * Also, should we use OFN_FILEMUSTEXIST when opening?  Vim can edit on
     * files that don't exist yet, so I haven't put it in.  What about
     * OFN_PATHMUSTEXIST?
     * Don't use OFN_OVERWRITEPROMPT, Vim has its own ":confirm" dialog.
     */
    fileStruct.Flags = (OFN_NOCHANGEDIR | OFN_PATHMUSTEXIST | OFN_HIDEREADONLY);
#ifdef FEAT_SHORTCUT
    if (curbuf->b_p_bin)
	fileStruct.Flags |= OFN_NODEREFERENCELINKS;
#endif
    if (saving)
    {
	if (!GetSaveFileName(&fileStruct))
	    return NULL;
    }
    else
    {
	if (!GetOpenFileName(&fileStruct))
	    return NULL;
    }

    vim_free(filterp);
    vim_free(initdirp);

    /* Give focus back to main window (when using MDI). */
    SetFocus(s_hwnd);

    /* Shorten the file name if possible */
    return vim_strsave(shorten_fname1((char_u *)fileBuf));
}


static void _OnDropFiles()
{
Error("dropping files on me");
}



#include "end_ns_vim.h"

