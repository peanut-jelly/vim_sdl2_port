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
 * Windows GUI.
 *
 * GUI support for Microsoft Windows.  Win32 initially; maybe Win16 later
 *
 * George V. Reilly <george@reilly.org> wrote the original Win32 GUI.
 * Robert Webb reworked it to use the existing GUI stuff and added menu,
 * scrollbars, etc.
 *
 * Note: Clipboard stuff, for cutting and pasting text to other windows, is in
 * os_win32.c.	(It can also be done from the terminal version).
 *
 * TODO: Some of the function signatures ought to be updated for Win64;
 * e.g., replace LONG with LONG_PTR, etc.
 */

#include "vim.h"
#include "adapter_sdl2.h"

/*
 * These are new in Windows ME/XP, only defined in recent compilers.
 */
#ifndef HANDLE_WM_XBUTTONUP
# define HANDLE_WM_XBUTTONUP(hwnd, wParam, lParam, fn) \
   ((fn)((hwnd), (int)(short)LOWORD(lParam), (int)(short)HIWORD(lParam), (UINT)(wParam)), 0L)
#endif
#ifndef HANDLE_WM_XBUTTONDOWN
# define HANDLE_WM_XBUTTONDOWN(hwnd, wParam, lParam, fn) \
   ((fn)((hwnd), FALSE, (int)(short)LOWORD(lParam), (int)(short)HIWORD(lParam), (UINT)(wParam)), 0L)
#endif
#ifndef HANDLE_WM_XBUTTONDBLCLK
# define HANDLE_WM_XBUTTONDBLCLK(hwnd, wParam, lParam, fn) \
   ((fn)((hwnd), TRUE, (int)(short)LOWORD(lParam), (int)(short)HIWORD(lParam), (UINT)(wParam)), 0L)
#endif

/*
 * Include the common stuff for MS-Windows GUI.
 */
#include "gui_w48.c"

#ifdef FEAT_XPM_W32
# include "xpm_w32.h"
#endif

#ifdef PROTO
# define WINAPI
#endif

#ifdef __MINGW32__
/*
 * Add a lot of missing defines.
 * They are not always missing, we need the #ifndef's.
 */
# ifndef _cdecl
#  define _cdecl
# endif
# ifndef IsMinimized
#  define     IsMinimized(hwnd)		IsIconic(hwnd)
# endif
# ifndef IsMaximized
#  define     IsMaximized(hwnd)		IsZoomed(hwnd)
# endif
# ifndef SelectFont
#  define     SelectFont(hdc, hfont)  ((HFONT)SelectObject((hdc), (HGDIOBJ)(HFONT)(hfont)))
# endif
# ifndef GetStockBrush
#  define     GetStockBrush(i)     ((HBRUSH)GetStockObject(i))
# endif
# ifndef DeleteBrush
#  define     DeleteBrush(hbr)     DeleteObject((HGDIOBJ)(HBRUSH)(hbr))
# endif

# ifndef HANDLE_WM_RBUTTONDBLCLK
#  define HANDLE_WM_RBUTTONDBLCLK(hwnd, wParam, lParam, fn) \
    ((fn)((hwnd), TRUE, (int)(short)LOWORD(lParam), (int)(short)HIWORD(lParam), (UINT)(wParam)), 0L)
# endif
# ifndef HANDLE_WM_MBUTTONUP
#  define HANDLE_WM_MBUTTONUP(hwnd, wParam, lParam, fn) \
    ((fn)((hwnd), (int)(short)LOWORD(lParam), (int)(short)HIWORD(lParam), (UINT)(wParam)), 0L)
# endif
# ifndef HANDLE_WM_MBUTTONDBLCLK
#  define HANDLE_WM_MBUTTONDBLCLK(hwnd, wParam, lParam, fn) \
    ((fn)((hwnd), TRUE, (int)(short)LOWORD(lParam), (int)(short)HIWORD(lParam), (UINT)(wParam)), 0L)
# endif
# ifndef HANDLE_WM_LBUTTONDBLCLK
#  define HANDLE_WM_LBUTTONDBLCLK(hwnd, wParam, lParam, fn) \
    ((fn)((hwnd), TRUE, (int)(short)LOWORD(lParam), (int)(short)HIWORD(lParam), (UINT)(wParam)), 0L)
# endif
# ifndef HANDLE_WM_RBUTTONDOWN
#  define HANDLE_WM_RBUTTONDOWN(hwnd, wParam, lParam, fn) \
    ((fn)((hwnd), FALSE, (int)(short)LOWORD(lParam), (int)(short)HIWORD(lParam), (UINT)(wParam)), 0L)
# endif
# ifndef HANDLE_WM_MOUSEMOVE
#  define HANDLE_WM_MOUSEMOVE(hwnd, wParam, lParam, fn) \
    ((fn)((hwnd), (int)(short)LOWORD(lParam), (int)(short)HIWORD(lParam), (UINT)(wParam)), 0L)
# endif
# ifndef HANDLE_WM_RBUTTONUP
#  define HANDLE_WM_RBUTTONUP(hwnd, wParam, lParam, fn) \
    ((fn)((hwnd), (int)(short)LOWORD(lParam), (int)(short)HIWORD(lParam), (UINT)(wParam)), 0L)
# endif
# ifndef HANDLE_WM_MBUTTONDOWN
#  define HANDLE_WM_MBUTTONDOWN(hwnd, wParam, lParam, fn) \
    ((fn)((hwnd), FALSE, (int)(short)LOWORD(lParam), (int)(short)HIWORD(lParam), (UINT)(wParam)), 0L)
# endif
# ifndef HANDLE_WM_LBUTTONUP
#  define HANDLE_WM_LBUTTONUP(hwnd, wParam, lParam, fn) \
    ((fn)((hwnd), (int)(short)LOWORD(lParam), (int)(short)HIWORD(lParam), (UINT)(wParam)), 0L)
# endif
# ifndef HANDLE_WM_LBUTTONDOWN
#  define HANDLE_WM_LBUTTONDOWN(hwnd, wParam, lParam, fn) \
    ((fn)((hwnd), FALSE, (int)(short)LOWORD(lParam), (int)(short)HIWORD(lParam), (UINT)(wParam)), 0L)
# endif
# ifndef HANDLE_WM_SYSCHAR
#  define HANDLE_WM_SYSCHAR(hwnd, wParam, lParam, fn) \
    ((fn)((hwnd), (TCHAR)(wParam), (int)(short)LOWORD(lParam)), 0L)
# endif
# ifndef HANDLE_WM_ACTIVATEAPP
#  define HANDLE_WM_ACTIVATEAPP(hwnd, wParam, lParam, fn) \
    ((fn)((hwnd), (BOOL)(wParam), (DWORD)(lParam)), 0L)
