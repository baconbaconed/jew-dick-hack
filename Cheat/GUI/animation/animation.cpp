#include "animation.h"
#include <algorithm>
#include <cmath>

namespace animation {
    float ease_expo(float time, float delay, float duration) {
        if (duration <= 0.0f) {
            return 1.0f;
        }
        if (time <= delay) {
            return 0.0f;
        }

        float progress = (time - delay) / duration;
        progress = std::clamp(progress, 0.0f, 1.0f);
        if (progress >= 1.0f) {
            return 1.0f;
        }

        return 1.0f - std::pow(2.0f, -10.0f * progress);
    }

    float ease_cubic_out(float progress) {
        progress = std::clamp(progress, 0.0f, 1.0f);
        return 1.0f - std::pow(1.0f - progress, 3.0f);
    }

    float ease_cubic_in(float progress) {
        progress = std::clamp(progress, 0.0f, 1.0f);
        return progress * progress * progress;
    }
}
