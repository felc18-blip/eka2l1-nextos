#version 100

// NextOS GLES 1.00 port: GLSL ES 1.00 has no integer / bit ops and no
// 'flat' qualifier, so the original line-stipple computation is gone.
// Pen now draws as a solid line — u_pattern / u_viewport uniforms are
// kept around in the C++ code but no longer consulted here.

precision highp float;

uniform vec4 u_color;

void main() {
    gl_FragColor = u_color / 255.0;
}
