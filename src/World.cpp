#include "../include/World.h"
#include <GLFW/glfw3.h>
#include <iostream>
#include <glm/gtc/matrix_access.hpp>

#define LOAD_RADIUS 8
#define UNLOAD_RADIUS 10

World::World() : lastUpdateTime(0.0f), running(true)
{
    lastCameraChunk = glm::ivec2(0);

    // Start worker threads (use full hardware concurrency)
    const int numThreads = std::max(1u, std::thread::hardware_concurrency());
    for (int i = 0; i < numThreads; ++i)
    {
        workers.emplace_back(&World::workerThread, this);
    }

    update(glm::vec3(0));
}

World::~World()
{
    {
        std::lock_guard<std::mutex> lock(taskMutex);
        running = false;
    }
    taskCondition.notify_all();
    for (auto& worker : workers)
    {
        if (worker.joinable())
            worker.join();
    }
}

void World::update(const glm::vec3& cameraPos)
{
    float currentTime = glfwGetTime();
    if (currentTime - lastUpdateTime < 0.2f)
        return;
    lastUpdateTime = currentTime;

    glm::ivec2 cameraChunk = glm::floor(glm::vec2(cameraPos.x, cameraPos.z) / float(CHUNK_SIZE * VOXEL_SIZE));
    if (cameraChunk != lastCameraChunk || chunks.empty())
    {
        queueChunks(cameraChunk, cameraPos);
        unloadChunks(cameraChunk);
        lastCameraChunk = cameraChunk;
    }

    processCompletedChunks();
}

void World::queueChunks(const glm::ivec2& centerChunk, const glm::vec3& cameraPos)
{
    std::lock_guard<std::mutex> lock(taskMutex);
    for (int x = -LOAD_RADIUS; x <= LOAD_RADIUS; ++x)
        for (int z = -LOAD_RADIUS; z <= LOAD_RADIUS; ++z)
        {
            glm::ivec2 pos = centerChunk + glm::ivec2(x, z);
            int distance = std::max(std::abs(x), std::abs(z));
            if (distance > LOAD_RADIUS) continue;

            if (chunks.find(pos) == chunks.end())
            {
                // Calculate world position of chunk center
                glm::vec3 chunkCenter = glm::vec3(pos.x * CHUNK_SIZE + CHUNK_SIZE / 2.0f,
                    CHUNK_HEIGHT / 2.0f,
                    pos.y * CHUNK_SIZE + CHUNK_SIZE / 2.0f);
                float dist = glm::distance(cameraPos, chunkCenter);
                taskQueue.push({ pos, dist });
            }
        }
    taskCondition.notify_one();
}

void World::workerThread()
{
    while (true)
    {
        glm::ivec2 pos;
        {
            std::unique_lock<std::mutex> lock(taskMutex);
            taskCondition.wait(lock, [this] { return !taskQueue.empty() || !running; });
            if (!running && taskQueue.empty())
                return;
            pos = taskQueue.top().pos;
            taskQueue.pop();
        }

        Chunk* chunk = new Chunk(pos);

        std::vector<glm::vec3> vertices, colors, normals;
        std::vector<unsigned int> indices;
        bool hasMesh = chunk->generateData(vertices, colors, normals, indices);

        // Store completed data
        {
            std::lock_guard<std::mutex> lock(completedMutex);
            completedChunks.push({ pos, chunk, vertices, colors, normals, indices, hasMesh });
        }
    }
}

void World::processCompletedChunks()
{
    std::queue<ChunkData> tempQueue;
    {
        std::lock_guard<std::mutex> lock(completedMutex);
        tempQueue = std::move(completedChunks);
        completedChunks = std::queue<ChunkData>();
    }

    int finalizedThisFrame = 0;
    while (!tempQueue.empty() && finalizedThisFrame < maxFinalizePerFrame)
    {
        ChunkData data = tempQueue.front();
        tempQueue.pop();

        if (chunks.find(data.pos) == chunks.end())
        {
            if (data.hasMesh) // Skip empty chunks
            {
                data.chunk->finalize(data.vertices, data.colors, data.normals, data.indices);
                chunks[data.pos] = data.chunk;
                finalizedThisFrame++;
            }
            else
            {
                delete data.chunk; // Discard empty chunk
            }
        }
        else
        {
            delete data.chunk; // Chunk already exists, discard
        }
    }

    // Push remaining chunks back to completedChunks
    if (!tempQueue.empty())
    {
        std::lock_guard<std::mutex> lock(completedMutex);
        while (!tempQueue.empty())
        {
            completedChunks.push(tempQueue.front());
            tempQueue.pop();
        }
    }
}

