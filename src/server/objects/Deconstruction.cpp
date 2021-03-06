#include "Deconstruction.h"

DeconstructionType *DeconstructionType::ItemAndTime(
    const ServerItem *itemThisBecomes, ms_t timeToDeconstruct) {
  return new DeconstructionType(itemThisBecomes, timeToDeconstruct);
}

DeconstructionType::DeconstructionType(const ServerItem *itemThisBecomes,
                                       ms_t timeToDeconstruct)
    : _itemThisBecomes(itemThisBecomes),
      _timeToDeconstruct(timeToDeconstruct) {}

Deconstruction::Deconstruction(Object &parent, const DeconstructionType &type)
    : _parent(&parent), _type(&type) {}
