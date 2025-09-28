#include "logger/logger.h"
#include <filesystem>
#include <string>

bool Logger::add_text(std::string text)
{
    if(this->is_opened)
    {
        if(std::filesystem::exists("logs/"+this->name))
        {
            this->log << text << std::endl;
            return true;
        }
    }
    return false;
}

std::string Logger::get_text()
{
    if(this->is_opened)
    {
        if(std::filesystem::exists("logs/"+this->name))
        {
            std::string text = "";
            std::string result = "";
            while(std::getline(this->log, text))
            {
                result += text;
            }
            return result;
        }
    }
    return "";
}

bool Logger::create_log(std::string name)
{
    if(!std::filesystem::exists("logs"))
    {
        std::filesystem::create_directory("logs");
    }
    this->log = std::fstream("logs/"+name, std::ios::out);

    this->is_opened = true;
    this->name = name;
    if(std::filesystem::exists("logs/"+this->name)) return true;
    return false;
}

bool Logger::is_open(Logger log)
{
    return log.is_opened && log.log.is_open();
}

bool Logger::close_log()
{
    this->log.close();
    this->is_opened = false;
    return true;
}

