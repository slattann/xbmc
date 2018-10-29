/*
 *  Copyright (C) 2018 Team Kodi
 *  This file is part of Kodi - https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSES/README.md for more information.
 */

#include "VideoLayerBridgeHDR.h"
#include "utils/log.h"
#include "windowing/gbm/DRMUtils.h"
#include <linux/videodev2.h>

using namespace KODI::WINDOWING::GBM;

/*
enum hdmi_colorimetry
{
  HDMI_COLORIMETRY_NONE,
  HDMI_COLORIMETRY_ITU_601,
  HDMI_COLORIMETRY_ITU_709,
  HDMI_COLORIMETRY_EXTENDED,
};
*/

enum hdmi_metadata_type
{
  HDMI_STATIC_METADATA_TYPE1 = 1,
};

enum hdmi_eotf
{
  HDMI_EOTF_TRADITIONAL_GAMMA_SDR,
  HDMI_EOTF_TRADITIONAL_GAMMA_HDR,
  HDMI_EOTF_SMPTE_ST2084,
  HDMI_EOTF_BT_2100_HLG,
};

/*
static int GetColorSpace(bool is10bit, int colorPrimaries)
{
  if (is10bit && colorPrimaries != AVCOL_PRI_BT709)
    return V4L2_COLORSPACE_BT2020;

  if (colorPrimaries == AVCOL_PRI_SMPTE170M)
    return V4L2_COLORSPACE_SMPTE170M;

  return V4L2_COLORSPACE_REC709;
}
*/

static int GetEOTF(bool is10bit, int colorTransfer)
{
  if (is10bit)
  {
    if (colorTransfer == AVCOL_TRC_SMPTE2084)
      return HDMI_EOTF_SMPTE_ST2084;

    if (colorTransfer == AVCOL_TRC_ARIB_STD_B67 ||
        colorTransfer == AVCOL_TRC_BT2020_10)
      return HDMI_EOTF_BT_2100_HLG;
  }

  return HDMI_EOTF_TRADITIONAL_GAMMA_SDR;
}

void CVideoLayerBridgeHDR::Configure(const VideoPicture& videoPicture)
{
  bool is10bit = videoPicture.colorBits == 10;
  m_hdr_metadata.type = HDMI_STATIC_METADATA_TYPE1;
  m_hdr_metadata.eotf = GetEOTF(is10bit, videoPicture.color_transfer);

  if (m_hdr_blob_id)
    drmModeDestroyPropertyBlob(m_DRM->GetFileDescriptor(), m_hdr_blob_id);

  m_hdr_blob_id = 0;
  if (m_hdr_metadata.eotf > 0)
    drmModeCreatePropertyBlob(m_DRM->GetFileDescriptor(), &m_hdr_metadata, sizeof(m_hdr_metadata), &m_hdr_blob_id);

  CLog::Log(LOGNOTICE, "CVideoLayerBridgeHDR::{} - format={} is10bit={} width={} height={} colorspace={} color_primaries={} color_trc={} color_range={} eotf={} blob_id={}",
            __FUNCTION__, videoPicture.videoBuffer->GetFormat(), is10bit, videoPicture.iWidth, videoPicture.iHeight, videoPicture.color_space, videoPicture.color_primaries, videoPicture.color_transfer, videoPicture.color_range == 1 ? "limited" : "full", m_hdr_metadata.eotf, m_hdr_blob_id);

  /*
  struct plane* plane = m_DRM->GetOverlayPlane();
  if (m_DRM->SupportsProperty(plane, "COLOR_SPACE") &&
      m_DRM->SupportsProperty(plane, "EOTF"))
  {
    m_DRM->AddProperty(plane, "COLOR_SPACE", GetColorSpace(is10bit, videoPicture.color_primaries));
    m_DRM->AddProperty(plane, "EOTF", m_hdr_metadata.eotf);
  }
  */

  struct connector* connector = m_DRM->GetConnector();
  if (m_DRM->SupportsProperty(connector, "HDR_SOURCE_METADATA"))
  {
    m_DRM->AddProperty(connector, "HDR_SOURCE_METADATA", m_hdr_blob_id);
  }

  m_DRM->SetActive(true);
}

void CVideoLayerBridgeHDR::Disable()
{
  /*
  struct plane* plane = m_DRM->GetOverlayPlane();
  if (m_DRM->SupportsProperty(plane, "COLOR_SPACE") &&
      m_DRM->SupportsProperty(plane, "EOTF"))
  {
    m_DRM->AddProperty(plane, "COLOR_SPACE", V4L2_COLORSPACE_DEFAULT);
    m_DRM->AddProperty(plane, "EOTF", HDMI_EOTF_TRADITIONAL_GAMMA_SDR);
  }
  */

  struct connector* connector = m_DRM->GetConnector();
  if (m_DRM->SupportsProperty(connector, "HDR_SOURCE_METADATA"))
  {
    m_DRM->AddProperty(connector, "HDR_SOURCE_METADATA", 0);
  }

  m_DRM->SetActive(true);

  if (m_hdr_blob_id)
    drmModeDestroyPropertyBlob(m_DRM->GetFileDescriptor(), m_hdr_blob_id);

  m_hdr_blob_id = 0;
}
