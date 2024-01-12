#version 460 core
out vec4 color;

uniform vec3 iResolution;
uniform float iTime;

void mainImage(out vec4 fragColor, in vec2 fragCoord);

void main() {
	mainImage(color, gl_FragCoord.xy);
} 

