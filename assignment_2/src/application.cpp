// cpp
#include "mesh.h"
#include "texture.h"
#include "bezier.h"
// Always include window first (because it includes glfw, which includes GL which needs to be included AFTER glew).
// Can't wait for modules to fix this stuff...
#include <framework/disable_all_warnings.h>
DISABLE_WARNINGS_PUSH()
#include <glad/glad.h>
// Include glad before glfw3
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_inverse.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/mat4x4.hpp>
#include <imgui/imgui.h>
DISABLE_WARNINGS_POP()
#include <framework/shader.h>
#include <framework/window.h>
#include <functional>
#include <iostream>
#include <vector>
#include <memory>

class Application {
public:
    Application()
        : m_window("Final Project", glm::ivec2(1024, 1024), OpenGLVersion::GL41)
    {
        // Register callbacks (no GL calls here)
        m_window.registerKeyCallback([this](int key, int scancode, int action, int mods) {
            if (action == GLFW_PRESS)
                onKeyPressed(key, mods);
            else if (action == GLFW_RELEASE)
                onKeyReleased(key, mods);
        });
        m_window.registerMouseMoveCallback(std::bind(&Application::onMouseMove, this, std::placeholders::_1));
        m_window.registerMouseButtonCallback([this](int button, int action, int mods) {
            if (action == GLFW_PRESS)
                onMouseClicked(button, mods);
            else if (action == GLFW_RELEASE)
                onMouseReleased(button, mods);
        });

        // Initialize GL function pointers now that the GLFW context (created by Window) is current
        if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
            std::cerr << "Failed to initialize GLAD - OpenGL function pointers not loaded\n";
            std::terminate();
        }
        std::cout << "GL initialized: " << (const char*)glGetString(GL_VERSION) << std::endl;

        // Now safe to create GL-backed resources
        m_texture = std::make_unique<Texture>(RESOURCE_ROOT "resources/checkerboard.png");

        // Load meshes and shaders (these may call GL functions)
        m_meshes = GPUMesh::loadMeshGPU(RESOURCE_ROOT "resources/dragon.obj");

        try {
            ShaderBuilder defaultBuilder;
            defaultBuilder.addStage(GL_VERTEX_SHADER, RESOURCE_ROOT "shaders/shader_vert.glsl");
            defaultBuilder.addStage(GL_FRAGMENT_SHADER, RESOURCE_ROOT "shaders/shader_frag.glsl");
            m_defaultShader = defaultBuilder.build();

            ShaderBuilder shadowBuilder;
            shadowBuilder.addStage(GL_VERTEX_SHADER, RESOURCE_ROOT "shaders/shadow_vert.glsl");
            shadowBuilder.addStage(GL_FRAGMENT_SHADER, RESOURCE_ROOT "shaders/shadow_frag.glsl");
            m_shadowShader = shadowBuilder.build();

        } catch (ShaderLoadingException e) {
            std::cerr << e.what() << std::endl;
        }

