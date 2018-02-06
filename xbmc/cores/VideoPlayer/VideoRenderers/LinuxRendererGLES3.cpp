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

#include "system_gl.h"

#include "LinuxRendererGLES3.h"

#include "ServiceBroker.h"
#include "RenderFactory.h"
#include "rendering/gles/RenderSystemGLES.h"
#include "utils/GLUtils.h"
#include "utils/log.h"

using namespace Shaders;

CBaseRenderer* CLinuxRendererGLES3::Create(CVideoBuffer *buffer)
{
  CRenderSystemGLES *renderSystem = dynamic_cast<CRenderSystemGLES*>(&CServiceBroker::GetRenderSystem());
  unsigned int major, minor;
  renderSystem->GetRenderVersion(major, minor);
  if (major >= 3)
    return new CLinuxRendererGLES3();

  return nullptr;
}

bool CLinuxRendererGLES3::Register()
{
  VIDEOPLAYER::CRendererFactory::RegisterRenderer("default", CLinuxRendererGLES3::Create);
  return true;
}

//********************************************************************************************************
// YV12 Texture creation, deletion, copying + clearing
//********************************************************************************************************
bool CLinuxRendererGLES3::UploadYV12Texture(int source)
{
  YUVBUFFER& buf = m_buffers[source];
  YuvImage* im = &buf.image;

  VerifyGLState();

  glPixelStorei(GL_UNPACK_ALIGNMENT,1);

  //Load Y plane
  LoadPlane(buf.fields[FIELD_FULL][0], GL_RED,
            im->width, im->height,
            im->stride[0], im->bpp, im->plane[0]);

  //load U plane
  LoadPlane(buf.fields[FIELD_FULL][1], GL_RED,
            im->width >> im->cshift_x, im->height >> im->cshift_y,
            im->stride[1], im->bpp, im->plane[1]);

  //load V plane
  LoadPlane(buf.fields[FIELD_FULL][2], GL_RED,
            im->width >> im->cshift_x, im->height >> im->cshift_y,
            im->stride[2], im->bpp, im->plane[2]);

  VerifyGLState();

  CalculateTextureSourceRects(source, 3);

  return true;
}

