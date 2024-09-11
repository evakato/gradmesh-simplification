#version 430

layout (location = 0) in vec2 vertPos;
layout (location = 1) in vec3 vertColorIn;

out vec3 vertColor;

void main() {
    gl_Position = vec4(vertPos, 0.0, 1.0);
    vertColor = vertColorIn;
}
