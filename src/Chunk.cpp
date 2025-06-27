#define GLM_ENABLE_EXPERIMENTAL
#include "../include/Chunk.h"
#include "../include/Voxel.h"
#include <glm/gtc/noise.hpp>
#include <glm/gtx/normal.hpp>
#include <GLFW/glfw3.h>
#include <iostream>
#include <algorithm>
#include <cmath>

Chunk::Chunk(glm::ivec2 pos)
    : position(pos), mesh(nullptr), dirty(true)
{
    density.resize(
        CHUNK_SIZE + 1,
        std::vector<std::vector<float>>(CHUNK_HEIGHT + 1,
            std::vector<float>(CHUNK_SIZE + 1)));

    voxelScale = float(VOXEL_SIZE) / DESIGN_VOXEL;

    continentalNoise.SetNoiseType(FastNoiseLite::NoiseType_OpenSimplex2);
    continentalNoise.SetFrequency(0.0001f / voxelScale);
    continentalNoise.SetFractalOctaves(4);
    continentalNoise.SetFractalGain(0.5f);

    hillNoise.SetNoiseType(FastNoiseLite::NoiseType_OpenSimplex2);
    hillNoise.SetFrequency(0.001f / voxelScale);
    hillNoise.SetFractalOctaves(3);
    hillNoise.SetFractalGain(0.5f);

    detailNoise.SetNoiseType(FastNoiseLite::NoiseType_OpenSimplex2);
    detailNoise.SetFrequency(0.003f / voxelScale);
    detailNoise.SetFractalOctaves(2);
    detailNoise.SetFractalGain(0.4f);
}

Chunk::~Chunk()
{
    if (mesh) delete mesh;
}

float Chunk::getPlainsNoise(float wx, float wz) const
{
    float continental = continentalNoise.GetNoise(wx, wz) * 0.6f;
    float hills = hillNoise.GetNoise(wx, wz) * 0.3f;
    float detail = detailNoise.GetNoise(wx, wz) * 0.1f;

    float n = continental + hills + detail;
    return glm::clamp(n, -1.0f, 1.0f);
}

float Chunk::getPlainsHeight(float wx, float wz) const
{
    return BASE_HEIGHT_WORLD + HEIGHT_VARIATION_WORLD * getPlainsNoise(wx, wz);
}

void Chunk::generateDensityField()
{
    int worldX = position.x * CHUNK_SIZE;
    int worldZ = position.y * CHUNK_SIZE;

    for (int x = 0; x <= CHUNK_SIZE; ++x)
        for (int y = 0; y <= CHUNK_HEIGHT; ++y)
            for (int z = 0; z <= CHUNK_SIZE; ++z)
            {
                float wx = (x + worldX) * VOXEL_SIZE;
                float wy = y * VOXEL_SIZE;
                float wz = (z + worldZ) * VOXEL_SIZE;

                float surfaceY = getPlainsHeight(wx, wz);
                density[x][y][z] = surfaceY - wy;
            }

    dirty = true;
}

float Chunk::getDensityAt(int x, int y, int z)
{
    if (x >= 0 && x <= CHUNK_SIZE &&
        y >= 0 && y <= CHUNK_HEIGHT &&
        z >= 0 && z <= CHUNK_SIZE)
        return density[x][y][z];

    float wx = (x + position.x * CHUNK_SIZE) * VOXEL_SIZE;
    float wy = y * VOXEL_SIZE;
    float wz = (z + position.y * CHUNK_SIZE) * VOXEL_SIZE;

    float surfaceY = getPlainsHeight(wx, wz);
    return surfaceY - wy;
}

