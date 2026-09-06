#pragma once

#include "parser/arguments.h"
#include <filesystem>
#include <ios>
#include <iostream>
#include <fstream>
#include <cstdlib>
#include <nlohmann/json.hpp>
#include <regex>

namespace pack
{
    namespace fs = std::filesystem;
    using json = nlohmann::json;

    struct info
    {
        std::string name, version, loader, loader_version, mc_version, git, branch, author;
    };

    inline bool command_exists(const std::string& cmd)
    {
#ifdef _WIN32
        return std::system(("where " + cmd + " >nul 2>nul").c_str()) == 0;
#else
        return std::system(("command -v " + cmd + " >/dev/null 2>&1").c_str()) == 0;
#endif
    }

    inline void create_packwizignore(const fs::path& base_path)
    {
        if(fs::exists(".packwizignore"))
        {
            return;
        }
        std::string content = "packwiz\npackwiz.exe\ninfo.json\nmods.json\nmanifest.json\n.git/\n*.zip\nQuestiMakinator*\n";
        std::ofstream(base_path / ".packwizignore") << content;
    }

    inline void generate_server_startup_scripts(const fs::path& server_dir, const std::string& loader_version)
    {
        std::string sh_content = R"SCRIPT(#!/bin/sh
set -eu

#If the server should restart on crash, delete the # to set to false
#RESTART=false
TIME_SLEEP=10
JAVA_PATH=""
JAVA="${JAVA_PATH:-java}"

LOADER_VERSION="<LOADER_VERSION>"
INSTALLER="neoforge-${LOADER_VERSION}-installer.jar"
URL="https://maven.neoforged.net/releases/net/neoforged/neoforge/${LOADER_VERSION}/${INSTALLER}"

Reset='\033[0m'
Red='\033[0;31m'
Cyan='\033[0;36m'

pause() {
    printf "%s\n" "Press enter to continue..."
    read ans
}

check_java() {
    if ! command -v "$JAVA" > /dev/null 2>&1; then
        echo "\n----------------------------------------"
        echo "Minecraft 1.21 requires at least Java 21"
        echo "----------------------------------------"
        pause
        exit 1
    fi

    local java_version=$("$JAVA" -version 2>&1 | head -n1 | cut -d'"' -f2 | cut -d'.' -f1)

    if [ "$java_version" -lt 21 ]; then
        echo "\n${Red}ERROR${Reset}: Incompatible Java Version"
        echo "----------------------------------------"
        echo "Minecraft 1.21 requires at least Java 21"
        echo "Found version: Java ${Cyan}$java_version${Reset}"
        echo "----------------------------------------"
        pause
        exit 1
    fi
}

check_java
cd "$(dirname "$0")"

if [ ! -d libraries ]; then
    echo "Installing NeoForge ${Cyan}${LOADER_VERSION}${Reset}, please wait..."
    
    if [ ! -f "$INSTALLER" ]; then
        if command -v wget > /dev/null; then
            echo "Downloading with wget: ${Cyan}$URL${Reset}"
            wget -O "$INSTALLER" "$URL" 2> /dev/null
        elif command -v curl > /dev/null; then
            echo "Downloading with curl: ${Cyan}$URL${Reset}"
            curl -Lo "$INSTALLER" "$URL" 2> /dev/null
        else
            echo "Error: You need wget or curl, please install it"
            pause
            exit 1
        fi
    fi

    echo "Starting NeoForge installation..."
    "$JAVA" -jar "$INSTALLER" --installServer

    rm "$INSTALLER"
    rm -f "run.sh" "run.bat"
fi

echo "Starting server..."
while true; do
    "$JAVA" @user_jvm_args.txt @libraries/net/neoforged/neoforge/${LOADER_VERSION}/unix_args.txt nogui "$@"

    if [ "${RESTART:-true}" = "false" ]; then
        exit 0
    fi

    echo "Restarting automatically in ${Red}$TIME_SLEEP${Reset} seconds (press Ctrl + C to cancel)"
    sleep $TIME_SLEEP
done
)SCRIPT";

