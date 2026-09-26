/** @file
    File:       IccHeifDump.cpp

    Contains:   Research Tooling for HEIF InterOp Testing

    Version:    V1

    Copyright:  (c) see Software License
*/

/*
 * Copyright (c) International Color Consortium.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 *
 * 3. In the absence of prior written permission, the names "ICC" and "The
 *    International Color Consortium" must not be used to imply that the
 *    ICC organization endorses or promotes products derived from this
 *    software.
 *
 *
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND ANY EXPRESSED OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED.  IN NO EVENT SHALL THE INTERNATIONAL COLOR CONSORTIUM OR
 * ITS CONTRIBUTING MEMBERS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF
 * USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT
 * OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 * ====================================================================
 *
 * This software consists of voluntary contributions made by many
 * individuals on behalf of the The International Color Consortium.
 *
 *
 * Membership in the ICC is encouraged when this software is used for
 * commercial purposes.
 *
 *
 * For more information on The International Color Consortium, please
 * see <http://www.color.org/>.
 *
 *
 */

#include <array>
#include <cerrno>
#include <cstdint>
#include <cstdlib>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>

#include "heifreader.h"

#if defined(__clang__)
#define HEIF_SHA256_WRAPAROUND __attribute__((no_sanitize("unsigned-integer-overflow")))
#else
#define HEIF_SHA256_WRAPAROUND
#endif

namespace
{
    class Sha256
    {
    public:
        Sha256()
            : mState{{0x6a09e667u, 0xbb67ae85u, 0x3c6ef372u, 0xa54ff53au,
                      0x510e527fu, 0x9b05688cu, 0x1f83d9abu, 0x5be0cd19u}}
            , mBuffer{}
            , mBufferSize(0)
            , mTotalSize(0)
        {
        }

        void update(const std::uint8_t* data, std::size_t size)
        {
            mTotalSize += size;
            while (size > 0)
            {
                const std::size_t available = mBuffer.size() - mBufferSize;
                const std::size_t count     = size < available ? size : available;
                for (std::size_t i = 0; i < count; ++i)
                {
                    mBuffer[mBufferSize + i] = data[i];
                }
                mBufferSize += count;
                data += count;
                size -= count;

                if (mBufferSize == mBuffer.size())
                {
                    transform(mBuffer.data());
                    mBufferSize = 0;
                }
            }
        }

        std::string finish()
        {
            const std::uint64_t bitSize = mTotalSize * 8;
            mBuffer[mBufferSize++]      = 0x80;
            if (mBufferSize > 56)
            {
                while (mBufferSize < mBuffer.size())
                {
                    mBuffer[mBufferSize++] = 0;
                }
                transform(mBuffer.data());
                mBufferSize = 0;
            }
            while (mBufferSize < 56)
            {
                mBuffer[mBufferSize++] = 0;
            }
            for (unsigned int i = 0; i < 8; ++i)
            {
                mBuffer[63 - i] = static_cast<std::uint8_t>(bitSize >> (i * 8));
            }
            transform(mBuffer.data());

            std::ostringstream digest;
            digest << std::hex << std::setfill('0');
            for (const auto word : mState)
            {
                digest << std::setw(8) << word;
            }
            return digest.str();
        }

    private:
        HEIF_SHA256_WRAPAROUND static std::uint32_t rotateRight(const std::uint32_t value,
                                                                const unsigned int count)
        {
            return (value >> count) |
                   static_cast<std::uint32_t>(static_cast<std::uint64_t>(value) << (32 - count));
        }

