#pragma once

#include <map>

class Object;
class User;

struct Action {
    using Function = void(*)(const Object &obj, User &performer, const std::string &textArg);
    using FunctionMap = std::map<std::string, Function>;

    static FunctionMap functionMap;

    Function function = nullptr;
};

// More specific functions for certain stimuli.
struct CallbackAction {
    using Function = void(*)(const Object &obj);
    using FunctionMap = std::map<std::string, Function>;

    static FunctionMap functionMap;

    Function function = nullptr;
};
