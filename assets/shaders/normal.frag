#version 450 core

in vec2 vUv;
out vec4 outNormal;

uniform sampler2D uHeight;
uniform sampler2D uColor;
uniform vec2 uTexel;
uniform vec2 uImageSize;
uniform float uPixelScale;
uniform float uAmplitude;
uniform float uNormalStrength;
uniform float uMaxTilt;
uniform vec3 uFlips;

float heightAt(vec2 uv) {
    return texture(uHeight, uv).r * uAmplitude;
}

void main() {
    // Sample the height target directly so preview and export use the same
    // footprint in output pixels. uPixelScale converts the preview gradient
    // back to source-image pixels for a resolution-independent slope.
    vec2 texel = uTexel;
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

    float jump = max(max(abs(r - c), abs(l - c)),
                     max(abs(u - c), abs(b - c)));
    float edgeMask = smoothstep(0.08, 0.22,
                                jump / max(uAmplitude, 1e-3));

    float scale = max(uPixelScale, 1e-6);
    vec2 slope = vec2(-dx / scale, dy / scale) * uImageSize.y * 0.5;
    slope *= uNormalStrength;
    // Relative depth has no reliable absolute scale. Limiting the slope keeps
    // the upsampled depth ramp from turning into a near-horizontal normal and
    // painting a wide rim highlight around silhouettes.
    float slopeLength = length(slope);
    if (slopeLength > uMaxTilt) {
        slope *= uMaxTilt / slopeLength;
    }

    vec3 n = normalize(vec3(slope, 1.0));
    n *= uFlips;
    n = normalize(n);

    outNormal = vec4(n * 0.5 + 0.5, edgeMask);
}
