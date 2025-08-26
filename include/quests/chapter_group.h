#ifndef CHAPTER_GROUP_QUESTS_H
#define CHAPTER_GROUP_QUESTS_H

#include <vector>
#include <string>
#include <quests/chapter.h>
#include <quests/reward.h>

/**
 * @brief Represent the tabs of quests
 * By default, there is one chapter, which include all
 * quests and rewards
 * While you can add more if you need
 * 
 * This class represents all quests and rewards and will generate all files
 * needed with generateData(), sort of manager of chapters and tables
 */
class chapter_group
{
private:
    //Tells the id of each chapter group, just for 1 file builder
    std::vector<std::string> chapters_group;
    //How many chapter there are, each chapter is a file
    std::vector<chapter> chapters;
    std::vector<reward> reward_tables;
public:
    chapter_group();
    ~chapter_group();

    std::vector<chapter> getChapters();
    std::vector<std::string> getChapterGroup();
    std::vector<reward> getRewardTables();
    bool addRewardTable(reward table);
    chapter* selectChapter(int index);
    std::string generateData();
};

namespace quests
{
    inline bool changeGroup(std::string id)
    {
        static chapter_group group;
        for(auto i : group.getChapterGroup())
        {
            if(i == id) return true;
        }
        return false;
    }
}


#endif