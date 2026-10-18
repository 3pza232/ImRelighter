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
    // vUv is top-down in this pass. gl_FragCoord.y is bottom-up, so using it
    // here would sample the height texture upside down and mirror the normals.
    ivec2 size = textureSize(uHeight, 0);
    ivec2 pixel = clamp(ivec2(floor(vUv * vec2(size))),
                        ivec2(0), size - 1);
    float c = heightAt(pixel);
    float l = heightAt(pixel - ivec2(1, 0));
    float r = heightAt(pixel + ivec2(1, 0));
    float b = heightAt(pixel - ivec2(0, 1));
    float u = heightAt(pixel + ivec2(0, 1));

    // Central differences are deliberate. Switching to one-sided differences
    // around noisy model edges flips the gradient between adjacent pixels and
    // shows up as alternating red/green speckles in the normal view.
    float dx = (r - l) * 0.5;
    float dy = (u - b) * 0.5;

    float jump = max(max(abs(r - c), abs(l - c)),
                     max(abs(u - c), abs(b - c)));
    float edgeMask = smoothstep(0.08, 0.22,
                                jump / max(uAmplitude, 1e-3));

    float scale = max(uPixelScale, 1e-6);
    vec2 slope = vec2(-dx / scale, dy / scale) * uImageSize.y * 0.5;
    slope *= uNormalStrength;

    // Add high-frequency albedo detail so a low-resolution depth map does not
    // shade flat surfaces like smooth cardboard. A difference of two gradient
    // scales cancels broad gradients such as baked shadows while keeping fine
    // cloth, hair, and skin texture.
    vec2 fineStep = uTexel * 1.5;
    vec2 wideStep = uTexel * 3.0;
    float fineL = colorLuma(vUv - vec2(fineStep.x, 0.0));
    float fineR = colorLuma(vUv + vec2(fineStep.x, 0.0));
    float fineB = colorLuma(vUv - vec2(0.0, fineStep.y));
    float fineU = colorLuma(vUv + vec2(0.0, fineStep.y));
    float wideL = colorLuma(vUv - vec2(wideStep.x, 0.0));
    float wideR = colorLuma(vUv + vec2(wideStep.x, 0.0));
    float wideB = colorLuma(vUv - vec2(0.0, wideStep.y));
    float wideU = colorLuma(vUv + vec2(0.0, wideStep.y));
    vec2 detail = vec2(-(fineR - fineL), fineU - fineB) -
                  0.5 * vec2(-(wideR - wideL), wideU - wideB);
    // Cap per-axis albedo slopes so hard graphic edges (ties, glasses, text)
    // do not emboss as raised ridges while fine texture still survives.
    detail = clamp(detail, vec2(-0.08), vec2(0.08));
    detail *= 1.0 - edgeMask;
    // Dark hair and fabric carry weaker luminance contrast; boost their
    // micro-normals so they do not collapse into a smooth leather sheen.
    float darkBoost =
        mix(2.0, 1.0, smoothstep(0.03, 0.35, colorLuma(vUv)));
    slope += detail * uDetailStrength * 12.0 * darkBoost;

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
