#version 450 core

out vec2 vUv;

void main() {
    vec2 p = vec2(float((gl_VertexID << 1) & 2), float(gl_VertexID & 2));
    vec2 ndc = p * 2.0 - 1.0;
    vUv = vec2(ndc.x * 0.5 + 0.5, 0.5 - ndc.y * 0.5);
    gl_Position = vec4(ndc, 0.0, 1.0);
}