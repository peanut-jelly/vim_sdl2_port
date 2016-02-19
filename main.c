#include "iVim.h"

#include "mock.h"

static SDL_Window* gWindow=NULL;
static SDL_Renderer* gWindowRenderer=NULL;
static int gWindow_w, gWindow_h;

static void
resize(int w, int h)
{
gWindow_w=w;
gWindow_h=h;
SDL_SetWindowSize(gWindow, w, h);
// clear colors to bg.
SDL_Rect clip={0,0,w,h};
// hope to restore render context,
// because of the bug with d3dx9 window resizing.
SDL_SetRenderDrawColor(gWindowRenderer, 0,0,0xff, 0xff);
SDL_RenderClear(gWindowRenderer);
}

static void
myWindow_draw()
{
const SDL_Surface* surface=iVim_getDisplaySurface();
SDL_Texture* tex=SDL_CreateTextureFromSurface(gWindowRenderer, surface);
SDL_Rect rect={0,0,gWindow_w, gWindow_h};
SDL_RenderCopy(gWindowRenderer, tex, &rect, &rect);
SDL_RenderPresent(gWindowRenderer);
SDL_DestroyTexture(tex);
}

static int
myEventSourceWindowID(SDL_Event e)
{
switch (e.type)
    {
    case SDL_WINDOWEVENT:
    case SDL_KEYUP:
    case SDL_KEYDOWN:
    case SDL_TEXTINPUT:
    case SDL_TEXTEDITING:
    case SDL_MOUSEMOTION:
    case SDL_MOUSEBUTTONDOWN:
    case SDL_MOUSEBUTTONUP:
    case SDL_MOUSEWHEEL:
    case SDL_USEREVENT:
        return e.window.windowID;
    default:
        return 0;
    }
}

static void 
main_loop()
{
SDL_Event evnt;

for (;;)
    {
    if (!iVim_running()) break;
    while(SDL_PollEvent(&evnt))
        {
        if (myEventSourceWindowID(evnt)==SDL_GetWindowID(gWindow))
            {
            iVim_onEvent(evnt);
            }
        }
    iVim_flush();
    int ww,hh;
    iVim_getDisplaySize(&ww, &hh);
    if (ww!=gWindow_w || hh!=gWindow_h)
        resize(ww,hh);
    myWindow_draw();

    SDL_Delay(50);
    }
}

static void init(int w, int h)
{
if (!SDL_WasInit(SDL_INIT_VIDEO))
    {
    int status=SDL_Init(SDL_INIT_VIDEO);
    if (status<0)
        fnError2("sdl-init-video failed.", SDL_GetError());
    }
iVim_init(w,h,0,0);
iVim_getDisplaySize(&gWindow_w, &gWindow_h);
gWindow=
    SDL_CreateWindow( "SDLdisplay", 
            SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
            gWindow_w, gWindow_h, 
            SDL_WINDOW_OPENGL | SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE);
if (gWindow==NULL)
    fnError("error creating window gWindow");
gWindowRenderer=SDL_CreateRenderer(gWindow, -1, 
         SDL_RENDERER_ACCELERATED);
if (gWindowRenderer==NULL)
    fnError("error creating gWindowRenderer");
SDL_SetRenderDrawColor(gWindowRenderer, 0, 20, 10, 0xff);
}

int main(int argc, char** argv)
{
init(500, 500);
main_loop();
iVim_quit();
return 0;
}

