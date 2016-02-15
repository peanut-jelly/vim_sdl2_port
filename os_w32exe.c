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
 * Windows GUI: main program (EXE) entry point:
 *
 * Ron Aaron <ronaharon@yahoo.com> wrote this and  the DLL support code.
 */
#include "vim.h"

#include "mock.h"
#include <pthread.h>
#include <SDL2/sdl.h>
#include "adapter_sdl2.h"

extern int VimMain(int argc, char** argv);

static int sdl_argc=0;
static char** sdl_argv=NULL;
void * fn_vim_thread(void *ud)
{

#ifdef DYNAMIC_GETTEXT
//#error dynamic_gettext
    /* Initialize gettext library */
    dyn_libintl_init(NULL);
#endif

    VimMain(sdl_argc, sdl_argv);

    disp_task_quitvim_t quitvim = {DISP_TASK_QUITVIM};
    display_push_task(&quitvim);

    return 0;
}

#include "mock.h"

int main(int argc, char** argv)
{
extern void mySDL_dosomething();
sdl_argc=argc;
sdl_argv=argv;
pthread_t thread_vim;
pthread_create(&thread_vim, 0, &fn_vim_thread, (void*)0);
mySDL_dosomething();
pthread_join(thread_vim, NULL);
return 0;
}

