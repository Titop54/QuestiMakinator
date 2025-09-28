#ifndef LOGGER_HPP
#define LOGGER_HPP

#include <fstream>
#include <string>

class Logger
{

private:
    std::fstream log;
    std::string name;
    bool is_opened;

public:

    Logger() = default;

    ~Logger() = default;

    bool add_text(std::string text);

    std::string get_text();

    bool create_log(std::string name);

    bool close_log();

    bool is_open(Logger log);

};

#endif