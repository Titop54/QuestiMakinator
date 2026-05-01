#include "integration/model.h"

#include <SFML/Graphics/Image.hpp>
#include <webp/encode.h>
#include <webp/mux.h>

#include <cmath>
#include <fstream>
#include <iostream>
#include <set>
#include <string>
#include <vector>
#include <numbers>

#include <lodepng.h>

namespace fs = std::filesystem;

/**
 * @brief Rotates a point around an origin by a given angle on the specified axis.
 * @param point The point to rotate.
 * @param origin The rotation origin.
 * @param axis The axis of rotation ("x", "y", or "z").
 * @param angle The rotation angle in degrees.
 * @return The rotated point.
 */
sf::Vector3f rotate_point(sf::Vector3f point, sf::Vector3f origin, const std::string& axis, float angle)
{
    if(angle == 0.0f) return point;
    float rad = angle * (std::numbers::pi_v<float> / 180.0f);
    float c = cos(rad), s = sin(rad);
    float x = point.x - origin.x;
    float y = point.y - origin.y;
    float z = point.z - origin.z;

    float nx = x, ny = y, nz = z;
    if(axis == "x")
    {
        ny = y * c - z * s;
        nz = y * s + z * c;
    }
    else if(axis == "y")
    {
        nx = x * c - z * s;
        nz = x * s + z * c;
    }
    else if(axis == "z")
    {
        nx = x * c - y * s;
        ny = x * s + y * c;
    }
    return { nx + origin.x, ny + origin.y, nz + origin.z };
}

void ModelGenerator::saveAssets(const std::string& item_id, int output_size, float pitch, float yaw, bool wireframe)
{
    std::string safeName = changeFilename(item_id);
    std::string targetDir = "img/" + safeName;

    if(!fs::exists(targetDir))
    {
        fs::create_directories(targetDir);
    }

    std::ofstream jsonFile(targetDir + "/model.json");
    jsonFile << std::setw(4) << model_json << std::endl;
    jsonFile.close();

    for(const auto& [path, animData] : textures)
    {
        if(animData.texture.getSize().x == 0) continue;

        std::string texName = changeFilename(path) + ".png";
        sf::Image img = animData.texture.copyToImage();
        if(!img.saveToFile(targetDir + "/" + texName)) std::cerr << "Error loading image" << std::endl;

        if(!animData.rawMcmeta.empty())
        {
            std::string metaName = texName + ".mcmeta";
            std::ofstream metaFile(targetDir + "/" + metaName);
            metaFile << animData.rawMcmeta;
            metaFile.close();
        }
    }

    bool isItem = false;
    if(model_json.contains("parent") && model_json["parent"].get<std::string>().find("item/generated") != std::string::npos) isItem = true;
    if(model_json.contains("textures") && model_json["textures"].contains("layer0")) isItem = true;
    if(!isItem) exportToObj(item_id, targetDir);

    if(isObj())
    {
        saveAnimationWebP(item_id, targetDir, generateIsometricSequenceOBJ(output_size, pitch, yaw, wireframe));
    }
    else
    {
        saveAnimationWebP(item_id, targetDir, generateIsometricSequence(output_size, pitch, yaw, wireframe));
    }
}

void ModelGenerator::saveAssets(const std::string& item_id, const std::vector<RenderedFrame>& frames)
{
    std::string safeName = changeFilename(item_id);
    std::string targetDir = "img/" + safeName;

    if(!fs::exists(targetDir))
    {
        fs::create_directories(targetDir);
    }

    std::ofstream jsonFile(targetDir + "/model.json");
    jsonFile << std::setw(4) << model_json << std::endl;
    jsonFile.close();

    for(const auto& [path, animData] : textures)
    {
        if(animData.texture.getSize().x == 0) continue;

        std::string texName = changeFilename(path) + ".png";

        sf::Image img = animData.texture.copyToImage();
        if(!img.saveToFile(targetDir + "/" + texName)) std::cerr << "Error loading image" << std::endl;

        if(!animData.rawMcmeta.empty())
        {
            std::string metaName = texName + ".mcmeta";
            std::ofstream metaFile(targetDir + "/" + metaName);
            metaFile << animData.rawMcmeta;
            metaFile.close();
        }
    }

    bool isItem = false;
    if(model_json.contains("parent") && model_json["parent"].get<std::string>().find("item/generated") != std::string::npos) isItem = true;
    if(model_json.contains("textures") && model_json["textures"].contains("layer0")) isItem = true;

    if(!isItem) exportToObj(item_id, targetDir);

    saveAnimationWebP(item_id, targetDir, frames);
}

