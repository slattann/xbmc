#version 300 es

#extension GL_OES_EGL_image_external_essl3 : require

precision mediump float;
uniform samplerExternalOES m_samp0;
in vec4 m_cord0;
out vec4 fragColor;

uniform float m_brightness;
uniform float m_contrast;

void main ()
{
    vec4 color = texture(m_samp0, m_cord0.xy).rgba;
    color = color * m_contrast;
    color = color + m_brightness;

    fragColor.rgba = color;
}
