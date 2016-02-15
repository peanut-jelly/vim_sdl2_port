/* vi:set ts=8 sts=4 sw=4:
 *
 * VIM - Vi IMproved	by Bram Moolenaar
 *
 * Do ":help uganda"  in Vim to read copying and usage conditions.
 * Do ":help credits" in Vim to see a list of people who contributed.
 * See README.txt for an overview of the Vim source code.
 */
/*
 * os_win32.c
 *
 * Used for both the console version and the Win32 GUI.  A lot of code is for
 * the console version only, so there is a lot of "#ifndef FEAT_GUI_W32".
 *
 * Win32 (Windows NT and Windows 95) system-dependent routines.
 * Portions lifted from the Win32 SDK samples, the MSDOS-dependent code,
 * NetHack 3.1.3, GNU Emacs 19.30, and Vile 5.5.
 *
 * George V. Reilly <george@reilly.org> wrote most of this.
 * Roger Knobbe <rogerk@wonderware.com> did the initial port of Vim 3.0.
 */

#include "vim.h"

#ifdef FEAT_MZSCHEME
# include "if_mzsch.h"
#endif

#include <sys/types.h>
#include <signal.h>
#include <limits.h>

/* cproto fails on missing include files */
#ifndef PROTO
# include <process.h>
#endif

#undef chdir
#ifdef __GNUC__
# ifndef __MINGW32__
#  include <dirent.h>
# endif
#else
# include <direct.h>
#endif

#ifndef PROTO
# if defined(FEAT_TITLE) && !defined(FEAT_GUI_W32)
#  include <shellapi.h>
# endif
#endif

#ifdef __MINGW32__
# ifndef FROM_LEFT_1ST_BUTTON_PRESSED
#  define FROM_LEFT_1ST_BUTTON_PRESSED    0x0001
# endif
# ifndef RIGHTMOST_BUTTON_PRESSED
#  define RIGHTMOST_BUTTON_PRESSED	  0x0002
# endif
# ifndef FROM_LEFT_2ND_BUTTON_PRESSED
#  define FROM_LEFT_2ND_BUTTON_PRESSED    0x0004
# endif
# ifndef FROM_LEFT_3RD_BUTTON_PRESSED
#  define FROM_LEFT_3RD_BUTTON_PRESSED    0x0008
# endif
# ifndef FROM_LEFT_4TH_BUTTON_PRESSED
#  define FROM_LEFT_4TH_BUTTON_PRESSED    0x0010
# endif

/*
 * EventFlags
 */
# ifndef MOUSE_MOVED
#  define MOUSE_MOVED   0x0001
# endif
# ifndef DOUBLE_CLICK
#  define DOUBLE_CLICK  0x0002
# endif
#endif

/*
 * Reparse Point
 */
#ifndef FILE_ATTRIBUTE_REPARSE_POINT
# define FILE_ATTRIBUTE_REPARSE_POINT	0x00000400
#endif
#ifndef IO_REPARSE_TAG_SYMLINK
# define IO_REPARSE_TAG_SYMLINK		0xA000000C
#endif

/* Record all output and all keyboard & mouse input */
/* #define MCH_WRITE_DUMP */


/*
 * When generating prototypes for Win32 on Unix, these lines make the syntax
 * errors disappear.  They do not need to be correct.
 */



#ifndef PROTO

/* Enable common dialogs input unicode from IME if possible. */
#ifdef FEAT_MBYTE
LRESULT (WINAPI *pDispatchMessage)(CONST MSG *) = DispatchMessage;
BOOL (WINAPI *pGetMessage)(LPMSG, HWND, UINT, UINT) = GetMessage;
BOOL (WINAPI *pIsDialogMessage)(HWND, LPMSG) = IsDialogMessage;
BOOL (WINAPI *pPeekMessage)(LPMSG, HWND, UINT, UINT, UINT) = PeekMessage;
#endif

#endif /* PROTO */

static int s_dont_use_vimrun = TRUE;
static int need_vimrun_warning = FALSE;
static char *vimrun_path = "vimrun ";

static int win32_getattrs(char_u *name);
static int win32_setattrs(char_u *name, int attrs);
static int win32_set_archive(char_u *name);


static char_u *exe_path = NULL;

    static void
get_exe_name(void)
{
    /* Maximum length of $PATH is more than MAXPATHL.  8191 is often mentioned
     * as the maximum length that works (plus a NUL byte). */
#define MAX_ENV_PATH_LEN 8192
    char	temp[MAX_ENV_PATH_LEN];
    char_u	*p;

    if (exe_name == NULL)
    {
	/* store the name of the executable, may be used for $VIM */
	GetModuleFileName(NULL, temp, MAX_ENV_PATH_LEN - 1);
	if (*temp != NUL)
	    exe_name = FullName_save((char_u *)temp, FALSE);
    }

    if (exe_path == NULL && exe_name != NULL)
    {
	exe_path = vim_strnsave(exe_name,
				     (int)(gettail_sep(exe_name) - exe_name));
	if (exe_path != NULL)
	{
	    /* Append our starting directory to $PATH, so that when doing
	     * "!xxd" it's found in our starting directory.  Needed because
	     * SearchPath() also looks there. */
	    p = mch_getenv("PATH");
	    if (p == NULL
		       || STRLEN(p) + STRLEN(exe_path) + 2 < MAX_ENV_PATH_LEN)
	    {
		if (p == NULL || *p == NUL)
		    temp[0] = NUL;
		else
		{
		    STRCPY(temp, p);
		    STRCAT(temp, ";");
		}
		STRCAT(temp, exe_path);
		vim_setenv((char_u *)"PATH", temp);
	    }
	}
    }
}

/*
 * Unescape characters in "p" that appear in "escaped".
 */
    static void
unescape_shellxquote(char_u *p, char_u *escaped)
{
    int	    l = (int)STRLEN(p);
    int	    n;

    while (*p != NUL)
    {
	if (*p == '^' && vim_strchr(escaped, p[1]) != NULL)
	    mch_memmove(p, p + 1, l--);
#ifdef FEAT_MBYTE
	n = (*mb_ptr2len)(p);
#else
	n = 1;
#endif
	p += n;
	l -= n;
    }
}

/*
 * Load library "name".
 */
    HINSTANCE
vimLoadLib(char *name)
{
    HINSTANCE	dll = NULL;
    char	old_dir[MAXPATHL];

    /* NOTE: Do not use mch_dirname() and mch_chdir() here, they may call
     * vimLoadLib() recursively, which causes a stack overflow. */
    if (exe_path == NULL)
	get_exe_name();
    if (exe_path != NULL)
    {
#ifdef FEAT_MBYTE
	WCHAR old_dirw[MAXPATHL];

	if (GetCurrentDirectoryW(MAXPATHL, old_dirw) != 0)
	{
	    /* Change directory to where the executable is, both to make
	     * sure we find a .dll there and to avoid looking for a .dll
	     * in the current directory. */
	    SetCurrentDirectory(exe_path);
	    dll = LoadLibrary(name);
	    SetCurrentDirectoryW(old_dirw);
	    return dll;
	}
	/* Retry with non-wide function (for Windows 98). */
	if (GetLastError() == ERROR_CALL_NOT_IMPLEMENTED)
#endif
	    if (GetCurrentDirectory(MAXPATHL, old_dir) != 0)
	    {
		/* Change directory to where the executable is, both to make
		 * sure we find a .dll there and to avoid looking for a .dll
		 * in the current directory. */
		SetCurrentDirectory(exe_path);
		dll = LoadLibrary(name);
		SetCurrentDirectory(old_dir);
	    }
    }
    return dll;
}

#if defined(DYNAMIC_GETTEXT) || defined(PROTO)
# ifndef GETTEXT_DLL
#  define GETTEXT_DLL "libintl.dll"
# endif
/* Dummy functions */
static char *null_libintl_gettext(const char *);
static char *null_libintl_textdomain(const char *);
static char *null_libintl_bindtextdomain(const char *, const char *);
static char *null_libintl_bind_textdomain_codeset(const char *, const char *);

static HINSTANCE hLibintlDLL = NULL;
char *(*dyn_libintl_gettext)(const char *) = null_libintl_gettext;
char *(*dyn_libintl_textdomain)(const char *) = null_libintl_textdomain;
char *(*dyn_libintl_bindtextdomain)(const char *, const char *)
						= null_libintl_bindtextdomain;
char *(*dyn_libintl_bind_textdomain_codeset)(const char *, const char *)
				       = null_libintl_bind_textdomain_codeset;

    int
dyn_libintl_init(char *libname)
{
    int i;
    static struct
    {
	char	    *name;
	FARPROC	    *ptr;
    } libintl_entry[] =
    {
	{"gettext", (FARPROC*)&dyn_libintl_gettext},
	{"textdomain", (FARPROC*)&dyn_libintl_textdomain},
	{"bindtextdomain", (FARPROC*)&dyn_libintl_bindtextdomain},
	{NULL, NULL}
    };

    /* No need to initialize twice. */
    if (hLibintlDLL)
	return 1;
    /* Load gettext library (libintl.dll) */
    hLibintlDLL = vimLoadLib(libname != NULL ? libname : GETTEXT_DLL);
    if (!hLibintlDLL)
    {
	if (p_verbose > 0)
	{
	    verbose_enter();
	    EMSG2(_(e_loadlib), GETTEXT_DLL);
	    verbose_leave();
	}
	return 0;
    }
    for (i = 0; libintl_entry[i].name != NULL
					 && libintl_entry[i].ptr != NULL; ++i)
    {
	if ((*libintl_entry[i].ptr = (FARPROC)GetProcAddress(hLibintlDLL,
					      libintl_entry[i].name)) == NULL)
	{
	    dyn_libintl_end();
	    if (p_verbose > 0)
	    {
		verbose_enter();
		EMSG2(_(e_loadfunc), libintl_entry[i].name);
		verbose_leave();
	    }
	    return 0;
	}
    }

    /* The bind_textdomain_codeset() function is optional. */
    dyn_libintl_bind_textdomain_codeset = (void *)GetProcAddress(hLibintlDLL,
						   "bind_textdomain_codeset");
    if (dyn_libintl_bind_textdomain_codeset == NULL)
	dyn_libintl_bind_textdomain_codeset =
					 null_libintl_bind_textdomain_codeset;

    return 1;
}

    void
dyn_libintl_end()
{
    if (hLibintlDLL)
	FreeLibrary(hLibintlDLL);
    hLibintlDLL			= NULL;
    dyn_libintl_gettext		= null_libintl_gettext;
    dyn_libintl_textdomain	= null_libintl_textdomain;
    dyn_libintl_bindtextdomain	= null_libintl_bindtextdomain;
    dyn_libintl_bind_textdomain_codeset = null_libintl_bind_textdomain_codeset;
}

/*ARGSUSED*/
    static char *
null_libintl_gettext(const char *msgid)
{
    return (char*)msgid;
}

/*ARGSUSED*/
    static char *
null_libintl_bindtextdomain(const char *domainname, const char *dirname)
{
    return NULL;
}

/*ARGSUSED*/
    static char *
null_libintl_bind_textdomain_codeset(const char *domainname,
							  const char *codeset)
{
    return NULL;
}

/*ARGSUSED*/
    static char *
null_libintl_textdomain(const char *domainname)
{
    return NULL;
}

#endif /* DYNAMIC_GETTEXT */

/* This symbol is not defined in older versions of the SDK or Visual C++ */

#ifndef VER_PLATFORM_WIN32_WINDOWS
# define VER_PLATFORM_WIN32_WINDOWS 1
#endif

DWORD g_PlatformId;

#ifdef HAVE_ACL
//#error HAVE_ACL
# ifndef PROTO
#  include <aclapi.h>
# endif
# ifndef PROTECTED_DACL_SECURITY_INFORMATION
#  define PROTECTED_DACL_SECURITY_INFORMATION	0x80000000L
# endif

/*
 * These are needed to dynamically load the ADVAPI DLL, which is not
 * implemented under Windows 95 (and causes VIM to crash)
 */
typedef DWORD (WINAPI *PSNSECINFO) (LPSTR, SE_OBJECT_TYPE,
	SECURITY_INFORMATION, PSID, PSID, PACL, PACL);
typedef DWORD (WINAPI *PGNSECINFO) (LPSTR, SE_OBJECT_TYPE,
	SECURITY_INFORMATION, PSID *, PSID *, PACL *, PACL *,
	PSECURITY_DESCRIPTOR *);
# ifdef FEAT_MBYTE
typedef DWORD (WINAPI *PSNSECINFOW) (LPWSTR, SE_OBJECT_TYPE,
	SECURITY_INFORMATION, PSID, PSID, PACL, PACL);
typedef DWORD (WINAPI *PGNSECINFOW) (LPWSTR, SE_OBJECT_TYPE,
	SECURITY_INFORMATION, PSID *, PSID *, PACL *, PACL *,
	PSECURITY_DESCRIPTOR *);
# endif

