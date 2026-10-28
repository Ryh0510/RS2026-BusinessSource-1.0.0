#version 420 core

#include "common/scene_ubo.glsl"


// =================================
// Material Type
// =================================
uniform int uMaterialType;

// =================================
// Base
// =================================
uniform vec4 uBaseColor;

// =================================
// Phong
// =================================
uniform vec3  uSpecular;
uniform float uShininess;

// =================================
// PBR
// =================================
uniform float uMetallic;
uniform float uRoughness;
uniform float uAO;

// =================================
// Emissive
// =================================
uniform vec3 uEmissiveColor;

// =================================
// Texture flags
// =================================
uniform bool uHasBaseColorMap;
uniform bool uHasNormalMap;
uniform bool uHasMetallicMap;
uniform bool uHasRoughnessMap;
uniform bool uHasAOMap;
uniform bool uHasEmissiveMap;
uniform bool uHasMetallicRoughnessMap;

// =================================
// Samplers
// =================================
uniform sampler2D uBaseColorMap;
uniform sampler2D uNormalMap;
uniform sampler2D uMetallicMap;
uniform sampler2D uRoughnessMap;
uniform sampler2D uAOMap;
uniform sampler2D uEmissiveMap;
uniform sampler2D uMetallicRoughnessMap;

// =================================
// Inputs
// =================================
in vec3 vWorldPos;
in vec3 vNormal;
in vec2 vTexCoord;
in vec4 vVertexColor;

out vec4 FragColor;

const float PI = 3.14159265359;

// =================================
// Helpers
// =================================

vec3 gammaToLinear(vec3 c)
{
    return pow(c, vec3(2.2));
}

vec3 linearToGamma(vec3 c)
{
    return pow(c, vec3(1.0/2.2));
}

vec3 getNormal()
{
    vec3 N = normalize(vNormal);

    if (uHasNormalMap)
    {
        vec3 normalTex = texture(uNormalMap, vTexCoord).rgb;
        normalTex = normalTex * 2.0 - 1.0;

        vec3 dpdx = dFdx(vWorldPos);
        vec3 dpdy = dFdy(vWorldPos);
        vec2 dtdx = dFdx(vTexCoord);
        vec2 dtdy = dFdy(vTexCoord);
        float determinant = dtdx.x * dtdy.y - dtdx.y * dtdy.x;
        if(abs(determinant) > 1e-8)
        {
            vec3 T = normalize((dpdx * dtdy.y - dpdy * dtdx.y) / determinant);
            T = normalize(T - N * dot(N, T));
            vec3 B = normalize(cross(N, T)) * (determinant < 0.0 ? -1.0 : 1.0);
            N = normalize(mat3(T, B, N) * normalTex);
        }
    }

    return N;
}

vec3 getBaseColor()
{
    //  vec3 base = uBaseColor.rgb;
    vec3 base = gammaToLinear(uBaseColor.rgb);

    if (uHasBaseColorMap)
        base *= gammaToLinear(texture(uBaseColorMap, vTexCoord).rgb);

    base *= vVertexColor.rgb;

    return base;
}

float getAlpha()
{
    float alpha = uBaseColor.a * vVertexColor.a;

    if (uHasBaseColorMap)
        alpha *= texture(uBaseColorMap, vTexCoord).a;

    return alpha;
}

float getMetallic()
{
    if (uHasMetallicRoughnessMap)
        return uMetallic * texture(uMetallicRoughnessMap, vTexCoord).b;
    if (uHasMetallicMap)
        return texture(uMetallicMap, vTexCoord).r;

    return uMetallic;
}

float getRoughness()
{
    if (uHasMetallicRoughnessMap)
        return uRoughness * texture(uMetallicRoughnessMap, vTexCoord).g;
    if (uHasRoughnessMap)
        return texture(uRoughnessMap, vTexCoord).r;

    return uRoughness;
}

float getAO()
{
    if (uHasAOMap)
        return texture(uAOMap, vTexCoord).r;

    return uAO;
}

vec3 getEmissive()
{
    vec3 e = uEmissiveColor;

    if (uHasEmissiveMap)
        e *= texture(uEmissiveMap, vTexCoord).rgb;

    return e;
}

