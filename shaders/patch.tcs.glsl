#version 430

layout (vertices = 16) out;

in vec3 vertColor[];

out vec3[] patchCoords;
out vec3[] patchColors;

void main() {
    patchColors[gl_InvocationID] = vertColor[gl_InvocationID];
    patchCoords[gl_InvocationID] = gl_in[gl_InvocationID].gl_Position.xyz;

    // Set tessellation levels
    float tessLevel = 16.0; // Set tessellation level as desired
    gl_TessLevelInner[0] = tessLevel; // Inner horizontal
    gl_TessLevelInner[1] = tessLevel; // Inner vertical
    gl_TessLevelOuter[0] = tessLevel; // Top
    gl_TessLevelOuter[1] = tessLevel; // Left
    gl_TessLevelOuter[2] = tessLevel; // Bottom
    gl_TessLevelOuter[3] = tessLevel; // Right
}
