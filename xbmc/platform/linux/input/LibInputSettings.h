/*
 *  Copyright (C) 2005-2018 Team Kodi
 *  This file is part of Kodi - https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSES/README.md for more information.
 */

#if defined(HAVE_GBM) || defined(TARGET_RASPBERRY_PI) || defined(HAS_LIBAMCODEC)

#include "settings/lib/ISettingCallback.h"
#include "settings/lib/ISettingsHandler.h"
#include "settings/Settings.h"

#include <memory>
#include <vector>

class CLibInputSettings : public ISettingCallback, public ISettingsHandler
{
public:
  static CLibInputSettings& GetInstance();

  void OnSettingChanged(std::shared_ptr<const CSetting> setting) override;
  static void SettingOptionsKeyboardLayoutsFiller(std::shared_ptr<const CSetting> setting, std::vector< std::pair<std::string, std::string> > &list, std::string &current, void *data);

protected:
  CLibInputSettings();
  CLibInputSettings(const CLibInputSettings&) = delete;
  CLibInputSettings& operator=(CLibInputSettings const&) = delete;

private:
  std::vector<std::pair<std::string, std::string>> m_layouts;
};

#else

class CLibInputSettings : public ISettingCallback, public ISettingsHandler
{
public:
  static CLibInputSettings& GetInstance()
  {
    static CLibInputSettings sLibInputSettings;
    return sLibInputSettings;
  }

  static void SettingOptionsKeyboardLayoutsFiller(std::shared_ptr<const CSetting> setting, std::vector< std::pair<std::string, std::string> > &list, std::string &current, void *data) { };
};

#endif
