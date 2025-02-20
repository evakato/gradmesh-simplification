#version 430

in vec3 fragColor;
in vec3 borderColor;

uniform vec3 border_color;

out vec4 out_color;

float circle_df(vec2 coords) {
    return distance(coords, vec2(0.5));
}

void main() {
    float dist = circle_df(gl_PointCoord.xy);
    float alpha = 1.0f - smoothstep(0.42, 0.5, dist);
    vec3 color = mix(fragColor, borderColor, smoothstep(0.30, 0.4, dist));
    out_color = vec4(color, alpha);
   // out_color = vec4(fragColor, alpha);

}