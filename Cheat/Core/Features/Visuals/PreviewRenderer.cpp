#define NOMINMAX
#include "PreviewRenderer.h"
#include "ModelLoader.h"
#include "../../Graphics.h"
#include <stb_image.h>
#include <d3dcompiler.h>
#include <DirectXMath.h>
#include <cmath>
#include <algorithm>
#include <vector>

#pragma comment(lib, "d3dcompiler.lib")

using namespace DirectX;

static constexpr float k_BG[4] = { 15.f/255.f, 14.f/255.f, 14.f/255.f, 1.0f };

static const char s_VS[] = R"(
cbuffer CB : register(b0) { float4x4 g_MVP; };
struct VS_In  { float3 pos : POSITION; float2 uv : TEXCOORD; };
struct VS_Out { float4 pos : SV_POSITION; float2 uv : TEXCOORD; };
VS_Out main(VS_In v) {
    VS_Out o;
    o.pos = mul(float4(v.pos, 1.0), g_MVP);
    o.uv  = v.uv;
    return o;
}
)";
static const char s_PS[] = R"(
Texture2D    g_Tex  : register(t0);
SamplerState g_Sam  : register(s0);
struct VS_Out { float4 pos : SV_POSITION; float2 uv : TEXCOORD; };
float4 main(VS_Out v) : SV_Target { return g_Tex.Sample(g_Sam, v.uv); }
)";

namespace Cheat::Core {

static bool CompileShader(const char* src, const char* entry,
                           const char* profile, ID3DBlob** out)
{
    ID3DBlob* err = nullptr;
    HRESULT hr = D3DCompile(src, strlen(src), nullptr, nullptr, nullptr,
                             entry, profile,
                             D3DCOMPILE_OPTIMIZATION_LEVEL3, 0, out, &err);
    if (err) err->Release();
    return SUCCEEDED(hr);
}

bool PreviewRenderer::Initialize(ID3D11Device* device, ID3D11DeviceContext* ctx,
                                  const std::string& obj_path,
                                  const std::string& tex_path,
                                  unsigned int rt_width, unsigned int rt_height)
{
    m_Device = device;
    m_Ctx    = ctx;
    m_Width  = rt_width;
    m_Height = rt_height;

    if (!CreateRenderTarget(rt_width, rt_height)) return false;
    if (!CreateShaders())                          return false;
    if (!UploadModel(obj_path))                   return false;
    if (!LoadTexture(tex_path))                   return false;

    D3D11_RASTERIZER_DESC rd{};
    rd.FillMode        = D3D11_FILL_SOLID;
    rd.CullMode        = D3D11_CULL_NONE;
    rd.DepthClipEnable = TRUE;
    m_Device->CreateRasterizerState(&rd, &m_RS);

    D3D11_DEPTH_STENCIL_DESC dd{};
    dd.DepthEnable    = TRUE;
    dd.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL;
    dd.DepthFunc      = D3D11_COMPARISON_LESS;
    m_Device->CreateDepthStencilState(&dd, &m_DSState);

    D3D11_BLEND_DESC bd{};
    bd.RenderTarget[0].BlendEnable           = FALSE;
    bd.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
    m_Device->CreateBlendState(&bd, &m_BlendState);

    m_Ready = true;
    return true;
}

bool PreviewRenderer::CreateRenderTarget(unsigned int w, unsigned int h)
{
    D3D11_TEXTURE2D_DESC td{};
    td.Width = w; td.Height = h; td.MipLevels = 1; td.ArraySize = 1;
    td.Format = DXGI_FORMAT_R8G8B8A8_UNORM; td.SampleDesc.Count = 1;
    td.Usage = D3D11_USAGE_DEFAULT;
    td.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
    if (FAILED(m_Device->CreateTexture2D(&td, nullptr, &m_RTTex))) return false;
    if (FAILED(m_Device->CreateRenderTargetView(m_RTTex, nullptr, &m_RTV)))   return false;
    if (FAILED(m_Device->CreateShaderResourceView(m_RTTex, nullptr, &m_SRV))) return false;

    D3D11_TEXTURE2D_DESC dd{};
    dd.Width = w; dd.Height = h; dd.MipLevels = 1; dd.ArraySize = 1;
    dd.Format = DXGI_FORMAT_D32_FLOAT; dd.SampleDesc.Count = 1;
    dd.Usage = D3D11_USAGE_DEFAULT; dd.BindFlags = D3D11_BIND_DEPTH_STENCIL;
    if (FAILED(m_Device->CreateTexture2D(&dd, nullptr, &m_DepthTex))) return false;
    return SUCCEEDED(m_Device->CreateDepthStencilView(m_DepthTex, nullptr, &m_DSV));
}

bool PreviewRenderer::CreateShaders()
{
    ID3DBlob* vsBlob = nullptr; ID3DBlob* psBlob = nullptr;
    if (!CompileShader(s_VS, "main", "vs_4_0", &vsBlob)) return false;
    if (!CompileShader(s_PS, "main", "ps_4_0", &psBlob)) { vsBlob->Release(); return false; }

    HRESULT hr = m_Device->CreateVertexShader(vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(), nullptr, &m_VS);
    if (SUCCEEDED(hr)) hr = m_Device->CreatePixelShader(psBlob->GetBufferPointer(), psBlob->GetBufferSize(), nullptr, &m_PS);

    D3D11_INPUT_ELEMENT_DESC layout[] = {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0,  0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,    0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0 },
    };
    if (SUCCEEDED(hr))
        hr = m_Device->CreateInputLayout(layout, 2, vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(), &m_IL);

