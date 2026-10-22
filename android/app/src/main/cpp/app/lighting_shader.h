#pragma once

// GLSL ES 3.00 port of desktop assets/shaders/lighting.frag.
constexpr const char* kLightingFragmentShaderEs = R"(#version 300 es
precision highp float;
precision highp int;

in vec2 vUv;
out vec4 fragColor;

uniform sampler2D uColor;
uniform sampler2D uNormal;
uniform sampler2D uHeight;
uniform sampler2D uBlur0;
uniform sampler2D uBlur1;

uniform vec2 uImageSize;
uniform float uAmplitude;
uniform vec3 uAmbientColor;
uniform float uAmbient;
uniform float uWrap;
uniform float uSpecular;
uniform float uShininess;
uniform float uRim;
uniform float uMaterialRoughness;
uniform float uMetalness;
uniform float uSheen;
uniform float uSubsurface;
uniform float uExposure;
uniform float uMix;
uniform float uComparison;
uniform int uBaseMode;
uniform int uViewMode;
uniform int uSelectedLight;
uniform int uLightCount;
uniform int uShadowsEnabled;
uniform int uLightingModel;
uniform int uToneMap;

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

vec3 softTonemap(vec3 color) {
    // Preserve the normal exposure range and roll off only highlights, so
    // individual channels do not clip at different points and shift hue.
    const vec3 knee = vec3(0.8);
    color = max(color, vec3(0.0));
    vec3 over = max(color - knee, vec3(0.0));
    return min(color, knee) +
           (vec3(1.0) - knee) * (vec3(1.0) - exp(-over / (vec3(1.0) - knee)));
}

