/*
 *  Copyright (C) 2005-2018 Team Kodi
 *  This file is part of Kodi - https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSES/README.md for more information.
 */

#include "LibInputSettings.h"

#include "FileItem.h"
#include "LibInputHandler.h"
#include "ServiceBroker.h"
#include "settings/lib/Setting.h"
#include "settings/lib/SettingsManager.h"
#include "settings/Settings.h"
#include "utils/log.h"
#include "utils/XBMCTinyXML.h"

#include <algorithm>

static std::vector<std::pair<std::string, std::string>> m_layouts;

CLibInputSettings::CLibInputSettings(CLibInputHandler *handler) :
  m_libInputHandler(handler)
{
  /* register the settings handler and filler */
  CServiceBroker::GetSettings().GetSettingsManager()->RegisterSettingsHandler(this);

  std::set<std::string> settingSet;
  settingSet.insert("input.libinputkeyboardlayout");
  CServiceBroker::GetSettings().RegisterCallback(this, settingSet);

  CServiceBroker::GetSettings().GetSettingsManager()->RegisterSettingOptionsFiller("libinputkeyboardlayout", CLibInputSettings::SettingOptionsKeyboardLayoutsFiller);

  /* load the keyboard layouts from xkeyboard-config */
  std::string xkbFile("/usr/share/X11/xkb/rules/base.xml");

  CFileItem file(xkbFile);

  if (!file.Exists())
  {
    CLog::Log(LOGWARNING, "CLibInputSettings: unable to load keyboard layouts from non-existing file: %s", xkbFile.c_str());
    return;
  }

  CXBMCTinyXML xmlDoc;
  if (!xmlDoc.LoadFile(xkbFile))
  {
    CLog::Log(LOGWARNING, "CLibInputSettings: unable to open: %s", xkbFile.c_str());
    return;
  }

  const TiXmlElement* rootElement = xmlDoc.RootElement();
  if (rootElement == nullptr)
  {
    CLog::Log(LOGWARNING, "CLibInputSettings: missing or invalid XML root element in: %s", xkbFile.c_str());
    return;
  }

  if (rootElement->ValueStr() != "xkbConfigRegistry")
  {
    CLog::Log(LOGWARNING, "CLibInputSettings: unexpected XML root element %s in: %s", rootElement->Value(), xkbFile.c_str());
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

CLibInputSettings::~CLibInputSettings()
{
  /* This currently will cause a segfault upon shutdown as the settings have gone away before windowing */
  // CServiceBroker::GetSettings().GetSettingsManager()->UnregisterSettingOptionsFiller("libinputkeyboardlayout");
  // CServiceBroker::GetSettings().GetSettingsManager()->UnregisterCallback(this);
}

void CLibInputSettings::SettingOptionsKeyboardLayoutsFiller(std::shared_ptr<const CSetting> setting, std::vector< std::pair<std::string, std::string> > &list, std::string &current, void *data)
{
  list = m_layouts;
}

void CLibInputSettings::OnSettingChanged(std::shared_ptr<const CSetting> setting)
{
  if (setting == nullptr)
    return;

  const std::string &settingId = setting->GetId();
  if (settingId == "input.libinputkeyboardlayout")
  {
    std::string layout = std::dynamic_pointer_cast<const CSettingString>(setting)->GetValue();
    m_libInputHandler->SetKeymap(static_cast<std::string>(layout));
  }
}
