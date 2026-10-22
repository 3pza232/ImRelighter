#pragma once

// GLSL ES 3.00 port of the desktop assets/shaders/normal.frag.
constexpr const char* kNormalFragmentShaderEs = R"(#version 300 es
precision highp float;
precision highp int;

in vec2 vUv;
out vec4 fragColor;

uniform sampler2D uHeight;
uniform sampler2D uColor;
uniform vec2 uTexel;
uniform vec2 uImageSize;
uniform float uPixelScale;
uniform float uAmplitude;
uniform float uNormalStrength;
uniform float uMaxTilt;
uniform float uDetailStrength;
uniform float uDenoise;
uniform vec3 uFlips;

float heightAt(ivec2 pixel) {
    ivec2 size = textureSize(uHeight, 0);
    return texelFetch(uHeight, clamp(pixel, ivec2(0), size - 1), 0).r * uAmplitude;
}

float medianHeight(ivec2 pixel) {
    float values[9];
    int index = 0;
    for (int y = -1; y <= 1; ++y) {
        for (int x = -1; x <= 1; ++x) {
            values[index++] = heightAt(pixel + ivec2(x, y));
        }
    }
    for (int i = 1; i < 9; ++i) {
        float key = values[i];
        int j = i - 1;
        while (j >= 0 && values[j] > key) {
            values[j + 1] = values[j];
            --j;
        }
        values[j + 1] = key;
    }
    return values[4];
}

float colorLuma(vec2 uv) {
    float lod = clamp(log2(max(uPixelScale, 2.0)), 1.0, 2.5);
    vec3 color = textureLod(uColor, uv, lod).rgb;
    return dot(color, vec3(0.2126, 0.7152, 0.0722));
}

void main() {
    ivec2 size = textureSize(uHeight, 0);
    ivec2 pixel = clamp(ivec2(floor(vUv * vec2(size))), ivec2(0), size - 1);
    bool denoise = uDenoise > 0.5;
    float c = denoise ? medianHeight(pixel) : heightAt(pixel);
    float l = denoise ? medianHeight(pixel + ivec2(-1, 0)) : heightAt(pixel + ivec2(-1, 0));
    float r = denoise ? medianHeight(pixel + ivec2(1, 0)) : heightAt(pixel + ivec2(1, 0));
    float u = denoise ? medianHeight(pixel + ivec2(0, -1)) : heightAt(pixel + ivec2(0, -1));
    float b = denoise ? medianHeight(pixel + ivec2(0, 1)) : heightAt(pixel + ivec2(0, 1));

    float dx = (r - l) * 0.5;
    float dy = (u - b) * 0.5;
    float jump = max(max(abs(r - c), abs(l - c)), max(abs(u - c), abs(b - c)));
    float edgeMask = smoothstep(0.08, 0.22, jump / max(uAmplitude, 0.001));

    float scale = max(uPixelScale, 0.000001);
    vec2 slope = vec2(-dx / scale, dy / scale) * uImageSize.y * 0.5;
    slope *= uNormalStrength;

    vec2 fineStep = uTexel * 1.5;
    vec2 wideStep = uTexel * 3.0;
    float fineL = colorLuma(vUv - vec2(fineStep.x, 0.0));
    float fineR = colorLuma(vUv + vec2(fineStep.x, 0.0));
    float fineU = colorLuma(vUv - vec2(0.0, fineStep.y));
    float fineB = colorLuma(vUv + vec2(0.0, fineStep.y));
    float wideL = colorLuma(vUv - vec2(wideStep.x, 0.0));
    float wideR = colorLuma(vUv + vec2(wideStep.x, 0.0));
    float wideU = colorLuma(vUv - vec2(0.0, wideStep.y));
    float wideB = colorLuma(vUv + vec2(0.0, wideStep.y));

    vec2 detail = vec2(-(fineR - fineL), fineU - fineB) -
                  0.5 * vec2(-(wideR - wideL), wideU - wideB);
    detail = clamp(detail, vec2(-0.08), vec2(0.08));
    detail *= 1.0 - edgeMask;
    float darkBoost = mix(2.0, 1.0, smoothstep(0.03, 0.35, colorLuma(vUv)));
    slope += detail * uDetailStrength * 12.0 * darkBoost;

    float slopeLength = length(slope);
    if (slopeLength > 0.000001) {
        float softLength = uMaxTilt * tanh(slopeLength / uMaxTilt);
        slope *= softLength / slopeLength;
    }

    vec3 n = normalize(vec3(slope, 1.0));
    n *= uFlips;
    n = normalize(n);
    fragColor = vec4(n * 0.5 + 0.5, edgeMask);
}
)";
