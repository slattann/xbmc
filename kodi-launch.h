#include <string.h>
#include <array>

#include <security/pam_appl.h>

#include <pwd.h>


class CLauncher
{
public:
  CLauncher() = default;
  ~CLauncher() = default;

  bool Launch(int argc, char* argv[]);

private:
  bool SetupTTY();
  bool SetupPAM();
  bool SetupLauncherSocket();
  void HandleSignal();
  void Quit(int status);
  bool SetupSignals();
  void SetupSession();
  void DropPrivileges();
  bool LaunchKodi();

  std::string m_user;
  std::string m_tty;
  int m_ttyFd{-1};
  int m_kbMode{-1};

  pam_conv m_pc;
  pam_handle_t* m_ph{nullptr};

  std::array<int, 2> m_sock;

  passwd* m_pw;
  int m_signalFd{-1};
  pid_t m_child;
};
