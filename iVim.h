#ifndef __iVim_H_
#define __iVim_H_

#include <SDL2/SDL.h>

extern int iVim_init(int w, int h, int argc, char** argv);
extern void iVim_onEvent(SDL_Event evnt);
extern void iVim_flush();
extern const SDL_Surface* iVim_getDisplaySurface();
extern void iVim_getDisplaySize(int* pw, int* ph);
extern int iVim_running();
extern int iVim_quit();

#endif
