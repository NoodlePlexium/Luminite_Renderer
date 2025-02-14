#pragma once

// EXTERNAL LIBRARIES
#include <GL/glew.h>

// STANDARD LIBRARY
#include <chrono>
#include <queue>
#include <iostream>

// PROJECT HEADERS
#include "luminite_camera.h"
#include "luminite_quad_renderer.h"

struct PathVertex
{
    alignas(16) glm::vec3 surfacePosition;
    alignas(16) glm::vec3 surfaceNormal;
    alignas(16) glm::vec3 surfaceColour;
    alignas(16) glm::vec3 reflectedDir;
    alignas(16) glm::vec3 outgoingLight;
    alignas(16) glm::vec3 directLight;
    float surfaceRoughness;
    float surfaceEmission;
    float IOR;
    int refractive;
    int hitSky;
    int inside;
    int refracted;
    int cachedDirectLight;
};

struct RenderTile
{
    int x;
    int y;
    int width;
    int height;
    float estimatedTime;

    RenderTile ()
    {
        x = 0;
        y = 0;
        width = 0;
        height = 0;
        estimatedTime = 0;
    }
};

class RenderSystem
{
public:

    RenderSystem(int width, int height)
    {   
        VIEWPORT_WIDTH = width;
        VIEWPORT_HEIGHT = height;

        int SCA_W = static_cast<int>(static_cast<float>(VIEWPORT_WIDTH) * resolutionScale);
        int SCA_H = static_cast<int>(static_cast<float>(VIEWPORT_HEIGHT) * resolutionScale);

        qRenderer.PrepareQuadShader();
        qRenderer.CreateFrameBuffer(VIEWPORT_WIDTH, VIEWPORT_HEIGHT);
        
        // RENDER TEXTURE SETUP
        glGenTextures(1, &RenderTexture);
        glBindTexture(GL_TEXTURE_2D, RenderTexture);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F, SCA_W, SCA_H, 0, GL_RGBA, GL_FLOAT, nullptr);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glBindTexture(GL_TEXTURE_2D, 0);

        // DISPLAY TEXTURE SETUP
        glGenTextures(1, &DisplayTexture);
        glBindTexture(GL_TEXTURE_2D, DisplayTexture);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, SCA_W, SCA_H, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glBindTexture(GL_TEXTURE_2D, 0);

