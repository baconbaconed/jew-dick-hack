#include "Classes.h"
#include "../../../Memory/Memory.h"
#include "../Offsets/Offsets.h"

Vector3 Cheat::Camera::GetPosition() const
{
    if (!g_Memory.IsValid(address))
        return {};

    return g_Memory.Read<Vector3>(address + Offsets::Camera::Position);
}

Matrix4x4 Cheat::Camera::GetRotation() const
{
    if (!g_Memory.IsValid(address))
        return {};

    float rot[9];
    g_Memory.ReadRaw(address + Offsets::Camera::Rotation, &rot, sizeof(rot));

    return Matrix4x4(
        rot[0], rot[1], rot[2], 0.0f,
        rot[3], rot[4], rot[5], 0.0f,
        rot[6], rot[7], rot[8], 0.0f,
        0.0f, 0.0f, 0.0f, 1.0f
    );
}

float Cheat::Camera::GetFieldOfView() const
{
    if (!g_Memory.IsValid(address))
        return 0.f;

    return g_Memory.Read<float>(address + Offsets::Camera::FieldOfView);
}

Vector2 Cheat::Camera::GetViewportSize() const
{
    if (!g_Memory.IsValid(address))
        return {};

    return g_Memory.Read<Vector2>(address + Offsets::Camera::ViewportSize);
}

void Cheat::Camera::SetFieldOfView(float fov) const
{
    if (!g_Memory.IsValid(address))
        return;

    g_Memory.Write<float>(address + Offsets::Camera::FieldOfView, fov);
}

void Cheat::Camera::SetRotation(const Matrix4x4& rot) const
{
    if (!g_Memory.IsValid(address))
        return;

    float r[9] = {
        rot.m[0][0], rot.m[0][1], rot.m[0][2],
        rot.m[1][0], rot.m[1][1], rot.m[1][2],
        rot.m[2][0], rot.m[2][1], rot.m[2][2]
    };
    g_Memory.WriteRaw(address + Offsets::Camera::Rotation, &r, sizeof(r));
}

bool Cheat::Camera::WorldToScreen(const Vector3& worldPos, Vector2& screenPos) const
{
    if (!g_Memory.IsValid(address))
        return false;

    Matrix4x4 viewMatrix = GetRotation();
    Vector3 cameraPos = GetPosition();
    Vector2 viewportSize = GetViewportSize();

    Vector3 relativePos = worldPos - cameraPos;

    float x = relativePos.x * viewMatrix.m[0][0] + relativePos.y * viewMatrix.m[0][1] + relativePos.z * viewMatrix.m[0][2];
    float y = relativePos.x * viewMatrix.m[1][0] + relativePos.y * viewMatrix.m[1][1] + relativePos.z * viewMatrix.m[1][2];
    float z = relativePos.x * viewMatrix.m[2][0] + relativePos.y * viewMatrix.m[2][1] + relativePos.z * viewMatrix.m[2][2];

    if (z > 0) return false;

    float fov = GetFieldOfView();
    float tanHalfFov = tanf(fov * 0.5f * (3.1415926535f / 180.f));
    float aspect = viewportSize.x / viewportSize.y;

    float screenX = (viewportSize.x / 2.0f) * (1.0f + (x / (-z * tanHalfFov * aspect)));
    float screenY = (viewportSize.y / 2.0f) * (1.0f - (y / (-z * tanHalfFov)));

    screenPos = { screenX, screenY };
    return true;
}