static HANDLE advapi_lib = NULL;	/* Handle for ADVAPI library */
static PSNSECINFO pSetNamedSecurityInfo;
static PGNSECINFO pGetNamedSecurityInfo;
# ifdef FEAT_MBYTE
static PSNSECINFOW pSetNamedSecurityInfoW;
static PGNSECINFOW pGetNamedSecurityInfoW;
# endif
#endif

typedef BOOL (WINAPI *PSETHANDLEINFORMATION)(HANDLE, DWORD, DWORD);

static BOOL allowPiping = FALSE;
static PSETHANDLEINFORMATION pSetHandleInformation;

#ifdef HAVE_ACL
/*
 * Enables or disables the specified privilege.
 */
    static BOOL
win32_enable_privilege(LPTSTR lpszPrivilege, BOOL bEnable)
{
    BOOL             bResult;
    LUID             luid;
    HANDLE           hToken;
    TOKEN_PRIVILEGES tokenPrivileges;

    if (!OpenProcessToken(GetCurrentProcess(),
		TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY, &hToken))
	return FALSE;

    if (!LookupPrivilegeValue(NULL, lpszPrivilege, &luid))
    {
	CloseHandle(hToken);
	return FALSE;
    }

    tokenPrivileges.PrivilegeCount           = 1;
    tokenPrivileges.Privileges[0].Luid       = luid;
    tokenPrivileges.Privileges[0].Attributes = bEnable ?
						    SE_PRIVILEGE_ENABLED : 0;

    bResult = AdjustTokenPrivileges(hToken, FALSE, &tokenPrivileges,
	    sizeof(TOKEN_PRIVILEGES), NULL, NULL);

    CloseHandle(hToken);

    return bResult && GetLastError() == ERROR_SUCCESS;
}
#endif

/*
 * Set g_PlatformId to VER_PLATFORM_WIN32_NT (NT) or
 * VER_PLATFORM_WIN32_WINDOWS (Win95).
 */
    void
PlatformId(void)
{
    static int done = FALSE;

    if (!done)
    {
	OSVERSIONINFO ovi;

	ovi.dwOSVersionInfoSize = sizeof(ovi);
	GetVersionEx(&ovi);

	g_PlatformId = ovi.dwPlatformId;

#ifdef HAVE_ACL
	/*
	 * Load the ADVAPI runtime if we are on anything
	 * other than Windows 95
	 */
	if (g_PlatformId == VER_PLATFORM_WIN32_NT)
	{
	    /*
	     * do this load.  Problems: Doesn't unload at end of run (this is
	     * theoretically okay, since Windows should unload it when VIM
	     * terminates).  Should we be using the 'mch_libcall' routines?
	     * Seems like a lot of overhead to load/unload ADVAPI32.DLL each
	     * time we verify security...
	     */
	    advapi_lib = vimLoadLib("ADVAPI32.DLL");
	    if (advapi_lib != NULL)
	    {
		pSetNamedSecurityInfo = (PSNSECINFO)GetProcAddress(advapi_lib,
						      "SetNamedSecurityInfoA");
		pGetNamedSecurityInfo = (PGNSECINFO)GetProcAddress(advapi_lib,
						      "GetNamedSecurityInfoA");
# ifdef FEAT_MBYTE
		pSetNamedSecurityInfoW = (PSNSECINFOW)GetProcAddress(advapi_lib,
						      "SetNamedSecurityInfoW");
		pGetNamedSecurityInfoW = (PGNSECINFOW)GetProcAddress(advapi_lib,
						      "GetNamedSecurityInfoW");
# endif
		if (pSetNamedSecurityInfo == NULL
			|| pGetNamedSecurityInfo == NULL
# ifdef FEAT_MBYTE
			|| pSetNamedSecurityInfoW == NULL
			|| pGetNamedSecurityInfoW == NULL
# endif
			)
		{
		    /* If we can't get the function addresses, set advapi_lib
		     * to NULL so that we don't use them. */
		    FreeLibrary(advapi_lib);
		    advapi_lib = NULL;
		}
		/* Enable privilege for getting or setting SACLs. */
		win32_enable_privilege(SE_SECURITY_NAME, TRUE);
	    }
	}
#endif
	/*
	 * If we are on windows NT, try to load the pipe functions, only
	 * available from Win2K.
	 */
	if (g_PlatformId == VER_PLATFORM_WIN32_NT)
	{
	    HANDLE kernel32 = GetModuleHandle("kernel32");
	    pSetHandleInformation = (PSETHANDLEINFORMATION)GetProcAddress(
					    kernel32, "SetHandleInformation");

	    allowPiping = pSetHandleInformation != NULL;
	}
	done = TRUE;
    }
}

/*
 * Return TRUE when running on Windows 95 (or 98 or ME).
 * Only to be used after mch_init().
 */
    int
mch_windows95(void)
{
    return g_PlatformId == VER_PLATFORM_WIN32_WINDOWS;
}

#ifdef FEAT_GUI_W32
/*
 * Used to work around the "can't do synchronous spawn"
 * problem on Win32s, without resorting to Universal Thunk.
 */
static int old_num_windows;
static int num_windows;

/*ARGSUSED*/
    static BOOL CALLBACK
win32ssynch_cb(HWND hwnd, LPARAM lparam)
{
    num_windows++;
    return TRUE;
}
#endif



#ifdef FEAT_MOUSE
//#error FEAT_MOUSE
/*
 * For the GUI the mouse handling is in gui_w32.c.
 */
/*ARGSUSED*/
    void
mch_setmouse(int on)
{
}
#endif /* FEAT_MOUSE */




/*
 * mch_inchar(): low-level input function.
 * Get one or more characters from the keyboard or the mouse.
 * If time == 0, do not wait for characters.
 * If time == n, wait a short time for characters.
 * If time == -1, wait forever for characters.
 * Returns the number of characters read into buf.
 */
/*ARGSUSED*/
    int
mch_inchar(
    char_u	*buf,
    int		maxlen,
    long	time,
    int		tb_change_cnt)
{
    return 0;
}

#ifndef PROTO
# ifndef __MINGW32__
#  include <shellapi.h>	/* required for FindExecutable() */
# endif
#endif

/*
 * Return TRUE if "name" is in $PATH.
 * TODO: Should somehow check if it's really executable.
 */
    static int
executable_exists(char *name)
{
    char	*dum;
    char	fname[_MAX_PATH];

#ifdef FEAT_MBYTE
    if (enc_codepage >= 0 && (int)GetACP() != enc_codepage)
    {
	WCHAR	*p = enc_to_utf16(name, NULL);
	WCHAR	fnamew[_MAX_PATH];
	WCHAR	*dumw;
	long	n;

	if (p != NULL)
	{
	    n = (long)SearchPathW(NULL, p, NULL, _MAX_PATH, fnamew, &dumw);
	    vim_free(p);
	    if (n > 0 || GetLastError() != ERROR_CALL_NOT_IMPLEMENTED)
	    {
		if (n == 0)
		    return FALSE;
		if (GetFileAttributesW(fnamew) & FILE_ATTRIBUTE_DIRECTORY)
		    return FALSE;
		return TRUE;
	    }
	    /* Retry with non-wide function (for Windows 98). */
	}
    }
#endif
    if (SearchPath(NULL, name, NULL, _MAX_PATH, fname, &dum) == 0)
	return FALSE;
    if (mch_isdir(fname))
	return FALSE;
    return TRUE;
}

# define SET_INVALID_PARAM_HANDLER


/*
 * GUI version of mch_init().
 */
    void
mch_init(void)
{
#ifndef __MINGW32__
    extern int _fmode;
#endif

    /* Silently handle invalid parameters to CRT functions */
    SET_INVALID_PARAM_HANDLER;

    /* Let critical errors result in a failure, not in a dialog box.  Required
     * for the timestamp test to work on removed floppies. */
    SetErrorMode(SEM_FAILCRITICALERRORS);

    _fmode = O_BINARY;		/* we do our own CR-LF translation */

    /* Specify window size.  Is there a place to get the default from? */
    Rows = 25;
    Columns = 80;

    /* Look for 'vimrun' */
    if (!gui_is_win32s())
    {
	char_u vimrun_location[_MAX_PATH + 4];

	/* First try in same directory as gvim.exe */
	STRCPY(vimrun_location, exe_name);
	STRCPY(gettail(vimrun_location), "vimrun.exe");
	if (mch_getperm(vimrun_location) >= 0)
	{
	    if (*skiptowhite(vimrun_location) != NUL)
	    {
		/* Enclose path with white space in double quotes. */
		mch_memmove(vimrun_location + 1, vimrun_location,
						 STRLEN(vimrun_location) + 1);
		*vimrun_location = '"';
		STRCPY(gettail(vimrun_location), "vimrun\" ");
	    }
	    else
		STRCPY(gettail(vimrun_location), "vimrun ");

	    vimrun_path = (char *)vim_strsave(vimrun_location);
	    s_dont_use_vimrun = FALSE;
	}
	else if (executable_exists("vimrun.exe"))
	    s_dont_use_vimrun = FALSE;

	/* Don't give the warning for a missing vimrun.exe right now, but only
	 * when vimrun was supposed to be used.  Don't bother people that do
	 * not need vimrun.exe. */
	if (s_dont_use_vimrun)
	    need_vimrun_warning = TRUE;
    }

    /*
     * If "finstr.exe" doesn't exist, use "grep -n" for 'grepprg'.
     * Otherwise the default "findstr /n" is used.
     */
    if (!executable_exists("findstr.exe"))
	set_option_value((char_u *)"grepprg", 0, (char_u *)"grep -n", 0);

#ifdef FEAT_CLIPBOARD
    win_clip_init();
#endif
}



/*
 * Do we have an interactive window?
 */
/*ARGSUSED*/
    int
mch_check_win(
    int argc,
    char **argv)
{
    get_exe_name();

    return OK;	    /* GUI always has a tty */
}


/*
 * fname_case(): Set the case of the file name, if it already exists.
 * When "len" is > 0, also expand short to long filenames.
 */
    void
fname_case(
    char_u	*name,
    int		len)
{
    char		szTrueName[_MAX_PATH + 2];
    char		szTrueNameTemp[_MAX_PATH + 2];
    char		*ptrue, *ptruePrev;
    char		*porig, *porigPrev;
    int			flen;
    WIN32_FIND_DATA	fb;
    HANDLE		hFind;
    int			c;
    int			slen;

    flen = (int)STRLEN(name);
    if (flen == 0 || flen > _MAX_PATH)
	return;

    slash_adjust(name);

    /* Build the new name in szTrueName[] one component at a time. */
    porig = name;
    ptrue = szTrueName;

    if (isalpha(porig[0]) && porig[1] == ':')
    {
	/* copy leading drive letter */
	*ptrue++ = *porig++;
	*ptrue++ = *porig++;
	*ptrue = NUL;	    /* in case nothing follows */
    }

    while (*porig != NUL)
    {
	/* copy \ characters */
	while (*porig == psepc)
	    *ptrue++ = *porig++;

	ptruePrev = ptrue;
	porigPrev = porig;
	while (*porig != NUL && *porig != psepc)
	{
#ifdef FEAT_MBYTE
	    int l;

	    if (enc_dbcs)
	    {
		l = (*mb_ptr2len)(porig);
		while (--l >= 0)
		    *ptrue++ = *porig++;
	    }
	    else
#endif
		*ptrue++ = *porig++;
	}
	*ptrue = NUL;

	/* To avoid a slow failure append "\*" when searching a directory,
	 * server or network share. */
	STRCPY(szTrueNameTemp, szTrueName);
	slen = (int)strlen(szTrueNameTemp);
	if (*porig == psepc && slen + 2 < _MAX_PATH)
	    STRCPY(szTrueNameTemp + slen, "\\*");

	/* Skip "", "." and "..". */
	if (ptrue > ptruePrev
		&& (ptruePrev[0] != '.'
		    || (ptruePrev[1] != NUL
			&& (ptruePrev[1] != '.' || ptruePrev[2] != NUL)))
		&& (hFind = FindFirstFile(szTrueNameTemp, &fb))
						      != INVALID_HANDLE_VALUE)
	{
	    c = *porig;
	    *porig = NUL;

	    /* Only use the match when it's the same name (ignoring case) or
	     * expansion is allowed and there is a match with the short name
	     * and there is enough room. */
	    if (_stricoll(porigPrev, fb.cFileName) == 0
		    || (len > 0
			&& (_stricoll(porigPrev, fb.cAlternateFileName) == 0
			    && (int)(ptruePrev - szTrueName)
					   + (int)strlen(fb.cFileName) < len)))
	    {
		STRCPY(ptruePrev, fb.cFileName);

		/* Look for exact match and prefer it if found.  Must be a
		 * long name, otherwise there would be only one match. */
		while (FindNextFile(hFind, &fb))
		{
		    if (*fb.cAlternateFileName != NUL
			    && (strcoll(porigPrev, fb.cFileName) == 0
				|| (len > 0
				    && (_stricoll(porigPrev,
						   fb.cAlternateFileName) == 0
				    && (int)(ptruePrev - szTrueName)
					 + (int)strlen(fb.cFileName) < len))))
		    {
			STRCPY(ptruePrev, fb.cFileName);
			break;
		    }
		}
	    }
	    FindClose(hFind);
	    *porig = c;
	    ptrue = ptruePrev + strlen(ptruePrev);
	}
    }

    STRCPY(name, szTrueName);
}