        // CAMERA PATH BUFFER
        uint32_t cameraPathVertexCount = SCA_W * SCA_H * (cameraBounces+1);
        glGenBuffers(1, &cameraPathVertexBuffer);
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, cameraPathVertexBuffer);
        glBufferData(GL_SHADER_STORAGE_BUFFER, sizeof(PathVertex) * cameraPathVertexCount, nullptr, GL_DYNAMIC_DRAW);
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 12, cameraPathVertexBuffer);

        // LIGHT PATH BUFFER
        uint32_t lightPathVertexCount = SCA_W * SCA_H * (lightBounces+2);
        glGenBuffers(1, &lightPathVertexBuffer);
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, lightPathVertexBuffer);
        glBufferData(GL_SHADER_STORAGE_BUFFER, sizeof(PathVertex) * lightPathVertexCount, nullptr, GL_DYNAMIC_DRAW);
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 13, lightPathVertexBuffer);

        // RESERVE SPACE FOR GROUP TIMES
        uint32_t tilesX = static_cast<uint32_t>((static_cast<float>(SCA_W) + 32) / 32);
        uint32_t tilesY =  static_cast<uint32_t>((static_cast<float>(SCA_H) + 32) / 32);
        groupTimes.reserve(tilesX * tilesY);
    }

    ~RenderSystem()
    {
        glDeleteBuffers(1, &RenderTexture);
        glDeleteBuffers(1, &DisplayTexture);
        glDeleteBuffers(1, &cameraPathVertexBuffer);
        glDeleteBuffers(1, &lightPathVertexBuffer);
    }

    void ResizeFramebuffer(int width, int height)
    {
        VIEWPORT_WIDTH = width;
        VIEWPORT_HEIGHT = height;

        int SCA_W = static_cast<int>(static_cast<float>(VIEWPORT_WIDTH) * resolutionScale);
        int SCA_H = static_cast<int>(static_cast<float>(VIEWPORT_HEIGHT) * resolutionScale);

        qRenderer.ResizeFramebuffer(VIEWPORT_WIDTH, VIEWPORT_HEIGHT);

        // CLEAR TEXTURES
        glm::vec4 blackColor(0.0f, 0.0f, 0.0f, 0.0f);
        glClearTexImage(RenderTexture, 0, GL_RGBA, GL_FLOAT, &blackColor);
        glClearTexImage(DisplayTexture, 0, GL_RGBA, GL_UNSIGNED_BYTE, &blackColor);

        // RESIZE RENDER TEXTURE
        glBindTexture(GL_TEXTURE_2D, RenderTexture);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F, SCA_W, SCA_H, 0, GL_RGBA, GL_FLOAT, nullptr);
        glBindTexture(GL_TEXTURE_2D, 0);

        // RESIZE DISPLAY TEXTURE
        glBindTexture(GL_TEXTURE_2D, DisplayTexture);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, SCA_W, SCA_H, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
        glBindTexture(GL_TEXTURE_2D, 0);

        // RESIZE CAMERA PATH VERTEX BUFFER
        uint32_t cameraPathVertexCount = SCA_W * SCA_H * (cameraBounces+1);
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, cameraPathVertexBuffer);
        glBufferData(GL_SHADER_STORAGE_BUFFER, sizeof(PathVertex) * cameraPathVertexCount, nullptr, GL_DYNAMIC_DRAW);
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 10, cameraPathVertexBuffer);

        // RESIZE LIGHT PATH VERTEX BUFFER
        uint32_t lightPathVertexCount = SCA_W * SCA_H * (lightBounces+2);
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, lightPathVertexBuffer);
        glBufferData(GL_SHADER_STORAGE_BUFFER, sizeof(PathVertex) * lightPathVertexCount, nullptr, GL_DYNAMIC_DRAW);
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 11, lightPathVertexBuffer);

        // RESERVE SPACE FOR GROUP TIMES
        uint32_t tilesX = static_cast<uint32_t>((static_cast<float>(SCA_W) + 32) / 32);
        uint32_t tilesY =  static_cast<uint32_t>((static_cast<float>(SCA_H) + 32) / 32);
        groupTimes.clear();
        groupTimes.reserve(tilesX * tilesY);

        // EMPTY TILE QUEUE
        TileQueue.clear();
        accumulationFrame = 0;
        frameCount = 0;
    }

    void RestartRender()
    {
        TileQueue.clear();
        accumulationFrame = 0;
        frameCount = 0;
    }

    void SetRendererDynamic()
    {
        if (dynamicScene == false)
        {
            dynamicScene = true;

            // SAVE CURRENT SETTINGS
            revert_resolutionScale = resolutionScale;
            revert_cameraBoucnes = cameraBounces;

            // OPTIMISE THE FRAMERATE
            resolutionScale = 0.2f;
            cameraBounces = 1;

            // RESIZE IMAGES TO LOWER RESOLUTION
            ResizeFramebuffer(VIEWPORT_WIDTH, VIEWPORT_HEIGHT);
        }
    }

    void SetRendererStatic()
    {
        if (dynamicScene)
        {
            dynamicScene = false;

            // RESET RESOLUTION SCALE AND RENDER BUDGET
            resolutionScale = revert_resolutionScale;
            cameraBounces = revert_cameraBoucnes;

            // RESIZE IMAGES TO FULL RESOLUTION
            ResizeFramebuffer(VIEWPORT_WIDTH, VIEWPORT_HEIGHT);
        }
    }

    void PathtraceFrame(unsigned int pathtraceShader, Camera &camera)
    {
        auto startTime = std::chrono::high_resolution_clock::now();

        int SCA_W = static_cast<int>(static_cast<float>(VIEWPORT_WIDTH) * resolutionScale);
        int SCA_H = static_cast<int>(static_cast<float>(VIEWPORT_HEIGHT) * resolutionScale);

        glUseProgram(pathtraceShader);
        camera.UpdatePathtracerUniforms(); // CAMERA UNIFORM
        glUniform1ui(glGetUniformLocation(pathtraceShader, "u_frameCount"), frameCount); // FRAME COUNT FOR PSEUDO RANDOMNESS
        glUniform1ui(glGetUniformLocation(pathtraceShader, "u_accumulationFrame"), accumulationFrame); // FRAME ACCUMULATION COUNT
        glUniform1ui(glGetUniformLocation(pathtraceShader, "u_debugMode"), 0); // DEBUG MODE
        glUniform1ui(glGetUniformLocation(pathtraceShader, "u_bounces"), cameraBounces); // CAMERA BOUNCES
        glUniform1ui(glGetUniformLocation(pathtraceShader, "u_light_bounces"), lightBounces); // LIGHT BOUCNES
        glUniform1f(glGetUniformLocation(pathtraceShader, "u_resolution_scale"), resolutionScale); // RESOLUTION SCALE
        glBindImageTexture(0, RenderTexture, 0, GL_FALSE, 0, GL_READ_WRITE, GL_RGBA32F); // RENDER TEXTURE
        glBindImageTexture(1, DisplayTexture, 0, GL_FALSE, 0, GL_READ_WRITE, GL_RGBA8); // DISPLAY TEXTURE

        uint32_t tilesX = static_cast<uint32_t>((static_cast<float>(SCA_W) + 32) / 32);
        uint32_t tilesY =  static_cast<uint32_t>((static_cast<float>(SCA_H) + 32) / 32);

        if (TileQueue.empty())
        {
            ScheduleRenderTiles(tilesX, tilesY, accumulationFrame);
        }

        while (!TileQueue.empty())
        {
            const RenderTile &tile = TileQueue.front();

            // UPDATE TILE OFFSET UNIFORM
            glUniform1ui(glGetUniformLocation(pathtraceShader, "u_tileX"), tile.x);
            glUniform1ui(glGetUniformLocation(pathtraceShader, "u_tileY"), tile.y);

            // RENDER TILE SEGMENT OF IMAGE
            auto dispatchStartTime = std::chrono::high_resolution_clock::now();
            glDispatchCompute(tile.width, tile.height, 1);
            glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);
            glFinish();
            auto dispatchEndTime = std::chrono::high_resolution_clock::now();
            float dispatchDuration = std::chrono::duration_cast<std::chrono::nanoseconds>(dispatchEndTime - dispatchStartTime).count() / 1000000.0f;

            // SET GROUP TIMES
            if (!dynamicScene)
            {
                float timePerGroup = dispatchDuration / (tile.width * tile.height);
                for (int y=0; y<tile.height; y++) for (int x=0; x<tile.width; x++)
                {
                    int groupIndex = (y + tile.y) * tilesX + (x + tile.x);
                    groupTimes[groupIndex] = timePerGroup;
                }
            }

            // REMOVE TILE FROM QUEUE
            TileQueue.erase(TileQueue.begin());


            auto now = std::chrono::high_resolution_clock::now();
            float totalDuration = std::chrono::duration_cast<std::chrono::milliseconds>(now - startTime).count();
            if (totalDuration + dispatchDuration >= renderBudget) break; // STOP RENDERING AFTER 16 MILLISECONDS
        }

        if (TileQueue.empty()) {
            accumulationFrame += 1;
            frameCount += 1;
        }
    }

    void RenderToViewport()
    {
        qRenderer.RenderToViewport(DisplayTexture);
    }

    unsigned int GetFrameBufferTextureID()
    {
        return qRenderer.GetFrameBufferTextureID();
    }

    uint32_t accumulationFrame = 0;
