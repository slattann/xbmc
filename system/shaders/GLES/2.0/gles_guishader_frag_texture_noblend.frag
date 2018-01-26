#version 100

precision mediump   float;
uniform   sampler2D m_samp0;
varying   vec4      m_cord0;

// SM_TEXTURE_NOBLEND shader
void main ()
{
  gl_FragColor.rgba = vec4(texture2D(m_samp0, m_cord0.xy).rgba);
}
