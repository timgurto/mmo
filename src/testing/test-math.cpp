#include <SDL.h>

#include "testing.h"
#include "../Color.h"
#include "../Point.h"
#include "../NormalVariable.h"
#include "../server/Server.h"

#include "catch.hpp"

TEST_CASE("Convert from Uint32 to Color and back"){
    Uint32 testNum = rand();
    Color c = testNum;
    Uint32 result = c;
    CHECK(testNum == result);
}

TEST_CASE("Zero-SD normal variables"){
    NormalVariable a(0, 0);
    CHECK( a.generate() == 0);
    CHECK(a() == 0);

    NormalVariable b(1, 0);
    CHECK(b.generate() == 1);

    NormalVariable c(-0.5, 0);
    CHECK(c.generate() == -0.5);
}

TEST_CASE("Complex normal variable"){
    NormalVariable default;
    NormalVariable v(10, 2);
    static const size_t SAMPLES = 1000;
    size_t
        numWithin1SD = 0,
        numWithin2SD = 0;
    for (size_t i = 0; i != SAMPLES; ++i){
        double sample = v.generate();
        if (sample > 8 && sample < 12)
            ++numWithin1SD;
        if (sample > 6 && sample < 14)
            ++numWithin2SD;
    }
    double
        proportionWithin1SD = 1.0 * numWithin1SD / SAMPLES,
        proportionBetween1and2SD = 1.0 * (numWithin2SD - numWithin1SD) / SAMPLES;
    // Proportion within 1 standard deviation should be around 0.68
    CHECK (proportionWithin1SD >= 0.63);
    CHECK (proportionWithin1SD <= 0.73);
    // Proportion between 1 and 2 standard deviations should be around 0.27
    CHECK (proportionBetween1and2SD >= 0.22);
    CHECK (proportionBetween1and2SD <= 0.32);
}

TEST_CASE("NormalVariable copying"){
    NormalVariable nv1;
    NormalVariable nv2 = nv1;
    nv1();
}

TEST_CASE("Distance-to-line with A=B"){
    Point p(10, 10);
    Point q(5, 3);
    distance(p, q, q);
}

TEST_CASE("getTileRect() behaves as expected"){
    CHECK(Server::getTileRect(0, 0) == Rect(-16, 0, 32, 32));
    CHECK(Server::getTileRect(1, 0) == Rect(16, 0, 32, 32));
    CHECK(Server::getTileRect(0, 1) == Rect(0, 32, 32, 32));
    CHECK(Server::getTileRect(1, 1) == Rect(32, 32, 32, 32));

    CHECK(Server::getTileRect(4, 0) == Rect(112, 0, 32, 32));
    CHECK(Server::getTileRect(5, 0) == Rect(144, 0, 32, 32));
    CHECK(Server::getTileRect(0, 4) == Rect(-16, 128, 32, 32));
    CHECK(Server::getTileRect(0, 5) == Rect(0, 160, 32, 32));

    CHECK(Server::getTileRect(7, 5) == Rect(224, 160, 32, 32));
    CHECK(Server::getTileRect(7, 6) == Rect(208, 192, 32, 32));
}
