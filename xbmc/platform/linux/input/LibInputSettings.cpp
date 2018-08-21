/*
 *  Copyright (C) 2005-2018 Team Kodi
 *  This file is part of Kodi - https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSES/README.md for more information.
 */

#include "LibInputSettings.h"

#include "FileItem.h"
#include "utils/log.h"
#include "ServiceBroker.h"
#include "settings/lib/Setting.h"
#include "utils/XBMCTinyXML.h"
#if defined(HAVE_GBM)
#include "windowing/gbm/WinSystemGbm.h"
#elif defined(TARGET_RASPBERRY_PI)
#include "windowing/rpi/WinSystemRpi.h"
#elif defined(HAS_LIBAMCODEC)
#include "windowing/amlogic/WinSystemAmlogic.h"
#endif

#include <algorithm>

CLibInputSettings::CLibInputSettings()
{
  std::string xkbFile("/usr/share/X11/xkb/rules/base.xml");

  CFileItem file(xkbFile);

  if (!file.Exists())
  {
    CLog::Log(LOGWARNING, "CLibInputSettings: unable to load keyboard layouts from non-existing file \"%s\"", xkbFile.c_str());
    return;
  }

  CXBMCTinyXML xmlDoc;
  if (!xmlDoc.LoadFile(xkbFile))
  {
    CLog::Log(LOGWARNING, "CLibInputSettings: unable to open %s", xkbFile.c_str());
    return;
  }

  const TiXmlElement* rootElement = xmlDoc.RootElement();
  if (rootElement == nullptr)
  {
    CLog::Log(LOGWARNING, "CLibInputSettings: missing or invalid XML root element in %s", xkbFile.c_str());
    return;
  }

  if (rootElement->ValueStr() != "xkbConfigRegistry")
  {
    CLog::Log(LOGWARNING, "CLibInputSettings: unexpected XML root element \"%s\" in %s", rootElement->Value(), xkbFile.c_str());
    return;
  }

  const TiXmlElement* layoutElement = rootElement->FirstChildElement("layoutList")->FirstChildElement("layout");
  while (layoutElement != nullptr)
  {
    const TiXmlElement* configElement = layoutElement->FirstChildElement("configItem");
    std::string layout = configElement->FirstChild("name")->FirstChild()->ValueStr();
    std::string layoutDescription = configElement->FirstChild("description")->FirstChild()->ValueStr();

    if (!layout.empty() && !layoutDescription.empty())
      m_layouts.emplace_back(std::make_pair(layoutDescription, layout));

    layoutElement = layoutElement->NextSiblingElement();
  }

  std::sort(m_layouts.begin(), m_layouts.end());
}

CLibInputSettings& CLibInputSettings::GetInstance()
{
  static CLibInputSettings sLibInputSettings;
  return sLibInputSettings;
}

void CLibInputSettings::SettingOptionsKeyboardLayoutsFiller(std::shared_ptr<const CSetting> setting, std::vector< std::pair<std::string, std::string> > &list, std::string &current, void *data)
{
  list = CLibInputSettings::GetInstance().m_layouts;
}

void CLibInputSettings::OnSettingChanged(std::shared_ptr<const CSetting> setting)
{
  if (setting == nullptr)
    return;

#if defined(HAVE_GBM)
  CWinSystemGbm *winSystem = dynamic_cast<CWinSystemGbm*>(CServiceBroker::GetWinSystem());
#elif defined(TARGET_RASPBERRY_PI)
  CWinSystemRpi *winSystem = dynamic_cast<CWinSystemRpi*>(CServiceBroker::GetWinSystem());
#elif defined(HAS_LIBAMCODEC)
  CWinSystemAmlogic *winSystem = dynamic_cast<CWinSystemAmlogic*>(CServiceBroker::GetWinSystem());
#endif

  const std::string &settingId = setting->GetId();
  if (settingId == CSettings::SETTING_INPUT_LIBINPUTKEYBOARDLAYOUT)
  {
    std::string layout = std::dynamic_pointer_cast<const CSettingString>(setting)->GetValue();
    winSystem->GetInputManager()->SetKeymap(static_cast<std::string>(layout));
  }
}
