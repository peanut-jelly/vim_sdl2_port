/* vi:set ts=8 sts=4 sw=4:
 *
 * VIM - Vi IMproved    by Bram Moolenaar
 *
 * Do ":help uganda"  in Vim to read copying and usage conditions.
 * Do ":help credits" in Vim to see a list of people who contributed.
 * See README.txt for an overview of the Vim source code.
 */
/*
 * sdl2_misc.c
 * the code for displaying routines.
 */
/*
 * Known bugs
 * 1) a freshly started sdl2 cannot get the status of capslock and numlock
 *      correctly. In fact, it just thinks they are off at start up regardless
 *      of the led lights on my keyboard.
 * 2) sdl2 window may not display properly after resizing because of dirextx9
 *      problem.
 */
#include <math.h>
#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include "adapter_sdl2.h"
#include <pthread.h>
#include "iVim.h"
#include "vim.h"
#include "mock.h"






#include "assert_out_ns_vim.h"
#include "begin_ns_vim.h"


extern void send_setcellsize_to_adapter(int, int);
extern void send_textarea_resize_to_adapter(int, int);
extern void send_mousebuttondown_to_adapter(SDL_MouseButtonEvent);
extern void send_mousebuttonup_or_move_to_adapter(SDL_Event);





typedef struct tex_font
{
    // one line of '.' '\1' '\2' ....'\255' '\0'
    SDL_Texture *texture;
    SDL_Rect rect[256];
    int char_w, char_h;
} tex_font_t;

static pthread_t thread_vim;

static int s_info_shown=0;
void (*s_logger)(const char*)= NULL;

static SDL_Window *gInfo=NULL;
static SDL_Renderer *gInfoRenderer=NULL;
static TTF_Font *gFont=NULL;
static TTF_Font *gFont_zh=NULL;

static tex_font_t myInfoFont;
static tex_font_t myDisplayFont;

static SDL_Texture *text_hello=NULL;
static SDL_Color color_white={255, 255, 255, 0xff};
static SDL_Color color_black={0,0,0,0},
                 color_velvet={0x82, 0x17, 0x2b},
                 color_red={0xff, 0, 0, 0xff};


//static SDL_Window* gDisplay=NULL;
//static SDL_Renderer* gDisplayRenderer=NULL;
static int gDisplay_width = 600,
           gDisplay_height = 410;
static int gDisplay_cell_w= 0,
           gDisplay_cell_h= 0;
static int gDisplay_columns = -1,
    gDisplay_rows = -1;
#define DISPLAY_W gDisplay_width
#define DISPLAY_H gDisplay_height
static SDL_Surface 
    *gDisplayTarget0_surf = NULL,
    *gDisplayTarget1_surf = NULL,
    *gDisplayTargetCur_surf = NULL;
static int s_surface_dirty=1;

static SDL_Color gDisplayColorFG,
                 gDisplayColorBG,
                 gDisplayColorSP;

static TTF_Font *gDisplayFontCur = NULL;
TTF_Font* gDisplayFontNormal[4];
TTF_Font* gDisplayFontWide[4];

const Uint8* sKeyboardState = NULL;

static int myDisplayRunning=0;
static int mySDLrunning=0;

static void
adjust_cell_size_from_font(TTF_Font* font)
{
//fnWarn("here in adjust cell size in sdl");
int i;
int char_w, char_h;
char s[257];
s[0]='.';
s[256]=0;
for (i=1; i<=255; i++) s[i]=i;
SDL_Surface *sf=TTF_RenderText_Blended(font, s, color_white);
// check all char have equal width.
for (i=2; i<256; i++)
    {
    int w0, w1;
    char ss[2];
    ss[0]=i; ss[1]=0;
    TTF_SizeText(font, ss, &w1, NULL);
    ss[0]=i-1; ss[1]=0;
    TTF_SizeText(font, ss, &w0, NULL);
    if (w0 != w1)
        {
        info_push_message("font not equal width");
        //fnError("font not equal width");
        }
    char_w=w0;
    }
char_h = sf->h;
SDL_FreeSurface(sf);

// setting width to width of 'x' , similar to aveWidth, according to msdn
sf=TTF_RenderText_Blended(font, "x", color_white);
char_w = sf->w;
SDL_FreeSurface(sf);

gDisplay_cell_w = char_w;
gDisplay_cell_h = char_h;
}
static int
hiragana_A_width(TTF_Font* wfont)
{
int w;
//HIRAGANA LETTER A utf-8 is 0xe3,0x81, 0x82
//not the small A.
char s[10];
s[0]=0xe3; s[1]=0x81; s[2]=0x82; s[3]=0;
TTF_SizeUTF8(wfont, s, &w, 0);
return w;
}
static int
hiragana_A_height(TTF_Font* wfont)
{
int w, h;
//HIRAGANA LETTER A utf-8 is 0xe3,0x81, 0x82
//not the small A.
char s[10];
s[0]=0xe3; s[1]=0x81; s[2]=0x82; s[3]=0;
TTF_SizeUTF8(wfont, s, &w, &h);
return h;
}

