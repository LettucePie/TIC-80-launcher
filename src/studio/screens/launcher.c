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

#include "launcher.h"
#include "studio/fs.h"
#include "studio/net.h"
#include "console.h"
#include "menu.h"
#include "ext/gif.h"
#include "ext/png.h"

#if defined(TIC80_PRO)
#include "studio/project.h"
#else
#include "cart.h"
#endif

#include <string.h>

#define MAIN_OFFSET 4
#define MENU_HEIGHT 10
#define ANIM 10
#define PAGE 5
#define COVER_WIDTH 140
#define COVER_HEIGHT 116
#define COVER_Y 5
#define COVER_X (TIC80_WIDTH - COVER_WIDTH - COVER_Y)
#define COVER_FADEIN 96
#define COVER_FADEOUT 256
#define CAN_OPEN_URL (__TIC_WINDOWS__ || __TIC_LINUX__ || __TIC_MACOSX__ || __TIC_ANDROID__)

static const char* PngExt = PNG_EXT;

typedef struct SurfItem SurfItem;

struct SurfItem
{
    char* label;
    char* name;
    char* hash;
    s32 id;
    tic_screen* cover;

    tic_palette* palette;

    bool coverLoading;
    bool dir;
    bool project;
};

typedef struct
{
    SurfItem* items;
    s32 count;
    Launcher* launcher;
    fs_done_callback done;
    void* data;
} AddMenuItemData;

static void drawTopToolbar(Launcher* launcher, s32 x, s32 y)
{
    tic_mem* tic = launcher->tic;

    enum{Height = MENU_HEIGHT};

    tic_api_rect(tic, x, y, TIC80_WIDTH, Height, tic_color_grey);
    tic_api_rect(tic, x, y + Height, TIC80_WIDTH, 1, tic_color_black);

    {
        static const char Label[] = "TIC-80 SURF";
        s32 xl = x + MAIN_OFFSET;
        s32 yl = y + (Height - TIC_FONT_HEIGHT)/2;
        tic_api_print(tic, Label, xl, yl+1, tic_color_black, true, 1, false);
        tic_api_print(tic, Label, xl, yl, tic_color_white, true, 1, false);
    }

    enum{Gap = 10, TipX = 150, SelectWidth = 54};

    u8 colorkey = 0;
    tiles2ram(tic->ram, &getConfig(launcher->studio)->cart->bank0.tiles);
    tic_api_spr(tic, 12, TipX, y+1, 1, 1, &colorkey, 1, 1, tic_no_flip, tic_no_rotate);
    {
        static const char Label[] = "SELECT";
        tic_api_print(tic, Label, TipX + Gap, y+3, tic_color_black, true, 1, false);
        tic_api_print(tic, Label, TipX + Gap, y+2, tic_color_white, true, 1, false);
    }

    tic_api_spr(tic, 13, TipX + SelectWidth, y + 1, 1, 1, &colorkey, 1, 1, tic_no_flip, tic_no_rotate);
    {
        static const char Label[] = "BACK";
        tic_api_print(tic, Label, TipX + Gap + SelectWidth, y +3, tic_color_black, true, 1, false);
        tic_api_print(tic, Label, TipX + Gap + SelectWidth, y +2, tic_color_white, true, 1, false);
    }
}

static SurfItem* getMenuItem(Launcher* launcher)
{
    //printf("\nlauncher.c getMenuItem Called");
    return &launcher->menu.items[launcher->menu.pos];
}

