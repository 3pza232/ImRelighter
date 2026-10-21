#pragma once

// GLSL ES 3.00 port of the desktop lighting model. It keeps the same
// multi-light, point/spot, physical/classic and tonemap concepts while using
// the mobile renderer's RGB8 depth and normal textures.
constexpr const char* kLightingFragmentShaderEs = R"(#version 300 es
precision highp float;
precision highp int;

in vec2 vUv;
out vec4 fragColor;

uniform sampler2D uAlbedo;
uniform sampler2D uNormal;
uniform sampler2D uHeight;
uniform vec2 uImageSize;
uniform float uAmplitude;
uniform vec3 uAmbientColor;
uniform float uAmbient;
uniform float uWrap;
uniform float uSpecular;
uniform float uShininess;
uniform float uRim;
uniform float uRoughness;
uniform float uMetalness;
uniform float uSheen;
uniform float uSubsurface;
uniform float uExposure;
uniform float uNormalStrength;
uniform float uMix;
uniform int uMode;
uniform int uLightingModel;
uniform int uToneMap;
uniform int uShadowsEnabled;
uniform int uLightCount;
uniform vec3 uLightPos[8];
uniform vec3 uLightColor[8];
uniform float uLightIntensity[8];
uniform float uLightRange[8];
uniform int uLightEnabled[8];
uniform int uLightType[8];
uniform float uLightSpotCos[8];
uniform float uLightSpotSoft[8];

vec3 linearize(vec3 color) {
    return mix(color / 12.92,
               pow((color + 0.055) / 1.055, vec3(2.4)),
               step(vec3(0.04045), color));
}

vec3 encode(vec3 color) {
    color = max(color, vec3(0.0));
    return mix(color * 12.92,
               1.055 * pow(color, vec3(1.0 / 2.4)) - 0.055,
               step(vec3(0.0031308), color));
}

vec3 tonemap(vec3 color) {
    if (uToneMap == 1) {
        const float a = 2.51;
        const float b = 0.03;
        const float c = 2.43;
        const float d = 0.59;
        const float e = 0.14;
        color = max(color, vec3(0.0));
        return clamp((color * (a * color + b)) /
                         (color * (c * color + d) + e),
                     0.0, 1.0);
    }
    const vec3 knee = vec3(0.8);
    color = max(color, vec3(0.0));
    vec3 over = max(color - knee, vec3(0.0));
    return min(color, knee) +
           (vec3(1.0) - knee) *
               (vec3(1.0) - exp(-over / (vec3(1.0) - knee)));
}

float distributionGGX(float ndh, float roughness) {
    float a = roughness * roughness;
    float a2 = a * a;
    float denom = ndh * ndh * (a2 - 1.0) + 1.0;
    return a2 / max(3.14159265 * denom * denom, 1e-5);
}

float geometrySchlickGGX(float ndx, float roughness) {
    float r = roughness + 1.0;
    float k = (r * r) / 8.0;
    return ndx / max(ndx * (1.0 - k) + k, 1e-5);
}

float geometrySmith(float ndv, float ndl, float roughness) {
    return geometrySchlickGGX(ndv, roughness) *
           geometrySchlickGGX(ndl, roughness);
}

vec3 fresnelSchlick(float cosTheta, vec3 f0) {
    return f0 + (vec3(1.0) - f0) * pow(1.0 - cosTheta, 5.0);
}

vec3 positionAt(vec2 uv, float height) {
    float aspect = uImageSize.x / max(uImageSize.y, 1.0);
    return vec3((uv * 2.0 - 1.0) * vec2(aspect, -1.0), height);
}

void main() {
    vec4 source = texture(uAlbedo, vUv);
    vec3 base = linearize(source.rgb);
    float height = texture(uHeight, vUv).r;
    vec3 normal = normalize(texture(uNormal, vUv).rgb * 2.0 - 1.0);
    normal = normalize(mix(vec3(0.0, 0.0, 1.0), normal, uNormalStrength));
    vec3 p = positionAt(vUv, height * uAmplitude);
    vec3 viewDir = vec3(0.0, 0.0, 1.0);

    vec3 ambient = pow(max(uAmbientColor, vec3(0.0)), vec3(2.2)) * uAmbient;
    vec3 direct = vec3(0.0);
    vec3 highlights = vec3(0.0);

    for (int i = 0; i < 8; ++i) {
        if (i >= uLightCount || uLightEnabled[i] == 0) {
            continue;
        }
        vec3 offset = uLightPos[i] - p;
        float distanceToLight = max(length(offset), 0.01);
        vec3 l = offset / distanceToLight;
        float attenuation = 1.0 / (1.0 + pow(distanceToLight /
                                              max(uLightRange[i], 0.01), 2.0));

        if (uLightType[i] == 1) {
            vec3 spotDirection = normalize(-uLightPos[i] + vec3(1e-5));
            float cone = dot(-l, spotDirection);
            float inner = mix(uLightSpotCos[i], 1.0,
                              clamp(uLightSpotSoft[i], 0.0, 1.0));
            attenuation *= smoothstep(uLightSpotCos[i], inner, cone);
        }

        float ndl = dot(normal, l);
        float wrapAmount = max(uWrap + uSubsurface, 0.0);
        float diffuse =
            clamp((ndl + wrapAmount) / (1.0 + wrapAmount), 0.0, 1.0);
        vec3 radiance = pow(max(uLightColor[i], vec3(0.0)), vec3(2.2)) *
                        uLightIntensity[i] * attenuation;

        direct += radiance * diffuse * (1.0 - uMetalness);
        vec3 halfVector = normalize(l + viewDir);
        float ndv = max(dot(normal, viewDir), 0.001);
        float ndlSafe = max(ndl, 0.001);
        float ndh = max(dot(normal, halfVector), 0.0);
        if (uLightingModel == 0) {
            float shine = pow(max(dot(normal, halfVector), 0.0),
                              max(uShininess, 1.0)) *
                          uSpecular;
            highlights += radiance * shine;
        } else {
            float roughness = clamp(uRoughness, 0.05, 1.0);
            float D = distributionGGX(ndh, roughness);
            float G = geometrySmith(ndv, ndlSafe, roughness);
            vec3 F0 = mix(vec3(0.04), base, clamp(uMetalness, 0.0, 1.0));
            vec3 F = fresnelSchlick(max(dot(viewDir, halfVector), 0.0), F0);
            vec3 spec = min((D * G * F) / max(4.0 * ndv * ndlSafe, 0.05),
                            vec3(1.0));
            highlights += radiance * spec * uSpecular;
        }
        float rim = pow(1.0 - max(normal.z, 0.0), 3.0) *
                    max(ndl, 0.0) * uRim;
        highlights += radiance * rim;
        highlights += radiance * pow(1.0 - ndv, 3.0) * uSheen * 0.25;
    }

    vec3 result = base * (ambient + direct) + highlights;
    result *= exp2(uExposure);
    vec3 only = ambient + direct + highlights;
    vec3 outputColor = tonemap(result);
    if (uMode == 1) {
        float shadow = clamp(1.0 - dot(direct, vec3(0.3333)), 0.0, 1.0);
        outputColor = vec3(shadow);
    } else if (uMode == 2) {
        outputColor = tonemap(only);
    }
    outputColor = encode(clamp(outputColor, 0.0, 1.0));
    fragColor = vec4(outputColor, source.a);
}
)";
