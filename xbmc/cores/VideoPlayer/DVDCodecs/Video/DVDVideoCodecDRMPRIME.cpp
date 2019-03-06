/*
 *  Copyright (C) 2017-2018 Team Kodi
 *  This file is part of Kodi - https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSES/README.md for more information.
 */

#include "DVDVideoCodecDRMPRIME.h"

#include "ServiceBroker.h"
#include "cores/VideoPlayer/DVDCodecs/DVDFactoryCodec.h"
#include "cores/VideoPlayer/Process/gbm/VideoBufferDumb.h"
#include "cores/VideoPlayer/Process/gbm/VideoBufferDRMPRIME.h"
#include "settings/Settings.h"
#include "settings/SettingsComponent.h"
#include "settings/lib/Setting.h"
#include "threads/SingleLock.h"
#include "utils/CPUInfo.h"
#include "utils/log.h"
#include "windowing/gbm/WinSystemGbm.h"

extern "C" {
#include "libavcodec/avcodec.h"
#include "libavutil/pixdesc.h"
}

using namespace KODI::WINDOWING::GBM;

CDVDVideoCodecDRMPRIME::CDVDVideoCodecDRMPRIME(CProcessInfo& processInfo)
  : CDVDVideoCodec(processInfo)
{
  m_pFrame = av_frame_alloc();
  m_videoBufferPool = std::make_shared<CVideoBufferPoolDRMPRIME>();
  m_videoBufferPoolDumb = std::make_shared<CVideoBufferPoolDumb>();
}

CDVDVideoCodecDRMPRIME::~CDVDVideoCodecDRMPRIME()
{
  av_frame_free(&m_pFrame);
  avcodec_free_context(&m_pCodecContext);
}

CDVDVideoCodec* CDVDVideoCodecDRMPRIME::Create(CProcessInfo& processInfo)
{
//  if (CServiceBroker::GetSettingsComponent()->GetSettings()->GetBool(CSettings::SETTING_VIDEOPLAYER_USEPRIMEDECODER))
  return new CDVDVideoCodecDRMPRIME(processInfo);
  //return nullptr;
}

void CDVDVideoCodecDRMPRIME::Register()
{
  CServiceBroker::GetSettingsComponent()->GetSettings()->GetSetting(CSettings::SETTING_VIDEOPLAYER_USEPRIMEDECODER)->SetVisible(true);
  CDVDFactoryCodec::RegisterHWVideoCodec("drm_prime", CDVDVideoCodecDRMPRIME::Create);
}

static const AVCodecHWConfig* FindHWConfig(const AVCodec* codec)
{
  const AVCodecHWConfig* config = nullptr;
  for (int n = 0; (config = avcodec_get_hw_config(codec, n)); n++)
  {
    if (config->pix_fmt != AV_PIX_FMT_DRM_PRIME)
      continue;

    if ((config->methods & AV_CODEC_HW_CONFIG_METHOD_HW_DEVICE_CTX) &&
        config->device_type == AV_HWDEVICE_TYPE_DRM)
      return config;

    if ((config->methods & AV_CODEC_HW_CONFIG_METHOD_INTERNAL))
      return config;
  }

  return nullptr;
}

static const AVCodec* FindDecoder(CDVDStreamInfo& hints)
{
  const AVCodec* codec = nullptr;
  void *i = 0;

  while ((codec = av_codec_iterate(&i)))
  {
    if (!av_codec_is_decoder(codec))
      continue;
    if (codec->id == hints.codec)
      return codec;
  }

  return nullptr;
}

static enum AVPixelFormat GetFormat(struct AVCodecContext* avctx, const enum AVPixelFormat* fmt)
{
  for (int n = 0; fmt[n] != AV_PIX_FMT_NONE; n++)
    if (fmt[n] == AV_PIX_FMT_DRM_PRIME)
      return fmt[n];

  return AV_PIX_FMT_NONE;
}

void CDVDVideoCodecDRMPRIME::FFReleaseBuffer(void *opaque, uint8_t *data)
{
  CVideoBufferDumb* buffer = static_cast<CVideoBufferDumb*>(opaque);
  AVDRMFrameDescriptor* descriptor = buffer->GetDescriptor();
  CLog::Log(LOGDEBUG, "CDVDVideoCodecDRMPRIME::{} id={} descriptor={}", __FUNCTION__, buffer->GetId(), fmt::ptr(descriptor));
  buffer->Release();
}

