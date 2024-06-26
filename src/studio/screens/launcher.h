// MIT License

// Copyright (c) 2017 Vadim Grigoruk @nesbox // grigoruk@gmail.com

// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:

// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.

// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

#pragma once

#include "studio/studio.h"

enum Screens
{
    SCREEN_MAIN,
    SCREEN_SETTINGS,
    SCREEN_LOCAL_SELECT,
    SCREEN_LOCAL_ALL,
    SCREEN_LOCAL_RECENT,
    SCREEN_LOCAL_FAVORITES,
    SCREEN_BROWSE_WEB
};

typedef struct Launcher Launcher;

struct Launcher
{
    Studio* studio;
    tic_mem* tic;
    struct tic_fs* fs;
    struct tic_net* net;
    struct Console* console;
    enum Screens screen;

    bool init;
    bool loading;
    s32 ticks;

    struct
    {
        s32 pos;
        s32 target;
        struct SurfItem* items;
        s32 count;
        s32 column;
    } menu;

    struct
    {
        struct
        {
            s32 topBarY;
            s32 bottomBarY;
            s32 menuX;
            s32 menuHeight;
            s32 coverFade;
            s32 pos;
        } val;

        Movie* movie;

        Movie idle;
        Movie show;
        Movie play;
        Movie move;

        struct
        {
            Movie show;
            Movie hide;
        } gotodir;

        struct
        {
            Movie show;
            Movie hide;
        } goback;

    } anim;

    void(*tick)(Launcher* launcher);
    void(*resume)(Launcher* launcher);
};

void initLauncher(Launcher* launcher, Studio* studio, struct Console* console);
void freeLauncher(Launcher* launcher);
