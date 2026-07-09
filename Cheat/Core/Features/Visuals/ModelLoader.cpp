#define NOMINMAX
#include "ModelLoader.h"
#include <fstream>
#include <sstream>
#include <cstring>
#include <algorithm>
#include <cmath>

namespace Cheat::Core {

    static bool ParseOBJStream(std::istream& stream,
                               std::vector<ModelVertex>& out_vertices,
                               float& out_scale,
                               float  out_center[3],
                               float  out_raw_min[3],
                               float  out_raw_max[3],
                               std::vector<float>* out_unique_positions)
    {
        std::vector<float> positions;
        std::vector<float> uvs;

        std::string line;
        while (std::getline(stream, line)) {
            if (line.empty()) continue;
            const char* s = line.c_str();

            if (s[0] == 'v' && s[1] == ' ') {
                float x, y, z;
                if (sscanf_s(s + 2, "%f %f %f", &x, &y, &z) == 3) {
                    positions.push_back(x); positions.push_back(y); positions.push_back(z);
                }
            } else if (s[0] == 'v' && s[1] == 't') {
                float u, v;
                if (sscanf_s(s + 3, "%f %f", &u, &v) == 2) { uvs.push_back(u); uvs.push_back(v); }
            } else if (s[0] == 'f' && s[1] == ' ') {
                struct Idx { int v, vt; };
                Idx idx[4]; int count = 0;
                const char* p = s + 2;
                while (*p && count < 4) {
                    while (*p == ' ') ++p;
                    if (!*p) break;
                    int vi = 0, vti = 0;
                    while (*p >= '0' && *p <= '9') vi  = vi  * 10 + (*p++ - '0');
                    if (*p == '/') { ++p; while (*p >= '0' && *p <= '9') vti = vti * 10 + (*p++ - '0');
                        if (*p == '/') { ++p; while (*p >= '0' && *p <= '9') ++p; } }
                    if (vi > 0) { idx[count++] = { vi - 1, vti - 1 }; } else break;
                }
                auto emit = [&](Idx a, Idx b, Idx c) {
                    auto vv = [&](Idx i) {
                        ModelVertex mv{};
                        if (i.v * 3 + 2 < (int)positions.size()) {
                            mv.x = positions[i.v*3+0]; mv.y = positions[i.v*3+1]; mv.z = positions[i.v*3+2];
                        }
                        if (i.vt >= 0 && i.vt * 2 + 1 < (int)uvs.size()) {
                            mv.u = uvs[i.vt*2+0]; mv.v = 1.0f - uvs[i.vt*2+1];
                        }
                        out_vertices.push_back(mv);
                    };
                    vv(a); vv(b); vv(c);
                };
                if (count == 3) emit(idx[0], idx[1], idx[2]);
                else if (count == 4) { emit(idx[0], idx[1], idx[2]); emit(idx[0], idx[2], idx[3]); }
            }
        }

        if (out_vertices.empty()) return false;

        float mn[3] = {1e9f,1e9f,1e9f}, mx[3] = {-1e9f,-1e9f,-1e9f};
        for (auto& v : out_vertices) {
            mn[0]=std::min(mn[0],v.x); mx[0]=std::max(mx[0],v.x);
            mn[1]=std::min(mn[1],v.y); mx[1]=std::max(mx[1],v.y);
            mn[2]=std::min(mn[2],v.z); mx[2]=std::max(mx[2],v.z);
        }
        for (int i=0;i<3;i++) { out_raw_min[i]=mn[i]; out_raw_max[i]=mx[i]; }
        float ext[3]={mx[0]-mn[0],mx[1]-mn[1],mx[2]-mn[2]};
        float maxExt=std::max({ext[0],ext[1],ext[2]});
        out_scale=maxExt>0.0f?1.0f/maxExt:1.0f;
        out_center[0]=(mn[0]+mx[0])*0.5f; out_center[1]=(mn[1]+mx[1])*0.5f; out_center[2]=(mn[2]+mx[2])*0.5f;
        if (out_unique_positions) *out_unique_positions = positions;
        return true;
    }

    bool LoadOBJ(const std::string& path,
                 std::vector<ModelVertex>& out_vertices,
                 float& out_scale,
                 float  out_center[3],
                 float  out_raw_min[3],
                 float  out_raw_max[3],
                 std::vector<float>* out_unique_positions)
    {
        std::ifstream file(path);
        if (!file.is_open()) return false;
        return ParseOBJStream(file, out_vertices, out_scale, out_center, out_raw_min, out_raw_max, out_unique_positions);
    }

    bool LoadOBJFromMemory(const char* src, std::size_t len,
                           std::vector<ModelVertex>& out_vertices,
                           float& out_scale,
                           float  out_center[3],
                           float  out_raw_min[3],
                           float  out_raw_max[3],
                           std::vector<float>* out_unique_positions)
    {
        if (!src || len == 0) return false;
        std::istringstream ss(std::string(src, len));
        return ParseOBJStream(ss, out_vertices, out_scale, out_center, out_raw_min, out_raw_max, out_unique_positions);
    }

}
