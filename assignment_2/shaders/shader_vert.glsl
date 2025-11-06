#version 410 core

// Match VAO attribute locations
layout(location = 0) in vec3 aPos;      // position
layout(location = 1) in vec3 aNormal;   // normal
layout(location = 2) in vec2 aTex;      // texcoord (if present)

// Uniforms you already set in C++
uniform mat4 mvpMatrix;
uniform mat4 modelMatrix;
uniform mat3 normalModelMatrix;

// Varyings to fragment
out vec3 vWorldPos;
out vec3 vWorldNrm;
out vec2 vTex;

void main() {
    vWorldPos = vec3(modelMatrix * vec4(aPos, 1.0));
    vWorldNrm = normalize(normalModelMatrix * aNormal);
    vTex      = aTex;

    gl_Position = mvpMatrix * vec4(aPos, 1.0);
}
