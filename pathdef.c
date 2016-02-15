/* pathdef.c */
#include "vim.h"
char_u *default_vim_dir = (char_u *)"";
char_u *default_vimruntime_dir = (char_u *)"";
char_u *all_cflags = (char_u *)"gcc  -Iproto -DWIN32 -DWINVER=0x0500 -D_WIN32_WINNT=0x0500 -DHAVE_PATHDEF -DFEAT_BIG -DHAVE_GETTEXT -DHAVE_LOCALE_H -DDYNAMIC_GETTEXT -DFEAT_CSCOPE -DFEAT_GUI_W32 -DFEAT_CLIPBOARD -DDYNAMIC_ICONV -pipe -w -march=i386 -Wall  -DFEAT_PYTHON ";
char_u *all_lflags = (char_u *)"gcc  -Iproto -DWIN32 -DWINVER=0x0500 -D_WIN32_WINNT=0x0500 -DHAVE_PATHDEF -DFEAT_BIG -DHAVE_GETTEXT -DHAVE_LOCALE_H -DDYNAMIC_GETTEXT -DFEAT_CSCOPE -DFEAT_GUI_W32 -DFEAT_CLIPBOARD -DDYNAMIC_ICONV -pipe -w -march=i386 -Wall  -DFEAT_PYTHON  -mwindows -o gvim.exe -lkernel32 -luser32 -lgdi32 -ladvapi32 -lcomdlg32 -lcomctl32 -lversion -lpthread -lstdc++ -lmingw32 -lSDL2main -lSDL2 -lSDL2_ttf  -lole32 -luuid    -L/d/programs1/python27/libs -lpython27  ";
char_u *compiled_user = (char_u *)"ikali";
char_u *compiled_sys = (char_u *)"ikali-PC";