# endif
# ifndef HANDLE_WM_WINDOWPOSCHANGING
#  define HANDLE_WM_WINDOWPOSCHANGING(hwnd, wParam, lParam, fn) \
    (LRESULT)(DWORD)(BOOL)(fn)((hwnd), (LPWINDOWPOS)(lParam))
# endif
# ifndef HANDLE_WM_VSCROLL
#  define HANDLE_WM_VSCROLL(hwnd, wParam, lParam, fn) \
    ((fn)((hwnd), (HWND)(lParam), (UINT)(LOWORD(wParam)),  (int)(short)HIWORD(wParam)), 0L)
# endif
# ifndef HANDLE_WM_SETFOCUS
#  define HANDLE_WM_SETFOCUS(hwnd, wParam, lParam, fn) \
    ((fn)((hwnd), (HWND)(wParam)), 0L)
# endif
# ifndef HANDLE_WM_KILLFOCUS
#  define HANDLE_WM_KILLFOCUS(hwnd, wParam, lParam, fn) \
    ((fn)((hwnd), (HWND)(wParam)), 0L)
# endif
# ifndef HANDLE_WM_HSCROLL
#  define HANDLE_WM_HSCROLL(hwnd, wParam, lParam, fn) \
    ((fn)((hwnd), (HWND)(lParam), (UINT)(LOWORD(wParam)), (int)(short)HIWORD(wParam)), 0L)
# endif
# ifndef HANDLE_WM_DROPFILES
#  define HANDLE_WM_DROPFILES(hwnd, wParam, lParam, fn) \
    ((fn)((hwnd), (HDROP)(wParam)), 0L)
# endif
# ifndef HANDLE_WM_CHAR
#  define HANDLE_WM_CHAR(hwnd, wParam, lParam, fn) \
    ((fn)((hwnd), (TCHAR)(wParam), (int)(short)LOWORD(lParam)), 0L)
# endif
# ifndef HANDLE_WM_SYSDEADCHAR
#  define HANDLE_WM_SYSDEADCHAR(hwnd, wParam, lParam, fn) \
    ((fn)((hwnd), (TCHAR)(wParam), (int)(short)LOWORD(lParam)), 0L)
# endif
# ifndef HANDLE_WM_DEADCHAR
#  define HANDLE_WM_DEADCHAR(hwnd, wParam, lParam, fn) \
    ((fn)((hwnd), (TCHAR)(wParam), (int)(short)LOWORD(lParam)), 0L)
# endif
#endif /* __MINGW32__ */


/* Some parameters for tearoff menus.  All in pixels. */
#define TEAROFF_PADDING_X	2
#define TEAROFF_BUTTON_PAD_X	8
#define TEAROFF_MIN_WIDTH	200
#define TEAROFF_SUBMENU_LABEL	">>"
#define TEAROFF_COLUMN_PADDING	3	// # spaces to pad column with.


/* For the Intellimouse: */
#ifndef WM_MOUSEWHEEL
#define WM_MOUSEWHEEL	0x20a
#endif


#ifdef FEAT_BEVAL
# define ID_BEVAL_TOOLTIP   200
# define BEVAL_TEXT_LEN	    MAXPATHL

#if (defined(_MSC_VER) && _MSC_VER < 1300) || !defined(MAXULONG_PTR)
/* Work around old versions of basetsd.h which wrongly declares
 * UINT_PTR as unsigned long. */
# undef  UINT_PTR
# define UINT_PTR UINT
#endif

static void make_tooltip __ARGS((BalloonEval *beval, char *text, POINT pt));
static void delete_tooltip __ARGS((BalloonEval *beval));
static VOID CALLBACK BevalTimerProc __ARGS((HWND hwnd, UINT uMsg, UINT_PTR idEvent, DWORD dwTime));

static BalloonEval  *cur_beval = NULL;
static UINT_PTR	    BevalTimerId = 0;
static DWORD	    LastActivity = 0;


/* cproto fails on missing include files */
#ifndef PROTO

/*
 * excerpts from headers since this may not be presented
 * in the extremely old compilers
 */
# include <pshpack1.h>

#endif

typedef struct _DllVersionInfo
{
    DWORD cbSize;
    DWORD dwMajorVersion;
    DWORD dwMinorVersion;
    DWORD dwBuildNumber;
    DWORD dwPlatformID;
} DLLVERSIONINFO;

#ifndef PROTO
# include <poppack.h>
#endif

typedef struct tagTOOLINFOA_NEW
{
	UINT cbSize;
	UINT uFlags;
	HWND hwnd;
	UINT_PTR uId;
	RECT rect;
	HINSTANCE hinst;
	LPSTR lpszText;
	LPARAM lParam;
} TOOLINFO_NEW;

typedef struct tagNMTTDISPINFO_NEW
{
    NMHDR      hdr;
    LPSTR      lpszText;
    char       szText[80];
    HINSTANCE  hinst;
    UINT       uFlags;
    LPARAM     lParam;
} NMTTDISPINFO_NEW;

typedef HRESULT (WINAPI* DLLGETVERSIONPROC)(DLLVERSIONINFO *);
#ifndef TTM_SETMAXTIPWIDTH
# define TTM_SETMAXTIPWIDTH	(WM_USER+24)
#endif

#ifndef TTF_DI_SETITEM
# define TTF_DI_SETITEM		0x8000
#endif

#ifndef TTN_GETDISPINFO
# define TTN_GETDISPINFO	(TTN_FIRST - 0)
#endif

#endif /* defined(FEAT_BEVAL) */

#if defined(FEAT_TOOLBAR) || defined(FEAT_GUI_TABLINE)
/* Older MSVC compilers don't have LPNMTTDISPINFO[AW] thus we need to define
 * it here if LPNMTTDISPINFO isn't defined.
 * MingW doesn't define LPNMTTDISPINFO but typedefs it.  Thus we need to check
 * _MSC_VER. */
# if !defined(LPNMTTDISPINFO) && defined(_MSC_VER)
typedef struct tagNMTTDISPINFOA {
    NMHDR	hdr;
    LPSTR	lpszText;
    char	szText[80];
    HINSTANCE	hinst;
    UINT	uFlags;
    LPARAM	lParam;
} NMTTDISPINFOA, *LPNMTTDISPINFOA;
#  define LPNMTTDISPINFO LPNMTTDISPINFOA

#  ifdef FEAT_MBYTE
typedef struct tagNMTTDISPINFOW {
    NMHDR	hdr;
    LPWSTR	lpszText;
    WCHAR	szText[80];
    HINSTANCE	hinst;
    UINT	uFlags;
    LPARAM	lParam;
} NMTTDISPINFOW, *LPNMTTDISPINFOW;
#  endif
# endif
#endif

#ifndef TTN_GETDISPINFOW
# define TTN_GETDISPINFOW	(TTN_FIRST - 10)
#endif

/* Local variables: */

#ifdef FEAT_MENU
static UINT	s_menu_id = 100;
#endif

