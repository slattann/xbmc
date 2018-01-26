#version 300 es

precision mediump float;
uniform sampler2D m_samp0;
uniform lowp vec4 m_unicol;
in vec4 m_cord0;
out vec4 fragColor;

// SM_TEXTURE shader
void main ()
{
  fragColor.rgba = vec4(texture(m_samp0, m_cord0.xy).rgba * m_unicol);
}