        std::string bat_content = R"SCRIPT(@echo off
setlocal enabledelayedexpansion

:: If the server should restart on crash, remove "rem" below
rem set RESTART=false
set TIME_SLEEP=10
set JAVA="java"

set LOADER_VERSION=<LOADER_VERSION>
set INSTALLER=neoforge-%LOADER_VERSION%-installer.jar
set URL=https://maven.neoforged.net/releases/net/neoforged/neoforge/%LOADER_VERSION%/%INSTALLER%

cd /d "%~dp0"

:: Quick Java check
%JAVA% -version >nul 2>&1
if errorlevel 1 (
    echo [ERROR] Java 21 is not installed or not in PATH.
    pause
    exit /b 1
)

if not exist libraries\ (
    echo Installing NeoForge %LOADER_VERSION%, please wait...
    if not exist "%INSTALLER%" (
        echo Downloading installer via PowerShell...
        powershell -Command "Invoke-WebRequest -Uri '%URL%' -OutFile '%INSTALLER%'"
    )
    
    echo Starting NeoForge installation...
    %JAVA% -jar "%INSTALLER%" --installServer
    
    del "%INSTALLER%"
    if exist run.bat del run.bat
    if exist run.sh del run.sh
)

:loop
echo Starting server...
%JAVA% @user_jvm_args.txt @libraries/net/neoforged/neoforge/%LOADER_VERSION%/win_args.txt nogui %*

if /I "%RESTART%"=="false" goto :EOF

echo Restarting automatically in %TIME_SLEEP% seconds (press Ctrl + C to cancel)
timeout /t %TIME_SLEEP% /nobreak >nul
goto loop
)SCRIPT";

        auto replace_all = [](std::string& str, const std::string& from, const std::string& to) {
            size_t start_pos = 0;
            while((start_pos = str.find(from, start_pos)) != std::string::npos)
            {
                str.replace(start_pos, from.length(), to);
                start_pos += to.length();
            }
        };

        replace_all(sh_content, "<LOADER_VERSION>", loader_version);
        replace_all(bat_content, "<LOADER_VERSION>", loader_version);

        std::ofstream sh_file(server_dir / "start.sh");
        sh_file << sh_content;
        sh_file.close();

#ifndef _WIN32
        fs::permissions(server_dir / "start.sh", fs::perms::owner_all | fs::perms::group_read | fs::perms::group_exec | fs::perms::others_read | fs::perms::others_exec);
