#pragma once
#include <glm/glm.hpp>

class Biome
{
public:
    virtual ~Biome() = default;

    // Height of solid ground at world-space (x,z) in *world units*
    virtual float getHeight(float wx, float wz) const = 0;

    // Colour you want on top of that ground (later you might add vegetation masks, etc.)
    virtual glm::vec3 getSurfaceColor(float wy) const = 0;
};
