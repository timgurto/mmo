#include "Wars.h"

bool Wars::Belligerent::operator<(const Belligerent &rhs) const{
    if (name != rhs.name)
        return name < rhs.name;
    return type < rhs.type;
}

bool Wars::Belligerent::operator==(const Belligerent &rhs) const{
    return name == rhs.name && type == rhs.type;
}

void Wars::declare(const Belligerent &a, const Belligerent &b){
    container.insert(std::make_pair(a, b));
    container.insert(std::make_pair(b, a));
}

bool Wars::isAtWar(const Belligerent &a, const Belligerent &b) const{
    auto matches = container.equal_range(a);
    for (auto it = matches.first; it != matches.second; ++it){
        if (it->second == b)
            return true;
    }
    return false;
}

std::pair<Wars::container_t::const_iterator, Wars::container_t::const_iterator>
        Wars::getAllWarsInvolving(const Belligerent &a) const{

    return container.equal_range(a);
}