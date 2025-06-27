#include "../include/World.h"
#include <GLFW/glfw3.h>
#include <iostream>
#include <glm/gtc/matrix_access.hpp>

#define LOAD_RADIUS 4
#define UNLOAD_RADIUS 5

/* ------------------------- */
/* World Constructor / Destructor */
/* ------------------------- */
World::World()
    : lastUpdateTime(0.0f), running(true), lastCameraChunk(0)
{
    // Launch worker threads equal to hardware concurrency
    const int numThreads = std::max(1u, std::thread::hardware_concurrency());
    for (int i = 0; i < numThreads; ++i)
    {
        workers.emplace_back(&World::workerThread, this);
    }

    // Initialize by updating at origin
    update(glm::vec3(0));
}

World::~World()
{
    // Signal workers to stop and join threads
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

    // Clean up all chunks
    for (auto& entry : chunks)
        delete entry.second;
    chunks.clear();
}

/* ------------------------- */
/* Update world: manage chunks loading/unloading */
/* ------------------------- */
void World::update(const glm::vec3& cameraPos)
{
    float currentTime = glfwGetTime();

    // Limit update frequency to 5Hz (every 0.2s)
    if (currentTime - lastUpdateTime < 0.2f)
        return;

    lastUpdateTime = currentTime;

    // Determine which chunk the camera is currently in
    glm::ivec2 cameraChunk = glm::floor(glm::vec2(cameraPos.x, cameraPos.z) / float(CHUNK_SIZE * VOXEL_SIZE));

    // Queue new chunks or unload distant ones only if camera chunk changed or no chunks loaded
    if (cameraChunk != lastCameraChunk || chunks.empty())
    {
        queueChunks(cameraChunk, cameraPos);
        unloadChunks(cameraChunk);
        lastCameraChunk = cameraChunk;
    }

    // Process chunks completed by worker threads
    processCompletedChunks();
}

/* ------------------------- */
/* Add nearby chunks to the task queue for generation */
/* ------------------------- */
void World::queueChunks(const glm::ivec2& centerChunk, const glm::vec3& cameraPos)
{
    std::lock_guard<std::mutex> lock(taskMutex);

    for (int x = -LOAD_RADIUS; x <= LOAD_RADIUS; ++x)
    {
        for (int z = -LOAD_RADIUS; z <= LOAD_RADIUS; ++z)
        {
            glm::ivec2 pos = centerChunk + glm::ivec2(x, z);
            int distanceGrid = std::max(std::abs(x), std::abs(z));
            if (distanceGrid > LOAD_RADIUS) continue;

            if (chunks.find(pos) == chunks.end())
            {
                // Calculate chunk center position in world space
                glm::vec3 chunkCenter(
                    pos.x * CHUNK_SIZE + CHUNK_SIZE / 2.0f,
                    CHUNK_HEIGHT / 2.0f,
                    pos.y * CHUNK_SIZE + CHUNK_SIZE / 2.0f);

                float dist = glm::distance(cameraPos, chunkCenter);
                taskQueue.push({ pos, dist });
            }
        }
    }

    taskCondition.notify_one();  // Wake one worker thread
}

/* ------------------------- */
/* Worker thread function: generates chunk mesh data */
/* ------------------------- */
void World::workerThread()
{
    while (true)
    {
        glm::ivec2 pos;

        // Wait for task or shutdown signal
        {
            std::unique_lock<std::mutex> lock(taskMutex);
            taskCondition.wait(lock, [this] { return !taskQueue.empty() || !running; });

            if (!running && taskQueue.empty())
                return;

            pos = taskQueue.top().pos;
            taskQueue.pop();
        }

        // Create and generate chunk data
        Chunk* chunk = new Chunk(pos);

        std::vector<glm::vec3> vertices, colors, normals;
        std::vector<unsigned int> indices;
        bool hasMesh = chunk->generateData(vertices, colors, normals, indices);

        // Store completed chunk data for finalization in main thread
        {
            std::lock_guard<std::mutex> lock(completedMutex);
            completedChunks.push({ pos, chunk, vertices, colors, normals, indices, hasMesh });
        }
    }
}

