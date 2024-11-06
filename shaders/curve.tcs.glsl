#version 430

layout (vertices = 4) out;

in vec3[] curve_color;

out vec3[] curve_coords;
out vec3[] curve_colors;

void main() {
	if (gl_InvocationID == 0) {
		gl_TessLevelOuter[0] = 1.0;
		gl_TessLevelOuter[1] = 64.0;
	}
	vec3 coords = gl_in[gl_InvocationID].gl_Position.xyz;
	curve_coords[gl_InvocationID] = coords;
	curve_colors[gl_InvocationID] = curve_color[gl_InvocationID];
}
