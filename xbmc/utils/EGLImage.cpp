/*
 *  Copyright (C) 2017-2018 Team Kodi
 *  This file is part of Kodi - https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSES/README.md for more information.
 */

#include "EGLImage.h"

#include "EGLUtils.h"
#include "log.h"
#include "windowing/gbm/DRMUtils.h"

namespace
{
  const EGLint eglDmabufPlaneFdAttr[CEGLImage::MAX_NUM_PLANES] =
  {
    EGL_DMA_BUF_PLANE0_FD_EXT,
    EGL_DMA_BUF_PLANE1_FD_EXT,
    EGL_DMA_BUF_PLANE2_FD_EXT,
  };

  const EGLint eglDmabufPlaneOffsetAttr[CEGLImage::MAX_NUM_PLANES] =
  {
    EGL_DMA_BUF_PLANE0_OFFSET_EXT,
    EGL_DMA_BUF_PLANE1_OFFSET_EXT,
    EGL_DMA_BUF_PLANE2_OFFSET_EXT,
  };

  const EGLint eglDmabufPlanePitchAttr[CEGLImage::MAX_NUM_PLANES] =
  {
    EGL_DMA_BUF_PLANE0_PITCH_EXT,
    EGL_DMA_BUF_PLANE1_PITCH_EXT,
    EGL_DMA_BUF_PLANE2_PITCH_EXT,
  };

#if defined(EGL_EXT_image_dma_buf_import_modifiers)
  const EGLint eglDmabufPlaneModifierLoAttr[CEGLImage::MAX_NUM_PLANES] =
  {
    EGL_DMA_BUF_PLANE0_MODIFIER_LO_EXT,
    EGL_DMA_BUF_PLANE1_MODIFIER_LO_EXT,
    EGL_DMA_BUF_PLANE2_MODIFIER_LO_EXT,
  };

  const EGLint eglDmabufPlaneModifierHiAttr[CEGLImage::MAX_NUM_PLANES] =
  {
    EGL_DMA_BUF_PLANE0_MODIFIER_HI_EXT,
    EGL_DMA_BUF_PLANE1_MODIFIER_HI_EXT,
    EGL_DMA_BUF_PLANE2_MODIFIER_HI_EXT,
  };
#endif
} // namespace

CEGLImage::CEGLImage(EGLDisplay display) :
  m_display(display)
{
  m_eglCreateImageKHR = CEGLUtils::GetRequiredProcAddress<PFNEGLCREATEIMAGEKHRPROC>("eglCreateImageKHR");
  m_eglDestroyImageKHR = CEGLUtils::GetRequiredProcAddress<PFNEGLDESTROYIMAGEKHRPROC>("eglDestroyImageKHR");
  m_glEGLImageTargetTexture2DOES = CEGLUtils::GetRequiredProcAddress<PFNGLEGLIMAGETARGETTEXTURE2DOESPROC>("glEGLImageTargetTexture2DOES");
}

bool CEGLImage::CreateImage(EglAttrs imageAttrs)
{
  CEGLAttributes<22> attribs;
  attribs.Add({{EGL_WIDTH, imageAttrs.width},
               {EGL_HEIGHT, imageAttrs.height},
               {EGL_LINUX_DRM_FOURCC_EXT, static_cast<EGLint>(imageAttrs.format)}});

  if (imageAttrs.colorSpace != 0 && imageAttrs.colorRange != 0)
  {
    attribs.Add({{EGL_YUV_COLOR_SPACE_HINT_EXT, imageAttrs.colorSpace},
                 {EGL_SAMPLE_RANGE_HINT_EXT, imageAttrs.colorRange},
                 {EGL_YUV_CHROMA_VERTICAL_SITING_HINT_EXT, EGL_YUV_CHROMA_SITING_0_EXT},
                 {EGL_YUV_CHROMA_HORIZONTAL_SITING_HINT_EXT, EGL_YUV_CHROMA_SITING_0_EXT}});
  }

  for (int i = 0; i < MAX_NUM_PLANES; i++)
  {
    if (imageAttrs.planes[i].fd != 0)
    {
      attribs.Add({{eglDmabufPlaneFdAttr[i], imageAttrs.planes[i].fd},
                   {eglDmabufPlaneOffsetAttr[i], imageAttrs.planes[i].offset},
                   {eglDmabufPlanePitchAttr[i], imageAttrs.planes[i].pitch}});

#if defined(EGL_EXT_image_dma_buf_import_modifiers)
      if (imageAttrs.planes[i].modifier != DRM_FORMAT_MOD_INVALID && imageAttrs.planes[i].modifier != DRM_FORMAT_MOD_LINEAR)
        attribs.Add({{eglDmabufPlaneModifierLoAttr[i], static_cast<EGLint>(imageAttrs.planes[i].modifier & 0xFFFFFFFF)},
                     {eglDmabufPlaneModifierHiAttr[i], static_cast<EGLint>(imageAttrs.planes[i].modifier >> 32)}});
#endif
    }
  }

  m_image = m_eglCreateImageKHR(m_display, EGL_NO_CONTEXT, EGL_LINUX_DMA_BUF_EXT, nullptr, attribs.Get());

  if(!m_image)
  {
    CLog::Log(LOGERROR, "CEGLImage::%s - failed to import buffer into EGL image: %d", __FUNCTION__, eglGetError());
    return false;
  }

  return true;
}

