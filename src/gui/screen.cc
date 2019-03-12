#include "screen.h"

#include <assert.h>
#include <limits.h>
#include <math.h>

#if !OS_WIN
#include <libgen.h>
#include <unistd.h>
#endif

#include <iostream>
#include <sstream>
#include <string>

#include <glog/logging.h>

#include "base/base.h"
#include "base/file/path.h"
#include "gui/util.h"

using namespace std;

Screen::Screen(int width, int height, const Box& mainBox) :
    surface_(makeUniqueSDLSurface(SDL_CreateRGBSurface(0, width, height, 32, 0x00FF0000, 0x0000FF00, 0x000000FF, 0xFF000000))),
    mainBox_(mainBox),
    font_(nullptr)
{
    init();
}

void Screen::init()
{
    bgColor_.r = 198;
    bgColor_.g = 196;
    bgColor_.b = 156;

    if (!TTF_WasInit()) {
        CHECK_EQ(TTF_Init(), 0) << TTF_GetError();
    }

    string fontFilePath = file::joinPath(DATA_DIR, "mikachan-p.ttf");
#if OS_WIN
    font_ = TTF_OpenFont(fontFilePath.c_str(), 16);
#else
    if (access(fontFilePath.c_str(), R_OK) == 0) {
        font_ = TTF_OpenFont(fontFilePath.c_str(), 16);
    }
#endif

    if (!font_) {
        LOG(FATAL) << TTF_GetError();
    }
}

Screen::~Screen()
{
    TTF_CloseFont(font_);
}

void Screen::clear()
{
    SDL_FillRect(surface(), NULL, SDL_MapRGB(surface()->format, bgColor_.r, bgColor_.g, bgColor_.b));
}