vec3 scalarOverlayDisplayColor(vec3 N)
{
    vec3 overlayLinear = clamp(uBaseColor.rgb * vVertexColor.rgb, 0.0, 1.0);
    vec3 normal = normalize(N);
    vec3 viewDir = normalize(cameraPos.xyz - vWorldPos);
    vec3 detailLightDir = normalize(vec3(0.35, 0.55, 0.76));

    float diffuse = max(dot(normal, detailLightDir), 0.0);
    float contour = pow(1.0 - abs(dot(normal, viewDir)), 2.0);
    float crease = clamp(length(fwidth(normal)) * 5.0, 0.0, 1.0);
    float detail = 1.0 + (diffuse - 0.5) * 0.34 + contour * 0.12 - crease * 0.10;

    return linearToGamma(clamp(overlayLinear * detail, 0.0, 1.0));
}

float computeSpot(GPULight light, vec3 L)
{
    float innerCone = light.params.z;
    float outerCone = light.params.w;

    vec3 D = normalize(light.direction.xyz);

    float theta = dot(normalize(-L), D);

    float inner = cos(innerCone);
    float outer = cos(outerCone);

    float t = clamp((theta - outer) / (inner - outer), 0.0, 1.0);

    return t;
}

// =================================
// Phong Lighting
// =================================
vec3 phongShading(GPULight light, vec3 N, vec3 baseColor)
{
    int type       = int(light.params.x);
    float range    = light.params.y;

    vec3 lightDir;
    if(type == 0) // directional
    {
        lightDir = normalize(-light.direction.xyz);
    }
    else
    {
        lightDir = normalize(light.position.xyz - vWorldPos);
    }

    vec3 viewDir  = normalize(cameraPos.xyz - vWorldPos);

    float attenuation = 1.0;

    if(range > 0.0)
    {
        float dist = length(light.position.xyz - vWorldPos);
        attenuation = clamp(1.0 - dist / range, 0.0, 1.0);
    }

    // spotlight
    if(type == 2)
    {
        float spot = computeSpot(light, lightDir);
        attenuation *= spot;
    }

    // Diffuse
    float diff = max(dot(N, lightDir), 0.0);
    vec3 diffuse = diff * baseColor * light.color.rgb;

    // Specular
    vec3 reflectDir = reflect(-lightDir, N);
    float spec = pow(max(dot(viewDir, reflectDir), 0.0), uShininess);
    vec3 specular = spec * uSpecular * light.color.rgb;

    return (diffuse + specular) * attenuation;
}

// =================================
// Simple PBR (GGX simplified)
// =================================

float DistributionGGX(vec3 N, vec3 H, float roughness)
{
    float a      = roughness * roughness;
    float a2     = a * a;
    float NdotH  = max(dot(N, H), 0.0);
    float NdotH2 = NdotH * NdotH;

    float denom = (NdotH2 * (a2 - 1.0) + 1.0);
    denom = PI * denom * denom;

    return a2 / max(denom, 0.000001);
}

float GeometrySchlickGGX(float NdotV, float roughness)
{
    float r = (roughness + 1.0);
    float k = (r*r) / 8.0;

    float denom = NdotV * (1.0 - k) + k;
    return NdotV / denom;
}

float GeometrySmith(vec3 N, vec3 V, vec3 L, float roughness)
{
    float NdotV = max(dot(N, V), 0.0);
    float NdotL = max(dot(N, L), 0.0);

    float ggx2 = GeometrySchlickGGX(NdotV, roughness);
    float ggx1 = GeometrySchlickGGX(NdotL, roughness);

    return ggx1 * ggx2;
}

vec3 FresnelSchlick(float cosTheta, vec3 F0)
{
    return F0 + (1.0 - F0) * pow(1.0 - cosTheta, 5.0);
}

