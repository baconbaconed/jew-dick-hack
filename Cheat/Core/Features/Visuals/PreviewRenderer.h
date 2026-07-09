#pragma once
#include <d3d11.h>
#include <string>
#include <vector>

namespace Cheat::Core {

    class PreviewRenderer {
    public:
        PreviewRenderer() = default;
        ~PreviewRenderer() { Shutdown(); }

        bool Initialize(ID3D11Device* device, ID3D11DeviceContext* ctx,
                        const std::string& obj_path,
                        const std::string& tex_path,
                        unsigned int rt_width  = 360,
                        unsigned int rt_height = 560);

        void Update(float dt_seconds);

        void AddRotationDelta(float dyaw, float dpitch);

        void AddZoom(float delta);

        void SetAutoSpin(bool on) { m_AutoSpin = on; }
        bool IsAutoSpinning()     const { return m_AutoSpin; }

        void NotifyManualInput() { m_SpinPauseRemaining = 2.0f; }

        void* GetTextureID() const;

        bool GetProjectedUVBounds(float& u0, float& v0, float& u1, float& v1) const;

        bool GetProjectedSilhouette(std::vector<std::pair<float,float>>& out_uvs) const;

        bool GetProjectedJoint(int idx, float& u, float& v) const;
        int  GetJointCount() const { return k_JointCount; }

        bool IsReady() const { return m_Ready; }
        void Shutdown();

        bool UploadModel(const std::string& obj_path);
        bool UploadModelFromMemory(const char* obj_src, std::size_t obj_len);
        bool LoadTexture(const std::string& tex_path);
        bool LoadTextureFromMemory(const unsigned char* png_data, std::size_t png_len);

    private:
        bool CreateRenderTarget(unsigned int w, unsigned int h);
        bool CreateShaders();

        ID3D11Device*              m_Device        = nullptr;
        ID3D11DeviceContext*       m_Ctx            = nullptr;

        ID3D11Texture2D*           m_RTTex          = nullptr;
        ID3D11RenderTargetView*    m_RTV            = nullptr;
        ID3D11ShaderResourceView*  m_SRV            = nullptr;
        ID3D11Texture2D*           m_DepthTex       = nullptr;
        ID3D11DepthStencilView*    m_DSV            = nullptr;
        unsigned int               m_Width          = 0;
        unsigned int               m_Height         = 0;

        ID3D11VertexShader*        m_VS             = nullptr;
        ID3D11PixelShader*         m_PS             = nullptr;
        ID3D11InputLayout*         m_IL             = nullptr;
        ID3D11Buffer*              m_CB             = nullptr;

        ID3D11Buffer*              m_VB             = nullptr;
        unsigned int               m_VertCount      = 0;
        float                      m_ModelScale     = 1.0f;
        float                      m_ModelCenter[3] = {};
        float                      m_RawMin[3]      = {};
        float                      m_RawMax[3]      = {};
        std::vector<float>         m_UniquePos;

        static constexpr int k_JointCount = 13;
        float m_Joints[k_JointCount][3] = {};

        ID3D11Texture2D*           m_TexRes         = nullptr;
        ID3D11ShaderResourceView*  m_TexSRV         = nullptr;
        ID3D11SamplerState*        m_Sampler        = nullptr;

        ID3D11RasterizerState*     m_RS             = nullptr;
        ID3D11DepthStencilState*   m_DSState        = nullptr;
        ID3D11BlendState*          m_BlendState     = nullptr;

        float                      m_Angle             = 0.0f;
        float                      m_ManualYaw         = 3.14159265f;
        float                      m_ManualPitch       = 0.12f;
        float                      m_Zoom              = 1.0f;
        bool                       m_AutoSpin          = false;
        float                      m_SpinPauseRemaining= 0.0f;

        float                      m_LastMVP[16]    = {};
        bool                       m_LastMVPValid   = false;

        bool                       m_SceneDirty     = true;

        float                      m_SpinAccum      = 0.0f;

        mutable std::vector<std::pair<float,float>> m_CachedSilhouette;
        mutable bool                       m_CachedSilValid    = false;
        mutable float                      m_CachedU0 = 0, m_CachedV0 = 0;
        mutable float                      m_CachedU1 = 1, m_CachedV1 = 1;
        mutable bool                       m_CachedBoundsValid = false;

        bool                       m_Ready          = false;
    };

}
