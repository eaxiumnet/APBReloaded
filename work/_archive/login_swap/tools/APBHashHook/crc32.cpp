#include "d3d9.h"
#include <cstdio>

namespace APBHashHook {

unsigned int ComputeCRC32(const void* data, unsigned int len) {
    const unsigned char* bytes = static_cast<const unsigned char*>(data);
    unsigned int crc = 0xFFFFFFFF;
    for (unsigned int i = 0; i < len; i++) {
        unsigned int byte = bytes[i];
        for (int bit = 0; bit < 8; bit++, byte >>= 1) {
            crc = (crc >> 1) ^ (((crc ^ byte) & 1) ? 0xEDB88320 : 0);
        }
    }
    return crc;
}

} // namespace APBHashHook
