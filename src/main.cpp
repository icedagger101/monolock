#include "LockerApp.h"
#include <iostream>

int main()
{
    try {
        LockerApp app;
        app.run();
    }
    catch (const std::exception& e) {
        std::cerr << "Fatal Error: " << e.what() << std::endl;
        return 1;
    }
    return 0;
}
