#version 450 core

in vec2 vUv;
out float outHeight;

uniform sampler2D uDepth;
uniform sampler2D uColor;
uniform vec2 uTexel;
uniform vec2 uImageSize;
uniform float uLow;
uniform float uSpan;
uniform float uSmooth;
uniform int uInvert;
uniform int uLineAssist;

float normalizedDepth(vec2 uv) {
    float n = texture(uDepth, uv).r;
    n = clamp((n - uLow) / max(uSpan, 1e-6), 0.0, 1.0);
    return uInvert != 0 ? 1.0 - n : n;
}

void main() {
    float center = normalizedDepth(vUv);
    if (uSmooth < 0.01) {
        outHeight = center;
        return;
    }

    vec4 centerColor = texture(uColor, vUv);
    if (centerColor.a < 0.01) {
        outHeight = center;
        return;
    }

    const vec2 texel = 1.0 / max(uImageSize, vec2(1.0));
    float sum = 0.0;
    float weightSum = 0.0;
    for (int y = -2; y <= 2; ++y) {
        for (int x = -2; x <= 2; ++x) {
            vec2 uv = vUv + vec2(float(x), float(y)) * texel;
            if (uv.x < 0.0 || uv.x > 1.0 || uv.y < 0.0 || uv.y > 1.0) {
                continue;
            }
            vec4 color = texture(uColor, uv);
            if (color.a < 0.01) {
                continue;
            }
            float n = normalizedDepth(uv);
            float weight = exp(-float(x * x + y * y) / 4.0);
            weight *= exp(-abs(n - center) * 60.0);
            if (uLineAssist != 0) {
                weight *= exp(-length(color.rgb - centerColor.rgb) * 12.0);
            }
            if (weight < 1e-5) {
                continue;
            }
            sum += n * weight;
            weightSum += weight;
        }
    }

    outHeight = weightSum > 0.0 ? sum / weightSum : center;
}