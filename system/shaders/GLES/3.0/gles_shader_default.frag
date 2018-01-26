#version 300 es

precision mediump float;
uniform lowp vec4 m_unicol;
out vec4 fragColor;

// SM_DEFAULT shader
void main ()
{
  fragColor = m_unicol;
}
