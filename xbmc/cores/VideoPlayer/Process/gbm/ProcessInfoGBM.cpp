/*
 *  Copyright (C) 2019 Team Kodi
 *  This file is part of Kodi - https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSES/README.md for more information.
 */

#include "ProcessInfoGBM.h"
#include "VideoBufferDumb.h"

using namespace VIDEOPLAYER;

CProcessInfo* CProcessInfoGBM::Create()
{
  return new CProcessInfoGBM();
}

void CProcessInfoGBM::Register()
{
  CProcessInfo::RegisterProcessControl("gbm", CProcessInfoGBM::Create);
}

CProcessInfoGBM::CProcessInfoGBM()
{
  std::shared_ptr<IVideoBufferPool> pool = std::make_shared<CVideoBufferPoolDumb>();
  m_videoBufferManager.RegisterPool(pool);
}