/*
 * Use the system font for dialogs and tear-off menus.  Remove this line to
 * use DLG_FONT_NAME.
 */
#define USE_SYSMENU_FONT

#define VIM_NAME	"vim"
#define VIM_CLASS	"Vim"
#define VIM_CLASSW	L"Vim"

/* Initial size for the dialog template.  For gui_mch_dialog() it's fixed,
 * thus there should be room for every dialog.  For tearoffs it's made bigger
 * when needed. */
#define DLG_ALLOC_SIZE 16 * 1024

/*
 * stuff for dialogs, menus, tearoffs etc.
 */
static LRESULT APIENTRY dialog_callback(HWND, UINT, WPARAM, LPARAM);
static LRESULT APIENTRY tearoff_callback(HWND, UINT, WPARAM, LPARAM);
static PWORD
add_dialog_element(
	PWORD p,
	DWORD lStyle,
	WORD x,
	WORD y,
	WORD w,
	WORD h,
	WORD Id,
	WORD clss,
	const char *caption);
static LPWORD lpwAlign(LPWORD);
static int nCopyAnsiToWideChar(LPWORD, LPSTR);
static void gui_mch_tearoff(char_u *title, vimmenu_T *menu, int initX, int initY);
static void get_dialog_font_metrics(void);

static int dialog_default_button = -1;

/* Intellimouse support */
static int mouse_scroll_lines = 0;
static UINT msh_msgmousewheel = 0;

static int	s_usenewlook;	    /* emulate W95/NT4 non-bold dialogs */
#ifdef FEAT_TOOLBAR
static void initialise_toolbar(void);
static LRESULT CALLBACK toolbar_wndproc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
static int get_toolbar_bitmap(vimmenu_T *menu);
#endif

#ifdef FEAT_GUI_TABLINE
static void initialise_tabline(void);
static LRESULT CALLBACK tabline_wndproc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
#endif

#ifdef FEAT_MBYTE_IME
static LRESULT _OnImeComposition(HWND hwnd, WPARAM dbcs, LPARAM param);
static char_u *GetResultStr(HWND hwnd, int GCS, int *lenp);
#endif
#if defined(FEAT_MBYTE_IME) && defined(DYNAMIC_IME)
# ifdef NOIME
typedef struct tagCOMPOSITIONFORM {
    DWORD dwStyle;
    POINT ptCurrentPos;
    RECT  rcArea;
} COMPOSITIONFORM, *PCOMPOSITIONFORM, NEAR *NPCOMPOSITIONFORM, FAR *LPCOMPOSITIONFORM;
typedef HANDLE HIMC;
# endif

static HINSTANCE hLibImm = NULL;
static LONG (WINAPI *pImmGetCompositionStringA)(HIMC, DWORD, LPVOID, DWORD);
static LONG (WINAPI *pImmGetCompositionStringW)(HIMC, DWORD, LPVOID, DWORD);
static HIMC (WINAPI *pImmGetContext)(HWND);
static HIMC (WINAPI *pImmAssociateContext)(HWND, HIMC);
static BOOL (WINAPI *pImmReleaseContext)(HWND, HIMC);
static BOOL (WINAPI *pImmGetOpenStatus)(HIMC);
static BOOL (WINAPI *pImmSetOpenStatus)(HIMC, BOOL);
static BOOL (WINAPI *pImmGetCompositionFont)(HIMC, LPLOGFONTA);
static BOOL (WINAPI *pImmSetCompositionFont)(HIMC, LPLOGFONTA);
static BOOL (WINAPI *pImmSetCompositionWindow)(HIMC, LPCOMPOSITIONFORM);
static BOOL (WINAPI *pImmGetConversionStatus)(HIMC, LPDWORD, LPDWORD);
static BOOL (WINAPI *pImmSetConversionStatus)(HIMC, DWORD, DWORD);
static void dyn_imm_load(void);
#else
# define pImmGetCompositionStringA ImmGetCompositionStringA
# define pImmGetCompositionStringW ImmGetCompositionStringW
# define pImmGetContext		  ImmGetContext
# define pImmAssociateContext	  ImmAssociateContext
# define pImmReleaseContext	  ImmReleaseContext
# define pImmGetOpenStatus	  ImmGetOpenStatus
# define pImmSetOpenStatus	  ImmSetOpenStatus
# define pImmGetCompositionFont   ImmGetCompositionFontA
# define pImmSetCompositionFont   ImmSetCompositionFontA
# define pImmSetCompositionWindow ImmSetCompositionWindow
# define pImmGetConversionStatus  ImmGetConversionStatus
# define pImmSetConversionStatus  ImmSetConversionStatus
#endif

#ifndef ETO_IGNORELANGUAGE
# define ETO_IGNORELANGUAGE  0x1000
#endif

/* multi monitor support */
typedef struct _MONITORINFOstruct
{
    DWORD cbSize;
    RECT rcMonitor;
    RECT rcWork;
    DWORD dwFlags;
} _MONITORINFO;

typedef HANDLE _HMONITOR;
typedef _HMONITOR (WINAPI *TMonitorFromWindow)(HWND, DWORD);
typedef BOOL (WINAPI *TGetMonitorInfo)(_HMONITOR, _MONITORINFO *);

static TMonitorFromWindow   pMonitorFromWindow = NULL;
static TGetMonitorInfo	    pGetMonitorInfo = NULL;
static HANDLE		    user32_lib = NULL;
#ifdef FEAT_NETBEANS_INTG
int WSInitialized = FALSE; /* WinSock is initialized */
#endif
/*
 * Return TRUE when running under Windows NT 3.x or Win32s, both of which have
 * less fancy GUI APIs.
 */
    static int
is_winnt_3(void)
{
    return ((os_version.dwPlatformId == VER_PLATFORM_WIN32_NT
		&& os_version.dwMajorVersion == 3)
	    || (os_version.dwPlatformId == VER_PLATFORM_WIN32s));
}

/*
 * Return TRUE when running under Win32s.
 */
    int
gui_is_win32s(void)
{
    return (os_version.dwPlatformId == VER_PLATFORM_WIN32s);
}



/*
 * Setup for the Intellimouse
 */
    static void
