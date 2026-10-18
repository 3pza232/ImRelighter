#version 450 core

in vec2 vUv;
out vec4 outColor;

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
uniform float uExposure;
uniform float uMix;
uniform float uComparison;
uniform int uBaseMode;
uniform int uViewMode;
uniform int uSelectedLight;
uniform int uLightCount;
uniform int uShadowsEnabled;

uniform vec3 uLightPos[8];
uniform vec3 uLightColor[8];
uniform float uLightIntensity[8];
uniform float uLightRange[8];
uniform int uLightEnabled[8];

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

vec3 tonemap(vec3 color) {
    // Preserve the normal exposure range and roll off only highlights, so
    // individual channels do not clip at different points and shift hue.
    const vec3 knee = vec3(0.8);
    color = max(color, vec3(0.0));
    vec3 over = max(color - knee, vec3(0.0));
    return min(color, knee) +
           (vec3(1.0) - knee) * (vec3(1.0) - exp(-over / (vec3(1.0) - knee)));
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
    vec4 normalSample = texture(uNormal, vec2(vUv.x, 1.0 - vUv.y));
    vec3 n = normalize(normalSample.rgb * 2.0 - 1.0);
    float edgeMask = clamp(normalSample.a, 0.0, 1.0);

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
    float roughness = smoothstep(0.02, 0.14, microContrast);

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
        float ndl = dot(n, l);
        float diffuse = clamp((ndl + uWrap) / (1.0 + uWrap), 0.0, 1.0);

        float vis = uShadowsEnabled != 0 ? visibility[i] : 1.0;
        vec3 radiance = uLightColor[i] * uLightIntensity[i] * attenuation;
        direct += radiance * diffuse * vis;

        vec3 halfVector = normalize(l + vec3(0.0, 0.0, 1.0));
        float shininess = mix(uShininess, max(8.0, uShininess * 0.5), roughness);
        float specular = uSpecular * mix(1.0, 0.35, roughness);
        float shine = pow(max(dot(n, halfVector), 0.0), max(shininess, 1.0)) *
                      specular * smoothstep(0.0, 0.15, ndl);
        float rim = pow(1.0 - max(n.z, 0.0), 3.0) * max(ndl, 0.0) * uRim;
        // Depth discontinuities are not real surface curvature. Fade the
        // additive highlights there to avoid colored halos on silhouettes.
        shine *= 1.0 - 0.6 * edgeMask;
        rim *= 1.0 - edgeMask;
        highlights += radiance * (shine + rim) * vis;
    }

    vec3 ambientLight = uAmbientColor * uAmbient;
    vec3 illumination = (ambientLight + direct) * occlusion;
    vec3 result = base * illumination + highlights;
    if (uBaseMode == 0) {
        result = (base * illumination + highlights) / (uAmbient + 0.8);
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
        outColor = vec4(clamp(outputColor, 0.0, 1.0), source.a);
    } else {
        float mixed = uViewMode == 2 ? 1.0 : uMix;
        outputColor = mix(base, outputColor, mixed);
        outColor = vec4(clamp(encode(outputColor), 0.0, 1.0), source.a);
    }
}