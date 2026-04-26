#version 100

precision highp float;

uniform sampler2D u_tex;
uniform vec4 u_color;

varying vec2 r_texcoord;

void main() {
    gl_FragColor = texture2D(u_tex, r_texcoord) * (u_color / 255.0);
}
