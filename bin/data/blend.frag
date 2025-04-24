#version 150

uniform sampler2DRect texA;
uniform sampler2DRect texB;
uniform float alpha;

in vec2 texCoordVarying;
out vec4 fragColor;

void main() {
    vec4 colorA = texture(texA, texCoordVarying);
    vec4 colorB = texture(texB, texCoordVarying);
    // fragColor = colorA;
    fragColor = mix(colorA, colorB, alpha);
    // fragColor = vec4(1.0, 0.0, 1.0, 1.0); // hot pink test
    // fragColor = vec4(texCoordVarying.x / 1000, texCoordVarying.y / 1000, 0.5, 1.0);
}