init_mouse_wheel(void)
{

#ifndef SPI_GETWHEELSCROLLLINES
# define SPI_GETWHEELSCROLLLINES    104
#endif
#ifndef SPI_SETWHEELSCROLLLINES
# define SPI_SETWHEELSCROLLLINES    105
#endif

#define VMOUSEZ_CLASSNAME  "MouseZ"		/* hidden wheel window class */
#define VMOUSEZ_TITLE      "Magellan MSWHEEL"	/* hidden wheel window title */
#define VMSH_MOUSEWHEEL    "MSWHEEL_ROLLMSG"
#define VMSH_SCROLL_LINES  "MSH_SCROLL_LINES_MSG"

    HWND hdl_mswheel;
    UINT msh_msgscrolllines;

    msh_msgmousewheel = 0;
    mouse_scroll_lines = 3;	/* reasonable default */

    if ((os_version.dwPlatformId == VER_PLATFORM_WIN32_NT
		&& os_version.dwMajorVersion >= 4)
	    || (os_version.dwPlatformId == VER_PLATFORM_WIN32_WINDOWS
		&& ((os_version.dwMajorVersion == 4
			&& os_version.dwMinorVersion >= 10)
		    || os_version.dwMajorVersion >= 5)))
    {
	/* if NT 4.0+ (or Win98) get scroll lines directly from system */
	SystemParametersInfo(SPI_GETWHEELSCROLLLINES, 0,
		&mouse_scroll_lines, 0);
    }
    else if (os_version.dwPlatformId == VER_PLATFORM_WIN32_WINDOWS
	    || (os_version.dwPlatformId == VER_PLATFORM_WIN32_NT
		&& os_version.dwMajorVersion < 4))
    {	/*
	 * If Win95 or NT 3.51,
	 * try to find the hidden point32 window.
	 */
	hdl_mswheel = FindWindow(VMOUSEZ_CLASSNAME, VMOUSEZ_TITLE);
	if (hdl_mswheel)
	{
	    msh_msgscrolllines = RegisterWindowMessage(VMSH_SCROLL_LINES);
	    if (msh_msgscrolllines)
	    {
		mouse_scroll_lines = (int)SendMessage(hdl_mswheel,
			msh_msgscrolllines, 0, 0);
		msh_msgmousewheel  = RegisterWindowMessage(VMSH_MOUSEWHEEL);
	    }
	}
    }
}

#if 0
/* Intellimouse wheel handler */
    static void
_OnMouseWheel(
    HWND hwnd,
    short zDelta)
{
/* Treat a mouse wheel event as if it were a scroll request */
    int i;
    int size;
    HWND hwndCtl;

    if (curwin->w_scrollbars[SBAR_RIGHT].id != 0)
    {
	hwndCtl = curwin->w_scrollbars[SBAR_RIGHT].id;
	size = curwin->w_scrollbars[SBAR_RIGHT].size;
    }
    else if (curwin->w_scrollbars[SBAR_LEFT].id != 0)
    {
	hwndCtl = curwin->w_scrollbars[SBAR_LEFT].id;
	size = curwin->w_scrollbars[SBAR_LEFT].size;
    }
    else
	return;

    size = curwin->w_height;
    if (mouse_scroll_lines == 0)
	init_mouse_wheel();

    if (mouse_scroll_lines > 0
	    && mouse_scroll_lines < (size > 2 ? size - 2 : 1))
    {
	for (i = mouse_scroll_lines; i > 0; --i)
	    _OnScroll(hwnd, hwndCtl, zDelta >= 0 ? SB_LINEUP : SB_LINEDOWN, 0);
    }
    else
	_OnScroll(hwnd, hwndCtl, zDelta >= 0 ? SB_PAGEUP : SB_PAGEDOWN, 0);
}
#endif

#ifdef USE_SYSMENU_FONT
/*
 * Get Menu Font.
 * Return OK or FAIL.
 */
    static int
gui_w32_get_menu_font(LOGFONT *lf)
{
    NONCLIENTMETRICS nm;

    nm.cbSize = sizeof(NONCLIENTMETRICS);
    if (!SystemParametersInfo(
	    SPI_GETNONCLIENTMETRICS,
	    sizeof(NONCLIENTMETRICS),
	    &nm,
	    0))
	return FAIL;
    *lf = nm.lfMenuFont;
    return OK;
}
#endif


#if defined(FEAT_GUI_TABLINE) && defined(USE_SYSMENU_FONT)
/*
 * Set the GUI tabline font to the system menu font
 */
    static void
set_tabline_font(void)
{
    LOGFONT	lfSysmenu;
    HFONT	font;
    HWND	hwnd;
    HDC		hdc;
    HFONT	hfntOld;
    TEXTMETRIC	tm;

    if (gui_w32_get_menu_font(&lfSysmenu) != OK)
	return;

    font = CreateFontIndirect(&lfSysmenu);

    SendMessage(s_tabhwnd, WM_SETFONT, (WPARAM)font, TRUE);

    /*
     * Compute the height of the font used for the tab text
     */
    hwnd = GetDesktopWindow();
    hdc = GetWindowDC(hwnd);
    hfntOld = SelectFont(hdc, font);

    GetTextMetrics(hdc, &tm);

    SelectFont(hdc, hfntOld);
    ReleaseDC(hwnd, hdc);

    /*
     * The space used by the tab border and the space between the tab label
     * and the tab border is included as 7.
     */
    gui.tabline_height = tm.tmHeight + tm.tmInternalLeading + 7;
}
#endif

/*
 * Invoked when a setting was changed.
 */
    static LRESULT CALLBACK
_OnSettingChange(UINT n)
{
    if (n == SPI_SETWHEELSCROLLLINES)
	SystemParametersInfo(SPI_GETWHEELSCROLLLINES, 0,
		&mouse_scroll_lines, 0);
#if defined(FEAT_GUI_TABLINE) && defined(USE_SYSMENU_FONT)
    if (n == SPI_SETNONCLIENTMETRICS)
	set_tabline_font();
#endif
    return 0;
}

#ifdef FEAT_NETBEANS_INTG
    static void
_OnWindowPosChanged(
    HWND hwnd,
    const LPWINDOWPOS lpwpos)
{
    static int x = 0, y = 0, cx = 0, cy = 0;

    if (WSInitialized && (lpwpos->x != x || lpwpos->y != y
				     || lpwpos->cx != cx || lpwpos->cy != cy))
    {
	x = lpwpos->x;
	y = lpwpos->y;
	cx = lpwpos->cx;
	cy = lpwpos->cy;
	netbeans_frame_moved(x, y);
    }
    /* Allow to send WM_SIZE and WM_MOVE */
    FORWARD_WM_WINDOWPOSCHANGED(hwnd, lpwpos, MyWindowProc);
}
#endif

    static int
_DuringSizing(
    UINT fwSide,
    LPRECT lprc)
{
Warn("_DuringSizing called");
return TRUE;

#if 0
    int	    w, h;
    int	    valid_w, valid_h;
    int	    w_offset, h_offset;

    w = lprc->right - lprc->left;
    h = lprc->bottom - lprc->top;
    gui_mswin_get_valid_dimensions(w, h, &valid_w, &valid_h);
    w_offset = w - valid_w;
    h_offset = h - valid_h;

    if (fwSide == WMSZ_LEFT || fwSide == WMSZ_TOPLEFT
			    || fwSide == WMSZ_BOTTOMLEFT)
	lprc->left += w_offset;
    else if (fwSide == WMSZ_RIGHT || fwSide == WMSZ_TOPRIGHT
			    || fwSide == WMSZ_BOTTOMRIGHT)
	lprc->right -= w_offset;

    if (fwSide == WMSZ_TOP || fwSide == WMSZ_TOPLEFT
			    || fwSide == WMSZ_TOPRIGHT)
	lprc->top += h_offset;
    else if (fwSide == WMSZ_BOTTOM || fwSide == WMSZ_BOTTOMLEFT
			    || fwSide == WMSZ_BOTTOMRIGHT)
	lprc->bottom -= h_offset;
    return TRUE;
#endif // 0
}



