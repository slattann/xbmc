/*
 *      Copyright (C) 2017 Team Kodi
 *      http://kodi.tv
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
 *  along with this Program; see the file COPYING.  If not, see
 *  <http://www.gnu.org/licenses/>.
 *
 */

#include "RenderBufferHardware.h"

#include "windowing/X11/WinSystemX11GLContext.h"
#include "ServiceBroker.h"
#include "utils/log.h"

using namespace KODI;
using namespace RETRO;

CRenderBufferHardware::CRenderBufferHardware(AVPixelFormat format, AVPixelFormat targetFormat, unsigned int width, unsigned int height) :
  m_format(format),
  m_targetFormat(targetFormat),
  m_width(width),
  m_height(height)
{
}

CRenderBufferHardware::~CRenderBufferHardware()
{
  if (glIsTexture(m_texture.tex_id))
    glDeleteTextures(1, &m_texture.tex_id);

  m_texture.tex_id = 0;
}

bool CRenderBufferHardware::Allocate(AVPixelFormat format, unsigned int width, unsigned int height, unsigned int size)
{
  if (!CreateContext())
    return false;

  if (!CreateShaders())
    return false;

  if (!CreateTexture())
    return false;

  if (!CreateFramebuffer())
    return false;

  if (!CreateDepthbuffer())
    return false;

  return CheckFrameBufferStatus();
}

bool CRenderBufferHardware::UploadTexture()
{
  glBindFramebuffer(GL_FRAMEBUFFER, 0);

  glUseProgram(m_shader.program);

  float bottom = 1;
  float right  = 1;

  float vertex_data[] =
  {
    // pos, coord
    -1.0f, -1.0f, 0.0f,  bottom, // left-bottom
    -1.0f,  1.0f, 0.0f,  0.0f,   // left-top
     1.0f, -1.0f, right,  bottom,// right-bottom
     1.0f,  1.0f, right,  0.0f,  // right-top
  };

  GLubyte idx[4] = {0, 1, 3, 2};  //determines order of the vertices
  GLuint vertexVBO;
  GLuint indexVBO;

  glGenBuffers(1, &vertexVBO);
  glBindBuffer(GL_ARRAY_BUFFER, vertexVBO);
  glBufferData(GL_ARRAY_BUFFER, sizeof(vertex_data), vertex_data, GL_STREAM_DRAW);

  glVertexAttribPointer(m_shader.i_pos, 2, GL_FLOAT, GL_FALSE, sizeof(float)*4, 0);
  glVertexAttribPointer(m_shader.i_coord, 2, GL_FLOAT, GL_FALSE, sizeof(float)*4, (void*)(2 * sizeof(float)));

  glEnableVertexAttribArray(m_shader.i_pos);
  glEnableVertexAttribArray(m_shader.i_coord);

  glGenBuffers(1, &indexVBO);
  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, indexVBO);
  glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(GLubyte)*4, idx, GL_STATIC_DRAW);

  glDrawElements(GL_TRIANGLE_STRIP, 4, GL_UNSIGNED_BYTE, 0);

  glDisableVertexAttribArray(m_shader.i_pos);
  glDisableVertexAttribArray(m_shader.i_coord);

  glBindBuffer(GL_ARRAY_BUFFER, 0);
  glDeleteBuffers(1, &vertexVBO);
  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
  glDeleteBuffers(1, &indexVBO);

  glUseProgram(0);

  return true;
}

bool CRenderBufferHardware::CreateTexture()
{
  glBindTexture(GL_TEXTURE_2D, 0);
  glGenTextures(1, &m_texture.tex_id);

  glBindTexture(GL_TEXTURE_2D, m_texture.tex_id);
  glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
  glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D, GL_GENERATE_MIPMAP, GL_TRUE); // automatic mipmap
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, m_width, m_height, 0, GL_RGB, GL_UNSIGNED_BYTE, 0);

  return true;
}

bool CRenderBufferHardware::CreateFramebuffer()
{
  glGenFramebuffers(1, &m_texture.fbo_id);
  glBindFramebuffer(GL_FRAMEBUFFER, m_texture.fbo_id);

  // attach the texture to FBO color attachment point
  glFramebufferTexture2D(GL_FRAMEBUFFER,            // 1. fbo target: GL_FRAMEBUFFER
                         GL_COLOR_ATTACHMENT0,      // 2. attachment point
                         GL_TEXTURE_2D,             // 3. tex target: GL_TEXTURE_2D
                         m_texture.tex_id,          // 4. tex ID
                         0);                        // 5. mipmap level: 0(base){

  return CheckFrameBufferStatus();
}

bool CRenderBufferHardware::CreateDepthbuffer()
{
  glGenRenderbuffers(1, &m_texture.rbo_id);
  glBindRenderbuffer(GL_RENDERBUFFER, m_texture.rbo_id);
  glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT16, m_width, m_height);
  glBindRenderbuffer(GL_RENDERBUFFER, 0);
  glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, m_texture.rbo_id);

  return true;
}

bool CRenderBufferHardware::CheckFrameBufferStatus()
{
  GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
  if(status != GL_FRAMEBUFFER_COMPLETE)
    return false;

  return true;
}

