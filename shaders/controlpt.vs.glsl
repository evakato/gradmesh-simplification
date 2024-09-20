#version 400

layout (location = 0) in vec2 vertexPos;
layout (location = 1) in vec3 vertexCol;

uniform mat4 projection;
uniform int selectedIndex;

out vec3 fragColor;
out vec3 borderColor;

void main()
{
    gl_Position = projection * vec4(vertexPos, 0.0, 1.0);
    gl_PointSize = 15.0;

    //vec3 color = vec3(1.0, 1.0, 1.0);
    fragColor = vertexCol;

    borderColor = vec3(0.0, 0.0, 0.0);
    if (gl_VertexID == selectedIndex) {
        //borderColor = vec3(0.0, 0.4, 1.0);
    };

    /*
    if (gl_VertexID % 4 == 1 || gl_VertexID % 4 == 2) {
        fragColor = vec3(0.5, 0.5, 1.0);
    } else {
        fragColor = vec3(1.0, 1.0, 1.0);
    }
    */
}