#ifndef REWARD_QUESTS_H
#define REWARD_QUESTS_H

#include <files/basic_header.h>
#include <cstdint>
#include <string>
#include <stdlib.h>
#include <parser/parser.h>

/**
 * @brief Represent a reward from a quest
 * Since each reward is a quest, they have an ID
 * Then, they have a table_id which is the file name (table_id.snbt)
 * And a a reward with a count
 * 
 * Optionally, it can be converted to an reward table
 * when reward_table is set to true
 * This will allow more than one input
 * 
 */
class reward
{
private:

public:
    std::string title;
    std::string icon; //icon{id: "modid:item"}
    snbt::List tags;
    Hide team_reward = Hide::DEFAULT;
    Hide auto_claim = Hide::DEFAULT;
    bool exclude_from_claim_all = false;
    bool ignore_reward_blocking = false;
    std::string item_id;
    uint16_t count = 1; //min = 1 and max = 8192
    uint16_t random_bonus = 0; //min = 0 and max = 8192
    bool only_one = false;
    
    //Needed for both
    std::string id; //ftb uuid and file name if its a reward table
    

    reward() = default;
    ~reward() = default;


    /**
     * @brief Generate a string representation of the quests (basically snbt::to_string)
     * 
     * @return std::string 
     */
    std::string generateReward();

    reward generateItemReward();
    reward generateChoiseReward();
    reward generateALLTableReward();
    reward generateRandomReward();
    reward generateLootReward();
    reward generateCommandReward();
    reward generateXPReward();
    reward generateXPLevelReward();
    reward generateAdvancementReward();
    reward generateToeastReward();
    reward generateStageReward();
};

#endif