#pragma once
/*
 *      Copyright (C) 2017 Gerald Dachs
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

#include "PeripheralHID.h"
#include "interfaces/AnnouncementManager.h"
#include "threads/Thread.h"
#include "threads/CriticalSection.h"
#include <queue>
#include <vector>

#include <linux/cec.h>

using namespace ANNOUNCEMENT;
using namespace XbmcThreads;
using namespace PERIPHERALS;

namespace PERIPHERALS
{
  typedef struct
  {
    int iButton;
    unsigned int iDuration;
  } CecButtonPress;

  typedef enum
  {
    VOLUME_CHANGE_NONE,
    VOLUME_CHANGE_UP,
    VOLUME_CHANGE_DOWN,
    VOLUME_CHANGE_MUTE
  } CecVolumeChange;

  typedef struct _short_audio_desc
  {
  	/* Byte 1 */
  	__u8 num_channels;
  	__u8 format_code;

  	/* Byte 2 */
  	__u8 sample_freq_mask;

  	/* Byte 3 */
  	union
  	{
  		__u8 bit_depth_mask;    // LPCM
  		__u8 max_bitrate;       // Format codes 2-8
  		__u8 format_dependent;  // Format codes 9-13
  		__u8 wma_profile;       // WMA Pro
  		__u8 frame_length_mask; // Extension type codes 4-6, 8-10
  	};
  	__u8 mps;                       // Format codes 8-10
  	__u8 extension_type_code;
  } ShortAudioDesc;

  typedef ShortAudioDesc *PShortAudioDesc;

  typedef struct _state
  {
  	__u16 active_source_pa;
  	__u8 old_power_status;
  	__u8 power_status;
  	time_t power_status_changed_time;
  	char menu_language[4];
  	__u8 video_latency;
  	__u8 low_latency_mode;
  	__u8 audio_out_compensated;
  	__u8 audio_out_delay;
  	bool arc_active;
  	bool sac_active;
  	__u8 volume;
  	bool mute;
  	unsigned rc_state;
  	__u8 rc_ui_cmd;
  	__u64 rc_press_rx_ts;
  	unsigned rc_press_hold_count;
  	unsigned rc_duration_sum;
  } State;

  typedef State *PState;

  typedef struct _node
  {
  	int fd;
  	const char *device;
  	unsigned caps;
  	unsigned available_log_addrs;
  	unsigned adap_la_mask;
  	unsigned remote_la_mask;
  	__u16 remote_phys_addr[15];
  	State state;
  	__u16 phys_addr;
  	__u8 cec_version;
  	bool has_arc_rx;
  	bool has_arc_tx;
  	bool has_aud_rate;
  	bool has_deck_ctl;
  	bool has_rec_tv;
  	bool has_osd_string;
  } Node;

  typedef Node *PNode;

  typedef struct _laInfo
  {
  	__u64 ts;
  	struct
  	{
  		unsigned count;
  		__u64 ts;
  	} feature_aborted[256];
  	__u16 phys_addr;
  } LaInfo;

  class CPeripheralCecFrameworkAdapter : public CPeripheralHID, public ANNOUNCEMENT::IAnnouncer, private CThread
  {
    friend class CPeripheralCecFrameworkAdapterUpdateThread;
    friend class CPeripheralCecFrameworkAdapterReopenJob;

  public:
    CPeripheralCecFrameworkAdapter(CPeripherals& manager, const PeripheralScanResult& scanResult, CPeripheralBus* bus);
    ~CPeripheralCecFrameworkAdapter(void) override;

    void Announce(ANNOUNCEMENT::AnnouncementFlag flag, const char *sender, const char *message, const CVariant &data) override;

    // audio control
    bool HasAudioControl(void);
    void VolumeUp(void);
    void VolumeDown(void);
    void ToggleMute(void);
    bool IsMuted(void);

    // CPeripheral callbacks
    void OnSettingChanged(const std::string &strChangedSetting) override;
    void OnDeviceRemoved(void) override;

    // input
    int GetButton(void);
    unsigned int GetHoldTime(void);
    void ResetButton(void);

    // public CEC methods
    void ActivateSource(void);
    bool ToggleDeviceState(CecStateChange CEC_OP_ABORT_UNRECOGNIZED_OPmode = STATE_SWITCH_TOGGLE, bool forceType = false);

  	void StandbyDevices(unsigned device = CEC_LOG_ADDR_BROADCAST);

  private:
    bool InitialiseFeature(const PeripheralFeature feature) override;
    void ResetMembers(void);
    void Process(void) override;
    bool IsRunning(void) const;

    bool ReopenConnection(bool bAsync = false);

//    static void ReadLogicalAddresses(const std::string &strString, CEC::cec_logical_addresses &addresses);
//    static void ReadLogicalAddresses(int iLocalisedId, CEC::cec_logical_addresses &addresses);
//    bool WriteLogicalAddresses(const CEC::cec_logical_addresses& addresses, const std::string& strSettingName, const std::string& strAdvancedSettingName);

    void ProcessActivateSource(void);
    void ProcessStandbyDevices(void);
    void ProcessVolumeChange(void);

//    void PushCecKeypress(const CEC::cec_keypress &key);
    void PushCecKeypress(const CecButtonPress &key);
    void GetNextKey(void);

    void SetAudioSystemConnected(bool bSetTo);
    void SetMenuLanguage(const char *strLanguage);
    void OnTvStandby(void);

  	const char *la2s(unsigned la);

  	void sadEncode(const ShortAudioDesc *sad, __u32 *descriptor);

  	std::string audioFormatCode2s(__u8 format_code);

  	std::string extensionTypeCode2s(__u8 type_code);

  	std::string audioFormatIdCode2s(__u8 audio_format_id, __u8 audio_format_code);

  	std::string opcode2s(const struct cec_msg *msg);

  	int cecNamedIoctl(int fd, const char *name,
  			    unsigned long int request, void *parm);

  	unsigned tsToMs(__u64 ts);

  	unsigned tsToS(__u64 ts);

  	__u64 getTs();

  	const char *getUiCmdString(__u8 ui_cmd);

  	void logEvent(struct cec_event &ev);

  	void replyFeatureAbort(struct cec_msg *msg,
  			__u8 reason = CEC_OP_ABORT_UNRECOGNIZED_OP);

  	bool exitStandby();

  	bool enterStandby();

  	unsigned getDurationMs(__u64 ts_a, __u64 ts_b);

  	void rcPressHoldStop(const PState state);

  	__u16 paCommonMask(__u16 pa1, __u16 pa2);

  	bool paAreAdjacent(__u16 pa1, __u16 pa2);

  	bool paIsUpstreamFrom(__u16 pa1, __u16 pa2);

  	void processMsg(struct cec_msg &msg, unsigned me);

  	void pollRemoteDevs(unsigned me);

  	unsigned GetMyLogicalAddress();

  	std::string GetDeviceOSDName(unsigned device);

  	void SetActiveSource();

  	void PowerOnDevices(unsigned device);

  	bool IsActiveSource();

  	void SetInactiveView();


  	bool m_bStarted;
    bool m_bHasButton;
    bool m_bIsReady;
    bool m_bHasConnectedAudioSystem;
    std::string m_strMenuLanguage;
    CDateTime m_standbySent;
    std::vector<CecButtonPress> m_buttonQueue;
    CecButtonPress m_currentButton;
    std::queue<CecVolumeChange> m_volumeChangeQueue;
    unsigned int m_lastKeypress;
    CecVolumeChange m_lastChange;
    int m_iExitCode;
    bool m_bIsMuted;
    bool m_bGoingToStandby;
    bool m_bIsRunning;
    bool m_bDeviceRemoved;
    CCriticalSection m_critSection;
    bool m_bActiveSourcePending;
    bool m_bStandbyPending;
    CDateTime m_preventActivateSourceOnPlay;
    bool m_bActiveSourceBeforeStandby;
    bool m_bOnPlayReceived;
    bool m_bPlaybackPaused;
    std::string m_strComPort;
    bool m_bPowerOnScreensaver;
    bool m_bUseTVMenuLanguage;
    bool m_bSendInactiveSource;
    bool m_bPowerOffScreensaver;
    bool m_bShutdownOnStandby;

  	PNode m_node;

  	bool m_trace;

  	bool m_showInfo;

  	bool m_showState;

  	bool m_showWarnings;

  	bool m_showMsgs;

  	int m_warnings;

  	LaInfo m_laInfo[15];
  };

  class CPeripheralCecFrameworkAdapterReopenJob: public CJob
  {
  public:
    CPeripheralCecFrameworkAdapterReopenJob(CPeripheralCecFrameworkAdapter *adapter)
      : m_adapter(adapter)
    {
    }
    ~CPeripheralCecFrameworkAdapterReopenJob() override = default;

    bool DoWork(void) override { return m_adapter->ReopenConnection(false); };

  private:
    CPeripheralCecFrameworkAdapter *m_adapter;
  };
}
