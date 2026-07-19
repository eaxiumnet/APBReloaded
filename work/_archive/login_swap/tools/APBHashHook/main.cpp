#define WIN32_LEAN_AND_MEAN
#include "d3d9.h"
#include <windows.h>
#include <cstdio>
#include <cstdarg>
#include <string>

namespace APBHashHook {

static HMODULE g_RealD3D9 = nullptr;
static FILE* g_LogFile = nullptr;
static unsigned int g_TextureCount = 0;

void LogMessage(const char* fmt, ...) {
    if (!g_LogFile) {
        g_LogFile = fopen("D:\\APBReloaded\\work\\login_swap\\mod\\captures\\apb_hash_hook.log", "a");
        if (!g_LogFile) return;
    }
    va_list args;
    va_start(args, fmt);
    vfprintf(g_LogFile, fmt, args);
    va_end(args);
    fflush(g_LogFile);
}

void LogHash(IDirect3DTexture9* tex, unsigned int hash, int width, int height, D3DFORMAT format) {
    LogMessage("Texture %p: hash=%08X width=%d height=%d format=%d\n", tex, hash, width, height, format);
}

typedef IDirect3D9* (WINAPI* Direct3DCreate9_t)(UINT SDKVersion);

class MyDirect3DTexture9 : public IDirect3DTexture9 {
public:
    MyDirect3DTexture9(IDirect3DTexture9* real, int width, int height, D3DFORMAT format)
        : m_real(real), m_width(width), m_height(height), m_format(format), m_refCount(1) {}

    STDMETHOD(QueryInterface)(REFIID riid, void** ppvObj) { return m_real->QueryInterface(riid, ppvObj); }
    STDMETHOD_(ULONG, AddRef)() { return ++m_refCount; }
    STDMETHOD_(ULONG, Release)() {
        ULONG ref = --m_refCount;
        if (ref == 0) {
            m_real->Release();
            delete this;
        }
        return ref;
    }
    STDMETHOD(GetDevice)(IDirect3DDevice9** ppDevice) { return m_real->GetDevice(ppDevice); }
    STDMETHOD(SetPrivateData)(REFGUID refguid, const void* pData, DWORD SizeOfData, DWORD Flags) { return m_real->SetPrivateData(refguid, pData, SizeOfData, Flags); }
    STDMETHOD(GetPrivateData)(REFGUID refguid, void* pData, DWORD* pSizeOfData) { return m_real->GetPrivateData(refguid, pData, pSizeOfData); }
    STDMETHOD(FreePrivateData)(REFGUID refguid) { return m_real->FreePrivateData(refguid); }
    STDMETHOD_(DWORD, SetPriority)(DWORD PriorityNew) { return m_real->SetPriority(PriorityNew); }
    STDMETHOD_(DWORD, GetPriority)() { return m_real->GetPriority(); }
    STDMETHOD_(void, PreLoad)() { m_real->PreLoad(); }
    STDMETHOD_(D3DRESOURCETYPE, GetType)() { return m_real->GetType(); }
    STDMETHOD_(DWORD, SetLOD)(DWORD LODNew) { return m_real->SetLOD(LODNew); }
    STDMETHOD_(DWORD, GetLOD)() { return m_real->GetLOD(); }
    STDMETHOD_(DWORD, GetLevelCount)() { return m_real->GetLevelCount(); }
    STDMETHOD(SetAutoGenFilterType)(D3DTEXTUREFILTERTYPE FilterType) { return m_real->SetAutoGenFilterType(FilterType); }
    STDMETHOD_(D3DTEXTUREFILTERTYPE, GetAutoGenFilterType)() { return m_real->GetAutoGenFilterType(); }
    STDMETHOD_(void, GenerateMipSubLevels)() { m_real->GenerateMipSubLevels(); }
    STDMETHOD(GetLevelDesc)(UINT Level, D3DSURFACE_DESC* pDesc) { return m_real->GetLevelDesc(Level, pDesc); }
    STDMETHOD(GetSurfaceLevel)(UINT Level, IDirect3DSurface9** ppSurfaceLevel) { return m_real->GetSurfaceLevel(Level, ppSurfaceLevel); }

