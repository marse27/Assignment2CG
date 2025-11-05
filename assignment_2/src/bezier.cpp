#include "bezier.h"
#include <glm/glm.hpp>
#include <algorithm>
#include <cmath>

glm::vec3 CubicBezier::eval(float t) const {
    float u = 1.0f - t;
    float uu = u * u;
    float tt = t * t;
    return u*uu * p0 + 3.0f*uu*t * p1 + 3.0f*u*tt * p2 + tt*t * p3;
}

glm::vec3 CubicBezier::tangent(float t) const {
    float u = 1.0f - t;
    return 3.0f*u*u*(p1 - p0) + 6.0f*u*t*(p2 - p1) + 3.0f*t*t*(p3 - p2);
}

BezierPath::BezierPath(int samplesPerSeg)
    : m_samplesPerSeg(std::max(16, samplesPerSeg)) {
    // no GL in ctor (lazy)
}

BezierPath::~BezierPath() {
    if (m_vbo) glDeleteBuffers(1, &m_vbo);
    if (m_vao) glDeleteVertexArrays(1, &m_vao);
}

void BezierPath::ensureGL() const {
    if (!m_vao) glGenVertexArrays(1, &m_vao);
    if (!m_vbo) glGenBuffers(1, &m_vbo);
}

void BezierPath::setSegments(const std::vector<CubicBezier>& segs) {
    m_segments = segs;
    rebuildArcLengthLUT();
    rebuildGLLine();
}

glm::vec3 BezierPath::evalSeg(int i, float t) const { return m_segments[(size_t)i].eval(t); }
glm::vec3 BezierPath::tangentSeg(int i, float t) const { return m_segments[(size_t)i].tangent(t); }

static float dist3(const glm::vec3& a, const glm::vec3& b) {
    glm::vec3 d = b - a;
    return std::sqrt(glm::dot(d, d));
}

void BezierPath::rebuildArcLengthLUT() {
    m_luts.clear();
    m_segLengths.clear();
    m_totalLength = 0.0f;

    m_luts.resize(m_segments.size());
    m_segLengths.resize(m_segments.size(), 0.0f);

    for (size_t si = 0; si < m_segments.size(); ++si) {
        const auto& seg = m_segments[si];
        auto& lut = m_luts[si];
        lut.clear();
        lut.reserve((size_t)m_samplesPerSeg + 1);

        float s = 0.0f;
        glm::vec3 prev = seg.eval(0.0f);
        lut.push_back({0.0f, 0.0f});

        for (int k = 1; k <= m_samplesPerSeg; ++k) {
            float t = float(k) / float(m_samplesPerSeg);
            glm::vec3 p = seg.eval(t);
            s += dist3(prev, p);
            lut.push_back({t, s});
            prev = p;
        }
        m_segLengths[si] = s;
        m_totalLength += s;
    }
}

glm::vec3 BezierPath::sample(float u) const {
    if (m_segments.empty()) return glm::vec3(0);
    u = std::clamp(u, 0.0f, 1.0f);
    float target = u * m_totalLength;

    float acc = 0.0f;
    int seg = 0;
    for (; seg < (int)m_segments.size(); ++seg) {
        if (target <= acc + m_segLengths[(size_t)seg] || seg == (int)m_segments.size() - 1) {
            target -= acc;
            break;
        }
        acc += m_segLengths[(size_t)seg];
    }

    const auto& lut = m_luts[(size_t)seg];
    int lo = 0, hi = (int)lut.size() - 1;
    while (lo + 1 < hi) {
        int mid = (lo + hi) / 2;
        if (lut[(size_t)mid].s < target) lo = mid;
        else hi = mid;
    }

    const auto& a = lut[(size_t)lo];
    const auto& b = lut[(size_t)hi];
    float span = std::max(1e-6f, b.s - a.s);
    float alpha = std::clamp((target - a.s) / span, 0.0f, 1.0f);
    float t = (1.0f - alpha) * a.t + alpha * b.t;
    return evalSeg(seg, t);
}

glm::vec3 BezierPath::tangentAt(float u) const {
    if (m_segments.empty()) return glm::vec3(0,0,1);
    u = std::clamp(u, 0.0f, 1.0f);
    float target = u * m_totalLength;

    float acc = 0.0f;
    int seg = 0;
    for (; seg < (int)m_segments.size(); ++seg) {
        if (target <= acc + m_segLengths[(size_t)seg] || seg == (int)m_segments.size() - 1) {
            target -= acc;
            break;
        }
        acc += m_segLengths[(size_t)seg];
    }

    const auto& lut = m_luts[(size_t)seg];
    int lo = 0, hi = (int)lut.size() - 1;
    while (lo + 1 < hi) {
        int mid = (lo + hi) / 2;
        if (lut[(size_t)mid].s < target) lo = mid;
        else hi = mid;
    }

    const auto& a = lut[(size_t)lo];
    const auto& b = lut[(size_t)hi];
    float span = std::max(1e-6f, b.s - a.s);
    float alpha = std::clamp((target - a.s) / span, 0.0f, 1.0f);
    float t = (1.0f - alpha) * a.t + alpha * b.t;

    glm::vec3 T = tangentSeg(seg, t);
    float len2 = glm::dot(T, T);
    if (len2 < 1e-12f) return glm::vec3(0,0,1);
    return T / std::sqrt(len2);
}

void BezierPath::rebuildGLLine() {
    if (m_segments.empty()) {
        m_totalLineVerts = 0;
        return;
    }
    std::vector<glm::vec3> pts;
    pts.reserve(m_segments.size() * ((size_t)m_samplesPerSeg + 1));
    for (size_t si = 0; si < m_segments.size(); ++si) {
        for (int k = 0; k <= m_samplesPerSeg; ++k) {
            float t = float(k) / float(m_samplesPerSeg);
            pts.push_back(m_segments[si].eval(t));
        }
    }
    m_totalLineVerts = (int)pts.size();

    ensureGL();

    glBindVertexArray(m_vao);
    glBindBuffer(GL_ARRAY_BUFFER, m_vbo);
    glBufferData(GL_ARRAY_BUFFER, (GLsizeiptr)(sizeof(glm::vec3) * pts.size()), pts.data(), GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(glm::vec3), (void*)0);
    glBindVertexArray(0);
}

void BezierPath::drawGL() const {
    if (m_totalLineVerts == 0) return;
    ensureGL();
    glBindVertexArray(m_vao);
    glDrawArrays(GL_LINE_STRIP, 0, m_totalLineVerts);
    glBindVertexArray(0);
}
