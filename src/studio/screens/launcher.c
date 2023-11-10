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
    s32 screen_id;
    s32 column;
    tic_screen* cover;

    tic_palette* palette;

    bool coverLoading;
    bool menuButton;
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
        static const char Label[] = "TIC-80 LAUNCHER";
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
        char label[] = "MAIN MENU       ";
        //char dir[TICNAME_MAX];
        //tic_fs_dir(launcher->fs, dir);

        //sprintf(label, "/%s", dir);
        if (launcher->screen == SCREEN_SETTINGS)
        {
            strcpy(label, "SETTINGS");
        }
        if (launcher->screen == SCREEN_LOCAL_SELECT)
        {
            strcpy(label, "SORTING SELECT");
        }
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

static void drawColumns(Launcher* launcher)
{

}

static void drawMenu(Launcher* launcher, s32 x, s32 y)
{
    // This is drawn every frame.

    // Vars for working
    tic_mem* tic = launcher->tic;
    enum {Height = MENU_HEIGHT};
    // Vars for Dimensions
    s32 div_x = TIC80_WIDTH / 3;
    s32 w_pad = 14;
    s32 icon_w = div_x - w_pad;
    s32 div_y = TIC80_HEIGHT / 2;
    s32 h_pad = 20;
    s32 icon_h = div_y - h_pad;
    s32 y_pos = div_y - (icon_h / 2);
    // Drawing the Selector position based on launcher->menu.pos
    tic_api_rect(tic, launcher->menu.pos - 1, y_pos - 1, icon_w + 2, icon_h + 2, tic_color_red);

    if (launcher->screen == SCREEN_MAIN)
    {
        // Create 3 Icons.
        // Icon One
        tic_api_rect(tic, w_pad / 2, y_pos, icon_w, icon_h, tic_color_white);
        // Icon Two
        tic_api_rect(tic, div_x + (w_pad / 2), y_pos, icon_w, icon_h, tic_color_white);
        // Icon Three
        tic_api_rect(tic, div_x * 2 + (w_pad / 2), y_pos, icon_w, icon_h, tic_color_white);
        // Label One
        const char* label_1 = "LIBRARY";
        tic_api_print(tic, label_1, w_pad / 2, y_pos + icon_h + 5, tic_color_black, false, 1, false);
        tic_api_print(tic, label_1, w_pad / 2, y_pos + icon_h + 4, tic_color_white, false, 1, false);
        // Label Two
        const char* label_2 = "WEB";
        tic_api_print(tic, label_2, div_x + (w_pad / 2), y_pos + icon_h + 5, tic_color_black, false, 1, false);
        tic_api_print(tic, label_2, div_x + (w_pad / 2), y_pos + icon_h + 4, tic_color_white, false, 1, false);
        // Label Three
        const char* label_3 = "SETTINGS";
        tic_api_print(tic, label_3, div_x * 2 + (w_pad / 2), y_pos + icon_h + 5, tic_color_black, false, 1, false);
        tic_api_print(tic, label_3, div_x * 2 + (w_pad / 2), y_pos + icon_h + 4, tic_color_white, false, 1, false);
        //printf("\nlauncher.c drawMenu: Icon One X Pos = %i", w_pad / 2);
        //printf("\nlauncher.c drawMenu: Icon Two X Pos = %i", div_x + (w_pad / 2));
        //printf("\nlauncher.c drawMenu: Icon Three X Pos = %i", div_x * 2 + (w_pad / 2));
        // 7, 87, 167
    }
    else
    {
        // What is s32 ym?
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
}

static inline void cutExt(char* name, const char* ext)
{
    name[strlen(name)-strlen(ext)] = '\0';
}

static bool addMenuItem(const char* name, const char* title, const char* hash, s32 id, void* ptr, bool dir)
{
    printf("\nlauncher.c addMenuItem Called");
    printf("\nlauncher.c addMenuItem (name) = %s", name);
    printf("\nlauncher.c addMenuItem (title) = %s", title);
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

static bool addMenuButton(const char* name, s32 screen_id, s32 id, void* ptr)
{
    printf("\nlauncher.c addMenuButton Called");
    printf("\nlauncher.c addMenuButton (name) = %s", name);
    printf("\nlauncher.c addMenuButton (screen_id) = %i", screen_id);
    //I don't know what a pointer is'
    AddMenuItemData* data = (AddMenuItemData*)ptr;

    data->items = realloc(data->items, sizeof(SurfItem) * ++data->count);
    SurfItem* item = &data->items[data->count-1];

    *item = (SurfItem)
    {
        .name = strdup(name),
        .hash = NULL,
        .id = id,
        .screen_id = screen_id,
        .menuButton = true,
    };

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
    printf("\nlauncher.c addMenuItemsDone Called");
    AddMenuItemData* addMenuItemData = data;
    Launcher* launcher = addMenuItemData->launcher;

    printf("\nlauncher.c addMenuItemsDone: assigning menu.items from addMenuItemData->items");
    launcher->menu.items = addMenuItemData->items;
    printf("\nlauncher.c addMenuItemsDone: assigning menu.count from addMenuItemData->count");
    launcher->menu.count = addMenuItemData->count;
    printf("\nlauncher.c addMenuItemsDone: menu.count = %i", launcher->menu.count);

    if(!tic_fs_ispubdir(launcher->fs))
        printf("\nlauncher.c addMenuItemsDone calling qsort");
        qsort(launcher->menu.items, launcher->menu.count, sizeof *launcher->menu.items, itemcmp);

    if (addMenuItemData->done)
        addMenuItemData->done(addMenuItemData->data);
    printf("\nlauncher.c addMenuItemsDone calling free(addMenuItemData");
    free(addMenuItemData);
    printf("\nlauncher.c addMenuItemsDone: setting launcher->loading = false");
    launcher->loading = false;
}

static void resetMenu(Launcher* launcher)
{
    printf("\nlauncher.c resetMenu Called");
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

    launcher->menu.pos = 7;
    launcher->menu.column = 0;
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
    printf("\nlauncher.c initItemsAsync Called");
    printf("\nlauncher.c initItemsAsync calling resetMenu");
    resetMenu(launcher);

    launcher->loading = true;

    char dir[TICNAME_MAX];
    printf("\nlauncher.c initItemsAsync calling tis_fs_dir");
    tic_fs_dir(launcher->fs, dir);
    printf("\nlauncher.c initItemsAsync constructing data of type AddMenuItemData");
    AddMenuItemData data = { NULL, 0, launcher, callback, calldata};
    printf("\nlauncher.c initItemsAsync: NOTE: I think this is where i add in my Menu Buttons?");
    // I can add in menu buttons into this system maybe
    // then sort out drawing them in columns later in the drawMenu function... maybe lol
    if(launcher->screen == SCREEN_MAIN)
    {
        addMenuButton("LIBRARY", SCREEN_MAIN, 0, &data);
        addMenuButton("WEB", SCREEN_MAIN, 1, &data);
        addMenuButton("SETTINGS", SCREEN_MAIN, 2, &data);
        addMenuItemsDone(MOVE(data));
    }
    else
    {
            if(strcmp(dir, "") != 0)
            {
                printf("\nlauncher.c initItemsAsync calling addMenuItem (..)");
                addMenuItem("..", NULL, NULL, 0, &data, true);
            }
        printf("\nlauncher.c initItemsAsync calling tic_fs_enum");
        tic_fs_enum(launcher->fs, addMenuItem, addMenuItemsDone, MOVE(data));
    }
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
    printf("\nlauncher.c onGoToDirDone Called");
    Launcher* launcher = data;
    launcher->anim.movie = resetMovie(&launcher->anim.gotodir.show);
}

static void onGoToDir(void* data)
{
    printf("\nlauncher.c onGoToDir Called");
    Launcher* launcher = data;
    SurfItem* item = getMenuItem(launcher);

    tic_fs_changedir(launcher->fs, item->name);
    initItemsAsync(launcher, onGoToDirDone, launcher);
}

static void goBackDir(Launcher* launcher)
{
    printf("\nlauncher.c goBackDir Called");
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
    printf("\nlauncher.c changeDirectory(name) = %s", name);
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
    // menu.target will be the target id of what the selector should load/interact with
    // menu.pos will be the position of the selector in actual pixel space, not index.
    printf("\nlauncher.c move Called");

    //launcher->menu.target = (launcher->menu.count + dir) % launcher->menu.count;
    // Okay... so anim.move is a Movie that is contained in the anim struct of the Launcher
    // anim.move.items are Anim Object(s) contained within the Movie
    Anim* anim = launcher->anim.move.items;
    printf("\nlauncher.c move: anim.move.items->start = %i before", anim->start);
    printf("\nlauncher.c move: anim.move.items->end = %i before", anim->end);
    printf("\nlauncher.c move: launcher->menu.target = %i before", launcher->menu.target);
    if(dir > 0)
    {
        printf("\nlauncher.c move: input is Positive");
        if(launcher->menu.target < launcher->menu.count - 1)
        {
            launcher->menu.target = launcher->menu.target + 1;
        }
        if(anim->end < 167)
        {
            printf("\nlauncher.c move: Add 80 to end after setting start to end");
            anim->start = anim->end;
            anim->end = anim->end + 80;
        }
        else
        {
            printf("\nlauncher.c move: Selector already at left side... Check if Gallery can Scroll");
        }
    }
    else
    {
        printf("\nlauncher.c move: input is Negative");
        if(launcher->menu.target > 0)
        {
            launcher->menu.target = launcher->menu.target - 1;
        }
        if(anim->end > 7)
        {
            printf("\nlauncher.c move: Subtract 80 to end after setting start to end");
            anim->start = anim->end;
            anim->end = anim->end - 80;
        }
        else
        {
            printf("\nlauncher.c move: Selector already at right side... Check if Gallery can Scroll");
        }
    }
    // anim is now a new Anim i think? and its assigned to the items within the Movie called "move"
    // Assigning Start and End for Anim
    printf("\nlauncher.c move: anim.move.items->start = %i after", anim->start);
    printf("\nlauncher.c move: anim.move.items->end = %i after", anim->end);
    printf("\nlauncher.c move: launcher->menu.target = %i after", launcher->menu.target);
    //anim->end = (launcher->menu.target - launcher->menu.pos) * MENU_HEIGHT;

    printf("\nlauncher.c move calling resetMovie");
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
            printf("\nlauncher.c processGamepad: Wraparound functionality Left Side");
            if(launcher->menu.target == 0) dir = -1;
            else if(launcher->menu.target <= PAGE) dir = -launcher->menu.target;

            move(launcher, dir);
        }
        else if(tic_api_btnp(tic, Right, Hold, Period)
            || tic_api_keyp(tic, tic_key_right, Hold, Period)
            || tic_api_keyp(tic, tic_key_pagedown, Hold, Period))
        {
            s32 dir = +PAGE, last = launcher->menu.count - 1;
            printf("\nlauncher.c processGamepad: Wraparound functionality Right Side");
            if(launcher->menu.target == last) dir = +1;
            else if(launcher->menu.target + PAGE >= last) dir = last - launcher->menu.target;

            move(launcher, dir);
        }

        if(tic_api_btnp(tic, A, -1, -1)
            || ticEnterWasPressed(tic, -1, -1))
        {
            printf("\nlauncher.c processGamepad: A or Enter Pressed");
            SurfItem* item = getMenuItem(launcher);
            printf("\nlauncher.c processGamepad: SurfItem bool dir = %i", item->dir);
            printf("\nlauncher.c processGamepad: SurfItem bool menuButton = %i", item->menuButton);
            if(item->dir)
            {
                changeDirectory(launcher, item->name);
            }
            else if(item->menuButton)
            {
                printf("\nlauncher.c processGamepad: Time to make a function that changes menu screens!");
            }
            else
            {
                loadCart(launcher);
            }
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
    //printf("\nlauncher.c tick Called");
    //printf("\nlauncher.c tick calling processAnim");
    processAnim(launcher->anim.movie, launcher);

    if(!launcher->init)
    {
        printf("\nlauncher.c tick launcher->init == false");
        initItemsAsync(launcher, NULL, NULL);
        launcher->anim.movie = resetMovie(&launcher->anim.show);
        launcher->init = true;
    }

    tic_mem* tic = launcher->tic;
    tic_api_cls(tic, TIC_COLOR_BG);

    studio_menu_anim(launcher->tic, launcher->ticks++);

    if (isIdle(launcher) && launcher->menu.count > 0)
    {
        //printf("\nlauncher.tick: isIdle(launcher) == true AND launcher->menu.count > 0");
        processGamepad(launcher);
        if(tic_api_keyp(tic, tic_key_escape, -1, -1))
            setStudioMode(launcher->studio, TIC_CONSOLE_MODE);
    }

    if (getStudioMode(launcher->studio) != TIC_LAUNCHER_MODE) return;

    if (launcher->menu.count > 0)
    {
        //printf("\nlauncher.c tick: launcher->menu.count > 0");
        if(launcher->screen == SCREEN_MAIN)
        {
            //printf("\nlauncher.c tick: launcher->screen == SCREEN_MAIN");
        }
        else
        {
            //printf("\nlauncher.c tick: calling loadCover");
            loadCover(launcher);
            //printf("\nlauncher.c tick: assigning cover from return of getMenuItem");
            tic_screen* cover = getMenuItem(launcher)->cover;
            if(cover)
                //printf("\nlauncher.c tick: cover == true");
                memcpy(tic->ram->vram.screen.data, cover->data, sizeof(tic_screen));
        }
    }

    VBANK(tic, 1)
    {
        tic_api_cls(tic, tic->ram->vram.vars.clear = tic_color_yellow);
        memcpy(tic->ram->vram.palette.data, getConfig(launcher->studio)->cart->bank0.palette.vbank0.data, sizeof(tic_palette));

        if(launcher->menu.count > 0)
        {
            //printf("\nlauncher.c tick: launcher->menu.count > 0 CLAUSE VBANK(tic, 1)");
            //printf("\nlauncher.c tick: calling drawMenu");
            drawMenu(launcher, launcher->anim.val.menuX, (TIC80_HEIGHT - MENU_HEIGHT)/2);
        }
        else if(!launcher->loading)
        {
            //printf("\nlauncher.c tick: launcher->loading == false");
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
    printf("\nlauncher.c moveDone Called");
    Launcher* launcher = data;
    printf("\nlauncher.c moveDone assigning menu.pos to anim.val.pos");
    launcher->menu.pos = launcher->anim.val.pos;
    printf("\nlauncher.c moveDone assigning anim.val.pos = 0");
    launcher->anim.val.pos = 0;
    printf("\nlauncher.c moveDone calling reset move and setting anim.idle");
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
        .screen = SCREEN_MAIN,
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
            // This is where .move movie is designed... i think
            // Alright so 9 is the TIME
            // moveDone is the DONE
            // The rest of it is within two {} brackets... I do not know why, but it looks like an Anim Object
            // 0 is start
            // 0 is end
            // 9 is time ... again?
            // &launcher->anim.val.pos is the value
            // lastly AnimLinear is the AnimEffect.
            // Current theory, the animation system is a thing that lerps two given values at a rate of Time on the sidelines...
            // As it lerps those values it assigns them to the Value property, and when it is done calls its Done function.
            // So, if I want to move things horizontally, I need to change how anim.val.pos is interacted with... ? I guess? lol
            .move = MOVIE_DEF(9, moveDone, {{7, 7, 9, &launcher->anim.val.pos, AnimLinear}}),

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
    printf("\nlauncher.c freeLauncher Called");
    freeAnim(launcher);
    resetMenu(launcher);
    free(launcher);
}
