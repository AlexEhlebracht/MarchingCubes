#define GLM_ENABLE_EXPERIMENTAL
#include "../include/Chunk.h"
#include "../include/Voxel.h"
#include <glm/gtc/noise.hpp>
#include <glm/gtx/normal.hpp>
#include <GLFW/glfw3.h>
#include <iostream>
#include <algorithm>
#include <cmath>

/* -------------------------- */
/* Chunk Constructor          */
/* Allocates density 3D array */
/* -------------------------- */
Chunk::Chunk(glm::ivec2 pos, const BiomeManager* biomeMgr)
    : position(pos), biome(biomeMgr), mesh(nullptr), dirty(true)
{
    // Allocate 3D density field with one extra for boundary (CHUNK_SIZE+1)
    density.resize(
        CHUNK_SIZE + 1,
        std::vector<std::vector<float>>(CHUNK_HEIGHT + 1,
            std::vector<float>(CHUNK_SIZE + 1)));
}

/* -------------------------- */
/* Chunk Destructor           */
/* Cleans up allocated mesh  */
/* -------------------------- */
Chunk::~Chunk()
{
    if (mesh)
        delete mesh;
}

/* -------------------------- */
/* Generate density field for entire chunk */
/* Density = surfaceHeight - current voxel world y */
/* Positive density = inside terrain, negative = outside */
/* -------------------------- */
void Chunk::generateDensityField()
{
    int worldX = position.x * CHUNK_SIZE;
    int worldZ = position.y * CHUNK_SIZE;

    for (int x = 0; x <= CHUNK_SIZE; ++x)
        for (int y = 0; y <= CHUNK_HEIGHT; ++y)
            for (int z = 0; z <= CHUNK_SIZE; ++z)
            {
                float wx = (x + worldX) * VOXEL_SIZE;
                float wz = (z + worldZ) * VOXEL_SIZE;
                float wy = y * VOXEL_SIZE;

                float surfaceY = biome->sample(wx, wz).height;
                density[x][y][z] = surfaceY - wy;
            }

    dirty = true;
}

/* -------------------------- */
/* Get density value at voxel coordinate (x,y,z) */
/* Returns cached density if in bounds, otherwise computes on the fly */
/* -------------------------- */
float Chunk::getDensityAt(int x, int y, int z)
{
    if (x >= 0 && x <= CHUNK_SIZE &&
        y >= 0 && y <= CHUNK_HEIGHT &&
        z >= 0 && z <= CHUNK_SIZE)
        return density[x][y][z];

    float wx = (x + position.x * CHUNK_SIZE) * VOXEL_SIZE;
    float wz = (z + position.y * CHUNK_SIZE) * VOXEL_SIZE;
    float wy = y * VOXEL_SIZE;

    return biome->sample(wx, wz).height - wy;
}

