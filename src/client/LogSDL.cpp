#include <fstream>

#include "Client.h"
#include "LogSDL.h"
#include "ui/Label.h"
#include "ui/List.h"

LogSDL::LogSDL(const std::string &logFileName):
_compilationColor(Color::FONT),
_logFileName(logFileName){
    if (!logFileName.empty()) {
        std::ofstream of(logFileName);
        of.close();
    }
}

void LogSDL::writeToFile(const std::string &msg) const{
    if (_logFileName.empty())
        return;
    std::ofstream of(_logFileName, std::ios_base::app);
    of << msg << std::endl;
    of.close();
}

void LogSDL::operator()(const std::string &message, const Color &color){
    writeToFile(message);
    Client::_instance->addChatMessage(message, color);
}

LogSDL &LogSDL::operator<<(const std::string &val) {
    _oss << val;
    return *this;
}

LogSDL &LogSDL::operator<<(const LogSpecial &val) {
    switch (val){
    case endl:
        operator()(_oss.str(), _compilationColor);
        _oss.str("");
        _compilationColor = Color::FONT; // reset color for next compilation
        break;

    case uncolor:
        _compilationColor = Color::FONT;
        break;
    }
    return *this;
}
    
LogSDL &LogSDL::operator<<(const Color &c) {
    _compilationColor = c;
    return *this;
}