#endif

        std::ofstream bat_file(server_dir / "start.bat");
        bat_file << bat_content;
        bat_file.close();
    }

    inline void generate_server_from_client(const std::string& name, const std::string& version, const std::string& loader_version, const fs::path& client_base_path)
    {
        std::cout << "\nGenerating Server from Local Client\n";

        std::string safe_name = name;
        std::string safe_version = version;

        auto sanitize = [](std::string& s) {
            std::replace(s.begin(), s.end(), '/', '_');
            std::replace(s.begin(), s.end(), '\\', '_');
            std::replace(s.begin(), s.end(), ' ', '_');
            std::replace(s.begin(), s.end(), ':', '_');
        };

        sanitize(safe_name);
        sanitize(safe_version);

        std::string server_dir_name = safe_name + "_Server_" + safe_version;
        fs::path server_dir = fs::absolute(server_dir_name).lexically_normal();

        if(server_dir == server_dir.root_path())
        {
            std::cerr << "[FATAL] Root not allowed.\n";
            return;
        }

        fs::path server_mods_dir = server_dir / "mods";

        try
        {
            fs::create_directories(server_mods_dir);
        }
        catch(const fs::filesystem_error& e)
        {
            std::cerr << "[ERROR] Error while creating folder:\n"
                      << e.what() << "\n";
            return;
        }

        std::cout << "Copying configurations (config, kubejs, defaultconfigs)...\n";
        std::vector<std::string> folders = { "config", "kubejs", "defaultconfigs" };
        for(const auto& folder : folders)
        {
            if(fs::exists(folder))
            {
                try
                {
                    fs::copy(folder, server_dir / folder, fs::copy_options::recursive | fs::copy_options::overwrite_existing);
                }
                catch(const std::exception& e)
                {
                    std::cerr << "[WARN] Error copying " << folder << ": " << e.what() << "\n";
                }
            }
        }

        fs::path client_mods_dir = client_base_path / "mods";

        if(!fs::exists(client_mods_dir))
        {
            std::cerr << "\n[ERROR] Mods path not found:\n"
                      << client_mods_dir.string()
                      << "\nCheck that the provided path exists and contains a 'mods' folder with .jar.\n";
            return;
        }

        std::cout << "\nCopying mods from " << client_mods_dir.string() << "...\n";
        int copied_count = 0;

        for(const auto& entry : fs::directory_iterator(client_mods_dir))
        {
            if(entry.is_regular_file() && entry.path().extension() == ".jar")
            {
                fs::copy_file(entry.path(), server_mods_dir / entry.path().filename(), fs::copy_options::overwrite_existing);
                copied_count++;
            }
        }
        std::cout << " -> " << copied_count << " mods copied to the server.\n";

        std::cout << "\nCleaning client-side only mods to prevent server crashes...\n";
        std::vector<std::string> default_keywords = {
            "oculus", "embeddium", "rubidium", "sodium", "iris",
            "xaero", "journeymap", "minimap",
            "mousetweaks", "controlling", "searchables", "defaultoptions",
            "soundphysics", "ambientsounds", "presencefootsteps",
            "moreoverlays", "entityculling", "blur", "inventoryprofiles",
            "lighty", "emixx", "watermedia", "onlyexcavator"
        };

        std::vector<std::string> client_only_keywords;
        fs::path keywords_path = "client_keywords.json";

        if(!fs::exists(keywords_path))
        {
            std::ofstream out(keywords_path);
            if(out.is_open())
            {
                json j_default = default_keywords;
                out << j_default.dump(4);
                out.close();
            }
            client_only_keywords = default_keywords;
        }
        else
        {
            std::ifstream in(keywords_path);
            if(in.is_open())
            {
                json j = json::parse(in, nullptr, false);

                if(!j.is_discarded() && j.is_array())
                {
                    for(const auto& item : j)
                    {
                        if(item.is_string())
                        {
                            client_only_keywords.push_back(item.get<std::string>());
                        }
                    }
                }
                else
                {
                    std::cerr << "[WARN] client_keywords.json has an invalid format. It should be a list of strings. Using default.\n";
                    client_only_keywords = default_keywords;
                }
            }
            else
            {
                client_only_keywords = default_keywords;
            }
        }

        int removed_count = 0;
        for(const auto& entry : fs::directory_iterator(server_mods_dir))
        {
            std::string filename = entry.path().filename().string();
            std::string lower_name = filename;
            std::transform(lower_name.begin(), lower_name.end(), lower_name.begin(), ::tolower);

            bool is_client = false;
            for(const auto& keyword : client_only_keywords)
            {
                if(lower_name.find(keyword) != std::string::npos)
                {
                    is_client = true;
                    break;
                }
            }

            if(is_client)
            {
                try
                {
                    fs::remove(entry.path());
                    std::cout << "    - Removed (Client-side): " << filename << "\n";
                    removed_count++;
                }
                catch(...)
                {
                }
            }
        }
        std::cout << " -> " << removed_count << " client mods removed.\n";

        std::ofstream eula(server_dir / "eula.txt");
        eula << "eula=false\n";
        eula.close();
        std::cout << "\n -> eula.txt file generated set to false.\n";

        std::string jvm_args_content = R"(# Xmx and Xms set the maximum and minimum RAM usage, respectively.
# They can take any number, followed by an M or a G.
# M means Megabyte, G means Gigabyte.
# For example, to set the maximum to 3GB: -Xmx3G
# Another example, setthing the minimum to 2.5GB: -Xms2500M
# A good default for a modded server is 4-6GB + 1GB per extra player.

-Xms4G
-Xmx8G
-XX:+UseZGC
-XX:+ZGenerational
-XX:SoftMaxHeapSize=6g
-XX:+DisableExplicitGC
-XX:+AlwaysPreTouch
-XX:+PerfDisableSharedMem
-XX:+UseDynamicNumberOfGCThreads
)";
        std::ofstream jvm(server_dir / "user_jvm_args.txt");
        jvm << jvm_args_content;
        jvm.close();
        std::cout << " -> user_jvm_args.txt file generated with ZGC flags.\n";

        std::cout << "\nGenerating startup scripts (start.sh / start.bat)...\n";
        generate_server_startup_scripts(server_dir, loader_version);

        std::cout << "\nCompressing server pack into " << server_dir_name << ".zip...\n";

#ifdef _WIN32
        std::string zip_cmd = "tar -a -c -f " + server_dir_name + ".zip " + server_dir_name;
