#pragma once

#include "parser/arguments.h"
#include "parser/snbt.h"
#include <cstddef>
#include <filesystem>
#include <fstream>
#include <ios>
#include <iostream>
#include <ostream>
#include <stdexcept>
#include <string>
#include <vector>

namespace splitter
{
    namespace fs = std::filesystem;

    inline void construct_folder_and_files(const fs::path& path, std::vector<std::string>& paths, bool use_json)
    {
        std::string destination = path.parent_path().parent_path().string() + "/split/" + path.stem().string();
        if(fs::exists(destination)) //clean
        {
            fs::remove_all(destination);
        }

        fs::create_directories(destination + "/misc/");

        std::vector<std::string> names = { //different type
            "file",
            "chapter",
            "chapter_group",
            "reward",
            "reward_table",
            "task"
        };

        std::string ext = use_json ? ".json" : ".snbt";

        for(auto& relative : names)
        {
            std::string file_path = destination + "/misc/" + relative + ext;
            std::ofstream file;
            file.open(file_path, std::ios::out);
            file.close();
            paths.emplace_back(file_path);
        }
    }

    inline void close(std::string message, snbt::SnbtParser& parser)
    {
        parser.close();
        std::cout << message << std::endl;
    }

    inline void close(std::string message)
    {
        std::cout << message << std::endl;
    }