        HEIF_SHA256_WRAPAROUND void transform(const std::uint8_t* block)
        {
            static const std::uint32_t constants[64] = {
                0x428a2f98u, 0x71374491u, 0xb5c0fbcfu, 0xe9b5dba5u, 0x3956c25bu, 0x59f111f1u, 0x923f82a4u,
                0xab1c5ed5u, 0xd807aa98u, 0x12835b01u, 0x243185beu, 0x550c7dc3u, 0x72be5d74u, 0x80deb1feu,
                0x9bdc06a7u, 0xc19bf174u, 0xe49b69c1u, 0xefbe4786u, 0x0fc19dc6u, 0x240ca1ccu, 0x2de92c6fu,
                0x4a7484aau, 0x5cb0a9dcu, 0x76f988dau, 0x983e5152u, 0xa831c66du, 0xb00327c8u, 0xbf597fc7u,
                0xc6e00bf3u, 0xd5a79147u, 0x06ca6351u, 0x14292967u, 0x27b70a85u, 0x2e1b2138u, 0x4d2c6dfcu,
                0x53380d13u, 0x650a7354u, 0x766a0abbu, 0x81c2c92eu, 0x92722c85u, 0xa2bfe8a1u, 0xa81a664bu,
                0xc24b8b70u, 0xc76c51a3u, 0xd192e819u, 0xd6990624u, 0xf40e3585u, 0x106aa070u, 0x19a4c116u,
                0x1e376c08u, 0x2748774cu, 0x34b0bcb5u, 0x391c0cb3u, 0x4ed8aa4au, 0x5b9cca4fu, 0x682e6ff3u,
                0x748f82eeu, 0x78a5636fu, 0x84c87814u, 0x8cc70208u, 0x90befffau, 0xa4506cebu, 0xbef9a3f7u,
                0xc67178f2u};

            std::uint32_t words[64];
            for (unsigned int i = 0; i < 16; ++i)
            {
                words[i] = (static_cast<std::uint32_t>(block[i * 4]) << 24) |
                           (static_cast<std::uint32_t>(block[i * 4 + 1]) << 16) |
                           (static_cast<std::uint32_t>(block[i * 4 + 2]) << 8) |
                           static_cast<std::uint32_t>(block[i * 4 + 3]);
            }
            for (unsigned int i = 16; i < 64; ++i)
            {
                const std::uint32_t s0 = rotateRight(words[i - 15], 7) ^ rotateRight(words[i - 15], 18) ^
                                         (words[i - 15] >> 3);
                const std::uint32_t s1 = rotateRight(words[i - 2], 17) ^ rotateRight(words[i - 2], 19) ^
                                         (words[i - 2] >> 10);
                words[i] = words[i - 16] + s0 + words[i - 7] + s1;
            }

            std::uint32_t a = mState[0];
            std::uint32_t b = mState[1];
            std::uint32_t c = mState[2];
            std::uint32_t d = mState[3];
            std::uint32_t e = mState[4];
            std::uint32_t f = mState[5];
            std::uint32_t g = mState[6];
            std::uint32_t h = mState[7];

            for (unsigned int i = 0; i < 64; ++i)
            {
                const std::uint32_t sum1 = rotateRight(e, 6) ^ rotateRight(e, 11) ^ rotateRight(e, 25);
                const std::uint32_t choice = (e & f) ^ ((~e) & g);
                const std::uint32_t temp1  = h + sum1 + choice + constants[i] + words[i];
                const std::uint32_t sum0 = rotateRight(a, 2) ^ rotateRight(a, 13) ^ rotateRight(a, 22);
                const std::uint32_t majority = (a & b) ^ (a & c) ^ (b & c);
                const std::uint32_t temp2    = sum0 + majority;

                h = g;
                g = f;
                f = e;
                e = d + temp1;
                d = c;
                c = b;
                b = a;
                a = temp1 + temp2;
            }

            mState[0] += a;
            mState[1] += b;
            mState[2] += c;
            mState[3] += d;
            mState[4] += e;
            mState[5] += f;
            mState[6] += g;
            mState[7] += h;
        }

        std::array<std::uint32_t, 8> mState;
        std::array<std::uint8_t, 64> mBuffer;
        std::size_t mBufferSize;
        std::uint64_t mTotalSize;
    };

    std::string sha256(const HEIF::Array<std::uint8_t>& data)
    {
        Sha256 hash;
        hash.update(data.elements, data.size);
        return hash.finish();
    }

    std::string outputPath(const std::string& directory,
                           const std::uint32_t itemId,
                           const std::uint32_t propertyIndex,
                           const char* colourType)
    {
        std::ostringstream path;
        path << directory;
        if (!directory.empty() && directory.back() != '/')
        {
            path << '/';
        }
        path << "item-" << itemId << "-property-" << propertyIndex << '-' << colourType << ".icc";
        return path.str();
    }

    bool writeProfile(const std::string& path, const HEIF::Array<std::uint8_t>& profile)
    {
        std::ofstream output(path, std::ios::binary);
        if (!output)
        {
            return false;
        }
        output.write(reinterpret_cast<const char*>(profile.elements), static_cast<std::streamsize>(profile.size));
        return output.good();
    }

    void printUsage(const char* executable)
    {
        std::cerr << "Usage: " << executable << " input.heic [existing-output-directory]\n"
                  << "       " << executable << " --colr-property INDEX input.heic" << std::endl;
    }

    bool parsePropertyIndex(const char* text, std::uint32_t& index)
    {
        errno     = 0;
        char* end = nullptr;
        const unsigned long value = std::strtoul(text, &end, 10);
        if (errno != 0 || end == text || *end != '\0' || value > 0xffffffffUL)
        {
            return false;
        }
        index = static_cast<std::uint32_t>(value);
        return true;
    }
}

