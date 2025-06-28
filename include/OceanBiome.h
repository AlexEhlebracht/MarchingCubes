#pragma once
#include "Biome.h"
#include <FastNoiseLite.h>

class OceanBiome : public Biome
{
    FastNoiseLite floorNoise;
    float waterLevel;
public:
    OceanBiome(float voxelScale, float waterLevelWorld);
    float getHeight(float wx, float wz) const override;
    glm::vec3 getSurfaceColor(float wy) const override;
};