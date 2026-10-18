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
uniform float uDetailStrength;
uniform vec3 uFlips;

float heightAt(ivec2 pixel) {
    ivec2 size = textureSize(uHeight, 0);
    return texelFetch(uHeight, clamp(pixel, ivec2(0), size - 1), 0).r *
           uAmplitude;
}

float colorLuma(vec2 uv) {
    // The height target is already top-down, matching the color texture. Do not
    // flip V here or detail gradients come from a vertically mirrored image.
    vec3 color = texture(uColor, uv).rgb;
    return dot(color, vec3(0.2126, 0.7152, 0.0722));
}

void main() {
    // Sample the height target directly so preview and export use the same
    // footprint in output pixels. uPixelScale converts the preview gradient
    // back to source-image pixels for a resolution-independent slope.
    vec2 texel = uTexel;
    ivec2 pixel = ivec2(gl_FragCoord.xy);
    float c = heightAt(pixel);
    float l = heightAt(pixel - ivec2(1, 0));
    float r = heightAt(pixel + ivec2(1, 0));
    float b = heightAt(pixel - ivec2(0, 1));
    float u = heightAt(pixel + ivec2(0, 1));

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

    // Add high-frequency albedo detail so a low-resolution depth map does not
    // shade flat surfaces like smooth cardboard.
    vec2 detailTexel = uTexel * 1.5;
    vec2 detail = vec2(
        -(colorLuma(vUv + vec2(detailTexel.x, 0.0)) -
          colorLuma(vUv - vec2(detailTexel.x, 0.0))),
        colorLuma(vUv + vec2(0.0, detailTexel.y)) -
            colorLuma(vUv - vec2(0.0, detailTexel.y)));
    // Cap per-axis albedo slopes so hard graphic edges (ties, glasses, text)
    // do not emboss as raised ridges while fine texture still survives.
    detail = clamp(detail, vec2(-0.08), vec2(0.08));
    slope += detail * uDetailStrength * 8.0;

    // Depth discontinuities are handled by the lighting pass through
    // outNormal.a; keep their actual slope here so the normal view does not
    // grow a neutral band around silhouettes.

    // Relative depth has no reliable absolute scale. Soft-saturating the slope
    // keeps the upsampled depth ramp from turning into a near-horizontal normal
    // without flattening whole regions into a constant plane.
    float slopeLength = length(slope);
    if (slopeLength > 1e-6) {
        float softLength = uMaxTilt * tanh(slopeLength / uMaxTilt);
        slope *= softLength / slopeLength;
    }

    vec3 n = normalize(vec3(slope, 1.0));
    n *= uFlips;
    n = normalize(n);

    outNormal = vec4(n * 0.5 + 0.5, edgeMask);
}
