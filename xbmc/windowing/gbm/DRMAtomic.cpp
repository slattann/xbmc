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
#include <drm/drm_mode.h>
#include <EGL/egl.h>
#include <EGL/eglext.h>
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

bool CDRMAtomic::DrmAtomicCommit(int fb_id, int flags, bool rendered, bool videoLayer)
{
  uint32_t blob_id;
  struct plane *plane;

  if (flags & DRM_MODE_ATOMIC_ALLOW_MODESET)
  {
    if (!AddConnectorProperty(m_drm->req, m_drm->connector_id, "CRTC_ID", m_drm->crtc_id))
    {
      return false;
    }

    if (drmModeCreatePropertyBlob(m_drm->fd, m_drm->mode, sizeof(*m_drm->mode), &blob_id) != 0)
    {
      return false;
    }

    if (!AddCrtcProperty(m_drm->req, m_drm->crtc_id, "MODE_ID", blob_id))
    {
      return false;
    }

    if (!AddCrtcProperty(m_drm->req, m_drm->crtc_id, "ACTIVE", 1))
    {
      return false;
    }
  }

  if (rendered)
  {
    if (videoLayer)
      plane = m_drm->overlay_plane;
    else
      plane = m_drm->primary_plane;

    AddPlaneProperty(m_drm->req, plane, "FB_ID", fb_id);
    AddPlaneProperty(m_drm->req, plane, "CRTC_ID", m_drm->crtc_id);
    AddPlaneProperty(m_drm->req, plane, "SRC_X", 0);
    AddPlaneProperty(m_drm->req, plane, "SRC_Y", 0);
    AddPlaneProperty(m_drm->req, plane, "SRC_W", m_drm->mode->hdisplay << 16);
    AddPlaneProperty(m_drm->req, plane, "SRC_H", m_drm->mode->vdisplay << 16);
    AddPlaneProperty(m_drm->req, plane, "CRTC_X", 0);
    AddPlaneProperty(m_drm->req, plane, "CRTC_Y", 0);
    AddPlaneProperty(m_drm->req, plane, "CRTC_W", m_drm->mode->hdisplay);
    AddPlaneProperty(m_drm->req, plane, "CRTC_H", m_drm->mode->vdisplay);
  }

  auto ret = drmModeAtomicCommit(m_drm->fd, m_drm->req, flags, nullptr);
  if (ret)
  {
    return false;
  }

  drmModeAtomicFree(m_drm->req);

  m_drm->req = drmModeAtomicAlloc();

  return true;
}

void CDRMAtomic::FlipPage(bool rendered, bool videoLayer)
{
  uint32_t flags = 0;

  if(m_drm->need_modeset)
  {
    flags |= DRM_MODE_ATOMIC_ALLOW_MODESET;
    m_drm->need_modeset = false;
  }

  if (rendered)
  {
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
  }

  auto ret = DrmAtomicCommit(m_drm_fb->fb_id, flags, rendered, videoLayer);
  if (!ret) {
    CLog::Log(LOGERROR, "CDRMAtomic::%s - failed to commit: %s", __FUNCTION__, strerror(errno));
    return;
  }
}

int CDRMAtomic::GetPlaneId(uint32_t type)
{
  drmModePlaneResPtr plane_resources;
  int ret = -EINVAL;
  int found_primary = 0;

  plane_resources = drmModeGetPlaneResources(m_drm->fd);
  if (!plane_resources)
  {
    CLog::Log(LOGERROR, "CDRMAtomic::%s - drmModeGetPlaneResources failed: %s", __FUNCTION__, strerror(errno));
    return -1;
  }

  for (uint32_t i = 0; (i < plane_resources->count_planes) && !found_primary; i++)
  {
    uint32_t id = plane_resources->planes[i];
    drmModePlanePtr plane = drmModeGetPlane(m_drm->fd, id);
    if (!plane)
    {
      CLog::Log(LOGERROR, "CDRMAtomic::%s - drmModeGetPlane(%u) failed: %s", __FUNCTION__, id, strerror(errno));
      continue;
    }

    if (plane->possible_crtcs & (1 << m_drm->crtc_index))
    {
      drmModeObjectPropertiesPtr props = drmModeObjectGetProperties(m_drm->fd, id, DRM_MODE_OBJECT_PLANE);

      /* primary or not, this plane is good enough to use: */
      ret = id;

      for (uint32_t j = 0; j < props->count_props; j++)
      {
        drmModePropertyPtr p = drmModeGetProperty(m_drm->fd, props->props[j]);

        if ((strcmp(p->name, "type") == 0) && (props->prop_values[j] == type))
        {
          /* found our primary plane, lets use that: */
          found_primary = 1;
        }

        drmModeFreeProperty(p);
      }

      drmModeFreeObjectProperties(props);
    }

    drmModeFreePlane(plane);
  }

  drmModeFreePlaneResources(plane_resources);

  return ret;
}

