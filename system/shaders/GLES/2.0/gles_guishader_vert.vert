#version 100

attribute vec4 m_attrpos;
attribute vec4 m_attrcol;
attribute vec4 m_attrcord0;
attribute vec4 m_attrcord1;
varying vec4   m_cord0;
varying vec4   m_cord1;
varying lowp vec4 m_colour;
uniform mat4   m_proj;
uniform mat4   m_model;
uniform mat4   m_coord0Matrix;

void main ()
{
  mat4 mvp    = m_proj * m_model;
  gl_Position = mvp * m_attrpos;
  m_colour    = m_attrcol;
  m_cord0     = m_coord0Matrix * m_attrcord0;
  m_cord1     = m_attrcord1;
}
