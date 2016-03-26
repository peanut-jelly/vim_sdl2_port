/* iVim.h
 * API for embedding vim.
 *
 * Known issues
 * [1] Can't `printf` to `stdout`, maybe have something to do with linked
 *      libraries.
 * 
 */
#ifndef __iVim_H_
#define __iVim_H_

#include <SDL2/SDL.h>

extern int iVim_init(int w, int h, int argc, char** argv);

/* @func iVim_setLogger
 * All messages passed to debug window (info window) are recorded by
 * the registered logger function (if non null), so logger should be
 * set before calling `iVim_flush`, otherwise those messages flushed
 * before logger is set is lost.
 */
extern void iVim_setLogger(void (*f)(const char*));

extern void iVim_onEvent(SDL_Event evnt);

/* @func iVim_flush
 * returns 1 if surface is dirty (need redraw), 0 otherwise.
 * In a typical mainloop with frame interval of 30ms, for example,
 * the mainloop used in `./main.c`, even under heavy editing workload, 
 * only about (at most) 30% of the frames need to redraw because of 
 * vim's internal surface is changed. At normal cases only 10% of frames
 * need redraw.
 */
extern int iVim_flush();

extern const SDL_Surface* iVim_getDisplaySurface();

extern void iVim_getDisplaySize(int* pw, int* ph);

extern int iVim_running();

extern int iVim_quit();

extern void iVim_showDebugWindow(int shown);


#endif
