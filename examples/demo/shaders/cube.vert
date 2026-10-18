#version 450

layout(set = 1, binding = 0) uniform View {
    // Pitch, yaw, aspect ratio, camera distance.
    vec4 parameters;
}
view;

layout(location = 0) out vec3 color;

const vec3 vertices[36] = vec3[](
    // Front face.
    vec3(-1, -1, 1), vec3(1, -1, 1), vec3(1, 1, 1), vec3(-1, -1, 1), vec3(1, 1, 1), vec3(-1, 1, 1),
    // Back face.
    vec3(1, -1, -1), vec3(-1, -1, -1), vec3(-1, 1, -1), vec3(1, -1, -1), vec3(-1, 1, -1), vec3(1, 1, -1),
    // Left face.
    vec3(-1, -1, -1), vec3(-1, -1, 1), vec3(-1, 1, 1), vec3(-1, -1, -1), vec3(-1, 1, 1), vec3(-1, 1, -1),
    // Right face.
    vec3(1, -1, 1), vec3(1, -1, -1), vec3(1, 1, -1), vec3(1, -1, 1), vec3(1, 1, -1), vec3(1, 1, 1),
    // Top face.
    vec3(-1, 1, 1), vec3(1, 1, 1), vec3(1, 1, -1), vec3(-1, 1, 1), vec3(1, 1, -1), vec3(-1, 1, -1),
    // Bottom face.
    vec3(-1, -1, -1), vec3(1, -1, -1), vec3(1, -1, 1), vec3(-1, -1, -1), vec3(1, -1, 1), vec3(-1, -1, 1));

const vec3 palette[6] = vec3[](vec3(0.36, 0.91, 0.77), vec3(0.37, 0.58, 0.94), vec3(0.95, 0.54, 0.30), vec3(0.75, 0.43, 0.95), vec3(0.98, 0.80, 0.38), vec3(0.27, 0.65, 0.72));

void main() {
    vec3 position = vertices[gl_VertexIndex] * 0.85;
    float cos_pitch = cos(view.parameters.x);
    float sin_pitch = sin(view.parameters.x);
    float cos_yaw = cos(view.parameters.y);
    float sin_yaw = sin(view.parameters.y);

    position = vec3(cos_yaw * position.x + sin_yaw * position.z, position.y, -sin_yaw * position.x + cos_yaw * position.z);
    position = vec3(position.x, cos_pitch * position.y - sin_pitch * position.z, sin_pitch * position.y + cos_pitch * position.z);
    position.z += view.parameters.w;

    const float near_plane = 0.1;
    const float far_plane = 100.0;
    const float focal_length = 1.7320508;
    float aspect_ratio = view.parameters.z;

    // Perspective projection with Vulkan's 0..1 depth range.
    gl_Position = vec4(position.x * focal_length / aspect_ratio, position.y * focal_length, far_plane / (far_plane - near_plane) * position.z - far_plane * near_plane / (far_plane - near_plane), position.z);
    color = palette[gl_VertexIndex / 6];
}
