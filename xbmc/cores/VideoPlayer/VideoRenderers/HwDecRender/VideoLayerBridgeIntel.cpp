/*
 *  Copyright (C) 2017-2018 Team Kodi
 *  This file is part of Kodi - https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSES/README.md for more information.
 */

#include "VideoLayerBridgeIntel.h"

#include "cores/VideoPlayer/Process/gbm/VideoBufferDRMPRIME.h"
#include "utils/EDIDUtils.h"
#include "utils/log.h"
#include "windowing/gbm/DRMUtils.h"

using namespace KODI::WINDOWING::GBM;

namespace
{
  /* Colorspace values as per CEA spec */
const int DRM_MODE_COLORIMETRY_DEFAULT = 0;

/* CEA 861 Normal Colorimetry options */
const int DRM_MODE_COLORIMETRY_NO_DATA = 0;
const int DRM_MODE_COLORIMETRY_SMPTE_170M_YCC = 1;
const int DRM_MODE_COLORIMETRY_BT709_YCC = 2;

/* CEA 861 Extended Colorimetry Options */
const int DRM_MODE_COLORIMETRY_XVYCC_601 = 3;
const int DRM_MODE_COLORIMETRY_XVYCC_709 = 4;
const int DRM_MODE_COLORIMETRY_SYCC_601 = 5;
const int DRM_MODE_COLORIMETRY_OPYCC_601 = 6;
const int DRM_MODE_COLORIMETRY_OPRGB = 7;
const int DRM_MODE_COLORIMETRY_BT2020_CYCC = 8;
const int DRM_MODE_COLORIMETRY_BT2020_RGB = 9;
const int DRM_MODE_COLORIMETRY_BT2020_YCC = 10;

/* Additional Colorimetry extension added as part of CTA 861.G */
const int DRM_MODE_COLORIMETRY_DCI_P3_RGB_D65 = 11;
const int DRM_MODE_COLORIMETRY_DCI_P3_RGB_THEATER = 12;
}

CVideoLayerBridgeIntel::CVideoLayerBridgeIntel(std::shared_ptr<CDRMUtils> drm)
  : CVideoLayerBridgeDRMPRIME(drm)
{
}

void CVideoLayerBridgeIntel::Disable()
{
  CVideoLayerBridgeDRMPRIME::Disable();

  struct connector* connector = m_DRM->GetConnector();
  if (m_DRM->SupportsProperty(connector, "Colorspace"))
  {
    CLog::Log(LOGDEBUG, "CVideoLayerBridgeIntel::{} - Colorspace={}", __FUNCTION__, DRM_MODE_COLORIMETRY_DEFAULT);
    m_DRM->AddProperty(connector, "Colorspace", DRM_MODE_COLORIMETRY_DEFAULT);
  }
}

void CVideoLayerBridgeIntel::Configure(IVideoBufferDRMPRIME* buffer)
{
  CVideoLayerBridgeDRMPRIME::Configure(buffer);

  struct connector* connector = m_DRM->GetConnector();
  if (m_DRM->SupportsProperty(connector, "Colorspace"))
  {
    if (m_edid->SupportsColorSpace(buffer->GetColorEncoding()))
    {
      CLog::Log(LOGDEBUG, "CVideoLayerBridgeIntel::{} - Colorspace={}", __FUNCTION__, DRM_MODE_COLORIMETRY_BT2020_RGB);
      m_DRM->AddProperty(connector, "Colorspace", DRM_MODE_COLORIMETRY_BT2020_RGB);
    }
  }
}
