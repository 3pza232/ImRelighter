#pragma once

// GLSL ES 3.00 adaptation of the desktop shadow.frag. The desktop pass writes
// two RGBA8 targets; the mobile build runs it twice with uLightBase 0 and 4.
constexpr const char* kShadowFragmentShaderEs = R"(#version 300 es
precision highp float;
precision highp int;

in vec2 vUv;
out vec4 fragColor;

uniform sampler2D uHeight;
uniform sampler2D uColor;
uniform vec2 uImageSize;
uniform float uAmplitude;
uniform float uBias;
uniform float uSoftness;
uniform int uLightCount;
uniform int uLightBase;
uniform int uSteps;
uniform int uSamples;
uniform int uShadowsEnabled;
uniform vec3 uLightPos[8];
uniform int uLightEnabled[8];

vec2 worldToUv(vec2 p) {
    float aspect = uImageSize.x / max(uImageSize.y, 1.0);
    return vec2(p.x / aspect * 0.5 + 0.5, 0.5 - p.y * 0.5);
}

float visibility(vec3 p, vec3 lightPosition, int lightIndex) {
    float result = 0.0;
    int samples = clamp(uSamples, 1, 8);
    float goldenAngle = 2.39996323;
    float radius = max(uSoftness, 0.0) * 0.55;

    for (int sampleIndex = 0; sampleIndex < 8; ++sampleIndex) {
        if (sampleIndex >= samples) break;
        float angle = float(sampleIndex) * goldenAngle + float(lightIndex) * 0.7;
        float r = radius * sqrt(float(sampleIndex + 1) / float(samples));
        vec3 target = lightPosition + vec3(cos(angle) * r, sin(angle) * r, 0.0);
        vec3 delta = target - p;

        float visible = 1.0;
        for (int stepIndex = 1; stepIndex <= 192; ++stepIndex) {
            if (stepIndex > uSteps) break;
            float t = float(stepIndex) / float(max(uSteps, 1));
            vec3 q = p + delta * t;
            if (q.z > uAmplitude + uBias) break;

            vec2 uv = worldToUv(q.xy);
            if (uv.x < 0.0 || uv.x > 1.0 || uv.y < 0.0 || uv.y > 1.0) {
                break;
            }
            if (texture(uColor, uv).a < 0.05) continue;
            float height = texture(uHeight, uv).r * uAmplitude;
            if (height > q.z + uBias) {
                visible = 0.0;
                break;
            }
        }
        result += visible;
    }
    return result / float(max(samples, 1));
}

void main() {
    if (uShadowsEnabled == 0) {
        fragColor = vec4(1.0);
        return;
    }

    vec4 color = texture(uColor, vUv);
    float height = texture(uHeight, vUv).r * uAmplitude;
    float aspect = uImageSize.x / max(uImageSize.y, 1.0);
    vec3 p = vec3((vUv * 2.0 - 1.0) * vec2(aspect, -1.0), height);

    vec4 values = vec4(1.0);
    for (int i = 0; i < 4; ++i) {
        int light = uLightBase + i;
        if (light >= uLightCount || uLightEnabled[light] == 0 || color.a < 0.05) {
            continue;
        }
        float v = visibility(p, uLightPos[light], light);
        if (i == 0) values.x = v;
        else if (i == 1) values.y = v;
        else if (i == 2) values.z = v;
        else values.w = v;
    }
    fragColor = values;
}
)";

constexpr const char* kShadowBlurFragmentShaderEs = R"(#version 300 es
precision highp float;

in vec2 vUv;
out vec4 fragColor;

uniform sampler2D uShadow;
uniform sampler2D uHeight;
uniform sampler2D uColor;
uniform vec2 uTexel;
uniform float uAmplitude;

void main() {
    vec4 c0 = texture(uColor, vUv);
    float h0 = texture(uHeight, vUv).r;
    if (c0.a < 0.01) {
        fragColor = texture(uShadow, vUv);
        return;
    }

    vec4 sum = vec4(0.0);
    float weightSum = 0.0;
    for (int y = -1; y <= 1; ++y) {
        for (int x = -1; x <= 1; ++x) {
            vec2 uv = vUv + vec2(float(x), float(y)) * uTexel;
            if (uv.x < 0.0 || uv.x > 1.0 || uv.y < 0.0 || uv.y > 1.0) {
                continue;
            }
            vec4 color = texture(uColor, uv);
            if (abs(color.a - c0.a) > 0.1) continue;
            float h = texture(uHeight, uv).r;
            float weight = exp(-float(x * x + y * y) * 0.7);
            weight *= exp(-abs(h - h0) * 180.0 / max(uAmplitude, 0.01));
            if (weight < 1e-5) continue;
            sum += texture(uShadow, uv) * weight;
            weightSum += weight;
        }
    }
    fragColor = weightSum <= 0.0 ? texture(uShadow, vUv) : sum / weightSum;
}
)";