vec3 pbrShading(
    GPULight light,
    vec3 N,
    vec3 baseColor,
    float metallic,
    float roughness)
{
    int type       = int(light.params.x);
    float range    = light.params.y;

    vec3 V = normalize(cameraPos.xyz - vWorldPos);

    vec3 L;     //  LightDir
    if(type == 0)
    {
        L = normalize(-light.direction.xyz);
    }
    else
    {
        L = normalize(light.position.xyz - vWorldPos);
    }

    vec3 H = normalize(V + L);

    roughness = clamp(roughness, 0.04, 1.0);

    float NdotL = max(dot(N, L), 0.0);
    float NdotV = max(dot(N, V), 0.0);

    //  ----    Spot Light  ----
    float attenuation = 1.0;

//    float dist = length(light.position.xyz - vWorldPos);
//    attenuation = clamp(1.0 - dist / range, 0.0, 1.0);

    float dist = length(light.position.xyz - vWorldPos);
    if(range > 0.0)
    {
        float dist = length(light.position.xyz - vWorldPos);
        attenuation = 1.0 / (1.0 + dist * dist / (range * range));
    }

    if(type == 2)
    {
        float spot = computeSpot(light, L);
        attenuation *= spot;
    }

    // ---- F0 ----
    vec3 F0 = vec3(0.04);
    F0 = mix(F0, baseColor, metallic);

    // ---- Cook-Torrance ----
    float D = DistributionGGX(N, H, roughness);
    float G = GeometrySmith(N, V, L, roughness);
    vec3  F = FresnelSchlick(max(dot(H, V), 0.0), F0);

    vec3 numerator    = D * G * F;
    float denominator = 4.0 * NdotV * NdotL + 0.0001;
    vec3 specular = numerator / denominator;

    vec3 kS = F;
    vec3 kD = (vec3(1.0) - kS) * (1.0 - metallic);

    vec3 diffuse = kD * baseColor / PI;

    vec3 radiance = light.color.rgb; // HDR 强光

    //vec3 pbrColor = (diffuse + specular) * radiance * NdotL;
    vec3 pbrColor = (diffuse + specular) * radiance * NdotL * attenuation;

    return pbrColor;
}

vec3 ambientLighting(vec3 N, vec3 baseColor, float metallic, float roughness, float ao)
{
    vec3 V = normalize(cameraPos.xyz - vWorldPos);
    float horizon = clamp(N.z * 0.5 + 0.5, 0.0, 1.0);
    vec3 groundLight = vec3(0.055, 0.065, 0.075);
    vec3 skyLight = vec3(0.24, 0.29, 0.34);
    vec3 irradiance = mix(groundLight, skyLight, horizon);
    vec3 F0 = mix(vec3(0.04), baseColor, metallic);
    vec3 fresnel = FresnelSchlick(max(dot(N, V), 0.0), F0);
    vec3 diffuse = (1.0 - fresnel) * (1.0 - metallic) * baseColor;
    vec3 specular = fresnel * mix(0.35, 0.08, roughness);
    return (diffuse * irradiance + specular * skyLight) * ao;
}

vec3 acesToneMap(vec3 color)
{
    const float a = 2.51;
    const float b = 0.03;
    const float c = 2.43;
    const float d = 0.59;
    const float e = 0.14;
    return clamp((color * (a * color + b)) / (color * (c * color + d) + e), 0.0, 1.0);
}

// =================================
// Main
// =================================
void main()
{
    vec3 N = getNormal();
    vec3 baseColor = getBaseColor();
    vec3 emissive = getEmissive();

    if (uMaterialType == 2)       // Unlit
    {
        vec3 unlitColor = linearToGamma(max(baseColor + emissive, vec3(0.0)));
        FragColor = vec4(unlitColor, getAlpha());
        return;
    }

    if (uMaterialType == 3)       // ScalarOverlay
    {
        FragColor = vec4(scalarOverlayDisplayColor(N), getAlpha());
        return;
    }

    vec3 result = vec3(0.0f, 0.0f, 0.0f);
    float metallic = getMetallic();
    float roughness = getRoughness();
    float ao = getAO();


    for(int i=0;i<lightInfo.x;i++)
    {
        GPULight light = lights[i];

        if (uMaterialType == 0)       // Phong
        {
            result += phongShading(light, N, baseColor);
        }
        else if (uMaterialType == 1)  // PBR
        {
            result += pbrShading(light, N, baseColor, metallic, roughness);
        }
    }

    if(uMaterialType == 0)
        result += baseColor * mix(vec3(0.08, 0.09, 0.10), vec3(0.22, 0.25, 0.29), clamp(N.z * 0.5 + 0.5, 0.0, 1.0));
    else
        result += ambientLighting(N, baseColor, metallic, roughness, ao);

    result += emissive;

    result = acesToneMap(result);

    // ===== Gamma Output =====
    result = linearToGamma(result);

    FragColor = vec4(result, getAlpha());
}