int CDVDVideoCodecDRMPRIME::FFGetBuffer(AVCodecContext *avctx, AVFrame *frame, int flags)
{
  CVideoBufferPoolDumb* pool = static_cast<CVideoBufferPoolDumb*>(avctx->opaque);

  CLog::Log(LOGDEBUG, "CDVDVideoCodecDRMPRIME::{} {}x{} format:{}:{} flags:{}", __FUNCTION__, frame->width, frame->height, frame->format, avctx->pix_fmt, flags);

  if ((avctx->codec && (avctx->codec->capabilities & AV_CODEC_CAP_DR1) == 0))
  {
    CLog::Log(LOGDEBUG, "CDVDVideoCodecDRMPRIME::{} error caps", __FUNCTION__);
    return avcodec_default_get_buffer2(avctx, frame, flags);
  }

  if (!pool->IsConfigured())
  {
    int aligned_width = frame->width;
    int aligned_height = frame->height;

    aligned_height = FFALIGN(aligned_height, 16);

    pool->Configure(avctx->pix_fmt, aligned_width, aligned_height);
  }

  CVideoBufferDumb* buffer = static_cast<CVideoBufferDumb*>(pool->Get());
  if (!buffer)
  {
    CLog::Log(LOGERROR,"CDVDVideoCodecDRMPRIME::{} Failed to allocated buffer in time", __FUNCTION__);
    return -1;
  }

  AVDRMFrameDescriptor* descriptor = buffer->GetDescriptor();

  AVBufferRef *buf = av_buffer_create(reinterpret_cast<uint8_t*>(descriptor), sizeof(*descriptor), CDVDVideoCodecDRMPRIME::FFReleaseBuffer, buffer, AV_BUFFER_FLAG_READONLY);
  if (!buf)
  {
    CLog::Log(LOGERROR, "CDVDVideoCodecDRMPRIME::{} av_buffer_create() failed", __FUNCTION__);
    buffer->Release();
    return -1;
  }

  YuvImage image = {};
  buffer->GetPlanes(image.plane);
  buffer->GetStrides(image.stride);

  for (int i = 0; i < AV_NUM_DATA_POINTERS; i++)
  {
    frame->data[i] = i < YuvImage::MAX_PLANES ? image.plane[i] : nullptr;
    frame->linesize[i] = i < YuvImage::MAX_PLANES ? image.stride[i] : 0;
    frame->buf[i] = i == 0 ? buf : nullptr;
  }

  CLog::Log(LOGDEBUG, "CDVDVideoCodecDRMPRIME::{} id={} descriptor={}", __FUNCTION__, buffer->GetId(), fmt::ptr(descriptor));

  frame->extended_data = frame->data;

  return 0;
}

