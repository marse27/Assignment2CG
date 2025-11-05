#pragma once
#include <glm/glm.hpp>
#include <vector>
#include <glad/glad.h>

struct CubicBezier {
    glm::vec3 p0, p1, p2, p3;
    glm::vec3 eval(float t) const;
    glm::vec3 tangent(float t) const;
};

class BezierPath {
public:
    explicit BezierPath(int samplesPerSeg = 200);
    ~BezierPath();

    void setSegments(const std::vector<CubicBezier>& segs);

    glm::vec3 sample(float u) const;
    glm::vec3 tangentAt(float u) const;

    glm::vec3 evalSeg(int segIdx, float t) const;
    glm::vec3 tangentSeg(int segIdx, float t) const;

    void drawGL() const;

    float length() const { return m_totalLength; }

private:
    void rebuildArcLengthLUT();
    void rebuildGLLine();
    void ensureGL() const;  // lazy VAO/VBO creation

    struct ArcEntry { float t; float s; };

    std::vector<CubicBezier> m_segments;
    std::vector<std::vector<ArcEntry>> m_luts;
    std::vector<float> m_segLengths;
    float m_totalLength = 0.0f;

    mutable GLuint m_vao = 0;
    mutable GLuint m_vbo = 0;
    int m_samplesPerSeg;
    int m_totalLineVerts = 0;
};