/*
 * Insert user name in s[len].
 */
    int
mch_get_user_name(
    char_u  *s,
    int	    len)
{
    char szUserName[256 + 1];	/* UNLEN is 256 */
    DWORD cch = sizeof szUserName;

    if (GetUserName(szUserName, &cch))
    {
	vim_strncpy(s, szUserName, len - 1);
	return OK;
    }
    s[0] = NUL;
    return FAIL;
}


/*
 * Insert host name in s[len].
 */
    void
mch_get_host_name(
    char_u	*s,
    int		len)
{
    DWORD cch = len;

    if (!GetComputerName(s, &cch))
	vim_strncpy(s, "PC (Win32 Vim)", len - 1);
}


/*
 * return process ID
 */
    long
mch_get_pid(void)
{
    return (long)GetCurrentProcessId();
}


/*
 * Get name of current directory into buffer 'buf' of length 'len' bytes.
 * Return OK for success, FAIL for failure.
 */
    int
mch_dirname(
    char_u	*buf,
    int		len)
{
    /*
     * Originally this was:
     *    return (getcwd(buf, len) != NULL ? OK : FAIL);
     * But the Win32s known bug list says that getcwd() doesn't work
     * so use the Win32 system call instead. <Negri>
     */
#ifdef FEAT_MBYTE
    if (enc_codepage >= 0 && (int)GetACP() != enc_codepage)
    {
	WCHAR	wbuf[_MAX_PATH + 1];

	if (GetCurrentDirectoryW(_MAX_PATH, wbuf) != 0)
	{
	    char_u  *p = utf16_to_enc(wbuf, NULL);

	    if (p != NULL)
	    {
		vim_strncpy(buf, p, len - 1);
		vim_free(p);
		return OK;
	    }
	}
	/* Retry with non-wide function (for Windows 98). */
    }
#endif
    return (GetCurrentDirectory(len, buf) != 0 ? OK : FAIL);
}

/*
 * get file permissions for `name'
 * -1 : error
 * else mode_t
 */
    long
mch_getperm(char_u *name)
{
    struct stat st;
    int n;

    n = mch_stat(name, &st);
    return n == 0 ? (int)st.st_mode : -1;
}


/*
 * Set file permission for "name" to "perm".
 *
 * Return FAIL for failure, OK otherwise.
 */
    int
mch_setperm(char_u *name, long perm)
{
    long	n = -1;

#ifdef FEAT_MBYTE
    if (enc_codepage >= 0 && (int)GetACP() != enc_codepage)
    {
	WCHAR *p = enc_to_utf16(name, NULL);

	if (p != NULL)
	{
	    n = _wchmod(p, perm);
	    vim_free(p);
	    if (n == -1 && GetLastError() != ERROR_CALL_NOT_IMPLEMENTED)
		return FAIL;
	    /* Retry with non-wide function (for Windows 98). */
	}
    }
    if (n == -1)
#endif
	n = _chmod(name, perm);
    if (n == -1)
	return FAIL;

    win32_set_archive(name);

    return OK;
}

/*
 * Set hidden flag for "name".
 */
    void
mch_hide(char_u *name)
{
    int attrs = win32_getattrs(name);
    if (attrs == -1)
	return;

    attrs |= FILE_ATTRIBUTE_HIDDEN;
    win32_setattrs(name, attrs);
}

/*
 * return TRUE if "name" is a directory
 * return FALSE if "name" is not a directory or upon error
 */
    int
mch_isdir(char_u *name)
{
    int f = win32_getattrs(name);

    if (f == -1)
	return FALSE;		    /* file does not exist at all */

    return (f & FILE_ATTRIBUTE_DIRECTORY) != 0;
}

/*
 * Create directory "name".
 * Return 0 on success, -1 on error.
 */
    int
mch_mkdir(char_u *name)
{
#ifdef FEAT_MBYTE
    if (enc_codepage >= 0 && (int)GetACP() != enc_codepage)
    {
	WCHAR	*p;
	int	retval;

	p = enc_to_utf16(name, NULL);
	if (p == NULL)
	    return -1;
	retval = _wmkdir(p);
	vim_free(p);
	return retval;
    }
#endif
    return _mkdir(name);
}

/*
 * Return TRUE if file "fname" has more than one link.
 */
    int
mch_is_hard_link(char_u *fname)
{
    BY_HANDLE_FILE_INFORMATION info;

    return win32_fileinfo(fname, &info) == FILEINFO_OK
						   && info.nNumberOfLinks > 1;
}

/*
 * Return TRUE if file "fname" is a symbolic link.
 */
    int
mch_is_symbolic_link(char_u *fname)
{
    HANDLE		hFind;
    int			res = FALSE;
    WIN32_FIND_DATAA	findDataA;
    DWORD		fileFlags = 0, reparseTag = 0;
#ifdef FEAT_MBYTE
    WCHAR		*wn = NULL;
    WIN32_FIND_DATAW	findDataW;

    if (enc_codepage >= 0 && (int)GetACP() != enc_codepage)
	wn = enc_to_utf16(fname, NULL);
    if (wn != NULL)
    {
	hFind = FindFirstFileW(wn, &findDataW);
	vim_free(wn);
	if (hFind == INVALID_HANDLE_VALUE
		&& GetLastError() == ERROR_CALL_NOT_IMPLEMENTED)
	{
	    /* Retry with non-wide function (for Windows 98). */
	    hFind = FindFirstFile(fname, &findDataA);
	    if (hFind != INVALID_HANDLE_VALUE)
	    {
		fileFlags = findDataA.dwFileAttributes;
		reparseTag = findDataA.dwReserved0;
	    }
	}
	else
	{
	    fileFlags = findDataW.dwFileAttributes;
	    reparseTag = findDataW.dwReserved0;
	}
    }
    else
#endif
    {
	hFind = FindFirstFile(fname, &findDataA);
	if (hFind != INVALID_HANDLE_VALUE)
	{
	    fileFlags = findDataA.dwFileAttributes;
	    reparseTag = findDataA.dwReserved0;
	}
    }

    if (hFind != INVALID_HANDLE_VALUE)
	FindClose(hFind);

    if ((fileFlags & FILE_ATTRIBUTE_REPARSE_POINT)
	    && reparseTag == IO_REPARSE_TAG_SYMLINK)
	res = TRUE;

    return res;
}

/*
 * Return TRUE if file "fname" has more than one link or if it is a symbolic
 * link.
 */
    int
mch_is_linked(char_u *fname)
{
    if (mch_is_hard_link(fname) || mch_is_symbolic_link(fname))
	return TRUE;
    return FALSE;
}

/*
 * Get the by-handle-file-information for "fname".
 * Returns FILEINFO_OK when OK.
 * returns FILEINFO_ENC_FAIL when enc_to_utf16() failed.
 * Returns FILEINFO_READ_FAIL when CreateFile() failed.
 * Returns FILEINFO_INFO_FAIL when GetFileInformationByHandle() failed.
 */
    int
win32_fileinfo(char_u *fname, BY_HANDLE_FILE_INFORMATION *info)
{
    HANDLE	hFile;
    int		res = FILEINFO_READ_FAIL;
#ifdef FEAT_MBYTE
    WCHAR	*wn = NULL;

    if (enc_codepage >= 0 && (int)GetACP() != enc_codepage)
    {
	wn = enc_to_utf16(fname, NULL);
	if (wn == NULL)
	    res = FILEINFO_ENC_FAIL;
    }
    if (wn != NULL)
    {
	hFile = CreateFileW(wn,		/* file name */
		    GENERIC_READ,	/* access mode */
		    FILE_SHARE_READ | FILE_SHARE_WRITE,	/* share mode */
		    NULL,		/* security descriptor */
		    OPEN_EXISTING,	/* creation disposition */
		    FILE_FLAG_BACKUP_SEMANTICS,	/* file attributes */
		    NULL);		/* handle to template file */
	if (hFile == INVALID_HANDLE_VALUE
			      && GetLastError() == ERROR_CALL_NOT_IMPLEMENTED)
	{
	    /* Retry with non-wide function (for Windows 98). */
	    vim_free(wn);
	    wn = NULL;
	}
    }
    if (wn == NULL)
#endif
	hFile = CreateFile(fname,	/* file name */
		    GENERIC_READ,	/* access mode */
		    FILE_SHARE_READ | FILE_SHARE_WRITE,	/* share mode */
		    NULL,		/* security descriptor */
		    OPEN_EXISTING,	/* creation disposition */
		    FILE_FLAG_BACKUP_SEMANTICS,	/* file attributes */
		    NULL);		/* handle to template file */

    if (hFile != INVALID_HANDLE_VALUE)
    {
	if (GetFileInformationByHandle(hFile, info) != 0)
	    res = FILEINFO_OK;
	else
	    res = FILEINFO_INFO_FAIL;
	CloseHandle(hFile);
    }

#ifdef FEAT_MBYTE
    vim_free(wn);
#endif
    return res;
}

/*
 * get file attributes for `name'
 * -1 : error
 * else FILE_ATTRIBUTE_* defined in winnt.h
 */
    static
    int
win32_getattrs(char_u *name)
{
    int		attr;
#ifdef FEAT_MBYTE
    WCHAR	*p = NULL;

    if (enc_codepage >= 0 && (int)GetACP() != enc_codepage)
	p = enc_to_utf16(name, NULL);

    if (p != NULL)
    {
	attr = GetFileAttributesW(p);
	if (attr < 0 && GetLastError() == ERROR_CALL_NOT_IMPLEMENTED)
	{
	    /* Retry with non-wide function (for Windows 98). */
	    vim_free(p);
	    p = NULL;
	}
    }
    if (p == NULL)
#endif
	attr = GetFileAttributes((char *)name);
#ifdef FEAT_MBYTE
    vim_free(p);
#endif
    return attr;
}

/*
 * set file attributes for `name' to `attrs'
 *
 * return -1 for failure, 0 otherwise
 */
    static
    int
win32_setattrs(char_u *name, int attrs)
{
    int res;
#ifdef FEAT_MBYTE
    WCHAR	*p = NULL;

    if (enc_codepage >= 0 && (int)GetACP() != enc_codepage)
	p = enc_to_utf16(name, NULL);

    if (p != NULL)
    {
	res = SetFileAttributesW(p, attrs);
	if (res == FALSE
	    && GetLastError() == ERROR_CALL_NOT_IMPLEMENTED)
	{
	    /* Retry with non-wide function (for Windows 98). */
	    vim_free(p);
	    p = NULL;
	}
    }
    if (p == NULL)
#endif
	res = SetFileAttributes((char *)name, attrs);
#ifdef FEAT_MBYTE
    vim_free(p);
#endif
    return res ? 0 : -1;
}

/*
 * Set archive flag for "name".
 */
    static
    int
win32_set_archive(char_u *name)
{
    int attrs = win32_getattrs(name);
    if (attrs == -1)
	return -1;

    attrs |= FILE_ATTRIBUTE_ARCHIVE;
    return win32_setattrs(name, attrs);
}

/*
 * Return TRUE if file or directory "name" is writable (not readonly).
 * Strange semantics of Win32: a readonly directory is writable, but you can't
 * delete a file.  Let's say this means it is writable.
 */
    int
mch_writable(char_u *name)
{
    int attrs = win32_getattrs(name);

    return (attrs != -1 && (!(attrs & FILE_ATTRIBUTE_READONLY)
			  || (attrs & FILE_ATTRIBUTE_DIRECTORY)));
}

/*
 * Return 1 if "name" can be executed, 0 if not.
 * Return -1 if unknown.
 */
    int
mch_can_exe(char_u *name)
{
    char_u	buf[_MAX_PATH];
    int		len = (int)STRLEN(name);
    char_u	*p;

    if (len >= _MAX_PATH)	/* safety check */
	return FALSE;

    /* If there already is an extension try using the name directly.  Also do
     * this with a Unix-shell like 'shell'. */
    if (vim_strchr(gettail(name), '.') != NULL
			       || strstr((char *)gettail(p_sh), "sh") != NULL)
	if (executable_exists((char *)name))
	    return TRUE;

    /*
     * Loop over all extensions in $PATHEXT.
     */
    vim_strncpy(buf, name, _MAX_PATH - 1);
    p = mch_getenv("PATHEXT");
    if (p == NULL)
	p = (char_u *)".com;.exe;.bat;.cmd";
    while (*p)
    {
	if (p[0] == '.' && (p[1] == NUL || p[1] == ';'))
	{
	    /* A single "." means no extension is added. */
	    buf[len] = NUL;
	    ++p;
	    if (*p)
		++p;
	}
	else
	    copy_option_part(&p, buf + len, _MAX_PATH - len, ";");
	if (executable_exists((char *)buf))
	    return TRUE;
    }
    return FALSE;
}

/*
 * Check what "name" is:
 * NODE_NORMAL: file or directory (or doesn't exist)
 * NODE_WRITABLE: writable device, socket, fifo, etc.
 * NODE_OTHER: non-writable things
 */
    int