/* ------------------------- */
/* Finalize and upload completed chunk mesh data */
/* ------------------------- */
void World::processCompletedChunks()
{
    std::queue<ChunkData> tempQueue;

    // Move all completed chunks into temporary queue (thread safe)
    {
        std::lock_guard<std::mutex> lock(completedMutex);
        tempQueue = std::move(completedChunks);
        completedChunks = std::queue<ChunkData>();
    }

    int finalizedThisFrame = 0;

    // Finalize up to maxFinalizePerFrame chunks this frame
    while (!tempQueue.empty() && finalizedThisFrame < maxFinalizePerFrame)
    {
        ChunkData data = tempQueue.front();
        tempQueue.pop();

        if (chunks.find(data.pos) == chunks.end())
        {
            if (data.hasMesh)
            {
                data.chunk->finalize(data.vertices, data.colors, data.normals, data.indices);
                chunks[data.pos] = data.chunk;
                finalizedThisFrame++;
            }
            else
            {
                delete data.chunk;  // Discard empty chunk
            }
        }
        else
        {
            delete data.chunk;  // Chunk already exists, discard duplicate
        }
    }

    // Return remaining chunks to completed queue for next frame
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

/* ------------------------- */
/* Unload chunks far from camera to free memory */
/* ------------------------- */
void World::unloadChunks(const glm::ivec2& centerChunk)
{
    std::vector<glm::ivec2> toRemove;

    // Find chunks beyond unload radius
    for (const auto& entry : chunks)
    {
        glm::ivec2 pos = entry.first;
        int distance = std::max(std::abs(pos.x - centerChunk.x), std::abs(pos.y - centerChunk.y));
        if (distance > UNLOAD_RADIUS)
            toRemove.push_back(pos);
    }

    // Delete and remove those chunks
    for (const auto& pos : toRemove)
    {
        delete chunks[pos];
        chunks.erase(pos);
    }
}

/* ------------------------- */
/* Check if a chunk is within the camera's view frustum */
/* ------------------------- */
bool World::isChunkInFrustum(const glm::ivec2& pos, const glm::mat4& viewProj)
{
    const float SHRINK = 1.0f;  // <1 means tighter culling, 1.0 means normal

    // Define chunk bounding box in world space
    glm::vec3 minCorner(pos.x * CHUNK_SIZE * VOXEL_SIZE, 0.0f,
        pos.y * CHUNK_SIZE * VOXEL_SIZE);
    glm::vec3 maxCorner = minCorner + glm::vec3(CHUNK_SIZE, CHUNK_HEIGHT, CHUNK_SIZE) * float(VOXEL_SIZE);

    glm::vec3 center = (minCorner + maxCorner) * 0.5f;
    glm::vec3 extents = (maxCorner - minCorner) * 0.5f;

    // Extract frustum planes from view-projection matrix
    glm::vec4 planes[6];
    planes[0] = glm::row(viewProj, 3) + glm::row(viewProj, 0); // Left
    planes[1] = glm::row(viewProj, 3) - glm::row(viewProj, 0); // Right
    planes[2] = glm::row(viewProj, 3) + glm::row(viewProj, 1); // Bottom
    planes[3] = glm::row(viewProj, 3) - glm::row(viewProj, 1); // Top
    planes[4] = glm::row(viewProj, 3) + glm::row(viewProj, 2); // Near
    planes[5] = glm::row(viewProj, 3) - glm::row(viewProj, 2); // Far

    // Normalize planes
    for (int i = 0; i < 6; ++i)
    {
        float length = glm::length(glm::vec3(planes[i]));
        if (length > 0.0001f)
            planes[i] /= length;
    }

    // Test each plane against the chunk's bounding box
    for (int i = 0; i < 6; ++i)
    {
        glm::vec3 n = glm::vec3(planes[i]);
        float d = planes[i].w;

        // Projected radius of the box onto plane normal, scaled by SHRINK
        float r = (extents.x * std::abs(n.x) +
            extents.y * std::abs(n.y) +
            extents.z * std::abs(n.z)) * SHRINK;

        // Signed distance from box center to plane
        float s = glm::dot(n, center) + d;

        // If completely outside this plane, cull
        if (s + r < 0.0f)
            return false;
    }

    return true; // Intersects or is inside the frustum
}

/* ------------------------- */
/* Render all visible chunks */
/* ------------------------- */
void World::draw(const Shader& shader, const glm::vec3& cameraPos, const glm::mat4& view, const glm::mat4& projection)
{
    glm::mat4 viewProj = projection * view;

    for (auto& entry : chunks)
    {
        if (isChunkInFrustum(entry.first, viewProj))
        {
            entry.second->draw(shader);
        }
        else
        {
            // Could add optional chunk LOD or skip rendering silently
        }
    }
}
