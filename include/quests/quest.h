#ifndef QUEST_QUESTS_H
#define QUEST_QUESTS_H

#include "parser/parser.h"
#include <cstdint>
#include <vector>
#include <string>
#include <quests/reward.h>

struct connected_nodes;

class quest
{
private:

public:
    //quest property
    std::string id; //quest id
    std::string icon; //icon of the quest
    snbt::List tags; //Tags for kubejs events (should only have snbt::String)
    bool disable_completion_toast; //If the textbox from completing should appear

    //apprearance
    Shapes shape;
    float size; //8 max, min 0.10
    vec2 position = {0, 0};
    uint16_t min_opened_quest_window_width = 0; //0 to 3000
    float icon_scaling = 1; //0.10 min, 2 max

    //dependecies
    std::vector<std::string> needs_node_completed; //when a node is linked, their id is store here
    DependecyMode dependency_requirement = DependecyMode::ALL_COMPLETED;
    uint32_t min_required_dependecies = 0; //this should be keep to 0
    Hide hide_dependency_lines = Hide::DEFAULT; //if the lines from node A to B should be hidden
    bool hide_dependent_lines = false; //if the lines from nodes B to A should be hidden

    //Misc
    std::string guide_page; //link to an guide url (needs to check if its a url)
    Hide disable_jei_recipe = Hide::DEFAULT; //Cant open jei / emi
    Hide repeatable_quest = Hide::DEFAULT; //Can repeat the quest
    bool optional_quest = false; //Its optional to complete the chapter
    bool ignore_reward_blocking = false;
    Progression progression = Progression::DEFAULT; //Default should be the default
    Hide sequential_task_completion = Hide::DEFAULT; //if tasks should be completed one by one or could completed a locked quest but not receive the items

    //visibility
    Hide hide_until_deps_completed = Hide::DEFAULT; //Hide until all dependencies are completed
    Hide hide_until_deps_visible = Hide::DEFAULT; //Hide until all dependencies are visible
    bool invisible_until_completed = false; //invisible until completed, like an easter egg
    uint32_t invisible_until_X_completed = 0; //invisible until X number of quests are completed
    Hide hide_details_until_startable = Hide::DEFAULT; //hide until it can be started, sort of make the player focus
    Hide hide_text_until_completed = Hide::DEFAULT; //Hide text until the quest is completed


    std::vector<snbt::Tag> tasks; //all the tasks you need to do, 1 entry per task
    std::vector<reward> rewards; //all the rewards you put
    std::vector<connected_nodes> linked_nodes; //linked node aka dependency of other nodes, needed for dfs

    //text
    std::string title; //ignore
    std::string subtitle; //ignore
    std::vector<std::string> description; //ignore


    quest() = default;
    ~quest() = default;

    bool operator==(quest const& obj) const
    {
        return this->id == obj.id;
    }

    bool has_loop();
    bool addConnection(connected_nodes n);
    bool hasNode(quest n);

    std::string generateQuest();

    quest generateItemQuest();
    quest generateFluidQuest();
    quest generateEnergyQuest();
    quest generateCustomQuest();
    quest generateXPLevelQuest();
    quest generateVisitDimensionQuest();
    quest generateStatQuest();
    quest generateKillEntityQuest();
    quest generateLocationQuest();
    quest generateCheckmarkQuest();
    quest generateAdvancementQuest();
    quest generateVisitBiome();
    quest generateFindStructureQuest();
    quest generateStageQuest();
};

//Tell how this quest is connected with other nodes
struct connected_nodes
{
    //value of the direction
    int value;

    //Dst quest
    quest* node_linked;
};

#endif