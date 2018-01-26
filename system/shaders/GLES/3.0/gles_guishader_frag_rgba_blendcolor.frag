#version 300 es

precision mediump float;
uniform sampler2D m_samp0;
uniform sampler2D m_samp1;
in vec4 m_cord0;
in vec4 m_cord1;
in lowp vec4 m_colour;
uniform int m_method;
out vec4 fragColor;

// SM_TEXTURE
void main ()
{
  fragColor.rgba = vec4(texture(m_samp0, m_cord0.xy).rgba * m_colour);
}