static void drawBottomToolbar(Launcher* launcher, s32 x, s32 y)
{
    tic_mem* tic = launcher->tic;

    enum{Height = MENU_HEIGHT};

    tic_api_rect(tic, x, y, TIC80_WIDTH, Height, tic_color_grey);
    tic_api_rect(tic, x, y + Height, TIC80_WIDTH, 1, tic_color_black);
    {
        char label[TICNAME_MAX + 1];
        char dir[TICNAME_MAX];
        tic_fs_dir(launcher->fs, dir);

        sprintf(label, "/%s", dir);
        s32 xl = x + MAIN_OFFSET;
        s32 yl = y + (Height - TIC_FONT_HEIGHT)/2;
        tic_api_print(tic, label, xl, yl+1, tic_color_black, true, 1, false);
        tic_api_print(tic, label, xl, yl, tic_color_white, true, 1, false);
    }

#ifdef CAN_OPEN_URL 

    if(launcher->menu.count > 0 && getMenuItem(launcher)->hash)
    {
        enum{Gap = 10, TipX = 134, SelectWidth = 54};

        u8 colorkey = 0;

        tiles2ram(tic->ram, &getConfig(launcher->studio)->cart->bank0.tiles);
        tic_api_spr(tic, 15, TipX + SelectWidth, y + 1, 1, 1, &colorkey, 1, 1, tic_no_flip, tic_no_rotate);
        {
            static const char Label[] = "WEBSITE";
            tic_api_print(tic, Label, TipX + Gap + SelectWidth, y + 3, tic_color_black, true, 1, false);
            tic_api_print(tic, Label, TipX + Gap + SelectWidth, y + 2, tic_color_white, true, 1, false);
        }
    }
#endif

}

static void drawMenu(Launcher* launcher, s32 x, s32 y)
{
    tic_mem* tic = launcher->tic;

    enum {Height = MENU_HEIGHT};

    tic_api_rect(tic, 0, y + (MENU_HEIGHT - launcher->anim.val.menuHeight) / 2, TIC80_WIDTH, launcher->anim.val.menuHeight, tic_color_red);

    s32 ym = y - launcher->menu.pos * MENU_HEIGHT + (MENU_HEIGHT - TIC_FONT_HEIGHT) / 2 - launcher->anim.val.pos;
    for(s32 i = 0; i < launcher->menu.count; i++, ym += Height)
    {
        const char* name = launcher->menu.items[i].label;

        if (ym > (-(TIC_FONT_HEIGHT + 1)) && ym <= TIC80_HEIGHT) 
        {
            tic_api_print(tic, name, x + MAIN_OFFSET, ym + 1, tic_color_black, false, 1, false);
            tic_api_print(tic, name, x + MAIN_OFFSET, ym, tic_color_white, false, 1, false);
        }
    }
}

static inline void cutExt(char* name, const char* ext)
{
    name[strlen(name)-strlen(ext)] = '\0';
}

static bool addMenuItem(const char* name, const char* title, const char* hash, s32 id, void* ptr, bool dir)
{
    AddMenuItemData* data = (AddMenuItemData*)ptr;

    static const char CartExt[] = CART_EXT;

    if(dir 
        || tic_tool_has_ext(name, CartExt)
        || tic_tool_has_ext(name, PngExt)
#if defined(TIC80_PRO)
        || tic_project_ext(name)
#endif
        )
    {
        data->items = realloc(data->items, sizeof(SurfItem) * ++data->count);
        SurfItem* item = &data->items[data->count-1];

        *item = (SurfItem)
        {
            .name = strdup(name),
            .hash = hash ? strdup(hash) : NULL,
            .id = id,
            .dir = dir,
        };

        if(dir)
        {
            char folder[TICNAME_MAX];
            sprintf(folder, "[%s]", name);
            item->label = strdup(folder);
        }
        else
        {
            item->label = title ? strdup(title) : strdup(name);

            if(tic_tool_has_ext(name, CartExt))
                cutExt(item->label, CartExt);
            else
                item->project = true;
        }
    }

    return true;
}

static s32 itemcmp(const void* a, const void* b)
{
    const SurfItem* item1 = a;
    const SurfItem* item2 = b;

    if(item1->dir != item2->dir)
        return item1->dir ? -1 : 1;
    else if(item1->dir && item2->dir)
        return strcmp(item1->name, item2->name);

    return 0;
}

static void addMenuItemsDone(void* data)
{
    AddMenuItemData* addMenuItemData = data;
    Launcher* launcher = addMenuItemData->launcher;

    launcher->menu.items = addMenuItemData->items;
    launcher->menu.count = addMenuItemData->count;

    if(!tic_fs_ispubdir(launcher->fs))
        qsort(launcher->menu.items, launcher->menu.count, sizeof *launcher->menu.items, itemcmp);

    if (addMenuItemData->done)
        addMenuItemData->done(addMenuItemData->data);

    free(addMenuItemData);

    launcher->loading = false;
}

