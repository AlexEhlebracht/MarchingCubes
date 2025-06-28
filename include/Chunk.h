#pragma once

#include <glm/glm.hpp>
#include <vector>
#include "Mesh.h"
#include "../include/BiomeManager.h"

/* ------------------------- */
/* Configuration constants */
/* ------------------------- */
#define VOXEL_SIZE 8
#define DESIGN_VOXEL (VOXEL_SIZE)  // Alias for voxel size (for design clarity)

#define CHUNK_SIZE 32
#define CHUNK_HEIGHT (256 / VOXEL_SIZE)  // Vertical size in voxels

// Heights measured in world units
#define BASE_HEIGHT_WORLD       (CHUNK_HEIGHT * VOXEL_SIZE / 2)
#define HEIGHT_VARIATION_WORLD  (CHUNK_HEIGHT * VOXEL_SIZE / 4)
#define WATER_LEVEL_WORLD       (BASE_HEIGHT_WORLD)

/* ------------------------- */
/* Chunk class: represents a voxel chunk with density field and mesh data */
/* Responsible for generating terrain data and mesh via marching cubes */
/* ------------------------- */
class Chunk
{
public:
    // Constructor takes chunk position in chunk coordinates (x,z)
    explicit Chunk(glm::ivec2 pos, const BiomeManager* biomeMgr);

    // Destructor cleans up allocated mesh
    ~Chunk();

    // Draws the mesh with provided shader
    void draw(const Shader& shader);

    // Generates mesh data arrays from density field
    // Returns true if mesh was generated, false if empty
    bool generateData(std::vector<glm::vec3>& vertices,
        std::vector<glm::vec3>& colors,
        std::vector<glm::vec3>& normals,
        std::vector<unsigned int>& indices);

    // Uploads vertex data to GPU buffers and prepares mesh for drawing
    void finalize(std::vector<glm::vec3>& vertices,
        std::vector<glm::vec3>& colors,
        std::vector<glm::vec3>& normals,
        std::vector<unsigned int>& indices);

    // Chunk position in chunk grid coordinates
    glm::ivec2 position;

private:
    // BiomeManager to know what biome the chunk is
    const BiomeManager* biome;

    // 3D density field: density[x][y][z]
    std::vector<std::vector<std::vector<float>>> density;

    Mesh* mesh;      // Mesh object containing vertex buffers, etc.
    bool dirty;      // Flag indicating mesh needs rebuilding

    // Retrieves density value at voxel coordinates (including boundary)
    float getDensityAt(int x, int y, int z);

    // Fills the density field based on procedural noise functions
    void generateDensityField();

    // Builds mesh vertex/index data using marching cubes polygonization
    void buildMeshData(std::vector<glm::vec3>& vertices,
        std::vector<glm::vec3>& colors,
        std::vector<glm::vec3>& normals,
        std::vector<unsigned int>& indices);

    // Runs marching cubes on a single cube within the density field
    void polygoniseCube(int x, int y, int z,
        std::vector<glm::vec3>& vertices,
        std::vector<glm::vec3>& colors,
        std::vector<glm::vec3>& normals,
        std::vector<unsigned int>& indices,
        int& indexOffset,
        float isoLevel);
};