    STDMETHOD(LockRect)(UINT Level, D3DLOCKED_RECT* pLockedRect, const RECT* pRect, DWORD Flags) {
        HRESULT hr = m_real->LockRect(Level, pLockedRect, pRect, Flags);
        if (SUCCEEDED(hr) && pLockedRect && Level == 0) {
            int bpp = 32;
            switch (m_format) {
                case D3DFMT_A8R8G8B8: case D3DFMT_X8R8G8B8: case D3DFMT_A8B8G8R8: case D3DFMT_X8B8G8R8:
                    bpp = 32; break;
                case D3DFMT_R8G8B8: bpp = 24; break;
                case D3DFMT_A4R4G4B4: case D3DFMT_X4R4G4B4: case D3DFMT_R5G6B5: case D3DFMT_A1R5G5B5:
                    bpp = 16; break;
                case D3DFMT_DXT1: bpp = 4; break;
                case D3DFMT_DXT3: case D3DFMT_DXT5: bpp = 8; break;
                default: bpp = 32; break;
            }
            unsigned int size = (bpp * m_width * m_height) / 8;
            unsigned int hash = ComputeCRC32(pLockedRect->pBits, size);
            LogHash(m_real, hash, m_width, m_height, m_format);
        }
        return hr;
    }

    STDMETHOD(UnlockRect)(UINT Level) { return m_real->UnlockRect(Level); }
    STDMETHOD(AddDirtyRect)(const RECT* pDirtyRect) { return m_real->AddDirtyRect(pDirtyRect); }

private:
    IDirect3DTexture9* m_real;
    int m_width;
    int m_height;
    D3DFORMAT m_format;
    ULONG m_refCount;
};

class MyDirect3DDevice9 : public IDirect3DDevice9 {
public:
    MyDirect3DDevice9(IDirect3DDevice9* real) : m_real(real), m_refCount(1) {}