static void resetMenu(Launcher* launcher)
{
    if(launcher->menu.items)
    {
        for(s32 i = 0; i < launcher->menu.count; i++)
        {
            SurfItem* item = &launcher->menu.items[i];

            free(item->name);

            FREE(item->hash);
            FREE(item->cover);
            FREE(item->label);
            FREE(item->palette);
        }

        free(launcher->menu.items);

        launcher->menu.items = NULL;
        launcher->menu.count = 0;
    }

    launcher->menu.pos = 0;
}

static void updateMenuItemCover(Launcher* launcher, s32 pos, const u8* cover, s32 size)
{
    SurfItem* item = &launcher->menu.items[pos];

    gif_image* image = gif_read_data(cover, size);

    if(image)
    {
        item->cover = malloc(sizeof(tic_screen));
        item->palette = malloc(sizeof(tic_palette));

        if (image->width == TIC80_WIDTH 
            && image->height == TIC80_HEIGHT 
            && image->colors <= TIC_PALETTE_SIZE)
        {
            memcpy(item->palette, image->palette, image->colors * sizeof(tic_rgb));

            for(s32 i = 0; i < TIC80_WIDTH * TIC80_HEIGHT; i++)
                tic_tool_poke4(item->cover->data, i, image->buffer[i]);
        }
        else
        {
            memset(item->cover, 0, sizeof(tic_screen));
            memset(item->palette, 0, sizeof(tic_palette));
        }

        gif_close(image);
    }
}

typedef struct
{
    Launcher* launcher;
    s32 pos;
    char cachePath[TICNAME_MAX];
    char dir[TICNAME_MAX];
} CoverLoadingData;

static void coverLoaded(const net_get_data* netData)
{
    CoverLoadingData* coverLoadingData = netData->calldata;
    Launcher* launcher = coverLoadingData->launcher;

    if (netData->type == net_get_done)
    {
        tic_fs_saveroot(launcher->fs, coverLoadingData->cachePath, netData->done.data, netData->done.size, false);

        char dir[TICNAME_MAX];
        tic_fs_dir(launcher->fs, dir);

        if(strcmp(dir, coverLoadingData->dir) == 0)
            updateMenuItemCover(launcher, coverLoadingData->pos, netData->done.data, netData->done.size);
    }

    switch (netData->type)
    {
    case net_get_done:
    case net_get_error:
        free(coverLoadingData);
        break;
    default: break;
    }
}

static void requestCover(Launcher* launcher, SurfItem* item)
{
    CoverLoadingData coverLoadingData = {launcher, launcher->menu.pos};
    tic_fs_dir(launcher->fs, coverLoadingData.dir);

    const char* hash = item->hash;
    sprintf(coverLoadingData.cachePath, TIC_CACHE "%s.gif", hash);

    {
        s32 size = 0;
        void* data = tic_fs_loadroot(launcher->fs, coverLoadingData.cachePath, &size);

        if (data)
        {
            updateMenuItemCover(launcher, launcher->menu.pos, data, size);
            free(data);
        }
    }

    char path[TICNAME_MAX];
    sprintf(path, "/cart/%s/cover.gif", hash);

    tic_net_get(launcher->net, path, coverLoaded, MOVE(coverLoadingData));
}

static void loadCover(Launcher* launcher)
{
    tic_mem* tic = launcher->tic;
    
    SurfItem* item = getMenuItem(launcher);
    
    if(item->coverLoading)
        return;

    item->coverLoading = true;

    if(!tic_fs_ispubdir(launcher->fs))
    {

        s32 size = 0;
        void* data = tic_fs_load(launcher->fs, item->name, &size);

        if(data)
        {
            tic_cartridge* cart = (tic_cartridge*)malloc(sizeof(tic_cartridge));

            if(cart)
            {

                if(tic_tool_has_ext(item->name, PngExt))
                {
                    tic_cartridge* pngcart = loadPngCart((png_buffer){data, size});

                    if(pngcart)
                    {
                        memcpy(cart, pngcart, sizeof(tic_cartridge));
                        free(pngcart);
                    }
                    else memset(cart, 0, sizeof(tic_cartridge));
                }
#if defined(TIC80_PRO)
                else if(tic_project_ext(item->name))
                    tic_project_load(item->name, data, size, cart);
#endif
                else
                    tic_cart_load(cart, data, size);

                if(!EMPTY(cart->bank0.screen.data) && !EMPTY(cart->bank0.palette.vbank0.data))
                {
                    memcpy((item->palette = malloc(sizeof(tic_palette))), &cart->bank0.palette.vbank0, sizeof(tic_palette));
                    memcpy((item->cover = malloc(sizeof(tic_screen))), &cart->bank0.screen, sizeof(tic_screen));
                }

                free(cart);
            }

            free(data);
        }
    }
    else if(item->hash && !item->cover)
    {
        requestCover(launcher, item);
    }
}

