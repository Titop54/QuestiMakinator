#include <quests/chapter.h>
#include <quests/chapter_group.h>
#include <sstream>
#include <filesystem>
#include <fstream>
#include <quests/uuid.h>

chapter::chapter()
{
    this->id = uuid::generate_v4();
    this->order = 1;
}

chapter::~chapter(){}

void chapter::changeFileName(std::string name)
{
    this->file_name = name;
}

void chapter::setAutoFocus(std::string quest_id)
{
    this->autofocus = quest_id;
}

void chapter::hideDependecyLines(bool hide)
{
    this->hide_dependency_lines = hide;
}

bool chapter::changeGroup(std::string group_id)
{
    if(quests::changeGroup(group_id))
    {
        this->group = group_id;
        return true;
    }
    return false;
}

void chapter::changeIcon(std::string icon_id)
{
    this->icon = icon_id;
}

bool chapter::addQuest(quest q)
{
    for(auto quest : this->quests)
    {
        if(q == quest)
        {
            return false;
        }
    }
    this->quests.push_back(q);
    return true;
}

bool chapter::removeQuest(quest q)
{
    for(auto quest : this->quests)
    {
        if(quest == q)
        {
            return true;
        }
    }
    return false;
}

bool chapter::generateQuestFile()
{
    std::stringstream ss;
    const std::string indent0(4, ' ');
    const std::string indent1(8, ' ');
    const std::string indent2(12, ' ');

    // Inicio del objeto
    ss << "{\n";

    // Propiedades del chapter
    if(!autofocus.empty())
    {
        ss << indent0 << "autofocus_id: \"" << autofocus << "\"\n";
    }

    ss << indent0 << "default_hide_dependency_lines: " 
       << (hide_dependency_lines ? "true" : "false") << "\n";
    
    ss << indent0 << "default_quest_shape: \"\"\n";
    ss << indent0 << "filename: \"" << file_name << "\"\n";
    
    ss << indent0 << "group: \"" << (group.empty() ? "" : group) << "\"\n";
    
    if(!icon.empty())
    {
        ss << indent0 << "icon: {\n" 
           << indent1 << "id: \"" << icon << "\"\n" 
           << indent0 << "}\n";
    }
    
    ss << indent0 << "id: \"" << id << "\"\n";
    ss << indent0 << "order_index: " << std::to_string(order) << "\n";
    
    // Array de quests
    ss << indent0 << "quests: [\n";
    for(auto q : quests)
    {
        std::istringstream rs(q.generateQuest());
        std::string line;
        while(std::getline(rs, line))
        {
            if(!line.empty())
            {
                ss << indent1 << line << "\n";
            }
        }
    }
    ss << indent0 << "]\n";
    
    // Cierre del objeto
    ss << "}\n";

    // Crear directorio si no existe
    std::filesystem::path dir("chapter");
    if(!std::filesystem::exists(dir))
    {
        std::filesystem::create_directory(dir);
    }

    // Escribir archivo
    std::ofstream file(dir / (file_name + ".snbt"));
    if(file.is_open())
    {
        file << ss.str();
        file.close();
        return true;
    }
    file.close();
    return false;
}