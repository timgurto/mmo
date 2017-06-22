#define CATCH_CONFIG_RUNNER
#include "catch.hpp"

#include <SDL.h>

#include "../Args.h"
#include "../client/Renderer.h"

Args cmdLineArgs;
Renderer renderer;

int main( int argc, char* argv[] )
{
    srand(static_cast<unsigned>(time(0)));

    cmdLineArgs.init(argc, argv);
    cmdLineArgs.add("new");
    cmdLineArgs.add("server-ip", "127.0.0.1");
    cmdLineArgs.add("window");
    cmdLineArgs.add("quiet");
    cmdLineArgs.add("user-files-path", "testing/users");
    cmdLineArgs.add("hideLoadingScreen");
    
    renderer.init();

    int result = Catch::Session().run( argc, argv );

    return ( result < 0xff ? result : 0xff );
}