/*
 * End of call-back routines
 */

/* parent window, if specified with -P */
HWND vim_parent_hwnd = NULL;

    static BOOL CALLBACK
FindWindowTitle(HWND hwnd, LPARAM lParam)
{
    char	buf[2048];
    char	*title = (char *)lParam;

    if (GetWindowText(hwnd, buf, sizeof(buf)))
    {
	if (strstr(buf, title) != NULL)
	{
	    /* Found it.  Store the window ref. and quit searching if MDI
	     * works. */
	    vim_parent_hwnd = FindWindowEx(hwnd, NULL, "MDIClient", NULL);
	    if (vim_parent_hwnd != NULL)
		return FALSE;
	}
    }
    return TRUE;	/* continue searching */
}

/*
 * Invoked for '-P "title"' argument: search for parent application to open
 * our window in.
 */
    void
gui_mch_set_parent(char *title)
{
    EnumWindows(FindWindowTitle, (LPARAM)title);
    if (vim_parent_hwnd == NULL)
    {
	EMSG2(_("E671: Cannot find window title \"%s\""), title);
	mch_exit(2);
    }
}

#ifndef FEAT_OLE
    static void
ole_error(char *arg)
{
    char buf[IOSIZE];

    /* Can't use EMSG() here, we have not finished initialisation yet. */
    vim_snprintf(buf, IOSIZE,
	    _("E243: Argument not supported: \"-%s\"; Use the OLE version."),
	    arg);
    mch_errmsg(buf);
}
#endif

/*
 * Parse the GUI related command-line arguments.  Any arguments used are
 * deleted from argv, and *argc is decremented accordingly.  This is called
 * when vim is started, whether or not the GUI has been started.
 */
    void
gui_mch_prepare(int *argc, char **argv)
{
    int		silent = FALSE;
    int		idx;

    /* Check for special OLE command line parameters */
    if ((*argc == 2 || *argc == 3) && (argv[1][0] == '-' || argv[1][0] == '/'))
    {
	/* Check for a "-silent" argument first. */
	if (*argc == 3 && STRICMP(argv[1] + 1, "silent") == 0
		&& (argv[2][0] == '-' || argv[2][0] == '/'))
	{
	    silent = TRUE;
	    idx = 2;
	}
	else
	    idx = 1;

	/* Register Vim as an OLE Automation server */
	if (STRICMP(argv[idx] + 1, "register") == 0)
	{
#ifdef FEAT_OLE
	    RegisterMe(silent);
	    mch_exit(0);
#else
	    if (!silent)
		ole_error("register");
	    mch_exit(2);
#endif
	}

	/* Unregister Vim as an OLE Automation server */
	if (STRICMP(argv[idx] + 1, "unregister") == 0)
	{
#ifdef FEAT_OLE
	    UnregisterMe(!silent);
	    mch_exit(0);
#else
	    if (!silent)
		ole_error("unregister");
	    mch_exit(2);
#endif
	}

	/* Ignore an -embedding argument. It is only relevant if the
	 * application wants to treat the case when it is started manually
	 * differently from the case where it is started via automation (and
	 * we don't).
	 */
	if (STRICMP(argv[idx] + 1, "embedding") == 0)
	{
#ifdef FEAT_OLE
	    *argc = 1;
#else
	    ole_error("embedding");
	    mch_exit(2);
#endif
	}
    }

#ifdef FEAT_OLE
#error fear_ole
    {
	int	bDoRestart = FALSE;

	InitOLE(&bDoRestart);
	/* automatically exit after registering */
	if (bDoRestart)
	    mch_exit(0);
    }
#endif

#ifdef FEAT_NETBEANS_INTG
#error fear_netbeans_intg
    {
	/* stolen from gui_x11.c */
	int arg;

	for (arg = 1; arg < *argc; arg++)
	    if (strncmp("-nb", argv[arg], 3) == 0)
	    {
		netbeansArg = argv[arg];
		mch_memmove(&argv[arg], &argv[arg + 1],
					    (--*argc - arg) * sizeof(char *));
		argv[*argc] = NULL;
		break;	/* enough? */
	    }
    }
#endif

    /* get the OS version info */
    os_version.dwOSVersionInfoSize = sizeof(os_version);
    GetVersionEx(&os_version); /* this call works on Win32s, Win95 and WinNT */

#if 0
    /* try and load the user32.dll library and get the entry points for
     * multi-monitor-support. */
    if ((user32_lib = vimLoadLib("User32.dll")) != NULL)
    {
	pMonitorFromWindow = (TMonitorFromWindow)GetProcAddress(user32_lib,
							 "MonitorFromWindow");

	/* there are ...A and ...W version of GetMonitorInfo - looking at
	 * winuser.h, they have exactly the same declaration. */
	pGetMonitorInfo = (TGetMonitorInfo)GetProcAddress(user32_lib,
							  "GetMonitorInfoA");
    }
#endif // 0

// not using these apis.
#if 0
#ifdef FEAT_MBYTE
    /* If the OS is Windows NT, use wide functions;
     * this enables common dialogs input unicode from IME. */
    if (os_version.dwPlatformId == VER_PLATFORM_WIN32_NT)
    {
	pDispatchMessage = DispatchMessageW;
	pGetMessage = GetMessageW;
	pIsDialogMessage = IsDialogMessageW;
	pPeekMessage = PeekMessageW;
    }
    else
    {
	pDispatchMessage = DispatchMessageA;
	pGetMessage = GetMessageA;
	pIsDialogMessage = IsDialogMessageA;
	pPeekMessage = PeekMessageA;
    }
#endif
#endif // 0
}

/*
 * Initialise the GUI.	Create all the windows, set up all the call-backs
 * etc.
 */
    int
