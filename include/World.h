#pragma once

#include <glm/glm.hpp>
#include <unordered_map>
#include <queue>
#include <vector>
#include <thread>
#include <mutex>
#include <condition_variable>
#include "Chunk.h"

/* ------------------------------------------------------------ */
/* Custom hash function for glm::ivec2 to use in unordered_map */
/* ------------------------------------------------------------ */
struct Vec2Hash
{
    std::size_t operator()(const glm::ivec2& v) const
    {
        return std::hash<int>()(v.x) ^ (std::hash<int>()(v.y) << 1);
    }
};

/* ------------------------------------------- */
/* Data container for completed chunk mesh data */
/* ------------------------------------------- */
struct ChunkData
{
    glm::ivec2 pos;                          // Chunk position (grid coords)
    Chunk* chunk;                           // Pointer to the chunk object
    std::vector<glm::vec3> vertices;       // Mesh vertex positions
    std::vector<glm::vec3> colors;         // Vertex colors
    std::vector<glm::vec3> normals;        // Vertex normals
    std::vector<unsigned int> indices;     // Triangle indices
    bool hasMesh = false;                   // True if mesh data is valid
};

/* -------------------------------------------- */
/* Task struct used for chunk loading prioritization */
/* -------------------------------------------- */
struct ChunkTask
{
    glm::ivec2 pos;     // Chunk position to process
    float distance;     // Distance from camera (priority key)

    // Priority comparison: smaller distance = higher priority
    bool operator<(const ChunkTask& other) const
    {
        return distance > other.distance;
    }
};

/* ------------------- */
/* World class manages chunks, multithreading, and rendering */
/* ------------------- */
class World
{
public:
    World();
    ~World();

    // Update world state, loading/unloading chunks as needed
    void update(const glm::vec3& cameraPos);

    // Render all loaded chunks
    void draw(const Shader& shader,
        const glm::vec3& cameraPos,
        const glm::mat4& view,
        const glm::mat4& projection);

    // Map of chunk positions to chunk pointers
    std::unordered_map<glm::ivec2, Chunk*, Vec2Hash> chunks;

private:
    glm::ivec2 lastCameraChunk;            // Last chunk the camera was in
    float lastUpdateTime = 0.0f;           // Time of last update call

    std::priority_queue<ChunkTask> taskQueue; // Chunk processing tasks queue
    std::mutex taskMutex;                     // Mutex for task queue
    std::condition_variable taskCondition;   // Condition variable to wake worker threads

    std::vector<std::thread> workers;         // Worker threads for background chunk generation
    bool running = true;                       // Worker thread run control flag

    std::queue<ChunkData> completedChunks;    // Queue of chunks completed by workers
    std::mutex completedMutex;                 // Mutex for completed chunks queue

    const int maxFinalizePerFrame = 30;       // Max chunks finalized per frame

    // Add chunks near the camera to the processing queue
    void queueChunks(const glm::ivec2& centerChunk, const glm::vec3& cameraPos);

    // Unload chunks far from the camera
    void unloadChunks(const glm::ivec2& centerChunk);

    // Worker thread function: processes chunk generation tasks
    void workerThread();

    // Finalize and upload chunk mesh data from completed chunks
    void processCompletedChunks();

    // Frustum culling helper to check if chunk is visible
    bool isChunkInFrustum(const glm::ivec2& pos,
        const glm::mat4& viewProj);
};
