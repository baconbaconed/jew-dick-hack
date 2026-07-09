#pragma once
#include <vector>
#include <string>

namespace Cheat::Core {

    struct ModelVertex {
        float x, y, z;
        float u, v;
    };

    bool LoadOBJ(const std::string& path,
                 std::vector<ModelVertex>& out_vertices,
                 float& out_scale,
                 float  out_center[3],
                 float  out_raw_min[3],
                 float  out_raw_max[3],
                 std::vector<float>* out_unique_positions = nullptr);

    bool LoadOBJFromMemory(const char* src, std::size_t len,
                           std::vector<ModelVertex>& out_vertices,
                           float& out_scale,
                           float  out_center[3],
                           float  out_raw_min[3],
                           float  out_raw_max[3],
                           std::vector<float>* out_unique_positions = nullptr);
}
