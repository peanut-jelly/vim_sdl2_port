#ifndef __iVim_H_
#define __iVim_H_

#include <SDL2/SDL.h>

extern int iVim_init(int w, int h, int argc, char** argv);

extern void iVim_onEvent(SDL_Event evnt);

/* @func iVim_flush
 * returns 1 if surface is dirty (need redraw), 0 otherwise.
 * In a typical mainloop with frame interval of 30ms, for example,
 * the mainloop used in `./main.c`, only about 30% of the frames 
 * need to redraw because of vim's internal surface is changed.
 */
extern int iVim_flush();

extern const SDL_Surface* iVim_getDisplaySurface();

extern void iVim_getDisplaySize(int* pw, int* ph);

extern int iVim_running();

extern int iVim_quit();

extern void iVim_showDebugWindow(int shown);


#endif