static void initItemsAsync(Launcher* launcher, fs_done_callback callback, void* calldata)
{
    resetMenu(launcher);

    launcher->loading = true;

    char dir[TICNAME_MAX];
    tic_fs_dir(launcher->fs, dir);

    AddMenuItemData data = { NULL, 0, launcher, callback, calldata};

    if(strcmp(dir, "") != 0)
        addMenuItem("..", NULL, NULL, 0, &data, true);

    tic_fs_enum(launcher->fs, addMenuItem, addMenuItemsDone, MOVE(data));
}

typedef struct
{
    Launcher* launcher;
    char* last;
} GoBackDirDoneData;

static void onGoBackDirDone(void* data)
{
    GoBackDirDoneData* goBackDirDoneData = data;
    Launcher* launcher = goBackDirDoneData->launcher;

    char current[TICNAME_MAX];
    tic_fs_dir(launcher->fs, current);

    for(s32 i = 0; i < launcher->menu.count; i++)
    {
        const SurfItem* item = &launcher->menu.items[i];

        if(item->dir)
        {
            char path[TICNAME_MAX];

            if(strlen(current))
                sprintf(path, "%s/%s", current, item->name);
            else strcpy(path, item->name);

            if(strcmp(path, goBackDirDoneData->last) == 0)
            {
                launcher->menu.pos = i;
                break;
            }
        }
    }

    free(goBackDirDoneData->last);
    free(goBackDirDoneData);

    launcher->anim.movie = resetMovie(&launcher->anim.goback.show);
}

static void onGoBackDir(void* data)
{
    Launcher* launcher = data;
    char last[TICNAME_MAX];
    tic_fs_dir(launcher->fs, last);

    tic_fs_dirback(launcher->fs);

    GoBackDirDoneData goBackDirDoneData = {launcher, strdup(last)};
    initItemsAsync(launcher, onGoBackDirDone, MOVE(goBackDirDoneData));
}

static void onGoToDirDone(void* data)
{
    Launcher* launcher = data;
    launcher->anim.movie = resetMovie(&launcher->anim.gotodir.show);
}

static void onGoToDir(void* data)
{
    Launcher* launcher = data;
    SurfItem* item = getMenuItem(launcher);

    tic_fs_changedir(launcher->fs, item->name);
    initItemsAsync(launcher, onGoToDirDone, launcher);
}

static void goBackDir(Launcher* launcher)
{
    char dir[TICNAME_MAX];
    tic_fs_dir(launcher->fs, dir);

    if(strcmp(dir, "") != 0)
    {
        playSystemSfx(launcher->studio, 2);

        launcher->anim.movie = resetMovie(&launcher->anim.goback.hide);
    }
}

static void changeDirectory(Launcher* launcher, const char* name)
{
    printf("\nlauncher.c changeDirectory Called");
    if (strcmp(name, "..") == 0)
    {
        goBackDir(launcher);
    }
    else
    {
        playSystemSfx(launcher->studio, 2);
        launcher->anim.movie = resetMovie(&launcher->anim.gotodir.hide);
    }
}

static void onCartLoaded(void* data)
{
    printf("\nlauncher.c onCartLoaded Called");
    Launcher* launcher = data;
    runGame(launcher->studio);
}

static void onLoadCommandConfirmed(Studio* studio, bool yes, void* data)
{
    printf("\nlauncher.c onLoadCommandConfirmed Called");
    if(yes)
    {
        Launcher* launcher = data;
        SurfItem* item = getMenuItem(launcher);

        if (item->hash)
        {
            printf("\nlauncher.c onLoadCommandConfirmed calling loadByHash");
            launcher->console->loadByHash(launcher->console, item->name, item->hash, NULL, onCartLoaded, launcher);
        }
        else
        {
            printf("\nlauncher.c onLoadCommandConfirmed calling runGame");
            launcher->console->load(launcher->console, item->name);
            runGame(launcher->studio);
        }
    }
}

