/*
 *      Copyright (C) 2010-2013 Team XBMC
 *      http://xbmc.org
 *
 *  This Program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2, or (at your option)
 *  any later version.
 *
 *  This Program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with XBMC; see the file COPYING.  If not, see
 *  <http://www.gnu.org/licenses/>.
 *
 */

#version 100

#extension GL_OES_EGL_image_external : require

precision mediump float;
uniform samplerExternalOES m_samp0;
uniform samplerExternalOES m_samp1;

varying vec4 m_cord0;

uniform float m_brightness;
uniform float m_contrast;

mat4 csc = mat4(1.0,  0.0,    1.402, -0.701,
                1.0, -0.344, -0.714,  0.529,
                1.0,  1.772,  0.0,   -0.886,
                0.0,  0.0,    0.0,    0.0);

void main ()
{
  vec4 yuv;

  yuv = vec4(texture2D(m_samp0, m_cord0.xy).r,
             texture2D(m_samp1, m_cord0.xy).r,
             texture2D(m_samp1, m_cord0.xy).g,
             1.0);

  yuv *= m_contrast;
  yuv += m_brightness;

  gl_FragColor = yuv * csc;
}
