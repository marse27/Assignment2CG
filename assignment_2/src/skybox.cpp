#include "skybox.h"
#include <framework/shader.h>
#include <stb/stb_image.h>
#include <vector>
#include <iostream>

static const float CUBE_VERTS[] = {
    // 36 verts, a big cube
    -1,-1,-1,  1,-1,-1,  1, 1,-1,  1, 1,-1, -1, 1,-1, -1,-1,-1,
    -1,-1, 1,  1,-1, 1,  1, 1, 1,  1, 1, 1, -1, 1, 1, -1,-1, 1,
    -1, 1, 1, -1, 1,-1, -1,-1,-1, -1,-1,-1, -1,-1, 1, -1, 1, 1,
     1, 1, 1,  1, 1,-1,  1,-1,-1,  1,-1,-1,  1,-1, 1,  1, 1, 1,
    -1,-1,-1,  1,-1,-1,  1,-1, 1,  1,-1, 1, -1,-1, 1, -1,-1,-1,
    -1, 1,-1,  1, 1,-1,  1, 1, 1,  1, 1, 1, -1, 1, 1, -1, 1,-1
};

static GLuint loadCubemap(const std::array<std::string,6>& faces) {
    GLuint tex; glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_CUBE_MAP, tex);

    stbi_set_flip_vertically_on_load(false);
    int w,h,n;
    for (int i=0;i<6;++i) {
        unsigned char* data = stbi_load(faces[i].c_str(), &w,&h,&n, 0);
        if (!data) { std::cerr << "Failed to load cubemap face: " << faces[i] << "\n"; continue; }
        GLenum fmt = (n==4 ? GL_RGBA : GL_RGB);
        glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, 0, fmt, w, h, 0, fmt, GL_UNSIGNED_BYTE, data);
        stbi_image_free(data);
    }
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
    return tex;
}

Skybox::Skybox(const std::array<std::string,6>& faces) {
    m_cubemap = loadCubemap(faces);
    glGenVertexArrays(1, &m_vao);
    glGenBuffers(1, &m_vbo);
    glBindVertexArray(m_vao);
    glBindBuffer(GL_ARRAY_BUFFER, m_vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(CUBE_VERTS), CUBE_VERTS, GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0,3,GL_FLOAT,GL_FALSE,3*sizeof(float),(void*)0);
    glBindVertexArray(0);
}

Skybox::~Skybox() {
    if (m_vbo) glDeleteBuffers(1,&m_vbo);
    if (m_vao) glDeleteVertexArrays(1,&m_vao);
    if (m_cubemap) glDeleteTextures(1,&m_cubemap);
}

void Skybox::draw(const Shader& shader, const glm::mat4& proj, const glm::mat4& viewNoTrans) const {
    glDepthFunc(GL_LEQUAL);      // draw behind everything
    shader.bind();
    glUniformMatrix4fv(shader.getUniformLocation("uProj"), 1, GL_FALSE, &proj[0][0]);
    glUniformMatrix4fv(shader.getUniformLocation("uView"), 1, GL_FALSE, &viewNoTrans[0][0]);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_CUBE_MAP, m_cubemap);
    glUniform1i(shader.getUniformLocation("uSky"), 0);

    glBindVertexArray(m_vao);
    glDrawArrays(GL_TRIANGLES, 0, 36);
    glBindVertexArray(0);
    glDepthFunc(GL_LESS);
}
