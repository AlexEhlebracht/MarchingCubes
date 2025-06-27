#pragma once
#include <glm/glm.hpp>
#include <vector>
#include "Mesh.h"
#include <FastNoiseLite.h>

// Configuration
#define CHUNK_SIZE 32
#define CHUNK_HEIGHT (256 / VOXEL_SIZE)
#define VOXEL_SIZE 8
#define DESIGN_VOXEL (VOXEL_SIZE)

// All heights are in world units
#define BASE_HEIGHT_WORLD (CHUNK_HEIGHT * VOXEL_SIZE / 2)
#define HEIGHT_VARIATION_WORLD (CHUNK_HEIGHT * VOXEL_SIZE / 4)
#define WATER_LEVEL_WORLD (BASE_HEIGHT_WORLD)

class Chunk
{
public:
    Chunk(glm::ivec2 pos);
    ~Chunk();
    void draw(const Shader& shader);

    bool generateData(std::vector<glm::vec3>& vertices,
        std::vector<glm::vec3>& colors,
        std::vector<glm::vec3>& normals,
        std::vector<unsigned int>& indices);

    void finalize(std::vector<glm::vec3>& vertices,
        std::vector<glm::vec3>& colors,
        std::vector<glm::vec3>& normals,
        std::vector<unsigned int>& indices);

    glm::ivec2 position;

private:
    FastNoiseLite continentalNoise;
    FastNoiseLite hillNoise;
    FastNoiseLite detailNoise;
    FastNoiseLite beachNoise;

    float voxelScale;

    std::vector<std::vector<std::vector<float>>> density;
    Mesh* mesh;
    bool dirty;

    float getPlainsNoise(float wx, float wz) const;
    float getPlainsHeight(float wx, float wz) const;
    float getDensityAt(int x, int y, int z);

    void generateDensityField();
    void buildMeshData(std::vector<glm::vec3>& vertices,
        std::vector<glm::vec3>& colors,
        std::vector<glm::vec3>& normals,
        std::vector<unsigned int>& indices);

    void polygoniseCube(int x, int y, int z,
        std::vector<glm::vec3>& vertices,
        std::vector<glm::vec3>& colors,
        std::vector<glm::vec3>& normals,
        std::vector<unsigned int>& indices,
        int& indexOffset,
        float isoLevel);
};