void ModelGenerator::saveAnimationWebP(const std::string& item_id, const std::string& output_dir, const std::vector<RenderedFrame>& frames)
{
    if(frames.empty()) return;

    std::string baseName = changeFilename(item_id);
    std::string filename = baseName + ".webp";
    std::string fullPath = output_dir + "/" + filename;

    int width = frames[0].image.getSize().x;
    int height = frames[0].image.getSize().y;

    WebPAnimEncoderOptions enc_options;
    WebPAnimEncoderOptionsInit(&enc_options);

    WebPAnimEncoder* enc = WebPAnimEncoderNew(width, height, &enc_options);
    if(!enc)
    {
        std::cerr << "WebP: Failed to create encoder" << std::endl;
        return;
    }

    int timestamp_ms = 0;

    for(const auto& img : frames)
    {
        WebPConfig config;
        WebPConfigInit(&config);
        config.lossless = 1;

        WebPPicture pic;
        WebPPictureInit(&pic);
        pic.width = width;
        pic.height = height;
        pic.use_argb = 1;

        if(!WebPPictureAlloc(&pic))
        {
            WebPAnimEncoderDelete(enc);
            return;
        }

        const uint8_t* pixels = img.image.getPixelsPtr();
        WebPPictureImportRGBA(&pic, pixels, width * 4);

        if(!WebPAnimEncoderAdd(enc, &pic, timestamp_ms, &config))
        {
            std::cerr << "WebP: Error adding frame" << std::endl;
            WebPPictureFree(&pic);
            WebPAnimEncoderDelete(enc);
            return;
        }

        WebPPictureFree(&pic);
        timestamp_ms += img.timeInTicks * 50;
    }

    WebPAnimEncoderAdd(enc, nullptr, timestamp_ms, nullptr);

    WebPData webp_data;
    WebPDataInit(&webp_data);
    WebPAnimEncoderAssemble(enc, &webp_data);

    if(!fs::exists(output_dir))
    {
        fs::create_directories(output_dir);
    }

    std::ofstream file(fullPath, std::ios::binary);
    if(file.is_open())
    {
        file.write(reinterpret_cast<const char*>(webp_data.bytes), webp_data.size);
        file.close();
    }

    WebPDataClear(&webp_data);
    WebPAnimEncoderDelete(enc);
}

