#version 430

in vec3 fragColor;
uniform vec3 lineColor;

out vec4 out_color;

void main() {
	out_color = vec4(lineColor, 1.0f);
	//out_color = vec4(fragColor, 1.0f);
}