    vsBlob->Release(); psBlob->Release();
    if (FAILED(hr)) return false;

    D3D11_BUFFER_DESC cbd{};
    cbd.ByteWidth = 64; cbd.Usage = D3D11_USAGE_DYNAMIC;
    cbd.BindFlags = D3D11_BIND_CONSTANT_BUFFER; cbd.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    return SUCCEEDED(m_Device->CreateBuffer(&cbd, nullptr, &m_CB));
}

bool PreviewRenderer::UploadModel(const std::string& obj_path)
{
    std::vector<ModelVertex> verts;
    if (!LoadOBJ(obj_path, verts, m_ModelScale, m_ModelCenter, m_RawMin, m_RawMax, &m_UniquePos)) return false;
    m_VertCount = (unsigned int)verts.size();

    D3D11_BUFFER_DESC bd{};
    bd.ByteWidth = (UINT)(sizeof(ModelVertex) * verts.size());
    bd.Usage = D3D11_USAGE_IMMUTABLE; bd.BindFlags = D3D11_BIND_VERTEX_BUFFER;

    D3D11_SUBRESOURCE_DATA sd{}; sd.pSysMem = verts.data();
    if (FAILED(m_Device->CreateBuffer(&bd, &sd, &m_VB))) return false;

    auto norm2raw = [&](float nx, float ny, float nz, float out[3]) {
        out[0] = nx / m_ModelScale + m_ModelCenter[0];
        out[1] = ny / m_ModelScale + m_ModelCenter[1];
        out[2] = nz / m_ModelScale + m_ModelCenter[2];
    };

    norm2raw( 0.0000f,  0.4200f,  0.0000f, m_Joints[ 0]);
    norm2raw( 0.0000f,  0.3200f,  0.0000f, m_Joints[ 1]);
    norm2raw(-0.1290f,  0.2918f,  0.0176f, m_Joints[ 2]);
    norm2raw( 0.1176f,  0.2949f, -0.0125f, m_Joints[ 3]);
    norm2raw(-0.1635f,  0.0949f,  0.0027f, m_Joints[ 4]);
    norm2raw( 0.1527f,  0.1253f,  0.0092f, m_Joints[ 5]);
    norm2raw(-0.1571f, -0.0250f,  0.0439f, m_Joints[ 6]);
    norm2raw( 0.1547f, -0.0284f,  0.0241f, m_Joints[ 7]);
    norm2raw(-0.0093f, -0.0400f, -0.0021f, m_Joints[ 8]);
    norm2raw(-0.0590f, -0.2736f,  0.0146f, m_Joints[ 9]);
    norm2raw( 0.0713f, -0.2797f, -0.0065f, m_Joints[10]);
    norm2raw(-0.0537f, -0.4546f,  0.0039f, m_Joints[11]);
    norm2raw( 0.0625f, -0.4686f, -0.0057f, m_Joints[12]);

    return true;
}

