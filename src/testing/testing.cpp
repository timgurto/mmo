#include <iomanip>
#include <iostream>

#include <SDL.h>
#include "Test.h"
#include "../client/Renderer.h"
#include "../Args.h"
#include "../Socket.h"
#include "../server/LogConsole.h"

Args cmdLineArgs;
Renderer renderer;

int main(int argc, char **argv){
    srand(static_cast<unsigned>(time(0)));

    cmdLineArgs.init(argc, argv);
    cmdLineArgs.add("new");
    cmdLineArgs.add("server-ip", "127.0.0.1");
    cmdLineArgs.add("window");
    cmdLineArgs.add("quiet");

    renderer.init();

    LogConsole log;
    Socket::debug = &log;
    if (cmdLineArgs.contains("quiet"))
        log.quiet();

    std::string
        colorStart = "\x1B[38;2;0;127;127m",
        colorFail  = "\x1B[38;2;255;0;0m",
        colorPass  = "\x1B[38;2;0;192;0m",
        colorSkip  = "\x1B[38;2;51;51;51m",
        colorReset = "\x1B[0m";
    char
        GLYPH_PASS = 254,
        GLYPH_FAIL = 254,
        GLYPH_SKIP = 254;

    if (cmdLineArgs.contains("no-color")){
        colorStart = colorFail = colorPass = colorSkip = colorReset = "";
        GLYPH_FAIL = 219;
        GLYPH_SKIP = 250;
    }

    size_t
        i = 0,
        passed = 0,
        failed = 0,
        skipped = 0;
    for (const Test &test : Test::testContainer()){
        ++i;
        const std::string *statusColor;
        char glyph;
        size_t gapLength = Test::STATUS_MARGIN - 
                           min(Test::STATUS_MARGIN, test.description().length());
        std::string gap(gapLength, ' ');
        std::cout
            << colorStart << std::setw(4) << i << colorReset << ' '
            << test.description() << gap << std::flush;
        if (test.shouldSkip()) {
            statusColor = &colorSkip;
            glyph = GLYPH_SKIP;
            ++skipped;
        } else {
            bool result = test.fun()();
            if (!result){
                statusColor = &colorFail;
                glyph = GLYPH_FAIL;
                ++failed;
            } else {
                statusColor = &colorPass;
                glyph = GLYPH_PASS;
                ++passed;
            }
        }
        std::cout << *statusColor << glyph << colorReset << std::endl;
    }

    std::cout << "----------" << std::endl
              << "  " << i << " tests in total";
    if (passed > 0)  std::cout << ", " << colorPass << passed  << " passed"  << colorReset;
    if (failed > 0)  std::cout << ", " << colorFail << failed  << " failed"  << colorReset;
    if (skipped > 0) std::cout << ", " << colorSkip << skipped << " skipped" << colorReset;
    std::cout << "." << std::endl;

    return 0;
}
