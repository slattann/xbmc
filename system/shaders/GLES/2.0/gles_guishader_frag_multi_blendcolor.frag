#version 100

precision mediump   float;
uniform   sampler2D m_samp0;
uniform   sampler2D m_samp1;
varying   vec4      m_cord0;
varying   vec4      m_cord1;
uniform   lowp vec4 m_unicol;

// SM_MULTI shader
void main ()
{
  gl_FragColor.rgba = m_unicol * texture2D(m_samp0, m_cord0.xy) * texture2D(m_samp1, m_cord1.xy);
}
