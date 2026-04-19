#pragma once

#include "parser/arguments.h"
#include <filesystem>
#include <iostream>
#include <fstream>
#include <cstdlib>
#include <nlohmann/json.hpp>
#include <SFML/Network.hpp>
#include <stdexcept>

namespace gen
{
    namespace fs = std::filesystem;
    using json = nlohmann::json;

    inline bool command_exists(const std::string &cmd)
    {
        #ifdef _WIN32
            std::string check = "where " + cmd + " >nul 2>nul";
        #else
            std::string check = "command -v " + cmd + " >/dev/null 2>&1";
        #endif
        return std::system(check.c_str()) == 0;
    }

    inline void download_file(const std::string &url, const std::string &output)
    {
        if(command_exists("wget"))
        {
            std::cout << "[INFO] Downloading with wget...\n";
            std::string cmd = "wget -q --show-progress -O \"" + output + "\" \"" + url + "\"";
            std::system(cmd.c_str());
        }
        else if(command_exists("curl"))
        {
            std::cout << "[INFO] Downloading with curl...\n";
            std::string cmd = "curl -# -L -o \"" + output + "\" \"" + url + "\"";
            std::system(cmd.c_str());
        }
        else
        {
            std::cout << "[WARN] wget or curl not found. Stopping!\n";
            return;
        }
    }

    inline void download_packwiz(const fs::path &base_path)
    {
        std::string url;
        std::string check_name;

#ifdef _WIN32
        url = "https://nightly.link/packwiz/packwiz/workflows/go/main/Windows%2064-bit.zip";
        check_name = "packwiz.exe";
#elif __linux__
        url = "https://nightly.link/packwiz/packwiz/workflows/go/main/Linux%2064-bit%20x86.zip";
        check_name = "packwiz";
#elif __APPLE__
        url = "https://nightly.link/packwiz/packwiz/workflows/go/main/macOS%2064-bit%20x86.zip";
        check_name = "packwiz";
#else
        std::cerr << "[ERROR] OS not supported by packwiz.\n";
        return;
#endif

        fs::path exe_path = base_path / check_name;
        if(fs::exists(exe_path))
        {
            std::cout << "[INFO] " << check_name << " already exists. Skipping downloading!.\n";
            return;
        }

        std::cout << "[INFO] Dowloading PackWiz from nightly.link...\n";
        fs::path zip_path = base_path / "packwiz.zip";

        download_file(url, zip_path.string());

        if(!fs::exists(zip_path))
        {
            std::cerr << "[ERROR] Couldn't download packwiz.zip.\n";
            return;
        }

        std::cout << "[INFO] Extracting Packwiz...\n";

        auto current_dir = fs::current_path();
        fs::current_path(base_path);

        #ifdef _WIN32
            std::system("tar -xf packwiz.zip");
        #else
            std::system("unzip -o -q packwiz.zip");

            fs::permissions(check_name,
                            fs::perms::owner_all  |
                                fs::perms::group_read  | fs::perms::group_exec |
                                fs::perms::others_read | fs::perms::others_exec,
                            fs::perm_options::replace);
        #endif

        fs::remove("packwiz.zip");
        fs::current_path(current_dir);

        std::cout << "[INFO] Packwiz ready to be used!.\n";
    }

    inline void handle_git_clone(const fs::path &base_path)
    {
        fs::path info_path = base_path / "info.json";
        if(!fs::exists(info_path)) return;

        if(!command_exists("git"))
        {
            std::cerr << "[WARN] 'git' command not found. Skipping cloning/updating.\n";
            return;
        }

        try
        {
            std::ifstream file(info_path);
            json data = json::parse(file);

            if(data.contains("git") && !data["git"].get<std::string>().empty())
            {
                std::string git_url = data["git"];
                std::string branch = data.value("branch", "");

                std::string branch_flag = "";
                std::string branch_name = branch.empty() ? "main" : branch;

                if(!branch.empty())
                {
                    branch_flag = "-b " + branch + " ";
                    std::cout << "[INFO] Using custom branch: " << branch << "\n";
                }

                fs::path target_dir = base_path;

                if(fs::exists(target_dir / ".git"))
                {
                    std::cout << "[INFO] Updating repo...\n";
                    std::string cmd = "git -C \"" + target_dir.string() + "\" pull origin " + branch_name;
                    std::system(cmd.c_str());
                }
                else
                {
                    std::cout << "[INFO] Cloning repo with depth 1...\n";

                    std::string cmd_init = "git -C \"" + target_dir.string() + "\" init -q";
                    std::string cmd_remote = "git -C \"" + target_dir.string() + "\" remote add origin " + git_url;
                    std::string cmd_fetch = "git -C \"" + target_dir.string() + "\" fetch --depth 1 origin " + branch_name + " -q";
                    std::string cmd_reset = "git -C \"" + target_dir.string() + "\" reset --hard FETCH_HEAD -q";

                    std::system(cmd_init.c_str());
                    std::system(cmd_remote.c_str());
                    std::system(cmd_fetch.c_str());
                    std::system(cmd_reset.c_str());
                }
            }
        }
        catch(const std::runtime_error &e)
        {
            std::cerr << "[ERROR] Error parsing info.json or with git command.\n"
                      << e.what();
        }
    }

