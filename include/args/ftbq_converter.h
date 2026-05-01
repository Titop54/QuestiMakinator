#pragma once

#include "parser/arguments.h"
#include "parser/snbt.h"
#include <filesystem>
#include <fstream>
#include <ios>
#include <iostream>
#include <ostream>
#include <string>
#include <nlohmann/json.hpp>

namespace ftbq_converter
{
    namespace fs = std::filesystem;

    inline snbt::Compound convert_tag_to_components(const snbt::Compound& tag_compound, const std::string& context)
    {
        snbt::Compound components;

        for(const auto& [key, value] : tag_compound)
        {
            if(key == "Damage")
            {
                components["\"minecraft:damage\""] = value;
            }
            else if(key == "display")
            {
                if(value.type == snbt::Tag::Type::Compound)
                {
                    const auto& display = std::get<snbt::Compound>(value.value);
                    if(display.contains("Name"))
                    {
                        components["\"minecraft:custom_name\""] = display.at("Name");
                    }
                    if(display.contains("Lore"))
                    {
                        components["\"minecraft:lore\""] = display.at("Lore");
                    }
                }
            }
            else if(key == "Enchantments")
            {
                if(value.type == snbt::Tag::Type::List)
                {
                    snbt::Compound levels;
                    const auto& ench_list = std::get<snbt::List>(value.value);
                    for(const auto& ench : ench_list)
                    {
                        if(ench.type != snbt::Tag::Type::Compound) continue;
                        const auto& ec = std::get<snbt::Compound>(ench.value);
                        if(!ec.contains("id")) continue;

                        std::string ench_id = std::get<snbt::String>(ec.at("id").value);
                        snbt::Tag level_tag(snbt::Int(1));

                        if(ec.contains("lvl"))
                        {
                            const auto& lvl = ec.at("lvl");
                            if(lvl.type == snbt::Tag::Type::Short)
                                level_tag = snbt::Tag(snbt::Int(std::get<snbt::Short>(lvl.value)));
                            else if(lvl.type == snbt::Tag::Type::Int)
                                level_tag = lvl;
                        }
                        levels[ench_id] = level_tag;
                    }

                    snbt::Compound ench_component;
                    ench_component["levels"] = snbt::Tag(levels);
                    components["\"minecraft:enchantments\""] = snbt::Tag(ench_component);
                }
            }
            else if(key == "CustomModelData")
            {
                components["\"minecraft:custom_model_data\""] = value;
            }
            else if(key == "Unbreakable")
            {
                components["\"minecraft:unbreakable\""] = snbt::Tag(snbt::Compound{});
            }
            else if(key == "RepairCost")
            {
                components["\"minecraft:repair_cost\""] = value;
            }
            else
            {
                std::cerr << "[WARN] Unknown tag key \"" << key << "\" in " << context
                          << " — skipping conversion, manual review required.\n";
            }
        }
        return components;
    }

    inline void convert_item_stack(snbt::Compound& item, const std::string& context)
    {
        if(item.contains("Count"))
        {
            snbt::Tag count_tag = item["Count"];
            snbt::Int count_val = 1;

            if(count_tag.type == snbt::Tag::Type::Byte)
                count_val = static_cast<snbt::Int>(std::get<snbt::Byte>(count_tag.value));
            else if(count_tag.type == snbt::Tag::Type::Int)
                count_val = std::get<snbt::Int>(count_tag.value);
            else if(count_tag.type == snbt::Tag::Type::Short)
                count_val = static_cast<snbt::Int>(std::get<snbt::Short>(count_tag.value));

            item.erase("Count");
            item["count"] = snbt::Tag(count_val);
        }

        if(item.contains("tag"))
        {
            if(item["tag"].type == snbt::Tag::Type::Compound)
            {
                auto tag_compound = std::get<snbt::Compound>(item["tag"].value);

                if(item.contains("id"))
                {
                    std::string item_id = std::get<snbt::String>(item["id"].value);
                    if(item_id == "itemfilters:or" || item_id == "itemfilters:and" ||
                        item_id == "itemfilters:not" || item_id == "itemfilters:tag")
                    {
                        if(tag_compound.contains("items") &&
                            tag_compound["items"].type == snbt::Tag::Type::List)
                        {
                            auto& items_list = std::get<snbt::List>(tag_compound["items"].value);
                            for(auto& inner_item : items_list)
                            {
                                if(inner_item.type == snbt::Tag::Type::Compound)
                                {
                                    auto inner = std::get<snbt::Compound>(inner_item.value);
                                    convert_item_stack(inner, context);
                                    inner_item = snbt::Tag(inner);
                                }
                            }
                        }
                        return;
                    }
                }

                snbt::Compound components = convert_tag_to_components(tag_compound, context);
                item.erase("tag");
                if(!components.empty())
                {
                    item["components"] = snbt::Tag(components);
                }
            }
        }
    }