bool CDRMAtomic::InitDrmAtomic(drm *drm, gbm *gbm)
{
  int plane_id;
  int ret;

  m_drm = drm;
  m_gbm = gbm;

  if (!CDRMUtils::InitDrm(m_drm))
  {
    return false;
  }

  ret = drmSetClientCap(m_drm->fd, DRM_CLIENT_CAP_UNIVERSAL_PLANES, 1);
  if (ret)
  {
    CLog::Log(LOGERROR, "CDRMAtomic::%s - failed to set universal planes capability: %s", __FUNCTION__, strerror(errno));
    return false;
  }

  ret = drmSetClientCap(m_drm->fd, DRM_CLIENT_CAP_ATOMIC, 1);
  if (ret)
  {
    CLog::Log(LOGERROR, "CDRMAtomic::%s - no atomic modesetting support: %s", __FUNCTION__, strerror(errno));
    return false;
  }

  /* We only do single plane to single crtc to single connector, no
     * fancy multi-monitor.  So just grab the
     * plane/crtc/connector property info for one of each:
     */
  m_drm->primary_plane = new plane;
  m_drm->overlay_plane = new plane;
  m_drm->crtc = new crtc;
  m_drm->connector = new connector;

  // primary plane
  ret = GetPlaneId(DRM_PLANE_TYPE_PRIMARY);
  if (!ret)
  {
    CLog::Log(LOGERROR, "CDRMAtomic::%s - could not find a suitable primary plane", __FUNCTION__);
    return false;
  }
  else
  {
    CLog::Log(LOGDEBUG, "CDRMAtomic::%s - primary plane %d", __FUNCTION__, ret);
    plane_id = ret;
  }

  m_drm->primary_plane->plane = drmModeGetPlane(m_drm->fd, plane_id);
  if (!m_drm->primary_plane->plane)
  {
    CLog::Log(LOGERROR, "CDRMAtomic::%s - could not get plane %i: %s", __FUNCTION__, plane_id, strerror(errno));
    return false;
  }

  m_drm->primary_plane->props = drmModeObjectGetProperties(m_drm->fd, plane_id, DRM_MODE_OBJECT_PLANE);
  if (!m_drm->primary_plane->props)
  {
    CLog::Log(LOGERROR, "CDRMAtomic::%s - could not get plane %u properties: %s", __FUNCTION__, plane_id, strerror(errno));
    return false;
  }

  m_drm->primary_plane->props_info = new drmModePropertyPtr;
  for (uint32_t i = 0; i < m_drm->primary_plane->props->count_props; i++)
  {
    m_drm->primary_plane->props_info[i] = drmModeGetProperty(m_drm->fd, m_drm->primary_plane->props->props[i]);
  }

  // overlay plane
  ret = GetPlaneId(DRM_PLANE_TYPE_OVERLAY);
  if (!ret)
  {
    CLog::Log(LOGERROR, "CDRMAtomic::%s - could not find a suitable overlay plane", __FUNCTION__);
    return false;
  }
  else
  {
    CLog::Log(LOGDEBUG, "CDRMAtomic::%s - overlay plane %d", __FUNCTION__, ret);
    plane_id = ret;
  }

  m_drm->overlay_plane->plane = drmModeGetPlane(m_drm->fd, plane_id);
  if (!m_drm->overlay_plane->plane)
  {
    CLog::Log(LOGERROR, "CDRMAtomic::%s - could not get plane %i: %s", __FUNCTION__, plane_id, strerror(errno));
    return false;
  }

  m_drm->overlay_plane->props = drmModeObjectGetProperties(m_drm->fd, plane_id, DRM_MODE_OBJECT_PLANE);
  if (!m_drm->overlay_plane->props)
  {
    CLog::Log(LOGERROR, "CDRMAtomic::%s - could not get plane %u properties: %s", __FUNCTION__, plane_id, strerror(errno));
    return false;
  }

  m_drm->overlay_plane->props_info = new drmModePropertyPtr;
  for (uint32_t i = 0; i < m_drm->overlay_plane->props->count_props; i++)
  {
    m_drm->overlay_plane->props_info[i] = drmModeGetProperty(m_drm->fd, m_drm->overlay_plane->props->props[i]);
  }

  // crtc
  m_drm->crtc->crtc = drmModeGetCrtc(m_drm->fd, m_drm->crtc_id);
  if (!m_drm->crtc->crtc)
  {
    CLog::Log(LOGERROR, "CDRMAtomic::%s - could not get crtc %i: %s", __FUNCTION__, m_drm->crtc_id, strerror(errno));
    return false;
  }

  m_drm->crtc->props = drmModeObjectGetProperties(m_drm->fd, m_drm->crtc_id, DRM_MODE_OBJECT_CRTC);
  if (!m_drm->crtc->props)
  {
    CLog::Log(LOGERROR, "CDRMAtomic::%s - could not get crtc %u properties: %s", __FUNCTION__, m_drm->crtc_id, strerror(errno));
    return false;
  }

  m_drm->crtc->props_info = new drmModePropertyPtr;
  for (uint32_t i = 0; i < m_drm->crtc->props->count_props; i++)
  {
    m_drm->crtc->props_info[i] = drmModeGetProperty(m_drm->fd, m_drm->crtc->props->props[i]);             \
  }

  // connector
  m_drm->connector->connector = drmModeGetConnector(m_drm->fd, m_drm->connector_id);
  if (!m_drm->connector->connector)
  {
    CLog::Log(LOGERROR, "CDRMAtomic::%s - could not get plane %i: %s", __FUNCTION__, m_drm->connector_id, strerror(errno));
    return false;
  }

  m_drm->connector->props = drmModeObjectGetProperties(m_drm->fd, m_drm->connector_id, DRM_MODE_OBJECT_CONNECTOR);
  if (!m_drm->connector->props)
  {
    CLog::Log(LOGERROR, "CDRMAtomic::%s - could not get connector %u properties: %s", __FUNCTION__, m_drm->connector_id, strerror(errno));
    return false;
  }

  m_drm->connector->props_info = new drmModePropertyPtr;
  for (uint32_t i = 0; i < m_drm->connector->props->count_props; i++)
  {
    m_drm->connector->props_info[i] = drmModeGetProperty(m_drm->fd, m_drm->connector->props->props[i]);             \
  }

  m_gbm->dev = gbm_create_device(m_drm->fd);

  if (!CGBMUtils::InitGbm(m_gbm, m_drm->mode->hdisplay, m_drm->mode->vdisplay))
  {
    return false;
  }

  m_drm->req = drmModeAtomicAlloc();

  return true;
}

void CDRMAtomic::DestroyDrmAtomic()
{
  CDRMUtils::DestroyDrm();

  if(m_gbm->surface)
  {
    gbm_surface_destroy(m_gbm->surface);
  }

  if(m_gbm->dev)
  {
    gbm_device_destroy(m_gbm->dev);
  }

  drmModeAtomicFree(m_drm->req);
}

bool CDRMAtomic::SetVideoMode(RESOLUTION_INFO res)
{
  m_drm->need_modeset = true;

  return true;
}