void World::unloadChunks(const glm::ivec2& centerChunk)
{
    std::vector<glm::ivec2> toRemove;
    for (const auto& entry : chunks)
    {
        glm::ivec2 pos = entry.first;
        int distance = std::max(std::abs(pos.x - centerChunk.x),
            std::abs(pos.y - centerChunk.y));
        if (distance > UNLOAD_RADIUS)
            toRemove.push_back(pos);
    }

    for (const auto& pos : toRemove)
    {
        delete chunks[pos];
        chunks.erase(pos);
    }
}

bool World::isChunkInFrustum(const glm::ivec2& pos, const glm::vec3& cameraPos, const glm::mat4& viewProj)
{
    // Define chunk AABB in world space
    glm::vec3 minCorner(pos.x * CHUNK_SIZE * VOXEL_SIZE, 0.0f,
        pos.y * CHUNK_SIZE * VOXEL_SIZE);
    glm::vec3 maxCorner = minCorner +
        glm::vec3(CHUNK_SIZE, CHUNK_HEIGHT, CHUNK_SIZE) * float(VOXEL_SIZE);

    // 8 corners of the chunk's AABB
    glm::vec3 corners[8] = {
        minCorner,
        { maxCorner.x, minCorner.y, minCorner.z },
        { minCorner.x, maxCorner.y, minCorner.z },
        { maxCorner.x, maxCorner.y, minCorner.z },
        { minCorner.x, minCorner.y, maxCorner.z },
        { maxCorner.x, minCorner.y, maxCorner.z },
        { minCorner.x, maxCorner.y, maxCorner.z },
        maxCorner
    };

    // Extract frustum planes from view-projection matrix
    glm::vec4 planes[6];
    // Left: row3 + row1
    planes[0] = glm::row(viewProj, 3) + glm::row(viewProj, 0);
    // Right: row3 - row1
    planes[1] = glm::row(viewProj, 3) - glm::row(viewProj, 0);
    // Bottom: row3 + row2
    planes[2] = glm::row(viewProj, 3) + glm::row(viewProj, 1);
    // Top: row3 - row2
    planes[3] = glm::row(viewProj, 3) - glm::row(viewProj, 1);
    // Near: row3 + row3 (assuming OpenGL convention)
    planes[4] = glm::row(viewProj, 3) + glm::row(viewProj, 2);
    // Far: row3 - row3
    planes[5] = glm::row(viewProj, 3) - glm::row(viewProj, 2);

    // Normalize planes (normals point outward)
    for (int i = 0; i < 6; ++i)
    {
        float length = glm::length(glm::vec3(planes[i]));
        if (length > 0.0001f)
            planes[i] /= length;
    }

    // Check if chunk is outside any frustum plane
    for (const auto& plane : planes)
    {
        bool inside = false;
        for (const auto& corner : corners)
        {
            // Add small margin (1.0f) to avoid edge-case culling
            if (glm::dot(plane, glm::vec4(corner, 1.0f)) + 1.0f >= 0)
            {
                inside = true;
                break;
            }
        }
        if (!inside)
            return false;
    }
    return true;
}

void World::draw(const Shader& shader, const glm::vec3& cameraPos, const glm::mat4& view, const glm::mat4& projection)
{
    glm::mat4 viewProj = projection * view;
    for (auto& entry : chunks)
    {
        if (isChunkInFrustum(entry.first, cameraPos, viewProj))
        {
            entry.second->draw(shader);
        }
        else
        {
        }
    }
}