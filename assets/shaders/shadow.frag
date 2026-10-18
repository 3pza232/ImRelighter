#version 450 core

in vec2 vUv;
layout(location = 0) out vec4 outShadow0;
layout(location = 1) out vec4 outShadow1;

uniform sampler2D uHeight;
uniform sampler2D uColor;
uniform vec2 uTexel;
uniform vec2 uImageSize;
uniform float uAmplitude;
uniform float uBias;
uniform float uSoftness;
uniform int uLightCount;
uniform int uSteps;
uniform int uSamples;
uniform int uShadowsEnabled;
uniform vec3 uLightPos[8];
uniform int uLightEnabled[8];

vec2 worldToUv(vec2 p) {
    float aspect = uImageSize.x / max(uImageSize.y, 1.0);
    return vec2(p.x / aspect * 0.5 + 0.5, 0.5 - p.y * 0.5);
}

float visibility(vec2 baseUv, vec3 p, vec3 lightPosition, int lightIndex) {
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
        outShadow0 = vec4(1.0);
        outShadow1 = vec4(1.0);
        return;
    }

    vec4 color = texture(uColor, vUv);
    float height = texture(uHeight, vUv).r * uAmplitude;
    float aspect = uImageSize.x / max(uImageSize.y, 1.0);
    vec3 p = vec3((vUv * 2.0 - 1.0) * vec2(aspect, -1.0), height);

    float values[8];
    for (int i = 0; i < 8; ++i) {
        if (i >= uLightCount || uLightEnabled[i] == 0 || color.a < 0.05) {
            values[i] = 1.0;
        } else {
            values[i] = visibility(vUv, p, uLightPos[i], i);
        }
    }

    outShadow0 = vec4(values[0], values[1], values[2], values[3]);
    outShadow1 = vec4(values[4], values[5], values[6], values[7]);
}