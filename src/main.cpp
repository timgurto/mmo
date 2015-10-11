// (C) 2015 Tim Gurto

#include <cassert>
#include <ctime>
#include <cstdlib>
#include <string>
#include <SDL.h>

#include "Args.h"
#include "client/Client.h"
#include "client/Renderer.h"
#include "client/Texture.h"
#include "server/Server.h"

Args cmdLineArgs; // MUST be defined before renderer
Renderer renderer; // MUST be defined after cmdLineArgs

int main(int argc, char* argv[]){
    cmdLineArgs.init(argc, argv);
    renderer.init();

    srand(static_cast<unsigned>(time(0)));

    if (cmdLineArgs.contains("server")){
        Server server;
        server.run();
    } else {
        Client client;
        client.run();
    }

    assert(Texture::numTextures() == 0);
    return 0;
}