#else
        std::string zip_cmd = "zip -r " + server_dir_name + ".zip " + server_dir_name + " > /dev/null";
#endif
        std::system(zip_cmd.c_str());

        std::cout << "Cleaning up temporary server folder...\n";
        try
        {
            fs::remove_all(server_dir);
        }
        catch(const fs::filesystem_error& e)
        {
            std::cerr << "[WARN] Couldn't clean up temp folder: " << e.what() << "\n";
        }
        std::cout << "\n -> Server pack successfully created as '" << server_dir_name << ".zip'!\n";
    }

    inline void pack(args::parser args)
    {
        bool has_cf = args.result.contains("--curseforge");

        fs::path raw_path = args.result.contains("--path") ? args.result["--path"][0] : ".";
        fs::path base_path = fs::absolute(raw_path).lexically_normal();

        if(base_path == base_path.root_path())
        {
            std::cerr << "[FATAL] Root path not allowed (" << base_path << ")\n";
            return;
        }

        std::ifstream info_file(base_path / "info.json");
        if(!info_file.is_open())
        {
            std::cerr << "[ERROR] info.json not found in " << base_path << ".\n";
            return;
        }

        json ij = json::parse(info_file);
        info pack_info = {
            ij.value("name", "Pack"), ij.value("version", "1.0.0"),
            ij.value("loader", "neoforge"), ij.value("loader_version", "21.1.248"),
            ij.value("mc_version", "1.21.1"), ij.value("git", ""), ij.value("branch", "main"),
            ij.value("author", "Auto Generated")
        };

        if(fs::exists(base_path / ".git") && !pack_info.git.empty() && command_exists("git"))
        {
            std::cout << "[INFO] Updating repo (Branch: " << pack_info.branch << ")...\n";
            std::string pull_cmd = "git -C \"" + base_path.string() + "\" pull origin " + pack_info.branch;
            std::system(pull_cmd.c_str());
        }

#ifdef _WIN32
        std::string pw = (base_path / "packwiz.exe").string();
#else
        std::string pw = "./packwiz";
#endif

        auto old_path = fs::current_path();

        try
        {
            fs::current_path(base_path);
        }
        catch(const fs::filesystem_error& e)
        {
            std::cerr << "[ERROR] Couldn't access the folder (" << base_path << "):\n"
                      << e.what() << "\n";
            return;
        }

        std::cout << "\nStarting Packwiz\n";

        if(!fs::exists("pack.toml"))
        {
            std::string init = pw + " init" +
                               " --name \"" + pack_info.name + "\"" +
                               " --version \"" + pack_info.version + "\"" +
                               " --mc-version " + pack_info.mc_version +
                               " --author \"" + pack_info.author + "\"" +
                               " --modloader " + pack_info.loader +
                               " --" + pack_info.loader + "-version " + pack_info.loader_version +
                               " -r";

            std::system(init.c_str());
        }
        else
        {
            std::cout << "\nPackwiz already started, deleting cache files!\n";
            try
            {
                if(fs::exists("index.toml"))
                {
                    fs::remove("index.toml");
                }
                std::ofstream("index.toml", std::ios::out);
            }
            catch(const fs::filesystem_error& e)
            {
                std::cerr << "[WARN] Error creating index.toml: " << e.what() << "\n";
            }
        }
#ifdef _WIN32
        std::string sep = " & ";
#else
        std::string sep = " ; ";
#endif

        std::system((pw + " settings acceptable-versions 1.21").c_str());

        std::cout << "\nInstalling Client mods\n";
        if(has_cf && fs::exists("manifest.json"))
        {
            std::ifstream cf_file("manifest.json");
            json manifest = json::parse(cf_file);

            if(manifest.contains("files"))
            {
                std::string batch_cmd = "";
                int count = 0;

                for(const auto& f : manifest["files"])
                {
                    std::string project_id = std::to_string(f.value("projectID", 0));
                    std::string file_id = std::to_string(f.value("fileID", 0));

                    batch_cmd += "echo n | " + pw + " curseforge add --addon-id " + project_id + " --file-id " + file_id + " " + sep + " ";
                    count++;

                    if(count % 50 == 0)
                    {
                        batch_cmd = batch_cmd.substr(0, batch_cmd.size() - 4);
                        int result = std::system(batch_cmd.c_str());
                        if(result != 0)
                        {
                            std::cerr << "Failed on the command!\n";
                        }

                        batch_cmd = "";
                    }
                }

                if(!batch_cmd.empty())
                {
                    std::system(batch_cmd.substr(0, batch_cmd.size() - 4).c_str());
                }
            }
        }
        else if(!has_cf && fs::exists("mods.json"))
        {
            std::ifstream prism_file("mods.json");
            json prism_mods = json::parse(prism_file);
            std::cout << "[INFO] Using Prism JSON export\nNeeds URL file set on!\nYou might need more control over which mods you add, check this website for more info: https://packwiz.infra.link/.\n";

            std::regex project_regex(R"(projects/(\d+))");
            std::smatch match;

            for(const auto& mod : prism_mods)
            {
                if(mod.contains("url"))
                {
                    std::string url = mod["url"];

                    if(std::regex_search(url, match, project_regex))
                    {
                        std::string project_id = match[1].str();

                        std::cout << "[INFO] Adding mod Prism export [" << mod.value("name", "Unknown") << "] (ID: " << project_id << ")...\n";

                        std::string cmd = "echo n | " + pw + " curseforge add --addon-id " + project_id;
                        std::system(cmd.c_str());
                    }
                    else
                    {
                        std::cerr << "[WARN] Couldn't extract ID: " << url << "\n";
                    }
                }
            }
        }
        else
        {
            std::cout << "[WARN] No mods installed!.\n";
        }

        std::cout.flush();

        if(fs::exists("extra.json"))
        {
            std::cout << "\nAdding mods from Modrith from extra.json!\n";
            try
            {
                std::ifstream extra_file("extra.json");
                json extra_mods = json::parse(extra_file);

                if(extra_mods.is_array())
                {
                    for(const auto& item : extra_mods)
                    {
                        if(item.is_string())
                        {
                            std::string modrinth_url = item.get<std::string>();
                            std::cout << "[INFO] Added extra mod: " << modrinth_url << "...\n";

                            std::string cmd = "echo n | " + pw + " modrinth add " + modrinth_url;
                            std::system(cmd.c_str());
                        }
                    }
                }
                else
                {
                    std::cerr << "[WARN] extra.json must be a JSON array of strings! These strings are Modrith url.\n";
                }
            }
            catch(const json::parse_error& e)
            {
                std::cerr << "[ERROR] Error reading extra.json: " << e.what() << "\n";
            }
        }
        else
        {
            std::cerr << "[ERROR] No extra.json found, an array of URL in JSON array\n";
        }

        if(fs::exists("resourcepacks.json"))
        {
            std::ifstream rp_file("resourcepacks.json");
            json resource_packs = json::parse(rp_file);

            if(resource_packs.is_array())
            {
                std::cout << "[INFO] Preparing Resources Packs...\n";

                std::string batch_cmd = "";
                size_t count = 0;
                size_t batch_size = 10;

                for(const auto& url : resource_packs)
                {
                    std::string rp_url = url.get<std::string>();

                    batch_cmd += "echo n | " + pw + " curseforge add \"" + rp_url + "\"";

                    count++;
                    if(count % batch_size == 0 || count == resource_packs.size())
                    {
                        std::cout << "[INFO] Command block for resources pack (" << count << "/" << resource_packs.size() << ")...\n";

                        std::system(batch_cmd.c_str());
                        batch_cmd = "";
                    }
                    else
                    {
                        batch_cmd += " "+ sep + " ";
                    }
                }
            }
        }
        else
        {
            std::cout << "[WARN] No se encontró resourcepacks.json\n";
        }

        create_packwizignore(".");
        std::system((pw + " curseforge export").c_str());

        if(args.result.contains("--mods") && !args.result["--mods"].empty())
        {
            std::string mods_path_raw = args.result["--mods"][0];
            fs::path safe_mods_path = fs::absolute(mods_path_raw).lexically_normal();
            generate_server_from_client(pack_info.name, pack_info.version, pack_info.loader_version, safe_mods_path);
        }
        else
        {
            std::cout << "\n[INFO] Skipping server generation. Use --mods <path to the mods> to generate.\n";
        }

        fs::current_path(old_path);
        std::cout << "\n[SUCCESS] Pack generated!.\n";
    }
} // namespace pack