mch_nodetype(char_u *name)
{
    HANDLE	hFile;
    int		type;

    /* We can't open a file with a name "\\.\con" or "\\.\prn" and trying to
     * read from it later will cause Vim to hang.  Thus return NODE_WRITABLE
     * here. */
    if (STRNCMP(name, "\\\\.\\", 4) == 0)
	return NODE_WRITABLE;

    hFile = CreateFile(name,		/* file name */
		GENERIC_WRITE,		/* access mode */
		0,			/* share mode */
		NULL,			/* security descriptor */
		OPEN_EXISTING,		/* creation disposition */
		0,			/* file attributes */
		NULL);			/* handle to template file */

    if (hFile == INVALID_HANDLE_VALUE)
	return NODE_NORMAL;

    type = GetFileType(hFile);
    CloseHandle(hFile);
    if (type == FILE_TYPE_CHAR)
	return NODE_WRITABLE;
    if (type == FILE_TYPE_DISK)
	return NODE_NORMAL;
    return NODE_OTHER;
}

#ifdef HAVE_ACL
struct my_acl
{
    PSECURITY_DESCRIPTOR    pSecurityDescriptor;
    PSID		    pSidOwner;
    PSID		    pSidGroup;
    PACL		    pDacl;
    PACL		    pSacl;
};
#endif

/*
 * Return a pointer to the ACL of file "fname" in allocated memory.
 * Return NULL if the ACL is not available for whatever reason.
 */
    vim_acl_T
mch_get_acl(char_u *fname)
{
#ifndef HAVE_ACL
    return (vim_acl_T)NULL;
#else
    struct my_acl   *p = NULL;
    DWORD   err;

    /* This only works on Windows NT and 2000. */
    if (g_PlatformId == VER_PLATFORM_WIN32_NT && advapi_lib != NULL)
    {
	p = (struct my_acl *)alloc_clear((unsigned)sizeof(struct my_acl));
	if (p != NULL)
	{
# ifdef FEAT_MBYTE
	    WCHAR	*wn = NULL;

	    if (enc_codepage >= 0 && (int)GetACP() != enc_codepage)
		wn = enc_to_utf16(fname, NULL);
	    if (wn != NULL)
	    {
		/* Try to retrieve the entire security descriptor. */
		err = pGetNamedSecurityInfoW(
			    wn,			// Abstract filename
			    SE_FILE_OBJECT,	// File Object
			    OWNER_SECURITY_INFORMATION |
			    GROUP_SECURITY_INFORMATION |
			    DACL_SECURITY_INFORMATION |
			    SACL_SECURITY_INFORMATION,
			    &p->pSidOwner,	// Ownership information.
			    &p->pSidGroup,	// Group membership.
			    &p->pDacl,		// Discretionary information.
			    &p->pSacl,		// For auditing purposes.
			    &p->pSecurityDescriptor);
		if (err == ERROR_ACCESS_DENIED ||
			err == ERROR_PRIVILEGE_NOT_HELD)
		{
		    /* Retrieve only DACL. */
		    (void)pGetNamedSecurityInfoW(
			    wn,
			    SE_FILE_OBJECT,
			    DACL_SECURITY_INFORMATION,
			    NULL,
			    NULL,
			    &p->pDacl,
			    NULL,
			    &p->pSecurityDescriptor);
		}
		if (p->pSecurityDescriptor == NULL)
		{
		    mch_free_acl((vim_acl_T)p);
		    p = NULL;
		}
		vim_free(wn);
	    }
	    else
# endif
	    {
		/* Try to retrieve the entire security descriptor. */
		err = pGetNamedSecurityInfo(
			    (LPSTR)fname,	// Abstract filename
			    SE_FILE_OBJECT,	// File Object
			    OWNER_SECURITY_INFORMATION |
			    GROUP_SECURITY_INFORMATION |
			    DACL_SECURITY_INFORMATION |
			    SACL_SECURITY_INFORMATION,
			    &p->pSidOwner,	// Ownership information.
			    &p->pSidGroup,	// Group membership.
			    &p->pDacl,		// Discretionary information.
			    &p->pSacl,		// For auditing purposes.
			    &p->pSecurityDescriptor);
		if (err == ERROR_ACCESS_DENIED ||
			err == ERROR_PRIVILEGE_NOT_HELD)
		{
		    /* Retrieve only DACL. */
		    (void)pGetNamedSecurityInfo(
			    (LPSTR)fname,
			    SE_FILE_OBJECT,
			    DACL_SECURITY_INFORMATION,
			    NULL,
			    NULL,
			    &p->pDacl,
			    NULL,
			    &p->pSecurityDescriptor);
		}
		if (p->pSecurityDescriptor == NULL)
		{
		    mch_free_acl((vim_acl_T)p);
		    p = NULL;
		}
	    }
	}
    }

    return (vim_acl_T)p;
#endif
}

#ifdef HAVE_ACL
/*
 * Check if "acl" contains inherited ACE.
 */
    static BOOL
is_acl_inherited(PACL acl)
{
    DWORD   i;
    ACL_SIZE_INFORMATION    acl_info;
    PACCESS_ALLOWED_ACE	    ace;

    acl_info.AceCount = 0;
    GetAclInformation(acl, &acl_info, sizeof(acl_info), AclSizeInformation);
    for (i = 0; i < acl_info.AceCount; i++)
    {
	GetAce(acl, i, (LPVOID *)&ace);
	if (ace->Header.AceFlags & INHERITED_ACE)
	    return TRUE;
    }
    return FALSE;
}
#endif

/*
 * Set the ACL of file "fname" to "acl" (unless it's NULL).
 * Errors are ignored.
 * This must only be called with "acl" equal to what mch_get_acl() returned.
 */
    void
mch_set_acl(char_u *fname, vim_acl_T acl)
{
#ifdef HAVE_ACL
    struct my_acl   *p = (struct my_acl *)acl;
    SECURITY_INFORMATION    sec_info = 0;

    if (p != NULL && advapi_lib != NULL)
    {
# ifdef FEAT_MBYTE
	WCHAR	*wn = NULL;
# endif

	/* Set security flags */
	if (p->pSidOwner)
	    sec_info |= OWNER_SECURITY_INFORMATION;
	if (p->pSidGroup)
	    sec_info |= GROUP_SECURITY_INFORMATION;
	if (p->pDacl)
	{
	    sec_info |= DACL_SECURITY_INFORMATION;
	    /* Do not inherit its parent's DACL.
	     * If the DACL is inherited, Cygwin permissions would be changed.
	     */
	    if (!is_acl_inherited(p->pDacl))
		sec_info |= PROTECTED_DACL_SECURITY_INFORMATION;
	}
	if (p->pSacl)
	    sec_info |= SACL_SECURITY_INFORMATION;

# ifdef FEAT_MBYTE
	if (enc_codepage >= 0 && (int)GetACP() != enc_codepage)
	    wn = enc_to_utf16(fname, NULL);
	if (wn != NULL)
	{
	    (void)pSetNamedSecurityInfoW(
			wn,			// Abstract filename
			SE_FILE_OBJECT,		// File Object
			sec_info,
			p->pSidOwner,		// Ownership information.
			p->pSidGroup,		// Group membership.
			p->pDacl,		// Discretionary information.
			p->pSacl		// For auditing purposes.
			);
	    vim_free(wn);
	}
	else
# endif
	{
	    (void)pSetNamedSecurityInfo(
			(LPSTR)fname,		// Abstract filename
			SE_FILE_OBJECT,		// File Object
			sec_info,
			p->pSidOwner,		// Ownership information.
			p->pSidGroup,		// Group membership.
			p->pDacl,		// Discretionary information.
			p->pSacl		// For auditing purposes.
			);
	}
    }
#endif
}

    void
mch_free_acl(vim_acl_T acl)
{
#ifdef HAVE_ACL
    struct my_acl   *p = (struct my_acl *)acl;

    if (p != NULL)
    {
	LocalFree(p->pSecurityDescriptor);	// Free the memory just in case
	vim_free(p);
    }
#endif
}




#if defined(FEAT_GUI_W32) || defined(PROTO)

/*
 * Specialised version of system() for Win32 GUI mode.
 * This version proceeds as follows:
 *    1. Create a console window for use by the subprocess
 *    2. Run the subprocess (it gets the allocated console by default)
 *    3. Wait for the subprocess to terminate and get its exit code
 *    4. Prompt the user to press a key to close the console window
 */
// FUNC_WAIT FIXME
    static int
mch_system_classic(char *cmd, int options)
{
    STARTUPINFO		si;
    PROCESS_INFORMATION pi;
    DWORD		ret = 0;
    HWND		hwnd = GetFocus();

    si.cb = sizeof(si);
    si.lpReserved = NULL;
    si.lpDesktop = NULL;
    si.lpTitle = NULL;
    si.dwFlags = STARTF_USESHOWWINDOW;
    /*
     * It's nicer to run a filter command in a minimized window, but in
     * Windows 95 this makes the command MUCH slower.  We can't do it under
     * Win32s either as it stops the synchronous spawn workaround working.
     * Don't activate the window to keep focus on Vim.
     */
    if ((options & SHELL_DOOUT) && !mch_windows95() && !gui_is_win32s())
	si.wShowWindow = SW_SHOWMINNOACTIVE;
    else
	si.wShowWindow = SW_SHOWNORMAL;
    si.cbReserved2 = 0;
    si.lpReserved2 = NULL;

    /* There is a strange error on Windows 95 when using "c:\\command.com".
     * When the "c:\\" is left out it works OK...? */
    if (mch_windows95()
	    && (STRNICMP(cmd, "c:/command.com", 14) == 0
		|| STRNICMP(cmd, "c:\\command.com", 14) == 0))
	cmd += 3;

    /* Now, run the command */
    CreateProcess(NULL,			/* Executable name */
		  cmd,			/* Command to execute */
		  NULL,			/* Process security attributes */
		  NULL,			/* Thread security attributes */
		  FALSE,		/* Inherit handles */
		  CREATE_DEFAULT_ERROR_MODE |	/* Creation flags */
			CREATE_NEW_CONSOLE,
		  NULL,			/* Environment */
		  NULL,			/* Current directory */
		  &si,			/* Startup information */
		  &pi);			/* Process information */


    /* Wait for the command to terminate before continuing */
    if (g_PlatformId != VER_PLATFORM_WIN32s)
    {
#ifdef FEAT_GUI
//#error gui
	int	    delay = 1;

	/* Keep updating the window while waiting for the shell to finish. */
	for (;;)
	{
	    MSG	msg;

	    if (pPeekMessage(&msg, (HWND)NULL, 0, 0, PM_REMOVE))
	    {
		TranslateMessage(&msg);
		pDispatchMessage(&msg);
		delay = 1;
		continue;
	    }
            // MYWAIT FIXME
	    if (WaitForSingleObject(pi.hProcess, delay) != WAIT_TIMEOUT)
		break;

	    /* We start waiting for a very short time and then increase it, so
	     * that we respond quickly when the process is quick, and don't
	     * consume too much overhead when it's slow. */
	    if (delay < 50)
		delay += 10;
	}
#else
        //MYWAIT FIXME
	WaitForSingleObject(pi.hProcess, INFINITE);
#endif

	/* Get the command exit code */
	GetExitCodeProcess(pi.hProcess, &ret);
    }
    else
    {
	/*
	 * This ugly code is the only quick way of performing
	 * a synchronous spawn under Win32s. Yuk.
	 */
	num_windows = 0;
	EnumWindows(win32ssynch_cb, 0);
	old_num_windows = num_windows;
	do
	{
	    Sleep(1000);
	    num_windows = 0;
	    EnumWindows(win32ssynch_cb, 0);
	} while (num_windows == old_num_windows);
	ret = 0;
    }

    /* Close the handles to the subprocess, so that it goes away */
    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);

    /* Try to get input focus back.  Doesn't always work though. */
    PostMessage(hwnd, WM_SETFOCUS, 0, 0);

    return ret;
}

/*
 * Thread launched by the gui to send the current buffer data to the
 * process. This way avoid to hang up vim totally if the children
 * process take a long time to process the lines.
 */
    static DWORD WINAPI
sub_process_writer(LPVOID param)
{
    HANDLE	    g_hChildStd_IN_Wr = param;
    linenr_T	    lnum = curbuf->b_op_start.lnum;
    DWORD	    len = 0;
    DWORD	    l;
    char_u	    *lp = ml_get(lnum);
    char_u	    *s;
    int		    written = 0;

    for (;;)
    {
	l = (DWORD)STRLEN(lp + written);
	if (l == 0)
	    len = 0;
	else if (lp[written] == NL)
	{
	    /* NL -> NUL translation */
	    WriteFile(g_hChildStd_IN_Wr, "", 1, &len, NULL);
	}
	else
	{
	    s = vim_strchr(lp + written, NL);
	    WriteFile(g_hChildStd_IN_Wr, (char *)lp + written,
		      s == NULL ? l : (DWORD)(s - (lp + written)),
		      &len, NULL);
	}
	if (len == (int)l)
	{
	    /* Finished a line, add a NL, unless this line should not have
	     * one. */
	    if (lnum != curbuf->b_op_end.lnum
		|| !curbuf->b_p_bin
		|| (lnum != curbuf->b_no_eol_lnum
		    && (lnum != curbuf->b_ml.ml_line_count
			|| curbuf->b_p_eol)))
	    {
		WriteFile(g_hChildStd_IN_Wr, "\n", 1, (LPDWORD)&ignored, NULL);
	    }

	    ++lnum;
	    if (lnum > curbuf->b_op_end.lnum)
		break;

	    lp = ml_get(lnum);
	    written = 0;
	}
	else if (len > 0)
	    written += len;
    }

    /* finished all the lines, close pipe */
    CloseHandle(g_hChildStd_IN_Wr);
    ExitThread(0);
}


