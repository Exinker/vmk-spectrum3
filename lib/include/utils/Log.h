#pragma once

#include <functional>
#include <string>

namespace hwlib {
    typedef std::function<void(std::string message)> LogCallback;

    class Log {
    public:
        static LogCallback instance;
    };
}
