# vim_sdl2_port
A port of vim7.4 to run on Simple DirectMedia Layer version 2(SDL2), and to be
embeded into SDL2 applications.

Licensed under Vim License (see uganda.txt for details).

For information of vim, see README_vim.txt

In this port, vim is run in a separate thread so it will not block the whole 
application. Many features relying on gui widgets are removed to make it easier
to do this.

Features removed are:
    FEAT_PRINTER
    FEAT_MENU
    FEAT_TEAROFF
    FEAT_TOOLBAR
    FEAT_GUI_TABLINE
    FEAT_BROWSE
    FEAT_GUI_DIALOG
    FIND_REPLACE_DIALOG
    FEAT_MOUSESHAPE
    FEAT_SIGN_ICONS
    FEAT_BEVAL

Note that text mode tabline and dialog still works.

Building

Currently compiles and runs in win32 MINGW.

build:
  make -f Make_ming.mak

run:
  ./gvim

Requirements

You need to run the command "set encoding=utf-8", by hand or via a .vimrc file.

