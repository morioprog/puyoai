#include <stdio.h>
#include <stdlib.h>

#include <iostream>

#include <gflags/gflags.h>
#include <glog/logging.h>
#include <SDL.h>
#include <SDL_image.h>

#include "capture/capture.h"
#include "capture/color.h"
#include "gui/bounding_box.h"
#include "gui/main_window.h"
#include "gui/unique_sdl_surface.h"
#include "gui/SDL_prims.h"

#if defined(PUYOPUYO_TSU)
#include "capture/ac_analyzer.h"
#elif defined(PUYOPUYO_ESPORTS)
#include "capture/es_analyzer.h"
#endif

using namespace std;

void showUsage(const char* name)
{
    fprintf(stderr, "Usage: %s <in-bmp> [1|2] [NEXT1-AXIS|NEXT1-AXIS-HALF|NEXT1-CHILD|NEXT2-AXIS|NEXT2-CHILD]\n", name);
    fprintf(stderr, "Usage: %s <in-bmp> [1|2] x y\n", name);
}

int main(int argc, char* argv[])
{
    google::ParseCommandLineFlags(&argc, &argv, true);
    google::InitGoogleLogging(argv[0]);

    if (argc < 4) {
        showUsage(argv[0]);
        exit(EXIT_FAILURE);
    }

    SDL_Init(SDL_INIT_VIDEO);
    atexit(SDL_Quit);

    string imgFilename = argv[1];
    int playerId = std::atoi(argv[2]);

    bool half = false;
    bool usesNextPuyoPosition = false;
    NextPuyoPosition npp = NextPuyoPosition::NEXT1_AXIS;
    int x = 1, y = 1;
    ACAnalyzer::AllowOjama allowOjama = ACAnalyzer::AllowOjama::ALLOW_OJAMA;
    if (argv[3] == string("NEXT1-AXIS")) {
        usesNextPuyoPosition = true;
        npp = NextPuyoPosition::NEXT1_AXIS;
        allowOjama = ACAnalyzer::AllowOjama::DONT_ALLOW_OJAMA;
    } else if (argv[3] == string("NEXT1-AXIS-HALF")) {
        usesNextPuyoPosition = true;
        npp = NextPuyoPosition::NEXT1_AXIS;
        half = true;
        allowOjama = ACAnalyzer::AllowOjama::DONT_ALLOW_OJAMA;
    } else if (argv[3] == string("NEXT1-CHILD")) {
        usesNextPuyoPosition = true;
        npp = NextPuyoPosition::NEXT1_CHILD;
        allowOjama = ACAnalyzer::AllowOjama::DONT_ALLOW_OJAMA;
    } else if (argv[3] == string("NEXT2-AXIS")) {
        usesNextPuyoPosition = true;
        npp = NextPuyoPosition::NEXT2_AXIS;
        allowOjama = ACAnalyzer::AllowOjama::DONT_ALLOW_OJAMA;
    } else if (argv[3] == string("NEXT2-CHILD")) {
        usesNextPuyoPosition = true;
        npp = NextPuyoPosition::NEXT2_CHILD;
        allowOjama = ACAnalyzer::AllowOjama::DONT_ALLOW_OJAMA;
    } else {
        if (argc < 5) {
            showUsage(argv[0]);
            exit(EXIT_FAILURE);
        }

        x = atoi(argv[3]);
        y = atoi(argv[4]);
    }

    UniqueSDLSurface surf(makeUniqueSDLSurface(IMG_Load(imgFilename.c_str())));
    if (!surf) {
        fprintf(stderr, "Failed to load %s!\n", argv[1]);
        exit(EXIT_FAILURE);
    }

    ACAnalyzer analyzer;

    // this should be called after |analyzer| is created.
    Box b;
    if (usesNextPuyoPosition) {
        b = BoundingBox::boxForAnalysis(playerId - 1, npp);
    } else {
        b = BoundingBox::boxForAnalysis(playerId - 1, x, y);
    }
    if (half) {
        b.sy += b.h() / 2;
    }

    const ACAnalyzer::ShowDebugMessage showsDebugMessage = ACAnalyzer::ShowDebugMessage::SHOW_DEBUG_MESSAGE;
    RealColor rc = analyzer.analyzeBox(surf.get(), b, allowOjama, showsDebugMessage);

    cout << "Color: " << rc << endl;
    if (surf->w == 16 && surf->h == 16)
        cout << "analyzed with AROW: " << analyzer.analyzeBoxWithRecognizer(surf.get(), b) << endl;

    if (usesNextPuyoPosition && ((npp == NextPuyoPosition::NEXT2_AXIS) || (npp == NextPuyoPosition::NEXT2_CHILD))) {
        RealColor rc = analyzer.analyzeBoxNext2(surf.get(), b);
        cout << "analyzed with NEXT2: " << rc << endl;
    }

    analyzer.drawWithAnalysisResult(surf.get());
    SDL_SaveBMP(surf.get(), "output.bmp");

    {
        UniqueSDLSurface sub_surf(makeUniqueSDLSurface(SDL_CreateRGBSurface(0, b.w(), b.h(), 32, 0, 0, 0, 0)));
        SDL_Rect src_rect = b.toSDLRect();
        SDL_BlitSurface(surf.get(), &src_rect, sub_surf.get(), nullptr);
        SDL_SaveBMP(sub_surf.get(), "output-sub.bmp");
    }

    return 0;
}