bool PreviewRenderer::LoadTexture(const std::string& tex_path)
{
    int w, h, ch;
    unsigned char* data = stbi_load(tex_path.c_str(), &w, &h, &ch, 4);
    if (!data) return false;

    D3D11_TEXTURE2D_DESC td{};
    td.Width = (UINT)w; td.Height = (UINT)h; td.MipLevels = 1; td.ArraySize = 1;
    td.Format = DXGI_FORMAT_R8G8B8A8_UNORM; td.SampleDesc.Count = 1;
    td.Usage = D3D11_USAGE_IMMUTABLE; td.BindFlags = D3D11_BIND_SHADER_RESOURCE;

    D3D11_SUBRESOURCE_DATA sd{}; sd.pSysMem = data; sd.SysMemPitch = (UINT)(w * 4);
    HRESULT hr = m_Device->CreateTexture2D(&td, &sd, &m_TexRes);
    stbi_image_free(data);
    if (FAILED(hr)) return false;

    hr = m_Device->CreateShaderResourceView(m_TexRes, nullptr, &m_TexSRV);
    if (FAILED(hr)) return false;

    D3D11_SAMPLER_DESC smd{};
    smd.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
    smd.AddressU = smd.AddressV = smd.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;
    smd.ComparisonFunc = D3D11_COMPARISON_ALWAYS; smd.MaxLOD = D3D11_FLOAT32_MAX;
    return SUCCEEDED(m_Device->CreateSamplerState(&smd, &m_Sampler));
}

void PreviewRenderer::AddRotationDelta(float dyaw, float dpitch)
{
    m_ManualYaw   += dyaw;
    m_ManualPitch += dpitch;
    const float kMax = 1.4f;
    if (m_ManualPitch >  kMax) m_ManualPitch =  kMax;
    if (m_ManualPitch < -kMax) m_ManualPitch = -kMax;
    m_SceneDirty = true;
}

void PreviewRenderer::AddZoom(float delta)
{
    m_Zoom += delta;
    if (m_Zoom < 0.3f) m_Zoom = 0.3f;
    if (m_Zoom > 5.0f) m_Zoom = 5.0f;
    m_SceneDirty = true;
}