static void
adjust_wide_font_from_cell_size(
        const char* filename,
        int cell_width,
        int cell_height)
{
if (gFont_zh)
    {
    TTF_CloseFont(gFont_zh);
    gFont_zh=0;
    }
// the size in TTF_OpenFont is in points.
// don't know how to do converting, or
// how to ensure the converting is right with right dpi
// so i simply do a binary search.
int wll=5, wrr=1000;
int wmid;
while(wll+1<wrr)
    {
    wmid=(wll+wrr)/2;
    TTF_Font *tf=TTF_OpenFont(filename,wmid);
    int w=hiragana_A_width(tf);
    TTF_CloseFont(tf);
    if (w<cell_width*2)
        wll=wmid;
    else wrr=wmid;
    }
int hll=5, hrr=1000;
int hmid;
while(hll+1<hrr)
    {
    hmid=(hll+hrr)/2;
    TTF_Font *tf=TTF_OpenFont(filename,hmid);
    int w=hiragana_A_height(tf);
    TTF_CloseFont(tf);
    if (w<cell_height)
        hll=hmid;
    else hrr=hmid;
    }
int mid=(wmid<hmid)?wmid:hmid;
fnWarnf("mid=%d", mid);
TTF_Font *tf=TTF_OpenFont(filename,mid);
gFont_zh=tf;
}

