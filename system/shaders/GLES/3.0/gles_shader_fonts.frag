#version 300 es

precision mediump float;
uniform sampler2D m_samp0;
in vec4 m_cord0;
in lowp vec4 m_colour;
out vec4 fragColor;

// SM_FONTS shader
void main ()
{
  fragColor.r   = m_colour.r;
  fragColor.g   = m_colour.g;
  fragColor.b   = m_colour.b;
  fragColor.a   = m_colour.a * texture(m_samp0, m_cord0.xy).a;
}
