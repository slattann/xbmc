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
#pragma once

#include "BaseRenderBuffer.h"

#define GLX_GLXEXT_PROTOTYPES
#include "system_gl.h"

#include <GL/glx.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>

#include "EGL/egl.h"

#include <stdint.h>
#include <vector>

namespace KODI
{
namespace RETRO
{
  class CRenderBufferHardware : public CBaseRenderBuffer
  {
  public:
    CRenderBufferHardware(AVPixelFormat format, AVPixelFormat targetFormat, unsigned int width, unsigned int height);
    ~CRenderBufferHardware() override;

    // implementation of IRenderBuffer
    bool Allocate(AVPixelFormat format, unsigned int width, unsigned int height, unsigned int size) override;

    // implementation of IRenderBuffer
    bool UploadTexture() override;

    uintptr_t GetCurrentFramebuffer() override { return m_texture.fbo_id; }

    size_t GetFrameSize() const override { return 0; }
    uint8_t *GetMemory() override { return nullptr; }

  protected:
    struct texture
    {
      GLuint tex_id;
      GLuint fbo_id;
      GLuint rbo_id;

      GLuint pitch;
      GLint tex_w, tex_h;
      GLuint clip_w, clip_h;

      GLuint pixfmt;
      GLuint pixtype;
      GLuint bpp;
    };

    texture m_texture;

    // Construction parameters
    const AVPixelFormat m_format;
    const AVPixelFormat m_targetFormat;
    const unsigned int m_width;
    const unsigned int m_height;

    const GLenum m_textureTarget = GL_TEXTURE_2D; //! @todo
    std::vector<uint8_t> m_textureBuffer;

  private:
    bool CreateTexture();
    bool CreateFramebuffer();
    bool CreateDepthbuffer();
    bool CheckFrameBufferStatus();
    bool CreateContext();
    bool CreateShaders();

    GLuint CompileShader(unsigned type, unsigned count, const char **strings);
    void Ortho2d(float m[4][4], float left, float right, float bottom, float top);

    struct shader
    {
      bool compiled = false;
      GLuint vao;
      GLuint vbo;
      GLuint program;

      GLint i_pos;
      GLint i_coord;
      GLint u_tex;
      GLint u_mvp;
    };

    shader m_shader;

    Display *m_Display;
    Window m_Window;
    GLXContext m_glContext;
    GLXWindow m_glWindow;
    Pixmap    m_pixmap;
    GLXPixmap m_glPixmap;
  };

}
}