# define BUFLEN 100	/* length for buffer, stolen from unix version */

/*
 * This function read from the children's stdout and write the
 * data on screen or in the buffer accordingly.
 */
    static void
dump_pipe(int	    options,
	  HANDLE    g_hChildStd_OUT_Rd,
	  garray_T  *ga,
	  char_u    buffer[],
	  DWORD	    *buffer_off)
{
    DWORD	availableBytes = 0;
    DWORD	i;
    int		ret;
    DWORD	len;
    DWORD	toRead;
    int		repeatCount;

    /* we query the pipe to see if there is any data to read
     * to avoid to perform a blocking read */
    ret = PeekNamedPipe(g_hChildStd_OUT_Rd, /* pipe to query */
			NULL,		    /* optional buffer */
			0,		    /* buffer size */
			NULL,		    /* number of read bytes */
			&availableBytes,    /* available bytes total */
			NULL);		    /* byteLeft */

    repeatCount = 0;
    /* We got real data in the pipe, read it */
    while (ret != 0 && availableBytes > 0)
    {
	repeatCount++;
	toRead =
# ifdef FEAT_MBYTE
		 (DWORD)(BUFLEN - *buffer_off);
# else
		 (DWORD)BUFLEN;
# endif
	toRead = availableBytes < toRead ? availableBytes : toRead;
	ReadFile(g_hChildStd_OUT_Rd, buffer
# ifdef FEAT_MBYTE
		 + *buffer_off, toRead
# else
		 , toRead
# endif
		 , &len, NULL);

	/* If we haven't read anything, there is a problem */
	if (len == 0)
	    break;

	availableBytes -= len;

	if (options & SHELL_READ)
	{
	    /* Do NUL -> NL translation, append NL separated
	     * lines to the current buffer. */
	    for (i = 0; i < len; ++i)
	    {
		if (buffer[i] == NL)
		    append_ga_line(ga);
		else if (buffer[i] == NUL)
		    ga_append(ga, NL);
		else
		    ga_append(ga, buffer[i]);
	    }
	}
# ifdef FEAT_MBYTE
	else if (has_mbyte)
	{
	    int		l;
	    int		c;
	    char_u	*p;

	    len += *buffer_off;
	    buffer[len] = NUL;

	    /* Check if the last character in buffer[] is
	     * incomplete, keep these bytes for the next
	     * round. */
	    for (p = buffer; p < buffer + len; p += l)
	    {
		l = mb_cptr2len(p);
		if (l == 0)
		    l = 1;  /* NUL byte? */
		else if (MB_BYTE2LEN(*p) != l)
		    break;
	    }
	    if (p == buffer)	/* no complete character */
	    {
		/* avoid getting stuck at an illegal byte */
		if (len >= 12)
		    ++p;
		else
		{
		    *buffer_off = len;
		    return;
		}
	    }
	    c = *p;
	    *p = NUL;
	    msg_puts(buffer);
	    if (p < buffer + len)
	    {
		*p = c;
		*buffer_off = (DWORD)((buffer + len) - p);
		mch_memmove(buffer, p, *buffer_off);
		return;
	    }
	    *buffer_off = 0;
	}
# endif /* FEAT_MBYTE */
	else
	{
	    buffer[len] = NUL;
	    msg_puts(buffer);
	}

	windgoto(msg_row, msg_col);
	cursor_on();
	out_flush();
    }
}

/*
 * Version of system to use for windows NT > 5.0 (Win2K), use pipe
 * for communication and doesn't open any new window.
 */
// FUNC_WAIT FIXME
    static int
mch_system_piped(char *cmd, int options)
{
    STARTUPINFO		si;
    PROCESS_INFORMATION pi;
    DWORD		ret = 0;

    HANDLE g_hChildStd_IN_Rd = NULL;
    HANDLE g_hChildStd_IN_Wr = NULL;
    HANDLE g_hChildStd_OUT_Rd = NULL;
    HANDLE g_hChildStd_OUT_Wr = NULL;

    char_u	buffer[BUFLEN + 1]; /* reading buffer + size */
    DWORD	len;

    /* buffer used to receive keys */
    char_u	ta_buf[BUFLEN + 1];	/* TypeAHead */
    int		ta_len = 0;		/* valid bytes in ta_buf[] */

    DWORD	i;
    int		c;
    int		noread_cnt = 0;
    garray_T	ga;
    int	    delay = 1;
    DWORD	buffer_off = 0;	/* valid bytes in buffer[] */
    char	*p = NULL;

    SECURITY_ATTRIBUTES saAttr;

    /* Set the bInheritHandle flag so pipe handles are inherited. */
    saAttr.nLength = sizeof(SECURITY_ATTRIBUTES);
    saAttr.bInheritHandle = TRUE;
    saAttr.lpSecurityDescriptor = NULL;

    if ( ! CreatePipe(&g_hChildStd_OUT_Rd, &g_hChildStd_OUT_Wr, &saAttr, 0)
	/* Ensure the read handle to the pipe for STDOUT is not inherited. */
       || ! pSetHandleInformation(g_hChildStd_OUT_Rd, HANDLE_FLAG_INHERIT, 0)
	/* Create a pipe for the child process's STDIN. */
       || ! CreatePipe(&g_hChildStd_IN_Rd, &g_hChildStd_IN_Wr, &saAttr, 0)
	/* Ensure the write handle to the pipe for STDIN is not inherited. */
       || ! pSetHandleInformation(g_hChildStd_IN_Wr, HANDLE_FLAG_INHERIT, 0) )
    {
	CloseHandle(g_hChildStd_IN_Rd);
	CloseHandle(g_hChildStd_IN_Wr);
	CloseHandle(g_hChildStd_OUT_Rd);
	CloseHandle(g_hChildStd_OUT_Wr);
	MSG_PUTS(_("\nCannot create pipes\n"));
    }

    si.cb = sizeof(si);
    si.lpReserved = NULL;
    si.lpDesktop = NULL;
    si.lpTitle = NULL;
    si.dwFlags = STARTF_USESHOWWINDOW | STARTF_USESTDHANDLES;

    /* set-up our file redirection */
    si.hStdError = g_hChildStd_OUT_Wr;
    si.hStdOutput = g_hChildStd_OUT_Wr;
    si.hStdInput = g_hChildStd_IN_Rd;
    si.wShowWindow = SW_HIDE;
    si.cbReserved2 = 0;
    si.lpReserved2 = NULL;

    if (options & SHELL_READ)
	ga_init2(&ga, 1, BUFLEN);

    if (cmd != NULL)
    {
	p = (char *)vim_strsave((char_u *)cmd);
	if (p != NULL)
	    unescape_shellxquote((char_u *)p, p_sxe);
	else
	    p = cmd;
    }

    /* Now, run the command */
    CreateProcess(NULL,			/* Executable name */
		  p,			/* Command to execute */
		  NULL,			/* Process security attributes */
		  NULL,			/* Thread security attributes */

		  // this command can be litigious, handle inheritance was
		  // deactivated for pending temp file, but, if we deactivate
		  // it, the pipes don't work for some reason.
		  TRUE,			/* Inherit handles, first deactivated,
					 * but needed */
		  CREATE_DEFAULT_ERROR_MODE, /* Creation flags */
		  NULL,			/* Environment */
		  NULL,			/* Current directory */
		  &si,			/* Startup information */
		  &pi);			/* Process information */

    if (p != cmd)
	vim_free(p);

    /* Close our unused side of the pipes */
    CloseHandle(g_hChildStd_IN_Rd);
    CloseHandle(g_hChildStd_OUT_Wr);

    if (options & SHELL_WRITE)
    {
	HANDLE thread =
	   CreateThread(NULL,  /* security attributes */
			0,     /* default stack size */
			sub_process_writer, /* function to be executed */
			g_hChildStd_IN_Wr,  /* parameter */
			0,		 /* creation flag, start immediately */
			NULL);		    /* we don't care about thread id */
	CloseHandle(thread);
	g_hChildStd_IN_Wr = NULL;
    }

    /* Keep updating the window while waiting for the shell to finish. */
    for (;;)
    {
	MSG	msg;

	if (PeekMessage(&msg, (HWND)NULL, 0, 0, PM_REMOVE))
	{
	    TranslateMessage(&msg);
	    DispatchMessage(&msg);
	}

	/* write pipe information in the window */
	if ((options & (SHELL_READ|SHELL_WRITE))
# ifdef FEAT_GUI
		|| gui.in_use
# endif
	    )
	{
	    len = 0;
	    if (!(options & SHELL_EXPAND)
		&& ((options &
			(SHELL_READ|SHELL_WRITE|SHELL_COOKED))
		    != (SHELL_READ|SHELL_WRITE|SHELL_COOKED)
# ifdef FEAT_GUI
		    || gui.in_use
# endif
		    )
		&& (ta_len > 0 || noread_cnt > 4))
	    {
		if (ta_len == 0)
		{
		    /* Get extra characters when we don't have any.  Reset the
		     * counter and timer. */
		    noread_cnt = 0;
# if defined(HAVE_GETTIMEOFDAY) && defined(HAVE_SYS_TIME_H)
		    gettimeofday(&start_tv, NULL);
# endif
		    len = ui_inchar(ta_buf, BUFLEN, 10L, 0);
		}
		if (ta_len > 0 || len > 0)
		{
		    /*
		     * For pipes: Check for CTRL-C: send interrupt signal to
		     * child.  Check for CTRL-D: EOF, close pipe to child.
		     */
		    if (len == 1 && cmd != NULL)
		    {
			if (ta_buf[ta_len] == Ctrl_C)
			{
			    /* Learn what exit code is expected, for
				* now put 9 as SIGKILL */
			    TerminateProcess(pi.hProcess, 9);
			}
			if (ta_buf[ta_len] == Ctrl_D)
			{
			    CloseHandle(g_hChildStd_IN_Wr);
			    g_hChildStd_IN_Wr = NULL;
			}
		    }

		    /* replace K_BS by <BS> and K_DEL by <DEL> */
		    for (i = ta_len; i < ta_len + len; ++i)
		    {
			if (ta_buf[i] == CSI && len - i > 2)
			{
			    c = TERMCAP2KEY(ta_buf[i + 1], ta_buf[i + 2]);
			    if (c == K_DEL || c == K_KDEL || c == K_BS)
			    {
				mch_memmove(ta_buf + i + 1, ta_buf + i + 3,
					    (size_t)(len - i - 2));
				if (c == K_DEL || c == K_KDEL)
				    ta_buf[i] = DEL;
				else
				    ta_buf[i] = Ctrl_H;
				len -= 2;
			    }
			}
			else if (ta_buf[i] == '\r')
			    ta_buf[i] = '\n';
# ifdef FEAT_MBYTE
			if (has_mbyte)
			    i += (*mb_ptr2len_len)(ta_buf + i,
						    ta_len + len - i) - 1;
# endif
		    }

		    /*
		     * For pipes: echo the typed characters.  For a pty this
		     * does not seem to work.
		     */
		    for (i = ta_len; i < ta_len + len; ++i)
		    {
			if (ta_buf[i] == '\n' || ta_buf[i] == '\b')
			    msg_putchar(ta_buf[i]);
# ifdef FEAT_MBYTE
			else if (has_mbyte)
			{
			    int l = (*mb_ptr2len)(ta_buf + i);

			    msg_outtrans_len(ta_buf + i, l);
			    i += l - 1;
			}
# endif
			else
			    msg_outtrans_len(ta_buf + i, 1);
		    }
		    windgoto(msg_row, msg_col);
		    out_flush();

		    ta_len += len;

		    /*
		     * Write the characters to the child, unless EOF has been
		     * typed for pipes.  Write one character at a time, to
		     * avoid losing too much typeahead.  When writing buffer
		     * lines, drop the typed characters (only check for
		     * CTRL-C).
		     */
		    if (options & SHELL_WRITE)
			ta_len = 0;
		    else if (g_hChildStd_IN_Wr != NULL)
		    {
			WriteFile(g_hChildStd_IN_Wr, (char*)ta_buf,
				    1, &len, NULL);
			// if we are typing in, we want to keep things reactive
			delay = 1;
			if (len > 0)
			{
			    ta_len -= len;
			    mch_memmove(ta_buf, ta_buf + len, ta_len);
			}
		    }
		}
	    }
	}

	if (ta_len)
	    ui_inchar_undo(ta_buf, ta_len);

        // MYWAIT FIXME
	if (WaitForSingleObject(pi.hProcess, delay) != WAIT_TIMEOUT)
	{
	    dump_pipe(options, g_hChildStd_OUT_Rd, &ga, buffer, &buffer_off);
	    break;
	}

	++noread_cnt;
	dump_pipe(options, g_hChildStd_OUT_Rd, &ga, buffer, &buffer_off);

	/* We start waiting for a very short time and then increase it, so
	 * that we respond quickly when the process is quick, and don't
	 * consume too much overhead when it's slow. */
	if (delay < 50)
	    delay += 10;
    }

    /* Close the pipe */
    CloseHandle(g_hChildStd_OUT_Rd);
    if (g_hChildStd_IN_Wr != NULL)
	CloseHandle(g_hChildStd_IN_Wr);

    WaitForSingleObject(pi.hProcess, INFINITE);

    /* Get the command exit code */
    GetExitCodeProcess(pi.hProcess, &ret);

    if (options & SHELL_READ)
    {
	if (ga.ga_len > 0)
	{
	    append_ga_line(&ga);
	    /* remember that the NL was missing */
	    curbuf->b_no_eol_lnum = curwin->w_cursor.lnum;
	}
	else
	    curbuf->b_no_eol_lnum = 0;
	ga_clear(&ga);
    }

    /* Close the handles to the subprocess, so that it goes away */
    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);

    return ret;
}

    static int
