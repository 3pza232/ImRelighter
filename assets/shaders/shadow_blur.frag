#version 450 core

in vec2 vUv;
layout(location = 0) out vec4 outBlur0;
layout(location = 1) out vec4 outBlur1;

uniform sampler2D uShadow0;
uniform sampler2D uShadow1;
uniform sampler2D uHeight;
uniform sampler2D uColor;
uniform vec2 uTexel;
uniform float uAmplitude;

void main() {
    vec4 c0 = texture(uColor, vUv);
    float h0 = texture(uHeight, vUv).r;

    if (c0.a < 0.01) {
        outBlur0 = texture(uShadow0, vUv);
        outBlur1 = texture(uShadow1, vUv);
        return;
    }

    vec4 sum0 = vec4(0.0);
    vec4 sum1 = vec4(0.0);
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
            sum0 += texture(uShadow0, uv) * weight;
            sum1 += texture(uShadow1, uv) * weight;
            weightSum += weight;
        }
    }

    if (weightSum <= 0.0) {
        outBlur0 = texture(uShadow0, vUv);
        outBlur1 = texture(uShadow1, vUv);
    } else {
        outBlur0 = sum0 / weightSum;
        outBlur1 = sum1 / weightSum;
    }
}