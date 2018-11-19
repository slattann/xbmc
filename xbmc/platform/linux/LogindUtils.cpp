/*
 *  Copyright (C) 2005-2018 Team Kodi
 *  This file is part of Kodi - https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSES/README.md for more information.
 */

#include "LogindUtils.h"
#include "SessionUtils.h"

#include "platform/linux/DBusMessage.h"
#include "utils/log.h"

#include <sys/sysmacros.h>

namespace
{
  const std::string logindService{"org.freedesktop.login1"};
  const std::string logindObject{"/org/freedesktop/login1"};
  const std::string logindManagerInterface{"org.freedesktop.login1.Manager"};
  const std::string logindSessionInterface{"org.freedesktop.login1.Session"};
}

uint32_t CSessionUtils::Open(std::string path, int flags)
{
  std::string sessionPath = CLogindUtils::GetSessionPath();

  if (sessionPath.empty())
  {
    return open(path.c_str(), O_RDWR | flags);
  }

  struct stat st;
  auto ret = stat(path.c_str(), &st);
  if (ret < 0)
  {
    CLog::Log(LOGERROR, "logind: stat failed for: {}", path);
    return -1;
  }

  if (!S_ISCHR(st.st_mode))
  {
    CLog::Log(LOGERROR, "logind: invalid device passed");
    return -1;
  }

  auto fd = CLogindUtils::TakeDevice(sessionPath, major(st.st_rdev), minor(st.st_rdev));
  if (fd < 0)
  {
    return fd;
  }

  auto fl = fcntl(fd, F_GETFL);
  if (fl < 0)
  {
    CLog::Log(LOGERROR, "logind: F_GETFL failed");
    close(fd);
    CLogindUtils::ReleaseDevice(sessionPath, major(st.st_rdev), minor(st.st_rdev));
    return -1;
  }

  if (flags & O_NONBLOCK)
    fl |= O_NONBLOCK;

  ret = fcntl(fd, F_SETFL, fl);
  if (ret < 0)
  {
    CLog::Log(LOGERROR, "logind: F_SETFL failed");
    close(fd);
    CLogindUtils::ReleaseDevice(sessionPath, major(st.st_rdev), minor(st.st_rdev));
    return -1;
  }

  return fd;
}

void CSessionUtils::Close(int fd)
{
  std::string sessionPath = CLogindUtils::GetSessionPath();

  if (sessionPath.empty())
  {
    close(fd);
    return;
  }

  struct stat st;
  auto ret = fstat(fd, &st);
  close(fd);
  if (ret < 0)
  {
    CLog::Log(LOGERROR, "logind: cannot fstat fd: {}", fd);
    return;
  }

  if (!S_ISCHR(st.st_mode))
  {
    CLog::Log(LOGERROR, "logind: invalid device passed");
    return;
  }

  CLogindUtils::ReleaseDevice(sessionPath, major(st.st_rdev), minor(st.st_rdev));
}

std::string CLogindUtils::GetSessionPath()
{
  CDBusMessage message(logindService, logindObject, logindManagerInterface, "GetSessionByPID");
  message.AppendArguments<uint32_t>(getpid());

  CDBusError error;
  auto reply = message.Send(DBUS_BUS_SYSTEM, error);
  if (!reply)
  {
    CLog::Log(LOGERROR, "logind: GetSessionByPID failed");
    return "";
  }

  // left this c-style for now as our dbus methods don't accept DBUS_TYPE_OBJECT_PATH
  char* path;
  auto b = dbus_message_get_args(reply, nullptr,
                                 DBUS_TYPE_OBJECT_PATH, &path,
                                 DBUS_TYPE_INVALID);

  if (!b)
  {
    CLog::Log(LOGERROR, "logind: failed to get session path");
    return "";
  }

  std::string sessionPath{path};
  return sessionPath;
}

uint32_t CLogindUtils::TakeDevice(std::string sessionPath, uint32_t major, uint32_t minor)
{
  CDBusMessage message(logindService, sessionPath, logindSessionInterface, "TakeDevice");
  message.AppendArguments<uint32_t, uint32_t>(major, minor);

  CDBusError error;
  auto reply = message.Send(DBUS_BUS_SYSTEM, error);
  if (!reply)
  {
    CLog::Log(LOGERROR, "logind: TakeDevice failed");
    return -1;
  }

  // left this c-style for now as our dbus methods don't accept DBUS_TYPE_UNIX_FD
  int fd;
  dbus_bool_t active;
  auto b = dbus_message_get_args(reply, nullptr,
                                 DBUS_TYPE_UNIX_FD, &fd,
                                 DBUS_TYPE_BOOLEAN, &active,
                                 DBUS_TYPE_INVALID);

  if (!b)
  {
    CLog::Log(LOGERROR, "logind: failed to get unix fd");
    return -1;
  }

  return fd;
}

void CLogindUtils::ReleaseDevice(std::string sessionPath, uint32_t major, uint32_t minor)
{
  CDBusMessage message(logindService, sessionPath, logindSessionInterface, "ReleaseDevice");
  message.AppendArguments<uint32_t, uint32_t>(major, minor);

  if (!message.SendAsyncSystem())
  {
    CLog::Log(LOGERROR, "logind: ReleaseDevice failed");
  }
}

bool CLogindUtils::TakeControl()
{
  CDBusMessage message(logindService, m_sessionPath, logindSessionInterface, "TakeControl");

  message.AppendArgument<bool>(false);

  CDBusError error;
  auto reply = message.Send(m_connection, error);

  if (!reply)
  {
    CLog::Log(LOGERROR, "logind: cannot take control over session: {}", m_sessionPath);
    return false;
  }

  return true;
}

bool CLogindUtils::Activate()
{
  CDBusMessage message(logindService, m_sessionPath, logindSessionInterface, "Activate");

  if (!message.SendAsync(m_connection))
  {
    CLog::Log(LOGERROR, "logind: cannot activate session: {}", m_sessionPath);
    return false;
  }

  return true;
}

bool CLogindUtils::Connect()
{
  if (!m_connection.Connect(DBUS_BUS_SYSTEM, false))
  {
    CLog::Log(LOGERROR, "logind: cannot connect to system dbus");
    return false;
  }

  m_sessionPath = GetSessionPath();

  if (!TakeControl())
  {
    return false;
  }

  if (!Activate())
  {
    return false;
  }

  CLog::Log(LOGDEBUG, "logind: successfully registered session: {}", m_sessionPath);

  return true;
}

void CLogindUtils::ReleaseControl()
{
  CDBusMessage message(logindService, m_sessionPath, logindSessionInterface, "ReleaseControl");
  message.SendAsync(m_connection);
}

void CLogindUtils::Destroy()
{
  ReleaseControl();
}