bool CDVDVideoCodecDRMPRIME::Open(CDVDStreamInfo& hints, CDVDCodecOptions& options)
{
  const AVCodec* pCodec = FindDecoder(hints);
  if (!pCodec)
  {
    CLog::Log(LOGDEBUG, "CDVDVideoCodecDRMPRIME::%s - unable to find decoder for codec %d", __FUNCTION__, hints.codec);
    return false;
  }

  CLog::Log(LOGNOTICE, "CDVDVideoCodecDRMPRIME::%s - using decoder %s", __FUNCTION__, pCodec->long_name ? pCodec->long_name : pCodec->name);

  m_pCodecContext = avcodec_alloc_context3(pCodec);
  if (!m_pCodecContext)
    return false;

  const AVCodecHWConfig* pConfig = FindHWConfig(pCodec);
  if (pConfig &&
      (pConfig->methods & AV_CODEC_HW_CONFIG_METHOD_HW_DEVICE_CTX) &&
      pConfig->device_type == AV_HWDEVICE_TYPE_DRM)
  {
    CWinSystemGbm* winSystem = dynamic_cast<CWinSystemGbm*>(CServiceBroker::GetWinSystem());
    if (av_hwdevice_ctx_create(&m_pCodecContext->hw_device_ctx, AV_HWDEVICE_TYPE_DRM, drmGetDeviceNameFromFd2(winSystem->GetDrm()->GetFileDescriptor()), nullptr, 0) < 0)
    {
      CLog::Log(LOGNOTICE, "CDVDVideoCodecDRMPRIME::%s - unable to create hwdevice context", __FUNCTION__);
      avcodec_free_context(&m_pCodecContext);
      return false;
    }

    m_pCodecContext->pix_fmt = AV_PIX_FMT_DRM_PRIME;
    m_pCodecContext->get_format = GetFormat;
  }
  else
  {
    m_pCodecContext->get_buffer2 = FFGetBuffer;
    m_pCodecContext->opaque = static_cast<void*>(m_videoBufferPoolDumb.get());

    int num_threads = g_cpuInfo.getCPUCount() * 3 / 2;
    num_threads = std::max(1, std::min(num_threads, 16));
    m_pCodecContext->thread_count = num_threads;
    m_pCodecContext->thread_safe_callbacks = 1;
    CLog::Log(LOGDEBUG, "CDVDVideoCodecDRMPRIME - open frame threaded with %d threads", num_threads);
  }

  m_pCodecContext->codec_tag = hints.codec_tag;
  m_pCodecContext->coded_width = hints.width;
  m_pCodecContext->coded_height = hints.height;
  m_pCodecContext->bits_per_coded_sample = hints.bitsperpixel;
  m_pCodecContext->time_base.num = 1;
  m_pCodecContext->time_base.den = DVD_TIME_BASE;

  if (hints.extradata && hints.extrasize > 0)
  {
    m_pCodecContext->extradata_size = hints.extrasize;
    m_pCodecContext->extradata = (uint8_t*)av_mallocz(hints.extrasize + AV_INPUT_BUFFER_PADDING_SIZE);
    memcpy(m_pCodecContext->extradata, hints.extradata, hints.extrasize);
  }

  if (avcodec_open2(m_pCodecContext, pCodec, nullptr) < 0)
  {
    CLog::Log(LOGNOTICE, "CDVDVideoCodecDRMPRIME::%s - unable to open codec", __FUNCTION__);
    avcodec_free_context(&m_pCodecContext);
    return false;
  }

  // if (m_pCodecContext->pix_fmt != AV_PIX_FMT_DRM_PRIME)
  // {
  //   CLog::Log(LOGNOTICE, "CDVDVideoCodecDRMPRIME::%s - unexpected pix fmt %s", __FUNCTION__, av_get_pix_fmt_name(m_pCodecContext->pix_fmt));
  //   avcodec_free_context(&m_pCodecContext);
  //   return false;
  // }

  const char* pixFmtName = av_get_pix_fmt_name(m_pCodecContext->pix_fmt);
  m_processInfo.SetVideoPixelFormat(pixFmtName ? pixFmtName : "");
  m_processInfo.SetVideoDimensions(hints.width, hints.height);
  m_processInfo.SetVideoDeintMethod("none");
  m_processInfo.SetVideoDAR(hints.aspect);

  if (pCodec->name)
    m_name = std::string("ff-") + pCodec->name;
  else
    m_name = "ffmpeg";

  m_processInfo.SetVideoDecoderName(m_name, true);

  return true;
}

bool CDVDVideoCodecDRMPRIME::AddData(const DemuxPacket& packet)
{
  if (!m_pCodecContext)
    return true;

  AVPacket avpkt;
  av_init_packet(&avpkt);
  avpkt.data = packet.pData;
  avpkt.size = packet.iSize;
  avpkt.dts = (packet.dts == DVD_NOPTS_VALUE) ? AV_NOPTS_VALUE : static_cast<int64_t>(packet.dts / DVD_TIME_BASE * AV_TIME_BASE);
  avpkt.pts = (packet.pts == DVD_NOPTS_VALUE) ? AV_NOPTS_VALUE : static_cast<int64_t>(packet.pts / DVD_TIME_BASE * AV_TIME_BASE);
  avpkt.side_data = static_cast<AVPacketSideData*>(packet.pSideData);
  avpkt.side_data_elems = packet.iSideDataElems;

  int ret = avcodec_send_packet(m_pCodecContext, &avpkt);
  if (ret == AVERROR(EAGAIN))
    return false;
  else if (ret == AVERROR_EOF)
    return true;
  else if (ret)
  {
    CLog::Log(LOGERROR, "CDVDVideoCodecDRMPRIME::%s - send packet failed, ret:%d", __FUNCTION__, ret);
    return false;
  }

  return true;
}

