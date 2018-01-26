#version 100

precision highp float;
uniform sampler2D m_samp0;
uniform sampler2D m_samp1;
varying vec4      m_cord0;
varying vec4      m_cord1;
varying lowp vec4 m_colour;
uniform int       m_method;
uniform int       m_field;
uniform float     m_step;

uniform float     m_brightness;
uniform float     m_contrast;

void main ()
{
  vec2 source;
  source = m_cord0.xy;

  float temp1 = mod(source.y, 2.0*m_step);
  float temp2 = source.y - temp1;
  source.y = temp2 + m_step/2.0 - float(m_field)*m_step;

  // Blend missing line
  vec2 below;
  float bstep = step(m_step, temp1);
  below.x = source.x;
  below.y = source.y + (2.0*m_step*bstep);

  vec4 color = mix(texture2D(m_samp0, source), texture2D(m_samp0, below), 0.5);
  color = color * m_contrast;
  color = color + m_brightness;

  gl_FragColor.rgba = color;
}
