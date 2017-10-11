/*
 *      Copyright (C) 2005-2013 Team XBMC
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

#include "utils/log.h"

#include "DRM.h"
#include "DRMAtomic.h"
#include "DRMLegacy.h"

CDRM::CDRM()
  : m_hasAtomic(false)
{
}

void CDRM::FlipPage(CGLContextEGL *pGLContext)
{
  if (m_hasAtomic)
  {
    CDRMAtomic::FlipPage(pGLContext);
  }
  else
  {
    CDRMLegacy::FlipPage(pGLContext);
  }
}

bool CDRM::SetVideoMode(RESOLUTION_INFO res)
{
  if (m_hasAtomic)
  {
    return CDRMAtomic::SetVideoMode(res);
  }
  else
  {
    return CDRMLegacy::SetVideoMode(res);
  }
}

bool CDRM::InitDrm(drm *drm, gbm *gbm)
{
  if (CDRMAtomic::InitDrmAtomic(drm, gbm))
  {
    m_hasAtomic = true;
    CLog::Log(LOGNOTICE, "CDRM::%s - initialized Atomic DRM", __FUNCTION__);
    return true;
  }
  else if (CDRMLegacy::InitDrmLegacy(drm, gbm))
  {
    CLog::Log(LOGNOTICE, "CDRM::%s - initialized Legacy DRM", __FUNCTION__);
    return true;
  }

  return false;
}

void CDRM::DestroyDrm()
{
  if (m_hasAtomic)
  {
    CDRMAtomic::DestroyDrmAtomic();
  }
  else
  {
    CDRMLegacy::DestroyDrmLegacy();
  }
}
