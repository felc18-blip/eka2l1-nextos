#version 100

// NextOS GLES 1.00 port: dropped 'flat' qualifier (not supported in ES 1.00)
// and the line-stipple machinery — pen now draws as a solid line, no
// pattern. The fragment shader matches.

precision highp float;

attribute vec2 in_position;

uniform mat4 u_proj;
uniform mat4 u_model;
uniform float u_pointSize;

void main() {
    gl_Position = u_proj * u_model * vec4(in_position, 0.0, 1.0);
    gl_Position.y = -gl_Position.y;
    gl_PointSize = u_pointSize;
}
