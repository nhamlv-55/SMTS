//
// Created by Matteo on 19/07/16.
//

#ifndef CLAUSE_SHARING_LOG_H
#define CLAUSE_SHARING_LOG_H

#include <assert.h>
#include <ctime>
#include <iostream>
#include <sstream>
#include <mutex>
#include <iomanip>


class Logger {
private:
    Logger() {}

public:
    template<typename T>
    static void log(uint8_t level, const T &message) {
        static std::mutex mtx;
        std::lock_guard<std::mutex> _l(mtx);
        int r = 0;
        std::stringstream stream;
        std::time_t time = std::time(nullptr);
        struct tm tm = *std::localtime(&time);
        stream << std::put_time(&tm, "%Y-%m-%d %H:%M:%S") << "\t";
        switch (level) {
            case INFO:
                stream << "INFO\t";
                break;
            case WARNING:
                if (getenv("TERM")) {
                    r += system("tput setaf 3");
                    r += system("tput bold");
                }
                stream << "WARNING\t";
                break;
            case ERROR:
                if (getenv("TERM")) {
                    r += system("tput setaf 9");
                    r += system("tput bold");
                }
                stream << "ERROR\t";
                break;
            default:
                stream << "UNKNOWN\t";
        }
        stream << message;
        std::cout << stream.str() << "\n";
        if (getenv("TERM")) {
            r = system("tput sgr0");
        }
    }

    static const uint8_t INFO = 1;
    static const uint8_t WARNING = 2;
    static const uint8_t ERROR = 3;
};


#endif //CLAUSE_SHARING_LOG_H
