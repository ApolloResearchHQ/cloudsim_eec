#include "Interfaces.h"
#include <iostream>
#include <string>

void SimOutput(string message, unsigned level) {
    if (level <= 1) {
        std::cout << message << std::endl;
    }
}

Time_t Now() {
    static Time_t current_time = 0;
    return current_time += 1000;
}
