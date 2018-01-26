#version 300 es

precision mediump float;
uniform sampler2D m_samp0;
uniform sampler2D m_samp1;
in vec4 m_cord0;
in vec4 m_cord1;
in lowp vec4 m_colour;
uniform int m_method;

uniform float m_brightness;
uniform float m_contrast;
out vec4 fragColor;

void main ()
{
  vec4 color = texture(m_samp0, m_cord0.xy).rgba;
  color = color * m_contrast;
  color = color + m_brightness;

  fragColor.rgba = color;
}