void ModelGenerator::exportToObj(const std::string& item_id, const std::string& output_dir)
{
    std::string baseName = changeFilename(item_id);
    std::string objFilename = output_dir + "/" + baseName + ".obj";
    std::string mtlFilename = output_dir + "/" + baseName + ".mtl";
    std::string mtlBaseName = baseName + ".mtl";

    std::ofstream objFile(objFilename);
    std::ofstream mtlFile(mtlFilename);

    if(!objFile.is_open() || !mtlFile.is_open()) return;

    objFile << "# Exported using QuestiMakinator\n";
    objFile << "mtllib " << mtlBaseName << "\n";

    int vertexCount = 0;
    int uvCount = 0;

    std::set<std::string> usedMaterials;

    if(model_json.contains("elements"))
    {
        for(const auto& element : model_json["elements"])
        {
            auto from = element["from"];
            auto to = element["to"];

            sf::Vector3f pMin(from[0], from[1], from[2]);
            sf::Vector3f pMax(to[0], to[1], to[2]);
            sf::Vector3f rotOrigin(8, 8, 8);

            std::string rotAxis = "y";
            float rotAngle = 0;
            if(element.contains("rotation"))
            {
                auto& rot = element["rotation"];
                rotOrigin = { rot["origin"][0], rot["origin"][1], rot["origin"][2] };
                rotAxis = rot.value("axis", "y");
                rotAngle = rot.value("angle", 0.0f);
            }

            struct FaceDef
            {
                std::string dir;
                sf::Vector3f v[4];
                sf::Vector3f normal;
            };
            FaceDef faces[] = {
                { "up", { { pMin.x, pMax.y, pMin.z }, { pMax.x, pMax.y, pMin.z }, { pMax.x, pMax.y, pMax.z }, { pMin.x, pMax.y, pMax.z } }, { 0, 1, 0 } },
                { "down", { { pMin.x, pMin.y, pMin.z }, { pMax.x, pMin.y, pMin.z }, { pMax.x, pMin.y, pMax.z }, { pMin.x, pMin.y, pMax.z } }, { 0, -1, 0 } },
                { "north", { { pMax.x, pMax.y, pMin.z }, { pMin.x, pMax.y, pMin.z }, { pMin.x, pMin.y, pMin.z }, { pMax.x, pMin.y, pMin.z } }, { 0, 0, -1 } },
                { "south", { { pMin.x, pMax.y, pMax.z }, { pMax.x, pMax.y, pMax.z }, { pMax.x, pMin.y, pMax.z }, { pMin.x, pMin.y, pMax.z } }, { 0, 0, 1 } },
                { "west", { { pMin.x, pMax.y, pMin.z }, { pMin.x, pMax.y, pMax.z }, { pMin.x, pMin.y, pMax.z }, { pMin.x, pMin.y, pMin.z } }, { -1, 0, 0 } },
                { "east", { { pMax.x, pMax.y, pMax.z }, { pMax.x, pMax.y, pMin.z }, { pMax.x, pMin.y, pMin.z }, { pMax.x, pMin.y, pMax.z } }, { 1, 0, 0 } }
            };

            for(const auto& f : faces)
            {
                if(!element.contains("faces") || !element["faces"].contains(f.dir))
                {
                    continue;
                }
                auto faceJson = element["faces"][f.dir];

                std::string texRef = faceJson.value("texture", "");
                if(texRef.empty()) continue;

                int depth = 0;
                std::string resolvedPath = texRef;
                while(!resolvedPath.empty() && resolvedPath[0] == '#' && depth < 5)
                {
                    std::string key = resolvedPath.substr(1);
                    if(model_json["textures"].contains(key))
                    {
                        resolvedPath = model_json["textures"][key];
                    }
                    else
                    {
                        break;
                    }
                    depth++;
                }
                if(resolvedPath.empty()) continue;

                std::string matName = changeFilename(resolvedPath);
                usedMaterials.insert(resolvedPath);
                objFile << "usemtl " << matName << "\n";

                // Vertices
                for(int i = 0; i < 4; i++)
                {
                    sf::Vector3f rv = rotate_point(f.v[i], rotOrigin, rotAxis, rotAngle);
                    objFile << "v " << rv.x / 16.0f << " " << rv.y / 16.0f << " " << rv.z / 16.0f << "\n";
                }

                // UVs
                float u1 = 0, v1 = 0, u2 = 16, v2 = 16;
                if(faceJson.contains("uv"))
                {
                    u1 = faceJson["uv"][0];
                    v1 = faceJson["uv"][1];
                    u2 = faceJson["uv"][2];
                    v2 = faceJson["uv"][3];
                }
                else
                {
                    if(f.dir == "up" || f.dir == "down")
                    {
                        u1 = f.v[0].x;
                        v1 = f.v[0].z;
                        u2 = f.v[2].x;
                        v2 = f.v[2].z;
                    }
                    else
                    {
                        u1 = 0;
                        v1 = 0;
                        u2 = 16;
                        v2 = 16;
                    }
                }

                sf::Vector2f uvs[4];
                uvs[0] = { u1, v1 };
                uvs[1] = { u2, v1 };
                uvs[2] = { u2, v2 };
                uvs[3] = { u1, v2 };

                for(int k = 0; k < 4; k++)
                {
                    objFile << "vt " << uvs[k].x / 16.0f << " " << (16.0f - uvs[k].y) / 16.0f << "\n";
                }

                int baseV = vertexCount + 1;
                int baseVT = uvCount + 1;

                objFile << "f "
                        << baseV << "/" << baseVT << " "
                        << baseV + 1 << "/" << baseVT + 1 << " "
                        << baseV + 2 << "/" << baseVT + 2 << " "
                        << baseV + 3 << "/" << baseVT + 3 << "\n";

                vertexCount += 4;
                uvCount += 4;
            }
        }
    }
    objFile.close();

    for(const auto& mat : usedMaterials)
    {
        std::string matName = changeFilename(mat);
        std::string texFilename = matName + ".png";

        mtlFile << "newmtl " << matName << "\n";
        mtlFile << "Ka 1.000 1.000 1.000\n";
        mtlFile << "Kd 1.000 1.000 1.000\n";
        mtlFile << "d 1.0\n";
        mtlFile << "illum 2\n";
        mtlFile << "map_Kd " << texFilename << "\n\n";
    }
    mtlFile.close();
}

