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
 */
#include <math.h>
#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include "adapter_sdl2.h"
//#include "globals.h"
#include "vim.h"

typedef struct tex_font
{
    // one line of '.' '\1' '\2' ....'\255' '\0'
    SDL_Texture *texture;
    SDL_Rect rect[256];
    int char_w, char_h;
} tex_font_t;

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


static SDL_Window* gDisplay=NULL;
static int gDisplay_width = 600,
           gDisplay_height = 410;
static int gDisplay_cell_w= 0,
           gDisplay_cell_h= 0;
static int gDisplay_columns = -1,
    gDisplay_rows = -1;
#define DISPLAY_W gDisplay_width
#define DISPLAY_H gDisplay_height
static SDL_Renderer* gDisplayRenderer=NULL;
static SDL_Surface 
    *gDisplayTarget0_surf = NULL,
    *gDisplayTarget1_surf = NULL,
    *gDisplayTargetCur_surf = NULL;
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

static void myDisplay_init(int, int);
static void myInfo_init(int, int);

static void 
mySDL_init(int w, int h)
{
int status=SDL_Init(SDL_INIT_VIDEO);
if (status<0)
    fnError2("sdl-init-video failed.", SDL_GetError());


// init ttf
TTF_Init();
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
init_font(&myDisplayFont, gDisplayRenderer, gFont, &color_black);

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
#define COLUMNS 30
static char buf[ROWS][COLUMNS];
/*
SDL_Surface *tmp_info_window=
    SDL_CreateRGBSurface(0,500, 500, 32, 0,0,0,0);
    */
int len, i, j;
int lines=info_message_number();
if (lines>=ROWS)
    {
    for (i=0; i<(lines-ROWS); i++)
        info_poll_message(NULL, NULL, 0);
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
extern void send_setcellsize_to_adapter(int, int);

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
myDisplayRunning=1;


gDisplayTarget0_surf = SDL_CreateRGBSurface(0, w, h, 32, 0,0,0,0);
gDisplayTarget1_surf = SDL_CreateRGBSurface(0, w, h, 32, 0,0,0,0);
gDisplayTargetCur_surf = gDisplayTarget0_surf;

sKeyboardState=SDL_GetKeyboardState(NULL);

send_textarea_resize_to_adapter(DISPLAY_W, DISPLAY_H);
send_redraw_to_adapter();
}

static void
myDisplay_update()
{
if (myDisplayRunning==0)
    SDL_DestroyWindow(gDisplay);
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

// assume the string fits into one line.
static void
draw_string_char_by_char(TTF_Font* font, int x, int y, const char* s)
{
static char buf[20];
int i=0;
int byte_len=strlen(s);
while (i<byte_len)
    {
    int c=utf_ptr2char(s+i);
    int ncells = utf_char2cells(c);
    int cl=utf_ptr2len(s+i);
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
display_task_cleanup(textout);
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
SDL_RenderDrawRect(gDisplayRenderer, &rect);
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
SDL_RenderFillRect(gDisplayRenderer, &rect);
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

SDL_RenderFillRect(gDisplayRenderer, &rect);
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
gDisplay_width=w;
gDisplay_height=h;
gDisplay_cell_w = setcellsize->cell_width;
gDisplay_cell_h = setcellsize->cell_height;
SDL_SetWindowSize(gDisplay, w, h);
// clear colors to bg.
SDL_Rect clip={0,0,w,h};
// hope to restore render context,
// because of the bug with d3dx9 window resizing.
display_use_color(gDisplayColorBG);
SDL_RenderClear(gDisplayRenderer);

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
SDL_DestroyWindow(gDisplay);
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
myDisplay_draw()
{
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
                _on_disp_textout(&task.textout);
                break;
            case DISP_TASK_SCROLL:
                _on_disp_scroll(&task.scroll);
                break;
            case DISP_TASK_HOLLOWCURSOR:
                _on_disp_hollowcursor(&task.hollowcursor);
                break;
            case DISP_TASK_PARTCURSOR:
                _on_disp_partcursor(&task.partcursor);
                break;
            case DISP_TASK_SETCOLOR:
                _on_disp_setcolor(&task.setcolor);
                break;
            case DISP_TASK_INVERTRECT:
                _on_disp_invertrect(&task.invertrect);
                break;
            case DISP_TASK_CLEARRECT:
                _on_disp_clearrect(&task.clearrect);
                break;
            case DISP_TASK_SETFONT:
                _on_disp_setfont(&task.setfont);
                break;
            case DISP_TASK_SETCELLSIZE:
                _on_disp_setcellsize(&task.setcellsize);
                break;
            case DISP_TASK_QUITVIM:
                _on_disp_quitvim();
                break;
            case DISP_TASK_DRAWLINE:
                _on_disp_drawline(&task.drawline);
                break;
            case DISP_TASK_BEEP:
                _on_disp_beep();
                break;
            case DISP_TASK_UNDERCURL:
                _on_disp_undercurl(&task.undercurl);
                break;
            case DISP_TASK_FLUSH:
                // do nothing
                break;
            default:
                fnWarn("unknown display task");
            }
        }

    SDL_Rect rect={0,0,DISPLAY_W, DISPLAY_H};
    SDL_SetRenderTarget(gDisplayRenderer, NULL);
    // the use of rect is necessary because the window and texture have
    // been resized. using NULL will cause the wrong clip rectangle to
    // be used (target rect as the rectangle at window creation, source
    // rect as the whole texture).
    SDL_Texture *tt=
        SDL_CreateTextureFromSurface(gDisplayRenderer, gDisplayTargetCur_surf);
    SDL_RenderCopy(gDisplayRenderer, tt, &rect, &rect);

    //SDL_RenderCopy(gDisplayRenderer, gDisplayCurTarget, &rect, &rect);
    SDL_RenderPresent(gDisplayRenderer);

    SDL_DestroyTexture(tt);
}

static void 
myDisplay_present()
{
if (!myDisplayRunning)
    return;
SDL_RenderPresent(gDisplayRenderer);
}

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


static void 
mySDL_main_loop()
{
SDL_Event evnt;

//SDL_RenderSetLogicalSize(gInfoRenderer, 1000, 1000);
/*
SDL_RenderSetScale(gInfoRenderer, 0.5, 1.5);
SDL_Rect vpRect={100, 0, 250,250};
SDL_RenderSetViewport(gInfoRenderer, &vpRect);
*/
for (;;)
    {
    if (!mySDLrunning) break;
    if (myDisplayRunning)
        myDisplay_clear();
    myInfo_clear();
    while(SDL_PollEvent(&evnt))
        {
        if (evnt.type==SDL_QUIT) 
            mySDLrunning=0;
        if (myEventSourceWindowID(evnt)==SDL_GetWindowID(gDisplay))
            {
            if (myDisplayRunning)
                myDisplay_on_event(evnt);
            }
        else
            {
            myInfo_on_event(evnt);
            }
        }

    myInfo_draw();
    if (myDisplayRunning)
        myDisplay_draw();

    SDL_Delay(50);
    if (myDisplayRunning)
        myDisplay_present();
    myInfo_present();
    }
}

void 
mySDL_dosomething()
{
mySDL_init(500, 500);
mySDL_main_loop();
SDL_DestroyWindow(gInfo);
}

