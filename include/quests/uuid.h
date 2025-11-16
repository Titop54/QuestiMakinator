#ifndef UUID_QUESTS_H
#define UUID_QUESTS_H

#include <random>
#include <string>
#include <sstream>

namespace uuid
{
//https://stackoverflow.com/questions/24365331/how-can-i-generate-uuid-in-c-without-using-boost-library
class UUIDGenerator
{
private:
    std::random_device rd;
    std::mt19937 gen;
    std::uniform_int_distribution<> dis;
    std::uniform_int_distribution<> dis2;

public:
    UUIDGenerator() : gen(rd()), dis(0, 15), dis2(8, 11) {}

    /**
     * Generate a uuid and then truncate it to the first 16 upper case characters
     * This is done to generate IDs valid for FTB quests
     */
    std::string generate_v4()
    {
        std::stringstream ss;
        ss << std::hex;
        
        for (int i = 0; i < 8; i++) ss << dis(gen);
        
        for(int i = 0; i < 4; i++) ss << dis(gen);
        
        ss << "4";
        
        for(int i = 0; i < 3; i++) ss << dis(gen);
        
        ss << dis2(gen);
        
        for(int i = 0; i < 3; i++) ss << dis(gen);
        for(int i = 0; i < 12; i++) ss << dis(gen);

        std::string string = ss.str().substr(0,16);
        for (size_t i = 0; i < string.length(); i++) if(string[i] >= 'a' && string[i] <= 'z') string[i] = string[i] - 32;

        return string;
    }
};

inline std::string generate_v4()
{
    static UUIDGenerator generator;
    return generator.generate_v4();
}

}

#endif