#version 410 core

in vec3 vWorldPos;
in vec3 vWorldNrm;
in vec2 vUv;

out vec4 fragColor;

// toggles
uniform bool hasTexCoords;
uniform bool useEnvMap;
uniform bool usePBR;

// textures
uniform sampler2D colorMap;    // albedo (sRGB assumed handled by loader)
uniform sampler2D normalMap;   // tangent-space normal (linear)
uniform sampler2D roughMap;    // R channel
uniform sampler2D metalMap;    // R channel
uniform samplerCube envMap;

// camera
uniform vec3 camPos;

// sun/light
uniform vec3  sunPos;          // world-space position of sun
uniform float sunIntensity;    // intensity scalar (try 4..20)
uniform int   isSun;           // 1 when drawing sun sphere, else 0
uniform vec3  sunEmissive;     // emissive color multiplier for sun draw

const vec3 F0dielectric = vec3(0.04);

// sRGB->linear helper (usually loader already does it)
vec3 SRGBtoLinear(vec3 c) { return pow(c, vec3(2.2)); }

// Build TBN from derivatives
mat3 computeTBN(vec3 pos, vec3 nrm, vec2 uv) {
    vec3 dp1 = dFdx(pos);
    vec3 dp2 = dFdy(pos);
    vec2 duv1 = dFdx(uv);
    vec2 duv2 = dFdy(uv);
    float det = duv1.x * duv2.y - duv1.y * duv2.x;
    float invDet = (abs(det) < 1e-8) ? 0.0 : 1.0 / det;
    vec3 T = normalize(( dp1 * duv2.y - dp2 * duv1.y) * invDet);
    vec3 B = normalize((-dp1 * duv2.x + dp2 * duv1.x) * invDet);
    // Orthonormalize
    T = normalize(T - nrm * dot(nrm, T));
    B = normalize(cross(nrm, T));
    return mat3(T, B, nrm);
}

// Fresnel-Schlick
vec3 F_Schlick(vec3 F0, float cosTheta) {
    return F0 + (1.0 - F0) * pow(1.0 - cosTheta, 5.0);
}

// GGX NDF
float D_GGX(float NoH, float alpha) {
    float a2 = alpha * alpha;
    float d = (NoH*NoH)*(a2 - 1.0) + 1.0;
    return a2 / (3.14159265 * d * d + 1e-7);
}

// Smith GGX Geometry (Schlick-GGX)
float G_SchlickGGX(float NoX, float k) { return NoX / (NoX * (1.0 - k) + k); }
float G_Smith(float NoV, float NoL, float rough) {
    float k = (rough + 1.0);
    k = (k*k) / 8.0; // UE4 remap
    return G_SchlickGGX(NoV, k) * G_SchlickGGX(NoL, k);
}

void main() {
    // World-space inputs
    vec3 N = normalize(vWorldNrm);
    vec3 V = normalize(camPos - vWorldPos);

    // Material params
    vec3  albedo = vec3(1.0);
    float rough  = 0.6;
    float metal  = 0.0;

    if (hasTexCoords) {
        albedo = texture(colorMap, vUv).rgb;       // assumed already linear
        rough  = texture(roughMap, vUv).r;
        metal  = texture(metalMap,  vUv).r;
    }

    rough = clamp(rough, 0.04, 1.0);
    float alpha = max(1e-3, rough * rough);

    // Normal mapping
    if (hasTexCoords) {
        vec3 nTex = texture(normalMap, vUv).xyz * 2.0 - 1.0;
        mat3 TBN = computeTBN(vWorldPos, N, vUv);
        N = normalize(TBN * nTex);
    }

    // SUN DRAW: emissive and exit
    if (isSun == 1) {
        vec3 base = hasTexCoords ? texture(colorMap, vUv).rgb : vec3(1.0, 0.9, 0.2);
        vec3 glow = base * sunEmissive; // scale this by UI intensity in C++
        fragColor = vec4(glow, 1.0);
        return;
    }

    // Base reflectance
    vec3 F0 = mix(F0dielectric, albedo, metal);

    // Point light from sun
    vec3  Lvec = sunPos - vWorldPos;
    float dist = length(Lvec);
    vec3  L = (dist > 1e-6) ? (Lvec / dist) : vec3(0,1,0);

    float NoL = clamp(dot(N, L), 0.0, 1.0);
    float NoV = clamp(dot(N, V), 0.0, 1.0);
    vec3  H   = normalize(V + L);
    float NoH = clamp(dot(N, H), 0.0, 1.0);
    float VoH = clamp(dot(V, H), 0.0, 1.0);

    // PBR BRDF terms
    vec3  F = F_Schlick(F0, VoH);
    float D = D_GGX(NoH, alpha);
    float G = G_Smith(NoV, NoL, rough);
    vec3  spec = (D * G * F) / max(4.0 * NoV * NoL, 1e-5);
    vec3  kd   = (1.0 - F) * (1.0 - metal);

    // Inverse-square attenuation with small clamp to avoid blowing up
    float atten = sunIntensity / max(dist * dist, 0.25);

    // Direct lighting
    vec3 direct = (kd * albedo / 3.14159265 + spec) * NoL * atten;

    // Very cheap IBL
    vec3 R = reflect(-V, N);
    vec3 envSpec = texture(envMap, R).rgb;
    vec3 envDiff = texture(envMap, N).rgb;
    vec3 ibl = kd * envDiff * 0.3 + envSpec * (0.2 * (1.0 - rough));

    vec3 color = usePBR ? (direct + ibl) : albedo;
    fragColor = vec4(color, 1.0);
}
