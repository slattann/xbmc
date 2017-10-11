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

#include <errno.h>
#include <drm_mode.h>
#include <string.h>
#include <unistd.h>

#include "settings/Settings.h"
#include "utils/log.h"

#include "DRMAtomic.h"
#include "WinSystemGbmGLESContext.h"

static struct drm *m_drm = nullptr;
static struct gbm *m_gbm = nullptr;

static struct drm_fb *m_drm_fb = new drm_fb;

static struct gbm_bo *m_bo = nullptr;
static struct gbm_bo *m_next_bo = nullptr;

bool CDRMAtomic::AddConnectorProperty(drmModeAtomicReq *req, int obj_id, const char *name, int value)
{
  struct connector *obj = m_drm->connector;
  int prop_id = 0;

  for (unsigned int i = 0 ; i < obj->props->count_props ; i++)
  {
    if (strcmp(obj->props_info[i]->name, name) == 0)
    {
      prop_id = obj->props_info[i]->prop_id;
      break;
    }
  }

  if (prop_id < 0)
  {
    CLog::Log(LOGERROR, "CDRMAtomic::%s - no connector property: %s", __FUNCTION__, name);
    return false;
  }

  auto ret = drmModeAtomicAddProperty(req, obj_id, prop_id, value);
  if (ret < 0)
  {
    return false;
  }

  return true;
}

bool CDRMAtomic::AddCrtcProperty(drmModeAtomicReq *req, int obj_id, const char *name, int value)
{
  struct crtc *obj = m_drm->crtc;
  int prop_id = -1;

  for (unsigned int i = 0 ; i < obj->props->count_props ; i++)
  {
    if (strcmp(obj->props_info[i]->name, name) == 0)
    {
      prop_id = obj->props_info[i]->prop_id;
      break;
    }
  }

  if (prop_id < 0)
  {
    CLog::Log(LOGERROR, "CDRMAtomic::%s - no crtc property: %s", __FUNCTION__, name);
    return false;
  }

  auto ret = drmModeAtomicAddProperty(req, obj_id, prop_id, value);
  if (ret < 0)
  {
    return false;
  }

  return true;
}

bool CDRMAtomic::AddPlaneProperty(drmModeAtomicReq *req, struct plane *obj, const char *name, int value)
{
  int prop_id = -1;

  for (unsigned int i = 0 ; i < obj->props->count_props ; i++)
  {
    if (strcmp(obj->props_info[i]->name, name) == 0)
    {
      prop_id = obj->props_info[i]->prop_id;
      break;
    }
  }

  if (prop_id < 0)
  {
    CLog::Log(LOGERROR, "CDRMAtomic::%s - no plane property: %s", __FUNCTION__, name);
    return false;
  }

  auto ret = drmModeAtomicAddProperty(req, obj->plane->plane_id, prop_id, value);
  if (ret < 0)
  {
    return false;
  }

  return true;
}

bool CDRMAtomic::DrmAtomicCommit(int fb_id, int flags)
{
  uint32_t blob_id;

  if (flags & DRM_MODE_ATOMIC_ALLOW_MODESET)
  {
    if (!AddConnectorProperty(m_drm->req, m_drm->connector->connector->connector_id, "CRTC_ID", m_drm->crtc->crtc->crtc_id))
    {
      return false;
    }

    if (drmModeCreatePropertyBlob(m_drm->fd, m_drm->mode, sizeof(*m_drm->mode), &blob_id) != 0)
    {
      return false;
    }

    if (!AddCrtcProperty(m_drm->req, m_drm->crtc->crtc->crtc_id, "MODE_ID", blob_id))
    {
      return false;
    }

    if (!AddCrtcProperty(m_drm->req, m_drm->crtc->crtc->crtc_id, "ACTIVE", 1))
    {
      return false;
    }
  }

  AddPlaneProperty(m_drm->req, m_drm->primary_plane, "FB_ID", fb_id);
  AddPlaneProperty(m_drm->req, m_drm->primary_plane, "CRTC_ID", m_drm->crtc->crtc->crtc_id);
  AddPlaneProperty(m_drm->req, m_drm->primary_plane, "SRC_X", 0);
  AddPlaneProperty(m_drm->req, m_drm->primary_plane, "SRC_Y", 0);
  AddPlaneProperty(m_drm->req, m_drm->primary_plane, "SRC_W", m_drm->mode->hdisplay << 16);
  AddPlaneProperty(m_drm->req, m_drm->primary_plane, "SRC_H", m_drm->mode->vdisplay << 16);
  AddPlaneProperty(m_drm->req, m_drm->primary_plane, "CRTC_X", 0);
  AddPlaneProperty(m_drm->req, m_drm->primary_plane, "CRTC_Y", 0);
  AddPlaneProperty(m_drm->req, m_drm->primary_plane, "CRTC_W", m_drm->mode->hdisplay);
  AddPlaneProperty(m_drm->req, m_drm->primary_plane, "CRTC_H", m_drm->mode->vdisplay);

  auto ret = drmModeAtomicCommit(m_drm->fd, m_drm->req, flags, nullptr);
  if (ret)
  {
    return false;
  }

  drmModeAtomicFree(m_drm->req);

  m_drm->req = drmModeAtomicAlloc();

  return true;
}

void CDRMAtomic::FlipPage()
{
  uint32_t flags = 0;

  if(m_drm->need_modeset)
  {
    flags |= DRM_MODE_ATOMIC_ALLOW_MODESET;
    m_drm->need_modeset = false;
  }

  gbm_surface_release_buffer(m_gbm->surface, m_bo);
  m_bo = m_next_bo;

  m_next_bo = gbm_surface_lock_front_buffer(m_gbm->surface);
  if (!m_next_bo)
  {
    CLog::Log(LOGERROR, "CDRMAtomic::%s - Failed to lock frontbuffer", __FUNCTION__);
    return;
  }

  m_drm_fb = CDRMUtils::DrmFbGetFromBo(m_next_bo);
  if (!m_drm_fb)
  {
    CLog::Log(LOGERROR, "CDRMAtomic::%s - Failed to get a new FBO", __FUNCTION__);
    return;
  }

  auto ret = DrmAtomicCommit(m_drm_fb->fb_id, flags);
  if (!ret) {
    CLog::Log(LOGERROR, "CDRMAtomic::%s - failed to commit: %s", __FUNCTION__, strerror(errno));
    return;
  }
}

bool CDRMAtomic::InitDrmAtomic(drm *drm, gbm *gbm)
{
  m_drm = drm;
  m_gbm = gbm;

  m_drm->req = drmModeAtomicAlloc();

  if (!CDRMUtils::InitDrm(m_drm))
  {
    return false;
  }

  m_gbm->dev = gbm_create_device(m_drm->fd);

  if (!CGBMUtils::InitGbm(m_gbm, m_drm->mode->hdisplay, m_drm->mode->vdisplay))
  {
    return false;
  }

  return true;
}

void CDRMAtomic::DestroyDrmAtomic()
{
  gbm_surface_release_buffer(m_gbm->surface, m_bo);
  m_bo = m_next_bo = nullptr;

  CDRMUtils::DestroyDrm();

  if(m_gbm->surface)
  {
    gbm_surface_destroy(m_gbm->surface);
    m_gbm->surface = nullptr;
  }

  if(m_gbm->dev)
  {
    gbm_device_destroy(m_gbm->dev);
    m_gbm->dev = nullptr;
  }

  drmModeAtomicFree(m_drm->req);
  m_drm->req = nullptr;
}

bool CDRMAtomic::SetVideoMode(RESOLUTION_INFO res)
{
  m_drm->need_modeset = true;

  return true;
}
