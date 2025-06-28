#include "../include/OceanBiome.h"
#include "../include/Chunk.h"

OceanBiome::OceanBiome(float scale, float water)
    : waterLevel(water)
{
    floorNoise.SetNoiseType(FastNoiseLite::NoiseType_OpenSimplex2);
    floorNoise.SetFrequency(0.00005f / scale);
    floorNoise.SetFractalOctaves(3);
}

float OceanBiome::getHeight(float wx, float wz) const
{
    float base = waterLevel - 15.0f * VOXEL_SIZE;          // deep
    float var = 1.0f * VOXEL_SIZE * floorNoise.GetNoise(wx, wz);
    return base + var;
}

glm::vec3 OceanBiome::getSurfaceColor(float) const
{
    return { 0.10f, 0.35f, 0.55f };  // dark sand / mud
}