static void onPlayCart(void* data)
{
    printf("\nlauncher.c onPlayCart Called");
    Launcher* launcher = data;
    SurfItem* item = getMenuItem(launcher);
    printf("\nlauncher.c onPlayCart item->label = %s", item->label);
    printf("\nlauncher.c onPlayCart item->name = %s", item->name);
    printf("\nlauncher.c onPlayCart item->dir = %i", item->dir);

    studioCartChanged(launcher->studio)
        ? confirmLoadCart(launcher->studio, onLoadCommandConfirmed, launcher)
        : onLoadCommandConfirmed(launcher->studio, true, launcher);
}

static void loadCart(Launcher* launcher)
{
    printf("\nlauncher.c loadCart Called");
    SurfItem* item = getMenuItem(launcher);

    if(tic_tool_has_ext(item->name, PngExt))
    {
        s32 size = 0;
        void* data = tic_fs_load(launcher->fs, item->name, &size);

        if(data)
        {
            tic_cartridge* cart = loadPngCart((png_buffer){data, size});

            if(cart)
            {
                launcher->anim.movie = resetMovie(&launcher->anim.play);
                free(cart);
            }
        }
    }
    else launcher->anim.movie = resetMovie(&launcher->anim.play);
}

static void move(Launcher* launcher, s32 dir)
{
    printf("\nstudio.c move Called");
    launcher->menu.target = (launcher->menu.pos + launcher->menu.count + dir) % launcher->menu.count;

    Anim* anim = launcher->anim.move.items;
    anim->end = (launcher->menu.target - launcher->menu.pos) * MENU_HEIGHT;

    launcher->anim.movie = resetMovie(&launcher->anim.move);
}

static void processGamepad(Launcher* launcher)
{
    tic_mem* tic = launcher->tic;

    enum{Frames = MENU_HEIGHT};

    {
        enum{Hold = KEYBOARD_HOLD, Period = Frames};

        enum
        {
            Up, Down, Left, Right, A, B, X, Y
        };

        if(tic_api_btnp(tic, Up, Hold, Period)
            || tic_api_keyp(tic, tic_key_up, Hold, Period))
        {
            move(launcher, -1);
            playSystemSfx(launcher->studio, 2);
        }
        else if(tic_api_btnp(tic, Down, Hold, Period)
            || tic_api_keyp(tic, tic_key_down, Hold, Period))
        {
            move(launcher, +1);
            playSystemSfx(launcher->studio, 2);
        }
        else if(tic_api_btnp(tic, Left, Hold, Period)
            || tic_api_keyp(tic, tic_key_left, Hold, Period)
            || tic_api_keyp(tic, tic_key_pageup, Hold, Period))
        {
            s32 dir = -PAGE;

            if(launcher->menu.pos == 0) dir = -1;
            else if(launcher->menu.pos <= PAGE) dir = -launcher->menu.pos;

            move(launcher, dir);
        }
        else if(tic_api_btnp(tic, Right, Hold, Period)
            || tic_api_keyp(tic, tic_key_right, Hold, Period)
            || tic_api_keyp(tic, tic_key_pagedown, Hold, Period))
        {
            s32 dir = +PAGE, last = launcher->menu.count - 1;

            if(launcher->menu.pos == last) dir = +1;
            else if(launcher->menu.pos + PAGE >= last) dir = last - launcher->menu.pos;

            move(launcher, dir);
        }

        if(tic_api_btnp(tic, A, -1, -1)
            || ticEnterWasPressed(tic, -1, -1))
        {
            SurfItem* item = getMenuItem(launcher);
            item->dir 
                ? changeDirectory(launcher, item->name)
                : loadCart(launcher);
        }

        if(tic_api_btnp(tic, B, -1, -1)
            || tic_api_keyp(tic, tic_key_backspace, -1, -1))
        {
            goBackDir(launcher);
        }

#ifdef CAN_OPEN_URL

        if(tic_api_btnp(tic, Y, -1, -1))
        {
            SurfItem* item = getMenuItem(launcher);

            if(!item->dir)
            {
                char url[TICNAME_MAX];
                sprintf(url, TIC_WEBSITE "/play?cart=%i", item->id);
                tic_sys_open_url(url);
            }
        }
#endif

    }

}

