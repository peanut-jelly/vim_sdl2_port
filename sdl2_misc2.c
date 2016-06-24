/* vi:set ts=8 sts=4 sw=4:
 *
 * VIM - Vi IMproved    by Bram Moolenaar
 *
 * Do ":help uganda"  in Vim to read copying and usage conditions.
 * Do ":help credits" in Vim to see a list of people who contributed.
 * See README.txt for an overview of the Vim source code.
 */
/*
 * sdl2_misc2.c
 * interacting layer between sdl window and vim core.
 */

#include <SDL2/SDL.h>
#include <windows.h>
#include <stdio.h>
#include <math.h>
#include "adapter_sdl2.h"

#include "vim.h"





#include "assert_out_ns_vim.h"
#include "begin_ns_vim.h"





void send_keydown_to_adapter(SDL_KeyboardEvent key)
{
extern void process_sdl_key(int scancode, int k_modifiers);
extern void _adapter_on_sdl_keydown(int, void*, void*);
Uint32 scode=key.keysym.scancode;
Uint32 mod=key.keysym.mod;
adapter_event_t evnt;
evnt.type=ADAPT_EVENT_KEYDOWN;
evnt.func=_adapter_on_sdl_keydown;
evnt.user_data0 = (void*)scode;
evnt.user_data1 = (void*)(Uint32)mod;
adapter_push_event(evnt);
}

static void
_adapter_on_redraw(int tt, void* ud0, void* ud1)
{
extern void force_redraw();

force_redraw();
}
void send_redraw_to_adapter()
{
adapter_event_t evnt={ADAPT_EVENT_REDRAW, _adapter_on_redraw, 0, 0};
adapter_push_event(evnt);
}

static void
_adapter_on_flush(int tt, void* ud0, void* ud1)
{
extern void out_flush();
extern void force_redraw();

out_flush();
//force_redraw();
}

void send_flush_to_adapter()
{
extern void force_process_message();
adapter_event_t evnt={ADAPT_EVENT_FLUSH, _adapter_on_flush, 0, 0};
adapter_push_event(evnt);
//force_process_message();
}



extern void _on_kill_focus_callback(int tt, void* ud0, void* ud1);
void send_killfocus_to_adapter()
{
adapter_event_t evnt={ADAPT_EVENT_KILLFOCUS, _on_kill_focus_callback, 0, 0};
adapter_push_event(evnt);
}

extern void _on_set_focus_callback(int tt, void* ud0, void* ud1);
void send_setfocus_to_adapter()
{
adapter_event_t evnt={ADAPT_EVENT_SETFOCUS, _on_set_focus_callback, 0, 0};
adapter_push_event(evnt);
}

extern void _on_textarea_resize_callback(int tt, void* ud0, void* ud1);
void send_textarea_resize_to_adapter(int w, int h)
{
adapter_event_t evnt={ADAPT_EVENT_TEXTAREA_RESIZE, 
    _on_textarea_resize_callback, 
    (void*)w, (void*)h};
adapter_push_event(evnt);
}

static Uint32 getCurrentKeymod()
{
extern const Uint8* sKeyboardState;
Uint32 modState = 0; // SDL_Keymod
#define SC(scode, mcode) \
    if (sKeyboardState[SDL_SCANCODE_##scode]) \
        modState |= KMOD_##mcode;

#define SM(scode) SC(scode,scode)

SM(LSHIFT); SM(RSHIFT);
SM(LCTRL); SM(RCTRL);
SM(LALT); SM(RALT);
// only check shift control, and alt
//SM(LGUI); SM(RGUI);
//SC(NUMLOCKCLEAR, NUM);
//SC(CAPSLOCK, CAPS);
#undef SM
#undef SC
return modState;
}

void send_mousebuttondown_to_adapter(SDL_MouseButtonEvent e)
{
extern void _on_mousebuttondown_callback(int, void*, void*);
Uint32 modState = getCurrentKeymod();
SDL_MouseButtonEvent* ne=(SDL_MouseButtonEvent*)malloc(sizeof(SDL_MouseButtonEvent));
*ne=e;
adapter_event_t evnt={ADAPT_EVENT_MOUSEBUTTON_DOWN,
    _on_mousebuttondown_callback,
    (void*)ne, (void*)modState};
adapter_push_event(evnt);
}

void send_mousebuttonup_or_move_to_adapter(SDL_Event e)
{
extern void _on_mouse_move_or_release_callback(int, void*, void*);
Uint32 modState = getCurrentKeymod();
SDL_Event* ne=(SDL_Event*)malloc(sizeof(SDL_Event));
*ne=e;
adapter_event_t evnt={ADAPT_EVENT_MOUSEBUTTON_UP,
    _on_mouse_move_or_release_callback,
    (void*)ne, 0};
adapter_push_event(evnt);
}

void send_quitall_to_adapter()
{
// vim_ex_quitall is in ex_docmd.c
extern void vim_ex_quitall(int, void*, void*);
adapter_event_t evnt={ADAPT_EVENT_QUITALL,
    vim_ex_quitall, 0, 0};
adapter_push_event(evnt);
// need another event to flush adapter queue.
// it is like a three-way tcp handshake.
// A simple send_flush_to_adapter does not work,
// so send a esc-keydown.
// when vim control flow reaches the point where mySDLrunning
// is set to zero, it will send a require_esc to sdl display.
// sdl display will send the important esc then.
// Don't send the esc now because maybe vim is in insert mode now
// (or whatever mode it happens to be), and the esc will back to 
// normal mode.
}

void send_setcellsize_to_adapter(int w, int h)
{
extern void adpt_set_cell_size_callback(int tt, void* ud0, void* ud1);
adapter_event_t evnt={ADAPT_EVENT_SETCELLSIZE,
    adpt_set_cell_size_callback,
    (void*)w, (void*)h};
adapter_push_event(evnt);
}

void send_esc_to_adapter()
{
SDL_KeyboardEvent k;
k.type= SDL_KEYDOWN;
k.timestamp = SDL_GetTicks();
k.windowID=NULL;
k.state=SDL_PRESSED;
k.repeat=0;
k.keysym= (SDL_Keysym)
    {
    SDL_SCANCODE_ESCAPE,
        SDLK_ESCAPE,
        0,
        0
    };
send_keydown_to_adapter(k);
}


#include "end_ns_vim.h"

