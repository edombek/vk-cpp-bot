#pragma once

#include <chrono>
#include <ctime>
#include <string>

class timer {
private:
    std::chrono::time_point<std::chrono::system_clock> begin;

public:
    timer();
    ~timer();

    uintmax_t getWorked();

    static void init();
    static std::string getTime(time_t tp);
    static std::string getWorkTime();
};
