#!/usr/bin/env python3
"""Verify uMod CRC32 implementation against known test vectors."""
import binascii


def umod_crc32(data: bytes) -> int:
    crc = 0xFFFFFFFF
    for byte in data:
        data_val = byte
        for _ in range(8):
            bit = (crc ^ data_val) & 1
            crc >>= 1
            if bit:
                crc ^= 0xEDB88320
            data_val >>= 1
    return crc


def main():
    test = b"123456789"
    umod = umod_crc32(test)
    standard = binascii.crc32(test) & 0xFFFFFFFF
    print(f"uMod CRC32 of '{test.decode()}': {umod:08X}")
    print(f"Standard CRC32 of '{test.decode()}': {standard:08X}")
    # uMod is standard CRC32 with init 0xFFFFFFFF and no final XOR.
    # Standard CRC32 uses init 0xFFFFFFFF and final XOR 0xFFFFFFFF.
    # So umod = standard ^ 0xFFFFFFFF for the same data.
    print(f"Expected uMod if standard CRC32: {standard ^ 0xFFFFFFFF:08X}")
    print(f"Match: {umod == (standard ^ 0xFFFFFFFF)}")


if __name__ == "__main__":
    main()
