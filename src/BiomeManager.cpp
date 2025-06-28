#include "../include/BiomeManager.h"
#include "../include/Chunk.h"

static constexpr float LAND_BIAS = 0.0f;

BiomeManager::BiomeManager(float scale, float water)
{
    biomeNoise.SetNoiseType(FastNoiseLite::NoiseType_OpenSimplex2);
    biomeNoise.SetFrequency(0.00025f / scale);          // gigantic continents
    biomeNoise.SetFractalOctaves(5);
    biomeNoise.SetFractalLacunarity(3);
    biomeNoise.SetFractalGain(0.2f);

    plains = std::make_unique<PlainsBiome>(scale, water);
    ocean = std::make_unique<OceanBiome >(scale, water);
}

BiomeSample BiomeManager::sample(float wx, float wz) const
{
    float mask = biomeNoise.GetNoise(wx, wz);          // [-1..1]
    float t = glm::smoothstep(-1.0f, 0.0f, mask + LAND_BIAS);      // mask weight

    float hOcean = ocean->getHeight(wx, wz);
    float hPlains = plains->getHeight(wx, wz);
    float h = glm::mix(hOcean, hPlains, t);

    /* If final height is below water, force ocean weight to 1.0 */
    float oceanWeight = (h <= WATER_LEVEL_WORLD) ? 1.0f : (1.0f - t);

    return { h, oceanWeight };
}

bool BiomeManager::nearOcean(float wx, float wz) const
{
    const float step = 4.0f * VOXEL_SIZE;  // how far to sample
    for (float dx = -step; dx <= step; dx += step)
    {
        for (float dz = -step; dz <= step; dz += step)
        {
            float mask = biomeNoise.GetNoise(wx + dx, wz + dz);
            float t = glm::smoothstep(0.5f, 1.0f, mask + LAND_BIAS);
            if (t < 0.5f)  // <0.5 = oceanish
                return true;
        }
    }
    return false;
}

glm::vec3 BiomeManager::blendedSurfaceColor(float wy, float oceanW, float wx, float wz) const
{
    const float solidSandStart = WATER_LEVEL_WORLD - 0.1f * VOXEL_SIZE;
    const float solidSandEnd = WATER_LEVEL_WORLD + 2.0f * VOXEL_SIZE;  // solid beach
    const float blendEnd = WATER_LEVEL_WORLD + 3.0f * VOXEL_SIZE;  // end of blend

    glm::vec3 sand = { 0.93f, 0.85f, 0.55f };
    glm::vec3 grass = plains->getSurfaceColor(wy);

    if (nearOcean(wx, wz))
    {
        if (wy >= solidSandStart && wy < solidSandEnd)
        {
            // Solid sand
            return sand;
        }
        else if (wy >= solidSandEnd && wy < blendEnd)
        {
            // Smooth blend into grass
            float t = (wy - solidSandEnd) / (blendEnd - solidSandEnd); // [0 → 1]
            return glm::mix(sand, grass, t);
        }
    }

    // Normal biome blend
    return glm::mix(ocean->getSurfaceColor(wy),
        plains->getSurfaceColor(wy),
        1.f - oceanW);
}