mch_system(char *cmd, int options)
{
    /* if we can pipe and the shelltemp option is off */
    if (allowPiping && !p_stmp)
	return mch_system_piped(cmd, options);
    else
	return mch_system_classic(cmd, options);
}
#else

# define mch_system(c, o) system(c)

#endif

/*
 * Either execute a command by calling the shell or start a new shell
 */
    int
mch_call_shell(
    char_u  *cmd,
    int	    options)	/* SHELL_*, see vim.h */
{
    int		x = 0;
    int		tmode = cur_tmode;
#ifdef FEAT_TITLE
    char szShellTitle[512];

    /* Change the title to reflect that we are in a subshell. */
    if (GetConsoleTitle(szShellTitle, sizeof(szShellTitle) - 4) > 0)
    {
	if (cmd == NULL)
	    strcat(szShellTitle, " :sh");
	else
	{
	    strcat(szShellTitle, " - !");
	    if ((strlen(szShellTitle) + strlen(cmd) < sizeof(szShellTitle)))
		strcat(szShellTitle, cmd);
	}
	mch_settitle(szShellTitle, NULL);
    }
#endif

    out_flush();

#ifdef MCH_WRITE_DUMP
    if (fdDump)
    {
	fprintf(fdDump, "mch_call_shell(\"%s\", %d)\n", cmd, options);
	fflush(fdDump);
    }
#endif

    /*
     * Catch all deadly signals while running the external command, because a
     * CTRL-C, Ctrl-Break or illegal instruction  might otherwise kill us.
     */
    signal(SIGINT, SIG_IGN);
#if defined(__GNUC__) && !defined(__MINGW32__)
    signal(SIGKILL, SIG_IGN);
#else
    signal(SIGBREAK, SIG_IGN);
#endif
    signal(SIGILL, SIG_IGN);
    signal(SIGFPE, SIG_IGN);
    signal(SIGSEGV, SIG_IGN);
    signal(SIGTERM, SIG_IGN);
    signal(SIGABRT, SIG_IGN);

    if (options & SHELL_COOKED)
	settmode(TMODE_COOK);	/* set to normal mode */

    if (cmd == NULL)
    {
	x = mch_system(p_sh, options);
    }
    else
    {
	/* we use "command" or "cmd" to start the shell; slow but easy */
	char_u	*newcmd = NULL;
	char_u	*cmdbase = cmd;
	long_u	cmdlen;

	/* Skip a leading ", ( and "(. */
	if (*cmdbase == '"' )
	    ++cmdbase;
	if (*cmdbase == '(')
	    ++cmdbase;

	if ((STRNICMP(cmdbase, "start", 5) == 0) && vim_iswhite(cmdbase[5]))
	{
	    STARTUPINFO		si;
	    PROCESS_INFORMATION	pi;
	    DWORD		flags = CREATE_NEW_CONSOLE;
	    char_u		*p;

	    si.cb = sizeof(si);
	    si.lpReserved = NULL;
	    si.lpDesktop = NULL;
	    si.lpTitle = NULL;
	    si.dwFlags = 0;
	    si.cbReserved2 = 0;
	    si.lpReserved2 = NULL;

	    cmdbase = skipwhite(cmdbase + 5);
	    if ((STRNICMP(cmdbase, "/min", 4) == 0)
		    && vim_iswhite(cmdbase[4]))
	    {
		cmdbase = skipwhite(cmdbase + 4);
		si.dwFlags = STARTF_USESHOWWINDOW;
		si.wShowWindow = SW_SHOWMINNOACTIVE;
	    }
	    else if ((STRNICMP(cmdbase, "/b", 2) == 0)
		    && vim_iswhite(cmdbase[2]))
	    {
		cmdbase = skipwhite(cmdbase + 2);
		flags = CREATE_NO_WINDOW;
		si.dwFlags = STARTF_USESTDHANDLES;
		si.hStdInput = CreateFile("\\\\.\\NUL",	// File name
		    GENERIC_READ,			// Access flags
		    0,					// Share flags
		    NULL,				// Security att.
		    OPEN_EXISTING,			// Open flags
		    FILE_ATTRIBUTE_NORMAL,		// File att.
		    NULL);				// Temp file
		si.hStdOutput = si.hStdInput;
		si.hStdError = si.hStdInput;
	    }

	    /* Remove a trailing ", ) and )" if they have a match
	     * at the start of the command. */
	    if (cmdbase > cmd)
	    {
		p = cmdbase + STRLEN(cmdbase);
		if (p > cmdbase && p[-1] == '"' && *cmd == '"')
		    *--p = NUL;
		if (p > cmdbase && p[-1] == ')'
			&& (*cmd =='(' || cmd[1] == '('))
		    *--p = NUL;
	    }

	    newcmd = cmdbase;
	    unescape_shellxquote(cmdbase, p_sxe);

	    /*
	     * If creating new console, arguments are passed to the
	     * 'cmd.exe' as-is. If it's not, arguments are not treated
	     * correctly for current 'cmd.exe'. So unescape characters in
	     * shellxescape except '|' for avoiding to be treated as
	     * argument to them. Pass the arguments to sub-shell.
	     */
	    if (flags != CREATE_NEW_CONSOLE)
	    {
		char_u	*subcmd;
		char_u	*cmd_shell = mch_getenv("COMSPEC");

		if (cmd_shell == NULL || *cmd_shell == NUL)
		    cmd_shell = default_shell();

		subcmd = vim_strsave_escaped_ext(cmdbase, "|", '^', FALSE);
		if (subcmd != NULL)
		{
		    /* make "cmd.exe /c arguments" */
		    cmdlen = STRLEN(cmd_shell) + STRLEN(subcmd) + 5;
		    newcmd = lalloc(cmdlen, TRUE);
		    if (newcmd != NULL)
			vim_snprintf((char *)newcmd, cmdlen, "%s /c %s",
						       cmd_shell, subcmd);
		    else
			newcmd = cmdbase;
		    vim_free(subcmd);
		}
	    }

	    /*
	     * Now, start the command as a process, so that it doesn't
	     * inherit our handles which causes unpleasant dangling swap
	     * files if we exit before the spawned process
	     */
	    if (CreateProcess(NULL,		// Executable name
		    newcmd,			// Command to execute
		    NULL,			// Process security attributes
		    NULL,			// Thread security attributes
		    FALSE,			// Inherit handles
		    flags,			// Creation flags
		    NULL,			// Environment
		    NULL,			// Current directory
		    &si,			// Startup information
		    &pi))			// Process information
		x = 0;
	    else
	    {
		x = -1;
#ifdef FEAT_GUI_W32
		EMSG(_("E371: Command not found"));
#endif
	    }

	    if (newcmd != cmdbase)
		vim_free(newcmd);

	    if (si.hStdInput != NULL)
	    {
		/* Close the handle to \\.\NUL */
		CloseHandle(si.hStdInput);
	    }
	    /* Close the handles to the subprocess, so that it goes away */
	    CloseHandle(pi.hThread);
	    CloseHandle(pi.hProcess);
	}
	else
	{
	    cmdlen = (
#ifdef FEAT_GUI_W32
		(allowPiping && !p_stmp ? 0 : STRLEN(vimrun_path)) +
#endif
		STRLEN(p_sh) + STRLEN(p_shcf) + STRLEN(cmd) + 10);

	    newcmd = lalloc(cmdlen, TRUE);
	    if (newcmd != NULL)
	    {
#if defined(FEAT_GUI_W32)
		if (need_vimrun_warning)
		{
		    MessageBox(NULL,
			    _("VIMRUN.EXE not found in your $PATH.\n"
				"External commands will not pause after completion.\n"
				"See  :help win32-vimrun  for more information."),
			    _("Vim Warning"),
			    MB_ICONWARNING);
		    need_vimrun_warning = FALSE;
		}
		if (!s_dont_use_vimrun && (!allowPiping || p_stmp))
		    /* Use vimrun to execute the command.  It opens a console
		     * window, which can be closed without killing Vim. */
		    vim_snprintf((char *)newcmd, cmdlen, "%s%s%s %s %s",
			    vimrun_path,
			    (msg_silent != 0 || (options & SHELL_DOOUT))
								 ? "-s " : "",
			    p_sh, p_shcf, cmd);
		else
#endif
		    vim_snprintf((char *)newcmd, cmdlen, "%s %s %s",
							   p_sh, p_shcf, cmd);
		x = mch_system((char *)newcmd, options);
		vim_free(newcmd);
	    }
	}
    }

    if (tmode == TMODE_RAW)
	settmode(TMODE_RAW);	/* set to raw mode */

    /* Print the return value, unless "vimrun" was used. */
    if (x != 0 && !(options & SHELL_SILENT) && !emsg_silent
#if defined(FEAT_GUI_W32)
		&& ((options & SHELL_DOOUT) || s_dont_use_vimrun
						  || (allowPiping && !p_stmp))
#endif
	    )
    {
	smsg(_("shell returned %d"), x);
	msg_putchar('\n');
    }
#ifdef FEAT_TITLE
    resettitle();
#endif

    signal(SIGINT, SIG_DFL);
#if defined(__GNUC__) && !defined(__MINGW32__)
    signal(SIGKILL, SIG_DFL);
#else
    signal(SIGBREAK, SIG_DFL);
#endif
    signal(SIGILL, SIG_DFL);
    signal(SIGFPE, SIG_DFL);
    signal(SIGSEGV, SIG_DFL);
    signal(SIGTERM, SIG_DFL);
    signal(SIGABRT, SIG_DFL);

    return x;
}




/*ARGSUSED*/
    void
mch_write(
    char_u  *s,
    int	    len)
{
    /* never used */
}



/*
 * Delay for half a second.
 */
/*ARGSUSED*/
    void
mch_delay(
    long    msec,
    int	    ignoreinput)
{
    Sleep((int)msec);	    /* never wait for input */
}


/*
 * this version of remove is not scared by a readonly (backup) file
 * Return 0 for success, -1 for failure.
 */
    int
mch_remove(char_u *name)
{
#ifdef FEAT_MBYTE
    WCHAR	*wn = NULL;
    int		n;
#endif

    win32_setattrs(name, FILE_ATTRIBUTE_NORMAL);

#ifdef FEAT_MBYTE
    if (enc_codepage >= 0 && (int)GetACP() != enc_codepage)
    {
	wn = enc_to_utf16(name, NULL);
	if (wn != NULL)
	{
	    n = DeleteFileW(wn) ? 0 : -1;
	    vim_free(wn);
	    if (n == 0 || GetLastError() != ERROR_CALL_NOT_IMPLEMENTED)
		return n;
	    /* Retry with non-wide function (for Windows 98). */
	}
    }
#endif
    return DeleteFile(name) ? 0 : -1;
}


/*
 * check for an "interrupt signal": CTRL-break or CTRL-C
 */
    void
mch_breakcheck(void)
{
#ifndef FEAT_GUI_W32	    /* never used */
    if (g_fCtrlCPressed || g_fCBrkPressed)
    {
	g_fCtrlCPressed = g_fCBrkPressed = FALSE;
	got_int = TRUE;
    }
#endif
}


#ifdef FEAT_MBYTE
/*
 * Same code as below, but with wide functions and no comments.
 * Return 0 for success, non-zero for failure.
 */
    int
