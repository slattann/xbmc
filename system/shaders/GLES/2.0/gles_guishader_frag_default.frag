#version 100

precision mediump float;
uniform lowp vec4 m_unicol;

// SM_DEFAULT shader
void main ()
{
  gl_FragColor = m_unicol;
}