void CEGLImage::UploadImage(GLenum textureTarget)
{
  m_glEGLImageTargetTexture2DOES(textureTarget, m_image);
}

void CEGLImage::DestroyImage()
{
  m_eglDestroyImageKHR(m_display, m_image);
}

bool CEGLImage::IsFormatSupported(EGLDisplay display, uint32_t fourcc)
{
#if defined(EGL_EXT_image_dma_buf_import_modifiers)

  if (!CEGLUtils::HasExtension(display, "EGL_EXT_image_dma_buf_import_modifiers"))
  {
    return false;
  }

  auto eglQueryDmaBufFormatsEXT = CEGLUtils::GetRequiredProcAddress<PFNEGLQUERYDMABUFFORMATSEXTPROC>("eglQueryDmaBufFormatsEXT");
  auto eglQueryDmaBufModifiersEXT = CEGLUtils::GetRequiredProcAddress<PFNEGLQUERYDMABUFMODIFIERSEXTPROC>("eglQueryDmaBufModifiersEXT");

  EGLint numFormats;

  if (eglQueryDmaBufFormatsEXT(display, 0, nullptr, &numFormats) != EGL_TRUE)
  {
    CEGLUtils::LogError("failed to query the max number of EGL DMA BUF formats");
    return false;
  }

  std::vector<EGLint> formats(numFormats);

  if (eglQueryDmaBufFormatsEXT(display, numFormats, formats.data(), &numFormats) != EGL_TRUE)
  {
    CEGLUtils::LogError("failed to query EGL DMA BUF formats");
    return false;
  }

  std::string formatsLogStr;

  std::map<EGLint, std::vector<EGLuint64KHR>> formatsMap;

  for (const auto& format : formats)
  {
    formatsLogStr.append(StringUtils::Format("\n%s:", KODI::WINDOWING::GBM::CDRMUtils::FourCCToString(format)));

    numFormats = {};
    if (eglQueryDmaBufModifiersEXT(display, format, 0, nullptr, nullptr, &numFormats) != EGL_TRUE)
    {
      CEGLUtils::LogError("failed to query the max number of EGL DMA BUF format modifiers");
      return false;
    }

    std::vector<EGLuint64KHR> modifiers(numFormats);

    if (eglQueryDmaBufModifiersEXT(display, format, numFormats, modifiers.data(), nullptr, &numFormats) != EGL_TRUE)
    {
      CEGLUtils::LogError("failed to query EGL DMA BUF format modifiers");
      return false;
    }

    for (const auto& modifier: modifiers)
    {
      formatsLogStr.append(StringUtils::Format(" %llx", modifier));
    }

    formatsMap.emplace(format, modifiers);
  }

  CLog::Log(LOGDEBUG, "CEGLImage::{} - supported EGL image formats and modifiers:{}", __FUNCTION__, formatsLogStr);

  auto i = formatsMap.find(fourcc);
  if (i != formatsMap.end())
  {
    return true;
  }

#else

  // assume mali support NV12
  if (fourcc == DRM_FORMAT_NV12)
  {
    return true;
  }

#endif

  return false;
}
