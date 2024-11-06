#version 400 core

layout(isolines) in;

uniform mat4 projection;
uniform int curve_type; // 0 for Bezier, 1 for Hermite

in vec3[] curve_coords;
in vec3[] curve_colors;

out vec3 vert_color;

vec3 bezier4(vec3 p0, vec3 p1, vec3 p2, vec3 p3, float t) {
    float u = 1.0 - t;
    return (u * u * u) * p0 +
           (3.0 * u * u * t) * p1 +
           (3.0 * u * t * t) * p2 +
           (t * t * t) * p3;
}

vec3 hermite4(vec3 p0, vec3 p1, vec3 p2, vec3 p3, float t) {
    float u = 1.0 - t;
	return (u * u * (3.0 - 2.0 * u)) * p0 +
		   (1.0 * u * u * t) * p1 +
		   (-1.0 * u * t * t) * p2 +
		   (t * t * (3.0 - 2.0 * t)) * p3;
}

void main() {
    float t = gl_TessCoord.x;  // Use x for the parameter t

    vec3 pos;
    vec3 col;
    if (curve_type == 0) {
        pos = bezier4(curve_coords[0], curve_coords[1], curve_coords[2], curve_coords[3], t);
        col = bezier4(curve_colors[0], curve_colors[1], curve_colors[2], curve_colors[3], t);
    } else {
        pos = hermite4(curve_coords[0], curve_coords[1], curve_coords[2], curve_coords[3], t);
        col = hermite4(curve_colors[0], curve_colors[1], curve_colors[2], curve_colors[3], t);
    }

    vert_color = col;
    gl_Position = projection * vec4(pos, 1.0);
}