void PreviewRenderer::Update(float dt)
{
    if (!m_Ready) return;

    if (m_SpinPauseRemaining > 0.0f) {
        m_SpinPauseRemaining -= dt;
        if (m_SpinPauseRemaining < 0.0f) m_SpinPauseRemaining = 0.0f;
    }

    if (m_AutoSpin && m_SpinPauseRemaining <= 0.0f) {
        m_SpinAccum += dt;
        constexpr float kSpinStep = 1.0f / 30.0f;
        if (m_SpinAccum >= kSpinStep) {
            m_Angle += m_SpinAccum * 0.35f;
            m_SpinAccum = 0.0f;
            m_SceneDirty = true;
        }
    } else {
        m_SpinAccum = 0.0f;
    }

    if (!m_SceneDirty) return;
    m_SceneDirty = false;

    const float totalYaw   = m_Angle + m_ManualYaw;
    const float totalPitch = m_ManualPitch;

    const float camDist = 1.25f / m_Zoom;

    constexpr float kWidthStretch = 1.4f;
    const float sX = m_ModelScale * kWidthStretch;
    const float sY = m_ModelScale;
    const float sZ = m_ModelScale * kWidthStretch;
    XMMATRIX world = XMMatrixScaling(sX, sY, sZ)
                   * XMMatrixTranslation(-m_ModelCenter[0] * sX,
                                         -m_ModelCenter[1] * sY,
                                         -m_ModelCenter[2] * sZ)
                   * XMMatrixRotationX(totalPitch)
                   * XMMatrixRotationY(totalYaw);

    float aspect = (float)m_Width / (float)m_Height;
    XMMATRIX view = XMMatrixLookAtLH(
        XMVectorSet(0.0f, 0.0f, -camDist, 1.0f),
        XMVectorSet(0.0f, 0.0f,  0.0f,    1.0f),
        XMVectorSet(0.0f, 1.0f,  0.0f,    0.0f));
    XMMATRIX proj = XMMatrixPerspectiveFovLH(XMConvertToRadians(50.0f), aspect, 0.01f, 100.0f);

    XMMATRIX cpuMVP = world * view * proj;
    XMStoreFloat4x4(reinterpret_cast<XMFLOAT4X4*>(m_LastMVP), cpuMVP);
    m_LastMVPValid = true;

    m_CachedBoundsValid = false;
    m_CachedSilValid    = false;

    XMMATRIX gpuMVP = XMMatrixTranspose(cpuMVP);

    D3D11_MAPPED_SUBRESOURCE ms{};
    if (SUCCEEDED(m_Ctx->Map(m_CB, 0, D3D11_MAP_WRITE_DISCARD, 0, &ms))) {
        memcpy(ms.pData, &gpuMVP, 64);
        m_Ctx->Unmap(m_CB, 0);
    }

    ID3D11RenderTargetView* prevRTV = nullptr;
    ID3D11DepthStencilView* prevDSV = nullptr;
    ID3D11RasterizerState*  prevRS  = nullptr;
    ID3D11DepthStencilState* prevDS = nullptr;
    UINT prevDSRef = 0;
    float prevBF[4]{}; UINT prevSM = 0;
    ID3D11BlendState* prevBS = nullptr;
    UINT numVP = 1; D3D11_VIEWPORT prevVP{};

    m_Ctx->OMGetRenderTargets(1, &prevRTV, &prevDSV);
    m_Ctx->RSGetState(&prevRS);
    m_Ctx->OMGetDepthStencilState(&prevDS, &prevDSRef);
    m_Ctx->OMGetBlendState(&prevBS, prevBF, &prevSM);
    m_Ctx->RSGetViewports(&numVP, &prevVP);

    m_Ctx->OMSetRenderTargets(1, &m_RTV, m_DSV);
    m_Ctx->ClearRenderTargetView(m_RTV, k_BG);
    m_Ctx->ClearDepthStencilView(m_DSV, D3D11_CLEAR_DEPTH, 1.0f, 0);

    D3D11_VIEWPORT vp{};
    vp.Width = (float)m_Width; vp.Height = (float)m_Height; vp.MaxDepth = 1.0f;
    m_Ctx->RSSetViewports(1, &vp);
    m_Ctx->RSSetState(m_RS);
    m_Ctx->OMSetDepthStencilState(m_DSState, 0);
    float bf[4]{};
    m_Ctx->OMSetBlendState(m_BlendState, bf, 0xFFFFFFFF);

    UINT stride = sizeof(ModelVertex), offset = 0;
    m_Ctx->IASetInputLayout(m_IL);
    m_Ctx->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    m_Ctx->IASetVertexBuffers(0, 1, &m_VB, &stride, &offset);
    m_Ctx->VSSetShader(m_VS, nullptr, 0);
    m_Ctx->VSSetConstantBuffers(0, 1, &m_CB);
    m_Ctx->PSSetShader(m_PS, nullptr, 0);
    m_Ctx->PSSetShaderResources(0, 1, &m_TexSRV);
    m_Ctx->PSSetSamplers(0, 1, &m_Sampler);
    m_Ctx->Draw(m_VertCount, 0);

    m_Ctx->OMSetRenderTargets(1, &prevRTV, prevDSV);
    m_Ctx->RSSetState(prevRS);
    m_Ctx->OMSetDepthStencilState(prevDS, prevDSRef);
    m_Ctx->OMSetBlendState(prevBS, prevBF, prevSM);
    m_Ctx->RSSetViewports(1, &prevVP);
    if (prevRTV) prevRTV->Release();
    if (prevDSV) prevDSV->Release();
    if (prevRS)  prevRS->Release();
    if (prevDS)  prevDS->Release();
    if (prevBS)  prevBS->Release();
}

bool PreviewRenderer::GetProjectedUVBounds(float& u0, float& v0, float& u1, float& v1) const
{
    if (!m_LastMVPValid) return false;

    if (m_CachedBoundsValid) {
        u0 = m_CachedU0; v0 = m_CachedV0;
        u1 = m_CachedU1; v1 = m_CachedV1;
        return true;
    }

    const float* M = m_LastMVP;
    const float m00=M[0],m10=M[4],m20=M[8], m30=M[12];
    const float m01=M[1],m11=M[5],m21=M[9], m31=M[13];
    const float m03=M[3],m13=M[7],m23=M[11],m33=M[15];

    float minU = 1e9f, minV = 1e9f, maxU = -1e9f, maxV = -1e9f;
    bool any = false;

    const float* p   = m_UniquePos.data();
    const float* end = p + m_UniquePos.size();
    while (p < end) {
        const float x = p[0], y = p[1], z = p[2];
        p += 3;
        const float cw = x*m03 + y*m13 + z*m23 + m33;
        if (cw <= 0.0f) continue;
        const float inv = 1.0f / cw;
        const float pu  = ((x*m00 + y*m10 + z*m20 + m30) * inv + 1.0f) * 0.5f;
        const float pv  = (1.0f - (x*m01 + y*m11 + z*m21 + m31) * inv) * 0.5f;
        if (pu < minU) minU = pu;
        if (pv < minV) minV = pv;
        if (pu > maxU) maxU = pu;
        if (pv > maxV) maxV = pv;
        any = true;
    }

    if (!any) return false;

    m_CachedU0 = std::max(0.0f, minU);
    m_CachedV0 = std::max(0.0f, minV);
    m_CachedU1 = std::min(1.0f, maxU);
    m_CachedV1 = std::min(1.0f, maxV);
    m_CachedBoundsValid = true;

    u0 = m_CachedU0; v0 = m_CachedV0;
    u1 = m_CachedU1; v1 = m_CachedV1;
    return true;
}

