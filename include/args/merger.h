#pragma once
#include "parser/arguments.h"
#include "parser/snbt.h"
#include <cstddef>
#include <filesystem>
#include <fstream>
#include <ios>
#include <iostream>
#include <ostream>

namespace merger
{
    namespace fs = std::filesystem;

    inline void read_and_split(const fs::path& path, bool use_json, bool convert)
    {
        snbt::Compound merge;
        std::string lang_name = path.stem().string();

        std::vector<std::string> names = {
            "file",
            "chapter",
            "chapter_group",
            "reward",
            "reward_table",
            "task",
            "image",
            "misc"
        };

        for(auto& file : fs::recursive_directory_iterator(path))
        {
            if(file.is_directory()) continue;
            if(!file.path().has_stem()) continue;

            std::string expected_ext = convert ? ".json" : ".snbt";
            if(!file.path().has_extension() || file.path().extension().string() != expected_ext) continue;

            for(size_t i = 0; i < names.size(); i++)
            {
                if(names[i] == file.path().stem().string())
                {
                    names[i] = "";
                    break;
                }
            }

            snbt::Compound parsed_elements;

            if(convert)
            {
                std::ifstream ifs(file.path());
                nlohmann::json j = nlohmann::json::parse(ifs, nullptr, false, true);
                if(!j.is_discarded())
                {
                    parsed_elements = snbt::json_to_tag(j).cast<snbt::Compound>();
                }
            }
            else
            {
                snbt::SnbtParser parser(file);
                parsed_elements = parser.get_tag().cast<snbt::Compound>();
                parser.close();
            }

            for(auto& element : parsed_elements)
            {
                merge[element.first] = element.second;
            }
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

        std::string out_ext = use_json ? ".json" : ".snbt";
        std::string file_name = parent + lang_name + out_ext;
        
        if(fs::exists(file_name))
        {
            fs::remove(file_name);
        }

        std::ofstream file;
        file.open(file_name, std::ios::out);
        
        if(use_json)
        {
            snbt::Tag(merge).to_json(file, 0, 4);
        }
        else
        {
            snbt::Tag(merge).print(file, 4, false);
        }
        
        file.close();
        merge.clear();
    }

    inline void merge(args::parser args)
    {
        bool use_json = args.result.contains("--json");
        bool convert = args.result.contains("--convert");

        std::string path = args.result.contains("--path") ? args.result["--path"][0] : ".";
        path += "/config/ftbquests/quests/split";
        
        if(!fs::exists(path))
        {
            std::cerr << "There is no files to merge\n";
            return;
        }

        for(auto& file : fs::directory_iterator(path))
        {
            read_and_split(file.path(), use_json, convert);
        }
    }
}