void Chunk::polygoniseCube(int x, int y, int z,
    std::vector<glm::vec3>& vertices,
    std::vector<glm::vec3>& colors,
    std::vector<glm::vec3>& normals,
    std::vector<unsigned int>& indices,
    int& indexOffset,
    float isoLevel)
{
    auto getTerrainColor = [&](const glm::vec3& v) -> glm::vec3
        {
            float worldY = v.y;

            if (worldY < WATER_LEVEL_WORLD)
                return glm::vec3(0.2f, 0.4f, 1.0f);
            else if (worldY < WATER_LEVEL_WORLD + 1.5f * VOXEL_SIZE)
                return glm::vec3(0.93f, 0.85f, 0.55f);
            else
                return glm::vec3(0.25f, 0.6f, 0.25f);
        };

    float d[8];
    d[0] = getDensityAt(x, y, z);
    d[1] = getDensityAt(x + 1, y, z);
    d[2] = getDensityAt(x + 1, y, z + 1);
    d[3] = getDensityAt(x, y, z + 1);
    d[4] = getDensityAt(x, y + 1, z);
    d[5] = getDensityAt(x + 1, y + 1, z);
    d[6] = getDensityAt(x + 1, y + 1, z + 1);
    d[7] = getDensityAt(x, y + 1, z + 1);

    bool allInside = true, allOutside = true;
    for (int i = 0; i < 8; ++i)
    {
        if (d[i] <= isoLevel) allInside = false;
        if (d[i] >= isoLevel) allOutside = false;
    }

    if (allInside || allOutside)
        return;

    int cubeIndex = 0;
    for (int i = 0; i < 8; ++i)
        if (d[i] < isoLevel)
            cubeIndex |= 1 << i;

    if (edgeTable[cubeIndex] == 0)
        return;

    glm::vec3 vertList[12];
    for (int i = 0; i < 12; ++i)
    {
        if (edgeTable[cubeIndex] & (1 << i))
        {
            int v0 = edgeVertexIndices[i][0];
            int v1 = edgeVertexIndices[i][1];
            float t = (isoLevel - d[v0]) / (d[v1] - d[v0]);
            t = glm::clamp(t, 0.0f, 1.0f);

            glm::vec3 p0 = (vertexOffsets[v0] + glm::vec3(x, y, z)) * float(VOXEL_SIZE);
            glm::vec3 p1 = (vertexOffsets[v1] + glm::vec3(x, y, z)) * float(VOXEL_SIZE);
            vertList[i] = glm::mix(p0, p1, t);
        }
    }

    for (int i = 0; triTable[cubeIndex][i] != -1; i += 3)
    {
        glm::vec3 v0 = vertList[triTable[cubeIndex][i]];
        glm::vec3 v1 = vertList[triTable[cubeIndex][i + 1]];
        glm::vec3 v2 = vertList[triTable[cubeIndex][i + 2]];

        vertices.push_back(v0);
        vertices.push_back(v1);
        vertices.push_back(v2);

        colors.push_back(getTerrainColor(v0));
        colors.push_back(getTerrainColor(v1));
        colors.push_back(getTerrainColor(v2));

        const float eps = 0.25f * VOXEL_SIZE;

        auto sampleDensity = [&](const glm::vec3& p) -> float
            {
                float wx = p.x + position.x * CHUNK_SIZE * VOXEL_SIZE;
                float wy = p.y;
                float wz = p.z + position.y * CHUNK_SIZE * VOXEL_SIZE;
                float surfaceY = getPlainsHeight(wx, wz);
                return surfaceY - wy;
            };

        auto gradient = [&](const glm::vec3& p) -> glm::vec3
            {
                float dx = sampleDensity(glm::vec3(p.x + eps, p.y, p.z)) -
                    sampleDensity(glm::vec3(p.x - eps, p.y, p.z));
                float dy = sampleDensity(glm::vec3(p.x, p.y + eps, p.z)) -
                    sampleDensity(glm::vec3(p.x, p.y - eps, p.z));
                float dz = sampleDensity(glm::vec3(p.x, p.y, p.z + eps)) -
                    sampleDensity(glm::vec3(p.x, p.y, p.z - eps));
                return glm::vec3(dx, dy, dz);
            };

        glm::vec3 n0 = -glm::normalize(gradient(v0));
        glm::vec3 n1 = -glm::normalize(gradient(v1));
        glm::vec3 n2 = -glm::normalize(gradient(v2));

        if (glm::length(n0) < 1e-3f) n0 = glm::vec3(0, 1, 0);
        if (glm::length(n1) < 1e-3f) n1 = glm::vec3(0, 1, 0);
        if (glm::length(n2) < 1e-3f) n2 = glm::vec3(0, 1, 0);

        normals.push_back(n0);
        normals.push_back(n1);
        normals.push_back(n2);

        indices.push_back(indexOffset);
        indices.push_back(indexOffset + 2);
        indices.push_back(indexOffset + 1);
        indexOffset += 3;
    }
}

void Chunk::buildMeshData(std::vector<glm::vec3>& vertices,
    std::vector<glm::vec3>& colors,
    std::vector<glm::vec3>& normals,
    std::vector<unsigned int>& indices)
{
    if (!dirty)
        return;

    vertices.clear();
    colors.clear();
    normals.clear();
    indices.clear();

    int indexOffset = 0;
    float isoLevel = 0.0f;

    for (int x = 0; x < CHUNK_SIZE; ++x)
        for (int y = 0; y < CHUNK_HEIGHT; ++y)
            for (int z = 0; z < CHUNK_SIZE; ++z)
                polygoniseCube(x, y, z, vertices, colors, normals, indices, indexOffset, isoLevel);

    glm::vec3 offset(position.x * CHUNK_SIZE * VOXEL_SIZE,
        0.0f,
        position.y * CHUNK_SIZE * VOXEL_SIZE);
    for (auto& v : vertices)
        v += offset;

    dirty = false;
}

bool Chunk::generateData(std::vector<glm::vec3>& vertices,
    std::vector<glm::vec3>& colors,
    std::vector<glm::vec3>& normals,
    std::vector<unsigned int>& indices)
{
    if (dirty)
        generateDensityField();

    buildMeshData(vertices, colors, normals, indices);
    return !vertices.empty();
}

void Chunk::finalize(std::vector<glm::vec3>& vertices,
    std::vector<glm::vec3>& colors,
    std::vector<glm::vec3>& normals,
    std::vector<unsigned int>& indices)
{
    if (mesh)
        delete mesh;

    mesh = vertices.empty() ? nullptr : new Mesh(vertices, colors, normals, indices);
}

void Chunk::draw(const Shader& shader)
{
    if (mesh)
        mesh->draw();
}
