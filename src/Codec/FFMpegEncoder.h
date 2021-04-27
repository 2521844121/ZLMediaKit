/*
 * Copyright (c) 2016 The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/xiongziliang/ZLMediaKit).
 *
 * Use of this source code is governed by MIT license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#ifndef H264Encoder_H_
#define H264Encoder_H_
#include <string>
#include <memory>
#include <stdexcept>
#include <stdio.h>
#include <string.h>"

#ifdef WIN32
#pragma comment(lib, "libavcodec.a")
#pragma comment(lib, "libavdevice.a")
#pragma comment(lib, "libavfilter.a")
#pragma comment(lib, "libavformat.a")
#pragma comment(lib, "libavutil.a")
#pragma comment(lib, "libswresample.a")
#pragma comment(lib, "libavresample.a")
#pragma comment(lib, "libswscale.a")
#pragma comment(lib, "bcrypt.lib")
#pragma comment(lib, "libx264.lib")
#endif //  WIN32

#ifdef __cplusplus
extern "C" {
#endif
#include "libavcodec/avcodec.h"
#include "libavutil/opt.h"
#include "libavcodec/avcodec.h"
#include "libavformat/avformat.h"
#ifdef __cplusplus
}
#endif

using namespace std;
namespace mediakit 
{
	class FFMpegEncoder {
	public:
		//encoders: libx264 libx264rgb h264_amf h264_mf h264_nvenc h264_qsv nvenc nvenc_h264
		//			libx265 nvenc_hevc hevc_amf hevc_mf hevc_nvenc hevc_qsv
		FFMpegEncoder(char* codec_name, int width = 1920, int height = 1080, int bitrate = 2048000, int frameRage = 25, int gopSize = 50)
		{
			//auto ff_codec_id = AV_CODEC_ID_H264;
			//switch (codec_id) {
			//case CodecH264:
			//	ff_codec_id = AV_CODEC_ID_H264;
			//	break;
			//case CodecH265:
			//	ff_codec_id = AV_CODEC_ID_H265;
			//	break;
			//default:
			//	throw std::invalid_argument("不支持该编码格式");
			//}

			avcodec_register_all();
			av_log_set_level(AV_LOG_INFO);

			//AVCodec *pCodec = avcodec_find_encoder(ff_codec_id);
			AVCodec *pCodec = avcodec_find_encoder_by_name(codec_name);
			if (!pCodec) {
				throw std::runtime_error("未找到编码器");
			}
			m_pContext.reset(avcodec_alloc_context3(pCodec), [](AVCodecContext *pCtx) {
				avcodec_close(pCtx);
				avcodec_free_context(&pCtx);
			});
			if (!m_pContext) {
				throw std::runtime_error("创建编码器失败");
			}
			if (pCodec->capabilities & AV_CODEC_CAP_TRUNCATED) {
				/* we do not send complete frames */
				m_pContext->flags |= AV_CODEC_FLAG_TRUNCATED;
			}

			m_packet.reset(av_packet_alloc(), [](AVPacket *pPackage) {
				av_packet_free(&pPackage);
			});
			if (!m_packet) {
				throw std::runtime_error("创建AVPacket失败");
			}

			m_pContext->pix_fmt = AV_PIX_FMT_YUV420P; //AV_PIX_FMT_YUVJ420P;// AV_PIX_FMT_YUV420P;
			m_pContext->width = width;
			m_pContext->height = height;
			m_pContext->time_base.num = 1;
			m_pContext->time_base.den = frameRage;
			m_pContext->bit_rate = bitrate;
			m_pContext->gop_size = gopSize;
			m_pContext->qmin = 10;
			m_pContext->qmax = 51;
			m_pContext->max_b_frames = 0;

			//m_pContext->priv_data;


			// Set Option
			AVDictionary *param = 0;
			//H.264
			if (m_pContext->codec_id == AV_CODEC_ID_H264) {
				av_dict_set(&param, "preset", "slow", 0);
				av_dict_set(&param, "profile", "main", 0);
			}
			//H.265
			if ((m_pContext->codec_id == AV_CODEC_ID_HEVC) || (m_pContext->codec_id == AV_CODEC_ID_H265)) {
				av_dict_set(&param, "x265-params", "crf=25", 0);
				av_dict_set(&param, "preset", "fast", 0);
				av_dict_set(&param, "tune", "zero-latency", 0);


			}
#ifdef WIN32 //Linxu设置这个加速首帧速度会导致编码花瓶
			av_opt_set(m_pContext->priv_data, "tune", "zerolatency", 0);