mch_wrename(WCHAR *wold, WCHAR *wnew)
{
    WCHAR	*p;
    int		i;
    WCHAR	szTempFile[_MAX_PATH + 1];
    WCHAR	szNewPath[_MAX_PATH + 1];
    HANDLE	hf;

    if (!mch_windows95())
    {
	p = wold;
	for (i = 0; wold[i] != NUL; ++i)
	    if ((wold[i] == '/' || wold[i] == '\\' || wold[i] == ':')
		    && wold[i + 1] != 0)
		p = wold + i + 1;
	if ((int)(wold + i - p) < 8 || p[6] != '~')
	    return (MoveFileW(wold, wnew) == 0);
    }

    if (GetFullPathNameW(wnew, _MAX_PATH, szNewPath, &p) == 0 || p == NULL)
	return -1;
    *p = NUL;

    if (GetTempFileNameW(szNewPath, L"VIM", 0, szTempFile) == 0)
	return -2;

    if (!DeleteFileW(szTempFile))
	return -3;

    if (!MoveFileW(wold, szTempFile))
	return -4;

    if ((hf = CreateFileW(wold, GENERIC_WRITE, 0, NULL, CREATE_NEW,
		    FILE_ATTRIBUTE_NORMAL, NULL)) == INVALID_HANDLE_VALUE)
	return -5;
    if (!CloseHandle(hf))
	return -6;

    if (!MoveFileW(szTempFile, wnew))
    {
	(void)MoveFileW(szTempFile, wold);
	return -7;
    }

    DeleteFileW(szTempFile);

    if (!DeleteFileW(wold))
	return -8;

    return 0;
}
#endif


/*
 * mch_rename() works around a bug in rename (aka MoveFile) in
 * Windows 95: rename("foo.bar", "foo.bar~") will generate a
 * file whose short file name is "FOO.BAR" (its long file name will
 * be correct: "foo.bar~").  Because a file can be accessed by
 * either its SFN or its LFN, "foo.bar" has effectively been
 * renamed to "foo.bar", which is not at all what was wanted.  This
 * seems to happen only when renaming files with three-character
 * extensions by appending a suffix that does not include ".".
 * Windows NT gets it right, however, with an SFN of "FOO~1.BAR".
 *
 * There is another problem, which isn't really a bug but isn't right either:
 * When renaming "abcdef~1.txt" to "abcdef~1.txt~", the short name can be
 * "abcdef~1.txt" again.  This has been reported on Windows NT 4.0 with
 * service pack 6.  Doesn't seem to happen on Windows 98.
 *
 * Like rename(), returns 0 upon success, non-zero upon failure.
 * Should probably set errno appropriately when errors occur.
 */
    int
mch_rename(
    const char	*pszOldFile,
    const char	*pszNewFile)
{
    char	szTempFile[_MAX_PATH+1];
    char	szNewPath[_MAX_PATH+1];
    char	*pszFilePart;
    HANDLE	hf;
#ifdef FEAT_MBYTE
    WCHAR	*wold = NULL;
    WCHAR	*wnew = NULL;
    int		retval = -1;

    if (enc_codepage >= 0 && (int)GetACP() != enc_codepage)
    {
	wold = enc_to_utf16((char_u *)pszOldFile, NULL);
	wnew = enc_to_utf16((char_u *)pszNewFile, NULL);
	if (wold != NULL && wnew != NULL)
	    retval = mch_wrename(wold, wnew);
	vim_free(wold);
	vim_free(wnew);
	if (retval == 0 || GetLastError() != ERROR_CALL_NOT_IMPLEMENTED)
	    return retval;
	/* Retry with non-wide function (for Windows 98). */
    }
#endif

    /*
     * No need to play tricks if not running Windows 95, unless the file name
     * contains a "~" as the seventh character.
     */
    if (!mch_windows95())
    {
	pszFilePart = (char *)gettail((char_u *)pszOldFile);
	if (STRLEN(pszFilePart) < 8 || pszFilePart[6] != '~')
	    return rename(pszOldFile, pszNewFile);
    }

    /* Get base path of new file name.  Undocumented feature: If pszNewFile is
     * a directory, no error is returned and pszFilePart will be NULL. */
    if (GetFullPathName(pszNewFile, _MAX_PATH, szNewPath, &pszFilePart) == 0
	    || pszFilePart == NULL)
	return -1;
    *pszFilePart = NUL;

    /* Get (and create) a unique temporary file name in directory of new file */
    if (GetTempFileName(szNewPath, "VIM", 0, szTempFile) == 0)
	return -2;

    /* blow the temp file away */
    if (!DeleteFile(szTempFile))
	return -3;

    /* rename old file to the temp file */
    if (!MoveFile(pszOldFile, szTempFile))
	return -4;

    /* now create an empty file called pszOldFile; this prevents the operating
     * system using pszOldFile as an alias (SFN) if we're renaming within the
     * same directory.  For example, we're editing a file called
     * filename.asc.txt by its SFN, filena~1.txt.  If we rename filena~1.txt
     * to filena~1.txt~ (i.e., we're making a backup while writing it), the
     * SFN for filena~1.txt~ will be filena~1.txt, by default, which will
     * cause all sorts of problems later in buf_write().  So, we create an
     * empty file called filena~1.txt and the system will have to find some
     * other SFN for filena~1.txt~, such as filena~2.txt
     */
    if ((hf = CreateFile(pszOldFile, GENERIC_WRITE, 0, NULL, CREATE_NEW,
		    FILE_ATTRIBUTE_NORMAL, NULL)) == INVALID_HANDLE_VALUE)
	return -5;
    if (!CloseHandle(hf))
	return -6;

    /* rename the temp file to the new file */
    if (!MoveFile(szTempFile, pszNewFile))
    {
	/* Renaming failed.  Rename the file back to its old name, so that it
	 * looks like nothing happened. */
	(void)MoveFile(szTempFile, pszOldFile);

	return -7;
    }

    /* Seems to be left around on Novell filesystems */
    DeleteFile(szTempFile);

    /* finally, remove the empty old file */
    if (!DeleteFile(pszOldFile))
	return -8;

    return 0;	/* success */
}

/*
 * Get the default shell for the current hardware platform
 */
    char *
default_shell(void)
{
    char* psz = NULL;

    PlatformId();

    if (g_PlatformId == VER_PLATFORM_WIN32_NT)		/* Windows NT */
	psz = "cmd.exe";
    else if (g_PlatformId == VER_PLATFORM_WIN32_WINDOWS) /* Windows 95 */
	psz = "command.com";

    return psz;
}

/*
 * mch_access() extends access() to do more detailed check on network drives.
 * Returns 0 if file "n" has access rights according to "p", -1 otherwise.
 */
    int
mch_access(char *n, int p)
{
    HANDLE	hFile;
    DWORD	am;
    int		retval = -1;	    /* default: fail */
#ifdef FEAT_MBYTE
    WCHAR	*wn = NULL;

    if (enc_codepage >= 0 && (int)GetACP() != enc_codepage)
	wn = enc_to_utf16(n, NULL);
#endif

    if (mch_isdir(n))
    {
	char TempName[_MAX_PATH + 16] = "";
#ifdef FEAT_MBYTE
	WCHAR TempNameW[_MAX_PATH + 16] = L"";
#endif

	if (p & R_OK)
	{
	    /* Read check is performed by seeing if we can do a find file on
	     * the directory for any file. */
#ifdef FEAT_MBYTE
	    if (wn != NULL)
	    {
		int		    i;
		WIN32_FIND_DATAW    d;

		for (i = 0; i < _MAX_PATH && wn[i] != 0; ++i)
		    TempNameW[i] = wn[i];
		if (TempNameW[i - 1] != '\\' && TempNameW[i - 1] != '/')
		    TempNameW[i++] = '\\';
		TempNameW[i++] = '*';
		TempNameW[i++] = 0;

		hFile = FindFirstFileW(TempNameW, &d);
		if (hFile == INVALID_HANDLE_VALUE)
		{
		    if (GetLastError() != ERROR_CALL_NOT_IMPLEMENTED)
			goto getout;

		    /* Retry with non-wide function (for Windows 98). */
		    vim_free(wn);
		    wn = NULL;
		}
		else
		    (void)FindClose(hFile);
	    }
	    if (wn == NULL)
#endif
	    {
		char		    *pch;
		WIN32_FIND_DATA	    d;

		vim_strncpy(TempName, n, _MAX_PATH);
		pch = TempName + STRLEN(TempName) - 1;
		if (*pch != '\\' && *pch != '/')
		    *++pch = '\\';
		*++pch = '*';
		*++pch = NUL;

		hFile = FindFirstFile(TempName, &d);
		if (hFile == INVALID_HANDLE_VALUE)
		    goto getout;
		(void)FindClose(hFile);
	    }
	}

	if (p & W_OK)
	{
	    /* Trying to create a temporary file in the directory should catch
	     * directories on read-only network shares.  However, in
	     * directories whose ACL allows writes but denies deletes will end
	     * up keeping the temporary file :-(. */
#ifdef FEAT_MBYTE
	    if (wn != NULL)
	    {
		if (!GetTempFileNameW(wn, L"VIM", 0, TempNameW))
		{
		    if (GetLastError() != ERROR_CALL_NOT_IMPLEMENTED)
			goto getout;

		    /* Retry with non-wide function (for Windows 98). */
		    vim_free(wn);
		    wn = NULL;
		}
		else
		    DeleteFileW(TempNameW);
	    }
	    if (wn == NULL)
#endif
	    {
		if (!GetTempFileName(n, "VIM", 0, TempName))
		    goto getout;
		mch_remove((char_u *)TempName);
	    }
	}
    }
    else
    {
	/* Trying to open the file for the required access does ACL, read-only
	 * network share, and file attribute checks.  */
	am = ((p & W_OK) ? GENERIC_WRITE : 0)
		| ((p & R_OK) ? GENERIC_READ : 0);
#ifdef FEAT_MBYTE
	if (wn != NULL)
	{
	    hFile = CreateFileW(wn, am, 0, NULL, OPEN_EXISTING, 0, NULL);
	    if (hFile == INVALID_HANDLE_VALUE
			      && GetLastError() == ERROR_CALL_NOT_IMPLEMENTED)
	    {
		/* Retry with non-wide function (for Windows 98). */
		vim_free(wn);
		wn = NULL;
	    }
	}
	if (wn == NULL)
#endif
	    hFile = CreateFile(n, am, 0, NULL, OPEN_EXISTING, 0, NULL);
	if (hFile == INVALID_HANDLE_VALUE)
	    goto getout;
	CloseHandle(hFile);
    }

    retval = 0;	    /* success */
getout:
#ifdef FEAT_MBYTE
    vim_free(wn);
#endif
    return retval;
}

#if defined(FEAT_MBYTE) || defined(PROTO)
/*
 * Version of open() that may use UTF-16 file name.
 */
    int
mch_open(char *name, int flags, int mode)
{
    /* _wopen() does not work with Borland C 5.5: creates a read-only file. */

    return open(name, flags, mode);
}

/*
 * Version of fopen() that may use UTF-16 file name.
 */
    FILE *
mch_fopen(char *name, char *mode)
{
    WCHAR	*wn, *wm;
    FILE	*f = NULL;

    if (enc_codepage >= 0 && (int)GetACP() != enc_codepage)
    {
# if defined(DEBUG) && _MSC_VER >= 1400
	/* Work around an annoying assertion in the Microsoft debug CRT
	 * when mode's text/binary setting doesn't match _get_fmode(). */
	char newMode = mode[strlen(mode) - 1];
	int oldMode = 0;

	_get_fmode(&oldMode);
	if (newMode == 't')
	    _set_fmode(_O_TEXT);
	else if (newMode == 'b')
	    _set_fmode(_O_BINARY);
# endif
	wn = enc_to_utf16(name, NULL);
	wm = enc_to_utf16(mode, NULL);
	if (wn != NULL && wm != NULL)
	    f = _wfopen(wn, wm);
	vim_free(wn);
	vim_free(wm);

# if defined(DEBUG) && _MSC_VER >= 1400
	_set_fmode(oldMode);
# endif

	if (f != NULL)
	    return f;
	/* Retry with non-wide function (for Windows 98). Can't use
	 * GetLastError() here and it's unclear what errno gets set to if
	 * the _wfopen() fails for missing wide functions. */
    }

    return fopen(name, mode);
}
#endif

#ifdef FEAT_MBYTE
/*
 * SUB STREAM (aka info stream) handling:
 *
 * NTFS can have sub streams for each file.  Normal contents of file is
 * stored in the main stream, and extra contents (author information and
 * title and so on) can be stored in sub stream.  After Windows 2000, user
 * can access and store those informations in sub streams via explorer's
 * property menuitem in right click menu.  Those informations in sub streams
 * were lost when copying only the main stream.  So we have to copy sub
 * streams.
 *
 * Incomplete explanation:
 *	http://msdn.microsoft.com/library/en-us/dnw2k/html/ntfs5.asp
 * More useful info and an example:
 *	http://www.sysinternals.com/ntw2k/source/misc.shtml#streams
 */

/*
 * Copy info stream data "substream".  Read from the file with BackupRead(sh)
 * and write to stream "substream" of file "to".
 * Errors are ignored.
 */
    static void
