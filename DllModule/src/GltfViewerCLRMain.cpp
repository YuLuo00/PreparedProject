

#include "DllModuleInterface.h"

#include <chrono>
#include <thread>

void test_function()
{
    for (size_t i = 0; i < 10; i++) {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    return;
}