#pragma once
#include "parser/arguments.h"
#include "parser/snbt.h"
#include <cstddef>
#include <filesystem>
#include <ios>
#include <iostream>
#include <ostream>

namespace ftb_merger
{
    namespace fs = std::filesystem;

    inline void read_and_split(const fs::path& path)
    {
        snbt::Compound merge;
        std::string lang_name = path.stem();
        //files to check

        std::vector<std::string> names = { //different type
            "file",
            "chapter",
            "chapter_group",
            "reward",
            "reward_table",
            "task"
        };

        for(auto& file : fs::recursive_directory_iterator(path))
        {
            if(file.is_directory()) continue;
            if(!file.path().has_stem()) continue;

            for(size_t i = 0; i < names.size(); i++)
            {
                if(names[i] == file.path().stem().string())
                {
                    names[i] = "";
                    break;
                }
            }

            snbt::SnbtParser parser(file);
            snbt::Compound parsed_elements = parser.get_tag().cast<snbt::Compound>();

            for(auto& element : parsed_elements)
            {
                merge[element.first] = element.second;
            }
            parser.close();
        }

        for(size_t i = 0; i < names.size(); i++)
        {
            if(names[i] != "")
            {
                std::cerr << "Missing information for " << names[i] << "\n";
            }
        }

        std::string parent = path.parent_path().parent_path().string() + "/lang/";
        if(!fs::exists(parent))
        {
            std::cerr << "There is no lang folder, creating it\n";
            fs::create_directory(parent);
        }

        std::ofstream file;
        std::string file_name = parent + lang_name + ".snbt";
        
        if(fs::exists(file_name))
        {
            fs::remove(file_name);
        }

        file.open(file_name, std::ios::out);
        snbt::Tag(merge).print(file, 4, false);
        
        file.close();
        merge.clear();
    }

    inline void merge(args::parser args)
    {
        std::string path = args.result.contains("--path") ? args.result["--path"][0] : ".";
        path += "/config/ftbquests/quests/split";
        
        if(!fs::exists(path))
        {
            std::cerr << "There is no files to merge\n";
            return;
        }

        for(auto& file : fs::directory_iterator(path))
        {
            read_and_split(file.path());
        }
    }
}
