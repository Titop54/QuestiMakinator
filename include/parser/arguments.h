#pragma once

#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <vector>

namespace args
{

    //First value is path where its executed
    struct parser
    {
        //key -> --parser or -p, list of values
        std::unordered_map<std::string, std::vector<std::string>> result;

        std::unordered_map<std::string, std::vector<std::string>> parse_data(int argc, char** argv)
        {
            bool array = false;
            std::string type;
            for(int i = 0; i < argc; i++)
            {
                std::string v = argv[i];
                if(result.contains(v)) throw std::runtime_error("Duplicate arguments");

                if(array && !(v.contains("--") || v.contains("-")))
                {
                    size_t pos = v.rfind(".snbt");
                    if(pos != std::string::npos)
                    {
                        v = v.substr(0, pos);
                    }
                    result[type].emplace_back(v);
                }
                else
                {
                    array = false;
                    type.clear();
                    type.shrink_to_fit();
                }

                if(v == "--splitter" || v == "-s")
                {
                    result["--splitter"] = {};
                }

                if(v == "--merge" || v == "-m")
                {
                    result["--merge"] = {};
                }

                if(v == "--pack" || v == "-p")
                {
                    result["--pack"] = {};
                }

                if(v == "--lang" || v == "-l")
                {
                    result["--lang"] = {};
                    type = "--lang";
                    array = true;
                }

                if(v == "--path" || v == "-pt")
                {
                    if(argc <= i + 1) throw std::runtime_error("--path needs a path");
                    result["--path"] = {argv[i + 1]};
                    i++;
                }

                if(v == "--curseforge" || v == "-cf")
                {
                    result["--curseforge"] = {};
                }

                if(v == "--generate" || v == "-g")
                {
                    result["--generate"] = {};
                }

                if(v == "--mods" || v == "-md")
                {
                    if(argc <= i + 1) throw std::runtime_error("--mods needs a path to the mods");
                    result["--mods"] = {argv[i + 1]};
                    i++;
                }

                if(v == "--help" || v == "-h")
                {
                    printf("Usage: %s [OPTIONS]\n\n", argv[0]);
                    printf("Options:\n");
                    printf("  -h, --help             Show this help message and exits.\n\n");
                    printf("  -pt, --path            Path to the root folder, where config folder is located.\n\n");
                    printf("                         If no path it's selected, it will search in this folder\n\n");
                    printf("  -s, --splitter         Splits the files into multiple smaller files.\n"); 
                    printf("                         It deletes unnecesary entries, for example, from deleting a quest.\n");
                    printf("                         This option is done before merge.\n\n");
                    printf("  -m, --merge            Merges files from the previous split. Fails if split wasn't done before\n\n");
                    printf("  -l, --lang [Files]     Just split those langs instead of everything, can have .snbt on the end or not\n");
                    printf("                         For example, --lang en_us.snbt es_es pt_br\n\n");
                    printf("  -p, --pack             Pack the modpack into a distribution format that Curseforge can use. Also generate the server file if --mods it's present\n\n");
                    printf("  -cf, --curseforge      Instead of using mods.json, an JSON export from Prism Launcher, uses manisfest.json from CurseForge Launcher\n");
                    printf("  -g,  --generate        Generates the server environment and startup scripts.\n\n");
                    printf("  -md, --mods [Path]     Path to the client instance base folder to generate the server from.\n");
                    printf("                         It will automatically look for the 'mods' subfolder inside this path.\n\n");
                    exit(0);
                }
                
            }
            return result;
        }
    };
}