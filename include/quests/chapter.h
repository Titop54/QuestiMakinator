#ifndef CHAPTER_QUESTS_H
#define CHAPTER_QUESTS_H

#include <string>
#include <cstdint>
#include <vector>
#include <quests/quest.h>

class chapter
{
private:
    std::string file_name;
    std::string autofocus;
    bool hide_dependency_lines = false;
    std::string group;
    std::string icon;
    std::string id;
    uint8_t order;
    std::vector<std::string> quest_links;
    std::vector<quest> quests;
    
public:
    chapter();
    ~chapter();

    void changeFileName(std::string name);
    void setAutoFocus(std::string quest_id);
    void hideDependecyLines(bool hide);
    bool changeGroup(std::string group_id);
    void changeIcon(std::string icon_id);
    bool addQuest(quest q);
    bool removeQuest(quest target);

    bool generateQuestFile();
};

#endif