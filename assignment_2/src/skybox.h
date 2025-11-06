#pragma once
#include <glad/glad.h>
#include <glm/glm.hpp>
#include <string>
#include <array>

class Shader; // fwd

class Skybox {
public:
    // faces in order: right left top bottom front back
    Skybox(const std::array<std::string,6>& facePaths);
    ~Skybox();

    void draw(const Shader& shader, const glm::mat4& proj, const glm::mat4& viewNoTrans) const;
    GLuint cubemap() const { return m_cubemap; }

private:
    GLuint m_vao = 0, m_vbo = 0, m_cubemap = 0;
};