static inline bool isIdle(Launcher* launcher)
{
    return launcher->anim.movie == &launcher->anim.idle;
}

static void tick(Launcher* launcher)
{
    processAnim(launcher->anim.movie, launcher);

    if(!launcher->init)
    {
        initItemsAsync(launcher, NULL, NULL);
        launcher->anim.movie = resetMovie(&launcher->anim.show);
        launcher->init = true;
    }

    tic_mem* tic = launcher->tic;
    tic_api_cls(tic, TIC_COLOR_BG);

    studio_menu_anim(launcher->tic, launcher->ticks++);

    if (isIdle(launcher) && launcher->menu.count > 0)
    {
        processGamepad(launcher);
        if(tic_api_keyp(tic, tic_key_escape, -1, -1))
            setStudioMode(launcher->studio, TIC_CONSOLE_MODE);
    }

    if (getStudioMode(launcher->studio) != TIC_SURF_MODE) return;

    if (launcher->menu.count > 0)
    {
        loadCover(launcher);

        tic_screen* cover = getMenuItem(launcher)->cover;

        if(cover)
            memcpy(tic->ram->vram.screen.data, cover->data, sizeof(tic_screen));
    }

    VBANK(tic, 1)
    {
        tic_api_cls(tic, tic->ram->vram.vars.clear = tic_color_yellow);
        memcpy(tic->ram->vram.palette.data, getConfig(launcher->studio)->cart->bank0.palette.vbank0.data, sizeof(tic_palette));

        if(launcher->menu.count > 0)
        {
            drawMenu(launcher, launcher->anim.val.menuX, (TIC80_HEIGHT - MENU_HEIGHT)/2);
        }
        else if(!launcher->loading)
        {
            static const char Label[] = "You don't have any files...";
            s32 size = tic_api_print(tic, Label, 0, -TIC_FONT_HEIGHT, tic_color_white, true, 1, false);
            tic_api_print(tic, Label, (TIC80_WIDTH - size) / 2, (TIC80_HEIGHT - TIC_FONT_HEIGHT)/2, tic_color_white, true, 1, false);
        }

        drawTopToolbar(launcher, 0, launcher->anim.val.topBarY - MENU_HEIGHT);
        drawBottomToolbar(launcher, 0, TIC80_HEIGHT - launcher->anim.val.bottomBarY);
    }
}

static void resume(Launcher* launcher)
{
    launcher->anim.movie = resetMovie(&launcher->anim.show);
}

static void scanline(tic_mem* tic, s32 row, void* data)
{
    Launcher* launcher = (Launcher*)data;

    if(launcher->menu.count > 0)
    {
        const SurfItem* item = getMenuItem(launcher);

        if(item->palette)
        {
            if(row == 0)
            {
                memcpy(&tic->ram->vram.palette, item->palette, sizeof(tic_palette));
                fadePalette(&tic->ram->vram.palette, launcher->anim.val.coverFade);
            }

            return;
        }
    }

    studio_menu_anim_scanline(tic, row, NULL);
}

static void emptyDone(void* data) {}

static void setIdle(void* data)
{
    Launcher* launcher = data;
    launcher->anim.movie = resetMovie(&launcher->anim.idle);
}

static void setLeftShow(void* data)
{
    Launcher* launcher = data;
    launcher->anim.movie = resetMovie(&launcher->anim.gotodir.show);
}

static void freeAnim(Launcher* launcher)
{
    printf("\nlauncher.c freeAnim Called");
    FREE(launcher->anim.show.items);
    FREE(launcher->anim.play.items);
    FREE(launcher->anim.move.items);
    FREE(launcher->anim.gotodir.show.items);
    FREE(launcher->anim.gotodir.hide.items);
    FREE(launcher->anim.goback.show.items);
    FREE(launcher->anim.goback.hide.items);
}