vec3 acesTonemap(vec3 color) {
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

vec3 tonemap(vec3 color) {
    return uToneMap == 1 ? acesTonemap(color) : softTonemap(color);
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

float ditherNoise(vec2 p) {
    return fract(sin(dot(p, vec2(12.9898, 78.233))) * 43758.5453);
}

float sourceLuma(vec2 uv) {
    return dot(texture(uColor, uv).rgb, vec3(0.2126, 0.7152, 0.0722));
}

vec3 positionAt(vec2 uv, float height) {
    float aspect = uImageSize.x / max(uImageSize.y, 1.0);
    return vec3((uv * 2.0 - 1.0) * vec2(aspect, -1.0), height);
}

void main() {
    vec4 source = texture(uColor, vUv);
    vec3 base = linearize(source.rgb);
    float height = texture(uHeight, vUv).r;
    // The normal pass writes an FBO stored bottom-up; flip V only here.
    vec2 normalUv = vec2(vUv.x, 1.0 - vUv.y);
    vec4 normalSample = texture(uNormal, normalUv);
    vec3 n = normalize(normalSample.rgb * 2.0 - 1.0);
    float edgeMask = clamp(normalSample.a, 0.0, 1.0);

    // Surface detail is intentionally high-frequency. Using it directly for the
    // specular lobe turns single pixels into white fireflies, so average the
    // normal over a 2x2 footprint for shading while keeping the detailed normal
    // for diffuse and rim light.
    vec2 normalTexel = 1.0 / vec2(textureSize(uNormal, 0));
    vec3 specularNormal =
        texture(uNormal, normalUv + vec2(-0.5, -0.5) * normalTexel).rgb *
            2.0 -
        1.0;
    specularNormal +=
        texture(uNormal, normalUv + vec2(0.5, -0.5) * normalTexel).rgb * 2.0 -
        1.0;
    specularNormal +=
        texture(uNormal, normalUv + vec2(-0.5, 0.5) * normalTexel).rgb * 2.0 -
        1.0;
    specularNormal +=
        texture(uNormal, normalUv + vec2(0.5, 0.5) * normalTexel).rgb * 2.0 -
        1.0;
    if (dot(specularNormal, specularNormal) < 1e-6) {
        specularNormal = n;
    } else {
        specularNormal = normalize(specularNormal);
    }

    // Local luminance contrast separates woven/cloth/hair micro-structure from
    // broad illumination. Textured regions get a dimmer, less mirror-like
    // highlight; flat regions keep the authored specular response.
    vec2 microStep = 1.5 / vec2(textureSize(uColor, 0));
    float microL = sourceLuma(vUv - vec2(microStep.x, 0.0));
    float microR = sourceLuma(vUv + vec2(microStep.x, 0.0));
    float microB = sourceLuma(vUv - vec2(0.0, microStep.y));
    float microU = sourceLuma(vUv + vec2(0.0, microStep.y));
    float microContrast =
        length(vec2(microR - microL, microU - microB));
    float microRoughness = smoothstep(0.02, 0.14, microContrast);

    // A small depth-curvature term darkens creases where the low-resolution
    // depth map still carries reliable shape, adding contact without a
    // separate ambient-occlusion pass.
    vec2 heightStep = 1.0 / vec2(textureSize(uHeight, 0));
    float heightL = texture(uHeight, vUv - vec2(heightStep.x, 0.0)).r;
    float heightR = texture(uHeight, vUv + vec2(heightStep.x, 0.0)).r;
    float heightB = texture(uHeight, vUv - vec2(0.0, heightStep.y)).r;
    float heightU = texture(uHeight, vUv + vec2(0.0, heightStep.y)).r;
    float curvature =
        (heightL + heightR + heightB + heightU - 4.0 * height) /
        max(uAmplitude, 0.001);
    float cavity = smoothstep(0.01, 0.06, curvature);
    float occlusion = 1.0 - 0.35 * cavity * (1.0 - edgeMask);

    vec3 p = positionAt(vUv, height * uAmplitude);

    vec4 shadow0 = texture(uBlur0, vUv);
    vec4 shadow1 = texture(uBlur1, vUv);
    float visibility[8];
    visibility[0] = shadow0.x;
    visibility[1] = shadow0.y;
    visibility[2] = shadow0.z;
    visibility[3] = shadow0.w;
    visibility[4] = shadow1.x;
    visibility[5] = shadow1.y;
    visibility[6] = shadow1.z;
    visibility[7] = shadow1.w;

    vec3 direct = vec3(0.0);
    vec3 highlights = vec3(0.0);

    for (int i = 0; i < 8; ++i) {
        if (i >= uLightCount || uLightEnabled[i] == 0) continue;

        vec3 offset = uLightPos[i] - p;
        float distanceToLight = max(length(offset), 0.01);
        vec3 l = offset / distanceToLight;
        float attenuation = 1.0 / (1.0 + pow(distanceToLight /
                                              max(uLightRange[i], 0.01), 2.0));
        // A spot cone is expressed around the vector from the light toward the
        // document center. This keeps the control compact (position + angle)
        // while still giving a genuinely directional falloff.
        if (uLightType[i] == 1) {
            vec3 spotDirection = normalize(-uLightPos[i] + vec3(1e-5));
            float cone = dot(-l, spotDirection);
            float inner = mix(uLightSpotCos[i], 1.0,
                              clamp(uLightSpotSoft[i], 0.0, 1.0));
            attenuation *= smoothstep(uLightSpotCos[i], inner, cone);
        }

        float ndl = dot(n, l);
        float wrapAmount = max(uWrap + uSubsurface, 0.0);
        float diffuse =
            clamp((ndl + wrapAmount) / (1.0 + wrapAmount), 0.0, 1.0);

        float vis = uShadowsEnabled != 0 ? visibility[i] : 1.0;
        vec3 radiance = uLightColor[i] * uLightIntensity[i] * attenuation;

        vec3 viewDir = vec3(0.0, 0.0, 1.0);
        vec3 halfVector = normalize(l + viewDir);
        float ndlPositive = max(ndl, 0.0);
        float rim = pow(1.0 - max(n.z, 0.0), 3.0) * ndlPositive * uRim;
        vec3 dielectricF0 = vec3(0.04);
        if (uLightingModel == 0) {
            float shininess =
                mix(uShininess, max(8.0, uShininess * 0.5), microRoughness);
            shininess = mix(shininess, 8.0, uMaterialRoughness);
            float specular = uSpecular * mix(1.0, 0.35, microRoughness);
            float shine =
                pow(max(dot(specularNormal, halfVector), 0.0),
                    max(shininess, 1.0)) *
                specular *
                smoothstep(0.0, 0.15, dot(specularNormal, l));
            shine *= 1.0 - 0.6 * edgeMask;
            vec3 specTint = mix(vec3(1.0), base, uMetalness);
            direct += radiance * diffuse * (1.0 - uMetalness) * vis;
            highlights += radiance * shine * specTint * vis;
        } else {
            // A GGX lobe with Fresnel and Smith visibility replaces the broad
            // Blinn-Phong sheen that made fabric and hair look like leather.
            // A roughness floor keeps 1-pixel normal-map detail from turning
            // into mirror-like fireflies on fabric and hair. Material
            // roughness and metalness let the same depth map represent skin,
            // cloth, hair or metal instead of one generic plastic surface.
            // Shininess controls the base lobe width (hardness), while
            // material roughness adds a controllable roughness floor. This
            // keeps both UI sliders visible instead of letting one hide the
            // other. The default pair still lands near the previous 0.30 base.
            float shininessRoughness =
                clamp(sqrt(2.0 / max(uShininess + 2.0, 1.0)), 0.05, 1.0);
            float baseRoughness =
                clamp(shininessRoughness + uMaterialRoughness * 0.4,
                      0.05, 1.0);
            float rough = clamp(
                mix(baseRoughness, min(baseRoughness + 0.4, 1.0),
                    microRoughness),
                0.05, 1.0);
            float ndv = max(dot(specularNormal, viewDir), 0.001);
            float ndh = max(dot(specularNormal, halfVector), 0.0);
            float vdh = max(dot(viewDir, halfVector), 0.0);
            float ndlSafe = max(dot(specularNormal, l), 0.001);
            float D = distributionGGX(ndh, rough);
            float G = geometrySmith(ndv, ndlSafe, rough);
            vec3 F0 = mix(dielectricF0, base, clamp(uMetalness, 0.0, 1.0));
            vec3 F = fresnelSchlick(vdh, F0);
            // A tiny denominator turns grazing micro-normals into isolated
            // white fireflies. Clamp the geometric term and the final specular
            // so a single normal-map texel cannot dominate the frame.
            vec3 specularTerm =
                (D * G * F) / max(4.0 * ndv * ndlSafe, 0.05);
            specularTerm = min(specularTerm, vec3(1.0));
            specularTerm *= (1.0 - 0.6 * edgeMask);
            vec3 kD = (vec3(1.0) - F) * (1.0 - uMetalness);
            direct += radiance * diffuse * kD * vis;
            highlights += radiance * specularTerm * uSpecular * vis;
        }
        float sheen =
            pow(1.0 - max(dot(n, viewDir), 0.0), 3.0) * uSheen;
        highlights += radiance * sheen * 0.25 * vis * (1.0 - edgeMask);
        // Depth discontinuities are not real surface curvature. Fade the
        // additive highlights there to avoid colored halos on silhouettes.
        rim *= 1.0 - edgeMask;
        highlights += radiance * rim * vis;
    }

    vec3 ambientLight = uAmbientColor * uAmbient;
    vec3 illumination = (ambientLight + direct) * occlusion;
    vec3 result = base * illumination + highlights;
    if (uBaseMode == 0) {
        result = (base * illumination + highlights) / (uAmbient + 0.8);
    }
    // A small warm scatter term makes skin/hair subsurface control visible
    // without introducing a separate transmission pass.
    float scatterAmount = clamp(uSubsurface, 0.0, 1.0);
    if (scatterAmount > 0.0) {
        vec3 scatterTint = base * vec3(1.15, 0.60, 0.45);
        result += scatterTint * scatterAmount *
                  (uAmbient * 0.20 + length(direct) * 0.10);
    }
    result *= exp2(uExposure);

    vec3 outputColor = base;
    if (uViewMode == 0) {
        outputColor = tonemap(result);
    } else if (uViewMode == 1) {
        outputColor = vUv.x < uComparison ? base : tonemap(result);
    } else if (uViewMode == 2) {
        outputColor = base;
    } else if (uViewMode == 3) {
        outputColor = n * 0.5 + 0.5;
    } else if (uViewMode == 4) {
        outputColor = vec3(height / max(uAmplitude, 0.001));
    } else if (uViewMode == 5) {
        outputColor = tonemap(illumination + highlights);
    } else if (uViewMode == 6) {
        float selected = uSelectedLight >= 0 ? visibility[clamp(uSelectedLight, 0, 7)] : 1.0;
        outputColor = vec3(selected);
    }

    if (uViewMode == 3 || uViewMode == 4 || uViewMode == 6) {
        fragColor = vec4(clamp(outputColor, 0.0, 1.0), source.a);
    } else {
        float mixed = uViewMode == 2 ? 1.0 : uMix;
        outputColor = mix(base, outputColor, mixed);
        float dither =
            (ditherNoise(gl_FragCoord.xy) - 0.5) / 255.0;
        fragColor = vec4(clamp(encode(outputColor) + vec3(dither), 0.0, 1.0),
                        source.a);
    }
})";
