// (C) 2015 Tim Gurto

#ifndef LOG_CONSOLE_H
#define LOG_CONSOLE_H

#include <fstream>
#include <iostream>
#include <string>

#include "../Log.h"

// A message log that writes to the console.
class LogConsole : public Log{
public:
    LogConsole(const std::string &logFileName = "");
    ~LogConsole();

    void operator()(const std::string &message, const Color &color = Color::NO_KEY);

    template<typename T>
    LogConsole &operator<<(const T &val) {
        std::cout << val;
        if (_logFile.is_open())
            _logFile << val;
        return *this;
    }
    LogConsole &operator<<(const std::string &val);
    // endl: end message and begin a new one
    LogConsole &operator<<(const LogEndType &val);
    // color: set color of current compilation
    LogConsole &operator<<(const Color &c);

private:
    std::ofstream _logFile;
};

#endif