#endif

			if (avcodec_open2(m_pContext.get(), pCodec, NULL) < 0) {
				throw std::runtime_error("打开编码器失败");
			}

			m_frame.reset(av_frame_alloc(), [](AVFrame *pFrame) {
				av_frame_free(&pFrame);
			});
			if (!m_frame) {
				throw std::runtime_error("创建AVFrame失败");
			}

			int size = avpicture_get_size(m_pContext->pix_fmt, m_pContext->width, m_pContext->height);
			uint8_t* picture_buf = (uint8_t *)av_malloc(size);
			avpicture_fill((AVPicture *)m_frame.get(), picture_buf, m_pContext->pix_fmt, m_pContext->width, m_pContext->height);
			m_frame->format = m_pContext->pix_fmt;
			m_frame->width = m_pContext->width;
			m_frame->height = m_pContext->height;

		}

		virtual ~FFMpegEncoder(void)
		{

		}


		//data 待编码yuv数据， ppFrame编码后数据
		//yv12格式。排列顺序“Y0 - Y1 - ……”，“V0 - V1….”，“U0 - U1 - …..”
		//需要注意这个排列YVU的顺序，否则编码后色彩会不正常
		bool encode(unsigned char* data, unsigned int dataSize, AVPacket **ppFrame, bool& isKeyFrame)
		{
			//data yuv转AVFrame
			//AVFrame* picture = av_frame_alloc();
			////int size = avpicture_get_size(encodeContext->pix_fmt, encodeContext->width, encodeContext->height);
			//int size = avpicture_get_size(m_pContext->pix_fmt, m_pContext->width, m_pContext->height);

			//uint8_t* picture_buf = (uint8_t *)av_malloc(size);
			//avpicture_fill((AVPicture *)picture, picture_buf, m_pContext->pix_fmt, m_pContext->width, m_pContext->height);
			//picture->width = m_pContext->pix_fmt;
			//picture->height = m_pContext->width;
			//picture->format = m_pContext->height;
			int y_size = m_pContext->width*m_pContext->height;
			m_frame->data[0] = data; //亮度Y
			m_frame->data[2] = data + y_size; //V
			m_frame->data[1] = data + y_size * 5 / 4; //U
			m_frame->width = m_pContext->width;
			m_frame->height = m_pContext->height;
			return encode(m_frame.get(), ppFrame, isKeyFrame);
		}

		bool encode(AVFrame *beforeEncode, AVPacket **afterEncode, bool& isKeyFrame)
		{			
			int srcWidth = beforeEncode->width;
			int srcHeight = beforeEncode->height;

			////转换分辨率
				//AVFrame *sws_frame;
			   ////scale frame
			   //if (sws_ctx == NULL
			   //	|| codecCtx->width != srcWidth
			   //	|| codecCtx->height != srcHeight) {
			   //	qDebug() << "video src: " << srcWidth << " " << ((AVFrame*)frameBeforeEn)->height
			   //		<< " video dst: " << codecCtx->width << " " << codecCtx->height;

			   //	if (sws_ctx != NULL) {
			   //		av_free(sws_ctx);
			   //		sws_ctx = NULL;
			   //	}

			   //	sws_ctx = sws_getContext(srcWidth, srcHeight, AV_PIX_FMT_YUV420P,
			   //		codecCtx->width, codecCtx->height, AV_PIX_FMT_YUV420P,
			   //		SWS_BILINEAR, NULL, NULL, NULL);

			   //	if (!sws_ctx) {
			   //		av_log(NULL, AV_LOG_INFO, "sws_getContext error!\n");
			   //		return false;
			   //	}
			   //}

			   //sws_frame = alloc_picture(AV_PIX_FMT_YUV420P,
			   //	codecCtx->width,
			   //	codecCtx->height);
			   //int ret = sws_scale(sws_ctx, ((AVFrame*)frameBeforeEn)->data,
			   //	((AVFrame*)frameBeforeEn)->linesize, 0, ((AVFrame*)frameBeforeEn)->height,
			   //	sws_frame->data, sws_frame->linesize);
			   //if (ret <= 0) {
			   //	qDebug() << "sws_scale error!" << ret;
			   //	return false;
			   //}


			int got_frame;
			//av_init_packet(&m_packet->get);
			//m_package.data = nullptr;
			//m_package.size = 0;

			memset(m_packet.get(), 0, sizeof(AVPacket));
			av_init_packet(m_packet.get());

			beforeEncode->pts = m_index++;
			int ret = avcodec_encode_video2(m_pContext.get(), m_packet.get(),
				beforeEncode, &got_frame);

			//av_free_packet(&m_package);
			if (ret < 0)
			{				
				char errLog[1024] = {0};;
				av_strerror(ret, errLog, 1024);
				printf("FFMpegEncoder::encode failed! [%d]%s \r\n" , ret, errLog);
				return false;
			}
			
			isKeyFrame = (m_pContext->coded_frame->key_frame == 1) && (m_pContext->coded_frame->pict_type == AV_PICTURE_TYPE_I);
			*afterEncode = m_packet.get();
			return got_frame == 1;
		}

	private:
		std::shared_ptr<AVCodecContext> m_pContext;
		std::shared_ptr<AVPacket>  m_packet;
		std::shared_ptr<AVFrame>  m_frame;
		int m_index = 0;
	};
}
#endif