bool CLinuxRendererGLES3::CreateYV12Texture(int index)
{
  /* since we also want the field textures, pitch must be texture aligned */
  unsigned p;
  YuvImage &im = m_buffers[index].image;

  DeleteYV12Texture(index);

  im.height = m_sourceHeight;
  im.width  = m_sourceWidth;
  im.cshift_x = 1;
  im.cshift_y = 1;

  if(m_format == AV_PIX_FMT_YUV420P16 ||
     m_format == AV_PIX_FMT_YUV420P10)
    im.bpp = 2;
  else
    im.bpp = 1;

  im.stride[0] = im.bpp *   im.width;
  im.stride[1] = im.bpp * ( im.width >> im.cshift_x );
  im.stride[2] = im.bpp * ( im.width >> im.cshift_x );

  im.planesize[0] = im.stride[0] *   im.height;
  im.planesize[1] = im.stride[1] * ( im.height >> im.cshift_y );
  im.planesize[2] = im.stride[2] * ( im.height >> im.cshift_y );

  for (int i = 0; i < 3; i++)
    im.plane[i] = new uint8_t[im.planesize[i]];

  for(int f = 0;f<MAX_FIELDS;f++)
  {
    for(p = 0;p<YuvImage::MAX_PLANES;p++)
    {
      if (!glIsTexture(m_buffers[index].fields[f][p].id))
      {
        glGenTextures(1, &m_buffers[index].fields[f][p].id);
        VerifyGLState();
      }
    }
  }

  // YUV
  for (int f = FIELD_FULL; f<=FIELD_BOT ; f++)
  {
    int fieldshift = (f==FIELD_FULL) ? 0 : 1;
    YUVPLANE (&planes)[YuvImage::MAX_PLANES] = m_buffers[index].fields[f];

    planes[0].texwidth  = im.width;
    planes[0].texheight = im.height >> fieldshift;

    planes[1].texwidth  = planes[0].texwidth  >> im.cshift_x;
    planes[1].texheight = planes[0].texheight >> im.cshift_y;
    planes[2].texwidth  = planes[0].texwidth  >> im.cshift_x;
    planes[2].texheight = planes[0].texheight >> im.cshift_y;

    for (int p = 0; p < 3; p++)
    {
      planes[p].pixpertex_x = 1;
      planes[p].pixpertex_y = 1;
    }

    for(int p = 0; p < 3; p++)
    {
      YUVPLANE &plane = planes[p];
      if (plane.texwidth * plane.texheight == 0)
        continue;

      glBindTexture(m_textureTarget, plane.id);

      GLenum format;
      GLint internalformat;
      if (p == 2) //V plane needs an alpha texture
        format = GL_RED;
      else
        format = GL_RED;
      internalformat = GetInternalFormat(format, im.bpp);

      if(m_renderMethod & RENDER_POT)
        CLog::Log(LOGDEBUG, "GL: Creating YUV POT texture of size %d x %d",  plane.texwidth, plane.texheight);
      else
        CLog::Log(LOGDEBUG,  "GL: Creating YUV NPOT texture of size %d x %d", plane.texwidth, plane.texheight);

      glTexImage2D(m_textureTarget, 0, internalformat, plane.texwidth, plane.texheight, 0, format, GL_UNSIGNED_BYTE, NULL);

      glTexParameteri(m_textureTarget, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
      glTexParameteri(m_textureTarget, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
      glTexParameteri(m_textureTarget, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
      glTexParameteri(m_textureTarget, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
      VerifyGLState();
    }
  }
  return true;
}

//********************************************************************************************************
// NV12 Texture loading, creation and deletion
//********************************************************************************************************
bool CLinuxRendererGLES3::UploadNV12Texture(int source)
{
  YUVBUFFER& buf = m_buffers[source];
  YuvImage* im = &buf.image;

  bool deinterlacing;
  if (m_currentField == FIELD_FULL)
    deinterlacing = false;
  else
    deinterlacing = true;

  VerifyGLState();

  glPixelStorei(GL_UNPACK_ALIGNMENT, im->bpp);

  if (deinterlacing)
  {
    // Load Odd Y field
    LoadPlane(buf.fields[FIELD_TOP][0] , GL_RED,
              im->width, im->height >> 1,
              im->stride[0]*2, im->bpp, im->plane[0]);

    // Load Even Y field
    LoadPlane(buf.fields[FIELD_BOT][0], GL_RED,
              im->width, im->height >> 1,
              im->stride[0]*2, im->bpp, im->plane[0] + im->stride[0]) ;

    // Load Odd UV Fields
    LoadPlane(buf.fields[FIELD_TOP][1], GL_RG,
              im->width >> im->cshift_x, im->height >> (im->cshift_y + 1),
              im->stride[1]*2, im->bpp, im->plane[1]);

    // Load Even UV Fields
    LoadPlane(buf.fields[FIELD_BOT][1], GL_RG,
              im->width >> im->cshift_x, im->height >> (im->cshift_y + 1),
              im->stride[1]*2, im->bpp, im->plane[1] + im->stride[1]);

  }
  else
  {
    // Load Y plane
    LoadPlane(buf. fields[FIELD_FULL][0], GL_RED,
              im->width, im->height,
              im->stride[0], im->bpp, im->plane[0]);

    // Load UV plane
    LoadPlane(buf.fields[FIELD_FULL][1], GL_RG,
              im->width >> im->cshift_x, im->height >> im->cshift_y,
              im->stride[1], im->bpp, im->plane[1]);
  }

  VerifyGLState();

  CalculateTextureSourceRects(source, 3);

  return true;
}

bool CLinuxRendererGLES3::CreateNV12Texture(int index)
{
  // since we also want the field textures, pitch must be texture aligned
  YUVBUFFER& buf = m_buffers[index];
  YuvImage &im = buf.image;

  // Delete any old texture
  DeleteNV12Texture(index);

  im.height = m_sourceHeight;
  im.width  = m_sourceWidth;
  im.cshift_x = 1;
  im.cshift_y = 1;
  im.bpp = 1;

  im.stride[0] = im.width;
  im.stride[1] = im.width;
  im.stride[2] = 0;

  im.plane[0] = NULL;
  im.plane[1] = NULL;
  im.plane[2] = NULL;

  // Y plane
  im.planesize[0] = im.stride[0] * im.height;
  // packed UV plane
  im.planesize[1] = im.stride[1] * im.height / 2;
  // third plane is not used
  im.planesize[2] = 0;

  for (int i = 0; i < 2; i++)
    im.plane[i] = new uint8_t[im.planesize[i]];

  for(int f = 0;f<MAX_FIELDS;f++)
  {
    for(int p = 0;p<2;p++)
    {
      if (!glIsTexture(buf.fields[f][p].id))
      {
        glGenTextures(1, &buf.fields[f][p].id);
        VerifyGLState();
      }
    }
    buf.fields[f][2].id = buf.fields[f][1].id;
  }

  // YUV
  for (int f = FIELD_FULL; f<=FIELD_BOT ; f++)
  {
    int fieldshift = (f==FIELD_FULL) ? 0 : 1;
    YUVPLANE (&planes)[YuvImage::MAX_PLANES] = buf.fields[f];

    planes[0].texwidth  = im.width;
    planes[0].texheight = im.height >> fieldshift;

    planes[1].texwidth  = planes[0].texwidth  >> im.cshift_x;
    planes[1].texheight = planes[0].texheight >> im.cshift_y;
    planes[2].texwidth  = planes[1].texwidth;
    planes[2].texheight = planes[1].texheight;

    for (int p = 0; p < 3; p++)
    {
      planes[p].pixpertex_x = 1;
      planes[p].pixpertex_y = 1;
    }

    for(int p = 0; p < 2; p++)
    {
      YUVPLANE &plane = planes[p];
      if (plane.texwidth * plane.texheight == 0)
        continue;

      glBindTexture(m_textureTarget, plane.id);

      if (p == 1)
        glTexImage2D(m_textureTarget, 0, GL_RG, plane.texwidth, plane.texheight, 0, GL_RG, GL_UNSIGNED_BYTE, NULL);
      else
        glTexImage2D(m_textureTarget, 0, GL_RED, plane.texwidth, plane.texheight, 0, GL_RED, GL_UNSIGNED_BYTE, NULL);

      glTexParameteri(m_textureTarget, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
      glTexParameteri(m_textureTarget, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
      glTexParameteri(m_textureTarget, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
      glTexParameteri(m_textureTarget, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
      VerifyGLState();
    }
  }

  return true;
}
