#include <quests/chapter_group.h>

chapter_group::chapter_group()
{
    this->chapters.push_back(chapter());
}

chapter_group::~chapter_group(){}

std::vector<std::string> chapter_group::getChapterGroup()
{
    return this->chapters_group;
}