gui_mch_init(void)
{
    const char szVimWndClass[] = VIM_CLASS;
    const char szTextAreaClass[] = "VimTextArea";
    WNDCLASS wndclass;
#ifdef FEAT_MBYTE
    const WCHAR szVimWndClassW[] = VIM_CLASSW;
    const WCHAR szTextAreaClassW[] = L"VimTextArea";
    WNDCLASSW wndclassw;
#endif
#ifdef GLOBAL_IME
    ATOM	atom;
#endif

    /* Return here if the window was already opened (happens when
     * gui_mch_dialog() is called early). */
    /*
    if (s_hwnd != NULL)
	goto theend;
        */


    /*
    gui.scrollbar_width = GetSystemMetrics(SM_CXVSCROLL);
    gui.scrollbar_height = GetSystemMetrics(SM_CYHSCROLL);
    */
    gui.scrollbar_width=0;
    gui.scrollbar_height=0;
#ifdef FEAT_MENU
    gui.menu_height = 0;	/* Windows takes care of this */
#endif
    gui.border_width = 0;

    //s_brush = CreateSolidBrush(GetSysColor(COLOR_BTNFACE));
    s_brush=0;

    /*
#ifdef FEAT_WINDOWS
    DragAcceptFiles(s_hwnd, TRUE);
#endif
*/

    gui_mch_def_colors();

    /* Get the colors from the "Normal" group (set in syntax.c or in a vimrc
     * file) */
    set_normal_colors();

    /*
     * Check that none of the colors are the same as the background color.
     * Then store the current values as the defaults.
     */
    gui_check_colors();
    gui.def_norm_pixel = gui.norm_pixel;
    gui.def_back_pixel = gui.back_pixel;

    /* Get the colors for the highlight groups (gui_check_colors() might have
     * changed them) */
    highlight_gui_started();

    /*
     * Start out by adding the configured border width into the border offset
     */
    gui.border_offset = gui.border_width + 2;	/*CLIENT EDGE*/

    /*
     * Set up for Intellimouse processing
     */
    init_mouse_wheel();

#ifdef FEAT_EVAL
# ifndef HandleToLong
/* HandleToLong() only exists in compilers that can do 64 bit builds */
#  define HandleToLong(h) ((long)(h))
# endif
    /* set the v:windowid variable */
    set_vim_var_nr(VV_WINDOWID, HandleToLong(s_hwnd));
#endif

theend:
    /* Display any pending error messages */
    display_errors();

    return OK;
}


/*
 * Set the size of the window to the given width and height in pixels.
 */
/*ARGSUSED*/
// only used in gui_set_shellsize
// no use now.
    void
gui_mch_set_shellsize(int width, int height,
	int min_width, int min_height, int base_width, int base_height,
	int direction)
{
// don't do the os-resize.
}


    void
gui_mch_set_scrollbar_thumb(
    scrollbar_T *sb,
    long	val,
    long	size,
    long	max)
{
// do nothing
}


/*
 * Set the current text font.
 */
    void
gui_mch_set_font(GuiFont font)
{
    gui.currFont = font;


    int which;
    int flags=0;
    if (font==gui.norm_font)
        {
        which=DISP_TASK_SETFONTNORMAL;
        flags = 0;
        }
    else if (font== gui.ital_font)
        {
        which=DISP_TASK_SETFONTNORMAL;
        flags= DISP_TASK_SETFONT_ITALIC;
        }
    else if(font == gui.bold_font)
        {
        which=DISP_TASK_SETFONTNORMAL;
        flags= DISP_TASK_SETFONT_BOLD;
        }
    else if (font == gui.boldital_font)
        {
        which=DISP_TASK_SETFONTNORMAL;
        flags= DISP_TASK_SETFONT_ITALIC | DISP_TASK_SETFONT_BOLD;
        }
    // otherwise wide font.
    else if (font == gui.wide_font)
        {
        which = DISP_TASK_SETFONTWIDE;
        flags = 0;
        }
    else if (font == gui.wide_bold_font)
        {
        which = DISP_TASK_SETFONTWIDE;
        flags= DISP_TASK_SETFONT_BOLD;
        }
    else if (font == gui.wide_ital_font)
        {
        which = DISP_TASK_SETFONTWIDE;
        flags= DISP_TASK_SETFONT_ITALIC;
        }
    else if (font == gui.wide_boldital_font)
        {
        which = DISP_TASK_SETFONTWIDE;
        flags= DISP_TASK_SETFONT_ITALIC | DISP_TASK_SETFONT_BOLD;
        }
    else // font specially passed in.
        {
        info_push_messagef("special font used. %d", SDL_GetTicks());
        which=DISP_TASK_SETFONTWIDE;
        flags = 0;
        }
    display_set_font(which, flags);
}


/*
 * Set the current text foreground color.
 */
    void
gui_mch_set_fg_color(guicolor_T color)
{
    gui.currFgColor = color;

        // inform sdl to change color.
        disp_task_setcolor_t scolor;
        scolor.type=DISP_TASK_SETCOLOR;
        scolor.which=DISP_TASK_SETCOLORFG;
        scolor.color=(SDL_Color)
            {
            myGetRValue(gui.currFgColor),
            myGetGValue(gui.currFgColor),
            myGetBValue(gui.currFgColor),
            0xff //GetAValue(gui.currBgColor)
            };
        display_push_task(&scolor);

}

/*
 * Set the current text background color.
 */
    void
gui_mch_set_bg_color(guicolor_T color)
{
    gui.currBgColor = color;
    
        // inform sdl to change color.
        disp_task_setcolor_t scolor;
        scolor.type=DISP_TASK_SETCOLOR;
        scolor.which=DISP_TASK_SETCOLORBG;
        scolor.color=(SDL_Color)
            {
            myGetRValue(gui.currBgColor),
            myGetGValue(gui.currBgColor),
            myGetBValue(gui.currBgColor),
            0xff //GetAValue(gui.currBgColor)
            };
        display_push_task(&scolor);
}

/*
 * Set the current text special color.
 */
    void
gui_mch_set_sp_color(guicolor_T color)
{
    gui.currSpColor = color;
    
        // inform sdl to change color.
        disp_task_setcolor_t scolor;
        scolor.type=DISP_TASK_SETCOLOR;
        scolor.which=DISP_TASK_SETCOLORSP;
        scolor.color=(SDL_Color)
            {
            myGetRValue(gui.currSpColor),
            myGetGValue(gui.currSpColor),
            myGetBValue(gui.currSpColor),
            0xff //GetAValue(gui.currBgColor)
            };
        display_push_task(&scolor);
}

#ifdef FEAT_MBYTE
/*
 * Convert latin9 text "text[len]" to ucs-2 in "unicodebuf".
 */
    static void
latin9_to_ucs(char_u *text, int len, WCHAR *unicodebuf)
{
    int		c;

    while (--len >= 0)
    {
	c = *text++;
	switch (c)
	{
	    case 0xa4: c = 0x20ac; break;   /* euro */
	    case 0xa6: c = 0x0160; break;   /* S hat */
	    case 0xa8: c = 0x0161; break;   /* S -hat */
	    case 0xb4: c = 0x017d; break;   /* Z hat */
	    case 0xb8: c = 0x017e; break;   /* Z -hat */
	    case 0xbc: c = 0x0152; break;   /* OE */
	    case 0xbd: c = 0x0153; break;   /* oe */
	    case 0xbe: c = 0x0178; break;   /* Y */
	}
	*unicodebuf++ = c;
    }
}
#endif