void ModelGenerator::saveAnimationPNG(const std::string& item_id,
    const std::string& output_dir,
    const std::vector<RenderedFrame>& frames)
{
    // Crear directorio si no existe
    fs::create_directories(output_dir);

    // Prefijo para los archivos: sanitizar item_id
    std::string safe_name = item_id;
    std::replace(safe_name.begin(), safe_name.end(), ':', '_');
    std::replace(safe_name.begin(), safe_name.end(), '/', '_');

    int num_frames = (int)frames.size();
    int digits = std::to_string(num_frames).length();

    for(int i = 0; i < num_frames; ++i)
    {
        std::ostringstream oss;
        oss << output_dir << "/" << safe_name << "_frame_"
            << std::setw(digits) << std::setfill('0') << i << ".png";
        std::string filename = oss.str();

        if(!frames[i].image.saveToFile(filename))
        {
            std::cerr << "Error guardando frame " << i << " en " << filename << std::endl;
        }
    }
}

namespace
{
    uint32_t apng_crc32(const unsigned char* buf, size_t len)
    {
        uint32_t c = 0xFFFFFFFF;
        for(size_t i = 0; i < len; i++)
        {
            c ^= buf[i];
            for(int k = 0; k < 8; k++)
            {
                if(c & 1)
                {
                    c = 0xEDB88320 ^ (c >> 1);
                }
                else
                {
                    c = c >> 1;
                }
            }
        }
        return c ^ 0xFFFFFFFF;
    }

    void write32(std::vector<unsigned char>& vec, uint32_t val)
    {
        vec.emplace_back((val >> 24) & 0xFF);
        vec.emplace_back((val >> 16) & 0xFF);
        vec.emplace_back((val >> 8) & 0xFF);
        vec.emplace_back(val & 0xFF);
    }

    void write16(std::vector<unsigned char>& vec, uint16_t val)
    {
        vec.emplace_back((val >> 8) & 0xFF);
        vec.emplace_back(val & 0xFF);
    }

    struct PngChunk
    {
        std::string type;
        std::vector<unsigned char> data;
    };

    std::vector<PngChunk> extract_chunks(const std::vector<unsigned char>& png)
    {
        std::vector<PngChunk> chunks;
        size_t pos = 8;
        while(pos + 8 <= png.size())
        {
            uint32_t length = (png[pos] << 24) | (png[pos + 1] << 16) | (png[pos + 2] << 8) | png[pos + 3];
            if(pos + 12 + length > png.size()) break;
            std::string type(reinterpret_cast<const char*>(&png[pos + 4]), 4);
            std::vector<unsigned char> data(png.begin() + pos + 8, png.begin() + pos + 8 + length);
            chunks.push_back({ type, data });
            pos += 12 + length;
        }
        return chunks;
    }

