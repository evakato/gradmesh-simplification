#version 430

in vec2 vertUV;
in vec4 vertColor;

out vec4 outColor;

void main() {
    outColor = vec4(vertColor.xyz, 1.0);
}