bool CRenderBufferHardware::CreateContext()
{
  GLXContext glContext;

  CWinSystemX11GLContext &winSystem = dynamic_cast<CWinSystemX11GLContext&>(CServiceBroker::GetWinSystem());

  m_Display = winSystem.GetDisplay();
  glContext = static_cast<GLXContext>(winSystem.GetGlxContext());
  m_Window = winSystem.GetWindow();

  // Get our window attribs.
  XWindowAttributes wndattribs;
  XGetWindowAttributes(m_Display, m_Window, &wndattribs);

  // Get visual Info
  XVisualInfo visInfo;
  visInfo.visualid = wndattribs.visual->visualid;
  int nvisuals = 0;
  XVisualInfo* visuals = XGetVisualInfo(m_Display, VisualIDMask, &visInfo, &nvisuals);
  if (nvisuals != 1)
  {
    CLog::Log(LOGERROR, "RetroPlayerGL::CreateGlxContext - could not find visual");
    return false;
  }
  visInfo = visuals[0];
  XFree(visuals);

  m_pixmap = XCreatePixmap(m_Display,
                           m_Window,
                           192,
                           108,
                           visInfo.depth);
  if (!m_pixmap)
  {
    CLog::Log(LOGERROR, "RetroPlayerGL::CreateGlxContext - Unable to create XPixmap");
    return false;
  }

  // create gl pixmap
  m_glPixmap = glXCreateGLXPixmap(m_Display, &visInfo, m_pixmap);

  if (!m_glPixmap)
  {
    CLog::Log(LOGINFO, "RetroPlayerGL::CreateGlxContext - Could not create glPixmap");
    return false;
  }

  m_glContext = glXCreateContext(m_Display, &visInfo, glContext, True);

  if (!glXMakeCurrent(m_Display, m_glPixmap, m_glContext))
  {
    CLog::Log(LOGINFO, "RetroPlayerGL::CreateGlxContext - Could not make Pixmap current");
    return false;
  }

  CLog::Log(LOGNOTICE, "RetroPlayerGL::CreateGlxContext - created context");
  return true;
}

static const char *g_vshader_src =
  "#version 150\n"
  "in vec2 i_pos;\n"
  "in vec2 i_coord;\n"
  "out vec2 o_coord;\n"
  "uniform mat4 u_mvp;\n"
  "void main() {\n"
      "o_coord = i_coord;\n"
      "gl_Position = vec4(i_pos, 0.0, 1.0) * u_mvp;\n"
  "}";

static const char *g_fshader_src =
  "#version 150\n"
  "in vec2 o_coord;\n"
  "uniform sampler2D u_tex;\n"
  "void main() {\n"
      "gl_FragColor = texture2D(u_tex, o_coord);\n"
  "}";

bool CRenderBufferHardware::CreateShaders()
{
  GLuint vshader = CompileShader(GL_VERTEX_SHADER, 1, &g_vshader_src);
  GLuint fshader = CompileShader(GL_FRAGMENT_SHADER, 1, &g_fshader_src);
  GLuint program = glCreateProgram();

  glAttachShader(program, vshader);
  glAttachShader(program, fshader);
  glLinkProgram(program);

  glDeleteShader(vshader);
  glDeleteShader(fshader);

  glValidateProgram(program);

  GLint status;
  glGetProgramiv(program, GL_LINK_STATUS, &status);

  if(status == GL_FALSE)
  {
    char buffer[4096];
    glGetProgramInfoLog(program, sizeof(buffer), NULL, buffer);
    CLog::Log(LOGERROR, "Failed to link shader program: %s", buffer);
    return false;
  }

  m_shader.program = program;
  m_shader.i_pos   = glGetAttribLocation(program,  "i_pos");
  m_shader.i_coord = glGetAttribLocation(program,  "i_coord");
  m_shader.u_tex   = glGetUniformLocation(program, "u_tex");
  m_shader.u_mvp   = glGetUniformLocation(program, "u_mvp");

  glGenVertexArrays(1, &m_shader.vao);
  glGenBuffers(1, &m_shader.vbo);

  glUseProgram(m_shader.program);

  glUniform1i(m_shader.u_tex, 0);

  float m[4][4];
  //if (g_video.hw.bottom_left_origin)
      Ortho2d(m, -1, 1, 1, -1);
  //else
  //    ortho2d(m, -1, 1, -1, 1);

  glUniformMatrix4fv(m_shader.u_mvp, 1, GL_FALSE, (float*)m);

  glUseProgram(0);

  m_shader.compiled = true;

  return true;
}


GLuint CRenderBufferHardware::CompileShader(unsigned type, unsigned count, const char **strings)
{
  GLuint shader = glCreateShader(type);
  glShaderSource(shader, count, strings, NULL);
  glCompileShader(shader);

  GLint status;
  glGetShaderiv(shader, GL_COMPILE_STATUS, &status);

  if (status == GL_FALSE)
  {
    char buffer[4096];
    glGetShaderInfoLog(shader, sizeof(buffer), NULL, buffer);
    CLog::Log(LOGERROR, "Failed to compile %s shader: %s", type == GL_VERTEX_SHADER ? "vertex" : "fragment", buffer);
  }

  return shader;
}

void CRenderBufferHardware::Ortho2d(float m[4][4], float left, float right, float bottom, float top)
{
  m[0][0] = 1; m[0][1] = 0; m[0][2] = 0; m[0][3] = 0;
  m[1][0] = 0; m[1][1] = 1; m[1][2] = 0; m[1][3] = 0;
  m[2][0] = 0; m[2][1] = 0; m[2][2] = 1; m[2][3] = 0;
  m[3][0] = 0; m[3][1] = 0; m[3][2] = 0; m[3][3] = 1;

  m[0][0] = 2.0f / (right - left);
  m[1][1] = 2.0f / (top - bottom);
  m[2][2] = -1.0f;
  m[3][0] = -(right + left) / (right - left);
  m[3][1] = -(top + bottom) / (top - bottom);
}