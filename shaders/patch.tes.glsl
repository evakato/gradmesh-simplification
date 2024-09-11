#version 430

layout (quads, equal_spacing, ccw) in;

in vec3[] patchCoords;
in vec3[] patchColors;

out vec4 vertColor;
out vec2 vertUV;

vec4 getBlendingFunction(float t) {
    float t2 = t * t;
    float t3 = t2 * t;

    float H0 = (1.0 + 2.0 * t) * (1.0 - t) * (1.0 - t);
    float H1 = t * (1.0 - t) * (1.0 - t);
    float H2 = t2 * (t - 1.0);
    float H3 = t2 * (3.0 - 2.0 * t);

    return vec4(H0, H1, H2, H3);
}

mat4 mapToMat4(vec3[32] vectorData, int attributeIndex) {
	mat4 matrix = mat4(1.0);
	for (int i = 0; i < 4; ++i) 
    {
		for (int j = 0; j < 4; ++j) 
        {
			matrix[i][j] = vectorData[i * 4 + j][attributeIndex];
		}
	}
    return matrix;
}

void main() {
    vec2 uv = gl_TessCoord.xy;

    vec4 uH = getBlendingFunction(uv.x);
    vec4 vH = getBlendingFunction(uv.y);

    mat4 xPosMat = mapToMat4(patchCoords, 0);
    mat4 yPosMat = mapToMat4(patchCoords, 1);
    vec2 position = vec2(dot(vH, xPosMat * uH), dot(vH, yPosMat * uH));

    mat4 rColMat = mapToMat4(patchColors, 0);
    mat4 gColMat = mapToMat4(patchColors, 1);
    mat4 bColMat = mapToMat4(patchColors, 2);
    vec3 color = vec3(dot(vH, rColMat * uH), dot(vH, gColMat * uH), dot(vH, bColMat * uH));

    gl_Position = vec4(position, 0.0, 1.0);
    vertColor = vec4(color, 1.0);
}