static void moveDone(void* data)
{
    Launcher* launcher = data;
    launcher->menu.pos = launcher->menu.target;
    launcher->anim.val.pos = 0;
    launcher->anim.movie = resetMovie(&launcher->anim.idle);
}

void initLauncher(Launcher* launcher, Studio* studio, struct Console* console)
{
    printf("\nlauncher.c initLauncher Called");
    printf("\nlauncher.c initLauncher calling freeAnim");
    freeAnim(launcher);
    printf("\nlauncher.c initLauncher initializing Launcher Object");
    *launcher = (Launcher)
    {
        .studio = studio,
        .tic = getMemory(studio),
        .console = console,
        .fs = console->fs,
        .net = console->net,
        .tick = tick,
        .ticks = 0,
        .init = false,
        .loading = true,
        .resume = resume,
        .menu =
        {
            .pos = 0,
            .items = NULL,
            .count = 0,
        },
        .anim =
        {
            .idle = {.done = emptyDone,},

            .show = MOVIE_DEF(ANIM, setIdle,
            {
                {0, MENU_HEIGHT, ANIM, &launcher->anim.val.topBarY, AnimEaseIn},
                {0, MENU_HEIGHT, ANIM, &launcher->anim.val.bottomBarY, AnimEaseIn},
                {-TIC80_WIDTH, 0, ANIM, &launcher->anim.val.menuX, AnimEaseIn},
                {0, MENU_HEIGHT, ANIM, &launcher->anim.val.menuHeight, AnimEaseIn},
                {COVER_FADEOUT, COVER_FADEIN, ANIM, &launcher->anim.val.coverFade, AnimEaseIn},
            }),

            .play = MOVIE_DEF(ANIM, onPlayCart,
            {
                {MENU_HEIGHT, 0, ANIM, &launcher->anim.val.topBarY, AnimEaseIn},
                {MENU_HEIGHT, 0, ANIM, &launcher->anim.val.bottomBarY, AnimEaseIn},
                {0, -TIC80_WIDTH, ANIM, &launcher->anim.val.menuX, AnimEaseIn},
                {MENU_HEIGHT, 0, ANIM, &launcher->anim.val.menuHeight, AnimEaseIn},
                {COVER_FADEIN, COVER_FADEOUT, ANIM, &launcher->anim.val.coverFade, AnimEaseIn},
            }),

            .move = MOVIE_DEF(9, moveDone, {{0, 0, 9, &launcher->anim.val.pos, AnimLinear}}),

            .gotodir =
            {
                .show = MOVIE_DEF(ANIM, setIdle,
                {
                    {TIC80_WIDTH, 0, ANIM, &launcher->anim.val.menuX, AnimEaseIn},
                    {0, MENU_HEIGHT, ANIM, &launcher->anim.val.menuHeight, AnimEaseIn},
                }),

                .hide = MOVIE_DEF(ANIM, onGoToDir,
                {
                    {0, -TIC80_WIDTH, ANIM, &launcher->anim.val.menuX, AnimEaseIn},
                    {MENU_HEIGHT, 0, ANIM, &launcher->anim.val.menuHeight, AnimEaseIn},
                }),
            },

            .goback =
            {
                .show = MOVIE_DEF(ANIM, setIdle,
                {
                    {-TIC80_WIDTH, 0, ANIM, &launcher->anim.val.menuX, AnimEaseIn},
                    {0, MENU_HEIGHT, ANIM, &launcher->anim.val.menuHeight, AnimEaseIn},
                }),

                .hide = MOVIE_DEF(ANIM, onGoBackDir,
                {
                    {0, TIC80_WIDTH, ANIM, &launcher->anim.val.menuX, AnimEaseIn},
                    {MENU_HEIGHT, 0, ANIM, &launcher->anim.val.menuHeight, AnimEaseIn},
                }),
            },
        },
        .scanline = scanline,
    };
    printf("\nlauncher.c initLauncher calling resetMovie on launcher-anim.movie");
    launcher->anim.movie = resetMovie(&launcher->anim.idle);
    printf("\nlauncher.c initLauncher calling tic_fs_makedir");
    tic_fs_makedir(launcher->fs, TIC_CACHE);
}

void freeLauncher(Launcher* launcher)
{
    freeAnim(launcher);
    resetMenu(launcher);
    free(launcher);
}