    inline void convert_compound_recursive(snbt::Tag& tag, const std::string& context)
    {
        if(tag.type == snbt::Tag::Type::Compound)
        {
            auto& comp = std::get<snbt::Compound>(tag.value);

            if(comp.contains("id") && (comp.contains("Count") || comp.contains("tag")))
            {
                std::string id_str;
                if(comp["id"].type == snbt::Tag::Type::String)
                    id_str = std::get<snbt::String>(comp["id"].value);

                if(id_str.find(':') != std::string::npos)
                {
                    convert_item_stack(comp, context + " / item " + id_str);
                }
            }

            for(auto& [key, child] : comp)
            {
                convert_compound_recursive(child, context);
            }
        }
        else if(tag.type == snbt::Tag::Type::List)
        {
            auto& list = std::get<snbt::List>(tag.value);
            for(auto& child : list)
            {
                convert_compound_recursive(child, context);
            }
        }
    }

    inline void convert_1_20_to_1_21(args::parser args)
    {
        std::string base_path = args.result.contains("--path") ? args.result["--path"][0] : ".";
        std::string chapters_path = base_path + "/config/ftbquests/quests/chapters";

        if(!fs::exists(chapters_path))
        {
            std::cerr << "No chapters directory found at: " << chapters_path << "\n";
            return;
        }

        std::string output_dir = base_path + "/config/ftbquests/quests/1_21_1/chapters";
        fs::create_directories(output_dir);

        for(auto& file : fs::directory_iterator(chapters_path))
        {
            if(!file.is_regular_file()) continue;
            if(!file.path().has_extension()) continue;
            if(file.path().extension().string() != ".snbt") continue;

            std::string filename = file.path().stem().string();
            std::string context = filename + ".snbt";

            snbt::SnbtParser parser(file);
            snbt::Tag tag = parser.get_tag();
            parser.close();

            if(tag.type != snbt::Tag::Type::Compound)
            {
                std::cerr << "Skipping " << context << " — not a valid compound\n";
                continue;
            }

            convert_compound_recursive(tag, context);

            std::string out_path = output_dir + "/" + filename + ".snbt";
            std::ofstream out(out_path, std::ios::out);
            if(out.is_open())
            {
                tag.print(out, 4);
                out.close();
                std::cout << "Converted: " << context << " → " << out_path << "\n";
            }
            else
            {
                std::cerr << "Failed to write: " << out_path << "\n";
            }
        }

        std::cout << "Conversion complete. Output: " << output_dir << "\n";
    }

