#version 430

in vec3 vert_color;
out vec4 out_color;

void main() {
    //out_color = vec4(vert_color, 1.0);
    out_color = vec4(0.0, 0.0, 0.0, 1.0);
}
