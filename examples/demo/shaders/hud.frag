#version 450

layout(set = 2, binding = 0) uniform sampler2D hud_texture;
layout(location = 0) in vec2 uv;
layout(location = 0) out vec4 output_color;

void main() {
    output_color = texture(hud_texture, uv);
}
