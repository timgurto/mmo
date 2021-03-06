#pragma once

#include <vector>

#include "Point.h"
#include "XmlReader.h"

class Map {
 public:
  static const px_t TILE_W = 32, TILE_H = 32;

  void loadFromXML(XmlReader& xr);

  size_t width() const { return _w; }
  size_t height() const { return _h; }

  const std::vector<char>& operator[](size_t x) const { return _grid[x]; }
  std::vector<char>& operator[](size_t x) { return _grid[x]; }

  size_t getRow(double yCoord) const;
  size_t getCol(double xCoord, size_t row) const;
  static MapRect getTileRect(size_t x, size_t y);
  std::set<char> terrainTypesOverlapping(const MapRect& rect,
                                         double extraRadius = 0) const;
  char getTerrainAtPoint(const MapPoint& p) const;

  MapPoint randomPoint() const;
  static MapPoint randomPointInTile(size_t x, size_t y);

  size_t to1D(size_t x, size_t y) const;
  std::pair<size_t, size_t> from1D(size_t i) const;

 private:
  std::vector<std::vector<char> > _grid;
  size_t _w{0}, _h{0};
};
