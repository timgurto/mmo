#pragma once

#include <string>

void handleInput(unsigned timeElapsed);
void render();

enum Direction { UP, DOWN, LEFT, RIGHT };
void pan(Direction dir);

void enforcePanLimits();

void zoomIn();
void zoomOut();

int zoomed(int value);
int unzoomed(int value);
double zoomed(double value);
double unzoomed(double value);

void drawPoint(MapPoint &mapLoc, Color color, int radius = 0);

void initUI();