    STDMETHOD(QueryInterface)(REFIID riid, void** ppvObj) { return m_real->QueryInterface(riid, ppvObj); }
    STDMETHOD_(ULONG, AddRef)() { return ++m_refCount; }
    STDMETHOD_(ULONG, Release)() {
        ULONG ref = --m_refCount;
        if (ref == 0) {
            m_real->Release();
            delete this;
        }
        return ref;
    }
    STDMETHOD(TestCooperativeLevel)() { return m_real->TestCooperativeLevel(); }
    STDMETHOD_(UINT, GetAvailableTextureMem)() { return m_real->GetAvailableTextureMem(); }
    STDMETHOD(EvictManagedResources)() { return m_real->EvictManagedResources(); }
    STDMETHOD(GetDirect3D)(IDirect3D9** ppD3D9) { return m_real->GetDirect3D(ppD3D9); }
    STDMETHOD(GetDeviceCaps)(D3DCAPS9* pCaps) { return m_real->GetDeviceCaps(pCaps); }
    STDMETHOD(GetDisplayMode)(UINT iSwapChain, D3DDISPLAYMODE* pMode) { return m_real->GetDisplayMode(iSwapChain, pMode); }
    STDMETHOD(GetCreationParameters)(D3DDEVICE_CREATION_PARAMETERS *pParameters) { return m_real->GetCreationParameters(pParameters); }
    STDMETHOD(SetCursorProperties)(UINT XHotSpot, UINT YHotSpot, IDirect3DSurface9* pCursorBitmap) { return m_real->SetCursorProperties(XHotSpot, YHotSpot, pCursorBitmap); }
    STDMETHOD_(void, SetCursorPosition)(int X, int Y, DWORD Flags) { m_real->SetCursorPosition(X, Y, Flags); }
    STDMETHOD_(BOOL, ShowCursor)(BOOL bShow) { return m_real->ShowCursor(bShow); }
    STDMETHOD(CreateAdditionalSwapChain)(D3DPRESENT_PARAMETERS* pPresentationParameters, IDirect3DSwapChain9** pSwapChain) { return m_real->CreateAdditionalSwapChain(pPresentationParameters, pSwapChain); }
    STDMETHOD(GetSwapChain)(UINT iSwapChain, IDirect3DSwapChain9** pSwapChain) { return m_real->GetSwapChain(iSwapChain, pSwapChain); }
    STDMETHOD_(UINT, GetNumberOfSwapChains)() { return m_real->GetNumberOfSwapChains(); }
    STDMETHOD(Reset)(D3DPRESENT_PARAMETERS* pPresentationParameters) { return m_real->Reset(pPresentationParameters); }
    STDMETHOD(Present)(CONST RECT* pSourceRect, CONST RECT* pDestRect, HWND hDestWindowOverride, CONST RGNDATA* pDirtyRegion) { return m_real->Present(pSourceRect, pDestRect, hDestWindowOverride, pDirtyRegion); }
    STDMETHOD(GetBackBuffer)(UINT iSwapChain, UINT iBackBuffer, D3DBACKBUFFER_TYPE Type, IDirect3DSurface9** ppBackBuffer) { return m_real->GetBackBuffer(iSwapChain, iBackBuffer, Type, ppBackBuffer); }
    STDMETHOD(GetDepthStencilSurface)(IDirect3DSurface9** ppZStencilSurface) { return m_real->GetDepthStencilSurface(ppZStencilSurface); }
    STDMETHOD(SetRenderTarget)(DWORD RenderTargetIndex, IDirect3DSurface9* pRenderTarget) { return m_real->SetRenderTarget(RenderTargetIndex, pRenderTarget); }
    STDMETHOD(GetRenderTarget)(DWORD RenderTargetIndex, IDirect3DSurface9** ppRenderTarget) { return m_real->GetRenderTarget(RenderTargetIndex, ppRenderTarget); }
    STDMETHOD(SetDepthStencilSurface)(IDirect3DSurface9* pNewZStencil) { return m_real->SetDepthStencilSurface(pNewZStencil); }
    STDMETHOD(CreateTexture)(UINT Width, UINT Height, UINT Levels, DWORD Usage, D3DFORMAT Format, D3DPOOL Pool, IDirect3DTexture9** ppTexture, HANDLE* pSharedHandle) {
        HRESULT hr = m_real->CreateTexture(Width, Height, Levels, Usage, Format, Pool, ppTexture, pSharedHandle);
        if (SUCCEEDED(hr) && ppTexture && *ppTexture) {
            MyDirect3DTexture9* wrapped = new MyDirect3DTexture9(*ppTexture, Width, Height, Format);
            *ppTexture = wrapped;
        }
        return hr;
    }
    STDMETHOD(CreateVolumeTexture)(UINT Width, UINT Height, UINT Depth, UINT Levels, DWORD Usage, D3DFORMAT Format, D3DPOOL Pool, IDirect3DVolumeTexture9** ppVolumeTexture, HANDLE* pSharedHandle) { return m_real->CreateVolumeTexture(Width, Height, Depth, Levels, Usage, Format, Pool, ppVolumeTexture, pSharedHandle); }
    STDMETHOD(CreateCubeTexture)(UINT EdgeLength, UINT Levels, DWORD Usage, D3DFORMAT Format, D3DPOOL Pool, IDirect3DCubeTexture9** ppCubeTexture, HANDLE* pSharedHandle) { return m_real->CreateCubeTexture(EdgeLength, Levels, Usage, Format, Pool, ppCubeTexture, pSharedHandle); }
    STDMETHOD(CreateVertexBuffer)(UINT Length, DWORD Usage, DWORD FVF, D3DPOOL Pool, IDirect3DVertexBuffer9** ppVertexBuffer, HANDLE* pSharedHandle) { return m_real->CreateVertexBuffer(Length, Usage, FVF, Pool, ppVertexBuffer, pSharedHandle); }
    STDMETHOD(CreateIndexBuffer)(UINT Length, DWORD Usage, D3DFORMAT Format, D3DPOOL Pool, IDirect3DIndexBuffer9** ppIndexBuffer, HANDLE* pSharedHandle) { return m_real->CreateIndexBuffer(Length, Usage, Format, Pool, ppIndexBuffer, pSharedHandle); }
    STDMETHOD(CreateRenderTarget)(UINT Width, UINT Height, D3DFORMAT Format, D3DMULTISAMPLE_TYPE MultiSample, DWORD MultisampleQuality, BOOL Lockable, IDirect3DSurface9** ppSurface, HANDLE* pSharedHandle) { return m_real->CreateRenderTarget(Width, Height, Format, MultiSample, MultisampleQuality, Lockable, ppSurface, pSharedHandle); }
    STDMETHOD(CreateDepthStencilSurface)(UINT Width, UINT Height, D3DFORMAT Format, D3DMULTISAMPLE_TYPE MultiSample, DWORD MultisampleQuality, BOOL Discard, IDirect3DSurface9** ppSurface, HANDLE* pSharedHandle) { return m_real->CreateDepthStencilSurface(Width, Height, Format, MultiSample, MultisampleQuality, Discard, ppSurface, pSharedHandle); }
    STDMETHOD(UpdateSurface)(IDirect3DSurface9* pSourceSurface, const RECT* pSourceRect, IDirect3DSurface9* pDestSurface, const POINT* pDestPoint) { return m_real->UpdateSurface(pSourceSurface, pSourceRect, pDestSurface, pDestPoint); }
    STDMETHOD(UpdateTexture)(IDirect3DBaseTexture9* pSourceTexture, IDirect3DBaseTexture9* pDestinationTexture) { return m_real->UpdateTexture(pSourceTexture, pDestinationTexture); }
    STDMETHOD(GetRenderTargetData)(IDirect3DSurface9* pRenderTarget, IDirect3DSurface9* pDestSurface) { return m_real->GetRenderTargetData(pRenderTarget, pDestSurface); }
    STDMETHOD(GetFrontBufferData)(UINT iSwapChain, IDirect3DSurface9* pDestSurface) { return m_real->GetFrontBufferData(iSwapChain, pDestSurface); }
    STDMETHOD(StretchRect)(IDirect3DSurface9* pSourceSurface, const RECT* pSourceRect, IDirect3DSurface9* pDestSurface, const RECT* pDestRect, D3DTEXTUREFILTERTYPE Filter) { return m_real->StretchRect(pSourceSurface, pSourceRect, pDestSurface, pDestRect, Filter); }
    STDMETHOD(ColorFill)(IDirect3DSurface9* pSurface, const RECT* pRect, D3DCOLOR Color) { return m_real->ColorFill(pSurface, pRect, Color); }
    STDMETHOD(CreateOffscreenPlainSurface)(UINT Width, UINT Height, D3DFORMAT Format, D3DPOOL Pool, IDirect3DSurface9** ppSurface, HANDLE* pSharedHandle) { return m_real->CreateOffscreenPlainSurface(Width, Height, Format, Pool, ppSurface, pSharedHandle); }
    STDMETHOD(GetRasterStatus)(UINT iSwapChain, D3DRASTER_STATUS* pRasterStatus) { return m_real->GetRasterStatus(iSwapChain, pRasterStatus); }
    STDMETHOD(SetDialogBoxMode)(BOOL bEnableDialogs) { return m_real->SetDialogBoxMode(bEnableDialogs); }
    STDMETHOD_(void, SetGammaRamp)(UINT iSwapChain, DWORD Flags, const D3DGAMMARAMP* pRamp) { m_real->SetGammaRamp(iSwapChain, Flags, pRamp); }
    STDMETHOD_(void, GetGammaRamp)(UINT iSwapChain, D3DGAMMARAMP* pRamp) { m_real->GetGammaRamp(iSwapChain, pRamp); }
    STDMETHOD(SetTransform)(D3DTRANSFORMSTATETYPE State, const D3DMATRIX* pMatrix) { return m_real->SetTransform(State, pMatrix); }
    STDMETHOD(GetTransform)(D3DTRANSFORMSTATETYPE State, D3DMATRIX* pMatrix) { return m_real->GetTransform(State, pMatrix); }
    STDMETHOD(MultiplyTransform)(D3DTRANSFORMSTATETYPE State, const D3DMATRIX* pMatrix) { return m_real->MultiplyTransform(State, pMatrix); }
    STDMETHOD(SetViewport)(const D3DVIEWPORT9* pViewport) { return m_real->SetViewport(pViewport); }
    STDMETHOD(GetViewport)(D3DVIEWPORT9* pViewport) { return m_real->GetViewport(pViewport); }
    STDMETHOD(SetMaterial)(const D3DMATERIAL9* pMaterial) { return m_real->SetMaterial(pMaterial); }
    STDMETHOD(GetMaterial)(D3DMATERIAL9* pMaterial) { return m_real->GetMaterial(pMaterial); }
    STDMETHOD(SetLight)(DWORD Index, const D3DLIGHT9* pLight) { return m_real->SetLight(Index, pLight); }
    STDMETHOD(GetLight)(DWORD Index, D3DLIGHT9* pLight) { return m_real->GetLight(Index, pLight); }
    STDMETHOD(LightEnable)(DWORD Index, BOOL Enable) { return m_real->LightEnable(Index, Enable); }
    STDMETHOD(GetLightEnable)(DWORD Index, BOOL* pEnable) { return m_real->GetLightEnable(Index, pEnable); }
    STDMETHOD(SetClipPlane)(DWORD Index, const float* pPlane) { return m_real->SetClipPlane(Index, pPlane); }
    STDMETHOD(GetClipPlane)(DWORD Index, float* pPlane) { return m_real->GetClipPlane(Index, pPlane); }
    STDMETHOD(SetRenderState)(D3DRENDERSTATETYPE State, DWORD Value) { return m_real->SetRenderState(State, Value); }
    STDMETHOD(GetRenderState)(D3DRENDERSTATETYPE State, DWORD* pValue) { return m_real->GetRenderState(State, pValue); }
    STDMETHOD(CreateStateBlock)(D3DSTATEBLOCKTYPE Type, IDirect3DStateBlock9** ppSB) { return m_real->CreateStateBlock(Type, ppSB); }
    STDMETHOD(BeginStateBlock)() { return m_real->BeginStateBlock(); }
    STDMETHOD(EndStateBlock)(IDirect3DStateBlock9** ppSB) { return m_real->EndStateBlock(ppSB); }
    STDMETHOD(SetClipStatus)(const D3DCLIPSTATUS9* pClipStatus) { return m_real->SetClipStatus(pClipStatus); }
    STDMETHOD(GetClipStatus)(D3DCLIPSTATUS9* pClipStatus) { return m_real->GetClipStatus(pClipStatus); }
    STDMETHOD(GetTexture)(DWORD Stage, IDirect3DBaseTexture9** ppTexture) { return m_real->GetTexture(Stage, ppTexture); }
    STDMETHOD(SetTexture)(DWORD Stage, IDirect3DBaseTexture9* pTexture) { return m_real->SetTexture(Stage, pTexture); }
    STDMETHOD(GetTextureStageState)(DWORD Stage, D3DTEXTURESTAGESTATETYPE Type, DWORD* pValue) { return m_real->GetTextureStageState(Stage, Type, pValue); }
    STDMETHOD(SetTextureStageState)(DWORD Stage, D3DTEXTURESTAGESTATETYPE Type, DWORD Value) { return m_real->SetTextureStageState(Stage, Type, Value); }
    STDMETHOD(GetSamplerState)(DWORD Sampler, D3DSAMPLERSTATETYPE Type, DWORD* pValue) { return m_real->GetSamplerState(Sampler, Type, pValue); }
    STDMETHOD(SetSamplerState)(DWORD Sampler, D3DSAMPLERSTATETYPE Type, DWORD Value) { return m_real->SetSamplerState(Sampler, Type, Value); }
    STDMETHOD(ValidateDevice)(DWORD* pNumPasses) { return m_real->ValidateDevice(pNumPasses); }
    STDMETHOD(SetPaletteEntries)(UINT PaletteNumber, const PALETTEENTRY* pEntries) { return m_real->SetPaletteEntries(PaletteNumber, pEntries); }
    STDMETHOD(GetPaletteEntries)(UINT PaletteNumber, PALETTEENTRY* pEntries) { return m_real->GetPaletteEntries(PaletteNumber, pEntries); }
    STDMETHOD(SetCurrentTexturePalette)(UINT PaletteNumber) { return m_real->SetCurrentTexturePalette(PaletteNumber); }
    STDMETHOD(GetCurrentTexturePalette)(UINT* PaletteNumber) { return m_real->GetCurrentTexturePalette(PaletteNumber); }
    STDMETHOD(SetScissorRect)(const RECT* pRect) { return m_real->SetScissorRect(pRect); }
    STDMETHOD(GetScissorRect)(RECT* pRect) { return m_real->GetScissorRect(pRect); }
    STDMETHOD(SetSoftwareVertexProcessing)(BOOL bSoftware) { return m_real->SetSoftwareVertexProcessing(bSoftware); }
    STDMETHOD_(BOOL, GetSoftwareVertexProcessing)() { return m_real->GetSoftwareVertexProcessing(); }
    STDMETHOD(SetNPatchMode)(float nSegments) { return m_real->SetNPatchMode(nSegments); }
    STDMETHOD_(float, GetNPatchMode)() { return m_real->GetNPatchMode(); }
    STDMETHOD(DrawPrimitive)(D3DPRIMITIVETYPE PrimitiveType, UINT StartVertex, UINT PrimitiveCount) { return m_real->DrawPrimitive(PrimitiveType, StartVertex, PrimitiveCount); }
    STDMETHOD(DrawIndexedPrimitive)(D3DPRIMITIVETYPE PrimitiveType, INT BaseVertexIndex, UINT MinVertexIndex, UINT NumVertices, UINT StartIndex, UINT PrimitiveCount) { return m_real->DrawIndexedPrimitive(PrimitiveType, BaseVertexIndex, MinVertexIndex, NumVertices, StartIndex, PrimitiveCount); }
    STDMETHOD(DrawPrimitiveUP)(D3DPRIMITIVETYPE PrimitiveType, UINT PrimitiveCount, const void* pVertexStreamZeroData, UINT VertexStreamZeroStride) { return m_real->DrawPrimitiveUP(PrimitiveType, PrimitiveCount, pVertexStreamZeroData, VertexStreamZeroStride); }
    STDMETHOD(DrawIndexedPrimitiveUP)(D3DPRIMITIVETYPE PrimitiveType, UINT MinVertexIndex, UINT NumVertices, UINT PrimitiveCount, const void* pIndexData, D3DFORMAT IndexDataFormat, const void* pVertexStreamZeroData, UINT VertexStreamZeroStride) { return m_real->DrawIndexedPrimitiveUP(PrimitiveType, MinVertexIndex, NumVertices, PrimitiveCount, pIndexData, IndexDataFormat, pVertexStreamZeroData, VertexStreamZeroStride); }
    STDMETHOD(ProcessVertices)(UINT SrcStartIndex, UINT DestIndex, UINT VertexCount, IDirect3DVertexBuffer9* pDestBuffer, IDirect3DVertexDeclaration9* pVertexDecl, DWORD Flags) { return m_real->ProcessVertices(SrcStartIndex, DestIndex, VertexCount, pDestBuffer, pVertexDecl, Flags); }
    STDMETHOD(CreateVertexDeclaration)(const D3DVERTEXELEMENT9* pVertexElements, IDirect3DVertexDeclaration9** ppDecl) { return m_real->CreateVertexDeclaration(pVertexElements, ppDecl); }
    STDMETHOD(SetVertexDeclaration)(IDirect3DVertexDeclaration9* pDecl) { return m_real->SetVertexDeclaration(pDecl); }
    STDMETHOD(GetVertexDeclaration)(IDirect3DVertexDeclaration9** ppDecl) { return m_real->GetVertexDeclaration(ppDecl); }
    STDMETHOD(SetFVF)(DWORD FVF) { return m_real->SetFVF(FVF); }
    STDMETHOD(GetFVF)(DWORD* pFVF) { return m_real->GetFVF(pFVF); }
    STDMETHOD(CreateVertexShader)(const DWORD* pFunction, IDirect3DVertexShader9** ppShader) { return m_real->CreateVertexShader(pFunction, ppShader); }
    STDMETHOD(SetVertexShader)(IDirect3DVertexShader9* pShader) { return m_real->SetVertexShader(pShader); }
    STDMETHOD(GetVertexShader)(IDirect3DVertexShader9** ppShader) { return m_real->GetVertexShader(ppShader); }
    STDMETHOD(SetVertexShaderConstantF)(UINT StartRegister, const float* pConstantData, UINT Vector4fCount) { return m_real->SetVertexShaderConstantF(StartRegister, pConstantData, Vector4fCount); }
    STDMETHOD(GetVertexShaderConstantF)(UINT StartRegister, float* pConstantData, UINT Vector4fCount) { return m_real->GetVertexShaderConstantF(StartRegister, pConstantData, Vector4fCount); }
    STDMETHOD(SetVertexShaderConstantI)(UINT StartRegister, const int* pConstantData, UINT Vector4iCount) { return m_real->SetVertexShaderConstantI(StartRegister, pConstantData, Vector4iCount); }
    STDMETHOD(GetVertexShaderConstantI)(UINT StartRegister, int* pConstantData, UINT Vector4iCount) { return m_real->GetVertexShaderConstantI(StartRegister, pConstantData, Vector4iCount); }
    STDMETHOD(SetVertexShaderConstantB)(UINT StartRegister, const BOOL* pConstantData, UINT BoolCount) { return m_real->SetVertexShaderConstantB(StartRegister, pConstantData, BoolCount); }
    STDMETHOD(GetVertexShaderConstantB)(UINT StartRegister, BOOL* pConstantData, UINT BoolCount) { return m_real->GetVertexShaderConstantB(StartRegister, pConstantData, BoolCount); }
    STDMETHOD(SetStreamSource)(UINT StreamNumber, IDirect3DVertexBuffer9* pStreamData, UINT OffsetInBytes, UINT Stride) { return m_real->SetStreamSource(StreamNumber, pStreamData, OffsetInBytes, Stride); }
    STDMETHOD(GetStreamSource)(UINT StreamNumber, IDirect3DVertexBuffer9** ppStreamData, UINT* OffsetInBytes, UINT* pStride) { return m_real->GetStreamSource(StreamNumber, ppStreamData, OffsetInBytes, pStride); }
    STDMETHOD(SetStreamSourceFreq)(UINT StreamNumber, UINT FrequencyParameter) { return m_real->SetStreamSourceFreq(StreamNumber, FrequencyParameter); }
    STDMETHOD(GetStreamSourceFreq)(UINT StreamNumber, UINT* pFrequencyParameter) { return m_real->GetStreamSourceFreq(StreamNumber, pFrequencyParameter); }
    STDMETHOD(SetIndices)(IDirect3DIndexBuffer9* pIndexData) { return m_real->SetIndices(pIndexData); }
    STDMETHOD(GetIndices)(IDirect3DIndexBuffer9** ppIndexData) { return m_real->GetIndices(ppIndexData); }
    STDMETHOD(CreatePixelShader)(const DWORD* pFunction, IDirect3DPixelShader9** ppShader) { return m_real->CreatePixelShader(pFunction, ppShader); }
    STDMETHOD(SetPixelShader)(IDirect3DPixelShader9* pShader) { return m_real->SetPixelShader(pShader); }
    STDMETHOD(GetPixelShader)(IDirect3DPixelShader9** ppShader) { return m_real->GetPixelShader(ppShader); }
    STDMETHOD(SetPixelShaderConstantF)(UINT StartRegister, const float* pConstantData, UINT Vector4fCount) { return m_real->SetPixelShaderConstantF(StartRegister, pConstantData, Vector4fCount); }
    STDMETHOD(GetPixelShaderConstantF)(UINT StartRegister, float* pConstantData, UINT Vector4fCount) { return m_real->GetPixelShaderConstantF(StartRegister, pConstantData, Vector4fCount); }
    STDMETHOD(SetPixelShaderConstantI)(UINT StartRegister, const int* pConstantData, UINT Vector4iCount) { return m_real->SetPixelShaderConstantI(StartRegister, pConstantData, Vector4iCount); }
    STDMETHOD(GetPixelShaderConstantI)(UINT StartRegister, int* pConstantData, UINT Vector4iCount) { return m_real->GetPixelShaderConstantI(StartRegister, pConstantData, Vector4iCount); }
    STDMETHOD(SetPixelShaderConstantB)(UINT StartRegister, const BOOL* pConstantData, UINT BoolCount) { return m_real->SetPixelShaderConstantB(StartRegister, pConstantData, BoolCount); }
    STDMETHOD(GetPixelShaderConstantB)(UINT StartRegister, BOOL* pConstantData, UINT BoolCount) { return m_real->GetPixelShaderConstantB(StartRegister, pConstantData, BoolCount); }
    STDMETHOD(DrawRectPatch)(UINT Handle, const float* pNumSegs, const D3DRECTPATCH_INFO* pRectPatchInfo) { return m_real->DrawRectPatch(Handle, pNumSegs, pRectPatchInfo); }
    STDMETHOD(DrawTriPatch)(UINT Handle, const float* pNumSegs, const D3DTRIPATCH_INFO* pTriPatchInfo) { return m_real->DrawTriPatch(Handle, pNumSegs, pTriPatchInfo); }
    STDMETHOD(DeletePatch)(UINT Handle) { return m_real->DeletePatch(Handle); }
    STDMETHOD(CreateQuery)(D3DQUERYTYPE Type, IDirect3DQuery9** ppQuery) { return m_real->CreateQuery(Type, ppQuery); }
    STDMETHOD(BeginScene)() { return m_real->BeginScene(); }
    STDMETHOD(EndScene)() { return m_real->EndScene(); }
    STDMETHOD(Clear)(DWORD Count, const D3DRECT* pRects, DWORD Flags, D3DCOLOR Color, float Z, DWORD Stencil) { return m_real->Clear(Count, pRects, Flags, Color, Z, Stencil); }

private:
    IDirect3DDevice9* m_real;
    ULONG m_refCount;
};

class MyDirect3D9 : public IDirect3D9 {
public:
    MyDirect3D9(IDirect3D9* real) : m_real(real), m_refCount(1) {}

