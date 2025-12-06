#ifndef KUBEJS_H
#define KUBEJS_H

#include <SFML/Graphics/Image.hpp>
#include <SFML/Network.hpp>
#include <SFML/Graphics.hpp>
#include <string>
#include <vector>


enum class ReloadType{CLIENT, SERVER, STARTUP};
enum class TypeElement {ITEM, BLOCK, FLUID, ENTITY, MUSIC, GUI, NONE};

/**
 * This response struct respresents an element from a json array
 * Example of mod and how its displayed on the response
 *[
 *   {
 *      "id": "emi",
 *       "name": "EMI",
 *       "version": "1.1.22+1.21.1+neoforge"
 *   }
 *]
 */
struct Mod
{
    std::string name; //Catalyst Core
    std::string id; //catalystcore
    std::string version; //0.2 or 0.0NONE
};

/**
 * @brief A struct containing a list of responses
 * Example of an response from the server might be the following:
 *[
 *   "catalystgraves": [
 *      "grave_1.png",
 *      "grave_2.png",
 *      "grave_3.png",
 *      "skull.png"
 *   ]
 *]
 * Where we have an array that might contains 1 or more mods,
 * that has a list of elements inside
 * This doesn't guarantee that .png.mcmeta are found, so we need
 * to search them by hand
 */
struct ListResponse {
    std::string mod; //minecraft or kubejs
    std::string path; //the return string
    TypeElement type; //if comes from models folder, type is model, if not it comes from texture/block or texture_item
};

class KubeJSClient {
private:
    int port; //61423 by default
    std::string auth;
    sf::TcpSocket socket;
    bool connected = false; //if false, before doing anything, we call tryConnect(), if that fails, we exit from any function that downloads stuff

    bool tryConnect(); //try to connect to the server 5 times with a timeout of 5s
    std::string extractHttpBody(const std::string& httpResponse); //extract http body from response
    

public:
    bool needs_manual = false;
    KubeJSClient(int port, const std::string& auth); //auth is "" by default since we can't know if there is a correct folder
    
    bool connect(); //connect to the server
    void disconnect(); //disconnect from it
    bool isConnected() const; //return if the client is connected
    
    bool sendReloadCommand(const ReloadType& type_reload); //Determines which reload to do 
    bool sendHttpRequest(const std::string& method, //GET or POST
                         const std::string& path, //URL to get the item
                         const std::string& body, //Blank on GET, if not the data we want to post (not all post do need data, like reload)
                         std::string& response //Response of the server, if nullptr, it will create a new std::string
                        );
    bool sendHttpRequest(const std::string& method, const std::string& path, const std::string& body = ""); //Basically the same as above just no response back
    
    /**
     * @brief Get a list of all mods available in the instance
     * 
     * @return std::vector<Mod> 
     */
    std::vector<Mod> getAvailableMods(); //Return a list of all mods (http://localhost:61423/api/mods)
    
    /**
     * @brief This return a list of all type of assets of a models and textures
     * This module does at least 3 calls to the server (http://localhost:61423/api/client/assets/list/models, http://localhost:61423/api/client/assets/list/textures/block and http://localhost:61423/api/client/assets/list/textures/item)
     * On textures/block and textures/item, there might be .png.mcmeta that should be included on the result
     * 
     * @return std::vector<ListResponse>
     */
    std::vector<ListResponse> listAllAssets();

    /**
     * @brief Calls listAllAssets and filter it out based on the prefix
     * 
     * @param prefix .json or .png or .png.mcmeta
     * @return std::vector<std::string> 
     */
    std::vector<ListResponse> listAssetsByPrefix(const std::string& prefix);
    
    /**
     * @brief Given an id, for example, minecraft:stone, downloads all assests related to it
     * From the .png.mcmeta if exists, to textures and models, it downloads them
     * 
     * @param modid_itemid ID of the item/block
     * @param outputPath Folder to download, by default it's where the executable is running/download (download might not exist )
     * @return true If downloaded correctly
     * @return false If not downloaded correctly
     */
    bool downloadAssetFile(const std::string& id);
    
