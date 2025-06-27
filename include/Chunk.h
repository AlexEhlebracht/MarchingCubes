#pragma once
#include <glm/glm.hpp>
#include <vector>
#include "Mesh.h"
#include <FastNoiseLite.h>

#define CHUNK_SIZE 32
#define CHUNK_HEIGHT 32
#define VOXEL_SIZE 8
#define BASE_HEIGHT 16
#define HEIGHT_VARIATION 16

class Chunk
{
public:
    Chunk(glm::ivec2 pos);
    ~Chunk();
    void draw(const Shader& shader);
    glm::ivec2 position;
    bool generateData(std::vector<glm::vec3>& vertices, std::vector<glm::vec3>& colors,
        std::vector<glm::vec3>& normals, std::vector<unsigned int>& indices);
    void finalize(std::vector<glm::vec3>& vertices, std::vector<glm::vec3>& colors,
        std::vector<glm::vec3>& normals, std::vector<unsigned int>& indices);

private:
    FastNoiseLite continentalNoise;
    FastNoiseLite hillNoise;
    FastNoiseLite detailNoise;
    float getPlainsHeight(float wx, float wz) const;
    float getPlainsNoise(float wx, float wz) const;
    std::vector<std::vector<std::vector<float>>> density;
    Mesh* mesh;
    bool dirty;
    float getDensityAt(int x, int y, int z);

    void generateDensityField();
    void buildMeshData(std::vector<glm::vec3>& vertices, std::vector<glm::vec3>& colors,
        std::vector<glm::vec3>& normals, std::vector<unsigned int>& indices);
    void polygoniseCube(int x, int y, int z,
        std::vector<glm::vec3>& vertices,
        std::vector<glm::vec3>& colors,
        std::vector<glm::vec3>& normals,
        std::vector<unsigned int>& indices,
        int& indexOffset,
        float isoLevel);
};