#ifdef FEAT_RIGHTLEFT
//#error feat_rightleft
/*
 * What is this for?  In the case where you are using Win98 or Win2K or later,
 * and you are using a Hebrew font (or Arabic!), Windows does you a favor and
 * reverses the string sent to the TextOut... family.  This sucks, because we
 * go to a lot of effort to do the right thing, and there doesn't seem to be a
 * way to tell Windblows not to do this!
 *
 * The short of it is that this 'RevOut' only gets called if you are running
 * one of the new, "improved" MS OSes, and only if you are running in
 * 'rightleft' mode.  It makes display take *slightly* longer, but not
 * noticeably so.
 */
    static void
RevOut( HDC s_hdc,
	int col,
	int row,
	UINT foptions,
	CONST RECT *pcliprect,
	LPCTSTR text,
	UINT len,
	CONST INT *padding)
{
Warn("calling RevOut");

    int		ix;
    static int	special = -1;

    if (special == -1)
    {
	/* Check windows version: special treatment is needed if it is NT 5 or
	 * Win98 or higher. */
	if  ((os_version.dwPlatformId == VER_PLATFORM_WIN32_NT
		    && os_version.dwMajorVersion >= 5)
		|| (os_version.dwPlatformId == VER_PLATFORM_WIN32_WINDOWS
		    && (os_version.dwMajorVersion > 4
			|| (os_version.dwMajorVersion == 4
			    && os_version.dwMinorVersion > 0))))
	    special = 1;
	else
	    special = 0;
    }

    if (special)
	for (ix = 0; ix < (int)len; ++ix)
	    ExtTextOut(s_hdc, col + TEXT_X(ix), row, foptions,
					    pcliprect, text + ix, 1, padding);
    else
	ExtTextOut(s_hdc, col, row, foptions, pcliprect, text, len, padding);
}
#endif

    void
