#include "Vehicle.h"
#include "VehicleType.h"

Vehicle::Vehicle(const VehicleType *type, const MapPoint &loc):
Object(type, loc){}