    inline void generate(args::parser args)
    {
        fs::path raw_path = args.result.contains("--path") ? args.result["--path"][0] : ".";
        fs::path base_path = fs::absolute(raw_path).lexically_normal();
        
        bool has_cf = args.result.contains("--curseforge");

        if (base_path == base_path.root_path())
        {
            std::cout << "[FATAL] You can't use root path (" << base_path << ")\n";
            return;
        }

        handle_git_clone(base_path);

        std::cout << "Generating structure for packing in : " << base_path << "\n";

        std::vector<std::string> volatile_dirs = {"mods", "resourcepacks"};
        for(const auto &dir : volatile_dirs)
        {
            fs::path p = base_path / dir;
            try 
            {
                if(fs::exists(p))
                {
                    fs::remove_all(p);
                    std::cout << "[INFO] Folder deleted: " << dir << "\n";
                }
                fs::create_directories(p);
                std::cout << "[INFO] Folder created: " << dir << "\n";
            }
            catch (const fs::filesystem_error& e)
            {
                std::cout << "[ERROR] Problems deleting folder '" << dir << "':\n" << e.what() << "\n";
                return;
            }
        }

        std::vector<std::string> persistent_dirs = {"config", "defaultconfigs", "changelogs", "kubejs"};
        for(const auto &dir : persistent_dirs)
        {
            fs::path p = base_path / dir;
            if(!fs::exists(p))
            {
                fs::create_directories(p);
                std::cout << "[INFO] Folder created: " << dir << "\n";
            }
        }

        fs::path info_path = base_path / "info.json";
        if(!fs::exists(info_path))
        {
            json info = {
                {"name", "MyModpack"},
                {"version", "1.0.0"},
                {"loader", "neoforge"},
                {"loader_version", "21.1.222"},
                {"mc_version", "1.21.1"},
                {"git", ""},
                {"branch", "main"}};
            std::ofstream(info_path) << info.dump(4);
            std::cout << "[INFO] info.json generated.\n";
        }

        fs::path rp_path = base_path / "resourcepacks.json";
        if(!fs::exists(rp_path))
        {
            json rp = json::array();
            std::ofstream(rp_path) << rp.dump(4);
            std::cout << "[INFO] resourcepacks.json generated.\n";
        }

        if(has_cf)
        {
            fs::path manifest_path = base_path / "manifest.json";
            if(!fs::exists(manifest_path))
            {
                json manifest = {
                    {"minecraft", {{"version", "1.21.1"}, {"modLoaders", {{{"id", "neoforge-21.1.219"}, {"primary", true}}}}}},
                    {"manifestType", "minecraftModpack"},
                    {"manifestVersion", 1},
                    {"name", "MyModpack"},
                    {"version", "1.0.0"},
                    {"author", "Catalyst Studios"},
                    {"files", json::array()}};
                std::ofstream(manifest_path) << manifest.dump(4);
                std::cout << "[INFO] manifest.json generado.\n";
            }
        }
        else
        {
            fs::path mods_path = base_path / "mods.json";
            if(!fs::exists(mods_path))
            {
                json mods = json::array();
                std::ofstream(mods_path) << mods.dump(4);
                std::cout << "[INFO] mods.json generado.\n";
            }
        }
        download_packwiz(base_path);
    }
}