    void write_chunk(std::ofstream& out, const std::string& type, const std::vector<unsigned char>& data)
    {
        uint32_t len = data.size();
        unsigned char len_bytes[4] = {
            (unsigned char)((len >> 24) & 0xFF), (unsigned char)((len >> 16) & 0xFF),
            (unsigned char)((len >> 8) & 0xFF), (unsigned char)(len & 0xFF)
        };
        out.write(reinterpret_cast<char*>(len_bytes), 4);

        std::vector<unsigned char> crc_data;
        crc_data.insert(crc_data.end(), type.begin(), type.end());
        crc_data.insert(crc_data.end(), data.begin(), data.end());

        out.write(reinterpret_cast<char*>(crc_data.data()), crc_data.size());

        uint32_t crc = apng_crc32(crc_data.data(), crc_data.size());
        unsigned char crc_bytes[4] = {
            (unsigned char)((crc >> 24) & 0xFF), (unsigned char)((crc >> 16) & 0xFF),
            (unsigned char)((crc >> 8) & 0xFF), (unsigned char)(crc & 0xFF)
        };
        out.write(reinterpret_cast<char*>(crc_bytes), 4);
    }

} // namespace

void ModelGenerator::saveAnimationAPNG(const std::string& item_id, const std::string& output_dir, const std::vector<RenderedFrame>& frames)
{
    if(frames.empty()) return;

    std::string baseName = changeFilename(item_id);
    std::string filename = baseName + ".png";
    std::string fullPath = output_dir + "/" + filename;

    if(!fs::exists(output_dir))
    {
        fs::create_directories(output_dir);
    }

    std::ofstream file(fullPath, std::ios::binary);
    if(!file.is_open())
    {
        std::cerr << "APNG: Failed to open file for writing: " << fullPath << std::endl;
        return;
    }

    const unsigned char png_signature[8] = { 137, 80, 78, 71, 13, 10, 26, 10 };
    file.write(reinterpret_cast<const char*>(png_signature), 8);

    unsigned int width = frames[0].image.getSize().x;
    unsigned int height = frames[0].image.getSize().y;
    uint32_t seq_num = 0;

    for(size_t i = 0; i < frames.size(); ++i)
    {
        std::vector<unsigned char> png_data;
        lodepng::State state;
        state.encoder.auto_convert = 0;
        state.info_png.color.colortype = LCT_RGBA;
        state.info_png.color.bitdepth = 8;
        state.info_raw.colortype = LCT_RGBA;
        state.info_raw.bitdepth = 8;

        unsigned error = lodepng::encode(png_data, frames[i].image.getPixelsPtr(), width, height, state);
        if(error)
        {
            std::cerr << "APNG: Lodepng error on frame " << i << ": " << lodepng_error_text(error) << std::endl;
            return;
        }

        auto chunks = extract_chunks(png_data);
        if(i == 0)
        {
            for(const auto& chunk : chunks)
            {
                if(chunk.type == "IHDR")
                {
                    write_chunk(file, "IHDR", chunk.data);
                    std::vector<unsigned char> actl;
                    write32(actl, frames.size());
                    write32(actl, 0);
                    write_chunk(file, "acTL", actl);
                }
            }
        }

        std::vector<unsigned char> fctl;
        write32(fctl, seq_num++);
        write32(fctl, width);
        write32(fctl, height);
        write32(fctl, 0);
        write32(fctl, 0);

        write16(fctl, frames[i].timeInTicks * 50);
        write16(fctl, 1000);

        fctl.emplace_back(1);
        fctl.emplace_back(0);

        write_chunk(file, "fcTL", fctl);

        for(const auto& chunk : chunks)
        {
            if(chunk.type == "IDAT")
            {
                if(i == 0)
                {
                    write_chunk(file, "IDAT", chunk.data);
                }
                else
                {
                    std::vector<unsigned char> fdat;
                    write32(fdat, seq_num++);
                    fdat.insert(fdat.end(), chunk.data.begin(), chunk.data.end());
                    write_chunk(file, "fdAT", fdat);
                }
            }
        }
    }

    std::vector<unsigned char> empty_data;
    write_chunk(file, "IEND", empty_data);
    file.close();
}