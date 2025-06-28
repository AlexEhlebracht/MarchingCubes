#pragma once
#include "PlainsBiome.h"
#include "OceanBiome.h"
#include <memory>

struct BiomeSample
{
    float height;          // blended height
    float oceanWeight;     // 1==pure ocean, 0==pure plains
};

class BiomeManager
{
    FastNoiseLite biomeNoise;                    // selects between biomes
    std::unique_ptr<PlainsBiome> plains;
    std::unique_ptr<OceanBiome>  ocean;
public:
    BiomeManager(float voxelScale, float waterLevelWorld);

    BiomeSample sample(float wx, float wz) const;

    glm::vec3 blendedSurfaceColor(float wy, float oceanW, float wx, float wz) const;

    bool nearOcean(float wx, float wz) const;
};