void CDVDVideoCodecDRMPRIME::Reset()
{
  if (!m_pCodecContext)
    return;

  avcodec_flush_buffers(m_pCodecContext);
  av_frame_unref(m_pFrame);
  m_codecControlFlags = 0;
}

void CDVDVideoCodecDRMPRIME::Drain()
{
  AVPacket avpkt;
  av_init_packet(&avpkt);
  avpkt.data = nullptr;
  avpkt.size = 0;
  avcodec_send_packet(m_pCodecContext, &avpkt);
}

void CDVDVideoCodecDRMPRIME::SetPictureParams(VideoPicture* pVideoPicture)
{
  pVideoPicture->iWidth = m_pFrame->width;
  pVideoPicture->iHeight = m_pFrame->height;

  double aspect_ratio = 0;
  AVRational pixel_aspect = m_pFrame->sample_aspect_ratio;
  if (pixel_aspect.num)
    aspect_ratio = av_q2d(pixel_aspect) * pVideoPicture->iWidth / pVideoPicture->iHeight;

  if (aspect_ratio <= 0.0)
    aspect_ratio = (float)pVideoPicture->iWidth / (float)pVideoPicture->iHeight;

  pVideoPicture->iDisplayWidth = ((int)lrint(pVideoPicture->iHeight * aspect_ratio)) & -3;
  pVideoPicture->iDisplayHeight = pVideoPicture->iHeight;
  if (pVideoPicture->iDisplayWidth > pVideoPicture->iWidth)
  {
    pVideoPicture->iDisplayWidth = pVideoPicture->iWidth;
    pVideoPicture->iDisplayHeight = ((int)lrint(pVideoPicture->iWidth / aspect_ratio)) & -3;
  }

  pVideoPicture->color_range = m_pFrame->color_range == AVCOL_RANGE_JPEG ? 1 : 0;
  pVideoPicture->color_primaries = m_pFrame->color_primaries;
  pVideoPicture->color_transfer = m_pFrame->color_trc;
  pVideoPicture->color_space = m_pFrame->colorspace;

  pVideoPicture->iRepeatPicture = 0;
  pVideoPicture->iFlags = 0;
  pVideoPicture->iFlags |= m_pFrame->interlaced_frame ? DVP_FLAG_INTERLACED : 0;
  pVideoPicture->iFlags |= m_pFrame->top_field_first ? DVP_FLAG_TOP_FIELD_FIRST: 0;
  pVideoPicture->iFlags |= m_pFrame->data[0] ? 0 : DVP_FLAG_DROPPED;

  int64_t pts = m_pFrame->pts;
  if (pts == AV_NOPTS_VALUE)
    pts = m_pFrame->best_effort_timestamp;
  pVideoPicture->pts = (pts == AV_NOPTS_VALUE) ? DVD_NOPTS_VALUE : (double)pts * DVD_TIME_BASE / AV_TIME_BASE;
  pVideoPicture->dts = DVD_NOPTS_VALUE;
}

CDVDVideoCodec::VCReturn CDVDVideoCodecDRMPRIME::GetPicture(VideoPicture* pVideoPicture)
{
  if (m_codecControlFlags & DVD_CODEC_CTRL_DRAIN)
    Drain();

  int ret = avcodec_receive_frame(m_pCodecContext, m_pFrame);
  if (ret == AVERROR(EAGAIN))
    return VC_BUFFER;
  else if (ret == AVERROR_EOF)
    return VC_EOF;
  else if (ret)
  {
    CLog::Log(LOGERROR, "CDVDVideoCodecDRMPRIME::%s - receive frame failed, ret:%d", __FUNCTION__, ret);
    return VC_ERROR;
  }

  if (pVideoPicture->videoBuffer)
    pVideoPicture->videoBuffer->Release();
  pVideoPicture->videoBuffer = nullptr;

  SetPictureParams(pVideoPicture);

  AVDRMFrameDescriptor* descriptor = reinterpret_cast<AVDRMFrameDescriptor*>(m_pFrame->buf[0]->data);

  CLog::Log(LOGDEBUG, "CDVDVideoCodecDRMPRIME::{} - descriptor={}", __FUNCTION__, fmt::ptr(descriptor));

  CVideoBufferDRMPRIME* buffer = dynamic_cast<CVideoBufferDRMPRIME*>(m_videoBufferPool->Get());
  buffer->SetRef(m_pFrame);
  pVideoPicture->videoBuffer = buffer;

  return VC_PICTURE;
}
