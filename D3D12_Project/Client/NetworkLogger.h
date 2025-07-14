#pragma once
#include <fstream>
#include <string>

class NetworkLogger {
public:
    static void Log(const char* message) {
        static std::ofstream logFile("client_network.log", std::ios::out | std::ios::app);
        if (logFile.is_open()) {
            logFile << message << std::endl;
            logFile.flush();
        }
    }
}; 