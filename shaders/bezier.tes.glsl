#version 400 core

layout(isolines) in;

uniform mat4 projection;

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
	return p0 * (1.0 + 2.0 * t) * pow(1.0 - t, 2.0)
			 + p1 * t * pow(1.0 - t, 2.0)
			 + p2 * pow(t, 2.0) * (t - 1.0)
			 + p3 * pow(t, 2.0) * (3.0 - 2.0 * t);
}

void main() {
    float t = gl_TessCoord.x;  // Use x for the parameter t

/*
    vec3 pos = bezier4(gl_in[0].gl_Position.xyz, 
                       gl_in[1].gl_Position.xyz, 
                       gl_in[2].gl_Position.xyz, 
                       gl_in[3].gl_Position.xyz, t);
                       */
    vec3 pos = bezier4(curve_coords[0], curve_coords[1], curve_coords[2], curve_coords[3], t);
    vec3 col = bezier4(curve_colors[0], curve_colors[1], curve_colors[2], curve_colors[3], t);

    gl_Position = projection * vec4(pos, 1.0);
    vert_color = col;
}