int main(int argc, char* argv[])
{
    const bool propertyProbeRequested = argc >= 2 && std::string(argv[1]) == "--colr-property";
    const bool propertyProbe = propertyProbeRequested && argc == 4;
    if ((propertyProbeRequested && !propertyProbe) ||
        (!propertyProbeRequested && (argc < 2 || argc > 3)))
    {
        printUsage(argv[0]);
        return 3;
    }

    std::uint32_t propertyIndex = 0;
    if (propertyProbe && !parsePropertyIndex(argv[2], propertyIndex))
    {
        std::cerr << "Error: invalid property index: " << argv[2] << std::endl;
        return 3;
    }

    const char* inputPath = propertyProbe ? argv[3] : argv[1];

    HEIF::Reader* reader = HEIF::Reader::Create();
    if (reader == nullptr)
    {
        std::cerr << "Error: unable to create HEIF reader" << std::endl;
        return 2;
    }

    const HEIF::ErrorCode initializeResult = reader->initialize(inputPath);
    if (initializeResult != HEIF::ErrorCode::OK)
    {
        std::cerr << "Error: unable to initialize HEIF reader: " << static_cast<int>(initializeResult) << std::endl;
        HEIF::Reader::Destroy(reader);
        return 2;
    }

    HEIF::FileInformation fileInformation;
    const HEIF::ErrorCode informationResult = reader->getFileInformation(fileInformation);
    if (informationResult != HEIF::ErrorCode::OK)
    {
        std::cerr << "Error: unable to read HEIF file information: " << static_cast<int>(informationResult)
                  << std::endl;
        HEIF::Reader::Destroy(reader);
        return 2;
    }

    if (propertyProbe)
    {
        bool isColourProperty = false;
        for (const auto& item : fileInformation.rootMetaBoxInformation.itemInformations)
        {
            HEIF::Array<HEIF::ItemPropertyInfo> properties;
            const HEIF::ErrorCode propertiesResult = reader->getItemProperties(item.itemId, properties);
            if (propertiesResult == HEIF::ErrorCode::INVALID_ITEM_ID)
            {
                continue;
            }
            if (propertiesResult != HEIF::ErrorCode::OK)
            {
                std::cerr << "Error: unable to read properties for item " << item.itemId.get() << ": "
                          << static_cast<int>(propertiesResult) << std::endl;
                HEIF::Reader::Destroy(reader);
                return 2;
            }

            for (const auto& property : properties)
            {
                if (property.index.get() == propertyIndex && property.type == HEIF::ItemPropertyType::COLR)
                {
                    isColourProperty = true;
                    break;
                }
            }
            if (isColourProperty)
            {
                break;
            }
        }

        if (!isColourProperty)
        {
            std::cout << "property=" << propertyIndex << " result="
                      << static_cast<int>(HEIF::ErrorCode::INVALID_PROPERTY_INDEX) << std::endl;
            HEIF::Reader::Destroy(reader);
            return 2;
        }

        HEIF::ColourInformation colourInformation{};
        const HEIF::ErrorCode propertyResult =
            reader->getProperty(HEIF::PropertyId(propertyIndex), colourInformation);
        std::cout << "property=" << propertyIndex << " result=" << static_cast<int>(propertyResult) << std::endl;
        HEIF::Reader::Destroy(reader);
        return propertyResult == HEIF::ErrorCode::OK ? 0 : 2;
    }

    std::size_t profileCount = 0;
    for (const auto& item : fileInformation.rootMetaBoxInformation.itemInformations)
    {
        HEIF::Array<HEIF::ItemPropertyInfo> properties;
        const HEIF::ErrorCode propertiesResult = reader->getItemProperties(item.itemId, properties);
        if (propertiesResult == HEIF::ErrorCode::INVALID_ITEM_ID)
        {
            continue;
        }
        if (propertiesResult != HEIF::ErrorCode::OK)
        {
            std::cerr << "Error: unable to read properties for item " << item.itemId.get() << ": "
                      << static_cast<int>(propertiesResult) << std::endl;
            HEIF::Reader::Destroy(reader);
            return 2;
        }

        for (const auto& property : properties)
        {
            if (property.type != HEIF::ItemPropertyType::COLR)
            {
                continue;
            }

            HEIF::ColourInformation colourInformation{};
            const HEIF::ErrorCode propertyResult = reader->getProperty(property.index, colourInformation);
            if (propertyResult != HEIF::ErrorCode::OK)
            {
                std::cerr << "Error: unable to read colr property " << property.index.get() << " for item "
                          << item.itemId.get() << ": " << static_cast<int>(propertyResult) << std::endl;
                HEIF::Reader::Destroy(reader);
                return 2;
            }

            const bool isIcc = colourInformation.colourType == HEIF::FourCC("rICC") ||
                               colourInformation.colourType == HEIF::FourCC("prof");
            std::cout << "item=" << item.itemId.get() << " property=" << property.index.get()
                      << " essential=" << (property.essential ? 1 : 0)
                      << " colour_type=" << colourInformation.colourType.value
                      << " profile_bytes=" << colourInformation.iccProfile.size;
            if (isIcc)
            {
                ++profileCount;
                std::cout << " sha256=" << sha256(colourInformation.iccProfile);
                if (argc == 3)
                {
                    const std::string path = outputPath(argv[2], item.itemId.get(), property.index.get(),
                                                        colourInformation.colourType.value);
                    if (!writeProfile(path, colourInformation.iccProfile))
                    {
                        std::cerr << "Error: unable to write extracted ICC profile: " << path << std::endl;
                        HEIF::Reader::Destroy(reader);
                        return 2;
                    }
                    std::cout << " output=" << path;
                }
            }
            std::cout << std::endl;
        }
    }

    std::cout << "icc_profiles=" << profileCount << std::endl;
    HEIF::Reader::Destroy(reader);
    return 0;
}
