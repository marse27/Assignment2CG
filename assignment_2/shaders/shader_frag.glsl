#version 410 core

in vec3 vWorldPos;
in vec3 vWorldNrm;
in vec2 vTex;

out vec4 fragColor;

// From your C++ code
uniform bool hasTexCoords;
uniform bool useMaterial;   // kept for compatibility; not used in this minimal version
uniform sampler2D colorMap;

// Environment mapping
uniform bool useEnvMap;
uniform samplerCube envMap;
uniform vec3 camPos;

void main() {
    vec3 baseColor = vec3(1.0);

    if (hasTexCoords) {
        baseColor = texture(colorMap, vTex).rgb;
    }

    vec3 N = normalize(vWorldNrm);
    vec3 V = normalize(camPos - vWorldPos);

    vec3 color = baseColor;
    if (useEnvMap) {
        vec3 R   = reflect(-V, N);
        vec3 env = texture(envMap, R).rgb;
        color = mix(baseColor, env, 0.4);  // cheap reflection mix
    }

    fragColor = vec4(color, 1.0);
}
