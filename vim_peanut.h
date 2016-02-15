#ifndef _VIM_PEANUT_H_
#define _VIM_PEANUT_H_

class Peanut
{
private:
public:
    static int init(int w, int h);
    static int onEvent(SDL_Event evnt);
    static const SDL_Surface* getSurface();
    static void flush();
};

#endif // _VIM_PEANUT_H_

