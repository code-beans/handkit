#version 450

layout(location = 0) out vec2 uv;

void main() {
    // Three vertices cover the viewport without a separate vertex buffer.
    vec2 position = vec2((gl_VertexIndex << 1) & 2, gl_VertexIndex & 2);
    uv = position;
    gl_Position = vec4(position.x * 2 - 1, 1 - position.y * 2, 0, 1);
}
