#version 300 es

precision mediump float;
uniform sampler2D m_samp0;
uniform lowp vec4 m_unicol;
in vec4 m_cord0;
out vec4 fragColor;

void main ()
{
  vec4 rgb;

  rgb = texture(m_samp0, m_cord0.xy).rgba * m_unicol;

#if defined(KODI_LIMITED_RANGE)
  rgb.rgb *= (235.0 - 16.0) / 255.0;
  rgb.rgb += 16.0 / 255.0;
#endif

  fragColor = rgb;
}
