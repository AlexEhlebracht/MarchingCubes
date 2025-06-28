#pragma once
#include "Biome.h"
#include <FastNoiseLite.h>

class PlainsBiome : public Biome
{
    FastNoiseLite continental, hills, detail;
    float waterLevel;
public:
    PlainsBiome(float voxelScale, float waterLevelWorld);
    float getHeight(float wx, float wz) const override;
    glm::vec3 getSurfaceColor(float wy) const override;
};