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

    noise.SetNoiseType(FastNoiseLite::NoiseType_OpenSimplex2);
    noise.SetFrequency(0.01f);
    noise.SetFractalOctaves(1);
    noise.SetFractalLacunarity(2.0f);
    noise.SetFractalGain(0.5f);
}

Chunk::~Chunk()
{
    if (mesh)
        delete mesh;
}

void Chunk::generateDensityField()
{
    int worldX = position.x * CHUNK_SIZE;
    int worldZ = position.y * CHUNK_SIZE;

    for (int x = 0; x <= CHUNK_SIZE; ++x)
        for (int y = 0; y <= CHUNK_HEIGHT; ++y)
            for (int z = 0; z <= CHUNK_SIZE; ++z)
            {
                float nx = (x + worldX) * VOXEL_SIZE;
                float ny = y * VOXEL_SIZE;
                float nz = (z + worldZ) * VOXEL_SIZE;

                float baseHeight = 8.0f;
                float heightVariation = 8.0f;
                float height = baseHeight + heightVariation * noise.GetNoise(nx, nz);

                density[x][y][z] = height - ny;
            }

    dirty = true;
}

float Chunk::getDensityAt(int x, int y, int z)
{
    if (x >= 0 && x <= CHUNK_SIZE &&
        y >= 0 && y <= CHUNK_HEIGHT &&
        z >= 0 && z <= CHUNK_SIZE)
        return density[x][y][z];

    float nx = static_cast<float>(x + position.x * CHUNK_SIZE);
    float ny = static_cast<float>(y);
    float nz = static_cast<float>(z + position.y * CHUNK_SIZE);

    float baseHeight = 8.0f;
    float heightVariation = 8.0f;
    float height = baseHeight + heightVariation * noise.GetNoise(nx, nz);
    return height - ny;
}

void Chunk::polygoniseCube(int x, int y, int z,
    std::vector<glm::vec3>& vertices,
    std::vector<glm::vec3>& colors,
    std::vector<glm::vec3>& normals,
    std::vector<unsigned int>& indices,
    int& indexOffset,
    float isoLevel)
{
    glm::vec3 cubeColor(39.0f / 255.0f, 138.0f / 255.0f, 69.0f / 255.0f);

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
        if (d[i] <= isoLevel)
            allInside = false;
        if (d[i] >= isoLevel)
            allOutside = false;
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

        colors.push_back(cubeColor);
        colors.push_back(cubeColor);
        colors.push_back(cubeColor);

        const float eps = 0.25f * VOXEL_SIZE;  // Gradient step in world units

        auto sampleDensity = [&](const glm::vec3& p) -> float
            {
                float wx = p.x + position.x * CHUNK_SIZE * VOXEL_SIZE;
                float wy = p.y;                                  // already world Y
                float wz = p.z + position.y * CHUNK_SIZE * VOXEL_SIZE;

                float baseHeight = 8.0f;
                float heightVariation = 8.0f;
                float height = baseHeight + heightVariation * noise.GetNoise(wx, wz);
                return height - wy;
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

        if (glm::length(n0) < 1e-3f)
            n0 = glm::vec3(0.0f, 1.0f, 0.0f);
        if (glm::length(n1) < 1e-3f)
            n1 = glm::vec3(0.0f, 1.0f, 0.0f);
        if (glm::length(n2) < 1e-3f)
            n2 = glm::vec3(0.0f, 1.0f, 0.0f);

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

    double start = glfwGetTime();

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
