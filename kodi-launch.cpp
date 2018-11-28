#include "kodi-launch.h"

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <poll.h>
#include <errno.h>

#include <error.h>
#include <getopt.h>

#include <sys/types.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <sys/socket.h>
#include <sys/signalfd.h>
#include <signal.h>
#include <unistd.h>
#include <fcntl.h>

#include <linux/vt.h>
#include <linux/major.h>
#include <linux/kd.h>

#include <grp.h>

#include <sys/sysmacros.h>

bool CLauncher::SetupTTY()
{
  if (m_user.empty())
  {
    m_ttyFd = STDIN_FILENO;
  }
  else if (!m_tty.empty())
  {
    m_ttyFd = open(m_tty.c_str(), O_RDWR | O_NOCTTY);
  }
  else
  {
    int tty0 = open("/dev/tty0", O_WRONLY | O_CLOEXEC);
    if (tty0 < 0)
    {
      fprintf(stderr,"could not open tty0\n");
      return false;
    }

    int ttynr;
    auto ret = ioctl(tty0, VT_OPENQRY, &ttynr);
    if (ret < 0 || ttynr == -1)
    {
      fprintf(stderr,"failed to find non-opened console: %s\n", strerror(errno));
      return false;
    }

    m_tty.append("/dev/tty" + std::to_string(ttynr));
    m_ttyFd = open(m_tty.c_str(), O_RDWR | O_NOCTTY);
    close(tty0);
  }

  if (m_ttyFd < 0)
  {
    fprintf(stderr,"failed to open tty\n");
    return false;
  }

  struct stat buf;
  auto ret = fstat(m_ttyFd, &buf);
  if (ret < 0)
  {
    fprintf(stderr,"fstat failed\n");
    return false;
  }

  if (major(buf.st_rdev) != TTY_MAJOR || minor(buf.st_rdev) == 0)
  {
    fprintf(stderr,"kodi-launch must be run from a virtual terminal\n");
    return false;
  }

  if (!m_tty.empty())
  {
    auto ret = fstat(m_ttyFd, &buf);
    if (ret < 0)
    {
      fprintf(stderr,"stat %s failed\n", m_tty.c_str());
      return false;
    }

    if (major(buf.st_rdev) != TTY_MAJOR)
    {
      fprintf(stderr,"invalid tty device %s\n", m_tty.c_str());
      return false;
    }
  }

  ret = ioctl(m_ttyFd, KDGKBMODE, &m_kbMode);
  if (ret < 0)
  {
    fprintf(stderr,"failed to get current keyboard mode: %s\n", strerror(errno));
    return false;
  }

  ret = ioctl(m_ttyFd, KDSKBMODE, K_OFF);
  if (ret < 0)
  {
    fprintf(stderr,"failed to set K_OFF keyboard mode: %s\n", strerror(errno));
    return false;
  }

  ret = ioctl(m_ttyFd, KDSETMODE, KD_GRAPHICS);
  if (ret < 0)
  {
    fprintf(stderr,"failed to set KD_GRAPHICS mode on tty: %s\n", strerror(errno));
    return false;
  }

  struct vt_mode mode = { 0 };
  mode.mode = VT_PROCESS;
  // mode.relsig = SIGUSR1;
  // mode.acqsig = SIGUSR2;
  ret = ioctl(m_ttyFd, VT_SETMODE, &mode);
  if (ret < 0)
  {
    fprintf(stderr,"failed to take control of vt handling: %s\n", strerror(errno));
    return false;
  }

  return true;
}

static int PamConversationFunction(int msg_count, const struct pam_message **messages, struct pam_response **responses, void *user_data)
{
  return PAM_SUCCESS;
}

bool CLauncher::SetupPAM()
{
  m_pc.conv = PamConversationFunction;

  auto ret = pam_start("login", m_pw->pw_name, &m_pc, &m_ph);
  if (ret != PAM_SUCCESS)
  {
    fprintf(stderr,"failed to start pam transaction: %s\n", pam_strerror(m_ph, ret));
    return false;
  }

  ret = pam_set_item(m_ph, PAM_TTY, ttyname(m_ttyFd));
  if (ret != PAM_SUCCESS)
  {
    fprintf(stderr,"failed to set PAM_TTY item: %s\n", pam_strerror(m_ph, ret));
    return false;
  }

  ret = pam_open_session(m_ph, 0);
  if (ret != PAM_SUCCESS)
  {
    fprintf(stderr,"failed to open pam session: %s\n", pam_strerror(m_ph, ret));
    return false;
  }

  return true;
}