gui_mch_draw_string(
    int		row,
    int		col,
    char_u	*text,
    int		len,
    int		flags)
{
    static int	*padding = NULL;
    static int	pad_size = 0;
    int		i;
    const RECT	*pcliprect = NULL;
    UINT	foptions = 0;
#ifdef FEAT_MBYTE
    static WCHAR *unicodebuf = NULL;
    static int   *unicodepdy = NULL;
    static int	unibuflen = 0;
    int		n = 0;
#endif
    HPEN	hpen, old_pen;
    int		y;

    /*
     * Italic and bold text seems to have an extra row of pixels at the bottom
     * (below where the bottom of the character should be).  If we draw the
     * characters with a solid background, the top row of pixels in the
     * character below will be overwritten.  We can fix this by filling in the
     * background ourselves, to the correct character proportions, and then
     * writing the character in transparent mode.  Still have a problem when
     * the character is "_", which gets written on to the character below.
     * New fix: set gui.char_ascent to -1.  This shifts all characters up one
     * pixel in their slots, which fixes the problem with the bottom row of
     * pixels.	We still need this code because otherwise the top row of pixels
     * becomes a problem. - webb.
     */
    static HBRUSH	hbr_cache[2] = {NULL, NULL};
    static guicolor_T	brush_color[2] = {INVALCOLOR, INVALCOLOR};
    static int		brush_lru = 0;
    HBRUSH		hbr;
    RECT		rc;

    if (!(flags & DRAW_TRANSP))
    {
	/*
	 * Clear background first.
	 * Note: FillRect() excludes right and bottom of rectangle.
	 */
	rc.left = FILL_X(col);
	rc.top = FILL_Y(row);
#ifdef FEAT_MBYTE
	if (has_mbyte)
	{
	    /* Compute the length in display cells. */
	    rc.right = FILL_X(col + mb_string2cells(text, len));
	}
	else
#endif
	    rc.right = FILL_X(col + len);
	rc.bottom = FILL_Y(row + 1);


	/* Cache the created brush, that saves a lot of time.  We need two:
	 * one for cursor background and one for the normal background. */
	if (gui.currBgColor == brush_color[0])
	{
	    hbr = hbr_cache[0];
	    brush_lru = 1;
	}
	else if (gui.currBgColor == brush_color[1])
	{
	    hbr = hbr_cache[1];
	    brush_lru = 0;
	}
	else
	{
	    if (hbr_cache[brush_lru] != NULL)
		DeleteBrush(hbr_cache[brush_lru]);
	    hbr_cache[brush_lru] = CreateSolidBrush(gui.currBgColor);
	    brush_color[brush_lru] = gui.currBgColor;
	    hbr = hbr_cache[brush_lru];
	    brush_lru = !brush_lru;
	}

        // inform sdl to fill rect.
        disp_task_clearrect_t clrect;
        clrect.type=DISP_TASK_CLEARRECT;
        clrect.rect=(Rect_f)
            {
            (double)col/Columns, (double)row/Rows, 
                (double)mb_string2cells(text,len)/Columns, (double)1/Rows
            };
        display_push_task(&clrect);

	/*
	 * When drawing block cursor, prevent inverted character spilling
	 * over character cell (can happen with bold/italic)
	 */
	if (flags & DRAW_CURSOR)
	{
	    pcliprect = &rc;
	    foptions = ETO_CLIPPED;
	}
    }

    if (pad_size != Columns || padding == NULL || padding[0] != gui.char_width)
    {
	vim_free(padding);
	pad_size = Columns;

	/* Don't give an out-of-memory message here, it would call us
	 * recursively. */
	padding = (int *)lalloc(pad_size * sizeof(int), FALSE);
	if (padding != NULL)
	    for (i = 0; i < pad_size; i++)
		padding[i] = gui.char_width;
    }

    /* On NT, tell the font renderer not to "help" us with Hebrew and Arabic
     * text.  This doesn't work in 9x, so we have to deal with it manually on
     * those systems. */
    if (os_version.dwPlatformId == VER_PLATFORM_WIN32_NT)
	foptions |= ETO_IGNORELANGUAGE;

    /*
     * We have to provide the padding argument because italic and bold versions
     * of fixed-width fonts are often one pixel or so wider than their normal
     * versions.
     * No check for DRAW_BOLD, Windows will have done it already.
     */

#ifdef FEAT_MBYTE
    /* Check if there are any UTF-8 characters.  If not, use normal text
     * output to speed up output. */
    if (enc_utf8)
	for (n = 0; n < len; ++n)
	    if (text[n] >= 0x80)
		break;

    /* Check if the Unicode buffer exists and is big enough.  Create it
     * with the same length as the multi-byte string, the number of wide
     * characters is always equal or smaller. */
    if ((enc_utf8
		|| (enc_codepage > 0 && (int)GetACP() != enc_codepage)
		|| enc_latin9)
	    && (unicodebuf == NULL || len > unibuflen))
    {
	vim_free(unicodebuf);
	unicodebuf = (WCHAR *)lalloc(len * sizeof(WCHAR), FALSE);

	vim_free(unicodepdy);
	unicodepdy = (int *)lalloc(len * sizeof(int), FALSE);

	unibuflen = len;
    }

    if (enc_utf8 && n < len && unicodebuf != NULL)
    {
	/* Output UTF-8 characters.  Caller has already separated
	 * composing characters. */
	int		i;
	int		wlen;	/* string length in words */
	int		clen;	/* string length in characters */
	int		cells;	/* cell width of string up to composing char */
	int		cw;	/* width of current cell */
	int		c;

	wlen = 0;
	clen = 0;
	cells = 0;
	for (i = 0; i < len; )
	{
	    c = utf_ptr2char(text + i);
	    if (c >= 0x10000)
	    {
		/* Turn into UTF-16 encoding. */
		unicodebuf[wlen++] = ((c - 0x10000) >> 10) + 0xD800;
		unicodebuf[wlen++] = ((c - 0x10000) & 0x3ff) + 0xDC00;
	    }
	    else
	    {
		unicodebuf[wlen++] = c;
	    }
	    cw = utf_char2cells(c);
	    if (cw > 2)		/* don't use 4 for unprintable char */
		cw = 1;
	    if (unicodepdy != NULL)
	    {
		/* Use unicodepdy to make characters fit as we expect, even
		 * when the font uses different widths (e.g., bold character
		 * is wider).  */
		unicodepdy[clen] = cw * gui.char_width;
	    }
	    cells += cw;
	    i += utfc_ptr2len_len(text + i, len - i);
	    ++clen;
	}

        //info_push_message("ExtTextOutW 2423");
        // utf-8 string is here.
        if (!is_valid_utf8(text, text+len))
            Warn("not valid utf8.");
        int u8len=utf8_distance(text, text+len);
        int na=num_ascii_chars(text, text+len);
        int ncell=mb_string2cells(text, len); // number of screen cells need
        disp_task_textout_t textout;
        display_fill_textout(&textout, 
                (double)col/Columns, (double)row/Rows,
                (double)ncell/Columns, (double)1/Rows,
                text, len);
        display_push_task(&textout);

        /*
	ExtTextOutW(s_hdc, TEXT_X(col), TEXT_Y(row),
			   foptions, pcliprect, unicodebuf, wlen, unicodepdy);
                           */
	len = cells;	/* used for underlining */
    }
    else if ((enc_codepage > 0 && (int)GetACP() != enc_codepage) || enc_latin9)
    {
	/* If we want to display codepage data, and the current CP is not the
	 * ANSI one, we need to go via Unicode. */
	if (unicodebuf != NULL)
	{
	    if (enc_latin9)
		latin9_to_ucs(text, len, unicodebuf);
	    else
		len = MultiByteToWideChar(enc_codepage,
			MB_PRECOMPOSED,
			(char *)text, len,
			(LPWSTR)unicodebuf, unibuflen);
	    if (len != 0)
	    {
		/* Use unicodepdy to make characters fit as we expect, even
		 * when the font uses different widths (e.g., bold character
		 * is wider). */
		if (unicodepdy != NULL)
		{
		    int i;
		    int cw;

		    for (i = 0; i < len; ++i)
		    {
			cw = utf_char2cells(unicodebuf[i]);
			if (cw > 2)
			    cw = 1;
			unicodepdy[i] = cw * gui.char_width;
		    }
		}
                Warn("textout ignored. because multibyte not utf-8");
                /*
		ExtTextOutW(s_hdc, TEXT_X(col), TEXT_Y(row),
			    foptions, pcliprect, unicodebuf, len, unicodepdy);
                            */
	    }
	}
    }
    else
#endif
    {
#ifdef FEAT_RIGHTLEFT
//#error feat_rightleft
	/* If we can't use ETO_IGNORELANGUAGE, we can't tell Windows not to
	 * mess up RL text, so we have to draw it character-by-character.
	 * Only do this if RL is on, since it's slow. */
	if (curwin->w_p_rl && !(foptions & ETO_IGNORELANGUAGE))
	    RevOut(s_hdc, TEXT_X(col), TEXT_Y(row),
			 foptions, pcliprect, (char *)text, len, padding);
	else
#endif
            {
            // i am using utf-8, so here is one-byte chars.
            // can i say here is only ascii chars?
            //info_push_message("ExtTextOut 2479");
            if (!is_valid_utf8(text, text+len))
                Warn("not valid utf8.");
            int u8len=utf8_distance(text, text+len);
            int ncell=mb_string2cells(text, len);
            disp_task_textout_t textout;
            display_fill_textout(&textout, 
                    (double)col/Columns, (double)row/Rows,
                    (double)ncell/Columns, (double)1/Rows,
                    text, len);
            display_push_task(&textout);
            /*
            ExtTextOut(s_hdc, TEXT_X(col), TEXT_Y(row),
                    foptions, pcliprect, (char *)text, len, padding);
                    */
            }
    }

    /* Underline */
    if (flags & DRAW_UNDERL)
    {
        // inform sdl to draw underline
        double y=(double)(row+1)/Rows-(double)6.0/Rows/gui.char_height;
        double x1=(double)col/Columns,
               x2=(double)(col+len)/Columns;
        disp_task_drawline_t drawline=
            {DISP_TASK_DRAWLINE, x1, y, x2, y};
        display_push_task(&drawline);
    }

    /* Undercurl */
    if (flags & DRAW_UNDERC)
    {
        //inform sdl to draw under curl
        double x=(double)col/Columns,
               w=(double)len/Columns;
        double y=(double)(row+1)/Rows-(double)6/Rows/gui.char_height;
        disp_task_undercurl_t undercurl=
            {DISP_TASK_UNDERCURL, x, y, w};
        display_push_task(&undercurl);
    }
}


/*
 * Output routines.
 */

/* Flush any output to the screen */
    void
gui_mch_flush(void)
{
disp_task_flush_t flush=
    {DISP_TASK_FLUSH};
display_push_task(&flush);
}


// only used in gui_set_shellsize @ gui.c
    void
gui_mch_get_screen_dimensions(int *screen_w, int *screen_h)
{
extern int screen_w_pending;
extern int screen_h_pending;
// only consider gui_get_base_width/height
// which is already in screen_w_pending and screen_h_pending
// assume they never change because i am not using them in sdl.
*screen_w = screen_w_pending;
*screen_h = screen_h_pending;
}



#if defined(FEAT_OLE) || defined(FEAT_EVAL) || defined(PROTO)
//#error fear_eval
/*
 * Make the GUI window come to the foreground.
 */
    void
gui_mch_set_foreground(void)
{
// do nothing
}
#endif


