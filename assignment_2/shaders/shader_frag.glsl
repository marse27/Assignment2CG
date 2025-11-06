#version 410 core
in vec3 vWorldPos;
in vec3 vWorldNrm;
in vec2 vUv;

out vec4 fragColor;

// toggles
uniform bool hasTexCoords;
uniform bool useEnvMap;
uniform bool usePBR;
uniform bool useNormalMap;     // NEW

// textures
uniform sampler2D colorMap;
uniform sampler2D normalMap;
uniform sampler2D roughMap;
uniform sampler2D metalMap;
uniform samplerCube envMap;

// camera
uniform vec3 camPos;

// sun/light
uniform vec3  sunPos;
uniform float sunIntensity;
uniform int   isSun;
uniform vec3  sunEmissive;

const vec3 F0dielectric = vec3(0.04);

mat3 computeTBN(vec3 pos, vec3 nrm, vec2 uv) {
    vec3 dp1 = dFdx(pos);
    vec3 dp2 = dFdy(pos);
    vec2 duv1 = dFdx(uv);
    vec2 duv2 = dFdy(uv);
    float det = duv1.x * duv2.y - duv1.y * duv2.x;
    float invDet = (abs(det) < 1e-8) ? 0.0 : 1.0 / det;
    vec3 T = normalize(( dp1 * duv2.y - dp2 * duv1.y) * invDet);
    vec3 B = normalize((-dp1 * duv2.x + dp2 * duv1.x) * invDet);
    T = normalize(T - nrm * dot(nrm, T));
    B = normalize(cross(nrm, T));
    return mat3(T, B, nrm);
}

vec3 F_Schlick(vec3 F0, float cosTheta) { return F0 + (1.0 - F0) * pow(1.0 - cosTheta, 5.0); }
float D_GGX(float NoH, float a) { float a2=a*a; float d=(NoH*NoH)*(a2-1.0)+1.0; return a2/(3.14159265*d*d+1e-7); }
float G_SchlickGGX(float NoX, float k){ return NoX/(NoX*(1.0-k)+k); }
float G_Smith(float NoV,float NoL,float rough){ float k=(rough+1.0); k=(k*k)/8.0; return G_SchlickGGX(NoV,k)*G_SchlickGGX(NoL,k); }

void main() {
    // If this draw is the sun, render emissive and get out. No normal map, no PBR.
    if (isSun == 1) {
        vec3 base = hasTexCoords ? texture(colorMap, vUv).rgb : vec3(1.0, 0.9, 0.2);
        fragColor = vec4(base * sunEmissive, 1.0);
        return;
    }

    vec3 N = normalize(vWorldNrm);
    vec3 V = normalize(camPos - vWorldPos);

    // Material
    vec3  albedo = vec3(1.0);
    float rough  = 0.6;
    float metal  = 0.0;
    if (hasTexCoords) {
        albedo = texture(colorMap, vUv).rgb;
        rough  = texture(roughMap, vUv).r;
        metal  = texture(metalMap,  vUv).r;
    }
    rough = clamp(rough, 0.04, 1.0);
    float alpha = max(1e-3, rough * rough);

    // Normal mapping only when requested
    if (hasTexCoords && useNormalMap) {
        vec3 nTex = texture(normalMap, vUv).xyz * 2.0 - 1.0;
        mat3 TBN = computeTBN(vWorldPos, N, vUv);
        N = normalize(TBN * nTex);
    }

    // Point light from sun
    vec3  Lvec = sunPos - vWorldPos;
    float dist = length(Lvec);
    vec3  L = (dist > 1e-6) ? (Lvec / dist) : vec3(0,1,0);

    float NoL = clamp(dot(N, L), 0.0, 1.0);
    float NoV = clamp(dot(N, V), 0.0, 1.0);
    vec3  H   = normalize(V + L);
    float NoH = clamp(dot(N, H), 0.0, 1.0);
    float VoH = clamp(dot(V, H), 0.0, 1.0);

    vec3  F = F_Schlick(mix(F0dielectric, albedo, metal), VoH);
    float D = D_GGX(NoH, alpha);
    float G = G_Smith(NoV, NoL, rough);
    vec3  spec = (D * G * F) / max(4.0 * NoV * NoL, 1e-5);
    vec3  kd   = (1.0 - F) * (1.0 - metal);

    float atten = sunIntensity / max(dist * dist, 0.25); // avoid explosion up close

    vec3 direct = (kd * albedo / 3.14159265 + spec) * NoL * atten;

    // Cheap IBL
    vec3 R = reflect(-V, N);
    vec3 envSpec = texture(envMap, R).rgb;
    vec3 envDiff = texture(envMap, N).rgb;
    vec3 ibl = kd * envDiff * 0.3 + envSpec * (0.2 * (1.0 - rough));

    vec3 color = usePBR ? (direct + ibl) : albedo;
    fragColor = vec4(color, 1.0);
}
