/*
 *      Copyright (C) 2005-2013 Team XBMC
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
 *  along with XBMC; see the file COPYING.  If not, see
 *  <http://www.gnu.org/licenses/>.
 *
 */

#include "WinSystemEGLStreams.h"
#include "ServiceBroker.h"
#include "settings/DisplaySettings.h"
#include "settings/Settings.h"
#include "settings/lib/Setting.h"
#include <string.h>

#include "windowing/GraphicContext.h"
#include "platform/linux/OptionalsReg.h"
#include "platform/linux/powermanagement/LinuxPowerSyscall.h"
#include "settings/DisplaySettings.h"
#include "utils/log.h"
#include "utils/StringUtils.h"
#include "../WinEventsLinux.h"
#include "DRMAtomic.h"
#include "messaging/ApplicationMessenger.h"


CWinSystemEGLStreams::CWinSystemEGLStreams() :
  m_DRM(new CDRMAtomic)
{
  std::string envSink;
  if (getenv("AE_SINK"))
    envSink = getenv("AE_SINK");
  if (StringUtils::EqualsNoCase(envSink, "ALSA"))
  {
    OPTIONALS::ALSARegister();
  }
  else if (StringUtils::EqualsNoCase(envSink, "PULSE"))
  {
    OPTIONALS::PulseAudioRegister();
  }
  else if (StringUtils::EqualsNoCase(envSink, "SNDIO"))
  {
    OPTIONALS::SndioRegister();
  }
  else
  {
    if (!OPTIONALS::PulseAudioRegister())
    {
      if (!OPTIONALS::ALSARegister())
      {
        OPTIONALS::SndioRegister();
      }
    }
  }

  m_winEvents.reset(new CWinEventsLinux());
  CLinuxPowerSyscall::Register();
}

bool CWinSystemEGLStreams::InitWindowSystem()
{
  if (!m_DRM->InitDrm())
  {
    CLog::Log(LOGERROR, "CWinSystemEGLStreams::%s - failed to initialize Atomic DRM", __FUNCTION__);
    m_DRM.reset();
    return false;
  }

  CLog::Log(LOGDEBUG, "CWinSystemEGLStreams::%s - initialized DRM", __FUNCTION__);
  return CWinSystemBase::InitWindowSystem();
}

bool CWinSystemEGLStreams::DestroyWindowSystem()
{
  CLog::Log(LOGDEBUG, "CWinSystemEGLStreams::%s - deinitialized DRM", __FUNCTION__);
  return true;
}

bool CWinSystemEGLStreams::CreateNewWindow(const std::string& name,
                                           bool fullScreen,
                                           RESOLUTION_INFO& res)
{
  if(!m_DRM->SetMode(res))
  {
    CLog::Log(LOGERROR, "CWinSystemEGLStreams::%s - failed to set DRM mode", __FUNCTION__);
    return false;
  }

  CLog::Log(LOGDEBUG, "CWinSystemEGLStreams::%s - initialized GBM", __FUNCTION__);
  return true;
}

bool CWinSystemEGLStreams::DestroyWindow()
{
  CLog::Log(LOGDEBUG, "CWinSystemEGLStreams::%s - deinitialized GBM", __FUNCTION__);
  return true;
}

void CWinSystemEGLStreams::UpdateResolutions()
{
  CWinSystemBase::UpdateResolutions();

  UpdateDesktopResolution(CDisplaySettings::GetInstance().GetResolutionInfo(RES_DESKTOP),
                          0,
                          m_DRM->m_mode->hdisplay,
                          m_DRM->m_mode->vdisplay,
                          m_DRM->m_mode->vrefresh);

  std::vector<RESOLUTION_INFO> resolutions;

  if (!m_DRM->GetModes(resolutions) || resolutions.empty())
  {
    CLog::Log(LOGWARNING, "CWinSystemEGLStreams::%s - Failed to get resolutions", __FUNCTION__);
  }
  else
  {
    CDisplaySettings::GetInstance().ClearCustomResolutions();

    for (unsigned int i = 0; i < resolutions.size(); i++)
    {
      CServiceBroker::GetWinSystem()->GetGfxContext().ResetOverscan(resolutions[i]);
      CDisplaySettings::GetInstance().AddResolutionInfo(resolutions[i]);

      CLog::Log(LOGNOTICE, "Found resolution %dx%d for display %d with %dx%d%s @ %f Hz",
                resolutions[i].iWidth,
                resolutions[i].iHeight,
                resolutions[i].iScreen,
                resolutions[i].iScreenWidth,
                resolutions[i].iScreenHeight,
                resolutions[i].dwFlags & D3DPRESENTFLAG_INTERLACED ? "i" : "",
                resolutions[i].fRefreshRate);
    }
  }

  CDisplaySettings::GetInstance().ApplyCalibrations();
}

bool CWinSystemEGLStreams::ResizeWindow(int newWidth, int newHeight, int newLeft, int newTop)
{
  return true;
}

bool CWinSystemEGLStreams::SetFullScreen(bool fullScreen, RESOLUTION_INFO& res, bool blankOtherDisplays)
{
  if(!m_DRM->SetMode(res))
  {
    CLog::Log(LOGERROR, "CWinSystemEGLStreams::%s - failed to set DRM mode", __FUNCTION__);
    return false;
  }

  auto result = m_DRM->SetVideoMode(res);

  bool rendered{true};
  bool videoLayer{false};

  m_DRM->FlipPage(rendered, videoLayer);

  return result;
}

bool CWinSystemEGLStreams::Hide()
{
  return false;
}

bool CWinSystemEGLStreams::Show(bool raise)
{
  return true;
}

void CWinSystemEGLStreams::Register(IDispResource *resource)
{
  CSingleLock lock(m_resourceSection);
  m_resources.push_back(resource);
}

void CWinSystemEGLStreams::Unregister(IDispResource *resource)
{
  CSingleLock lock(m_resourceSection);
  std::vector<IDispResource*>::iterator i = find(m_resources.begin(), m_resources.end(), resource);
  if (i != m_resources.end())
  {
    m_resources.erase(i);
  }
}
