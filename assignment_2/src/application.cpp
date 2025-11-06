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
#include <cassert>
#include "scene_node.h"
#include "skybox.h"

class Application {
public:
    Application()
        : m_window("Final Project", glm::ivec2(1024, 1024), OpenGLVersion::GL41) {
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
        if (!gladLoadGLLoader((GLADloadproc) glfwGetProcAddress)) {
            std::cerr << "Failed to initialize GLAD - OpenGL function pointers not loaded\n";
            std::terminate();
        }
        std::cout << "GL initialized: " << (const char *) glGetString(GL_VERSION) << std::endl;

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

        // build skybox shader
        ShaderBuilder skyB;
        skyB.addStage(GL_VERTEX_SHADER, RESOURCE_ROOT "shaders/skybox_vert.glsl");
        skyB.addStage(GL_FRAGMENT_SHADER, RESOURCE_ROOT "shaders/skybox_frag.glsl");
        m_skyShader = skyB.build();

        // load cubemap faces
        std::array<std::string, 6> faces = {
            RESOURCE_ROOT "resources/sky/mid right.png",
            RESOURCE_ROOT "resources/sky/left.png",
            RESOURCE_ROOT "resources/sky/top.png",
            RESOURCE_ROOT "resources/sky/down.png",
            RESOURCE_ROOT "resources/sky/mid.png",
            RESOURCE_ROOT "resources/sky/right.png"
        };
        m_sky = std::make_unique<Skybox>(faces);

        // Inner Bezier path (camera target dragon)
        {
            std::vector<CubicBezier> segs;
            const float R = 4.0f, H = 0.5f;
            segs.push_back({{R, 0, 0}, {R, 0, R * 0.55f}, {R * 0.55f, 0, R}, {0, 0, R}});
            segs.push_back({{0, 0, R}, {-R * 0.55f, 0, R}, {-R, 0, R * 0.55f}, {-R, 0, 0}});
            segs.push_back({{-R, 0, 0}, {-R, 0, -R * 0.55f}, {-R * 0.55f, 0, -R}, {0, 0, -R}});
            segs.push_back({{0, 0, -R}, {R * 0.55f, 0, -R}, {R, 0, -R * 0.55f}, {R, 0, 0}});
            for (size_t i = 0; i < segs.size(); ++i) {
                segs[i].p1.y += ((i % 2) ? H : -H);
                segs[i].p2.y += ((i % 2) ? -H : H);
            }
            m_path.setSegments(segs);
        }

        // Scene graph: inner root and escort root
        m_probeRoot   = new SceneNode();      // single dragon on inner path
        m_escortRoot  = new SceneNode();      // two stacked dragons on outer path
        m_probeAntennaBase = m_escortRoot->addChild(new SceneNode());  // first stacked dragon
        m_probeAntennaTip  = m_probeAntennaBase->addChild(new SceneNode()); // second stacked dragon above it

        // Outer Bezier path (larger radius)
        {
            std::vector<CubicBezier> segs;
            const float R = m_pathOuterRadius, H = 0.6f;
            segs.push_back({{R, 0, 0}, {R, 0, R * 0.55f}, {R * 0.55f, 0, R}, {0, 0, R}});
            segs.push_back({{0, 0, R}, {-R * 0.55f, 0, R}, {-R, 0, R * 0.55f}, {-R, 0, 0}});
            segs.push_back({{-R, 0, 0}, {-R, 0, -R * 0.55f}, {-R * 0.55f, 0, -R}, {0, 0, -R}});
            segs.push_back({{0, 0, -R}, {R * 0.55f, 0, -R}, {R, 0, -R * 0.55f}, {R, 0, 0}});
            for (size_t i = 0; i < segs.size(); ++i) {
                segs[i].p1.y += ((i % 2) ? H : -H);
                segs[i].p2.y += ((i % 2) ? -H : H);
            }
            m_pathOuter.setSegments(segs);
        }

        m_texAlbedo   = std::make_unique<Texture>(RESOURCE_ROOT "resources/spaceship/basecolor.png");
        m_texNormal   = std::make_unique<Texture>(RESOURCE_ROOT "resources/spaceship/normal.png");
        m_texRoughness= std::make_unique<Texture>(RESOURCE_ROOT "resources/spaceship/roughness.png");
        m_texMetallic = std::make_unique<Texture>(RESOURCE_ROOT "resources/spaceship/metallic.png");

        buildSunSphere();
        m_texSun = std::make_unique<Texture>(RESOURCE_ROOT "resources/sun/sunTex.jpg");

        // sanity
        assert(m_probeRoot && m_escortRoot && m_probeAntennaBase && m_probeAntennaTip);
    }

