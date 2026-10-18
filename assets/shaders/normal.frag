#version 450 core

in vec2 vUv;
out vec4 outNormal;

uniform sampler2D uHeight;
uniform sampler2D uColor;
uniform vec2 uTexel;
uniform vec2 uImageSize;
uniform float uAmplitude;
uniform float uNormalStrength;
uniform vec3 uFlips;

float heightAt(vec2 uv) {
    return texture(uHeight, uv).r * uAmplitude;
}

void main() {
    const vec2 texel = 1.0 / max(uImageSize, vec2(1.0));
    float c = heightAt(vUv);
    float l = heightAt(vUv - vec2(texel.x, 0.0));
    float r = heightAt(vUv + vec2(texel.x, 0.0));
    float b = heightAt(vUv - vec2(0.0, texel.y));
    float u = heightAt(vUv + vec2(0.0, texel.y));

    float limit = 0.055 * max(uAmplitude, 0.001);
    float dx = (abs(r - c) < limit && abs(c - l) < limit)
                   ? (r - l) * 0.5
                   : (abs(r - c) < abs(c - l) ? r - c : c - l);
    float dy = (abs(u - c) < limit && abs(c - b) < limit)
                   ? (u - b) * 0.5
                   : (abs(u - c) < abs(c - b) ? u - c : c - b);

    if (min(abs(r - c), abs(l - c)) > limit) dx = 0.0;
    if (min(abs(u - c), abs(b - c)) > limit) dy = 0.0;

    // Gradients are sampled in source-image UV space, matching the reference
    // pipeline at both preview and export resolutions.
    vec3 n = normalize(vec3(-dx * uImageSize.y * 0.5,
                            dy * uImageSize.y * 0.5,
                            1.0));
    n.xy *= uNormalStrength;
    n *= uFlips;
    n = normalize(n);

    outNormal = vec4(n * 0.5 + 0.5, texture(uColor, vUv).a);
}