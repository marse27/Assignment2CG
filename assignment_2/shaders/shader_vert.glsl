#version 410 core
layout(location=0) in vec3 aPos;
layout(location=1) in vec3 aNormal;
layout(location=2) in vec2 aTex;
// If your VAO has a tangent at location 3, we use it. If not, we’ll build TBN in fragment.
// layout(location=3) in vec3 aTangent;  // optional

uniform mat4 mvpMatrix;
uniform mat4 modelMatrix;
uniform mat3 normalModelMatrix;

out vec3 vWorldPos;
out vec3 vWorldNrm;
out vec2 vUv;
// Optional pass-through if you wire tangents later
// out vec3 vWorldTan;

void main() {
    vWorldPos = vec3(modelMatrix * vec4(aPos, 1.0));
    vWorldNrm = normalize(normalModelMatrix * aNormal);
    vUv = aTex;
    // vWorldTan = normalize(mat3(modelMatrix) * aTangent); // if you enable tangents
    gl_Position = mvpMatrix * vec4(aPos, 1.0);
}