bool PreviewRenderer::GetProjectedSilhouette(std::vector<std::pair<float,float>>& out_uvs) const
{
    if (!m_LastMVPValid || m_UniquePos.empty()) return false;

    if (m_CachedSilValid) {
        out_uvs = m_CachedSilhouette;
        return out_uvs.size() >= 3;
    }

    const float* M  = m_LastMVP;
    const float m00=M[0],m10=M[4],m20=M[8], m30=M[12];
    const float m01=M[1],m11=M[5],m21=M[9], m31=M[13];
    const float m03=M[3],m13=M[7],m23=M[11],m33=M[15];

    static thread_local std::vector<std::pair<float,float>> pts;
    pts.clear();
    pts.reserve(m_UniquePos.size() / 3);

    float minU = 1e9f, minV = 1e9f, maxU = -1e9f, maxV = -1e9f;

    const float* p   = m_UniquePos.data();
    const float* end = p + m_UniquePos.size();
    while (p < end) {
        const float x = p[0], y = p[1], z = p[2];
        p += 3;
        const float cw = x*m03 + y*m13 + z*m23 + m33;
        if (cw <= 0.0f) continue;
        const float inv = 1.0f / cw;
        const float pu  = ((x*m00 + y*m10 + z*m20 + m30) * inv + 1.0f) * 0.5f;
        const float pv  = (1.0f - (x*m01 + y*m11 + z*m21 + m31) * inv) * 0.5f;
        pts.emplace_back(pu, pv);
        if (pu < minU) minU = pu;
        if (pv < minV) minV = pv;
        if (pu > maxU) maxU = pu;
        if (pv > maxV) maxV = pv;
    }
    if (pts.size() < 3) return false;

    const float spanU = maxU - minU, spanV = maxV - minV;
    if (spanU <= 0.0001f || spanV <= 0.0001f) return false;

    constexpr int kGW = 96;
    const int gh = (std::max)(16, (std::min)(220, (int)(kGW * spanV / spanU)));

    static thread_local std::vector<unsigned char> grid;
    grid.assign((size_t)kGW * gh, 0);
    auto cell = [&](int cx, int cy) -> unsigned char& {
        return grid[(size_t)cy * kGW + cx];
    };

    for (const auto& q : pts) {
        int cx = (int)((q.first  - minU) / spanU * (kGW - 1) + 0.5f);
        int cy = (int)((q.second - minV) / spanV * (gh  - 1) + 0.5f);
        if (cx < 0) cx = 0; if (cx >= kGW) cx = kGW - 1;
        if (cy < 0) cy = 0; if (cy >= gh)  cy = gh  - 1;
        cell(cx, cy) = 1;
    }

    static thread_local std::vector<unsigned char> dil;
    dil = grid;
    for (int y = 0; y < gh; ++y)
        for (int x = 0; x < kGW; ++x) {
            if (grid[(size_t)y * kGW + x]) continue;
            bool n = false;
            for (int dy = -1; dy <= 1 && !n; ++dy)
                for (int dx = -1; dx <= 1 && !n; ++dx) {
                    const int nx = x + dx, ny = y + dy;
                    if (nx >= 0 && nx < kGW && ny >= 0 && ny < gh &&
                        grid[(size_t)ny * kGW + nx]) n = true;
                }
            if (n) dil[(size_t)y * kGW + x] = 1;
        }
    grid.swap(dil);

    int sx = -1, sy = -1;
    for (int y = 0; y < gh && sx < 0; ++y)
        for (int x = 0; x < kGW; ++x)
            if (grid[(size_t)y * kGW + x]) { sx = x; sy = y; break; }
    if (sx < 0) return false;

    static constexpr int DX[8] = { -1, -1,  0,  1, 1, 1, 0, -1 };
    static constexpr int DY[8] = {  0, -1, -1, -1, 0, 1, 1,  1 };

    auto occupied = [&](int x, int y) {
        return x >= 0 && x < kGW && y >= 0 && y < gh && grid[(size_t)y * kGW + x] != 0;
    };

    static thread_local std::vector<std::pair<int,int>> contour;
    contour.clear();
    contour.reserve(512);

    int cx = sx, cy = sy;
    int dir = 6;
    const int max_steps = kGW * gh * 4;

    for (int step = 0; step < max_steps; ++step) {
        contour.emplace_back(cx, cy);

        int found = -1;
        for (int i = 0; i < 8; ++i) {
            const int d = (dir + 6 + i) % 8;
            if (occupied(cx + DX[d], cy + DY[d])) { found = d; break; }
        }
        if (found < 0) break;

        cx += DX[found];
        cy += DY[found];
        dir = found;

        if (cx == sx && cy == sy) break;
    }

    if (contour.size() < 3) return false;

    const float cellU = spanU / (kGW - 1);
    const float cellV = spanV / (gh  - 1);

    std::vector<std::pair<float,float>> poly;
    poly.reserve(contour.size());
    for (const auto& c : contour)
        poly.emplace_back(minU + c.first * cellU, minV + c.second * cellV);

    std::vector<std::pair<float,float>> simple;
    simple.reserve(poly.size());
    for (size_t i = 0; i < poly.size(); ++i) {
        if (!simple.empty()) {
            const auto& a = simple.back();
            if (fabsf(a.first - poly[i].first) < 1e-6f &&
                fabsf(a.second - poly[i].second) < 1e-6f) continue;
        }
        simple.push_back(poly[i]);
    }
    if (simple.size() < 3) return false;

    std::vector<std::pair<float,float>> smooth = std::move(simple);
    for (int pass = 0; pass < 2; ++pass) {
        std::vector<std::pair<float,float>> next;
        next.reserve(smooth.size() * 2);
        const size_t n = smooth.size();
        for (size_t i = 0; i < n; ++i) {
            const auto& a = smooth[i];
            const auto& b = smooth[(i + 1) % n];
            next.emplace_back(a.first * 0.75f + b.first * 0.25f,
                              a.second * 0.75f + b.second * 0.25f);
            next.emplace_back(a.first * 0.25f + b.first * 0.75f,
                              a.second * 0.25f + b.second * 0.75f);
        }
        smooth = std::move(next);
    }

    m_CachedSilhouette = std::move(smooth);
    m_CachedSilValid   = true;
    out_uvs = m_CachedSilhouette;
    return out_uvs.size() >= 3;
}

