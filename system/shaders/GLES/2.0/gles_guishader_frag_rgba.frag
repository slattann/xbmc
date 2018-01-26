#version 100

precision mediump float;
uniform sampler2D m_samp0;
uniform sampler2D m_samp1;
varying vec4      m_cord0;
varying vec4      m_cord1;
varying lowp vec4 m_colour;
uniform int       m_method;

uniform float     m_brightness;
uniform float     m_contrast;

void main ()
{
  vec4 color = texture2D(m_samp0, m_cord0.xy).rgba;
  color = color * m_contrast;
  color = color + m_brightness;

  gl_FragColor.rgba = color;
}