copy_substream(HANDLE sh, void *context, WCHAR *to, WCHAR *substream, long len)
{
    HANDLE  hTo;
    WCHAR   *to_name;

    to_name = malloc((wcslen(to) + wcslen(substream) + 1) * sizeof(WCHAR));
    wcscpy(to_name, to);
    wcscat(to_name, substream);

    hTo = CreateFileW(to_name, GENERIC_WRITE, 0, NULL, OPEN_ALWAYS,
						 FILE_ATTRIBUTE_NORMAL, NULL);
    if (hTo != INVALID_HANDLE_VALUE)
    {
	long	done;
	DWORD	todo;
	DWORD	readcnt, written;
	char	buf[4096];

	/* Copy block of bytes at a time.  Abort when something goes wrong. */
	for (done = 0; done < len; done += written)
	{
	    /* (size_t) cast for Borland C 5.5 */
	    todo = (DWORD)((size_t)(len - done) > sizeof(buf) ? sizeof(buf)
						       : (size_t)(len - done));
	    if (!BackupRead(sh, (LPBYTE)buf, todo, &readcnt,
						       FALSE, FALSE, context)
		    || readcnt != todo
		    || !WriteFile(hTo, buf, todo, &written, NULL)
		    || written != todo)
		break;
	}
	CloseHandle(hTo);
    }

    free(to_name);
}

/*
 * Copy info streams from file "from" to file "to".
 */
    static void
copy_infostreams(char_u *from, char_u *to)
{
    WCHAR		*fromw;
    WCHAR		*tow;
    HANDLE		sh;
    WIN32_STREAM_ID	sid;
    int			headersize;
    WCHAR		streamname[_MAX_PATH];
    DWORD		readcount;
    void		*context = NULL;
    DWORD		lo, hi;
    int			len;

    /* Convert the file names to wide characters. */
    fromw = enc_to_utf16(from, NULL);
    tow = enc_to_utf16(to, NULL);
    if (fromw != NULL && tow != NULL)
    {
	/* Open the file for reading. */
	sh = CreateFileW(fromw, GENERIC_READ, FILE_SHARE_READ, NULL,
			     OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, NULL);
	if (sh != INVALID_HANDLE_VALUE)
	{
	    /* Use BackupRead() to find the info streams.  Repeat until we
	     * have done them all.*/
	    for (;;)
	    {
		/* Get the header to find the length of the stream name.  If
		 * the "readcount" is zero we have done all info streams. */
		ZeroMemory(&sid, sizeof(WIN32_STREAM_ID));
		headersize = (int)((char *)&sid.cStreamName - (char *)&sid.dwStreamId);
		if (!BackupRead(sh, (LPBYTE)&sid, headersize,
					   &readcount, FALSE, FALSE, &context)
			|| readcount == 0)
		    break;

		/* We only deal with streams that have a name.  The normal
		 * file data appears to be without a name, even though docs
		 * suggest it is called "::$DATA". */
		if (sid.dwStreamNameSize > 0)
		{
		    /* Read the stream name. */
		    if (!BackupRead(sh, (LPBYTE)streamname,
							 sid.dwStreamNameSize,
					  &readcount, FALSE, FALSE, &context))
			break;

		    /* Copy an info stream with a name ":anything:$DATA".
		     * Skip "::$DATA", it has no stream name (examples suggest
		     * it might be used for the normal file contents).
		     * Note that BackupRead() counts bytes, but the name is in
		     * wide characters. */
		    len = readcount / sizeof(WCHAR);
		    streamname[len] = 0;
		    if (len > 7 && wcsicmp(streamname + len - 6,
							      L":$DATA") == 0)
		    {
			streamname[len - 6] = 0;
			copy_substream(sh, &context, tow, streamname,
						    (long)sid.Size.u.LowPart);
		    }
		}

		/* Advance to the next stream.  We might try seeking too far,
		 * but BackupSeek() doesn't skip over stream borders, thus
		 * that's OK. */
		(void)BackupSeek(sh, sid.Size.u.LowPart, sid.Size.u.HighPart,
							  &lo, &hi, &context);
	    }

	    /* Clear the context. */
	    (void)BackupRead(sh, NULL, 0, &readcount, TRUE, FALSE, &context);

	    CloseHandle(sh);
	}
    }
    vim_free(fromw);
    vim_free(tow);
}
#endif

/*
 * Copy file attributes from file "from" to file "to".
 * For Windows NT and later we copy info streams.
 * Always returns zero, errors are ignored.
 */
    int
mch_copy_file_attribute(char_u *from, char_u *to)
{
#ifdef FEAT_MBYTE
    /* File streams only work on Windows NT and later. */
    PlatformId();
    if (g_PlatformId == VER_PLATFORM_WIN32_NT)
	copy_infostreams(from, to);
#endif
    return 0;
}

#if defined(MYRESETSTKOFLW) || defined(PROTO)
/*
 * Recreate a destroyed stack guard page in win32.
 * Written by Benjamin Peterson.
 */

/* These magic numbers are from the MS header files */
#define MIN_STACK_WIN9X 17
#define MIN_STACK_WINNT 2

/*
 * This function does the same thing as _resetstkoflw(), which is only
 * available in DevStudio .net and later.
 * Returns 0 for failure, 1 for success.
 */
    int
myresetstkoflw(void)
{
    BYTE	*pStackPtr;
    BYTE	*pGuardPage;
    BYTE	*pStackBase;
    BYTE	*pLowestPossiblePage;
    MEMORY_BASIC_INFORMATION mbi;
    SYSTEM_INFO si;
    DWORD	nPageSize;
    DWORD	dummy;

    /* This code will not work on win32s. */
    PlatformId();
    if (g_PlatformId == VER_PLATFORM_WIN32s)
	return 0;

    /* We need to know the system page size. */
    GetSystemInfo(&si);
    nPageSize = si.dwPageSize;

    /* ...and the current stack pointer */
    pStackPtr = (BYTE*)_alloca(1);

    /* ...and the base of the stack. */
    if (VirtualQuery(pStackPtr, &mbi, sizeof mbi) == 0)
	return 0;
    pStackBase = (BYTE*)mbi.AllocationBase;

    /* ...and the page thats min_stack_req pages away from stack base; this is
     * the lowest page we could use. */
    pLowestPossiblePage = pStackBase + ((g_PlatformId == VER_PLATFORM_WIN32_NT)
			     ? MIN_STACK_WINNT : MIN_STACK_WIN9X) * nPageSize;

    /* On Win95, we want the next page down from the end of the stack. */
    if (g_PlatformId == VER_PLATFORM_WIN32_WINDOWS)
    {
	/* Find the page that's only 1 page down from the page that the stack
	 * ptr is in. */
	pGuardPage = (BYTE*)((DWORD)nPageSize * (((DWORD)pStackPtr
						    / (DWORD)nPageSize) - 1));
	if (pGuardPage < pLowestPossiblePage)
	    return 0;

	/* Apply the noaccess attribute to the page -- there's no guard
	 * attribute in win95-type OSes. */
	if (!VirtualProtect(pGuardPage, nPageSize, PAGE_NOACCESS, &dummy))
	    return 0;
    }
    else
    {
	/* On NT, however, we want the first committed page in the stack Start
	 * at the stack base and move forward through memory until we find a
	 * committed block. */
	BYTE *pBlock = pStackBase;

	for (;;)
	{
	    if (VirtualQuery(pBlock, &mbi, sizeof mbi) == 0)
		return 0;

	    pBlock += mbi.RegionSize;

	    if (mbi.State & MEM_COMMIT)
		break;
	}

	/* mbi now describes the first committed block in the stack. */
	if (mbi.Protect & PAGE_GUARD)
	    return 1;

	/* decide where the guard page should start */
	if ((long_u)(mbi.BaseAddress) < (long_u)pLowestPossiblePage)
	    pGuardPage = pLowestPossiblePage;
	else
	    pGuardPage = (BYTE*)mbi.BaseAddress;

	/* allocate the guard page */
	if (!VirtualAlloc(pGuardPage, nPageSize, MEM_COMMIT, PAGE_READWRITE))
	    return 0;

	/* apply the guard attribute to the page */
	if (!VirtualProtect(pGuardPage, nPageSize, PAGE_READWRITE | PAGE_GUARD,
								      &dummy))
	    return 0;
    }

    return 1;
}
#endif


#if defined(FEAT_MBYTE) || defined(PROTO)
/*
 * The command line arguments in UCS2
 */
static int	nArgsW = 0;
static LPWSTR	*ArglistW = NULL;
static int	global_argc = 0;
static char	**global_argv;

static int	used_file_argc = 0;	/* last argument in global_argv[] used
					   for the argument list. */
static int	*used_file_indexes = NULL; /* indexes in global_argv[] for
					      command line arguments added to
					      the argument list */
static int	used_file_count = 0;	/* nr of entries in used_file_indexes */
static int	used_file_literal = FALSE;  /* take file names literally */
static int	used_file_full_path = FALSE;  /* file name was full path */
static int	used_file_diff_mode = FALSE;  /* file name was with diff mode */
static int	used_alist_count = 0;



/*
 * Remember "name" is an argument that was added to the argument list.
 * This avoids that we have to re-parse the argument list when fix_arg_enc()
 * is called.
 */
    void
used_file_arg(char *name, int literal, int full_path, int diff_mode)
{
    int		i;

    if (used_file_indexes == NULL)
	return;
    for (i = used_file_argc + 1; i < global_argc; ++i)
	if (STRCMP(global_argv[i], name) == 0)
	{
	    used_file_argc = i;
	    used_file_indexes[used_file_count++] = i;
	    break;
	}
    used_file_literal = literal;
    used_file_full_path = full_path;
    used_file_diff_mode = diff_mode;
}

/*
 * Remember the length of the argument list as it was.  If it changes then we
 * leave it alone when 'encoding' is set.
 */
    void
set_alist_count(void)
{
    used_alist_count = GARGCOUNT;
}

/*
 * Fix the encoding of the command line arguments.  Invoked when 'encoding'
 * has been changed while starting up.  Use the UCS-2 command line arguments
 * and convert them to 'encoding'.
 */
    void
fix_arg_enc(void)
{

    int		i;
    int		idx;
    char_u	*str;
    int		*fnum_list;

    /* Safety checks:
     * - if argument count differs between the wide and non-wide argument
     *   list, something must be wrong.
     * - the file name arguments must have been located.
     * - the length of the argument list wasn't changed by the user.
     */
    if (global_argc != nArgsW
	    || ArglistW == NULL
	    || used_file_indexes == NULL
	    || used_file_count == 0
	    || used_alist_count != GARGCOUNT)
	return;

    // my start up code in os_win32exe.c will not call
    // get_cmd_arg function, thus not setting ArglistW and nArgsW.
    // so this function should always return at the point above.
    fnError("fix_arg_enc @ os_win32.c is called");
    /* Remember the buffer numbers for the arguments. */
    fnum_list = (int *)alloc((int)sizeof(int) * GARGCOUNT);
    if (fnum_list == NULL)
	return;		/* out of memory */
    for (i = 0; i < GARGCOUNT; ++i)
	fnum_list[i] = GARGLIST[i].ae_fnum;

    /* Clear the argument list.  Make room for the new arguments. */
    alist_clear(&global_alist);
    if (ga_grow(&global_alist.al_ga, used_file_count) == FAIL)
	return;		/* out of memory */

    for (i = 0; i < used_file_count; ++i)
    {
	idx = used_file_indexes[i];
	str = utf16_to_enc(ArglistW[idx], NULL);
	if (str != NULL)
	{
#ifdef FEAT_DIFF
	    /* When using diff mode may need to concatenate file name to
	     * directory name.  Just like it's done in main(). */
	    if (used_file_diff_mode && mch_isdir(str) && GARGCOUNT > 0
				      && !mch_isdir(alist_name(&GARGLIST[0])))
	    {
		char_u	    *r;

		r = concat_fnames(str, gettail(alist_name(&GARGLIST[0])), TRUE);
		if (r != NULL)
		{
		    vim_free(str);
		    str = r;
		}
	    }
#endif
	    /* Re-use the old buffer by renaming it.  When not using literal
	     * names it's done by alist_expand() below. */
	    if (used_file_literal)
		buf_set_name(fnum_list[i], str);

	    alist_add(&global_alist, str, used_file_literal ? 2 : 0);
	}
    }

    if (!used_file_literal)
    {
	/* Now expand wildcards in the arguments. */
	/* Temporarily add '(' and ')' to 'isfname'.  These are valid
	 * filename characters but are excluded from 'isfname' to make
	 * "gf" work on a file name in parenthesis (e.g.: see vim.h). */
	do_cmdline_cmd((char_u *)":let SaVe_ISF = &isf|set isf+=(,)");
	alist_expand(fnum_list, used_alist_count);
	do_cmdline_cmd((char_u *)":let &isf = SaVe_ISF|unlet SaVe_ISF");
    }

    /* If wildcard expansion failed, we are editing the first file of the
     * arglist and there is no file name: Edit the first argument now. */
    if (curwin->w_arg_idx == 0 && curbuf->b_fname == NULL)
    {
	do_cmdline_cmd((char_u *)":rewind");
	if (GARGCOUNT == 1 && used_file_full_path)
	    (void)vim_chdirfile(alist_name(&GARGLIST[0]));
    }

    set_alist_count();
}
#endif