bool PreviewRenderer::GetProjectedJoint(int idx, float& u, float& v) const
{
    if (!m_LastMVPValid || idx < 0 || idx >= k_JointCount) return false;

    const float* M = m_LastMVP;
    const float m00=M[0],m10=M[4],m20=M[8], m30=M[12];
    const float m01=M[1],m11=M[5],m21=M[9], m31=M[13];
    const float m03=M[3],m13=M[7],m23=M[11],m33=M[15];

    const float x = m_Joints[idx][0];
    const float y = m_Joints[idx][1];
    const float z = m_Joints[idx][2];

    const float cw = x*m03 + y*m13 + z*m23 + m33;
    if (cw <= 0.0f) return false;

    const float inv = 1.0f / cw;
    u = ((x*m00 + y*m10 + z*m20 + m30) * inv + 1.0f) * 0.5f;
    v = (1.0f - (x*m01 + y*m11 + z*m21 + m31) * inv) * 0.5f;
    return true;
}

void* PreviewRenderer::GetTextureID() const { return (void*)m_SRV; }

void PreviewRenderer::Shutdown()
{
    m_Ready = false; m_LastMVPValid = false;
    auto rel = [](auto*& p){ if (p){ p->Release(); p = nullptr; } };
    rel(m_RS); rel(m_DSState); rel(m_BlendState);
    rel(m_Sampler); rel(m_TexSRV); rel(m_TexRes);
    rel(m_VB); rel(m_CB); rel(m_IL); rel(m_PS); rel(m_VS);
    rel(m_DSV); rel(m_DepthTex); rel(m_SRV); rel(m_RTV); rel(m_RTTex);
}

}