/* -------------------------- */
/* Polygonise a single cube in the density field using marching cubes */
/* Generates vertices, colors, normals, and indices */
/* -------------------------- */
void Chunk::polygoniseCube(int x, int y, int z,
    std::vector<glm::vec3>& vertices,
    std::vector<glm::vec3>& colors,
    std::vector<glm::vec3>& normals,
    std::vector<unsigned int>& indices,
    int& indexOffset,
    float isoLevel)
{
    // Colour helper that blends biomes
    auto vertexColour = [&](const glm::vec3& vLocal) -> glm::vec3
    {
        float wx = vLocal.x + position.x * CHUNK_SIZE * VOXEL_SIZE;
        float wz = vLocal.z + position.y * CHUNK_SIZE * VOXEL_SIZE;
        float wy = vLocal.y;
        auto sample = biome->sample(wx, wz);
        return biome->blendedSurfaceColor(wy, sample.oceanWeight, wx, wz);
    };

    // Get densities at cube corners
    float d[8];
    d[0] = getDensityAt(x, y, z);
    d[1] = getDensityAt(x + 1, y, z);
    d[2] = getDensityAt(x + 1, y, z + 1);
    d[3] = getDensityAt(x, y, z + 1);
    d[4] = getDensityAt(x, y + 1, z);
    d[5] = getDensityAt(x + 1, y + 1, z);
    d[6] = getDensityAt(x + 1, y + 1, z + 1);
    d[7] = getDensityAt(x, y + 1, z + 1);

    // Quick rejection if all corners inside or outside the surface
    bool allInside = true, allOutside = true;
    for (int i = 0; i < 8; ++i)
    {
        if (d[i] <= isoLevel) allInside = false;
        if (d[i] >= isoLevel) allOutside = false;
    }
    if (allInside || allOutside)
        return; // No polygons needed

    // Compute cube index for marching cubes table lookup
    int cubeIndex = 0;
    for (int i = 0; i < 8; ++i)
        if (d[i] < isoLevel)
            cubeIndex |= 1 << i;

    if (edgeTable[cubeIndex] == 0)
        return;

    // Interpolate vertices along edges where the surface crosses
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

    // Generate triangles from triTable for this cubeIndex
    for (int i = 0; triTable[cubeIndex][i] != -1; i += 3)
    {
        glm::vec3 v0 = vertList[triTable[cubeIndex][i]];
        glm::vec3 v1 = vertList[triTable[cubeIndex][i + 1]];
        glm::vec3 v2 = vertList[triTable[cubeIndex][i + 2]];

        vertices.push_back(v0);
        vertices.push_back(v1);
        vertices.push_back(v2);

        // Assign colors based on vertex height
        colors.push_back(vertexColour(v0));
        colors.push_back(vertexColour(v1));
        colors.push_back(vertexColour(v2));

        // Calculate normals by sampling density gradient
        const float eps = 0.25f * VOXEL_SIZE;

        auto densitySample = [&](const glm::vec3& p) -> float
        {
            float wx = p.x + position.x * CHUNK_SIZE * VOXEL_SIZE;
            float wz = p.z + position.y * CHUNK_SIZE * VOXEL_SIZE;
            float wy = p.y;
            return biome->sample(wx, wz).height - wy;
        };

        auto gradient = [&](const glm::vec3& p) -> glm::vec3
        {
            float dx = densitySample(glm::vec3(p.x + eps, p.y, p.z)) -
                densitySample(glm::vec3(p.x - eps, p.y, p.z));
            float dy = densitySample(glm::vec3(p.x, p.y + eps, p.z)) -
                densitySample(glm::vec3(p.x, p.y - eps, p.z));
            float dz = densitySample(glm::vec3(p.x, p.y, p.z + eps)) -
                densitySample(glm::vec3(p.x, p.y, p.z - eps));
            return glm::vec3(dx, dy, dz);
        };

        glm::vec3 n0 = -glm::normalize(gradient(v0));
        glm::vec3 n1 = -glm::normalize(gradient(v1));
        glm::vec3 n2 = -glm::normalize(gradient(v2));

        // Fallback normals if too small
        if (glm::length(n0) < 1e-3f) n0 = glm::vec3(0, 1, 0);
        if (glm::length(n1) < 1e-3f) n1 = glm::vec3(0, 1, 0);
        if (glm::length(n2) < 1e-3f) n2 = glm::vec3(0, 1, 0);

        normals.push_back(n0);
        normals.push_back(n1);
        normals.push_back(n2);

        // Add triangle indices (note winding order)
        indices.push_back(indexOffset);
        indices.push_back(indexOffset + 2);
        indices.push_back(indexOffset + 1);
        indexOffset += 3;
    }
}

/* -------------------------- */
/* Build entire mesh data for chunk by polygonizing all cubes */
/* Offsets vertices to world position */
/* -------------------------- */
void Chunk::buildMeshData(std::vector<glm::vec3>& vertices,
    std::vector<glm::vec3>& colors,
    std::vector<glm::vec3>& normals,
    std::vector<unsigned int>& indices)
{
    if (!dirty)
        return; // No rebuild needed

    vertices.clear();
    colors.clear();
    normals.clear();
    indices.clear();

    int indexOffset = 0;
    float isoLevel = 0.0f; // Surface threshold

    for (int x = 0; x < CHUNK_SIZE; ++x)
        for (int y = 0; y < CHUNK_HEIGHT; ++y)
            for (int z = 0; z < CHUNK_SIZE; ++z)
                polygoniseCube(x, y, z, vertices, colors, normals, indices, indexOffset, isoLevel);

    // Offset all vertices by chunk world position
    glm::vec3 offset(position.x * CHUNK_SIZE * VOXEL_SIZE,
        0.0f,
        position.y * CHUNK_SIZE * VOXEL_SIZE);

    for (auto& v : vertices)
        v += offset;

    dirty = false;
}

/* -------------------------- */
/* Generates chunk mesh data (vertices, colors, normals, indices) */
/* Returns true if mesh contains any vertices */
/* -------------------------- */
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

/* -------------------------- */
/* Finalizes mesh by uploading to GPU buffers */
/* Deletes existing mesh if any */
/* -------------------------- */
void Chunk::finalize(std::vector<glm::vec3>& vertices,
    std::vector<glm::vec3>& colors,
    std::vector<glm::vec3>& normals,
    std::vector<unsigned int>& indices)
{
    if (mesh)
        delete mesh;

    mesh = vertices.empty() ? nullptr : new Mesh(vertices, colors, normals, indices);
}

/* -------------------------- */
/* Draw the chunk's mesh */
/* -------------------------- */
void Chunk::draw(const Shader& shader)
{
    if (mesh)
        mesh->draw();
}