static 
const SDL_Texture*
init_font(
        tex_font_t *ttfont,
        SDL_Renderer * renderer, 
        TTF_Font* font,
        SDL_Color *color
        )
{
int i,j;
char s[257];
s[0]='.';
s[256]=0;
for (i=1; i<=255; i++) s[i]=i;
SDL_Surface *sf=TTF_RenderText_Blended(font, s, *color);
SDL_Texture *tt =
    SDL_CreateTextureFromSurface(renderer, sf);
if (tt==NULL)
    fnError("myFont_init error"); //SDL_GetError());
int w,h, char_w;
SDL_QueryTexture(tt, NULL, NULL, &w, &h);
// check all char have equal width.
for (i=2; i<256; i++)
    {
    int w0, w1;
    char ss[2];
    ss[0]=i; ss[1]=0;
    TTF_SizeText(font, ss, &w1, NULL);
    ss[0]=i-1; ss[1]=0;
    TTF_SizeText(font, ss, &w0, NULL);
    /*
    if (w0 != w1)
        fnError("font not equal width");
        */
    char_w=w0;
    }
for (i=0; i<257; i++)
    {
    ttfont->rect[i].h = h;
    ttfont->rect[i].w=char_w;
    ttfont->rect[i].x=i*char_w;
    ttfont->rect[i].y=0;
    }
ttfont->char_w=char_w;
ttfont->char_h=h;
ttfont->texture=tt;
SDL_FreeSurface(sf);
}
static void
myFont_render_str(tex_font_t *tfont, SDL_Renderer *renderer, SDL_Rect *dstRect, 
        const char* str, int len)
{
int slen=((len<=0)?strlen(str):len);
int str_w = tfont->char_w *slen,
    str_h= tfont->char_h;
SDL_Rect ddr=
    {
    dstRect->x, 
    dstRect->y, 
    (dstRect->w > 0)?dstRect->w:str_w,
    (dstRect->h > 0)?dstRect->h:str_h
    };
int scr_char_w=ddr.w/slen,
    scr_char_h=ddr.h;
SDL_Rect d_c={0,dstRect->y ,scr_char_w, scr_char_h};
int i;
for (i=0; i<slen; i++)
    {
    d_c.x=dstRect->x+ i*scr_char_w;
    SDL_RenderCopy(renderer, tfont->texture, 
            &tfont->rect[str[i]], &d_c);
    }
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
static void myDisplay_init(int, int);
static void myInfo_init(int, int);

static void 
mySDL_init(int w, int h)
{
if (!SDL_WasInit(SDL_INIT_VIDEO))
    {
    int status=SDL_Init(SDL_INIT_VIDEO);
    if (status<0)
        fnError2("sdl-init-video failed.", SDL_GetError());
    }


// init ttf
if (!TTF_WasInit())
    {
    int status=TTF_Init();
    if (status<0)
        fnError2("TTF_Init failed.", SDL_GetError());
    }
    
// FIXME the two following modern fonts do not work properly.
//gFont=TTF_OpenFont("fonts/DroidSansMono.ttf", 12);
//gFont=TTF_OpenFont("fonts/DejaVuSansMono.ttf", 12);
gFont=TTF_OpenFont("fonts/fixedsys.fon", 12);
adjust_cell_size_from_font(gFont);
gFont_zh=TTF_OpenFont("fonts/wqy-zenhei.ttc", 14);
// adjust wide font size according to cell size does not work well, for now.
/*
adjust_wide_font_from_cell_size("fonts/wqy-zenhei.ttc", 
        gDisplay_cell_w, gDisplay_cell_h);
        */

if (gFont==NULL || gFont_zh==NULL)
    fnError2("fail open font cour.ttf/msyh.ttf", SDL_GetError());
int i;
for (i=0; i<4; i++)
    {
    TTF_Font* gFont_tmp= TTF_OpenFont("fonts/fixedsys.fon", 10);
    if (gFont_tmp==NULL)
        fnError2("error load font ", SDL_GetError());
    gDisplayFontNormal[i]=gFont_tmp;
    TTF_SetFontStyle(gFont_tmp, i);
    gDisplayFontWide[i]=gFont_zh;
    }
gDisplayFontCur=gDisplayFontNormal[0];


myInfo_init(500,500);
send_setcellsize_to_adapter(gDisplay_cell_w, gDisplay_cell_h);
myDisplay_init(DISPLAY_W ,DISPLAY_H);
mySDLrunning=1;

/*
info_push_messagef("cell_pix_w=%d, h=%d", gDisplay_cell_w, 
        gDisplay_cell_h);
*/
SDL_Color color={0xff,0xff, 0xff,0xff};
init_font(&myInfoFont, gInfoRenderer, gFont, &color_black);
//init_font(&myDisplayFont, gDisplayRenderer, gFont, &color_black);

SDL_Color textColor={255, 255, 0, 0xff};
static char buf[300]="hello##";
int ii;
for (ii=0; ii<256; ii++)
    buf[ii]=ii+1;
buf[256]=0;
SDL_Surface *textSurface=TTF_RenderText_Blended(gFont, buf, textColor);
if (textSurface==NULL)
    fnWarn("textSurface==NULL");
text_hello=SDL_CreateTextureFromSurface(gInfoRenderer, textSurface);
if (text_hello==NULL)
    fnWarn("text_hello==NULL");
SDL_FreeSurface(textSurface);

}

static void
mySDL_show_info(SDL_Rect *dstRect)
{
#define ROWS 20
#define COLUMNS 35
static char buf[ROWS][COLUMNS];
static char log_buf[512];
static int log_len;
/*
SDL_Surface *tmp_info_window=
    SDL_CreateRGBSurface(0,500, 500, 32, 0,0,0,0);
    */
int len, i, j;
int lines=info_message_number();
if (lines>=ROWS)
    {
    for (i=0; i<(lines-ROWS); i++)
        {
        info_poll_message(NULL, NULL, 0);
        }
    for (i=0; i<ROWS; i++)
        {
        info_poll_message(buf[i], &len,COLUMNS);
        for (j=len; j<COLUMNS; j++)
            buf[i][j]=0;
        }
    }
else // need to move within buf
    {
    memmove(buf, ((char*)buf)+ COLUMNS*(lines), COLUMNS*(ROWS-lines));
    for (i=ROWS-lines; i<ROWS; i++)
        {
        info_poll_message(buf[i], &len,COLUMNS);
        for (j=len; j<COLUMNS; j++)
            buf[i][j]=0;
        }
    }

int scr_char_w = dstRect->w / COLUMNS;
int scr_char_h = dstRect->h / ROWS;
SDL_Rect dRect={dstRect->x,dstRect->y,dstRect->w,scr_char_h};
for (i=0; i<ROWS; i++)
    {
    myFont_render_str(&myInfoFont, gInfoRenderer, &dRect, buf[i], COLUMNS);
    dRect.y += scr_char_h;
    }

}


static void
myDisplay_clear()
{
if (!myDisplayRunning)
    return;
// vim is using accumulative drawing. cann't clear every frame.
//SDL_RenderClear(gDisplayRenderer);
}

static void
myDisplay_init(int w, int h)
{

/*
gDisplay= 
    SDL_CreateWindow( "SDLdisplay", 
            SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
            w,h , 
            SDL_WINDOW_OPENGL | SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE);
if (gDisplay==NULL)
    fnError("error creating window gDisplay");
gDisplayRenderer=SDL_CreateRenderer(gDisplay, -1, 
         SDL_RENDERER_ACCELERATED);
if (gDisplayRenderer==NULL)
    fnError("error creating gDisplayRenderer");
SDL_SetRenderDrawColor(gDisplayRenderer, 0, 20, 10, 0xff);
*/
myDisplayRunning=1;


gDisplayTarget0_surf = SDL_CreateRGBSurface(0, w, h, 32, 0,0,0,0);
gDisplayTarget1_surf = SDL_CreateRGBSurface(0, w, h, 32, 0,0,0,0);
gDisplayTargetCur_surf = gDisplayTarget0_surf;

sKeyboardState=SDL_GetKeyboardState(NULL);

send_textarea_resize_to_adapter(DISPLAY_W, DISPLAY_H);
// The following redraw will clear the intro message. 
// Redraw is not necessary here.
//send_redraw_to_adapter();
}

static void
myDisplay_update()
{
/*
if (myDisplayRunning==0)
    SDL_DestroyWindow(gDisplay);
    */
}

static void
myDisplay_on_event(SDL_Event e)
{
extern void send_keydown_to_adapter(SDL_KeyboardEvent);
extern void send_killfocus_to_adapter();
extern void send_setfocus_to_adapter();
extern void send_redraw_to_adapter();
extern void send_quitall_to_adapter();

if (!myDisplayRunning)
    return;
if (e.type==SDL_QUIT)
    mySDLrunning=0;
if (e.type==SDL_WINDOWEVENT)
    {
    switch (e.window.event)
        {
        case SDL_WINDOWEVENT_CLOSE:
            // wait for the quit command from vim.
            send_quitall_to_adapter();
            break;
        case SDL_WINDOWEVENT_FOCUS_GAINED:
            send_setfocus_to_adapter();
            break;
        case SDL_WINDOWEVENT_FOCUS_LOST:
            send_killfocus_to_adapter();
            break;
        case SDL_WINDOWEVENT_RESIZED:
            send_textarea_resize_to_adapter(e.window.data1, e.window.data2);
            // vim only draws the changed part.
            // need to inform vim to redraw the whole window.
            send_redraw_to_adapter();
            break;
        }
    return;
    }
// not a window-manager message
switch (e.type)
    {
    case SDL_KEYDOWN:
        send_keydown_to_adapter(e.key);
        break;
    case SDL_MOUSEBUTTONDOWN:
        send_mousebuttondown_to_adapter(e.button);
        break;
    case SDL_MOUSEBUTTONUP:
    case SDL_MOUSEMOTION:
        send_mousebuttonup_or_move_to_adapter(e);
        break;
    }
}
#if 0
#define display_save_color() \
     \
    SDL_Color color_save; \
    SDL_GetRenderDrawColor(gDisplayRenderer, &color_save.r, \
         &color_save.g, &color_save.b, &color_save.a); 

#define display_restore_color() \
    { \
    SDL_SetRenderDrawColor(gDisplayRenderer, color_save.r, \
        color_save.g, color_save.b, color_save.a);  \
    }
#define display_use_color(cc) \
    { \
    SDL_SetRenderDrawColor(gDisplayRenderer, cc.r, \
        cc.g, cc.b, cc.a);  \
    }
#else
#define display_save_color() ((void)0)
#define display_restore_color() ((void)0)
#define display_use_color(cc) ((void)0)
#endif

// assume the string fits into one line.
static void
draw_string_char_by_char(TTF_Font* font, int x, int y, const char* s)
{
static char buf[20];
int i=0;
int byte_len=strlen(s);
while (i<byte_len)
    {
    int c=utf_ptr2char((char_u*)s+i);
    int ncells = utf_char2cells(c);
    int cl=utf_ptr2len((char_u*)s+i);
    int j;
    for (j=0; j<cl; j++)
        buf[j]=s[i+j];
    buf[cl]=0;
    i+=cl;
    // buf is prepared then do rendering.
    SDL_Surface *sf=TTF_RenderUTF8_Blended(gDisplayFontCur
            , buf, //color_velvet);
                gDisplayColorFG);
    if (sf==NULL)
        fnWarn("error rendering text");
    SDL_Rect dstRect={x,y,ncells*gDisplay_cell_w, gDisplay_cell_h};
    SDL_BlitSurface(sf, NULL, gDisplayTargetCur_surf, &dstRect);
    SDL_FreeSurface(sf);
    x+=ncells*gDisplay_cell_w;
    }
}

static void
_on_disp_textout(disp_task_textout_t* textout)
{
static char buf[200];
double x,y,w,h;
int len;
int maxlen=200;
display_extract_textout(textout,
        &x, &y, &w, &h,
        buf, &len, maxlen); 
display_task_cleanup((disp_task_t*)textout);
int sx=round(x*DISPLAY_W),
    sy=round(y*DISPLAY_H),
    sw=round(w*DISPLAY_W),
    sh=round(h*DISPLAY_H);

/*
info_push_messagef("textout cells=%d", //num_utf8_chars(buf, buf+len));
    mb_string2cells(buf, len));
    */

SDL_Rect dstRect=
    {sx,sy,sw,sh};
    //myFont_render_str(&myDisplayFont, gDisplayRenderer, &dstRect,
            //buf, len);
if (gDisplayFontCur==NULL)
    fnWarn("null font");

// bold/italic font need to be drawn char by char because 
// it will contain extra paddings between chars if i directly
// pass the whole string to TTF_Render*
if (gDisplayFontCur!=gDisplayFontNormal[0] 
        //&& gDisplayFontCur!=gDisplayFontWide[0]
        ) 
    {
    draw_string_char_by_char(gDisplayFontCur, sx, sy, buf);
    return;
    }
else
    {
    SDL_Surface *sf=TTF_RenderUTF8_Blended(gDisplayFontCur
            , buf, //color_velvet);
                gDisplayColorFG);
    if (sf==NULL)
        fnWarn("error rendering text");

    SDL_BlitSurface(sf, NULL, gDisplayTargetCur_surf, &dstRect);
    SDL_FreeSurface(sf);
    //display_restore_color();
    }
}


#define init_display_rect(rect, frect)     \
    rect=(SDL_Rect) { \
    round((frect).x * DISPLAY_W), \
    round((frect).y * DISPLAY_H), \
    round((frect).w * DISPLAY_W), \
    round((frect).h * DISPLAY_H), \
    } ;

static void
_on_disp_scroll(disp_task_scroll_t* scroll)
{
int dist=round(scroll->distance*DISPLAY_H);
int adist = abs(dist);
SDL_Rect srcRect;
init_display_rect(srcRect, scroll->scroll_rect);
SDL_Rect dstRect=srcRect,
    no_change_rect={0,0,DISPLAY_W, srcRect.y};

if (dist>0) // move an area down
    {
    dstRect.y += adist;
    dstRect.h -= adist;
    srcRect.h -=adist;
    }
else // move an area up
    {
    srcRect.y += adist;
    srcRect.h -= adist;
    dstRect.h -= adist;
    }
/*
if (dstRect.y < no_change_rect.y+no_change_rect.h)
    dstRect.y = no_change_rect.y+ no_change_rect.h;
    */
// actual blitting
// scroll surfaces
SDL_Surface *s_from= gDisplayTargetCur_surf,
            *s_to= (gDisplayTargetCur_surf== gDisplayTarget0_surf)?
                gDisplayTarget1_surf : gDisplayTarget0_surf;
// necessary to keep what's already on screen
SDL_BlitSurface(s_from, NULL, s_to, NULL); 
SDL_BlitSurface(s_from, &srcRect, s_to, &dstRect);
SDL_BlitSurface(s_from, &no_change_rect, s_to, &no_change_rect);
gDisplayTargetCur_surf = s_to;
}

static void
_on_disp_hollowcursor(disp_task_hollowcursor_t* hollow)
{
display_save_color();
SDL_Rect rect;
init_display_rect(rect, hollow->rect);
display_use_color(gDisplayColorFG);
//SDL_RenderDrawRect(gDisplayRenderer, &rect);
display_restore_color();

// draw onto surface
SDL_Surface *s_to = gDisplayTargetCur_surf;
SDL_FillRect(s_to, &rect, SDL_MapRGBA(s_to->format, 
            gDisplayColorFG.r,  gDisplayColorFG.g,  gDisplayColorFG.b,
            gDisplayColorFG.a));
}

static void
_on_disp_partcursor(disp_task_partcursor_t* part)
{
display_save_color();
display_use_color(gDisplayColorFG);
SDL_Rect rect;
init_display_rect(rect, part->rect);
//SDL_RenderFillRect(gDisplayRenderer, &rect);
display_restore_color();

// draw onto surface.
SDL_Surface *s_to = gDisplayTargetCur_surf;
SDL_FillRect(s_to, &rect, SDL_MapRGBA(s_to->format, 
            gDisplayColorFG.r,  gDisplayColorFG.g,  gDisplayColorFG.b,
            gDisplayColorFG.a));
}

static void
_on_disp_setcolor(disp_task_setcolor_t* setcolor)
{
switch (setcolor->which)
    {
    case DISP_TASK_SETCOLORFG:
        gDisplayColorFG=setcolor->color;
        break;
    case DISP_TASK_SETCOLORBG:
        gDisplayColorBG=setcolor->color;
        break;
    case DISP_TASK_SETCOLORSP:
        gDisplayColorSP=setcolor->color;
        break;
    default:
        fnWarn("unknown set color");
    }
}

static void
_on_disp_invertrect(disp_task_invertrect_t* irect)
{
// do nothing for now.
fnWarn("disp_task invertrect");
#if 0
info_push_message("invertrect");
display_save_color();
display_use_color(gDisplayColorBG);
SDL_Rect rect;
init_display_rect(rect, irect->rect);
SDL_RenderFillRect(gDisplayRenderer, &rect);
display_restore_color();

// draw onto surface.
SDL_Surface *s_to = gDisplayTargetCur_surf;
SDL_FillRect(s_to, &rect, SDL_MapRGBA(s_to->format, 
            gDisplayColorFG.r,  gDisplayColorFG.g,  gDisplayColorFG.b,
            gDisplayColorFG.a));
#endif // 0
}

static const char*
color2str(SDL_Color color)
{
static char s[20];
snprintf(s, 20, "%02x%02x%02x", color.r, color.g, color.b);
return s;
}
static void
_on_disp_clearrect(disp_task_clearrect_t* clrect)
{
extern long Columns;
display_save_color();
display_use_color(gDisplayColorBG);
SDL_Rect rect;
init_display_rect(rect, clrect->rect);

/*
int wcells=round(clrect->rect.w * Columns);
int hcells=round(clrect->rect.h * Rows);
info_push_messagef("clear w=%d h=%d color=%s", wcells, hcells,
        color2str(gDisplayColorBG));
*/

//SDL_RenderFillRect(gDisplayRenderer, &rect);
display_restore_color();

// draw onto sdl
SDL_Surface *s_to = gDisplayTargetCur_surf;
SDL_FillRect(s_to, &rect, SDL_MapRGBA(s_to->format, 
            gDisplayColorBG.r,  gDisplayColorBG.g,  gDisplayColorBG.b,
            gDisplayColorBG.a));
}

static void
_on_disp_setfont(disp_task_setfont_t* setfont)
{
int flags= setfont->flags;
if (flags>3 || flags <0)
    fnWarnf("setting font flags=%d", setfont->flags);
switch (setfont->which)
    {
    case DISP_TASK_SETFONTNORMAL:
        gDisplayFontCur = gDisplayFontNormal[flags];
        break;
    case DISP_TASK_SETFONTWIDE:
        gDisplayFontCur = gFont_zh; //gDisplayFontWide[0];
        break;
    default:
        fnWarnf("unknown font in " __FILE__);
        break;
    }
}

static void
_on_disp_setcellsize(disp_task_setcellsize_t* setcellsize)
{

int w=setcellsize->cell_width * Columns,
    h=setcellsize->cell_height * Rows;
info_push_messagef("resz w=%d, h=%d", w, h);

gDisplay_width=w;
gDisplay_height=h;
gDisplay_cell_w = setcellsize->cell_width;
gDisplay_cell_h = setcellsize->cell_height;
SDL_Rect clip={0,0,w,h};
/*
SDL_SetWindowSize(gDisplay, w, h);
// clear colors to bg.
// hope to restore render context,
// because of the bug with d3dx9 window resizing.
display_use_color(gDisplayColorBG);
SDL_RenderClear(gDisplayRenderer);
*/

// resize surfaces.
// do not clear them for now.
SDL_FreeSurface(gDisplayTarget0_surf);
SDL_FreeSurface(gDisplayTarget1_surf);
gDisplayTarget0_surf = SDL_CreateRGBSurface(0, w, h, 32, 0,0,0,0);
gDisplayTarget1_surf = SDL_CreateRGBSurface(0, w, h, 32, 0,0,0,0);
SDL_SetClipRect(gDisplayTarget0_surf, &clip);
SDL_SetClipRect(gDisplayTarget1_surf, &clip);
gDisplayTargetCur_surf = gDisplayTarget0_surf;
}

static void
_on_disp_quitvim()
{
//SDL_DestroyWindow(gDisplay);
myDisplayRunning=0;
mySDLrunning=0;
}

static void
_on_disp_drawline(disp_task_drawline_t* drawline)
{
int x1=round(drawline->x1 * DISPLAY_W);
int y1=round(drawline->y1 * DISPLAY_W);
int x2=round(drawline->x2 * DISPLAY_W);
int y2=round(drawline->y2 * DISPLAY_W);
// draw onto surface.
SDL_Rect rect={x1, y1, x2-x1, 1};
SDL_FillRect(gDisplayTargetCur_surf, &rect, 
        SDL_MapRGBA(gDisplayTargetCur_surf->format, 
            gDisplayColorFG.r,  gDisplayColorFG.g,  gDisplayColorFG.b,
            gDisplayColorFG.a));
}

static void
_on_disp_beep()
{
// TODO
//fnWarn("beep!!!");
}
static void
draw_pixel32_unsafe(SDL_Surface *surface, int x, int y, SDL_Color color)
{
Uint8* pixel=(Uint8*)surface->pixels;
pixel += y * surface->pitch + x*sizeof(Uint32);
Uint32* p=(Uint32*) pixel;
*p=SDL_MapRGBA(surface->format, color.r, color.g, color.b, color.a);
}
static void
_on_disp_undercurl(disp_task_undercurl_t* undercurl)
{
// i don't know how to tell vim to draw a undercurl
info_push_messagef("undercurl not tested t%d", SDL_GetTicks());

int x=round(undercurl->x * DISPLAY_W);
int w=round(undercurl->w * DISPLAY_W);
int y=round(undercurl->y * DISPLAY_H);
// TODO
// for now it is same as under line.
/*
SDL_Rect rect={x, y, w, 1};
SDL_FillRect(gDisplayTargetCur_surf, &rect, 
        SDL_MapRGBA(gDisplayTargetCur_surf->format, 
            gDisplayColorSP.r,  gDisplayColorSP.g,  gDisplayColorSP.b,
            gDisplayColorSP.a));
            */

static const int	val[8] = {1, 0, 0, 0, 1, 2, 2, 2 };
int i;
if (SDL_MUSTLOCK(gDisplayTargetCur_surf))
    SDL_LockSurface(gDisplayTargetCur_surf);
for (i=0; i < w; ++x)
    {
    int offset = val[x % 8];
    draw_pixel32_unsafe(gDisplayTargetCur_surf, x, y - offset, gDisplayColorSP);
    }
if (SDL_MUSTLOCK(gDisplayTargetCur_surf))
    SDL_UnlockSurface(gDisplayTargetCur_surf);
}

static void
_on_disp_require_esc()
{
extern void send_esc_to_adapter();
send_esc_to_adapter();
}

static void
myDisplay_draw()
{
//iVim_log("begin myDisplay_draw");
    /*
    int tt=SDL_GetTicks(),
        ch=(tt/50)%256;
    SDL_Rect ch_rect={0,100,100,100};
    SDL_RenderCopy(gDisplayRenderer, myDisplayFont.texture, 
            &myDisplayFont.rect[ch], &ch_rect);
            */
    // poll tasks from display queue.

    disp_task_t task;
    while (display_poll_task(&task))
        {
        switch (task.type)
            {
            case DISP_TASK_TEXTOUT:
                //iVim_log("_on_disp_textout");
                _on_disp_textout(&task.textout);
                //iVim_log("/");
                break;
            case DISP_TASK_SCROLL:
                //iVim_log("_on_disp_scroll");
                _on_disp_scroll(&task.scroll);
                //iVim_log("/");
                break;
            case DISP_TASK_HOLLOWCURSOR:
                //iVim_log("_on_disp_hollowcursor");
                _on_disp_hollowcursor(&task.hollowcursor);
                //iVim_log("/");
                break;
            case DISP_TASK_PARTCURSOR:
                //iVim_log("_on_disp_partcursor");
                _on_disp_partcursor(&task.partcursor);
                //iVim_log("/");
                break;
            case DISP_TASK_SETCOLOR:
                //iVim_log("_on_disp_setcolor");
                _on_disp_setcolor(&task.setcolor);
                //iVim_log("/");
                break;
            case DISP_TASK_INVERTRECT:
                //iVim_log("_on_disp_invertrect");
                _on_disp_invertrect(&task.invertrect);
                //iVim_log("/");
                break;
            case DISP_TASK_CLEARRECT:
                //iVim_log("_on_disp_clearrect");
                _on_disp_clearrect(&task.clearrect);
                //iVim_log("/");
                break;
            case DISP_TASK_SETFONT:
                //iVim_log("_on_disp_setfont");
                _on_disp_setfont(&task.setfont);
                //iVim_log("/");
                break;
            case DISP_TASK_SETCELLSIZE:
                //iVim_log("_on_disp_setcellsize");
                _on_disp_setcellsize(&task.setcellsize);
                //iVim_log("/");
                break;
            case DISP_TASK_QUITVIM:
                //iVim_log("_on_disp_quitvim");
                _on_disp_quitvim();
                //iVim_log("/");
                break;
            case DISP_TASK_DRAWLINE:
                //iVim_log("_on_disp_drawline");
                _on_disp_drawline(&task.drawline);
                //iVim_log("/");
                break;
            case DISP_TASK_BEEP:
                //iVim_log("_on_disp_beep");
                _on_disp_beep();
                //iVim_log("/");
                break;
            case DISP_TASK_UNDERCURL:
                //iVim_log("_on_disp_undercurl");
                _on_disp_undercurl(&task.undercurl);
                //iVim_log("/");
                break;
            case DISP_TASK_FLUSH:
                // do nothing
                break;
            case DISP_TASK_REQUIREESC:
                //iVim_log("_on_disp_require_esc");
                _on_disp_require_esc();
                //iVim_log("/");
                break;
            default:
                fnWarn("unknown display task");
            }
        }

    //SDL_Rect rect={0,0,DISPLAY_W, DISPLAY_H};
    //SDL_SetRenderTarget(gDisplayRenderer, NULL);
    //
    // the use of rect is necessary because the window and texture have
    // been resized. using NULL will cause the wrong clip rectangle to
    // be used (target rect as the rectangle at window creation, source
    // rect as the whole texture).
    /*
    SDL_Texture *tt=
        SDL_CreateTextureFromSurface(gDisplayRenderer, gDisplayTargetCur_surf);
    SDL_RenderCopy(gDisplayRenderer, tt, &rect, &rect);

    //SDL_RenderCopy(gDisplayRenderer, gDisplayCurTarget, &rect, &rect);
    SDL_RenderPresent(gDisplayRenderer);

    SDL_DestroyTexture(tt);
    */
    //iVim_log("end myDisplay_draw");
}

/*
static void 
myDisplay_present()
{
if (!myDisplayRunning)
    return;
SDL_RenderPresent(gDisplayRenderer);
}
*/

static void
myInfo_init(int w, int h)
{
gInfo = 
    SDL_CreateWindow( "SDL Tutorial", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,w,h, SDL_WINDOW_HIDDEN );
if (gInfo==NULL)
    fnError("gInfo create window failed"); //SDL_GetError());
gInfoRenderer=SDL_CreateRenderer(gInfo, -1, SDL_RENDERER_ACCELERATED);
if(gInfoRenderer==NULL)
    fnError("gInfo renderer init error"); //SDL_GetError());
}

static void
myInfo_clear()
{
    SDL_SetRenderDrawColor(gInfoRenderer,135,206,235,0xff);
    SDL_RenderClear(gInfoRenderer);
}
static void
myInfo_draw()
{
    SDL_Rect titleRect={0,0,200, 40};
    SDL_Rect helloRect={0,0,20,20};
    // FIXME!! the following line of code causes crash
    // dont know why. Maybe text_hello is broken.
    SDL_RenderCopy(gInfoRenderer,text_hello, NULL, &titleRect);
    int tt=SDL_GetTicks(),
        ch=(tt/50)%256;
    SDL_Rect ch_rect={0,100,100,100};
    SDL_RenderCopy(gInfoRenderer, myInfoFont.texture, &myInfoFont.rect[ch], &ch_rect);
    
    SDL_Rect infoRect={0,40, 400,400};
    mySDL_show_info(&infoRect);
}
static void
myInfo_present()
{
    SDL_RenderPresent(gInfoRenderer);
}
static void
myInfo_on_event(SDL_Event e)
{
if (e.type==SDL_QUIT)
    mySDLrunning=0;
}

void * fn_vim_thread(void *ud)
{

#ifdef DYNAMIC_GETTEXT
//#error dynamic_gettext
    /* Initialize gettext library */
    dyn_libintl_init(NULL);
#endif

    extern int VimMain(int argc, char** argv);
    int sdl_argc=1;
    char* sdl_argv[]={"gvim", NULL};

    VimMain(sdl_argc, sdl_argv);

    disp_task_quitvim_t quitvim = {DISP_TASK_QUITVIM};
    display_push_task((disp_task_t*)&quitvim);

    return 0;
}

int iVim_init(int w, int h, int argc, char** argv)
{
// vim thread mainloop is started.
pthread_create(&thread_vim, 0, &fn_vim_thread, (void*)0);
mySDL_init(w,h);
return 0;
}

void iVim_onEvent(SDL_Event evnt)
{
iVim_log("iVim_onEvent");
// i need to be sure that this event should be processed by vim,
// which is to say, its source window is the vim display window,
// or the specific adjusted area within the window.
myDisplay_on_event(evnt);
iVim_log("/");
}

const SDL_Surface* iVim_getDisplaySurface()
{
return gDisplayTargetCur_surf;
}

void iVim_getDisplaySize(int* pw, int* ph)
{
if (pw!=NULL)
    *pw=DISPLAY_W;
if (ph!=NULL)
    *ph=DISPLAY_H;
}

int iVim_quit()
{
SDL_Event evnt;
evnt.type=SDL_QUIT;
iVim_onEvent(evnt);
pthread_join(thread_vim, NULL);
return 0;
}

int iVim_flush()
{
iVim_log("iVim_flush");
int dirty=display_has_task();
//iVim_log("iVim_flush_myDisplay");
myDisplay_draw();
//iVim_log("/");

//iVim_log("iVim_flush_myInfo");
myInfo_clear();
myInfo_draw();
if (s_info_shown)
    myInfo_present();
//iVim_log("/");

iVim_log("/");
return dirty;
}

int iVim_running()
{
return mySDLrunning;
}

void iVim_showDebugWindow(int shown)
{
if (shown)
    SDL_ShowWindow(gInfo);
else
    SDL_HideWindow(gInfo);
s_info_shown=shown;
}


void iVim_setLogger( void (*f)(const char*) )
{
s_logger=f;
}

void iVim_log(const char* msg)
{
// s_logger 's responsbility to perform locking.
if (s_logger) s_logger(msg);
}



#include "end_ns_vim.h"