        // Build the Bezier path segments
        {
            std::vector<CubicBezier> segs;
            const float R = 4.0f, H = 0.5f;
            segs.push_back({ { R,0,0 }, { R,0, R*0.55f }, {  R*0.55f,0, R }, { 0,0, R } });
            segs.push_back({ { 0,0, R }, { -R*0.55f,0, R }, { -R,0, R*0.55f }, { -R,0,0 } });
            segs.push_back({ { -R,0,0 }, { -R,0,-R*0.55f }, { -R*0.55f,0,-R }, { 0,0,-R } });
            segs.push_back({ { 0,0,-R }, {  R*0.55f,0,-R }, {  R,0,-R*0.55f }, { R,0,0 } });

            for (size_t i = 0; i < segs.size(); ++i) {
                segs[i].p1.y += ((i % 2) ? H : -H);
                segs[i].p2.y += ((i % 2) ? -H : H);
            }
            m_path.setSegments(segs);
        }
    }

    void update()
    {
        int dummyInteger = 0; // Initialized to 0
        while (!m_window.shouldClose()) {
            m_window.updateInput();

            ImGui::Begin("Window");
            ImGui::InputInt("This is an integer input", &dummyInteger);
            ImGui::Text("Value is: %i", dummyInteger);
            ImGui::Checkbox("Use material if no texture", &m_useMaterial);
            ImGui::Checkbox("Show path", &m_showPath);
            ImGui::SliderFloat("Path speed", &m_pathSpeed, 0.0f, 0.3f, "%.3f");
            ImGui::Checkbox("Chase camera", &m_chaseCam);
            ImGui::SliderFloat("Probe scale", &m_probeScale, 0.02f, 0.6f, "%.3f");
            ImGui::End();

            glClearColor(0.2f, 0.2f, 0.2f, 1.0f);
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
            glEnable(GL_DEPTH_TEST);

            const glm::mat4 mvpMatrix = m_projectionMatrix * m_viewMatrix * m_modelMatrix;
            const glm::mat3 normalModelMatrix = glm::inverseTranspose(glm::mat3(m_modelMatrix));

            static double last = glfwGetTime();
            double now = glfwGetTime();
            float dtSec = float(now - last);
            last = now;

            m_pathU = std::fmod(m_pathU + dtSec * m_pathSpeed, 1.0f);
            glm::vec3 probePos = m_path.sample(m_pathU);
            glm::vec3 probeDir = glm::normalize(m_path.tangentAt(m_pathU));

            // Build a probe model matrix at probePos, roughly aligned with tangent
            glm::vec3 fwd = glm::normalize(probeDir);
            glm::vec3 up  = glm::vec3(0,1,0);
            if (std::abs(glm::dot(up, fwd)) > 0.98f) up = glm::vec3(0,0,1); // avoid near-parallel
            glm::vec3 right = glm::normalize(glm::cross(fwd, up));
            up = glm::normalize(glm::cross(right, fwd));

            // simple chase camera (back and a bit above)
            glm::vec3 camTarget = probePos;
            glm::vec3 camPos    = probePos - fwd * 2.0f + up * 0.6f;
            m_viewMatrix = glm::lookAt(camPos, camTarget, up);

            glm::mat4 R = glm::mat4(
                glm::vec4(right, 0),
                glm::vec4(up,    0),
                glm::vec4(-fwd,  0),  // -fwd for look-at style
                glm::vec4(0,0,0,1)
            );
            glm::mat4 Mprobe =
                glm::translate(glm::mat4(1.0f), probePos) *
                R *
                glm::scale(glm::mat4(1.0f), glm::vec3(m_probeScale));



            // draw the probe (reuse first mesh as placeholder)
            // draw the moving probe using the first mesh as placeholder
            if (!m_meshes.empty()) {
                m_defaultShader.bind();
                glm::mat4 mvpProbe = m_projectionMatrix * m_viewMatrix * Mprobe;
                glm::mat3 nProbe   = glm::inverseTranspose(glm::mat3(Mprobe));
                glUniformMatrix4fv(m_defaultShader.getUniformLocation("mvpMatrix"), 1, GL_FALSE, glm::value_ptr(mvpProbe));
                glUniformMatrix3fv(m_defaultShader.getUniformLocation("normalModelMatrix"), 1, GL_FALSE, glm::value_ptr(nProbe));
                glUniform1i(m_defaultShader.getUniformLocation("hasTexCoords"), GL_FALSE);
                glUniform1i(m_defaultShader.getUniformLocation("useMaterial"), GL_TRUE);
                m_meshes.front().draw(m_defaultShader);
            }


            for (GPUMesh& mesh : m_meshes) {
                m_defaultShader.bind();
                glUniformMatrix4fv(m_defaultShader.getUniformLocation("mvpMatrix"), 1, GL_FALSE, glm::value_ptr(mvpMatrix));
                glUniformMatrix3fv(m_defaultShader.getUniformLocation("normalModelMatrix"), 1, GL_FALSE, glm::value_ptr(normalModelMatrix));
                if (mesh.hasTextureCoords()) {
                    if (m_texture) {
                        m_texture->bind(GL_TEXTURE0);
                        glUniform1i(m_defaultShader.getUniformLocation("colorMap"), 0);
                    }
                    glUniform1i(m_defaultShader.getUniformLocation("hasTexCoords"), GL_TRUE);
                    glUniform1i(m_defaultShader.getUniformLocation("useMaterial"), GL_FALSE);
                } else {
                    glUniform1i(m_defaultShader.getUniformLocation("hasTexCoords"), GL_FALSE);
                    glUniform1i(m_defaultShader.getUniformLocation("useMaterial"), m_useMaterial);
                }
                mesh.draw(m_defaultShader);
            }

            // draw the spline once
            // draw the spline once (avoid z-fighting)
            if (m_showPath) {
                glDisable(GL_DEPTH_TEST);
                m_defaultShader.bind();
                glm::mat4 mvpSpline = m_projectionMatrix * m_viewMatrix * glm::mat4(1.0f);
                glUniformMatrix4fv(m_defaultShader.getUniformLocation("mvpMatrix"), 1, GL_FALSE, glm::value_ptr(mvpSpline));
                m_path.drawGL();
                glEnable(GL_DEPTH_TEST);
            }

            m_window.swapBuffers();

        }
    }

    void onKeyPressed(int key, int mods)
    {
        std::cout << "Key pressed: " << key << std::endl;
    }

    void onKeyReleased(int key, int mods)
    {
        std::cout << "Key released: " << key << std::endl;
    }

    void onMouseMove(const glm::dvec2& cursorPos)
    {
        std::cout << "Mouse at position: " << cursorPos.x << " " << cursorPos.y << std::endl;
    }

    void onMouseClicked(int button, int mods)
    {
        std::cout << "Pressed mouse button: " << button << std::endl;
    }

    void onMouseReleased(int button, int mods)
    {
        std::cout << "Released mouse button: " << button << std::endl;
    }

    private:
        // GL context must exist before GL objects are created.
        Window m_window;

        // Shaders
        Shader m_defaultShader;
        Shader m_shadowShader;

        // Resources
        std::vector<GPUMesh> m_meshes;
        std::unique_ptr<Texture> m_texture;
        bool m_useMaterial { true };

        // Matrices
        glm::mat4 m_projectionMatrix = glm::perspective(glm::radians(80.0f), 1.0f, 0.1f, 30.0f);
        glm::mat4 m_viewMatrix = glm::lookAt(glm::vec3(-1, 1, -1), glm::vec3(0), glm::vec3(0, 1, 0));
        glm::mat4 m_modelMatrix { 1.0f };

        // --- Planet Hop path test --- MUST be after m_window
        BezierPath m_path{200};
        bool  m_showPath   = true;
        float m_pathU      = 0.0f;
        float m_pathSpeed  = 0.05f;
        GLuint m_basicLineProgram = 0;

        float m_probeScale = 0.12f;
        bool  m_chaseCam   = true;    // toggle chase camera


    };

int main()
{
    Application app;
    app.update();
    return 0;
}