    void update() {
        while (!m_window.shouldClose()) {
            m_window.updateInput();

            ImGui::Begin("Controls");
            ImGui::Checkbox("Use material if no texture", &m_useMaterial);
            ImGui::Checkbox("Show path", &m_showPath);
            ImGui::SliderFloat("Path speed", &m_pathSpeed, 0.0f, 0.3f, "%.3f");
            ImGui::Checkbox("Chase camera", &m_chaseCam);
            ImGui::SliderFloat("Probe scale", &m_probeScale, 0.02f, 0.6f, "%.3f");
            ImGui::Checkbox("Environment reflections", &m_useEnvMap);
            ImGui::Checkbox("PBR + Normal Map", &m_usePBR);
            ImGui::Text("Camera");
            ImGui::RadioButton("Chase", &m_camMode, 0);
            ImGui::SameLine();
            ImGui::RadioButton("Top", &m_camMode, 1);
            ImGui::SameLine();
            ImGui::RadioButton("Orbit", &m_camMode, 2);
            ImGui::DragFloat3("Sun pos", &m_sunPos.x, 0.05f);
            ImGui::SliderFloat("Sun radius", &m_sunRadius, 0.2f, 2.0f, "%.2f");
            ImGui::SliderFloat("Sun intensity", &m_sunIntensity, 0.0f, 40.0f, "%.1f");
            ImGui::Checkbox("Draw static scene", &m_drawStaticScene);
            ImGui::End();

            glClearColor(0.2f, 0.2f, 0.2f, 1.0f);
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
            glEnable(GL_DEPTH_TEST);

            static double last = glfwGetTime();
            double now = glfwGetTime();
            float dtSec = float(now - last);
            last = now;

            // Advance inner path
            m_pathU = std::fmod(m_pathU + dtSec * m_pathSpeed, 1.0f);
            glm::vec3 probePos = m_path.sample(m_pathU);
            glm::vec3 probeDir = glm::normalize(m_path.tangentAt(m_pathU));

            // Frame from tangent (inner)
            glm::vec3 fwd = glm::normalize(probeDir);
            glm::vec3 up  = glm::vec3(0, 1, 0);
            if (std::abs(glm::dot(up, fwd)) > 0.98f) up = glm::vec3(0, 0, 1);
            glm::vec3 right = glm::normalize(glm::cross(fwd, up));
            up = glm::normalize(glm::cross(right, fwd));
            glm::mat4 R = glm::mat4(
                glm::vec4(right, 0),
                glm::vec4(up, 0),
                glm::vec4(-fwd, 0),
                glm::vec4(0, 0, 0, 1)
            );

            // Place inner root
            glm::mat4 Mprobe =
                glm::translate(glm::mat4(1.0f), probePos) *
                R *
                glm::scale(glm::mat4(1.0f), glm::vec3(m_probeScale));
            m_probeRoot->local = Mprobe;

            // Advance outer path
            m_pathOuterU = std::fmod(m_pathOuterU + dtSec * m_pathOuterSpeed, 1.0f);
            glm::vec3 outerPos = m_pathOuter.sample(m_pathOuterU);
            glm::vec3 outerDir = glm::normalize(m_pathOuter.tangentAt(m_pathOuterU));

            // Frame from tangent (outer)
            glm::vec3 ofwd = glm::normalize(outerDir);
            glm::vec3 oup  = glm::vec3(0, 1, 0);
            if (std::abs(glm::dot(oup, ofwd)) > 0.98f) oup = glm::vec3(0, 0, 1);
            glm::vec3 oright = glm::normalize(glm::cross(ofwd, oup));
            oup = glm::normalize(glm::cross(oright, ofwd));
            glm::mat4 oR = glm::mat4(
                glm::vec4(oright, 0),
                glm::vec4(oup, 0),
                glm::vec4(-ofwd, 0),
                glm::vec4(0, 0, 0, 1)
            );

            // Place escort root at larger radius
            glm::mat4 Mescort =
                glm::translate(glm::mat4(1.0f), outerPos) *
                oR *
                glm::scale(glm::mat4(1.0f), glm::vec3(m_probeScale));
            m_escortRoot->local = Mescort;

            // Keep the stacked one above the other (base at root, tip above with a small bob)
            float bob = 0.25f * std::sin(float(glfwGetTime()) * 4.0f);
            m_probeAntennaBase->local = glm::mat4(1.0f);
            m_probeAntennaTip->local  = glm::translate(glm::mat4(1.0f), glm::vec3(0, 1.0f + bob, 0));

            // Camera selection
            if (m_camMode == 0) {
                glm::vec3 camTarget = probePos;
                glm::vec3 camPos = probePos - fwd * 2.0f + up * 0.6f;
                m_viewMatrix = glm::lookAt(camPos, camTarget, up);
            } else if (m_camMode == 1) {
                glm::vec3 camPos = probePos + glm::vec3(0, 5.0f, 0);
                m_viewMatrix = glm::lookAt(camPos, probePos, glm::vec3(0, 0, -1));
            } else {
                m_orbitAngle += dtSec * 0.5f;
                glm::vec3 camPos = probePos + glm::vec3(std::sin(m_orbitAngle) * 3.0f, 1.5f,
                                                        std::cos(m_orbitAngle) * 3.0f);
                m_viewMatrix = glm::lookAt(camPos, probePos, glm::vec3(0, 1, 0));
            }

            // skybox first (after you set m_viewMatrix)
            glm::mat4 viewNoTrans = m_viewMatrix;
            viewNoTrans[3] = glm::vec4(0, 0, 0, 1);
            m_sky->draw(m_skyShader, m_projectionMatrix, viewNoTrans);

            // bind cubemap to unit 1 for the rest of the frame
            glActiveTexture(GL_TEXTURE1);
            glBindTexture(GL_TEXTURE_CUBE_MAP, m_sky->cubemap());

            // cache camera world position for reflections
            glm::mat4 invV = glm::inverse(m_viewMatrix);
            glm::vec3 camPos = glm::vec3(invV[3]);

            // Propagate transforms
            m_probeRoot->update();
            m_escortRoot->update();

            // Draw the sun sphere (emissive)
            {
                m_defaultShader.bind();

                // Mark this draw as "sun" immediately
                glUniform1i(m_defaultShader.getUniformLocation("isSun"), 1);

                // Light uniforms
                glUniform3fv(m_defaultShader.getUniformLocation("sunPos"), 1, &m_sunPos[0]);
                glUniform1f (m_defaultShader.getUniformLocation("sunIntensity"), m_sunIntensity);

                // Emissive color
                glm::vec3 sunColor = glm::vec3(m_sunIntensity);
                glUniform3fv(m_defaultShader.getUniformLocation("sunEmissive"), 1, glm::value_ptr(sunColor));

                // Matrices for the sun sphere
                glm::mat4 Msun = glm::translate(glm::mat4(1.0f), m_sunPos)
                               * glm::scale(glm::mat4(1.0f), glm::vec3(m_sunRadius));
                glm::mat4 mvp = m_projectionMatrix * m_viewMatrix * Msun;
                glm::mat3 nrm = glm::inverseTranspose(glm::mat3(Msun));
                glUniformMatrix4fv(m_defaultShader.getUniformLocation("mvpMatrix"), 1, GL_FALSE, glm::value_ptr(mvp));
                glUniformMatrix3fv(m_defaultShader.getUniformLocation("normalModelMatrix"), 1, GL_FALSE, glm::value_ptr(nrm));
                glUniformMatrix4fv(m_defaultShader.getUniformLocation("modelMatrix"), 1, GL_FALSE, glm::value_ptr(Msun));

                // Base texture for the sun surface
                if (m_texSun) {
                    m_texSun->bind(GL_TEXTURE0);
                    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
                    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
                    glUniform1i(m_defaultShader.getUniformLocation("colorMap"), 0);
                    glUniform1i(m_defaultShader.getUniformLocation("hasTexCoords"), 1);
                } else {
                    glUniform1i(m_defaultShader.getUniformLocation("hasTexCoords"), 0);
                }

                // No PBR/EnvMap for emissive blob
                glUniform1i(m_defaultShader.getUniformLocation("usePBR"), 0);
                glUniform1i(m_defaultShader.getUniformLocation("useEnvMap"), 0);
                glUniform3fv(m_defaultShader.getUniformLocation("camPos"), 1, &camPos[0]);

                glBindVertexArray(m_sunVAO);
                glDrawElements(GL_TRIANGLES, m_sunIndexCount, GL_UNSIGNED_INT, nullptr);
                glBindVertexArray(0);

                // Reset for subsequent draws
                glUniform1i(m_defaultShader.getUniformLocation("isSun"), 0);
            }

            // Draw single dragon on INNER path (root only)
            {
                m_defaultShader.bind();
                const glm::mat4 &M = m_probeRoot->world;
                glm::mat4 mvp = m_projectionMatrix * m_viewMatrix * M;
                glm::mat3 nrm = glm::inverseTranspose(glm::mat3(M));
                glUniformMatrix4fv(m_defaultShader.getUniformLocation("mvpMatrix"), 1, GL_FALSE, glm::value_ptr(mvp));
                glUniformMatrix3fv(m_defaultShader.getUniformLocation("normalModelMatrix"), 1, GL_FALSE, glm::value_ptr(nrm));
                glUniformMatrix4fv(m_defaultShader.getUniformLocation("modelMatrix"), 1, GL_FALSE, glm::value_ptr(M));

                glUniform1i(m_defaultShader.getUniformLocation("usePBR"), m_usePBR ? 1 : 0);
                glUniform1i(m_defaultShader.getUniformLocation("useEnvMap"), m_useEnvMap ? 1 : 0);
                glUniform1i(m_defaultShader.getUniformLocation("hasTexCoords"), 1);

                if (m_texAlbedo)   { m_texAlbedo->bind(GL_TEXTURE0);   glUniform1i(m_defaultShader.getUniformLocation("colorMap"), 0); }
                if (m_texNormal)   { m_texNormal->bind(GL_TEXTURE2);   glUniform1i(m_defaultShader.getUniformLocation("normalMap"), 2); }
                if (m_texRoughness){ m_texRoughness->bind(GL_TEXTURE3); glUniform1i(m_defaultShader.getUniformLocation("roughMap"), 3); }
                if (m_texMetallic) { m_texMetallic->bind(GL_TEXTURE4); glUniform1i(m_defaultShader.getUniformLocation("metalMap"), 4); }
                glUniform1i(m_defaultShader.getUniformLocation("envMap"), 1);
                glUniform3fv(m_defaultShader.getUniformLocation("camPos"), 1, &camPos[0]);

                m_meshes.front().draw(m_defaultShader);
            }

            // Draw the two stacked dragons on the OUTER path (traverse escort hierarchy)
            m_escortRoot->traverse([&](const glm::mat4 &M) {
                m_defaultShader.bind();
                glm::mat4 mvp = m_projectionMatrix * m_viewMatrix * M;
                glm::mat3 nrm = glm::inverseTranspose(glm::mat3(M));
                glUniformMatrix4fv(m_defaultShader.getUniformLocation("mvpMatrix"), 1, GL_FALSE, glm::value_ptr(mvp));
                glUniformMatrix3fv(m_defaultShader.getUniformLocation("normalModelMatrix"), 1, GL_FALSE, glm::value_ptr(nrm));
                glUniformMatrix4fv(m_defaultShader.getUniformLocation("modelMatrix"), 1, GL_FALSE, glm::value_ptr(M));

                glUniform1i(m_defaultShader.getUniformLocation("usePBR"), m_usePBR ? 1 : 0);
                glUniform1i(m_defaultShader.getUniformLocation("useEnvMap"), m_useEnvMap ? 1 : 0);
                glUniform1i(m_defaultShader.getUniformLocation("hasTexCoords"), 1);

                if (m_texAlbedo)   { m_texAlbedo->bind(GL_TEXTURE0);   glUniform1i(m_defaultShader.getUniformLocation("colorMap"), 0); }
                if (m_texNormal)   { m_texNormal->bind(GL_TEXTURE2);   glUniform1i(m_defaultShader.getUniformLocation("normalMap"), 2); }
                if (m_texRoughness){ m_texRoughness->bind(GL_TEXTURE3); glUniform1i(m_defaultShader.getUniformLocation("roughMap"), 3); }
                if (m_texMetallic) { m_texMetallic->bind(GL_TEXTURE4); glUniform1i(m_defaultShader.getUniformLocation("metalMap"), 4); }
                glUniform1i(m_defaultShader.getUniformLocation("envMap"), 1);
                glUniform3fv(m_defaultShader.getUniformLocation("camPos"), 1, &camPos[0]);

                m_meshes.front().draw(m_defaultShader);
            });

            // draw the splines (avoid z-fighting)
            if (m_showPath) {
                glDisable(GL_DEPTH_TEST);
                m_defaultShader.bind();
                glm::mat4 mvpSpline = m_projectionMatrix * m_viewMatrix * glm::mat4(1.0f);
                glUniformMatrix4fv(m_defaultShader.getUniformLocation("mvpMatrix"), 1, GL_FALSE, glm::value_ptr(mvpSpline));
                m_path.drawGL();       // inner
                m_pathOuter.drawGL();  // outer
                glEnable(GL_DEPTH_TEST);
            }

            m_window.swapBuffers();
        }
    }