bool CLauncher::SetupLauncherSocket()
{
  auto ret = socketpair(AF_LOCAL, SOCK_SEQPACKET, 0, m_sock.data());
  if (ret < 0)
  {
    fprintf(stderr,"socketpair failed\n");
    return false;
  }

  ret = fcntl(m_sock[0], F_SETFD, FD_CLOEXEC);
  if (ret < 0)
  {
    fprintf(stderr,"fcntl failed\n");
    return false;
  }

  return true;
}

void CLauncher::HandleSignal()
{
  struct signalfd_siginfo sig;

  auto ret = read(m_signalFd, &sig, sizeof sig);
  if (ret != sizeof(sig))
  {
    fprintf(stderr,"reading signalfd failed: %s\n", strerror(errno));
    return;
  }

  int status;
  switch (sig.ssi_signo)
  {
  case SIGCHLD:
  {
    auto pid = waitpid(-1, &status, 0);
    if (pid == m_child)
    {
      m_child = 0;

      if (WIFEXITED(status) > 0)
      {
        ret = WEXITSTATUS(status);
      }
      else if (WIFSIGNALED(status) > 0)
      {
        ret = 10 + WTERMSIG(status);
      }
      else
      {
        ret = 0;
      }

      Quit(ret);
    }

    break;
  }
  case SIGTERM:
  case SIGINT:
  {
    if (m_child > 0)
    {
      kill(m_child, sig.ssi_signo);
    }
    break;
  }
  // case SIGUSR1:
  // {
  //   send_reply(wl, WESTON_LAUNCHER_DEACTIVATE);
  //   close_input_fds(wl);
  //   drmDropMaster(wl->drm_fd);
  //   ioctl(wl->tty, VT_RELDISP, 1);
  //   break;
  // }
  // case SIGUSR2:
  // {
  //   ioctl(wl->tty, VT_RELDISP, VT_ACKACQ);
  //   drmSetMaster(wl->drm_fd);
  //   send_reply(wl, WESTON_LAUNCHER_ACTIVATE);
  //   break;
  // }
  default:
  {
    break;
  }
  }
}

void CLauncher::Quit(int status)
{
  close(m_signalFd);
  close(m_sock[0]);

  if (!m_user.empty())
  {
    auto ret = pam_close_session(m_ph, 0);
    if (ret < 0)
    {
      fprintf(stderr,"pam_close_session failed: %s\n", pam_strerror(m_ph, ret));
    }

    pam_end(m_ph, ret);
  }

  auto ret = ioctl(m_ttyFd, KDSKBMODE, m_kbMode);
  if (ret < 0)
  {
    fprintf(stderr,"failed to restore keyboard mode: %s\n", strerror(errno));
  }

  ret = ioctl(m_ttyFd, KDSETMODE, KD_TEXT);
  if (ret < 0)
  {
    fprintf(stderr,"failed to set KD_TEXT mode on tty: %s\n", strerror(errno));
  }

  struct vt_mode mode = { 0 };
  mode.mode = VT_AUTO;
  ret = ioctl(m_ttyFd, VT_SETMODE, &mode);
  if (ret < 0)
  {
    fprintf(stderr,"could not reset vt handling\n");
  }

  exit(status);
}

bool CLauncher::SetupSignals()
{
  struct sigaction sa;
  sa.sa_handler = SIG_DFL;
  sa.sa_flags = SA_NOCLDSTOP | SA_RESTART;
  auto ret = sigaction(SIGCHLD, &sa, nullptr);
  if (ret < 0)
  {
    return false;
  }

  sa.sa_handler = SIG_IGN;
  sa.sa_flags = 0;
  sigaction(SIGHUP, &sa, nullptr);

  sigset_t mask;
  ret = sigemptyset(&mask);
  if (ret < 0)
  {
    return false;
  }

  sigaddset(&mask, SIGCHLD);
  sigaddset(&mask, SIGINT);
  sigaddset(&mask, SIGTERM);
  // sigaddset(&mask, SIGUSR1);
  // sigaddset(&mask, SIGUSR2);
  ret = sigprocmask(SIG_BLOCK, &mask, nullptr);
  if (ret < 0)
  {
    return false;
  }

  m_signalFd = signalfd(-1, &mask, SFD_NONBLOCK | SFD_CLOEXEC);
  if (m_signalFd < 0)
  {
    return false;
  }

  return true;
}

void CLauncher::SetupSession()
{
  if (m_tty != std::to_string(STDIN_FILENO))
  {
    if (setsid() < 0)
    {
      fprintf(stderr,"setsid failed\n");
    }

    auto ret = ioctl(m_ttyFd, TIOCSCTTY, 0);
    if (ret < 0)
    {
      fprintf(stderr,"tty in use\n");
    }
  }

  auto term = getenv("TERM");
  clearenv();

  if (term)
  {
    setenv("TERM", term, 1);
  }

  setenv("USER", m_pw->pw_name, 1);
  setenv("HOME", m_pw->pw_dir, 1);
  setenv("SHELL", m_pw->pw_shell, 1);

  auto env = pam_getenvlist(m_ph);
  if (env)
  {
    for (int i = 0; env[i]; ++i)
    {
      if (putenv(env[i]) != 0)
      {
        fprintf(stderr,"putenv %s failed\n", env[i]);
      }
    }

    free(env);
  }
}

