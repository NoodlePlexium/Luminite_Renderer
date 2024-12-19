// EXTERNAL LIBRARIES
#include <backends/imgui_impl_opengl3.h>
#include <backends/imgui_impl_glfw.h>
#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <imgui.h>
#include "./glm/glm.hpp"

// STANDARD LIBRARY
#include <iostream>
#include <vector>
#include <string>
#include <chrono>

// PROJECT HEADERS
#include "pathtrace_quad_renderer.h"
#include "pathtrace_shader.h"
#include "pathtrace_mesh.h"
#include "pathtrace_debug.h"
#include "pathtrace_ui.h"

int main()
{
    float WIDTH = 640;
    float HEIGHT = 480;
    float VIEWPORT_WIDTH = WIDTH * 0.8f;
    float VIEWPORT_HEIGHT = HEIGHT * 0.8f;
    uint32_t frameCount = 0;
    uint32_t accumulationFrame = 0;
    float frameTime = 0.0f;

    // SETUP A GLFW WINDOW
    if (!glfwInit()) exit(EXIT_FAILURE);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    GLFWwindow* window = glfwCreateWindow(WIDTH, HEIGHT, "Raytrace", NULL, NULL);
    if (!window)
    {
        glfwTerminate();
        exit(EXIT_FAILURE);
    }
    glfwMakeContextCurrent(window);
    
    // SETUP IMGUI
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    ImGui::StyleColorsDark();
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init("#version 130");

    // GLEW INIT CHECK
    if (glewInit() != GLEW_OK) 
    {
        std::cerr << "Failed to initialize GLEW!" << std::endl;
        return -1;
    }

    // OPENGL VIEWPORT
    glViewport(0, 0, VIEWPORT_WIDTH, VIEWPORT_HEIGHT);

    // QUAD REDERING SHADER
    QuadRenderer qRenderer;
    qRenderer.PrepareQuadShader();
    qRenderer.CreateFrameBuffer((int)VIEWPORT_WIDTH, (int)VIEWPORT_HEIGHT);
    
    // PATH TRACING COMPUTE SHADER
    std::string pathtraceShaderSource = LoadShaderFromFile("./shaders/pathtrace.shader");
    unsigned int pathtraceShader = CreateComputeShader(pathtraceShaderSource);

    // RENDER TEXTURE SETUP
    unsigned int RenderTexture;
    glGenTextures(1, &RenderTexture);
    glBindTexture(GL_TEXTURE_2D, RenderTexture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, int(VIEWPORT_WIDTH), int(VIEWPORT_HEIGHT), 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glBindTexture(GL_TEXTURE_2D, 0);
    
    // CAMERA INFO UNIFORM
    unsigned int camInfoPosLocation = glGetUniformLocation(pathtraceShader, "cameraInfo.pos");
    unsigned int camInfoForwardLocation = glGetUniformLocation(pathtraceShader, "cameraInfo.forward");
    unsigned int camInfoRightLocation = glGetUniformLocation(pathtraceShader, "cameraInfo.right");
    unsigned int camInfoUpLocation = glGetUniformLocation(pathtraceShader, "cameraInfo.up");
    unsigned int camInfoFOVLocation = glGetUniformLocation(pathtraceShader, "cameraInfo.FOV");




    // }----------{ LOAD 3D MESHES }----------{
    std::vector<Mesh*> meshes;
    Mesh lion;
    Mesh left_wall;
    Mesh right_wall;
    Mesh back_wall;
    Mesh floor;
    Mesh roof;
    Mesh light_plane;
    lion.LoadOBJ("./models/merc.obj");
    left_wall.LoadOBJ("./models/left_wall.obj");
    right_wall.LoadOBJ("./models/right_wall.obj");
    back_wall.LoadOBJ("./models/back_wall.obj");
    floor.LoadOBJ("./models/floor.obj");
    roof.LoadOBJ("./models/roof.obj");

    

    left_wall.material.colour = glm::vec3(1.0f, 0.2f, 0.2f);
    left_wall.material.roughness = 0.0f;
    right_wall.material.colour = glm::vec3(0.2f, 1.0f, 0.2f);
    right_wall.material.roughness = 0.0f;
    floor.material.colour = glm::vec3(0.9f, 0.9f, 0.9f);
    floor.material.roughness = 0.0f;
    roof.material.emission = 10.0f;
    lion.material.roughness = 1.0f;
    // lion.material.roughness = 0.3f;
    // lion.material.colour = glm::vec3(0.2f, 0.2f, 1.0f);
    

    meshes.push_back(&lion);
    // meshes.push_back(&left_wall);
    // meshes.push_back(&right_wall);
    // meshes.push_back(&back_wall);
    // meshes.push_back(&floor);
    // meshes.push_back(&roof);
    // }----------{ LOAD 3D MESHES }----------{


    // }----------{ SEND MESH DATA TO THE GPU }----------{
    std::vector<MeshPartition> meshPartitions;
    uint32_t vertexStart = 0;
    uint32_t indexStart = 0;
    uint32_t materialIndex = 0;
    uint32_t bvhStart = 0;
    for (const Mesh* mesh : meshes) 
    {
        MeshPartition mPart;
        mPart.verticesStart = vertexStart;
        mPart.indicesStart = indexStart;
        mPart.materialIndex = materialIndex;
        mPart.bvhNodeStart = bvhStart;
        vertexStart += mesh->vertices.size();
        indexStart += mesh->indices.size();
        materialIndex += 1;
        bvhStart += mesh->nodesUsed;
        meshPartitions.push_back(mPart);
    }

    size_t vertexBufferSize = 0;
    size_t indexBufferSize = 0;
    size_t materialBufferSize = 0;
    size_t bvhBufferSize = 0;
    for (const Mesh* mesh : meshes) {
        vertexBufferSize += mesh->vertices.size() * sizeof(Vertex);
        indexBufferSize += mesh->indices.size() * sizeof(uint32_t);
        materialBufferSize += sizeof(Material);
        bvhBufferSize += mesh->nodesUsed * sizeof(BVH_Node);
    }

    glUseProgram(pathtraceShader);
    glUniform1i(glGetUniformLocation(pathtraceShader, "u_meshCount"), meshes.size());

    // VERTEX BUFFER
    unsigned int vertexBuffer;
    glGenBuffers(1, &vertexBuffer);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, vertexBuffer);
    glBufferStorage(GL_SHADER_STORAGE_BUFFER, vertexBufferSize, nullptr, GL_MAP_WRITE_BIT | GL_MAP_PERSISTENT_BIT | GL_MAP_COHERENT_BIT);  
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, vertexBuffer);
    void* mappedVertexBuffer = glMapBufferRange(GL_SHADER_STORAGE_BUFFER, 0, vertexBufferSize, GL_MAP_WRITE_BIT);

    // INDEX BUFFER
    unsigned int indexBuffer;
    glGenBuffers(1, &indexBuffer);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, indexBuffer);
    glBufferStorage(GL_SHADER_STORAGE_BUFFER, indexBufferSize, nullptr, GL_MAP_WRITE_BIT | GL_MAP_PERSISTENT_BIT | GL_MAP_COHERENT_BIT);  
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 2, indexBuffer);
    void* mappedIndexBuffer = glMapBufferRange(GL_SHADER_STORAGE_BUFFER, 0, indexBufferSize, GL_MAP_WRITE_BIT);

    // MATERIAL BUFFER
    unsigned int materialBuffer;
    glGenBuffers(1, &materialBuffer);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, materialBuffer);
    glBufferStorage(GL_SHADER_STORAGE_BUFFER, materialBufferSize, nullptr, GL_MAP_WRITE_BIT | GL_MAP_PERSISTENT_BIT | GL_MAP_COHERENT_BIT);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 3, materialBuffer);
    void* mappedMaterialBuffer = glMapBufferRange(GL_SHADER_STORAGE_BUFFER, 0, materialBufferSize, GL_MAP_WRITE_BIT);

    // BVH BUFFER
    unsigned int bvhBuffer;
    glGenBuffers(1, &bvhBuffer);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, bvhBuffer);
    glBufferStorage(GL_SHADER_STORAGE_BUFFER, bvhBufferSize, nullptr, GL_MAP_WRITE_BIT | GL_MAP_PERSISTENT_BIT | GL_MAP_COHERENT_BIT);  // Use glBufferStorage instead
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 4, bvhBuffer);
    void* mappedBVHBuffer = glMapBufferRange(GL_SHADER_STORAGE_BUFFER, 0, bvhBufferSize, GL_MAP_WRITE_BIT);

    // PARTITION BUFFER
    unsigned int partitionBuffer;
    glGenBuffers(1, &partitionBuffer);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, partitionBuffer);
    glBufferData(GL_SHADER_STORAGE_BUFFER, sizeof(MeshPartition) * meshPartitions.size(), meshPartitions.data(), GL_STATIC_READ);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 5, partitionBuffer);

    // COPY VERTEX INDEX AND MATERIAL DATA TO THE GPU
    uint32_t vertexOffset = 0;
    uint32_t indexOffset = 0;
    uint32_t materialOffset = 0;
    uint32_t bvhOffset = 0;
    for (const Mesh* mesh : meshes) 
    {
        memcpy((char*)mappedVertexBuffer + vertexOffset, mesh->vertices.data(),
            mesh->vertices.size() * sizeof(Vertex));
        memcpy((char*)mappedIndexBuffer + indexOffset, mesh->indices.data(),
            mesh->indices.size() * sizeof(uint32_t));
        memcpy((char*)mappedMaterialBuffer + materialOffset, &mesh->material,
            sizeof(Material));
        memcpy((char*)mappedBVHBuffer + bvhOffset, mesh->bvhNodes,
            mesh->nodesUsed * sizeof(BVH_Node));

        vertexOffset += mesh->vertices.size() * sizeof(Vertex);
        indexOffset += mesh->indices.size() * sizeof(uint32_t);
        materialOffset += sizeof(Material);
        bvhOffset += mesh->nodesUsed * sizeof(BVH_Node);
    }
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, vertexBuffer);
    glUnmapBuffer(GL_SHADER_STORAGE_BUFFER);  
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, indexBuffer);
    glUnmapBuffer(GL_SHADER_STORAGE_BUFFER);  
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, materialBuffer);
    glUnmapBuffer(GL_SHADER_STORAGE_BUFFER);  
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, bvhBuffer);
    glUnmapBuffer(GL_SHADER_STORAGE_BUFFER);  
    // }----------{ SEND MESH DATA TO THE GPU }----------{










    // }----------{ APPLICATION LOOP }----------{
    while (!glfwWindowShouldClose(window))
    {
        auto start = std::chrono::high_resolution_clock::now();
        glfwPollEvents();

        // }----------{ HANDLE WINDOW RESIZING }----------{
        int windowbufferWidth, windowbufferHeight;
        glfwGetFramebufferSize(window, &windowbufferWidth, &windowbufferHeight);
        const float newWIDTH = float(windowbufferWidth);
        const float newHEIGHT = float(windowbufferHeight);

        // WINDOW RESIZED
        if (newWIDTH != WIDTH || newHEIGHT != HEIGHT)
        {
            HEIGHT = newHEIGHT;
            WIDTH = newWIDTH;
            VIEWPORT_WIDTH = WIDTH * 0.8f;
            VIEWPORT_HEIGHT = HEIGHT * 0.8f;

            // RESIZE FRAME BUFFER, OPENGL VIEWPORT AND RENDER TEXTURE
            qRenderer.ResizeFramebuffer((int)VIEWPORT_WIDTH, (int)VIEWPORT_HEIGHT);
            glViewport(0, 0, (int)VIEWPORT_WIDTH, (int)VIEWPORT_HEIGHT); 
            glBindTexture(GL_TEXTURE_2D, RenderTexture);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, (int)VIEWPORT_WIDTH, (int)VIEWPORT_HEIGHT, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
            glBindTexture(GL_TEXTURE_2D, 0);

            // RESET FRAME ACCUMULATION
            accumulationFrame = 0;
        }
        // }----------{ HANDLE WINDOW RESIZING }----------{


        UpdateUI();
        glClearColor(0.047f, 0.082f, 0.122f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        

        // }----------{ INVOKE PATH TRACER }----------{
        glUseProgram(pathtraceShader);

            // CAMERA UNIFORM
        glm::vec3 camPos{0.0f, 1.0f, -4.5f};
        glm::vec3 camForward{0.0f, 0.0f, 1.0f};
        glm::vec3 camRight{1.0f, 0.0f, 0.0f};
        glm::vec3 camUp{0.0f, 1.0f, 0.0f};
        float camFOV = 90.0f;
        glUniform3f(camInfoPosLocation, camPos.x, camPos.y, camPos.z);
        glUniform3f(camInfoForwardLocation, camForward.x, camForward.y, camForward.z);
        glUniform3f(camInfoRightLocation, camRight.x, camRight.y, camRight.z);
        glUniform3f(camInfoUpLocation, camUp.x, camUp.y, camUp.z);
        glUniform1f(camInfoFOVLocation, camFOV);

        // FRAME COUNT FOR PSEUDO RANDOMNESS
        glUniform1ui(glGetUniformLocation(pathtraceShader, "u_frameCount"), frameCount);

        // FRAME ACCUMULATION COUNT
        glUniform1ui(glGetUniformLocation(pathtraceShader, "u_accumulationFrame"), accumulationFrame);

        // RENDER TEXTURE
        glBindImageTexture(0, RenderTexture, 0, GL_FALSE, 0, GL_READ_WRITE, GL_RGBA8); 

        // RUN PATH TRACE SHADER
        GLuint numWorkGroupsX = (VIEWPORT_WIDTH + 32) / 32;
        GLuint numWorkGroupsY = (VIEWPORT_HEIGHT + 32) / 32;
        glDispatchCompute(numWorkGroupsX, numWorkGroupsY, 1);       
        glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);
        glFinish();
        // }----------{ PATH TRACER ENDS }----------{
    

        // }----------{ RENDER THE QUAD TO THE FRAME BUFFER }----------{
        qRenderer.RenderToViewport(RenderTexture);
        // }----------{ RENDER THE QUAD TO THE FRAME BUFFER }----------{


        // }----------{ APP LAYOUT }----------{
        ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
        ImGui::SetNextWindowPos(ImVec2(0, 0), ImGuiCond_Always);
        ImGui::SetNextWindowSize(ImGui::GetMainViewport()->Size, ImGuiCond_Always);
        ImGui::SetNextWindowBgAlpha(0.0f);
        ImGui::Begin(
            "App Layout", nullptr, 
            ImGuiWindowFlags_NoResize | 
            ImGuiWindowFlags_NoMove | 
            ImGuiWindowFlags_NoTitleBar | 
            ImGuiWindowFlags_NoCollapse
        );
        

        // }----------{ VIEWPORT WINDOW   }----------{
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
        ImGui::BeginChild("Viewport", ImVec2(VIEWPORT_WIDTH, VIEWPORT_HEIGHT), true);
        ImVec2 cursorPos = ImGui::GetCursorScreenPos();
        ImGui::GetWindowDrawList()->AddImage(
            (ImTextureID)(intptr_t)qRenderer.GetFrameBufferTextureID(),
            cursorPos,
            ImVec2(cursorPos.x + VIEWPORT_WIDTH, cursorPos.y + VIEWPORT_HEIGHT),
            ImVec2(0, 1),
            ImVec2(1, 0)
        );
        int frameRate = int((1.0f / frameTime) + 0.5f);
        std::string frameTimeString = std::to_string(frameRate) + "fps";
        ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(0, 0, 0, 255));
        ImGui::Text("%s", frameTimeString.c_str());
        ImGui::PopStyleColor();
        ImGui::EndChild();
        ImGui::PopStyleVar();
        // }----------{ VIEWPORT WINDOW   }----------{
        

        // }----------{ OBJECTS PANEL     }----------{
        ImGui::SameLine();
        ImGui::BeginChild("Objects", ImVec2(0, VIEWPORT_HEIGHT), true);
        ImGui::Text("Objects");
        ImGui::EndChild();
        // }----------{ OBJECTS PANEL     }----------{


        // }----------{ MODEL EXPLORER    }----------{
        ImGui::BeginChild(
            "Model Explorer", 
            ImVec2(ImGui::GetContentRegionAvail().x * 0.333f, 
            ImGui::GetContentRegionAvail().y), true
        );
        ImGui::Text("Model Explorer");
        ImGui::EndChild();
        // }----------{ MODEL EXPLORER    }----------{


        // }----------{ MATERIAL EXPLORER }----------{
        ImGui::SameLine();
        ImGui::BeginChild(
            "Material Explorer", 
            ImVec2(ImGui::GetContentRegionAvail().x * 0.5f, 
            ImGui::GetContentRegionAvail().y), true
        );
        ImGui::Text("Material Explorer");
        ImGui::EndChild();
        // }----------{ MATERIAL EXPLORER }----------{


        // }----------{ TEXTURE PANEL     }----------{
        ImGui::SameLine();
        ImGui::BeginChild("Texture Panel", ImVec2(0, ImGui::GetContentRegionAvail().y), true);
        ImGui::Text("Texture Panel");
        ImGui::EndChild();
        // }----------{ TEXTURE PANEL     }----------{


        ImGui::End();
        ImGui::PopStyleVar();
        // }----------{ APP LAYOUT ENDS   }----------{


        RenderUI();
        glfwSwapBuffers(window);
        frameCount += 1;
        accumulationFrame += 1;

        auto end = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::duration<float>>(end - start);
        frameTime = duration.count();
    }

    glDeleteBuffers(1, &vertexBuffer);
    glDeleteBuffers(1, &indexBuffer);
    glDeleteBuffers(1, &materialBuffer);
    glDeleteBuffers(1, &partitionBuffer);

    glfwDestroyWindow(window);
    glfwTerminate();
    exit(EXIT_SUCCESS);
}