private:

    float renderBudget = 15;
    float resolutionScale = 1.0f;
    uint32_t cameraBounces = 3;
    uint32_t lightBounces = 2;
    uint32_t frameCount = 0;

    QuadRenderer qRenderer;
    int VIEWPORT_WIDTH;
    int VIEWPORT_HEIGHT;
    unsigned int DisplayTexture;
    unsigned int RenderTexture;
    unsigned int cameraPathVertexBuffer;
    unsigned int lightPathVertexBuffer;
    std::vector<RenderTile> TileQueue;

    std::vector<float> groupTimes;

    // DYNAMIC SCENES
    bool dynamicScene = false;
    float revert_resolutionScale;
    float revert_cameraBoucnes;


    void ScheduleRenderTiles(int x_blocks, int y_blocks, uint32_t accumulationFrame)
    {
        if (dynamicScene) 
        {
            RenderTile tile;
            tile.width = x_blocks;
            tile.height = y_blocks;
            TileQueue.push_back(tile);
        }
        else if (accumulationFrame == 0) // INITIAL TILE WIDTH
        {
            int tileWidth = 3;

            // CREATE SCHEDULE QUEUE
            int x = 0;
            int y = 0;
            while (y < y_blocks)
            {
                // CREATE NEW RENDER TILE
                RenderTile tile;
                tile.width = std::min(tileWidth, x_blocks - x);
                tile.height = std::min(tileWidth, y_blocks - y);
                tile.x = x;
                tile.y = y; 

                // ADD RENDER TILE TO QUEUE
                TileQueue.push_back(tile);

                // INCREMENT X
                x += tile.width;

                // MOVE TO NEXT ROW
                if (x >= x_blocks)
                {
                    x = 0;
                    y += tile.height;
                }
            }
        }
        else 
        { 
            int x = 0;
            int y = 0;
            while (true)
            {
                // CREATE NEW RENDER TILE
                std::tuple<int, int> coords = FindAvailableTileSpace(x_blocks, y_blocks);
                x = std::get<0>(coords);
                y = std::get<1>(coords);
                if (x >= x_blocks || y >= y_blocks) break;

                RenderTile tile;
                tile.estimatedTime = 0;
                tile.width = 1;
                tile.height = 1;
                tile.x = std::get<0>(coords);
                tile.y = std::get<1>(coords);

                GrowTile(tile, x_blocks, y_blocks);
                TileQueue.push_back(tile);
            }
        }
    }
    bool isHorizontalSpace(int x, int y, int width, int y_blocks)
    {
        if (y >= y_blocks) return false;

        for (int t=0; t<TileQueue.size(); t++)
        {
            const RenderTile &tile = TileQueue[t];
            bool horizontaOverlap = tile.x < x + width && tile.x + tile.width > x;
            bool verticalOverlap = tile.y < y + 1 && tile.y + tile.height > y;
            if (horizontaOverlap && verticalOverlap) return false;
        }
        return true;
    }

    bool isVerticalSpace(int x, int y, int height, int x_blocks)
    {
        if (x >= x_blocks) return false;

        for (int t=0; t<TileQueue.size(); t++)
        {
            const RenderTile &tile = TileQueue[t];
            bool horizontaOverlap = tile.x < x + 1 && tile.x + tile.width > x;
            bool verticalOverlap = tile.y < y + height && tile.y + tile.height > y;
            if (horizontaOverlap && verticalOverlap) return false;
        }
        return true;
    }

    float GetHorizontalTime(int x, int y, int width, int x_blocks)
    {
        float totalTime = 0;
        for (int i=0; i<width; i++)
        {
            totalTime += groupTimes[y * x_blocks + (i + x)];
        }
        return totalTime;
    }

    float GetVerticalTime(int x, int y, int height, int x_blocks)
    {
        float totalTime = 0;
        for (int i=0; i<height; i++)
        {
            totalTime += groupTimes[(y + i) * x_blocks + x];
        }
        return totalTime;
    }

    void GrowTile(RenderTile &tile, int x_blocks, int y_blocks)
    {
        while (tile.estimatedTime < renderBudget)
        {
            bool hSpace = isHorizontalSpace(tile.x, tile.y + tile.height, tile.width, y_blocks);
            bool vSpace = isVerticalSpace(tile.x + tile.width, tile.y, tile.width, x_blocks);

            if (hSpace)
            {
                float hTime = GetHorizontalTime(tile.x, tile.y + tile.height, tile.width, x_blocks);
                if (tile.estimatedTime + hTime < renderBudget) {
                    tile.height += 1;
                    tile.estimatedTime += hTime;
                }
                else
                {
                    break;
                }
            }

            if (vSpace)
            {
                float vTime = GetVerticalTime(tile.x + tile.width, tile.y, tile.height, x_blocks);
                if (tile.estimatedTime + vTime < renderBudget) {
                    tile.width += 1;
                    tile.estimatedTime += vTime;
                }
                else
                {
                    break;
                }
            }

            if (!hSpace && !vSpace) break;
        }
    }

    std::tuple<int, int> FindAvailableTileSpace(int x_blocks, int y_blocks)
    {
        int x = 0;
        int y = 0;
        if (TileQueue.size() == 0) { return std::make_tuple(0, 0); }

        while (y < y_blocks)
        {
            if (!isVerticalSpace(x, y, 1, x_blocks))
            {
                x += 1;
            }
            else
            {
                break;
            }
            if (x >= x_blocks)
            {
                x = 0;
                y += 1;
            }
        }
        return std::make_tuple(x, y);
    }
};



