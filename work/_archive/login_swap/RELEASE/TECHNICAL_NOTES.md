# Technical Notes — APB 2011 Main Menu Mod

## uMod hash algorithm

Source: `uMod_IDirect3DTexture9::GetHash()` in uMod source.

```cpp
int size = (GetBitsFromFormat(desc.Format) * desc.Width * desc.Height) / 8;
hash = GetCRC32((char*) d3dlr.pBits, size);
```

- Hashes exactly `width * height * bits_per_pixel / 8` bytes.
- No pitch padding is included.
- CRC32 polynomial: `0xEDB88320`
- Initial value: `0xFFFFFFFF`
- No final XOR.

## Hash size

- uMod source defines `MyTypeHash` as `DWORD32` (32-bit) by default.
- Some debug/logging builds may use `DWORD64` (64-bit).
- The standard release binary (`uMod_alpha_v2_r53.zip`) uses 32-bit hashes.
- Our pre-computed hashes are 32-bit and match the standard binary.

## Why pre-computed hashes may still fail

The pre-computed hashes assume the runtime locked surface format matches the exported TGA pixel layout. In practice, APB may store textures in DXT-compressed or other GPU-native formats. When uMod locks the surface, it hashes the raw locked bytes, which may be DXT blocks rather than decompressed BGRA/RGBA pixels. UE Viewer exports decompressed TGA images, so the hashed bytes will differ.

The v2 mod includes fallback hashes for common decompressed layouts, but these are a best-effort starting point only.

## Reliable method

Use uMod's built-in texture capture to get the exact runtime hashes, then use `capture_and_build.ps1` to rebuild the mod. This is the only method guaranteed to match the runtime locked surface.
