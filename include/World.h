#pragma once
#include <glm/glm.hpp>
#include <unordered_map>
#include <queue>
#include <vector>
#include <thread>
#include <mutex>
#include <condition_variable>
#include "Chunk.h"

struct Vec2Hash
{
    std::size_t operator()(const glm::ivec2& v) const
    {
        return std::hash<int>()(v.x) ^ (std::hash<int>()(v.y) << 1);
    }
};

// Struct to hold chunk data after CPU processing
struct ChunkData
{
    glm::ivec2 pos;
    Chunk* chunk;
    std::vector<glm::vec3> vertices;
    std::vector<glm::vec3> colors;
    std::vector<glm::vec3> normals;
    std::vector<unsigned int> indices;
    bool hasMesh;
};

// Struct for priority queue: prioritizes chunks closer to camera
struct ChunkTask
{
    glm::ivec2 pos;
    float distance;
    bool operator<(const ChunkTask& other) const
    {
        return distance > other.distance; // Smaller distance = higher priority
    }
};

class World
{
public:
    World();
    ~World();
    void update(const glm::vec3& cameraPos);
    void draw(const Shader& shader, const glm::vec3& cameraPos, const glm::mat4& view, const glm::mat4& projection);
    std::unordered_map<glm::ivec2, Chunk*, Vec2Hash> chunks;

private:
    glm::ivec2 lastCameraChunk;
    float lastUpdateTime;
    std::priority_queue<ChunkTask> taskQueue;
    std::mutex taskMutex;
    std::condition_variable taskCondition;
    std::vector<std::thread> workers;
    bool running;
    std::queue<ChunkData> completedChunks;
    std::mutex completedMutex;
    const int maxFinalizePerFrame = 30; // Reduced from 2

    void queueChunks(const glm::ivec2& centerChunk, const glm::vec3& cameraPos);
    void unloadChunks(const glm::ivec2& centerChunk);
    void workerThread();
    void processCompletedChunks();
    bool isChunkInFrustum(const glm::ivec2& pos, const glm::vec3& cameraPos, const glm::mat4& viewProj);
};