#pragma once
#include <d3d9.h>

namespace APBHashHook {

// Forward declarations
class MyDirect3D9;
class MyDirect3DDevice9;
class MyDirect3DTexture9;

// CRC32 helper
unsigned int ComputeCRC32(const void* data, unsigned int len);

// Logging
void LogHash(IDirect3DTexture9* tex, unsigned int hash, int width, int height, D3DFORMAT format);
void LogMessage(const char* fmt, ...);

} // namespace APBHashHook
