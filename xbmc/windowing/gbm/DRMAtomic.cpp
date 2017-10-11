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

bool CDRMAtomic::AddPlaneProperty(drmModeAtomicReq *req, int obj_id, const char *name, int value)
{
  struct plane *obj = m_drm->plane;
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

  auto ret = drmModeAtomicAddProperty(req, obj_id, prop_id, value);
  if (ret < 0)
  {
    return false;
  }

  return true;
}

bool CDRMAtomic::DrmAtomicCommit(int fb_id, int flags)
{
  drmModeAtomicReq *req;
  int plane_id = m_drm->plane->plane->plane_id;
  uint32_t blob_id;

  req = drmModeAtomicAlloc();

  if (flags & DRM_MODE_ATOMIC_ALLOW_MODESET)
  {
    if (!AddConnectorProperty(req, m_drm->connector_id, "CRTC_ID", m_drm->crtc_id))
    {
      return false;
    }

    if (drmModeCreatePropertyBlob(m_drm->fd, m_drm->mode, sizeof(*m_drm->mode), &blob_id) != 0)
    {
      return false;
    }

    if (!AddCrtcProperty(req, m_drm->crtc_id, "MODE_ID", blob_id))
    {
      return false;
    }

    if (!AddCrtcProperty(req, m_drm->crtc_id, "ACTIVE", 1))
    {
      return false;
    }
  }

  AddPlaneProperty(req, plane_id, "FB_ID", fb_id);
  AddPlaneProperty(req, plane_id, "CRTC_ID", m_drm->crtc_id);
  AddPlaneProperty(req, plane_id, "SRC_X", 0);
  AddPlaneProperty(req, plane_id, "SRC_Y", 0);
  AddPlaneProperty(req, plane_id, "SRC_W", m_drm->mode->hdisplay << 16);
  AddPlaneProperty(req, plane_id, "SRC_H", m_drm->mode->vdisplay << 16);
  AddPlaneProperty(req, plane_id, "CRTC_X", 0);
  AddPlaneProperty(req, plane_id, "CRTC_Y", 0);
  AddPlaneProperty(req, plane_id, "CRTC_W", m_drm->mode->hdisplay);
  AddPlaneProperty(req, plane_id, "CRTC_H", m_drm->mode->vdisplay);

  if (m_drm->kms_in_fence_fd != -1)
  {
    AddCrtcProperty(req, m_drm->crtc_id, "OUT_FENCE_PTR", (uint64_t)(unsigned long)&m_drm->kms_out_fence_fd);
    AddPlaneProperty(req, plane_id, "IN_FENCE_FD", m_drm->kms_in_fence_fd);
  }

  auto ret = drmModeAtomicCommit(m_drm->fd, req, flags, nullptr);
  if (ret)
  {
    return false;
  }

  if (m_drm->kms_in_fence_fd != -1)
  {
    close(m_drm->kms_in_fence_fd);
    m_drm->kms_in_fence_fd = -1;
  }

  return true;
}

void CDRMAtomic::FlipPage(CGLContextEGL *pGLContext)
{
  int flags = DRM_MODE_ATOMIC_NONBLOCK;

  pGLContext->CreateGPUFence();
  m_drm->kms_in_fence_fd = pGLContext->FlushFence();
  pGLContext->WaitSyncCPU();

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

  pGLContext->CreateKMSFence(m_drm->kms_out_fence_fd);
  pGLContext->WaitSyncGPU();
  m_drm->kms_out_fence_fd = -1;

  if(g_Windowing.NoOfBuffers() > 2 && gbm_surface_has_free_buffers(m_gbm->surface))
  {
    return;
  }

  gbm_surface_release_buffer(m_gbm->surface, m_bo);
  m_bo = m_next_bo;
}

int CDRMAtomic::GetPlaneId()
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

        if ((strcmp(p->name, "type") == 0) && (props->prop_values[j] == DRM_PLANE_TYPE_PRIMARY))
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

  m_drm->kms_out_fence_fd = -1;

  if (!CDRMUtils::InitDrm(m_drm))
  {
    return false;
  }

  ret = drmSetClientCap(m_drm->fd, DRM_CLIENT_CAP_ATOMIC, 1);
  if (ret)
  {
    CLog::Log(LOGERROR, "CDRMAtomic::%s - no atomic modesetting support: %s", __FUNCTION__, strerror(errno));
    return false;
  }

  ret = GetPlaneId();
  if (!ret)
  {
    CLog::Log(LOGERROR, "CDRMAtomic::%s - could not find a suitable plane", __FUNCTION__);
    return false;
  }
  else
  {
    plane_id = ret;
  }

  /* We only do single plane to single crtc to single connector, no
     * fancy multi-monitor or multi-plane stuff.  So just grab the
     * plane/crtc/connector property info for one of each:
     */
  m_drm->plane = new plane;
  m_drm->crtc = new crtc;
  m_drm->connector = new connector;

  // plane
  m_drm->plane->plane = drmModeGetPlane(m_drm->fd, plane_id);
  if (!m_drm->plane->plane)
  {
    CLog::Log(LOGERROR, "CDRMAtomic::%s - could not get plane %i: %s", __FUNCTION__, plane_id, strerror(errno));
    return false;
  }

  m_drm->plane->props = drmModeObjectGetProperties(m_drm->fd, plane_id, DRM_MODE_OBJECT_PLANE);
  if (!m_drm->plane->props)
  {
    CLog::Log(LOGERROR, "CDRMAtomic::%s - could not get plane %u properties: %s", __FUNCTION__, plane_id, strerror(errno));
    return false;
  }

  m_drm->plane->props_info = new drmModePropertyPtr;
  for (uint32_t i = 0; i < m_drm->plane->props->count_props; i++)
  {
    m_drm->plane->props_info[i] = drmModeGetProperty(m_drm->fd, m_drm->plane->props->props[i]);             \
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

  //
  m_gbm->dev = gbm_create_device(m_drm->fd);

  if (!CGBMUtils::InitGbm(m_gbm, m_drm->mode->hdisplay, m_drm->mode->vdisplay))
  {
    return false;
  }

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
}

bool CDRMAtomic::SetVideoMode(RESOLUTION_INFO res)
{
  CDRMUtils::GetMode(res);

  gbm_surface_release_buffer(m_gbm->surface, m_bo);
  m_bo = m_next_bo;

  m_next_bo = gbm_surface_lock_front_buffer(m_gbm->surface);
  if (!m_next_bo)
  {
    CLog::Log(LOGERROR, "CDRMAtomic::%s - Failed to lock frontbuffer", __FUNCTION__);
    return false;
  }

  m_drm_fb = CDRMUtils::DrmFbGetFromBo(m_next_bo);
  if (!m_drm_fb)
  {
    CLog::Log(LOGERROR, "CDRMAtomic::%s - Failed to get a new FBO", __FUNCTION__);
    return false;
  }

  auto ret = DrmAtomicCommit(m_drm_fb->fb_id, DRM_MODE_ATOMIC_ALLOW_MODESET);
  if (!ret)
  {
    CLog::Log(LOGERROR, "CDRMAtomic::%s - failed to commit modeset: %s", __FUNCTION__, strerror(errno));
    return false;
  }

  return true;
}
