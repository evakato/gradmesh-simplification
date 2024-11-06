#version 400

layout (location = 0) in vec2 vertexPos;
layout (location = 1) in vec3 vertexCol;

out vec3 curve_color;

void main()
{
    gl_Position = vec4(vertexPos, 0.0, 1.0);
	curve_color = vertexCol;
}