void CLauncher::DropPrivileges()
{
  auto ret = setgid(m_pw->pw_gid);
  if (ret < 0)
  {
    fprintf(stderr,"failed to drop group privileges\n");
  }

  ret = setuid(m_pw->pw_uid);
  if (ret < 0)
  {
    fprintf(stderr,"failed to drop group privileges\n");
  }
}

bool CLauncher::LaunchKodi()
{
  printf("spawned kodi with pid: %d\n", getpid());

  if (!m_user.empty())
  {
    SetupSession();
  }

  if (geteuid() == 0)
  {
    DropPrivileges();
  }

  std::array<char*, 6> args =
  {
    "/bin/sh",
    "-l",
    "-c",
    "/usr/local/lib64/kodi/kodi-gbm",
    "-fs",
    nullptr,
  };

  sigset_t mask;
  /* Do not give our signal mask to the new process. */
  sigemptyset(&mask);
  sigaddset(&mask, SIGTERM);
  sigaddset(&mask, SIGCHLD);
  sigaddset(&mask, SIGINT);
  sigprocmask(SIG_UNBLOCK, &mask, nullptr);

  execv(args[0], args.data());

  return true;
}

static void PrintHelp(const char *name)
{
  fprintf(stderr, "Usage: %s [args...] [-- [weston args..]]\n", name);
  fprintf(stderr, "  -u, --user      Start session as specified username,\n"
                  "                  e.g. -u joe, requires root.\n");
  fprintf(stderr, "  -t, --tty       Start session on alternative tty,\n"
                  "                  e.g. -t /dev/tty4, requires -u option.\n");
  fprintf(stderr, "  -h, --help      Display this help message\n");
}

bool CLauncher::Launch(int argc, char* argv[])
{
  std::array<struct option, 3> options =
  {{
    { "user", required_argument, nullptr, 'u' },
    { "tty", required_argument, nullptr, 't' },
    { "help", no_argument, nullptr, 'h' },
  }};

  int opt;
  while ((opt = getopt_long(argc, argv, "u:t:h", options.data(), nullptr)) != -1)
  {
    switch (opt)
    {
    case 'u':
    {
      m_user = optarg;
      if (getuid() != 0)
      {
        fprintf(stderr,"permission denied. -u option allowed for root only\n");
        return false;
      }

      break;
    }
    case 't':
    {
      m_tty = optarg;
      break;
    }
    case 'h':
    {
      PrintHelp("kodi-launch");
      return false;
      break;
    }
    default:
    {
      return false;
    }
    }
  }

  if (!m_tty.empty() && m_user.empty())
  {
    fprintf(stderr,"-t/--tty option requires -u/--user option as well\n");
    return false;
  }

  if (!m_user.empty())
  {
    m_pw = getpwnam(m_user.c_str());
  }
  else
  {
    m_pw = getpwuid(getuid());
  }

  if (!m_pw)
  {
    fprintf(stderr,"failed to get username\n");
    return false;
  }

  if (!SetupTTY())
  {
    return false;
  }

  if (!SetupLauncherSocket())
  {
    return false;
  }

  if (!SetupPAM())
  {
    return false;
  }

  if (!SetupSignals())
  {
    return false;
  }

  m_child = fork();
  if (m_child < 0)
  {
    fprintf(stderr,"fork failed\n");
    return false;
  }
  else if(m_child == 0)
  {
    LaunchKodi();
  }

  close(m_sock[1]);

  if (m_ttyFd != STDIN_FILENO)
  {
    close(m_ttyFd);
  }

  while (1)
  {
    std::array<struct pollfd, 2> fds =
    {{
      { m_sock[0], POLLIN, 0 },
      { m_signalFd, POLLIN, 0 },
    }};

    auto n = poll(fds.data(), 2, -1);
    if (n < 0)
    {
      fprintf(stderr,"poll failed\n");
    }

    // if (fds[0].revents & POLLIN)
    // {
    //   handle_socket_msg(&wl);
    // }

    if (fds[1].revents)
    {
      HandleSignal();
    }
  }

  return true;
}

int main(int argc, char* argv[])
{
  CLauncher launcher;

  if (!launcher.Launch(argc, argv))
  {
    return -1;
  }

  return 0;
}