    /**
     * @brief Helper functon to downloadAssetFile
     * @param urlPath Path to use to download (http://localhost:61423/api/client/assets/get/<modid>/<models or textures>/ListResponse.path)
     * @param outputPath Folder to download the file
     * @return true If it could download and save the file
     * @return false If it couldnt download and save the file
     */
    bool downloadToFile(const std::string& urlPath, 
                        const std::string& outputPath);
    

    /**
     * @brief Given an id, for example, minecraft:stone, downloads all assests related to it
     * From the .png.mcmeta if exists, to textures and models, it downloads them
     * After that get, it creates an isometric perspective for blocks.
     * And gives animation to blocks and items if needed
     * 
     * @param modid_itemid ID of the item/block
     * @return true If downloaded correctly
     * @return false If not downloaded correctly
     */
    std::vector<sf::Image> downloadAssetMem(const std::string& id);
    
    // Element searching
    /**
     * @brief Using http://localhost:61423/api/client/search/blocks to get a list of blocks with the
     * follow structure:
        [
            {
                "id": "minecraft:air",
                "name": "Air"
            },
            {
                "id": "minecraft:stone",
                "name": "Stone"
            }
        ]
     * 
     * @return std::vector<std::string>, where the string is the id 
     */
    std::vector<std::string> searchBlocks();

    /**
     * @brief Get a list of all items, there is a mix of blocks and items
     * If you want blocks, check searchBlocks(), those are filtered here
     * A response might look like this:
        {
            "world": false,
            "icon_path_root": "http://localhost:61423/img/64/item/",
            "results": [
                {
                "cache_key": "7037f2799fff350bbe884c05f6b3021d",
                "id": "ftbquests:screen_7",
                "name": "Task Screen (7x7)",
                "icon_path": "ftbquests/screen_7",
                "block": "ftbquests:screen_7" //Here for example, it will be deleted from the output
                },
                {
                "cache_key": "8874e21f57e93cbd96f66ac7b97439f8",
                "id": "catalystcore:complete_catalyst", //only this will be saved
                "name": "Catalyst",
                "icon_path": "catalystcore/complete_catalyst"
                }
            ]
        }
     * @return std::vector<std::string> containing only items
     */
    std::vector<std::string> searchItems();

    /**
     * @brief List of all fluids in the game, only id
     * A reponse might looks like this:
     [
        {
            "id": "minecraft:empty",
            "name": "Air"
        },
        {
            "id": "minecraft:flowing_water",
            "name": "Water"
        },
        {
            "id": "minecraft:water",
            "name": "Water"
        }
     ]
     * 
     * @return std::vector<std::string> containing only ids of fluids
     */
    std::vector<std::string> searchFluids();
    
    /**
     * @brief Get a preview using on of the following methods
        - GET	/img/{size}/block-tag/{namespace}/{path}
        - GET	/img/{size}/block/{namespace}/{path}
        - GET	/img/{size}/fluid-tag/{namespace}/{path}
        - GET	/img/{size}/fluid/{namespace}/{path}
        - GET	/img/{size}/item-tag/{namespace}/{path}
        - GET	/img/{size}/item/{namespace}/{path}
     * 
     * @param texturePath is minecraft:stone or minecraft:redstone, but the : is turned into /
     * @param size Size of the image
     * @return Image representing the block/item without animation
     */
    sf::Image getPreview(const std::string& id, int size, TypeElement type, bool isTag);
    
    /**
     * @brief Divide una imagen vertical en frames
     */
    std::vector<sf::Image> splitVerticalFrames(const sf::Image& spriteSheet, int frameHeight);

    sf::Image createIsometricView(const sf::Image& texture);

};

// Global client instance
extern KubeJSClient client;

#endif