    inline void extract_to_lang(args::parser args)
    {
        std::string base_path = args.result.contains("--path") ? args.result["--path"][0] : ".";
        std::string chapters_path = base_path + "/config/ftbquests/quests/chapters";

        if(!fs::exists(chapters_path))
        {
            std::cerr << "No chapters directory found at: " << chapters_path << "\n";
            return;
        }

        std::string output_dir = base_path + "/config/ftbquests/quests/1_20_1/chapters";
        fs::create_directories(output_dir);

        nlohmann::ordered_json lang_json;

        for(auto& file : fs::directory_iterator(chapters_path))
        {
            if(!file.is_regular_file()) continue;
            if(!file.path().has_extension()) continue;
            if(file.path().extension().string() != ".snbt") continue;

            std::string filename = file.path().stem().string();

            snbt::SnbtParser parser(file);
            snbt::Tag tag = parser.get_tag();
            parser.close();

            if(tag.type != snbt::Tag::Type::Compound)
            {
                std::cerr << "Skipping " << filename << ".snbt — not a valid compound\n";
                continue;
            }

            auto& chapter = std::get<snbt::Compound>(tag.value);

            if(chapter.contains("title") && chapter["title"].type == snbt::Tag::Type::String)
            {
                std::string title_text = std::get<snbt::String>(chapter["title"].value);

                if(!title_text.empty() && !(title_text.front() == '{' && title_text.back() == '}'))
                {
                    std::string title_key = "ftbquests.chapter." + filename + ".title";
                    lang_json[title_key] = title_text;
                    chapter["title"] = snbt::Tag(snbt::String("{" + title_key + "}"));
                }
            }

            if(!chapter.contains("quests") || chapter["quests"].type != snbt::Tag::Type::List)
            {
                continue;
            }

            auto& quests = std::get<snbt::List>(chapter["quests"].value);

            for(auto& quest_tag : quests)
            {
                if(quest_tag.type != snbt::Tag::Type::Compound) continue;

                auto& quest = std::get<snbt::Compound>(quest_tag.value);
                if(!quest.contains("id")) continue;

                std::string quest_id = std::get<snbt::String>(quest["id"].value);
                std::string key_prefix = "ftbquests.chapter." + filename + ".quest" + quest_id;

                if(quest.contains("title") && quest["title"].type == snbt::Tag::Type::String)
                {
                    std::string title_text = std::get<snbt::String>(quest["title"].value);

                    if(!title_text.empty() && !(title_text.front() == '{' && title_text.back() == '}'))
                    {
                        std::string title_key = key_prefix + ".title";
                        lang_json[title_key] = title_text;
                        quest["title"] = snbt::Tag(snbt::String("{" + title_key + "}"));
                    }
                }

                if(quest.contains("subtitle") && quest["subtitle"].type == snbt::Tag::Type::String)
                {
                    std::string subtitle_text = std::get<snbt::String>(quest["subtitle"].value);

                    if(!subtitle_text.empty() && !(subtitle_text.front() == '{' && subtitle_text.back() == '}'))
                    {
                        std::string sub_key = key_prefix + ".subtitle";
                        lang_json[sub_key] = subtitle_text;
                        quest["subtitle"] = snbt::Tag(snbt::String("{" + sub_key + "}"));
                    }
                }

                if(quest.contains("description") && quest["description"].type == snbt::Tag::Type::List)
                {
                    auto& desc_list = std::get<snbt::List>(quest["description"].value);
                    int desc_counter = 1;

                    for(auto& desc_entry : desc_list)
                    {
                        if(desc_entry.type != snbt::Tag::Type::String) continue;

                        std::string desc_text = std::get<snbt::String>(desc_entry.value);

                        if(desc_text.empty())
                        {
                            continue;
                        }

                        if(desc_text.front() == '{' && desc_text.back() == '}')
                        {
                            desc_counter++;
                            continue;
                        }

                        std::string desc_key = key_prefix + ".description" + std::to_string(desc_counter);
                        lang_json[desc_key] = desc_text;
                        desc_entry = snbt::Tag(snbt::String("{" + desc_key + "}"));
                        desc_counter++;
                    }
                }

                if(quest.contains("tasks") && quest["tasks"].type == snbt::Tag::Type::List)
                {
                    auto& tasks = std::get<snbt::List>(quest["tasks"].value);
                    for(auto& task_tag : tasks)
                    {
                        if(task_tag.type != snbt::Tag::Type::Compound) continue;
                        auto& task = std::get<snbt::Compound>(task_tag.value);

                        if(!task.contains("title") || task["title"].type != snbt::Tag::Type::String) continue;
                        if(!task.contains("id")) continue;

                        std::string task_title = std::get<snbt::String>(task["title"].value);
                        std::string task_id = std::get<snbt::String>(task["id"].value);

                        if(!task_title.empty() && !(task_title.front() == '{' && task_title.back() == '}'))
                        {
                            std::string task_key = key_prefix + ".task." + task_id + ".title";
                            lang_json[task_key] = task_title;
                            task["title"] = snbt::Tag(snbt::String("{" + task_key + "}"));
                        }
                    }
                }
            }

            std::string out_path = output_dir + "/" + filename + ".snbt";
            std::ofstream out(out_path, std::ios::out);
            if(out.is_open())
            {
                tag.print(out, 4);
                out.close();
                std::cout << "Extracted lang keys from: " << filename << ".snbt\n";
            }
        }

        std::string data_path = base_path + "/config/ftbquests/quests/chapter_groups.snbt";
        if(fs::exists(data_path))
        {
            snbt::SnbtParser data_parser;
            snbt::Tag data_tag = data_parser.parse(data_path, true);
            data_parser.close();

            if(data_tag.type == snbt::Tag::Type::Compound)
            {
                auto& data_comp = std::get<snbt::Compound>(data_tag.value);

                if(data_comp.contains("chapter_groups") &&
                    data_comp["chapter_groups"].type == snbt::Tag::Type::List)
                {
                    auto& groups = std::get<snbt::List>(data_comp["chapter_groups"].value);

                    for(auto& group_tag : groups)
                    {
                        if(group_tag.type != snbt::Tag::Type::Compound) continue;
                        auto& group = std::get<snbt::Compound>(group_tag.value);

                        if(!group.contains("id")) continue;
                        std::string group_id = std::get<snbt::String>(group["id"].value);

                        if(group.contains("title") && group["title"].type == snbt::Tag::Type::String)
                        {
                            std::string title_text = std::get<snbt::String>(group["title"].value);

                            if(!title_text.empty() && !(title_text.front() == '{' && title_text.back() == '}'))
                            {
                                std::string key = "ftbquests.chapter_groups." + group_id + ".title";
                                lang_json[key] = title_text;
                                group["title"] = snbt::Tag(snbt::String("{" + key + "}"));
                            }
                        }
                    }
                }

                std::string data_output_dir = base_path + "/config/ftbquests/quests/1_20_1";
                fs::create_directories(data_output_dir);

                std::ofstream data_out(data_output_dir + "/chapter_groups.snbt", std::ios::out);
                if(data_out.is_open())
                {
                    data_tag.print(data_out, 4);
                    data_out.close();
                    std::cout << "Extracted lang keys from: chapter_groups.snbt\n";
                }
            }
        }

        std::string lang_dir = base_path + "/kubejs/assets/ftbquests/lang";
        fs::create_directories(lang_dir);

        std::string lang_path = lang_dir + "/en_us.json";

        if(fs::exists(lang_path))
        {
            std::ifstream existing(lang_path);
            try
            {
                nlohmann::ordered_json existing_json;
                existing >> existing_json;
                for(auto& [k, v] : existing_json.items())
                {
                    if(!lang_json.contains(k))
                    {
                        lang_json[k] = v;
                    }
                }
            }
            catch(...)
            {
                std::cerr << "[WARN] Could not parse existing " << lang_path << ", overwriting.\n";
            }
        }

        std::ofstream lang_file(lang_path, std::ios::out);
        if(lang_file.is_open())
        {
            lang_file << lang_json.dump(4);
            lang_file.close();
            std::cout << "Lang file written: " << lang_path << "\n";
        }
        else
        {
            std::cerr << "Failed to write lang file: " << lang_path << "\n";
        }
    }
} // namespace ftbq_converter