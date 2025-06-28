#include "../include/PlainsBiome.h"
#include "../include/Chunk.h"        // BASE_HEIGHT_WORLD, etc.

PlainsBiome::PlainsBiome(float scale, float water)
    : waterLevel(water)
{
    auto cfg = [&](FastNoiseLite& n, float freq, int oct)
        {
            n.SetNoiseType(FastNoiseLite::NoiseType_OpenSimplex2);
            n.SetFrequency(freq / scale);
            n.SetFractalOctaves(oct);
            n.SetFractalGain(0.5f);
        };
    cfg(continental, 0.0001f, 4);
    cfg(hills, 0.0010f, 3);
    cfg(detail, 0.0060f, 2);
}

static float hillStrength(float aboveWater)
{
    return glm::clamp((aboveWater - 32.0f) / 8.0f, 0.f, 1.f);
}

float PlainsBiome::getHeight(float wx, float wz) const
{
    // --- 1. Continental & detail drive base terrain -----------
    float baseNoise = continental.GetNoise(wx, wz) * 0.85f
        + detail.GetNoise(wx, wz) * 0.15f;

    float baseShape = 0.5f * (baseNoise + 1.f);  // [0, 1]

    float h = waterLevel + 5.0f * VOXEL_SIZE
        + HEIGHT_VARIATION_WORLD * 0.4f * baseShape;  // base shape

    // --- 2. Hills are added separately ------------------------
    float hillNoise = hills.GetNoise(wx, wz);  // [-1, 1]
    float hillHeight = HEIGHT_VARIATION_WORLD * 0.5f * hillNoise;

    float above = h - waterLevel;
    float hillFade = hillStrength(above);   // 0 near water, 1 inland

    h += hillHeight * hillFade;

    return h;
}


glm::vec3 PlainsBiome::getSurfaceColor(float) const
{
    return { 0.25f, 0.6f, 0.25f };   // grass-green
}