    STDMETHOD(QueryInterface)(REFIID riid, void** ppvObj) { return m_real->QueryInterface(riid, ppvObj); }
    STDMETHOD_(ULONG, AddRef)() { return ++m_refCount; }
    STDMETHOD_(ULONG, Release)() {
        ULONG ref = --m_refCount;
        if (ref == 0) {
            m_real->Release();
            delete this;
        }
        return ref;
    }
    STDMETHOD(RegisterSoftwareDevice)(void* pInitializeFunction) { return m_real->RegisterSoftwareDevice(pInitializeFunction); }
    STDMETHOD_(UINT, GetAdapterCount)() { return m_real->GetAdapterCount(); }
    STDMETHOD(GetAdapterIdentifier)(UINT Adapter, DWORD Flags, D3DADAPTER_IDENTIFIER9* pIdentifier) { return m_real->GetAdapterIdentifier(Adapter, Flags, pIdentifier); }
    STDMETHOD_(UINT, GetAdapterModeCount)(UINT Adapter, D3DFORMAT Format) { return m_real->GetAdapterModeCount(Adapter, Format); }
    STDMETHOD(EnumAdapterModes)(UINT Adapter, D3DFORMAT Format, UINT Mode, D3DDISPLAYMODE* pMode) { return m_real->EnumAdapterModes(Adapter, Format, Mode, pMode); }
    STDMETHOD(GetAdapterDisplayMode)(UINT Adapter, D3DDISPLAYMODE* pMode) { return m_real->GetAdapterDisplayMode(Adapter, pMode); }
    STDMETHOD(CheckDeviceType)(UINT Adapter, D3DDEVTYPE DevType, D3DFORMAT AdapterFormat, D3DFORMAT BackBufferFormat, BOOL bWindowed) { return m_real->CheckDeviceType(Adapter, DevType, AdapterFormat, BackBufferFormat, bWindowed); }
    STDMETHOD(CheckDeviceFormat)(UINT Adapter, D3DDEVTYPE DeviceType, D3DFORMAT AdapterFormat, DWORD Usage, D3DRESOURCETYPE RType, D3DFORMAT CheckFormat) { return m_real->CheckDeviceFormat(Adapter, DeviceType, AdapterFormat, Usage, RType, CheckFormat); }
    STDMETHOD(CheckDeviceMultiSampleType)(UINT Adapter, D3DDEVTYPE DeviceType, D3DFORMAT SurfaceFormat, BOOL Windowed, D3DMULTISAMPLE_TYPE MultiSampleType, DWORD* pQualityLevels) { return m_real->CheckDeviceMultiSampleType(Adapter, DeviceType, SurfaceFormat, Windowed, MultiSampleType, pQualityLevels); }
    STDMETHOD(CheckDepthStencilMatch)(UINT Adapter, D3DDEVTYPE DeviceType, D3DFORMAT AdapterFormat, D3DFORMAT RenderTargetFormat, D3DFORMAT DepthStencilFormat) { return m_real->CheckDepthStencilMatch(Adapter, DeviceType, AdapterFormat, RenderTargetFormat, DepthStencilFormat); }
    STDMETHOD(CheckDeviceFormatConversion)(UINT Adapter, D3DDEVTYPE DeviceType, D3DFORMAT SourceFormat, D3DFORMAT TargetFormat) { return m_real->CheckDeviceFormatConversion(Adapter, DeviceType, SourceFormat, TargetFormat); }
    STDMETHOD(GetDeviceCaps)(UINT Adapter, D3DDEVTYPE DeviceType, D3DCAPS9* pCaps) { return m_real->GetDeviceCaps(Adapter, DeviceType, pCaps); }
    STDMETHOD_(HMONITOR, GetAdapterMonitor)(UINT Adapter) { return m_real->GetAdapterMonitor(Adapter); }
    STDMETHOD(CreateDevice)(UINT Adapter, D3DDEVTYPE DeviceType, HWND hFocusWindow, DWORD BehaviorFlags, D3DPRESENT_PARAMETERS* pPresentationParameters, IDirect3DDevice9** ppReturnedDeviceInterface) {
        HRESULT hr = m_real->CreateDevice(Adapter, DeviceType, hFocusWindow, BehaviorFlags, pPresentationParameters, ppReturnedDeviceInterface);
        if (SUCCEEDED(hr) && ppReturnedDeviceInterface && *ppReturnedDeviceInterface) {
            *ppReturnedDeviceInterface = new MyDirect3DDevice9(*ppReturnedDeviceInterface);
        }
        return hr;
    }

private:
    IDirect3D9* m_real;
    ULONG m_refCount;
};

BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved) {
    switch (ul_reason_for_call) {
    case DLL_PROCESS_ATTACH:
        {
            FILE* f = fopen("D:\\APBReloaded\\work\\login_swap\\mod\\captures\\apb_hash_hook_loaded.txt", "w");
            if (f) {
                fprintf(f, "APBHashHook loaded\n");
                fclose(f);
            }
            g_LogFile = fopen("D:\\APBReloaded\\work\\login_swap\\mod\\captures\\apb_hash_hook.log", "w");
            LogMessage("APBHashHook DllMain attach\n");
        }
        break;
    case DLL_PROCESS_DETACH:
        if (g_LogFile) {
            fclose(g_LogFile);
            g_LogFile = nullptr;
        }
        break;
    }
    return TRUE;
}

extern "C" __declspec(dllexport) IDirect3D9* WINAPI Direct3DCreate9(UINT SDKVersion) {
    if (!g_RealD3D9) {
        g_RealD3D9 = LoadLibraryA("C:\\Windows\\System32\\d3d9.dll");
        if (!g_RealD3D9) {
            LogMessage("Failed to load real d3d9.dll\n");
            return nullptr;
        }
    }
    Direct3DCreate9_t realCreate = (Direct3DCreate9_t)GetProcAddress(g_RealD3D9, "Direct3DCreate9");
    if (!realCreate) {
        LogMessage("Failed to get Direct3DCreate9\n");
        return nullptr;
    }
    IDirect3D9* real = realCreate(SDKVersion);
    if (!real) {
        LogMessage("real Direct3DCreate9 failed\n");
        return nullptr;
    }
    LogMessage("APBHashHook loaded\n");
    return new MyDirect3D9(real);
}

} // namespace APBHashHook
