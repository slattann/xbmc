#version 100

#extension GL_OES_EGL_image_external : require

precision mediump float;
uniform samplerExternalOES m_samp0;
varying vec4      m_cord0;

uniform float     m_brightness;
uniform float     m_contrast;

void main ()
{
    vec4 color = texture2D(m_samp0, m_cord0.xy).rgba;
    color = color * m_contrast;
    color = color + m_brightness;

    gl_FragColor.rgba = color;
}
