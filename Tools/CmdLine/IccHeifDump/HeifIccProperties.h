/*
 * Copyright (c) International Color Consortium.
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Redistribution and use in source and binary forms, with or without modification,
 * are permitted provided that the following conditions are met:
 *
 *     1. Redistributions of source code must retain the above copyright notice,
 *        this list of conditions and the following disclaimer.
 *
 *     2. Redistributions in binary form must reproduce the above copyright
 *        notice, this list of conditions and the following disclaimer in the
 *        documentation and/or other materials provided with the distribution.
 *
 *     3. Neither the name of International Color Consortium (ICC) nor the names of
 *        its contributors may be used to endorse or promote products derived from
 *        this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR
 * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 * ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef ICCDEV_HEIF_ICC_PROPERTIES_H
#define ICCDEV_HEIF_ICC_PROPERTIES_H

#include <cstddef>
#include <cstdint>
#include <fstream>
#include <string>
#include <vector>

namespace iccdev_heif
{
  struct Box
  {
    std::size_t payload;
    std::size_t end;
    std::string type;
  };

  struct IccProperty
  {
    std::uint32_t itemId;
    std::uint32_t propertyIndex;
    bool essential;
    std::string colourType;
    std::size_t profileOffset;
    std::size_t profileSize;
  };

  inline std::uint32_t read32(const std::vector<std::uint8_t>& bytes, std::size_t offset)
  {
    return (std::uint32_t(bytes[offset]) << 24) |
           (std::uint32_t(bytes[offset + 1]) << 16) |
           (std::uint32_t(bytes[offset + 2]) << 8) |
           std::uint32_t(bytes[offset + 3]);
  }

  inline std::uint64_t read64(const std::vector<std::uint8_t>& bytes, std::size_t offset)
  {
    return (std::uint64_t(read32(bytes, offset)) << 32) | read32(bytes, offset + 4);
  }

  inline bool nextBox(const std::vector<std::uint8_t>& bytes, std::size_t& offset,
                      std::size_t limit, Box& box)
  {
    if (offset > limit || limit - offset < 8) {
      return false;
    }
    std::uint64_t size = read32(bytes, offset);
    const std::size_t header = size == 1 ? 16 : 8;
    if (limit - offset < header) {
      return false;
    }
    if (size == 1) {
      size = read64(bytes, offset + 8);
    } else if (size == 0) {
      size = limit - offset;
    }
    if (size < header || size > limit - offset) {
      return false;
    }
    box.type.assign(reinterpret_cast<const char*>(bytes.data() + offset + 4), 4);
    box.payload = offset + header;
    box.end = offset + static_cast<std::size_t>(size);
    offset = box.end;
    return true;
  }

  inline bool findBox(const std::vector<std::uint8_t>& bytes, std::size_t start,
                      std::size_t limit, const char* type, Box& result)
  {
    for (std::size_t offset = start; offset < limit;) {
      Box box;
      if (!nextBox(bytes, offset, limit, box)) {
        return false;
      }
      if (box.type == type) {
        result = box;
        return true;
      }
    }
    return false;
  }

  inline bool collectIccProperties(const char* path, std::vector<IccProperty>& profiles,
                                   std::vector<std::uint8_t>& bytes)
  {
    // Bound the carrier, association table, and total bytes eligible for output.
    constexpr std::streamoff maxCarrierBytes = 256 * 1024 * 1024;
    constexpr std::size_t maxAssociations = 4096;
    constexpr std::size_t maxExtractedBytes = 256 * 1024 * 1024;
    constexpr std::size_t maxProperties = 32767;
    std::ifstream input(path, std::ios::binary | std::ios::ate);
    if (!input) {
      return false;
    }
    const std::streamoff length = input.tellg();
    if (length <= 0 || length > maxCarrierBytes) {
      return false;
    }
    input.seekg(0, std::ios::beg);
    bytes.resize(static_cast<std::size_t>(length));
    if (!input.read(reinterpret_cast<char*>(bytes.data()),
                    static_cast<std::streamsize>(length))) {
      return false;
    }
    Box meta;
    if (!findBox(bytes, 0, bytes.size(), "meta", meta) ||
        meta.end - meta.payload < 4) {
      return false;
    }
    Box iprp;
    if (!findBox(bytes, meta.payload + 4, meta.end, "iprp", iprp)) {
      return false;
    }
    Box ipco;
    if (!findBox(bytes, iprp.payload, iprp.end, "ipco", ipco)) {
      return false;
    }

    std::vector<Box> properties;
    for (std::size_t offset = ipco.payload; offset < ipco.end;) {
      Box box;
      if (!nextBox(bytes, offset, ipco.end, box)) {
        return false;
      }
      if (properties.size() >= maxProperties) {
        return false;
      }
      properties.push_back(box);
    }

    std::size_t associationCount = 0;
    std::size_t entryCount = 0;
    std::size_t extractedBytes = 0;
    for (std::size_t boxOffset = iprp.payload; boxOffset < iprp.end;) {
      Box ipma;
      if (!nextBox(bytes, boxOffset, iprp.end, ipma)) {
        return false;
      }
      if (ipma.type != "ipma") {
        continue;
      }
      std::size_t offset = ipma.payload;
      if (ipma.end - offset < 8) {
        return false;
      }
      const std::uint8_t version = bytes[offset];
      if (version > 1) {
        return false;
      }
      const bool widePropertyIndex = (bytes[offset + 3] & 1) != 0;
      offset += 4;
      const std::uint32_t entries = read32(bytes, offset);
      offset += 4;
      if (entries > maxAssociations - entryCount) {
        return false;
      }
      entryCount += entries;
      for (std::uint32_t entry = 0; entry < entries; ++entry) {
        const std::size_t idSize = version == 0 ? 2 : 4;
        if (ipma.end - offset < idSize + 1) {
          return false;
        }
        const std::uint32_t itemId = version == 0
            ? (std::uint32_t(bytes[offset]) << 8) | bytes[offset + 1]
            : read32(bytes, offset);
        offset += idSize;
        const std::uint8_t count = bytes[offset++];
        if (count > maxAssociations - associationCount) {
          return false;
        }
        associationCount += count;
        for (std::uint8_t association = 0; association < count; ++association) {
          const std::size_t indexSize = widePropertyIndex ? 2 : 1;
          if (ipma.end - offset < indexSize) {
            return false;
          }
          const std::uint32_t value = widePropertyIndex
              ? (std::uint32_t(bytes[offset]) << 8) | bytes[offset + 1]
              : bytes[offset];
          offset += indexSize;
          const std::uint32_t index = value & (widePropertyIndex ? 0x7fff : 0x7f);
          if (index == 0) {
            continue;
          }
          if (index > properties.size()) {
            return false;
          }
          const Box& property = properties[index - 1];
          if (property.type != "colr" || property.end - property.payload < 4) {
            continue;
          }
          const std::string colourType(
              reinterpret_cast<const char*>(bytes.data() + property.payload), 4);
          if (colourType != "prof" && colourType != "rICC") {
            continue;
          }
          const std::size_t profileSize = property.end - property.payload - 4;
          if (profileSize > maxExtractedBytes - extractedBytes) {
            return false;
          }
          extractedBytes += profileSize;
          IccProperty profile;
          profile.itemId = itemId;
          profile.propertyIndex = index - 1;
          profile.essential = (value & (widePropertyIndex ? 0x8000 : 0x80)) != 0;
          profile.colourType = colourType;
          profile.profileOffset = property.payload + 4;
          profile.profileSize = profileSize;
          profiles.push_back(profile);
        }
      }
    }
    return true;
  }
}

#endif