    inline void read_and_split(const fs::path& path, bool use_json, bool convert)
    {
        std::string expected_ext = convert ? ".json" : ".snbt";
        if(!path.has_extension() || !path.extension().string().ends_with(expected_ext))
        {
            std::cout << "Not a valid file\n" << std::endl;
            return;
        }

        snbt::SnbtParser parser;
        snbt::Tag tag;
        snbt::Compound comp;

        if(convert)
        {
            std::ifstream ifs(path);
            nlohmann::json j = nlohmann::json::parse(ifs);
            tag = snbt::json_to_tag(j);
        }
        else
        {
            tag = parser.parse(path.string(), true);
        }

        if(tag.type != snbt::Tag::Type::Compound)
        {
            if(convert) close("No a valid quest file lang\n");
            else close("No a valid quest file lang\n", parser);
            return;
        }

        comp = std::get<snbt::Compound>(tag.value);

        if(!convert && !parser.has_content())
        {
            close("Empty file with quests\n", parser);
        }
        else if(convert && comp.empty())
        {
            close("Empty file with quests\n");
        }

        std::vector<std::string> paths;
        construct_folder_and_files(path, paths, use_json);
        std::string quests = path.parent_path().parent_path().string() + "/chapters";

        if(!fs::exists(quests))
        {
            if(convert) close("No quests to examine\n");
            else close("No quests to examine\n", parser);
            return;
        }

        for(auto& file : fs::directory_iterator(quests))
        {
            if(!file.exists())
            {
                continue;
            }

            if(!file.is_regular_file())
            {
                continue;
            }

            if(!file.path().has_extension())
            {
                continue;
            }

            if(file.path().extension().string() != expected_ext)
            {
                continue;
            }

            snbt::SnbtParser p;
            snbt::Tag quest;

            if(convert)
            {
                std::ifstream ifs(file.path());
                nlohmann::json j = nlohmann::json::parse(ifs);
                quest = snbt::json_to_tag(j);
            }
            else
            {
                p = snbt::SnbtParser(file);
                quest = p.get_tag();
            }

            if(quest.type != snbt::Tag::Type::Compound)
            {
                if(convert) close("Not a valid quest\n");
                else close("Not a valid quest\n", p);
                continue;
            }

            snbt::Compound q = std::get<snbt::Compound>(quest.value);
            if(!q.contains("quests"))
            {
                if(convert) close("There is no quests\n");
                else close("There is no quests\n", p);
                continue;
            }

            auto qs = q["quests"];
            if(qs.type != snbt::Tag::Type::List)
            {
                if(convert) close("Not a list\n");
                else close("Not a list\n", p);
                continue;
            }

            auto list = std::get<snbt::List>(qs.value);
            snbt::Compound text_to_write;

            std::string output_dir = path.parent_path().parent_path().string() + "/split/" + path.stem().string() + "/";

            if(!fs::exists(output_dir))
            {
                fs::create_directories(output_dir);
            }
            
            std::ofstream of;
            std::string out_ext = use_json ? ".json" : file.path().extension().string();
            of.open((output_dir + file.path().stem().string() + out_ext), std::ios::out);

            for(auto& individial : list)
            {
                if(individial.type != snbt::Tag::Type::Compound)
                {
                    continue;
                }
                
                auto compo = std::get<snbt::Compound>(individial.value);
                if(!compo.contains("id"))
                {
                    continue;
                }

                std::string title = "quest." + std::get<std::string>(compo["id"].value) + ".title";
                std::string subtitle = "quest." + std::get<std::string>(compo["id"].value) + ".quest_subtitle";
                std::string description = "quest." + std::get<std::string>(compo["id"].value) + ".quest_desc";

                if(comp.contains(title))
                {
                    text_to_write[title] = comp[title];
                }

                if(comp.contains(subtitle))
                {
                    text_to_write[subtitle] = comp[subtitle];
                }

                if(comp.contains(description))
                {
                    text_to_write[description] = comp[description];
                }
            }

            snbt::Tag tag_to_write(text_to_write);
            if(use_json)
            {
                tag_to_write.to_json(of, 0, 4);
            }
            else
            {
                tag_to_write.print(of, 4);
            }
            of.close();

            if(!convert)
            {
                p.close();
            }
        }

        std::vector<snbt::Compound> files_to_handle = {
            {},{},{},{},{},{}
        };

        for(auto& misc : comp)
        {
            /*
            std::vector<std::string> names = { //different type
                "file", 0
                "chapter", 1
                "chapter_group", 2
                "reward", 3
                "reward_table", 4
                "task" 5
            };
            */

            if(misc.first.starts_with("file"))
            {
                files_to_handle[0][misc.first] = misc.second;
            }
            else if(misc.first.starts_with("chapter_group")) //if we put chapter first, it will include this one
            {
                files_to_handle[2][misc.first] = misc.second;
            }
            else if(misc.first.starts_with("chapter"))
            {
                files_to_handle[1][misc.first] = misc.second;
            }
            else if(misc.first.starts_with("reward_table")) //the same here as chapter_group
            {
                files_to_handle[4][misc.first] = misc.second;
            }
            else if(misc.first.starts_with("reward"))
            {
                files_to_handle[3][misc.first] = misc.second;
            }
            else if(misc.first.starts_with("task"))
            {
                files_to_handle[5][misc.first] = misc.second;
            }
            else if(!misc.first.starts_with("quest"))
            {
                throw std::runtime_error("We got some quests not done!!!\n" + misc.first);
            }
        }
        
        size_t counter = 0;
        for(auto& pth : paths)
        {
            std::ofstream f;
            f.open(pth, std::ios::out);
            snbt::Tag tag(files_to_handle[counter]);
            counter++;
            if(use_json)
            {
                tag.to_json(f, 0, 4);
            }
            else
            {
                tag.print(f, 4);
            }
            f.close();
        }

        if(!convert)
        {
            parser.close();
        }
    }

    inline void split(args::parser args)
    {
        bool use_json = args.result.contains("--json");
        bool convert = args.result.contains("--convert");

        std::string path = args.result.contains("--path") ? args.result["--path"][0] : ".";
        path += "/config/ftbquests/quests/lang";
        
        if(!fs::exists(path)) return;
        
        for(auto& file : fs::directory_iterator(path))
        {
            if(file.is_regular_file())
            {
                if(args.result.contains("--lang"))
                {
                    for(auto& lang : args.result["--lang"])
                    {
                        if(file.path().stem().string().contains(lang))
                        {
                            read_and_split(file.path(), use_json, convert);
                        }
                    }
                    continue;
                }
                //We dont have the --lang flag, so we dont care 
                read_and_split(file.path(), use_json, convert);
            }
        }
    }
}