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
#include "PeripheralCecFrameworkAdapter.h"
#include "input/XBIRRemote.h"
#include "Application.h"
#include "messaging/ApplicationMessenger.h"
#include "DynamicDll.h"
#include "threads/SingleLock.h"
#include "dialogs/GUIDialogKaiToast.h"
#include "guilib/GUIWindowManager.h"
#include "input/Key.h"
#include "guilib/LocalizeStrings.h"
#include "pictures/GUIWindowSlideShow.h"
#include "settings/AdvancedSettings.h"
#include "utils/JobManager.h"
#include "utils/log.h"
#include "utils/Variant.h"

#include <sys/ioctl.h>
#include <linux/cec.h>
#include <linux/cec-funcs.h>

using namespace KODI;
using namespace MESSAGING;
using namespace ANNOUNCEMENT;

namespace PERIPHERALS
{

/* time in seconds to ignore standby commands from devices after the screensaver has been activated */
#define SCREENSAVER_TIMEOUT       20
#define VOLUME_CHANGE_TIMEOUT     250
#define VOLUME_REFRESH_TIMEOUT    100

#define LOCALISED_ID_TV           36037
#define LOCALISED_ID_AVR          36038
#define LOCALISED_ID_TV_AVR       36039
#define LOCALISED_ID_STOP         36044
#define LOCALISED_ID_PAUSE        36045
#define LOCALISED_ID_POWEROFF     13005
#define LOCALISED_ID_SUSPEND      13011
#define LOCALISED_ID_QUIT         13009
#define LOCALISED_ID_IGNORE       36028

#define LOCALISED_ID_NONE         231

/* time in seconds to suppress source activation after receiving OnStop */
#define CEC_SUPPRESS_ACTIVATE_SOURCE_AFTER_ON_STOP 2

//##### new code
#include "linux/cec.h"
//#include "linux/cec-funcs.h"

#define VOLUME_MAX 0x64
#define VOLUME_MIN 0

/* States for the RC handling */
#define NOPRESS 0
#define PRESS 1
#define PRESS_HOLD 2

/* The follower safety timeout as defined in the spec */
#define FOLLOWER_SAFETY_TIMEOUT 450
#define MIN_INITIATOR_REP_TIME 200
#define MAX_INITIATOR_REP_TIME 500

#define SAD_FMT_CODE_LPCM 		1
#define SAD_FMT_CODE_AC3		2
#define SAD_FMT_CODE_MPEG1		3
#define SAD_FMT_CODE_MP3 		4
#define SAD_FMT_CODE_MPEG2 		5
#define SAD_FMT_CODE_AAC_LC 		6
#define SAD_FMT_CODE_DTS 		7
#define SAD_FMT_CODE_ATRAC 		8
#define SAD_FMT_CODE_ONE_BIT_AUDIO	9
#define SAD_FMT_CODE_ENHANCED_AC3	10
#define SAD_FMT_CODE_DTS_HD 		11
#define SAD_FMT_CODE_MAT 		12
#define SAD_FMT_CODE_DST 		13
#define SAD_FMT_CODE_WMA_PRO 		14
#define SAD_FMT_CODE_EXTENDED 		15

#define SAD_BIT_DEPTH_MASK_16 		1
#define SAD_BIT_DEPTH_MASK_20 		(1 << 1)
#define SAD_BIT_DEPTH_MASK_24 		(1 << 2)

#define SAD_SAMPLE_FREQ_MASK_32 	1
#define SAD_SAMPLE_FREQ_MASK_44_1 	(1 << 1)
#define SAD_SAMPLE_FREQ_MASK_48 	(1 << 2)
#define SAD_SAMPLE_FREQ_MASK_88_2 	(1 << 3)
#define SAD_SAMPLE_FREQ_MASK_96 	(1 << 4)
#define SAD_SAMPLE_FREQ_MASK_176_4 	(1 << 5)
#define SAD_SAMPLE_FREQ_MASK_192 	(1 << 6)

#define SAD_FRAME_LENGTH_MASK_960 	1
#define SAD_FRAME_LENGTH_MASK_1024	(1 << 1)

#define SAD_EXT_TYPE_MPEG4_HE_AAC 		4
#define SAD_EXT_TYPE_MPEG4_HE_AACv2 		5
#define SAD_EXT_TYPE_MPEG4_AAC_LC 		6
#define SAD_EXT_TYPE_DRA 			7
#define SAD_EXT_TYPE_MPEG4_HE_AAC_SURROUND 	8
#define SAD_EXT_TYPE_MPEG4_AAC_LC_SURROUND	10
#define SAD_EXT_TYPE_MPEG_H_3D_AUDIO		11
#define SAD_EXT_TYPE_AC_4			12
#define SAD_EXT_TYPE_LPCM_3D_AUDIO		13

/* Time between each polling message sent to a device */
#define POLL_PERIOD 15000

#define ARRAY_SIZE(a) \
	(sizeof(a) / sizeof(*a))

#define doioctl(n, r, p) cecNamedIoctl((n)->fd, #r, r, p)

#define cec_phys_addr_exp(pa) \
        ((pa) >> 12), ((pa) >> 8) & 0xf, ((pa) >> 4) & 0xf, (pa) & 0xf

#define transmit(n, m) (doioctl(n, CEC_TRANSMIT, m))

struct cec_enum_values
{
	const char *type_name;
	__u8 value;
};

static const struct cec_enum_values type_ui_cmd[] =
{
	{ "Select", 0x00 },
	{ "Up", 0x01 },
	{ "Down", 0x02 },
	{ "Left", 0x03 },
	{ "Right", 0x04 },
	{ "Right-Up", 0x05 },
	{ "Right-Down", 0x06 },
	{ "Left-Up", 0x07 },
	{ "Left-Down", 0x08 },
	{ "Device Root Menu", 0x09 },
	{ "Device Setup Menu", 0x0a },
	{ "Contents Menu", 0x0b },
	{ "Favorite Menu", 0x0c },
	{ "Back", 0x0d },
	{ "Media Top Menu", 0x10 },
	{ "Media Context-sensitive Menu", 0x11 },
	{ "Number Entry Mode", 0x1d },
	{ "Number 11", 0x1e },
	{ "Number 12", 0x1f },
	{ "Number 0 or Number 10", 0x20 },
	{ "Number 1", 0x21 },
	{ "Number 2", 0x22 },
	{ "Number 3", 0x23 },
	{ "Number 4", 0x24 },
	{ "Number 5", 0x25 },
	{ "Number 6", 0x26 },
	{ "Number 7", 0x27 },
	{ "Number 8", 0x28 },
	{ "Number 9", 0x29 },
	{ "Dot", 0x2a },
	{ "Enter", 0x2b },
	{ "Clear", 0x2c },
	{ "Next Favorite", 0x2f },
	{ "Channel Up", 0x30 },
	{ "Channel Down", 0x31 },
	{ "Previous Channel", 0x32 },
	{ "Sound Select", 0x33 },
	{ "Input Select", 0x34 },
	{ "Display Information", 0x35 },
	{ "Help", 0x36 },
	{ "Page Up", 0x37 },
	{ "Page Down", 0x38 },
	{ "Power", 0x40 },
	{ "Volume Up", 0x41 },
	{ "Volume Down", 0x42 },
	{ "Mute", 0x43 },
	{ "Play", 0x44 },
	{ "Stop", 0x45 },
	{ "Pause", 0x46 },
	{ "Record", 0x47 },
	{ "Rewind", 0x48 },
	{ "Fast forward", 0x49 },
	{ "Eject", 0x4a },
	{ "Skip Forward", 0x4b },
	{ "Skip Backward", 0x4c },
	{ "Stop-Record", 0x4d },
	{ "Pause-Record", 0x4e },
	{ "Angle", 0x50 },
	{ "Sub picture", 0x51 },
	{ "Video on Demand", 0x52 },
	{ "Electronic Program Guide", 0x53 },
	{ "Timer Programming", 0x54 },
	{ "Initial Configuration", 0x55 },
	{ "Select Broadcast Type", 0x56 },
	{ "Select Sound Presentation", 0x57 },
	{ "Audio Description", 0x58 },
	{ "Internet", 0x59 },
	{ "3D Mode", 0x5a },
	{ "Play Function", 0x60 },
	{ "Pause-Play Function", 0x61 },
	{ "Record Function", 0x62 },
	{ "Pause-Record Function", 0x63 },
	{ "Stop Function", 0x64 },
	{ "Mute Function", 0x65 },
	{ "Restore Volume Function", 0x66 },
	{ "Tune Function", 0x67 },
	{ "Select Media Function", 0x68 },
	{ "Select A/V Input Function", 0x69 },
	{ "Select Audio Input Function", 0x6a },
	{ "Power Toggle Function", 0x6b },
	{ "Power Off Function", 0x6c },
	{ "Power On Function", 0x6d },
	{ "F1 (Blue)", 0x71 },
	{ "F2 (Red)", 0x72 },
	{ "F3 (Green)", 0x73 },
	{ "F4 (Yellow)", 0x74 },
	{ "F5", 0x75 },
	{ "Data", 0x76 },
};

struct msgtable
{
	__u8 opcode;
	const char *name;
};

static const struct msgtable msgtable[] =
{
	{ CEC_MSG_ACTIVE_SOURCE, "ACTIVE_SOURCE" },
	{ CEC_MSG_IMAGE_VIEW_ON, "IMAGE_VIEW_ON" },
	{ CEC_MSG_TEXT_VIEW_ON, "TEXT_VIEW_ON" },
	{ CEC_MSG_INACTIVE_SOURCE, "INACTIVE_SOURCE" },
	{ CEC_MSG_REQUEST_ACTIVE_SOURCE, "REQUEST_ACTIVE_SOURCE" },
	{ CEC_MSG_ROUTING_INFORMATION, "ROUTING_INFORMATION" },
	{ CEC_MSG_ROUTING_CHANGE, "ROUTING_CHANGE" },
	{ CEC_MSG_SET_STREAM_PATH, "SET_STREAM_PATH" },
	{ CEC_MSG_STANDBY, "STANDBY" },
	{ CEC_MSG_RECORD_OFF, "RECORD_OFF" },
	{ CEC_MSG_RECORD_ON, "RECORD_ON" },
	{ CEC_MSG_RECORD_STATUS, "RECORD_STATUS" },
	{ CEC_MSG_RECORD_TV_SCREEN, "RECORD_TV_SCREEN" },
	{ CEC_MSG_TIMER_STATUS, "TIMER_STATUS" },
	{ CEC_MSG_TIMER_CLEARED_STATUS, "TIMER_CLEARED_STATUS" },
	{ CEC_MSG_CLEAR_ANALOGUE_TIMER, "CLEAR_ANALOGUE_TIMER" },
	{ CEC_MSG_CLEAR_DIGITAL_TIMER, "CLEAR_DIGITAL_TIMER" },
	{ CEC_MSG_CLEAR_EXT_TIMER, "CLEAR_EXT_TIMER" },
	{ CEC_MSG_SET_ANALOGUE_TIMER, "SET_ANALOGUE_TIMER" },
	{ CEC_MSG_SET_DIGITAL_TIMER, "SET_DIGITAL_TIMER" },
	{ CEC_MSG_SET_EXT_TIMER, "SET_EXT_TIMER" },
	{ CEC_MSG_SET_TIMER_PROGRAM_TITLE, "SET_TIMER_PROGRAM_TITLE" },
	{ CEC_MSG_CEC_VERSION, "CEC_VERSION" },
	{ CEC_MSG_GET_CEC_VERSION, "GET_CEC_VERSION" },
	{ CEC_MSG_REPORT_PHYSICAL_ADDR, "REPORT_PHYSICAL_ADDR" },
	{ CEC_MSG_GIVE_PHYSICAL_ADDR, "GIVE_PHYSICAL_ADDR" },
	{ CEC_MSG_SET_MENU_LANGUAGE, "SET_MENU_LANGUAGE" },
	{ CEC_MSG_GET_MENU_LANGUAGE, "GET_MENU_LANGUAGE" },
	{ CEC_MSG_REPORT_FEATURES, "REPORT_FEATURES" },
	{ CEC_MSG_GIVE_FEATURES, "GIVE_FEATURES" },
	{ CEC_MSG_DECK_CONTROL, "DECK_CONTROL" },
	{ CEC_MSG_DECK_STATUS, "DECK_STATUS" },
	{ CEC_MSG_GIVE_DECK_STATUS, "GIVE_DECK_STATUS" },
	{ CEC_MSG_PLAY, "PLAY" },
	{ CEC_MSG_TUNER_DEVICE_STATUS, "TUNER_DEVICE_STATUS" },
	{ CEC_MSG_GIVE_TUNER_DEVICE_STATUS, "GIVE_TUNER_DEVICE_STATUS" },
	{ CEC_MSG_SELECT_ANALOGUE_SERVICE, "SELECT_ANALOGUE_SERVICE" },
	{ CEC_MSG_SELECT_DIGITAL_SERVICE, "SELECT_DIGITAL_SERVICE" },
	{ CEC_MSG_TUNER_STEP_DECREMENT, "TUNER_STEP_DECREMENT" },
	{ CEC_MSG_TUNER_STEP_INCREMENT, "TUNER_STEP_INCREMENT" },
	{ CEC_MSG_DEVICE_VENDOR_ID, "DEVICE_VENDOR_ID" },
	{ CEC_MSG_GIVE_DEVICE_VENDOR_ID, "GIVE_DEVICE_VENDOR_ID" },
	{ CEC_MSG_VENDOR_REMOTE_BUTTON_UP, "VENDOR_REMOTE_BUTTON_UP" },
	{ CEC_MSG_SET_OSD_STRING, "SET_OSD_STRING" },
	{ CEC_MSG_SET_OSD_NAME, "SET_OSD_NAME" },
	{ CEC_MSG_GIVE_OSD_NAME, "GIVE_OSD_NAME" },
	{ CEC_MSG_MENU_STATUS, "MENU_STATUS" },
	{ CEC_MSG_MENU_REQUEST, "MENU_REQUEST" },
	{ CEC_MSG_USER_CONTROL_PRESSED, "USER_CONTROL_PRESSED" },
	{ CEC_MSG_USER_CONTROL_RELEASED, "USER_CONTROL_RELEASED" },
	{ CEC_MSG_REPORT_POWER_STATUS, "REPORT_POWER_STATUS" },
	{ CEC_MSG_GIVE_DEVICE_POWER_STATUS, "GIVE_DEVICE_POWER_STATUS" },
	{ CEC_MSG_FEATURE_ABORT, "FEATURE_ABORT" },
	{ CEC_MSG_ABORT, "ABORT" },
	{ CEC_MSG_REPORT_AUDIO_STATUS, "REPORT_AUDIO_STATUS" },
	{ CEC_MSG_GIVE_AUDIO_STATUS, "GIVE_AUDIO_STATUS" },
	{ CEC_MSG_SET_SYSTEM_AUDIO_MODE, "SET_SYSTEM_AUDIO_MODE" },
	{ CEC_MSG_SYSTEM_AUDIO_MODE_REQUEST, "SYSTEM_AUDIO_MODE_REQUEST" },
	{ CEC_MSG_SYSTEM_AUDIO_MODE_STATUS, "SYSTEM_AUDIO_MODE_STATUS" },
	{ CEC_MSG_GIVE_SYSTEM_AUDIO_MODE_STATUS, "GIVE_SYSTEM_AUDIO_MODE_STATUS" },
	{ CEC_MSG_REPORT_SHORT_AUDIO_DESCRIPTOR, "REPORT_SHORT_AUDIO_DESCRIPTOR" },
	{ CEC_MSG_REQUEST_SHORT_AUDIO_DESCRIPTOR, "REQUEST_SHORT_AUDIO_DESCRIPTOR" },
	{ CEC_MSG_SET_AUDIO_RATE, "SET_AUDIO_RATE" },
	{ CEC_MSG_REPORT_ARC_INITIATED, "REPORT_ARC_INITIATED" },
	{ CEC_MSG_INITIATE_ARC, "INITIATE_ARC" },
	{ CEC_MSG_REQUEST_ARC_INITIATION, "REQUEST_ARC_INITIATION" },
	{ CEC_MSG_REPORT_ARC_TERMINATED, "REPORT_ARC_TERMINATED" },
	{ CEC_MSG_TERMINATE_ARC, "TERMINATE_ARC" },
	{ CEC_MSG_REQUEST_ARC_TERMINATION, "REQUEST_ARC_TERMINATION" },
	{ CEC_MSG_REPORT_CURRENT_LATENCY, "REPORT_CURRENT_LATENCY" },
	{ CEC_MSG_REQUEST_CURRENT_LATENCY, "REQUEST_CURRENT_LATENCY" },
	{ CEC_MSG_VENDOR_COMMAND, "VENDOR_COMMAND" },
	{ CEC_MSG_VENDOR_COMMAND_WITH_ID, "VENDOR_COMMAND_WITH_ID" },
	{ CEC_MSG_VENDOR_REMOTE_BUTTON_DOWN, "VENDOR_REMOTE_BUTTON_DOWN" },
	{ CEC_MSG_CDC_MESSAGE, "CDC_MESSAGE" },
};

static const struct msgtable cdcmsgtable[] =
{
	{ CEC_MSG_CDC_HEC_INQUIRE_STATE, "CDC_HEC_INQUIRE_STATE" },
	{ CEC_MSG_CDC_HEC_REPORT_STATE, "CDC_HEC_REPORT_STATE" },
	{ CEC_MSG_CDC_HEC_SET_STATE, "CDC_HEC_SET_STATE" },
	{ CEC_MSG_CDC_HEC_SET_STATE_ADJACENT, "CDC_HEC_SET_STATE_ADJACENT" },
	{ CEC_MSG_CDC_HEC_REQUEST_DEACTIVATION, "CDC_HEC_REQUEST_DEACTIVATION" },
	{ CEC_MSG_CDC_HEC_NOTIFY_ALIVE, "CDC_HEC_NOTIFY_ALIVE" },
	{ CEC_MSG_CDC_HEC_DISCOVER, "CDC_HEC_DISCOVER" },
	{ CEC_MSG_CDC_HPD_SET_STATE, "CDC_HPD_SET_STATE" },
	{ CEC_MSG_CDC_HPD_REPORT_STATE, "CDC_HPD_REPORT_STATE" },
};

// ##############new code

CPeripheralCecFrameworkAdapter::CPeripheralCecFrameworkAdapter(
		CPeripherals& manager, const PeripheralScanResult& scanResult, CPeripheralBus* bus)
  : CPeripheralHID(manager, scanResult, bus)
  , CThread("CECAdapter")
{
	ResetMembers();
	m_features.push_back(FEATURE_CEC_FRAMEWORK);
	m_strComPort = scanResult.m_strLocation;
	Open();
}

CPeripheralCecFrameworkAdapter::~CPeripheralCecFrameworkAdapter(void)
{
	{
		CSingleLock lock(m_critSection);
		CAnnouncementManager::GetInstance().RemoveAnnouncer(this);
		m_bStop = true;
	}

	StopThread(true);
}

bool CPeripheralCecFrameworkAdapter::Open()
{
	m_node->fd = open(m_strComPort.c_str(), O_RDWR);
}

void CPeripheralCecFrameworkAdapter::ResetMembers(void)
{
	m_bStarted = false;
	m_bHasButton = false;
	m_bIsReady = false;
	m_bHasConnectedAudioSystem = false;
	m_strMenuLanguage = "???";
	m_lastKeypress = 0;
	m_lastChange = VOLUME_CHANGE_NONE;
	m_iExitCode = EXITCODE_QUIT;
	m_bIsMuted = false; //! @todo fetch the correct initial value when system audiostatus is implemented in libCEC
	m_bGoingToStandby = false;
	m_bIsRunning = false;
	m_bDeviceRemoved = false;
	m_bActiveSourcePending = false;
	m_bStandbyPending = false;
	m_bActiveSourceBeforeStandby = false;
	m_bOnPlayReceived = false;
	m_bPlaybackPaused = false;
	m_bPowerOnScreensaver = false;
	m_bUseTVMenuLanguage = false;
	m_bSendInactiveSource = false;
	m_bPowerOffScreensaver = false;
	m_bShutdownOnStandby = false;

	m_currentButton.iButton = 0;
	m_currentButton.iDuration = 0;
	m_standbySent.SetValid(false);
}

void CPeripheralCecFrameworkAdapter::Announce(AnnouncementFlag flag,
		const char *sender, const char *message, const CVariant &data)
{
	if (flag == ANNOUNCEMENT::System && !strcmp(sender, "xbmc")
			&& !strcmp(message, "OnQuit") && m_bIsReady)
	{
		CSingleLock lock(m_critSection);
		m_iExitCode = static_cast<int>(data["exitcode"].asInteger(EXITCODE_QUIT));
		CAnnouncementManager::GetInstance().RemoveAnnouncer(this);
		StopThread(false);
	}
	else if (flag == GUI && !strcmp(sender, "xbmc")
			&& !strcmp(message, "OnScreensaverDeactivated") && m_bIsReady)
	{
		bool bIgnoreDeactivate(false);
		if (data["shuttingdown"].isBoolean())
		{
			// don't respond to the deactivation if we are just going to suspend/shutdown anyway
			// the tv will not have time to switch on before being told to standby and
			// may not action the standby command.
			bIgnoreDeactivate = data["shuttingdown"].asBoolean();
			if (bIgnoreDeactivate)
				CLog::Log(LOGDEBUG, "%s - ignoring OnScreensaverDeactivated for power action", __FUNCTION__);
		}
		if (m_bPowerOnScreensaver && !bIgnoreDeactivate)
		{
			ActivateSource();
		}
	}
	else if (flag == GUI && !strcmp(sender, "xbmc")
			&& !strcmp(message, "OnScreensaverActivated") && m_bIsReady)
	{
		// Don't put devices to standby if application is currently playing
		if (!g_application.m_pPlayer->IsPlaying() && m_bPowerOffScreensaver)
		{
			// only power off when we're the active source
			if (IsActiveSource())
				StandbyDevices();
		}
	}
	else if (flag == ANNOUNCEMENT::System && !strcmp(sender, "xbmc")
			&& !strcmp(message, "OnSleep"))
	{
		// this will also power off devices when we're the active source
		{
			CSingleLock lock(m_critSection);
			m_bGoingToStandby = true;
		}
		StopThread();
	}
	else if (flag == ANNOUNCEMENT::System && !strcmp(sender, "xbmc")
			&& !strcmp(message, "OnWake"))
	{
		CLog::Log(LOGDEBUG, "%s - reconnecting to the CEC adapter after standby mode", __FUNCTION__);
		if (ReopenConnection())
		{
			bool bActivate(false);
			{
				CSingleLock lock(m_critSection);
				bActivate = m_bActiveSourceBeforeStandby;
				m_bActiveSourceBeforeStandby = false;
			}
			if (bActivate)
				ActivateSource();
		}
	}
	else if (flag == Player && !strcmp(sender, "xbmc")
			&& !strcmp(message, "OnStop"))
	{
		CSingleLock lock(m_critSection);
		m_preventActivateSourceOnPlay = CDateTime::GetCurrentDateTime();
		m_bOnPlayReceived = false;
	}
	else if (flag == Player && !strcmp(sender, "xbmc")
			&& !strcmp(message, "OnPlay"))
	{
		// activate the source when playback started, and the option is enabled
		bool bActivateSource(false);
		{
			CSingleLock lock(m_critSection);
			bActivateSource = (!m_bOnPlayReceived && !IsActiveSource()
					&& (!m_preventActivateSourceOnPlay.IsValid()
							|| CDateTime::GetCurrentDateTime() - m_preventActivateSourceOnPlay
									> CDateTimeSpan(0, 0, 0,
									CEC_SUPPRESS_ACTIVATE_SOURCE_AFTER_ON_STOP)));
			m_bOnPlayReceived = true;
		}
		if (bActivateSource)
			ActivateSource();
	}
}

bool CPeripheralCecFrameworkAdapter::InitialiseFeature(
		const PeripheralFeature feature)
{
	if (feature == FEATURE_CEC_FRAMEWORK && !m_bStarted && GetSettingBool("enabled"))
	{
		// hide settings that have an override set
		if (!GetSettingString("wake_devices_advanced").empty())
			SetSettingVisible("wake_devices", false);
		if (!GetSettingString("standby_devices_advanced").empty())
			SetSettingVisible("standby_devices", false);
		m_bStarted = true;
		Create();
	}

	return CPeripheral::InitialiseFeature(feature);
}

bool CPeripheralCecFrameworkAdapter::HasAudioControl(void)
{
	CSingleLock lock(m_critSection);
	return m_bHasConnectedAudioSystem;
}

void CPeripheralCecFrameworkAdapter::SetAudioSystemConnected(bool bSetTo)
{
	CSingleLock lock(m_critSection);
	m_bHasConnectedAudioSystem = bSetTo;
}

void CPeripheralCecFrameworkAdapter::ProcessVolumeChange(void)
{
	bool bSendRelease(false);
	CecVolumeChange pendingVolumeChange = VOLUME_CHANGE_NONE;
	{
		CSingleLock lock(m_critSection);
		if (!m_volumeChangeQueue.empty())
		{
			/* get the first change from the queue */
			pendingVolumeChange = m_volumeChangeQueue.front();
			m_volumeChangeQueue.pop();

			/* remove all dupe entries */
			while (!m_volumeChangeQueue.empty()
					&& m_volumeChangeQueue.front() == pendingVolumeChange)
				m_volumeChangeQueue.pop();

			/* send another keypress after VOLUME_REFRESH_TIMEOUT ms */
			bool bRefresh(
					XbmcThreads::SystemClockMillis()
							- m_lastKeypress> VOLUME_REFRESH_TIMEOUT);

			/* only send the keypress when it hasn't been sent yet */
			if (pendingVolumeChange != m_lastChange)
			{
				m_lastKeypress = XbmcThreads::SystemClockMillis();
				m_lastChange = pendingVolumeChange;
			}
			else if (bRefresh)
			{
				m_lastKeypress = XbmcThreads::SystemClockMillis();
				pendingVolumeChange = m_lastChange;
			}
			else
				pendingVolumeChange = VOLUME_CHANGE_NONE;
		}
		else if (m_lastKeypress
				> 0&& XbmcThreads::SystemClockMillis() - m_lastKeypress > VOLUME_CHANGE_TIMEOUT)
		{
			/* send a key release */
			m_lastKeypress = 0;
			bSendRelease = true;
			m_lastChange = VOLUME_CHANGE_NONE;
		}
	}

	switch (pendingVolumeChange)
	{
	case VOLUME_CHANGE_UP:
		VolumeUp();
		break;
	case VOLUME_CHANGE_DOWN:
		VolumeDown();
		break;
	case VOLUME_CHANGE_MUTE:
		ToggleMute();
		break;
	case VOLUME_CHANGE_NONE:
		break;
	}
}

void CPeripheralCecFrameworkAdapter::VolumeUp(void)
{
	if (HasAudioControl())
	{
		CSingleLock lock(m_critSection);
		m_volumeChangeQueue.push(VOLUME_CHANGE_UP);
	}
}

void CPeripheralCecFrameworkAdapter::VolumeDown(void)
{
	if (HasAudioControl())
	{
		CSingleLock lock(m_critSection);
		m_volumeChangeQueue.push(VOLUME_CHANGE_DOWN);
	}
}

void CPeripheralCecFrameworkAdapter::ToggleMute(void)
{
	if (HasAudioControl())
	{
		CSingleLock lock(m_critSection);
		m_volumeChangeQueue.push(VOLUME_CHANGE_MUTE);
	}
}

bool CPeripheralCecFrameworkAdapter::IsMuted(void)
{
	if (HasAudioControl())
	{
		CSingleLock lock(m_critSection);
		return m_bIsMuted;
	}
	return false;
}

void CPeripheralCecFrameworkAdapter::SetMenuLanguage(const char *strLanguage)
{
	if (StringUtils::EqualsNoCase(m_strMenuLanguage, strLanguage))
		return;

	std::string strGuiLanguage;

	if (!strcmp(strLanguage, "bul"))
		strGuiLanguage = "bg_bg";
	else if (!strcmp(strLanguage, "hrv"))
		strGuiLanguage = "hr_hr";
	else if (!strcmp(strLanguage, "cze"))
		strGuiLanguage = "cs_cz";
	else if (!strcmp(strLanguage, "dan"))
		strGuiLanguage = "da_dk";
	else if (!strcmp(strLanguage, "dut"))
		strGuiLanguage = "nl_nl";
	else if (!strcmp(strLanguage, "eng"))
		strGuiLanguage = "en_gb";
	else if (!strcmp(strLanguage, "fin"))
		strGuiLanguage = "fi_fi";
	else if (!strcmp(strLanguage, "fre"))
		strGuiLanguage = "fr_fr";
	else if (!strcmp(strLanguage, "ger"))
		strGuiLanguage = "de_de";
	else if (!strcmp(strLanguage, "gre"))
		strGuiLanguage = "el_gr";
	else if (!strcmp(strLanguage, "hun"))
		strGuiLanguage = "hu_hu";
	else if (!strcmp(strLanguage, "ita"))
		strGuiLanguage = "it_it";
	else if (!strcmp(strLanguage, "nor"))
		strGuiLanguage = "nb_no";
	else if (!strcmp(strLanguage, "pol"))
		strGuiLanguage = "pl_pl";
	else if (!strcmp(strLanguage, "por"))
		strGuiLanguage = "pt_pt";
	else if (!strcmp(strLanguage, "rum"))
		strGuiLanguage = "ro_ro";
	else if (!strcmp(strLanguage, "rus"))
		strGuiLanguage = "ru_ru";
	else if (!strcmp(strLanguage, "srp"))
		strGuiLanguage = "sr_rs@latin";
	else if (!strcmp(strLanguage, "slo"))
		strGuiLanguage = "sk_sk";
	else if (!strcmp(strLanguage, "slv"))
		strGuiLanguage = "sl_si";
	else if (!strcmp(strLanguage, "spa"))
		strGuiLanguage = "es_es";
	else if (!strcmp(strLanguage, "swe"))
		strGuiLanguage = "sv_se";
	else if (!strcmp(strLanguage, "tur"))
		strGuiLanguage = "tr_tr";

	if (!strGuiLanguage.empty())
	{
		strGuiLanguage = "resource.language." + strGuiLanguage;
		CApplicationMessenger::GetInstance().PostMsg(TMSG_SETLANGUAGE, -1, -1, nullptr, strGuiLanguage);
		CLog::Log(LOGDEBUG, "%s - language set to '%s'", __FUNCTION__, strGuiLanguage.c_str());
	}
	else
		CLog::Log(LOGWARNING, "%s - TV menu language set to unknown value '%s'", __FUNCTION__, strLanguage);
}

void CPeripheralCecFrameworkAdapter::OnTvStandby(void)
{
	int iActionOnTvStandby = GetSettingInt("standby_pc_on_tv_standby");
	switch (iActionOnTvStandby)
	{
	case LOCALISED_ID_POWEROFF:
		m_bStarted = false;
		g_application.ExecuteXBMCAction("Shutdown");
		break;
	case LOCALISED_ID_SUSPEND:
		m_bStarted = false;
		g_application.ExecuteXBMCAction("Suspend");
		break;
	case LOCALISED_ID_QUIT:
		m_bStarted = false;
		g_application.ExecuteXBMCAction("Quit");
		break;
	case LOCALISED_ID_PAUSE:
		g_application.OnAction(CAction(ACTION_PAUSE));
		break;
	case LOCALISED_ID_STOP:
		g_application.StopPlaying();
		break;
	default:
		CLog::Log(LOGERROR, "%s - Unexpected [standby_pc_on_tv_standby] setting value", __FUNCTION__);
		break;
	}
}

void CPeripheralCecFrameworkAdapter::GetNextKey(void)
{
	CSingleLock lock(m_critSection);
	m_bHasButton = false;
	if (m_bIsReady)
	{
		std::vector<CecButtonPress>::iterator it = m_buttonQueue.begin();
		if (it != m_buttonQueue.end())
		{
			m_currentButton = (*it);
			m_buttonQueue.erase(it);
			m_bHasButton = true;
		}
	}
}

void CPeripheralCecFrameworkAdapter::PushCecKeypress(const CecButtonPress &key)
{
	CLog::Log(LOGDEBUG, "%s - received key %2x duration %d", __FUNCTION__, key.iButton, key.iDuration);

	CSingleLock lock(m_critSection);
	if (key.iDuration > 0)
	{
		if (m_currentButton.iButton == key.iButton
				&& m_currentButton.iDuration == 0)
		{
			// update the duration
			if (m_bHasButton)
				m_currentButton.iDuration = key.iDuration;
			// ignore this one, since it's already been handled by xbmc
			return;
		}
		// if we received a keypress with a duration set, try to find the same one without a duration set, and replace it
		for (std::vector<CecButtonPress>::reverse_iterator it =
				m_buttonQueue.rbegin(); it != m_buttonQueue.rend(); ++it)
		{
			if ((*it).iButton == key.iButton)
			{
				if ((*it).iDuration == 0)
				{
					// replace this entry
					(*it).iDuration = key.iDuration;
					return;
				}
				// add a new entry
				break;
			}
		}
	}

	m_buttonQueue.push_back(key);
}

int CPeripheralCecFrameworkAdapter::GetButton(void)
{
	CSingleLock lock(m_critSection);
	if (!m_bHasButton)
		GetNextKey();

	return m_bHasButton ? m_currentButton.iButton : 0;
}

unsigned int CPeripheralCecFrameworkAdapter::GetHoldTime(void)
{
	CSingleLock lock(m_critSection);
	if (!m_bHasButton)
		GetNextKey();

	return m_bHasButton ? m_currentButton.iDuration : 0;
}

void CPeripheralCecFrameworkAdapter::ResetButton(void)
{
	CSingleLock lock(m_critSection);
	m_bHasButton = false;

// wait for the key release if the duration isn't 0
	if (m_currentButton.iDuration > 0)
	{
		m_currentButton.iButton = 0;
		m_currentButton.iDuration = 0;
	}
}

void CPeripheralCecFrameworkAdapter::OnSettingChanged(
		const std::string &strChangedSetting)
{
	if (StringUtils::EqualsNoCase(strChangedSetting, "enabled"))
	{
		bool bEnabled(GetSettingBool("enabled"));
		if (!bEnabled && IsRunning())
		{
			CLog::Log(LOGDEBUG, "%s - closing the CEC connection", __FUNCTION__);
			StopThread(true);
		}
		else if (bEnabled && !IsRunning())
		{
			CLog::Log(LOGDEBUG, "%s - starting the CEC connection", __FUNCTION__);
			InitialiseFeature(FEATURE_CEC_FRAMEWORK);
		}
	}
	else
	{
		CLog::Log(LOGDEBUG, "%s - restarting the CEC connection", __FUNCTION__);
		InitialiseFeature(FEATURE_CEC_FRAMEWORK);
	}
}

bool CPeripheralCecFrameworkAdapter::IsRunning(void) const
{
	CSingleLock lock(m_critSection);
	return m_bIsRunning;
}

void CPeripheralCecFrameworkAdapter::OnDeviceRemoved(void)
{
	CSingleLock lock(m_critSection);
	m_bDeviceRemoved = true;
}

bool CPeripheralCecFrameworkAdapter::ReopenConnection(bool bAsync /* = false */)
{
	if (bAsync)
	{
		CJobManager::GetInstance().AddJob(new CPeripheralCecFrameworkAdapterReopenJob(this), nullptr, CJob::PRIORITY_NORMAL);
		return true;
	}

// stop running thread
	{
		CSingleLock lock(m_critSection);
		m_iExitCode = EXITCODE_RESTARTAPP;
		CAnnouncementManager::GetInstance().RemoveAnnouncer(this);
		StopThread(false);
	}
	StopThread();

// reset all members to their defaults
	ResetMembers();

// reopen the connection
	return InitialiseFeature(FEATURE_CEC_FRAMEWORK);
}

void CPeripheralCecFrameworkAdapter::ActivateSource(void)
{
	CSingleLock lock(m_critSection);
	m_bActiveSourcePending = true;
}

void CPeripheralCecFrameworkAdapter::ProcessActivateSource(void)
{
	bool bActivate(false);

	{
		CSingleLock lock(m_critSection);
		bActivate = m_bActiveSourcePending;
		m_bActiveSourcePending = false;
	}

	if (bActivate)
		SetActiveSource();
}

void CPeripheralCecFrameworkAdapter::ProcessStandbyDevices(void)
{
	bool bStandby(false);

	{
		CSingleLock lock(m_critSection);
		bStandby = m_bStandbyPending;
		m_bStandbyPending = false;
		if (bStandby)
			m_bGoingToStandby = true;
	}

	if (bStandby)
	{
		if (true) // TODO: configuration option
		{
			m_standbySent = CDateTime::GetCurrentDateTime();
			StandbyDevices(CEC_LOG_ADDR_BROADCAST);
		}
		else if (m_bSendInactiveSource == 1)
		{
			CLog::Log(LOGDEBUG, "%s - sending inactive source commands", __FUNCTION__);
			SetInactiveView();
		}
	}
}

bool CPeripheralCecFrameworkAdapter::ToggleDeviceState(CecStateChange mode, bool forceType)
{
	if (!IsRunning())
		return false;
	if (IsActiveSource()
			&& (mode == STATE_SWITCH_TOGGLE || mode == STATE_STANDBY))
	{
		CLog::Log(LOGDEBUG, "%s - putting CEC device on standby...", __FUNCTION__);
		StandbyDevices();
		return false;
	}
	else if (mode == STATE_SWITCH_TOGGLE || mode == STATE_ACTIVATE_SOURCE)
	{
		CLog::Log(LOGDEBUG, "%s - waking up CEC device...", __FUNCTION__);
		ActivateSource();
		return true;
	}

	return false;
}

const char *CPeripheralCecFrameworkAdapter::la2s(unsigned la)
{
	switch (la & 0xf)
	{
	case 0:
		return "TV";
	case 1:
		return "Recording Device 1";
	case 2:
		return "Recording Device 2";
	case 3:
		return "Tuner 1";
	case 4:
		return "Playback Device 1";
	case 5:
		return "Audio System";
	case 6:
		return "Tuner 2";
	case 7:
		return "Tuner 3";
	case 8:
		return "Playback Device 2";
	case 9:
		return "Playback Device 3";
	case 10:
		return "Tuner 4";
	case 11:
		return "Playback Device 3";
	case 12:
		return "Reserved 1";
	case 13:
		return "Reserved 2";
	case 14:
		return "Specific";
	case 15:
	default:
		return "Unregistered";
	}
}

void CPeripheralCecFrameworkAdapter::sadEncode(const ShortAudioDesc *sad,
		__u32 *descriptor)
{
	__u8 b1, b2, b3 = 0;

	b1 = (sad->num_channels - 1) & 0x07;
	b1 |= (sad->format_code & 0x0f) << 3;

	b2 = sad->sample_freq_mask;

	switch (sad->format_code)
	{
	case SAD_FMT_CODE_LPCM:
		b3 = sad->bit_depth_mask & 0x07;
		break;
	case 2:
	case 3:
	case 4:
	case 5:
	case 6:
	case 7:
	case 8:
		b3 = sad->max_bitrate;
		break;
	case 9:
	case 10:
	case 11:
	case 12:
	case 13:
		b3 = sad->format_dependent;
		break;
	case SAD_FMT_CODE_WMA_PRO:
		b3 = sad->wma_profile & 0x03;
		break;
	case SAD_FMT_CODE_EXTENDED:
		b3 = (sad->extension_type_code & 0x1f) << 3;

		switch (sad->extension_type_code)
		{
		case 4:
		case 5:
		case 6:
			b3 |= (sad->frame_length_mask & 0x03) << 1;
			break;
		case 8:
		case 10:
			b3 |= (sad->frame_length_mask & 0x03) << 1;
			b3 |= sad->mps & 1;
			break;
		case SAD_EXT_TYPE_MPEG_H_3D_AUDIO:
		case SAD_EXT_TYPE_AC_4:
			b3 |= sad->format_dependent & 0x07;
		case SAD_EXT_TYPE_LPCM_3D_AUDIO:
			b3 |= sad->bit_depth_mask & 0x07;
			break;
		}
		break;
	}

	*descriptor = (b1 << 16) | (b2 << 8) | b3;
}

std::string CPeripheralCecFrameworkAdapter::audioFormatCode2s(__u8 format_code)
{
	switch (format_code)
	{
	case 0:
		return "Reserved";
	case SAD_FMT_CODE_LPCM:
		return "L-PCM";
	case SAD_FMT_CODE_AC3:
		return "AC-3";
	case SAD_FMT_CODE_MPEG1:
		return "MPEG-1";
	case SAD_FMT_CODE_MP3:
		return "MP3";
	case SAD_FMT_CODE_MPEG2:
		return "MPEG2";
	case SAD_FMT_CODE_AAC_LC:
		return "AAC LC";
	case SAD_FMT_CODE_DTS:
		return "DTS";
	case SAD_FMT_CODE_ATRAC:
		return "ATRAC";
	case SAD_FMT_CODE_ONE_BIT_AUDIO:
		return "One Bit Audio";
	case SAD_FMT_CODE_ENHANCED_AC3:
		return "Enhanced AC-3";
	case SAD_FMT_CODE_DTS_HD:
		return "DTS-HD";
	case SAD_FMT_CODE_MAT:
		return "MAT";
	case SAD_FMT_CODE_DST:
		return "DST";
	case SAD_FMT_CODE_WMA_PRO:
		return "WMA Pro";
	case SAD_FMT_CODE_EXTENDED:
		return "Extended";
	default:
		return "Illegal";
	};
	return "";
}

std::string CPeripheralCecFrameworkAdapter::extensionTypeCode2s(__u8 type_code)
{
	switch (type_code)
	{
	case 0:
	case 1:
	case 2:
	case 3:
		return "Not in use";
	case SAD_EXT_TYPE_MPEG4_HE_AAC:
		return "MPEG-4 HE AAC";
	case SAD_EXT_TYPE_MPEG4_HE_AACv2:
		return "MPEG-4 HE AAC v2";
	case SAD_EXT_TYPE_MPEG4_AAC_LC:
		return "MPEG-4 AAC LC";
	case SAD_EXT_TYPE_DRA:
		return "DRA";
	case SAD_EXT_TYPE_MPEG4_HE_AAC_SURROUND:
		return "MPEG-4 HE AAC + MPEG Surround";
	case SAD_EXT_TYPE_MPEG4_AAC_LC_SURROUND:
		return "MPEG-4 AAC LC + MPEG Surround";
	case SAD_EXT_TYPE_MPEG_H_3D_AUDIO:
		return "MPEG-H 3D Audio";
	case SAD_EXT_TYPE_AC_4:
		return "AC-4";
	case SAD_EXT_TYPE_LPCM_3D_AUDIO:
		return "L-PCM 3D Audio";
	default:
		return "Reserved";
	}
}

std::string CPeripheralCecFrameworkAdapter::audioFormatIdCode2s(
		__u8 audio_format_id, __u8 audio_format_code)
{
	if (audio_format_id == 0)
		return audioFormatCode2s(audio_format_code);
	else if (audio_format_id == 1)
		return extensionTypeCode2s(audio_format_code);
	return "Invalid";
}

std::string CPeripheralCecFrameworkAdapter::opcode2s(const struct cec_msg *msg)
{
	std::stringstream oss;
	__u8 opcode = msg->msg[1];

	if (opcode == CEC_MSG_CDC_MESSAGE)
	{
		__u8 cdc_opcode = msg->msg[4];

		for (unsigned i = 0; i < ARRAY_SIZE(cdcmsgtable); i++)
		{
			if (cdcmsgtable[i].opcode == cdc_opcode)
				return cdcmsgtable[i].name;
		}
		oss << "CDC: 0x" << std::hex << (unsigned) cdc_opcode;
		return oss.str();
	}

	for (unsigned i = 0; i < ARRAY_SIZE(msgtable); i++)
	{
		if (msgtable[i].opcode == opcode)
			return msgtable[i].name;
	}
	oss << "0x" << std::hex << (unsigned) opcode;
	return oss.str();
}

int CPeripheralCecFrameworkAdapter::cecNamedIoctl(int fd, const char *name,
		unsigned long int request, void *parm)
{
	int retval = ioctl(fd, request, parm);
	int e;

	e = retval == 0 ? 0 : errno;
	CLog::Log(LOGDEBUG, "%s - %s returned %d (%s)", __FUNCTION__, name, retval, strerror(e));

	return retval == -1 ? e : (retval ? -1 : 0);
}

unsigned CPeripheralCecFrameworkAdapter::tsToMs(__u64 ts)
{
	return ts / 1000000;
}

unsigned CPeripheralCecFrameworkAdapter::tsToS(__u64 ts)
{
	return ts / 1000000000;
}

__u64 CPeripheralCecFrameworkAdapter::getTs()
{
	struct timespec timespec;

	clock_gettime(CLOCK_MONOTONIC, &timespec);
	return timespec.tv_sec * 1000000000ull + timespec.tv_nsec;
}

const char *CPeripheralCecFrameworkAdapter::getUiCmdString(__u8 ui_cmd)
{
	for (unsigned i = 0; i < ARRAY_SIZE(type_ui_cmd); i++)
	{
		if (type_ui_cmd[i].value == ui_cmd)
			return type_ui_cmd[i].type_name;
	}
	return "Unknown";
}

void CPeripheralCecFrameworkAdapter::logEvent(struct cec_event &ev)
{
	__u16 pa;

	if (ev.flags & CEC_EVENT_FL_INITIAL_STATE)
		CLog::Log(LOGDEBUG, "%s - Event: initial", __FUNCTION__);
	switch (ev.event)
	{
	case CEC_EVENT_STATE_CHANGE:
		pa = ev.state_change.phys_addr;
		CLog::Log(LOGDEBUG, "%s - Event: State Change: PA: %x.%x.%x.%x, LA mask: 0x%04x", __FUNCTION__, pa >> 12, (pa >> 8) & 0xf, (pa >> 4) & 0xf, pa & 0xf, ev.state_change.log_addr_mask);
		break;
	case CEC_EVENT_LOST_MSGS:
		CLog::Log(LOGDEBUG, "%s - Event: Lost Messages", __FUNCTION__);
		break;
	default:
		CLog::Log(LOGDEBUG, "%s - Event: Unknown (0x%x)", __FUNCTION__, ev.event);
		break;
	}
	CLog::Log(LOGDEBUG, "%s - Event: Timestamp: %llu.%03llus", __FUNCTION__, ev.ts / 1000000000, (ev.ts % 1000000000) / 1000000);
}

void CPeripheralCecFrameworkAdapter::replyFeatureAbort(struct cec_msg *msg,
		__u8 reason)
{
	unsigned la = cec_msg_initiator(msg);
	__u8 opcode = cec_msg_opcode(msg);
	__u64 ts_now = getTs();

	if (cec_msg_is_broadcast(msg))
		return;
	if (reason == CEC_OP_ABORT_UNRECOGNIZED_OP)
	{
		m_laInfo[la].feature_aborted[opcode].count++;
		if (m_laInfo[la].feature_aborted[opcode].count == 2)
		{
			/* If the Abort Reason was "Unrecognized opcode", the Initiator should not send
			 the same message to the same Follower again at that time to avoid saturating
			 the bus. */
			CLog::Log(LOGWARNING, "%s - Received message %s from LA %d (%s) shortly after replying Feature Abort [Unrecognized Opcode] to the same message.", __FUNCTION__, opcode2s(msg).c_str(), la, la2s(la));
		}
	}
	else if (m_laInfo[la].feature_aborted[opcode].count)
	{
			CLog::Log(LOGWARNING, "%s - Replying Feature Abort with abort reason different than [Unrecognized Opcode] to message that has previously been replied Feature Abort to with [Unrecognized Opcode]", __FUNCTION__);
	}
	else
		m_laInfo[la].feature_aborted[opcode].ts = ts_now;

	cec_msg_reply_feature_abort(msg, reason);
	transmit(m_node, msg);
}

bool CPeripheralCecFrameworkAdapter::exitStandby()
{
	if (m_node->state.power_status == CEC_OP_POWER_STATUS_STANDBY
			|| m_node->state.power_status == CEC_OP_POWER_STATUS_TO_STANDBY)
	{
		m_node->state.old_power_status = m_node->state.power_status;
		m_node->state.power_status = CEC_OP_POWER_STATUS_ON;
		m_node->state.power_status_changed_time = time(NULL);
		CLog::Log(LOGDEBUG, "%s - Changing state to on", __FUNCTION__);
		return true;
	}
	return false;
}

bool CPeripheralCecFrameworkAdapter::enterStandby()
{
	if (m_node->state.power_status == CEC_OP_POWER_STATUS_ON
			|| m_node->state.power_status == CEC_OP_POWER_STATUS_TO_ON)
	{
		m_node->state.old_power_status = m_node->state.power_status;
		m_node->state.power_status = CEC_OP_POWER_STATUS_STANDBY;
		m_node->state.power_status_changed_time = time(NULL);
		CLog::Log(LOGDEBUG, "%s - Changing state to standby", __FUNCTION__);
		return true;
	}
	return false;
}

unsigned CPeripheralCecFrameworkAdapter::getDurationMs(__u64 ts_a, __u64 ts_b)
{
	return (ts_a - ts_b) / 1000000;
}

void CPeripheralCecFrameworkAdapter::rcPressHoldStop(const PState state)
{
	unsigned mean_duration = state->rc_duration_sum / state->rc_press_hold_count;

	CLog::Log(LOGDEBUG, "%s - Stop Press and Hold. Mean duration between User Control Pressed messages: %dms", __FUNCTION__, mean_duration);
	if (mean_duration < MIN_INITIATOR_REP_TIME)
	{
		CLog::Log(LOGWARNING, "%s - The mean duration between User Control Pressed messages is lower than the Minimum Initiator Repetition Time (200ms).", __FUNCTION__);
	}
	if (mean_duration > MAX_INITIATOR_REP_TIME)
	{
		CLog::Log(LOGWARNING, "%s - The mean duration between User Control Pressed messages is higher than the Maximum Initiator Repetition Time (500ms).", __FUNCTION__);
	}
}

__u16 CPeripheralCecFrameworkAdapter::paCommonMask(__u16 pa1, __u16 pa2)
{
	__u16 mask = 0xf000;

	for (int i = 0; i < 3; i++)
	{
		if ((pa1 & mask) != (pa2 & mask))
			break;
		mask = (mask >> 4) | 0xf000;
	}
	return mask << 4;
}

bool CPeripheralCecFrameworkAdapter::paAreAdjacent(__u16 pa1, __u16 pa2)
{
	const __u16 mask = paCommonMask(pa1, pa2);
	const __u16 trail_mask = ((~mask) & 0xffff) >> 4;

	if (pa1 == CEC_PHYS_ADDR_INVALID || pa2 == CEC_PHYS_ADDR_INVALID
			|| pa1 == pa2)
		return false;
	if ((pa1 & trail_mask) || (pa2 & trail_mask))
		return false;
	if (!((pa1 & ~mask) && (pa2 & ~mask)))
		return true;
	return false;
}

bool CPeripheralCecFrameworkAdapter::paIsUpstreamFrom(__u16 pa1, __u16 pa2)
{
	const __u16 mask = paCommonMask(pa1, pa2);

	if (pa1 == CEC_PHYS_ADDR_INVALID || pa2 == CEC_PHYS_ADDR_INVALID)
		return false;
	if (!(pa1 & ~mask) && (pa2 & ~mask))
		return true;
	return false;
}

void CPeripheralCecFrameworkAdapter::processMsg(struct cec_msg &msg,
		unsigned me)
{
	__u8 to = cec_msg_destination(&msg);
	__u8 from = cec_msg_initiator(&msg);
	bool is_bcast = cec_msg_is_broadcast(&msg);
	__u16 remote_pa =
			(from < 15) ? m_node->remote_phys_addr[from] : CEC_PHYS_ADDR_INVALID;
	const time_t time_to_transient = 4;
	const time_t time_to_stable = 8;

	switch (msg.msg[1])
	{

	/* OSD Display */

	case CEC_MSG_SET_OSD_STRING:
	{
		__u8 disp_ctl;
		char osd[14];

		if (to != 0 && to != 14)
			break;
		if (!m_node->has_osd_string)
			break;

		cec_ops_set_osd_string(&msg, &disp_ctl, osd);
		switch (disp_ctl)
		{
		case CEC_OP_DISP_CTL_DEFAULT:
		case CEC_OP_DISP_CTL_UNTIL_CLEARED:
		case CEC_OP_DISP_CTL_CLEAR:
			return;
		}
		cec_msg_reply_feature_abort(&msg, CEC_OP_ABORT_INVALID_OP);
		transmit(m_node, &msg);
		return;
	}

		/* Give Device Power Status */

	case CEC_MSG_GIVE_DEVICE_POWER_STATUS:
	{
		__u8 report_status;

		if (time(NULL) - m_node->state.power_status_changed_time
				<= time_to_transient)
			report_status = m_node->state.old_power_status;
		else if (time(NULL) - m_node->state.power_status_changed_time
				>= time_to_stable)
			report_status = m_node->state.power_status;
		else if (m_node->state.power_status == CEC_OP_POWER_STATUS_ON)
			report_status = CEC_OP_POWER_STATUS_TO_ON;
		else
			report_status = CEC_OP_POWER_STATUS_TO_STANDBY;

		cec_msg_set_reply_to(&msg, &msg);
		cec_msg_report_power_status(&msg, report_status);
		transmit(m_node, &msg);
		return;
	}
	case CEC_MSG_REPORT_POWER_STATUS:
		// Nothing to do here for now.
		return;

		/* Standby */

	case CEC_MSG_STANDBY:
		if (m_node->state.power_status == CEC_OP_POWER_STATUS_ON
				|| m_node->state.power_status == CEC_OP_POWER_STATUS_TO_ON)
		{
			m_node->state.old_power_status = m_node->state.power_status;
			m_node->state.power_status = CEC_OP_POWER_STATUS_STANDBY;
			m_node->state.power_status_changed_time = time(NULL);
			CLog::Log(LOGDEBUG, "%s - Changing state to standby", __FUNCTION__);
		}
		return;

		/* One Touch Play and Routing Control */

	case CEC_MSG_ACTIVE_SOURCE:
	{
		__u16 phys_addr;

		cec_ops_active_source(&msg, &phys_addr);
		m_node->state.active_source_pa = phys_addr;
		CLog::Log(LOGDEBUG, "%s - New active source: %x.%x.%x.%x", __FUNCTION__, cec_phys_addr_exp(phys_addr));
		return;
	}
	case CEC_MSG_IMAGE_VIEW_ON:
	case CEC_MSG_TEXT_VIEW_ON:
		if (!cec_has_tv(1 << me))
			break;
		exitStandby();
		return;
	case CEC_MSG_SET_STREAM_PATH:
	{
		__u16 phys_addr;

		cec_ops_set_stream_path(&msg, &phys_addr);
		if (phys_addr != m_node->phys_addr)
			return;

		if (!cec_has_tv(1 << me))
			exitStandby();

		cec_msg_init(&msg, me, CEC_LOG_ADDR_BROADCAST);
		cec_msg_active_source(&msg, m_node->phys_addr);
		transmit(m_node, &msg);
		CLog::Log(LOGDEBUG, "%s - Stream Path is directed to this device", __FUNCTION__);
		return;
	}
	case CEC_MSG_ROUTING_INFORMATION:
	{
		__u8 la = cec_msg_initiator(&msg);

		if (cec_has_tv(1 << la) && m_laInfo[la].phys_addr == 0)
			CLog::Log(LOGDEBUG, "%s - TV (0) at 0.0.0.0 sent Routing Information.", __FUNCTION__);
	}

		/* System Information */

	case CEC_MSG_GET_MENU_LANGUAGE:
		if (!cec_has_tv(1 << me))
			break;
		cec_msg_set_reply_to(&msg, &msg);
		cec_msg_set_menu_language(&msg, m_node->state.menu_language);
		transmit(m_node, &msg);
		return;
	case CEC_MSG_CEC_VERSION:
		return;
	case CEC_MSG_REPORT_PHYSICAL_ADDR:
	{
		__u16 phys_addr;
		__u8 prim_dev_type;

		cec_ops_report_physical_addr(&msg, &phys_addr, &prim_dev_type);
		if (from < 15)
			m_node->remote_phys_addr[from] = phys_addr;
		return;
	}

		/* Remote Control Passthrough
		 (System Audio Control, Device Menu Control) */

	case CEC_MSG_USER_CONTROL_PRESSED:
	{
		struct cec_op_ui_command rc_press;
		unsigned new_state;
		unsigned duration;

		cec_ops_user_control_pressed(&msg, &rc_press);
		duration = getDurationMs(msg.rx_ts, m_node->state.rc_press_rx_ts);

		new_state = PRESS;
		if (m_node->state.rc_state == NOPRESS)
			CLog::Log(LOGDEBUG, "%s - Button press: %s", __FUNCTION__, getUiCmdString(rc_press.ui_cmd));
		else if (rc_press.ui_cmd != m_node->state.rc_ui_cmd)
		{
			/* We have not yet received User Control Released, but have received
			 another User Control Pressed with a different UI Command. */
			if (m_node->state.rc_state == PRESS_HOLD)
				rcPressHoldStop(&m_node->state);
			CLog::Log(LOGDEBUG, "%s - Button press (no User Control Released between): %s", __FUNCTION__, getUiCmdString(rc_press.ui_cmd));

			/* The device shall send User Control Released if the time between
			 two messages is longer than the maximum Initiator Repetition Time. */
			if (duration > MAX_INITIATOR_REP_TIME)
				CLog::Log(LOGWARNING, "%s - Device waited more than the maximum Initiatior Repetition Time and should have sent a User Control Released message.", __FUNCTION__);
		}
		else if (duration < FOLLOWER_SAFETY_TIMEOUT)
		{
			/* We have not yet received a User Control Released, but received
			 another User Control Pressed, with the same UI Command as the
			 previous, which means that the Press and Hold behavior should
			 be invoked. */
			new_state = PRESS_HOLD;
			if (m_node->state.rc_state != PRESS_HOLD)
			{
				CLog::Log(LOGDEBUG, "%s - Start Press and Hold with button %s", __FUNCTION__, getUiCmdString(rc_press.ui_cmd));
				m_node->state.rc_duration_sum = 0;
				m_node->state.rc_press_hold_count = 0;
			}
			m_node->state.rc_duration_sum += duration;
			m_node->state.rc_press_hold_count++;
		}

		m_node->state.rc_state = new_state;
		m_node->state.rc_ui_cmd = rc_press.ui_cmd;
		m_node->state.rc_press_rx_ts = msg.rx_ts;

		switch (rc_press.ui_cmd)
		{
		case 0x41:
			if (m_node->state.volume < VOLUME_MAX)
				m_node->state.volume++;
			break;
		case 0x42:
			if (m_node->state.volume > VOLUME_MIN)
				m_node->state.volume--;
			break;
		case 0x43:
			m_node->state.mute = !m_node->state.mute;
			break;
		case 0x6B:
			if (!enterStandby())
				exitStandby();
			break;
		case 0x6C:
			enterStandby();
			break;
		case 0x6D:
			exitStandby();
			break;
		}
		if (rc_press.ui_cmd >= 0x41 && rc_press.ui_cmd <= 0x43)
		{
			CLog::Log(LOGDEBUG, "%s - Volume: %d%s", __FUNCTION__, m_node->state.volume, m_node->state.mute ? ", muted" : "");
			cec_msg_set_reply_to(&msg, &msg);
			cec_msg_report_audio_status(&msg, m_node->state.mute ? 1 : 0,
					m_node->state.volume);
			transmit(m_node, &msg);
		}

		return;
	}
	case CEC_MSG_USER_CONTROL_RELEASED:
		if (m_node->state.rc_state == PRESS_HOLD)
			rcPressHoldStop(&m_node->state);

		if (m_node->state.rc_state == NOPRESS)
			CLog::Log(LOGDEBUG, "%s - Button release (unexpected, %ums after last press)", __FUNCTION__, tsToMs(msg.rx_ts - m_node->state.rc_press_rx_ts));
		else if (getDurationMs(msg.rx_ts, m_node->state.rc_press_rx_ts) > FOLLOWER_SAFETY_TIMEOUT)
			CLog::Log(LOGDEBUG, "%s - Button release: %s (after timeout, %ums after last press)", __FUNCTION__, getUiCmdString(m_node->state.rc_ui_cmd), tsToMs(msg.rx_ts - m_node->state.rc_press_rx_ts));
		else
			CLog::Log(LOGDEBUG, "%s - Button release: %s (%ums after last press)", __FUNCTION__, getUiCmdString(m_node->state.rc_ui_cmd), tsToMs(msg.rx_ts - m_node->state.rc_press_rx_ts));
		m_node->state.rc_state = NOPRESS;
		return;

		/* Device Menu Control */

	case CEC_MSG_MENU_REQUEST:
	{
		if (m_node->cec_version < CEC_OP_CEC_VERSION_2_0 && !cec_has_tv(1 << me))
		{
			cec_msg_set_reply_to(&msg, &msg);
			cec_msg_menu_status(&msg, CEC_OP_MENU_STATE_ACTIVATED);
			transmit(m_node, &msg);
			return;
		}
		break;
	}
	case CEC_MSG_MENU_STATUS:
		if (m_node->cec_version < CEC_OP_CEC_VERSION_2_0 && cec_has_tv(1 << me))
			return;
		break;

		/*
		 Deck Control

		 This is only a basic implementation.

		 TODO: Device state should reflect whether we are playing,
		 fast forwarding, etc.
		 */

	case CEC_MSG_GIVE_DECK_STATUS:
		if (m_node->has_deck_ctl)
		{
			cec_msg_set_reply_to(&msg, &msg);
			cec_msg_deck_status(&msg, CEC_OP_DECK_INFO_STOP);
			transmit(m_node, &msg);
			return;
		}
		break;
	case CEC_MSG_PLAY:
		if (m_node->has_deck_ctl)
			return;
		break;
	case CEC_MSG_DECK_CONTROL:
		if (m_node->has_deck_ctl)
			return;
		break;
	case CEC_MSG_DECK_STATUS:
		return;

		/*
		 Tuner Control

		 This is only a basic implementation.

		 TODO: Device state should change when selecting services etc.
		 */

	case CEC_MSG_GIVE_TUNER_DEVICE_STATUS:
	{
		if (!cec_has_tuner(1 << me))
			break;

		struct cec_op_tuner_device_info tuner_dev_info =
		{ };

		cec_msg_set_reply_to(&msg, &msg);
		tuner_dev_info.rec_flag = CEC_OP_REC_FLAG_NOT_USED;
		tuner_dev_info.tuner_display_info = CEC_OP_TUNER_DISPLAY_INFO_NONE;
		tuner_dev_info.is_analog = false;
		tuner_dev_info.digital.service_id_method =
		CEC_OP_SERVICE_ID_METHOD_BY_CHANNEL;
		tuner_dev_info.digital.dig_bcast_system =
		CEC_OP_DIG_SERVICE_BCAST_SYSTEM_DVB_C;
		tuner_dev_info.digital.channel.channel_number_fmt =
		CEC_OP_CHANNEL_NUMBER_FMT_1_PART;
		tuner_dev_info.digital.channel.minor = 1;

		cec_msg_tuner_device_status(&msg, &tuner_dev_info);
		transmit(m_node, &msg);
		return;
	}

	case CEC_MSG_TUNER_DEVICE_STATUS:
		return;

	case CEC_MSG_SELECT_ANALOGUE_SERVICE:
	case CEC_MSG_SELECT_DIGITAL_SERVICE:
	case CEC_MSG_TUNER_STEP_DECREMENT:
	case CEC_MSG_TUNER_STEP_INCREMENT:
		if (!cec_has_tuner(1 << me))
			break;
		return;

		/*
		 One Touch Record

		 This is only a basic implementation.

		 TODO:
		 - If we are a TV, we should only send Record On if the
		 remote end is a Recording device or Reserved. Otherwise ignore.

		 - Device state should reflect whether we are recording, etc. In
		 recording mode we should ignore Standby messages.
		 */

	case CEC_MSG_RECORD_TV_SCREEN:
	{
		if (!m_node->has_rec_tv)
			break;

		struct cec_op_record_src rec_src =
		{ };

		rec_src.type = CEC_OP_RECORD_SRC_OWN;
		cec_msg_set_reply_to(&msg, &msg);
		cec_msg_record_on(&msg, false, &rec_src);
		transmit(m_node, &msg);
		return;
	}
	case CEC_MSG_RECORD_ON:
		if (!cec_has_record(1 << me))
			break;
		cec_msg_set_reply_to(&msg, &msg);
		cec_msg_record_status(&msg, CEC_OP_RECORD_STATUS_CUR_SRC);
		transmit(m_node, &msg);
		return;
	case CEC_MSG_RECORD_OFF:
		if (!cec_has_record(1 << me))
			break;
		cec_msg_set_reply_to(&msg, &msg);
		cec_msg_record_status(&msg, CEC_OP_RECORD_STATUS_TERMINATED_OK);
		transmit(m_node, &msg);
		return;
	case CEC_MSG_RECORD_STATUS:
		return;

		/*
		 Timer Programming

		 This is only a basic implementation.

		 TODO/Ideas:
		 - Act like an actual recording device; keep track of recording
		 schedule and act correctly when colliding timers are set.
		 - Emulate a finite storage space for recordings
		 */

	case CEC_MSG_SET_ANALOGUE_TIMER:
	case CEC_MSG_SET_DIGITAL_TIMER:
	case CEC_MSG_SET_EXT_TIMER:
		if (!cec_has_record(1 << me))
			break;
		cec_msg_set_reply_to(&msg, &msg);
		cec_msg_timer_status(&msg, CEC_OP_TIMER_OVERLAP_WARNING_NO_OVERLAP,
		CEC_OP_MEDIA_INFO_NO_MEDIA, CEC_OP_PROG_INFO_ENOUGH_SPACE, 0, 0, 0);
		transmit(m_node, &msg);
		return;
	case CEC_MSG_CLEAR_ANALOGUE_TIMER:
	case CEC_MSG_CLEAR_DIGITAL_TIMER:
	case CEC_MSG_CLEAR_EXT_TIMER:
		if (!cec_has_record(1 << me))
			break;
		cec_msg_set_reply_to(&msg, &msg);
		cec_msg_timer_cleared_status(&msg, CEC_OP_TIMER_CLR_STAT_CLEARED);
		transmit(m_node, &msg);
		return;
	case CEC_MSG_SET_TIMER_PROGRAM_TITLE:
		if (!cec_has_record(1 << me))
			break;
		return;
	case CEC_MSG_TIMER_CLEARED_STATUS:
	case CEC_MSG_TIMER_STATUS:
		return;

		/* Dynamic Auto Lipsync */

	case CEC_MSG_REQUEST_CURRENT_LATENCY:
	{
		__u16 phys_addr;

		cec_ops_request_current_latency(&msg, &phys_addr);
		if (phys_addr == m_node->phys_addr)
		{
			cec_msg_set_reply_to(&msg, &msg);
			cec_msg_report_current_latency(&msg, phys_addr,
					m_node->state.video_latency, m_node->state.low_latency_mode,
					m_node->state.audio_out_compensated, m_node->state.audio_out_delay);
			transmit(m_node, &msg);
		}
		return;
	}

		/* Audio Return Channel Control */

	case CEC_MSG_INITIATE_ARC:
		if (m_node->has_arc_tx)
		{
			if (!paIsUpstreamFrom(m_node->phys_addr, remote_pa)
					|| !paAreAdjacent(m_node->phys_addr, remote_pa))
			{
				cec_msg_reply_feature_abort(&msg, CEC_OP_ABORT_REFUSED);
				transmit(m_node, &msg);
				return;
			}
			cec_msg_set_reply_to(&msg, &msg);
			cec_msg_report_arc_initiated(&msg);
			transmit(m_node, &msg);
			m_node->state.arc_active = true;
			CLog::Log(LOGDEBUG, "%s - ARC is initiated", __FUNCTION__);
			return;
		}
		break;
	case CEC_MSG_TERMINATE_ARC:
		if (m_node->has_arc_tx)
		{
			if (!paIsUpstreamFrom(m_node->phys_addr, remote_pa)
					|| !paAreAdjacent(m_node->phys_addr, remote_pa))
			{
				cec_msg_reply_feature_abort(&msg, CEC_OP_ABORT_REFUSED);
				transmit(m_node, &msg);
				return;
			}
			cec_msg_set_reply_to(&msg, &msg);
			cec_msg_report_arc_terminated(&msg);
			transmit(m_node, &msg);
			m_node->state.arc_active = false;
			dev_info("ARC is terminated\n");
			return;
		}
		break;
	case CEC_MSG_REQUEST_ARC_INITIATION:
		if (m_node->has_arc_rx)
		{
			if (paIsUpstreamFrom(m_node->phys_addr, remote_pa)
					|| !paAreAdjacent(m_node->phys_addr, remote_pa))
			{
				cec_msg_reply_feature_abort(&msg, CEC_OP_ABORT_REFUSED);
				transmit(m_node, &msg);
				return;
			}
			cec_msg_set_reply_to(&msg, &msg);
			cec_msg_initiate_arc(&msg, false);
			transmit(m_node, &msg);
			CLog::Log(LOGDEBUG, "%s - ARC initiation has been requested.", __FUNCTION__);
			return;
		}
		break;
	case CEC_MSG_REQUEST_ARC_TERMINATION:
		if (m_node->has_arc_rx)
		{
			if (paIsUpstreamFrom(m_node->phys_addr, remote_pa)
					|| !paAreAdjacent(m_node->phys_addr, remote_pa))
			{
				cec_msg_reply_feature_abort(&msg, CEC_OP_ABORT_REFUSED);
				transmit(m_node, &msg);
				return;
			}
			cec_msg_set_reply_to(&msg, &msg);
			cec_msg_terminate_arc(&msg, false);
			transmit(m_node, &msg);
			CLog::Log(LOGDEBUG, "%s - ARC termination has been requested.", __FUNCTION__);
			return;
		}
		break;
	case CEC_MSG_REPORT_ARC_INITIATED:
		m_node->state.arc_active = true;
		CLog::Log(LOGDEBUG, "%s - ARC is initiated", __FUNCTION__);
		return;
	case CEC_MSG_REPORT_ARC_TERMINATED:
		m_node->state.arc_active = false;
		CLog::Log(LOGDEBUG, "%s - ARC is terminated", __FUNCTION__);
		return;

		/* System Audio Control */

	case CEC_MSG_SYSTEM_AUDIO_MODE_REQUEST:
	{
		if (!cec_has_audiosystem(1 << me))
			break;

		__u16 phys_addr;

		cec_ops_system_audio_mode_request(&msg, &phys_addr);
		cec_msg_init(&msg, me, CEC_LOG_ADDR_BROADCAST);
		if (phys_addr != CEC_PHYS_ADDR_INVALID)
		{
			cec_msg_set_system_audio_mode(&msg, CEC_OP_SYS_AUD_STATUS_ON);
			transmit(m_node, &msg);
			m_node->state.sac_active = true;
		}
		else
		{
			cec_msg_set_system_audio_mode(&msg, CEC_OP_SYS_AUD_STATUS_OFF);
			transmit(m_node, &msg);
			m_node->state.sac_active = false;
		}
		CLog::Log(LOGDEBUG, "%s - System Audio Mode: %s", __FUNCTION__, m_node->state.sac_active ? "on" : "off");
		return;
	}
	case CEC_MSG_SET_SYSTEM_AUDIO_MODE:
	{
		__u8 system_audio_status;

		if (!cec_msg_is_broadcast(&msg))
		{
			/* Directly addressed Set System Audio Mode is used to see
			 if the TV supports the feature. If we time out, we
			 signalize that we support SAC. */
			if (cec_has_tv(1 << me))
				return;
			else
				break;
		}
		cec_ops_set_system_audio_mode(&msg, &system_audio_status);
		if (system_audio_status == CEC_OP_SYS_AUD_STATUS_ON)
			m_node->state.sac_active = true;
		else if (system_audio_status == CEC_OP_SYS_AUD_STATUS_OFF)
			m_node->state.sac_active = false;
		CLog::Log(LOGDEBUG, "%s - System Audio Mode: %s", __FUNCTION__, m_node->state.sac_active ? "on" : "off");
		return;
	}
	case CEC_MSG_REPORT_AUDIO_STATUS:
		return;
	case CEC_MSG_GIVE_SYSTEM_AUDIO_MODE_STATUS:
		if (!cec_has_audiosystem(1 << me))
			break;
		cec_msg_set_reply_to(&msg, &msg);
		cec_msg_system_audio_mode_status(&msg,
				m_node->state.sac_active ?
				CEC_OP_SYS_AUD_STATUS_ON :
																		CEC_OP_SYS_AUD_STATUS_OFF);
		transmit(m_node, &msg);
	case CEC_MSG_GIVE_AUDIO_STATUS:
		if (!cec_has_audiosystem(1 << me))
			break;
		cec_msg_set_reply_to(&msg, &msg);
		cec_msg_report_audio_status(&msg, m_node->state.mute ? 1 : 0,
				m_node->state.volume);
		transmit(m_node, &msg);
		return;
	case CEC_MSG_SYSTEM_AUDIO_MODE_STATUS:
		return;
	case CEC_MSG_REQUEST_SHORT_AUDIO_DESCRIPTOR:
	{
		if (!cec_has_audiosystem(1 << me))
			break;

		/* The list of formats that the follower 'supports' */
		const ShortAudioDesc supported_formats[] =
		{
		{ 2, SAD_FMT_CODE_AC3, SAD_SAMPLE_FREQ_MASK_32 | SAD_SAMPLE_FREQ_MASK_44_1,
				64, 0, 0 },
		{ 4, SAD_FMT_CODE_AC3, SAD_SAMPLE_FREQ_MASK_32, 32, 0, 0 },
		{ 4, SAD_FMT_CODE_ONE_BIT_AUDIO, SAD_SAMPLE_FREQ_MASK_48
				| SAD_SAMPLE_FREQ_MASK_192, 123, 0, 0 },
		{ 8, SAD_FMT_CODE_EXTENDED, SAD_SAMPLE_FREQ_MASK_96, 0, 0,
		SAD_EXT_TYPE_DRA },
		{ 2, SAD_FMT_CODE_EXTENDED, SAD_SAMPLE_FREQ_MASK_176_4,
		SAD_FRAME_LENGTH_MASK_960 | SAD_FRAME_LENGTH_MASK_1024, 1,
		SAD_EXT_TYPE_MPEG4_HE_AAC_SURROUND },
		{ 2, SAD_FMT_CODE_EXTENDED, SAD_SAMPLE_FREQ_MASK_44_1,
		SAD_BIT_DEPTH_MASK_16 | SAD_BIT_DEPTH_MASK_24, 0,
		SAD_EXT_TYPE_LPCM_3D_AUDIO }, };

		__u8 num_descriptors, audio_format_id[4], audio_format_code[4];
		__u32 descriptors[4];
		std::string format_list;

		cec_ops_request_short_audio_descriptor(&msg, &num_descriptors,
				audio_format_id, audio_format_code);
		if (num_descriptors == 0)
		{
			CLog::Log(LOGWARNING, "%s - Got Request Short Audio Descriptor with no operands", __FUNCTION__);
			replyFeatureAbort(&msg, CEC_OP_ABORT_INVALID_OP);
			return;
		}

		unsigned found_descs = 0;

		for (int i = 0; i < num_descriptors; i++)
			format_list += audioFormatIdCode2s(audio_format_id[i],
					audio_format_code[i]) + ",";
		format_list.erase(format_list.end() - 1);
		CLog::Log(LOGDEBUG, "%s - Requested descriptors: %s", __FUNCTION__, format_list.c_str());
		for (unsigned i = 0; i < num_descriptors; i++)
		{
			for (unsigned j = 0; j < ARRAY_SIZE(supported_formats); j++)
			{
				if (found_descs >= 4)
					break;
				if ((audio_format_id[i] == 0
						&& audio_format_code[i] == supported_formats[j].format_code)
						|| (audio_format_id[i] == 1
								&& audio_format_code[i]
										== supported_formats[j].extension_type_code))
					sadEncode(&supported_formats[j], &descriptors[found_descs++]);
			}
		}

		if (found_descs > 0)
		{
			cec_msg_set_reply_to(&msg, &msg);
			cec_msg_report_short_audio_descriptor(&msg, found_descs, descriptors);
			transmit(m_node, &msg);
		}
		else
			replyFeatureAbort(&msg, CEC_OP_ABORT_INVALID_OP);
		return;
	}

		/* Device OSD Transfer */

	case CEC_MSG_SET_OSD_NAME:
		return;

		/*
		 Audio Rate Control

		 This is only a basic implementation.

		 TODO: Set Audio Rate shall be sent at least every 2 seconds by
		 the controlling device. This should be checked and kept track of.
		 */

	case CEC_MSG_SET_AUDIO_RATE:
		if (m_node->has_aud_rate)
			return;
		break;

		/* CDC */

	case CEC_MSG_CDC_MESSAGE:
	{
		switch (msg.msg[4])
		{
		case CEC_MSG_CDC_HEC_DISCOVER:
			__u16 phys_addr;

			cec_msg_set_reply_to(&msg, &msg);
			cec_ops_cdc_hec_discover(&msg, &phys_addr);
			cec_msg_cdc_hec_report_state(&msg, phys_addr,
			CEC_OP_HEC_FUNC_STATE_NOT_SUPPORTED,
			CEC_OP_HOST_FUNC_STATE_NOT_SUPPORTED,
			CEC_OP_ENC_FUNC_STATE_EXT_CON_NOT_SUPPORTED,
			CEC_OP_CDC_ERROR_CODE_NONE, 1, 0); // We do not support HEC on any HDMI connections
			transmit(m_node, &msg);
			return;
		}
	}

		/* Core */

	case CEC_MSG_FEATURE_ABORT:
		return;
	default:
		break;
	}

	if (is_bcast)
		return;

	replyFeatureAbort(&msg);
}

void CPeripheralCecFrameworkAdapter::pollRemoteDevs(unsigned me)
{
	m_node->remote_la_mask = 0;
	for (unsigned i = 0; i < 15; i++)
		m_node->remote_phys_addr[i] = CEC_PHYS_ADDR_INVALID;

	if (!(m_node->caps & CEC_CAP_TRANSMIT))
		return;

	for (unsigned i = 0; i < 15; i++)
	{
		struct cec_msg msg;

		cec_msg_init(&msg, 0xf, i);

		doioctl(m_node, CEC_TRANSMIT, &msg);
		if (msg.tx_status & CEC_TX_STATUS_OK)
		{
			m_node->remote_la_mask |= 1 << i;
			cec_msg_init(&msg, me, i);
			cec_msg_give_physical_addr(&msg, true);
			doioctl(m_node, CEC_TRANSMIT, &msg);
			if (cec_msg_status_is_ok(&msg))
				m_node->remote_phys_addr[i] = (msg.msg[2] << 8) | msg.msg[3];
		}
	}
}

void CPeripheralCecFrameworkAdapter::Process()
{
	struct cec_log_addrs laddrs;
	fd_set rd_fds;
	fd_set ex_fds;
	auto fd = m_node->fd;
	__u32 mode = CEC_MODE_INITIATOR | CEC_MODE_FOLLOWER;
	int me;
	unsigned last_poll_la = 15;

	doioctl(m_node, CEC_S_MODE, &mode);
	doioctl(m_node, CEC_ADAP_G_LOG_ADDRS, &laddrs);
	me = laddrs.log_addr[0];

	pollRemoteDevs(me);

	while (!m_bStop)
	{
		int res;
		struct timeval timeval =
		{ };

		timeval.tv_sec = 1;
		FD_ZERO(&rd_fds);
		FD_ZERO(&ex_fds);
		FD_SET(fd, &rd_fds);
		FD_SET(fd, &ex_fds);
		res = select(fd + 1, &rd_fds, NULL, &ex_fds, &timeval);
		if (res < 0)
			break;
		if (FD_ISSET(fd, &ex_fds))
		{
			struct cec_event ev;

			res = doioctl(m_node, CEC_DQEVENT, &ev);
			if (res == ENODEV)
			{
				CLog::Log(LOGDEBUG, "%s - Device was disconnected.", __FUNCTION__);
				break;
			}
			if (res)
				continue;
			logEvent(ev);
			if (ev.event == CEC_EVENT_STATE_CHANGE)
			{
				CLog::Log(LOGDEBUG, "%s - CEC adapter state change.", __FUNCTION__);
				m_node->phys_addr = ev.state_change.phys_addr;
				doioctl(m_node, CEC_ADAP_G_LOG_ADDRS, &laddrs);
				m_node->adap_la_mask = laddrs.log_addr_mask;
				m_node->state.active_source_pa = CEC_PHYS_ADDR_INVALID;
				me = laddrs.log_addr[0];
				memset(m_laInfo, 0, sizeof(m_laInfo));
			}
		}
		if (FD_ISSET(fd, &rd_fds))
		{
			struct cec_msg msg =
			{ };

			res = doioctl(m_node, CEC_RECEIVE, &msg);
			if (res == ENODEV)
			{
				CLog::Log(LOGDEBUG, "%s - Device was disconnected.", __FUNCTION__);
				break;
			}
			if (res)
				continue;

			__u8 from = cec_msg_initiator(&msg);
			__u8 to = cec_msg_destination(&msg);
			__u8 opcode = cec_msg_opcode(&msg);

			if (from != CEC_LOG_ADDR_UNREGISTERED
					&& m_laInfo[from].feature_aborted[opcode].ts
					&& tsToMs(getTs() - m_laInfo[from].feature_aborted[opcode].ts) < 200)
			{
				CLog::Log(LOGWARNING, "%s - Received message %s from LA %d (%s) less than 200 ms after replying Feature Abort (not [Unrecognized Opcode]) to the same message.", __FUNCTION__, opcode2s(&msg).c_str(), from, la2s(from));

			}
			if (from != CEC_LOG_ADDR_UNREGISTERED && !m_laInfo[from].ts)
				CLog::Log(LOGDEBUG, "%s - Logical address %d (%s) discovered.", __FUNCTION__, from, la2s(from));

			CLog::Log(LOGDEBUG, "%s -     %s to %s (%d to %d): ", __FUNCTION__, la2s(from), to == 0xf ? "all" : la2s(to), from, to);
			CLog::Log(LOGDEBUG, "%s -     Sequence: %u Rx Timestamp: %llu.%03llus", __FUNCTION__, msg.sequence,	msg.rx_ts / 1000000000, (msg.rx_ts % 1000000000) / 1000000);
			if (m_node->adap_la_mask)
				processMsg(msg, me);
		}

		__u64 ts_now = getTs();
		unsigned poll_la = tsToS(ts_now) % 16;

		if (poll_la != last_poll_la&& poll_la < 15 && m_laInfo[poll_la].ts &&
		tsToMs(ts_now - m_laInfo[poll_la].ts) > POLL_PERIOD)
		{
			struct cec_msg msg =
			{ };

			cec_msg_init(&msg, 0xf, poll_la);
			transmit(m_node, &msg);
			if (msg.tx_status & CEC_TX_STATUS_NACK)
			{
				CLog::Log(LOGDEBUG, "%s - Logical address %d stopped responding to polling message.", __FUNCTION__, poll_la);
				memset(&m_laInfo[poll_la], 0, sizeof(m_laInfo[poll_la]));
				m_node->remote_la_mask &= ~(1 << poll_la);
				m_node->remote_phys_addr[poll_la] = CEC_PHYS_ADDR_INVALID;
			}
		}
		last_poll_la = poll_la;

		unsigned ms_since_press = tsToMs(ts_now - m_node->state.rc_press_rx_ts);

		if (ms_since_press > FOLLOWER_SAFETY_TIMEOUT)
		{
			if (m_node->state.rc_state == PRESS_HOLD)
				rcPressHoldStop(&m_node->state);
			else if (m_node->state.rc_state == PRESS)
			{
				CLog::Log(LOGDEBUG, "%s - Button timeout: %s", __FUNCTION__, getUiCmdString(m_node->state.rc_ui_cmd));
				m_node->state.rc_state = NOPRESS;
			}
		}
	}
	mode = CEC_MODE_INITIATOR;
	doioctl(m_node, CEC_S_MODE, &mode);
}

std::string CPeripheralCecFrameworkAdapter::GetDeviceOSDName(unsigned device)
{
	struct cec_msg msg;
	char osd_name[15];

	cec_msg_init(&msg, CEC_LOG_ADDR_BROADCAST, device);
	cec_msg_give_osd_name(&msg, true);
	transmit(m_node, &msg);
	cec_ops_set_osd_name(&msg, osd_name);
	return std::string(osd_name);
}

unsigned CPeripheralCecFrameworkAdapter::GetMyLogicalAddress()
{
	struct cec_log_addrs laddrs;

	doioctl(m_node, CEC_ADAP_G_LOG_ADDRS, &laddrs);
	return laddrs.log_addr[0];
}

void CPeripheralCecFrameworkAdapter::SetActiveSource()
{
	struct cec_msg msg;

	cec_msg_init(&msg, GetMyLogicalAddress(), CEC_LOG_ADDR_BROADCAST);
	cec_msg_active_source(&msg, m_node->phys_addr);
	transmit(m_node, &msg);
}

void CPeripheralCecFrameworkAdapter::PowerOnDevices(unsigned device)
{
	/*
	 *   if (iDestination == CECDEVICE_TV)
	 return TransmitImageViewOn(iInitiator, iDestination);

	 return TransmitKeypress(iInitiator, iDestination, CEC_USER_CONTROL_CODE_POWER) &&
	 TransmitKeyRelease(iInitiator, iDestination);
	 *
	 */
}

void CPeripheralCecFrameworkAdapter::StandbyDevices(unsigned device)
{
}

bool CPeripheralCecFrameworkAdapter::IsActiveSource()
{
	return true;
}

void CPeripheralCecFrameworkAdapter::SetInactiveView()
{
}

} // namespace PERIPHERALS
