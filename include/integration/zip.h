#pragma once
#include <fstream>
#include <vector>
#include <string>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <zlib.h>

class ZipWriter
{
  private:
    struct CentralDirEntry
    {
        std::string filename;
        uint32_t crc32;
        uint32_t compSize;
        uint32_t uncompSize;
        uint32_t offset;
    };

    std::ofstream zipFile;
    std::vector<CentralDirEntry> cdEntries;

    void write16(uint16_t val)
    {
        zipFile.write(reinterpret_cast<const char*>(&val), sizeof(val));
    }
    void write32(uint32_t val)
    {
        zipFile.write(reinterpret_cast<const char*>(&val), sizeof(val));
    }

  public:
    ZipWriter(const std::string& outputPath)
    {
        std::filesystem::path p(outputPath);
        if(p.has_parent_path())
        {
            std::error_code ec;
            std::filesystem::create_directories(p.parent_path(), ec);
        }
        zipFile.open(outputPath, std::ios::binary);
    }

    ~ZipWriter()
    {
        if(zipFile.is_open())
        {
            close();
        }
    }

    bool isOpen() const
    {
        return zipFile.is_open();
    }

    bool addFile(const std::string& filenameInZip, const std::string& data)
    {
        if(!zipFile.is_open())
        {
            return false;
        }

        uint32_t offset = static_cast<uint32_t>(zipFile.tellp());
        uint32_t crc = crc32(0L, reinterpret_cast<const Bytef*>(data.data()), static_cast<uInt>(data.size()));
        uint32_t uncompSize = static_cast<uint32_t>(data.size());

        std::vector<uint8_t> compData;
        z_stream strm;
        std::memset(&strm, 0, sizeof(strm));

        if(deflateInit2(&strm, Z_DEFAULT_COMPRESSION, Z_DEFLATED, -15, 8, Z_DEFAULT_STRATEGY) != Z_OK)
        {
            return false;
        }

        strm.next_in = reinterpret_cast<Bytef*>(const_cast<char*>(data.data()));
        strm.avail_in = static_cast<uInt>(data.size());

        std::vector<uint8_t> buffer(32768);
        int ret;
        do
        {
            strm.next_out = buffer.data();
            strm.avail_out = static_cast<uInt>(buffer.size());
            ret = deflate(&strm, Z_FINISH);
            size_t bytesWritten = buffer.size() - strm.avail_out;
            compData.insert(compData.end(), buffer.data(), buffer.data() + bytesWritten);
        } while(ret == Z_OK);

        deflateEnd(&strm);

        if(ret != Z_STREAM_END)
        {
            return false;
        }

        uint32_t compSize = static_cast<uint32_t>(compData.size());
        uint16_t fnLen = static_cast<uint16_t>(filenameInZip.size());

        write32(0x04034b50);
        write16(20);
        write16(0);
        write16(8);
        write16(0);
        write16(0);
        write32(crc);
        write32(compSize);
        write32(uncompSize);
        write16(fnLen);
        write16(0);
        zipFile.write(filenameInZip.data(), fnLen);

        zipFile.write(reinterpret_cast<const char*>(compData.data()), compSize);

        cdEntries.push_back({ filenameInZip, crc, compSize, uncompSize, offset });
        return true;
    }

    void close()
    {
        if(!zipFile.is_open())
        {
            return;
        }

        uint32_t cdOffset = static_cast<uint32_t>(zipFile.tellp());

        for(const auto& entry : cdEntries)
        {
            uint16_t fnLen = static_cast<uint16_t>(entry.filename.size());

            write32(0x02014b50);
            write16(20);
            write16(20);
            write16(0);
            write16(8);
            write16(0);
            write16(0);
            write32(entry.crc32);
            write32(entry.compSize);
            write32(entry.uncompSize);
            write16(fnLen);
            write16(0);
            write16(0);
            write16(0);
            write16(0);
            write32(0);
            write32(entry.offset);
            zipFile.write(entry.filename.data(), fnLen);
        }

        uint32_t cdSize = static_cast<uint32_t>(zipFile.tellp()) - cdOffset;
        uint16_t numEntries = static_cast<uint16_t>(cdEntries.size() & 0xFFFF);

        write32(0x06054b50);
        write16(0);
        write16(0);
        write16(numEntries);
        write16(numEntries);
        write32(cdSize);
        write32(cdOffset);
        write16(0);

        zipFile.close();
    }
};