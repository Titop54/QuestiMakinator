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

                if(v == "--merger" || v == "-m")
                {
                    result["--merger"] = {};
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

                if(v == "--json" || v == "-j")
                {
                    result["--json"] = {};
                }

                if(v == "--convert" || v == "-c")
                {
                    result["--convert"] = {};
                }

                if(v == "--upgrade" || v == "-u")
                {
                    result["--upgrade"] = {};
                }

                if(v == "--extract-lang" || v == "-el")
                {
                    result["--extract-lang"] = {};
                }

                if(v == "--help" || v == "-h")
                {
                    printf("Usage: %s [OPTIONS]\n\n", argv[0]);
                    printf("Options:\n");
                    printf("  -h,  --help             Show this help message and exit.\n\n");

                    printf("  -pt, --path [Path]      Path to the root folder where the 'config' folder is located.\n");
                    printf("                          Defaults to the current directory if not specified.\n\n");

                    printf("  -s,  --splitter         Splits large quest lang files into multiple smaller files.\n"); 
                    printf("                          Cleans up obsolete entries (e.g., from deleted quests).\n");
                    printf("                          Should be executed before merging.\n\n");

                    printf("  -m,  --merger           Merges previously split files back into a single lang file.\n");
                    printf("                          Requires a valid 'split' directory structure.\n\n");

                    printf("  -l,  --lang [Files]     Filter specific languages to process instead of all.\n");
                    printf("                          Example: --lang en_us.snbt es_es pt_br\n\n");

                    printf("  -j,  --json             Output/Export mode: Generates .json files instead of .snbt.\n");
                    printf("                          When splitting: Saves the chunks as JSON.\n");
                    printf("                          When merging: Generates the final lang file as JSON.\n\n");

                    printf("  -c,  --convert          Input/Source mode: Reads .json files instead of .snbt.\n");
                    printf("                          When splitting: Looks for .json sources in the lang/chapters folders.\n");
                    printf("                          When merging: Looks for .json chunks in the split folder.\n\n");

                    printf("  -p,  --pack             Pack the modpack into a CurseForge-ready distribution format.\n");
                    printf("                          Generates server files if --mods is also present.\n\n");

                    printf("  -cf, --curseforge       Use 'manifest.json' (CurseForge) instead of 'mods.json' (Prism Launcher).\n\n");

                    printf("  -g,  --generate         Generates the server environment and startup scripts.\n\n");

                    printf("  -md, --mods [Path]      Path to the client instance base folder for server generation.\n");
                    printf("                          The tool will automatically locate the 'mods' subfolder.\n\n");

                    printf("  -u,  --upgrade          Upgrades quest chapter files from 1.20.1 to 1.21.1 format.\n");
                    printf("                          Converts item stacks (Count/tag to count/components).\n");
                    printf("                          Output: config/ftbquests/quests/1_21_1/chapters/\n\n");

                    printf("  -el, --extract-lang     Extracts hardcoded quest text into lang keys.\n");
                    printf("                          Writes kubejs/assets/ftbquests/lang/en_us.json.\n");
                    printf("                          Replaces text with {ftbquests.chapter.<file>.quest<ID>.<field>} keys.\n\n");
                    
                    printf("Order of execution, regardless of order:\n");
                    printf("  1. --generate   (Set up the environment)\n");
                    printf("  2. --splitter   (Extract and clean data)\n");
                    printf("  3. --merger     (Reassemble modified data)\n");
                    printf("  4. --upgrade    (Convert 1.20.1 to 1.21.1 format)\n");
                    printf("  5. --extract-lang (Extract text to lang keys)\n");
                    printf("  6. --pack       (Finalize for distribution)\n\n");
                    exit(0);
                }
                
            }
            return result;
        }
    };
}