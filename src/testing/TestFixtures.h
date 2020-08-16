#pragma once

#include "TestClient.h"
#include "TestServer.h"

class ThreeClients {
 public:
  ThreeClients()
      : cAlice(TestClient::WithUsername("Alice")),
        cBob(TestClient::WithUsername("Bob")),
        cCharlie(TestClient::WithUsername("Charlie")) {
    server.waitForUsers(3);
    alice = &server.findUser("Alice");
    bob = &server.findUser("Bob");
    charlie = &server.findUser("Charlie");
  }

  TestServer server;
  TestClient cAlice, cBob, cCharlie;
  User *alice, *bob, *charlie;
};