    void onKeyPressed(int key, int mods) {
        std::cout << "Key pressed: " << key << std::endl;
    }

    void onKeyReleased(int key, int mods) {
        std::cout << "Key released: " << key << std::endl;
    }

    void onMouseMove(const glm::dvec2 &cursorPos) {
        std::cout << "Mouse at position: " << cursorPos.x << " " << cursorPos.y << std::endl;
    }

    void onMouseClicked(int button, int mods) {
        std::cout << "Pressed mouse button: " << button << std::endl;
    }

    void onMouseReleased(int button, int mods) {
        std::cout << "Released mouse button: " << button << std::endl;
    }

    void buildSunSphere(int stacks = 32, int slices = 64) {
        std::vector<glm::vec3> pos;
        std::vector<glm::vec3> nrm;
        std::vector<glm::vec2> uv;
        std::vector<uint32_t> idx;

        for (int i = 0; i <= stacks; ++i) {
            float v = float(i) / stacks;
            float phi = v * glm::pi<float>();
            for (int j = 0; j <= slices; ++j) {
                float u = float(j) / slices;
                float theta = u * glm::two_pi<float>();
                glm::vec3 p = glm::vec3(
                    std::sin(phi) * std::cos(theta),
                    std::cos(phi),
                    std::sin(phi) * std::sin(theta)
                );
                pos.push_back(p);
                nrm.push_back(glm::normalize(p));
                uv.push_back(glm::vec2(u, 1.0f - v));
            }
        }
        auto ix = [slices](int i, int j) { return i * (slices + 1) + j; };
        for (int i = 0; i < stacks; ++i) {
            for (int j = 0; j < slices; ++j) {
                int a = ix(i, j);
                int b = ix(i + 1, j);
                int c = ix(i + 1, j + 1);
                int d = ix(i, j + 1);
                idx.push_back(a);
                idx.push_back(b);
                idx.push_back(c);
                idx.push_back(a);
                idx.push_back(c);
                idx.push_back(d);
            }
        }

        struct V {
            glm::vec3 p;
            glm::vec3 n;
            glm::vec2 t;
        };
        std::vector<V> verts(pos.size());
        for (size_t i = 0; i < pos.size(); ++i) verts[i] = {pos[i], nrm[i], uv[i]};

        glGenVertexArrays(1, &m_sunVAO);
        glGenBuffers(1, &m_sunVBO);
        glGenBuffers(1, &m_sunEBO);
        glBindVertexArray(m_sunVAO);
        glBindBuffer(GL_ARRAY_BUFFER, m_sunVBO);
        glBufferData(GL_ARRAY_BUFFER, GLsizeiptr(verts.size() * sizeof(V)), verts.data(), GL_STATIC_DRAW);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_sunEBO);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, GLsizeiptr(idx.size() * sizeof(uint32_t)), idx.data(), GL_STATIC_DRAW);

        glEnableVertexAttribArray(0); // pos
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(V), (void*)offsetof(V, p));
        glEnableVertexAttribArray(1); // nrm
        glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(V), (void*)offsetof(V, n));
        glEnableVertexAttribArray(2); // uv
        glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(V), (void*)offsetof(V, t));

        glBindVertexArray(0);
        m_sunIndexCount = int(idx.size());
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
    bool m_useMaterial{true};

    // Matrices
    glm::mat4 m_projectionMatrix = glm::perspective(glm::radians(80.0f), 1.0f, 0.1f, 30.0f);
    glm::mat4 m_viewMatrix = glm::lookAt(glm::vec3(-1, 1, -1), glm::vec3(0), glm::vec3(0, 1, 0));
    glm::mat4 m_modelMatrix{1.0f};

    // --- Inner path (camera target) ---
    BezierPath m_path{200};
    bool  m_showPath   = true;
    float m_pathU      = 0.0f;
    float m_pathSpeed  = 0.05f;
    GLuint m_basicLineProgram = 0;

    // Scene graph
    SceneNode *m_probeRoot = nullptr;          // inner single dragon
    SceneNode *m_probeAntennaBase = nullptr;   // first outer dragon (child of escort root)
    SceneNode *m_probeAntennaTip  = nullptr;   // second outer dragon (child of base)
    SceneNode *m_escortRoot = nullptr;         // parent for the two stacked dragons (outer)

    float m_probeScale = 0.12f;
    bool  m_chaseCam   = true;

    std::unique_ptr<Skybox> m_sky;
    Shader m_skyShader;
    bool   m_useEnvMap = true;

    std::unique_ptr<Texture> m_texAlbedo;
    std::unique_ptr<Texture> m_texNormal;
    std::unique_ptr<Texture> m_texRoughness;
    std::unique_ptr<Texture> m_texMetallic;
    bool   m_usePBR = true;

    int   m_camMode    = 0; // 0=chase, 1=top, 2=orbit
    float m_orbitAngle = 0.0f;

    // ---- Sun (sphere + light) ----
    GLuint m_sunVAO = 0, m_sunVBO = 0, m_sunEBO = 0;
    int    m_sunIndexCount = 0;
    std::unique_ptr<Texture> m_texSun;
    glm::vec3 m_sunPos = glm::vec3(0.0f, 1.2f, 0.0f);
    float     m_sunRadius = 0.6f;
    float     m_sunIntensity = 12.0f;
    bool      m_drawStaticScene = false;

    // --- Outer path for the two stacked dragons ---
    BezierPath m_pathOuter{200};
    float m_pathOuterU      = 0.0f;
    float m_pathOuterSpeed  = 0.035f;
    float m_pathOuterRadius = 7.0f;
};

int main() {
    Application app;
    app.update();
    return 0;
}
