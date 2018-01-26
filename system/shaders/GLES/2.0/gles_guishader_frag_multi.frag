#version 100

precision mediump   float;
uniform   sampler2D m_samp0;
uniform   sampler2D m_samp1;
varying   vec4      m_cord0;
varying   vec4      m_cord1;

// SM_MULTI shader
void main ()
{
  gl_FragColor.rgba = (texture2D(m_samp0, m_cord0.xy) * texture2D(m_samp1, m_cord1.xy)).rgba;
}
