/*
 *      Copyright (C) 2010-2013 Team XBMC
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

#include "system.h"

#include "system_gl.h"

#include "LinuxRendererGLES3.h"

#include "ServiceBroker.h"
#include "RenderFactory.h"
#include "rendering/gles/RenderSystemGLES.h"

using namespace Shaders;

CBaseRenderer* CLinuxRendererGLES3::Create(CVideoBuffer *buffer)
{
  CRenderSystemGLES *renderSystem = dynamic_cast<CRenderSystemGLES*>(&CServiceBroker::GetRenderSystem());
  unsigned int major, minor;
  renderSystem->GetRenderVersion(major, minor);
  if (major >= 3)
    return new CLinuxRendererGLES3();

  return nullptr;
}

bool CLinuxRendererGLES3::Register()
{
  VIDEOPLAYER::CRendererFactory::RegisterRenderer("OpenGLES3", CLinuxRendererGLES3::Create);
  return true;
}

CLinuxRendererGLES3::CLinuxRendererGLES3()
: CLinuxRendererGLES()
{
  m_format8 = GL_RED;
  m_format16 = GL_RG;
  m_format8alpha = GL_RED;
}
