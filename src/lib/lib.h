//
// Created by Matteo on 11/08/16.
//

#ifndef CLAUSE_SERVER_LIB_H
#define CLAUSE_SERVER_LIB_H

#include <string>
#include <sstream>
#include <map>
#include <vector>
#include <functional>


std::istream &split(std::istream &stream,
                    const char delimiter,
                    std::function<void(const std::string &)> callback);

std::istream &split(std::istream &stream,
                    const char delimiter,
                    std::vector<std::string> &vector);

void split(const std::string &, const std::string &, std::vector<std::string> &, uint32_t limit = 0);

void split(const std::string &, const std::string &, std::function<void(const std::string &)>, uint32_t limit = 0);

template<typename T>
std::ostream &join(std::ostream &stream, const std::string &delimiter, const std::vector<T> &vector) {
    for (auto it = vector.begin(); it != vector.end(); ++it) {
        stream << *it;
        if (it + 1 != vector.end())
            stream << delimiter;
    }
    return stream;
}

void replace(std::string &, const std::string &, const std::string &);

template<typename T>
const std::string to_string(const T &obj) {
    std::stringstream ss;
    ss << obj;
    return ss.str();
}

template<typename T>
std::ostream &operator<<(std::ostream &stream, const std::vector<T> &v) {
    for (auto &i:v) {
        stream << i << '\0';
    }
    return stream;
}

template<typename T>
std::istream &operator>>(std::istream &stream, std::vector<T> &v) {
    return ::split(stream, '\0', [&](const std::string &sub) {
        if (sub.size() == 0)
            return;
        T t;
        std::istringstream(sub) >> t;
        v.push_back(t);
    });
}

#include "Exception.h"
#include "Logger.h"
#include "Process.h"
#include "Thread.h"
#include "sqlite3.h"
#include "net.h"

#endif //CLAUSE_SERVER_LIB_H
