#include "mov-format.h"
#include "mov-buffer.h"
#include "mov-writer.h"
#include "fmp4-writer.h"
#include "mpeg4-aac.h"
#include "mpeg4-avc.h"
#include "mpeg4-hevc.h"
#include "mpeg4-aac.h"
#include "Extension/H265.h"
#include "Extension/H264.h"
#include "Extension/AAC.h"
#include "Extension/G711.h"
#include "Extension/Opus.h"
#include <memory>
#include "Fmp4Muxer.h"

namespace mediakit
{
	static int mov_live_read(void* param, void* data, uint64_t bytes)
	{
		return -1;
	}

	static int mov_live_write(void* param, const void* data, uint64_t bytes)
	{
		Fmp4Muxer *muxer = (Fmp4Muxer *)param;
		return muxer->handleWrite(data, bytes);
	}

	static int mov_live_seek(void* param, uint64_t offset)
	{
		Fmp4Muxer *muxer = (Fmp4Muxer *)param;
		return muxer->handleSeek(offset);
	}

	static uint64_t mov_live_tell(void* param)
	{
		Fmp4Muxer *muxer = (Fmp4Muxer *)param;
		return muxer->handleTell();
	}

	static void ftyp_write_completed(void* param)
	{
		Fmp4Muxer *muxer = (Fmp4Muxer *)param;
		return muxer->handleFtypwriteCompleted();
	}

	const struct mov_buffer_t* mov_file_buffer(void)
	{
		static struct mov_buffer_t s_io = {
			mov_live_read,
			mov_live_write,
			mov_live_seek,
			mov_live_tell,
			ftyp_write_completed,
		};
		return &s_io;
	}


	Fmp4Muxer::Fmp4Muxer(int width, int height, FragmentType type, int groupNum)
		:m_width(width), m_height(height)
	{
		m_writer = fmp4_writer_create(mov_file_buffer(), this, 0);
		isGotI = false;
		gotFtyp = false;
		m_curIndex = 0;
		m_FtypLen = 0;
		m_avc_track = -1;
		m_hevc_track = -1;
		m_aac_track = -1;
		if (type == FRAGMENT_PER_FRAME)
		{
			m_maxBufSize = FRAME_BUF_LEN;
		}
		else
		{
			m_maxBufSize = GOP_BUF_LEN;
		}
		m_buffer = (uint8_t*)malloc(m_maxBufSize);
		m_fType_buffer = (uint8_t*)malloc(FTYP_LEN);
	}

	Fmp4Muxer::~Fmp4Muxer()
	{
		fmp4_writer_destroy(m_writer);
		free(m_buffer);
		m_buffer = NULL;
		free(m_fType_buffer);
		m_fType_buffer = NULL;
	}

	int Fmp4Muxer::handleWrite(const void* data, uint64_t bytes)
	{
		if (bytes + m_curIndex < m_maxBufSize && m_buffer != NULL)
		{
			uint8_t *dest = m_buffer + m_curIndex;
			memcpy(dest, data, bytes);
			m_curIndex += bytes;
		}
		else
		{
			if (m_buffer != NULL)
				ErrorL << "[Fmp4Muxer::handleWrite] data too large! " << bytes << " " << m_curIndex;
			else
				ErrorL << "[Fmp4Muxer::handleWrite] buffer invilad! ";

			return -EIO;
		}

		return 0;
	}

	int Fmp4Muxer::handleSeek(uint64_t offset)
	{
		m_curIndex = offset;

		return 0;
	}

	uint64_t Fmp4Muxer::handleTell()
	{
		return m_curIndex;
	}


	int Fmp4Muxer::getStartCodeLen(unsigned char* nalu, int len)
	{
		if (nullptr == nalu
			|| len < 3)
		{
			return 0;
		}

		if (nalu[0] == 0x00 && nalu[1] == 0x00 && nalu[2] == 0x01)
		{
			return 3;
		}
		else if (nalu[0] == 0x00 && nalu[1] == 0x00 && nalu[2] == 0x00 && nalu[3] == 0x01)
		{
			return 4;
		}
		else
		{
			return 0;
		}
	}

	MSEErrCode Fmp4Muxer::mux(CodecId frameType, unsigned char* nalu, int len, unsigned int pts, unsigned int dts)
	{
		if (_codec_to_trackid.count(frameType) == 0)
		{
			ErrorL << "Fmp4Muxer::mux unknow frame type [" << frameType << "]";
			return MSEErrCode_UnknowCodecId;
		}

		int trackId = _codec_to_trackid[frameType].track_id;

		int ret = 0;
		if (frameType == CodecH264)
		{
			int startCodeLen = getStartCodeLen(nalu, len);
			if (startCodeLen == 0)
			{
				ErrorL << "Fmp4Muxer::mux invalidate start code!! len[" << len << "][" << (int)nalu[0] << "][" << (int)nalu[1] << "][" << (int)nalu[2] << "][" << (int)nalu[3] << "]";
				return MSEErrCode_InvalidStartCode;
			}

			int type = nalu[startCodeLen] & 0x1f;

			if (!isGotI && type != H264Frame::NAL_SPS)
			{
				ErrorL << "Fmp4Muxer::mux invalidate waitting i frame!!";
				return  MSEErrCode_NoKeyFrame;
			}


			if (type == H264Frame::NAL_SPS)
			{//SPS+PPS+IDR
				int i;
				int iFrameLen;
				unsigned char *iFrame = NULL;
				uint8_t *tmpBuffer = (uint8_t *)malloc(len + 20);

				if (!isGotI)
				{
					isGotI = true;

					int spsLen = 0, ppsLen = 0;
					unsigned char *sps = nalu + startCodeLen;
					for (i = 0; i < len; i++)
					{
						if (sps[i] == 0x00 && sps[i + 1] == 0x00 && sps[i + 2] == 0x01
							|| sps[i] == 0x00 && sps[i + 1] == 0x00 && sps[i + 2] == 0x00 && sps[i + 3] == 0x01)
						{
							spsLen = i;
							break;
						}
					}

					if (i == len)
					{
						ErrorL << "[Fmp4Muxer::mux] sps not found!";
						free(tmpBuffer);
						return MSEErrCode_NoSPS;
					}

					unsigned char *pps = sps + spsLen + startCodeLen;
					for (i = 0; i < len; i++)
					{
						if ((pps[i] == 0x00 && pps[i + 1] == 0x00 && pps[i + 2] == 0x01)// 00 00 01
							|| (pps[i] == 0x00 && pps[i + 1] == 0x00 && pps[i + 2] == 0x00 && pps[i + 3] == 0x01)
							)

						{
							ppsLen = i;
							break;
						}
					}

					if (i == len)
					{
						ErrorL << "[Fmp4Muxer::mux] pps not found!";
						free(tmpBuffer);
						return MSEErrCode_NoPPS;
					}

					//if (-1 == m_avc_track)
					//{
					//	uint8_t *p = tmpBuffer;
					//	*p++ = 0x01;
					//	*p++ = sps[1];
					//	*p++ = sps[2];
					//	*p++ = sps[3];
					//	*p++ = 0xFF; // lengthSizeMinusOne: 3, nalu length is 4 bytes
					//	*p++ = 0xE1;
					//	*p++ = (spsLen >> 8) & 0xFF;
					//	*p++ = spsLen & 0xFF;
					//	memcpy(p, sps, spsLen);
					//	p += spsLen;

					//	*p++ = 0x01;
					//	*p++ = (ppsLen >> 8) & 0xFF;
					//	*p++ = ppsLen & 0xFF;
					//	memcpy(p, pps, ppsLen);

					//	int bufSize = spsLen + ppsLen + 11;

					//	m_avc_track = fmp4_writer_add_video(m_writer, MOV_OBJECT_H264, m_width, m_height, tmpBuffer, bufSize);
					//}
				}

				for (i = 0; i < len; i++)
				{
					if (nalu[i] == 0x00 && nalu[i + 1] == 0x00 && nalu[i + 2] == 0x00 && nalu[i + 3] == 0x01 && (nalu[i + 4] & 0x1f) == H264Frame::NAL_IDR)
					{
						iFrame = nalu + i + 4;
						iFrameLen = len - i - 4;
						break;
					}
					else if (nalu[i] == 0x00 && nalu[i + 1] == 0x00 && nalu[i + 2] == 0x01 && (nalu[i + 3] & 0x1f) == H264Frame::NAL_IDR)
					{
						iFrame = nalu + i + 3;
						iFrameLen = len - i - 3;
						break;
					}
					else
					{//PPS和IDR之间有其他NAL单元?
						WarnL << "[Fmp4Muxer::mux] unknow nal type[" << (nalu[i + 4] & 0x1f) << "] befor IDR!";
					}
				}

				if (i < len)
				{
					uint8_t *p = tmpBuffer;
					*p++ = (iFrameLen >> 24) & 0xFF;
					*p++ = (iFrameLen >> 16) & 0xFF;
					*p++ = (iFrameLen >> 8) & 0xFF;
					*p++ = iFrameLen & 0xFF;

					memcpy(p, iFrame, iFrameLen);
					ret = fmp4_writer_write(m_writer, trackId, tmpBuffer, iFrameLen + 4, pts, dts, MOV_AV_FLAG_KEYFREAME);
				}
				else
				{
					ErrorL << "[Fmp4Muxer::mux] idr not found!";
					free(tmpBuffer);
					return MSEErrCode_NoIDR;
				}

				free(tmpBuffer);
			}
			else if (type == H264Frame::NAL_B_P || type == H264Frame::NAL_IDR)
			{
				if (type == H264Frame::NAL_IDR)
				{
					ErrorL << "[Fmp4Muxer::mux] idr without sps and pps!";
				}

				if (nalu[0] == 0x00 && nalu[1] == 0x00 && nalu[2] == 0x00 && nalu[3] == 0x01)
				{
					int pFrameLen = len - 4;
					nalu[0] = (pFrameLen >> 24) & 0xFF;
					nalu[1] = (pFrameLen >> 16) & 0xFF;
					nalu[2] = (pFrameLen >> 8) & 0xFF;
					nalu[3] = pFrameLen & 0xFF;

					ret = fmp4_writer_write(m_writer, trackId, nalu, len, pts, dts, MOV_AV_FLAG_KEYFREAME);

					nalu[0] = 0x00;
					nalu[1] = 0x00;
					nalu[2] = 0x00;
					nalu[3] = 0x01;
				}
				else if (nalu[0] == 0x00 && nalu[1] == 0x00 && nalu[2] == 0x01)
				{// 00 00 01
					uint8_t *tmpBuffer = (uint8_t *)malloc(len + 20);
					int pFrameLen = len - 3;
					tmpBuffer[0] = (pFrameLen >> 24) & 0xFF;
					tmpBuffer[1] = (pFrameLen >> 16) & 0xFF;
					tmpBuffer[2] = (pFrameLen >> 8) & 0xFF;
					tmpBuffer[3] = pFrameLen & 0xFF;
					memcpy(tmpBuffer + 4, nalu + 3, len - 3);
					ret = fmp4_writer_write(m_writer, trackId, tmpBuffer, len + 1/*去除的nal 00 00 01，但是新增了 4字节fmp4头*/, pts, dts, MOV_AV_FLAG_KEYFREAME);
					free(tmpBuffer);
				}
			}
			else
			{
				ErrorL << "[Fmp4Muxer::mux] data type unsupport!" << type;
				return  MSEErrCode_UnknowNalType;;
			}
			return MSEErrCode_Success;
		}
		else if (frameType == CodecH265)
		{
			if (nalu[0] == 0x00 && nalu[1] == 0x00 && nalu[2] == 0x01)
			{
				ErrorL << "[Fmp4Muxer::mux] h265 unsupport 00 00 01";
				return MSEErrCode_InvalidStartCode;
			}
			uint8_t nalutype = (nalu[4] >> 1) & 0x3f;

			if (!isGotI && nalutype != H265Frame::NAL_VPS)
				return  MSEErrCode_NoKeyFrame;

			if (nalutype == H265Frame::NAL_VPS)
			{
				int idx = 0;
				int iFrameLen;
				unsigned char *iFrame = NULL;
				uint8_t *tmpBuffer = (uint8_t *)malloc(len + 20);

				for (; idx < len; idx++)
				{
					if (nalu[idx] == 0x00 && nalu[idx + 1] == 0x00 && nalu[idx + 2] == 0x00 && nalu[idx + 3] == 0x01)
					{
						uint8_t type = (nalu[idx + 4] >> 1) & 0x3f;
						if (type == H265Frame::NAL_VPS || type == H265Frame::NAL_SPS || type == H265Frame::NAL_PPS)
						{
							continue;
						}
						else
						{
							break;
						}
					}
				}

				if (!isGotI)
				{
					isGotI = true;
					memcpy(tmpBuffer, nalu, idx);
					if (-1 == m_hevc_track)
					{
						char out[1024] = { 0 };
						unsigned char data[1024];
						int vcl;
						struct mpeg4_hevc_t hevc;
						//h265_annexbtomp4(&hevc, tmpBuffer, idx, out, 1024, &vcl);
						int hdcrSize = mpeg4_hevc_decoder_configuration_record_save(&hevc, data, sizeof(data));
						m_hevc_track = fmp4_writer_add_video(m_writer, MOV_OBJECT_HEVC, m_width, m_height, data, hdcrSize);
					}
				}

				for (; idx < len; idx++)
				{
					if (nalu[idx] == 0x00 && nalu[idx + 1] == 0x00 && nalu[idx + 2] == 0x00 && nalu[idx + 3] == 0x01)
					{
						uint8_t type = (nalu[idx + 4] >> 1) & 0x3f;
						if (type == H265Frame::NAL_IDR_W_RADL)
						{
							iFrame = nalu + idx + 4;
							iFrameLen = len - idx - 4;
							break;
						}
					}
				}

				if (idx < len)
				{
					uint8_t *p = tmpBuffer;
					*p++ = (iFrameLen >> 24) & 0xFF;
					*p++ = (iFrameLen >> 16) & 0xFF;
					*p++ = (iFrameLen >> 8) & 0xFF;
					*p++ = iFrameLen & 0xFF;

					memcpy(p, iFrame, iFrameLen);
					ret = fmp4_writer_write(m_writer, m_hevc_track, tmpBuffer, iFrameLen + 4, pts, dts, MOV_AV_FLAG_KEYFREAME);
				}
				else
				{
					ErrorL << "[Fmp4Muxer::mux] NO h265 IDR!";
					free(tmpBuffer);
					return MSEErrCode_NoIDR;
				}

				free(tmpBuffer);
			}
			else if (nalutype == H265Frame::NAL_TRAIL_R)
			{
				if (nalu[0] == 0x00 && nalu[1] == 0x00 && nalu[2] == 0x00 && nalu[3] == 0x01)
				{
					int pFrameLen = len - 4;
					nalu[0] = (pFrameLen >> 24) & 0xFF;
					nalu[1] = (pFrameLen >> 16) & 0xFF;
					nalu[2] = (pFrameLen >> 8) & 0xFF;
					nalu[3] = pFrameLen & 0xFF;

					ret = fmp4_writer_write(m_writer, m_hevc_track, nalu, len, pts, dts, MOV_AV_FLAG_KEYFREAME);

					nalu[0] = 0x00;
					nalu[1] = 0x00;
					nalu[2] = 0x00;
					nalu[3] = 0x01;
				}
			}
			else
			{
				ErrorL << "[Fmp4Muxer::mux] nalu type unsupport!" << nalutype;
				return MSEErrCode_UnknowNalType;
			}
		}
		else if (frameType == CodecAAC)
		{
			if (-1 == m_aac_track)
				return MSEErrCode_NoKeyFrame;

			if (!isGotI)
				return  MSEErrCode_NoKeyFrame;

			int framelen = ((nalu[3] & 0x03) << 11) | (nalu[4] << 3) | (nalu[5] >> 5);
			if (len != framelen)
			{
				ErrorL << "[Fmp4Muxer::mux] len != framelen!";
			}

			//char audBuf[2048] = { 0 };
			//int pFrameLen = len - 7;
			//audBuf[0] = (pFrameLen >> 24) & 0xFF;
			//audBuf[1] = (pFrameLen >> 16) & 0xFF;
			//audBuf[2] = (pFrameLen >> 8) & 0xFF;
			//audBuf[3] = pFrameLen & 0xFF;
			//memcpy(audBuf + 4, nalu + 7, pFrameLen);
			ret = fmp4_writer_write(m_writer, m_aac_track, nalu + 7, len - 7, pts, dts, MOV_AV_FLAG_KEYFREAME);
		}

		if (ret < 0)
		{
			//发生错误,缓冲清空
			ErrorL << "Fmp4Muxer::mux error";
			memset(m_buffer, 0, m_maxBufSize);
			m_curIndex = 0;
		}

		return MSEErrCode_Success;
	}

	void Fmp4Muxer::handleFtypwriteCompleted()
	{
		gotFtyp = true;
		if (m_curIndex > FTYP_LEN)
		{
			ErrorL << "[Fmp4Muxer::handleFtypwriteCompleted] Ftyp header too large!" << m_curIndex;
		}
		else
		{
			memcpy(m_fType_buffer, m_buffer, m_curIndex);
			m_FtypLen = m_curIndex;
		}
		memset(m_buffer, 0, m_maxBufSize);
		m_curIndex = 0;
	}

	MSEErrCode Fmp4Muxer::inputFrame(const Frame::Ptr &frame) {
		//if (frame->getCodecId() != CodecH264)
		//{//for test
		//	return MSEErrCode_UnknowCodecId;
		//}

		auto it = _codec_to_trackid.find(frame->getCodecId());
		if (it == _codec_to_trackid.end()) {
			//该Track不存在或初始化失败
			ErrorL << "Fmp4Muxer::inputFrame " << frame->getCodecId() << " not support!";
			return MSEErrCode_UnknowCodecId;
		}

		if (!_started) {
			//该逻辑确保含有视频时，第一帧为关键帧
			if (_have_video && !frame->keyFrame()) {
				//含有视频，但是不是关键帧，那么前面的帧丢弃
				return MSEErrCode_NoKeyFrame;
			}
			//开始写文件
			_started = true;
		}

		//mp4文件时间戳需要从0开始
		auto &track_info = it->second;
		int64_t dts_out = 0, pts_out =0;

		switch (frame->getCodecId())
		{
		case CodecH264: {
			int type = H264_TYPE(*((uint8_t *)frame->data() + frame->prefixSize()));
			if (type == H264Frame::NAL_SEI) {
				WarnL << "Fmp4Muxer::inputFrame nal " << type << " not support!";
				return MSEErrCode_UnknowNalType;
			}
		}

		case CodecH265: {
			//这里的代码逻辑是让SPS、PPS、IDR这些时间戳相同的帧打包到一起当做一个帧处理，
			_frame_merger.inputFrame(frame, [&](uint32_t dts, uint32_t pts, const Buffer::Ptr &buffer, bool have_idr) {
				track_info.stamp.revise(dts, pts, dts_out, pts_out);
				fmp4_writer_write(m_writer,
					track_info.track_id,
					buffer->data(),
					buffer->size(),
					pts_out,
					dts_out,
					/*have_idr ? MOV_AV_FLAG_KEYFREAME : 0*/ MOV_AV_FLAG_KEYFREAME);
			});
			break;
		}

		default: {
			track_info.stamp.revise(frame->dts(), frame->pts(), dts_out, pts_out);
			fmp4_writer_write(m_writer,
				track_info.track_id,
				frame->data() + frame->prefixSize(),
				frame->size() - frame->prefixSize(),
				pts_out,
				dts_out,
				/*frame->keyFrame() ? MOV_AV_FLAG_KEYFREAME : 0*/ MOV_AV_FLAG_KEYFREAME);
			break;
		}

		}
		return MSEErrCode_Success;
	}

	int Fmp4Muxer::getFtype(unsigned char **data)
	{
		if (!gotFtyp)
			return -1;

		*data = m_fType_buffer;
		return m_FtypLen;
	}

	int Fmp4Muxer::getMoofFragment(unsigned char **data)
	{
		*data = m_buffer;
		int len = m_curIndex;

		m_curIndex = 0;
		return len;
	}

	static uint8_t getObject(CodecId codecId) {
		switch (codecId) {
		case CodecG711A: return MOV_OBJECT_G711a;
		case CodecG711U: return MOV_OBJECT_G711u;
		case CodecOpus: return MOV_OBJECT_OPUS;
		case CodecAAC: return MOV_OBJECT_AAC;
		case CodecH264: return MOV_OBJECT_H264;
		case CodecH265: return MOV_OBJECT_HEVC;
		default: return 0;
		}
	}

	void Fmp4Muxer::addTrack(const Track::Ptr &track) {
		auto mp4_object = getObject(track->getCodecId());
		if (!mp4_object) {
			WarnL << "MP4录制不支持该编码格式:" << track->getCodecName();
			return;
		}

		if (!track->ready()) {
			WarnL << "Track[" << track->getCodecName() << "]未就绪";
			return;
		}

		switch (track->getCodecId()) {
		case CodecG711A:
		case CodecG711U:
		case CodecOpus: {
			auto audio_track = dynamic_pointer_cast<AudioTrack>(track);
			if (!audio_track) {
				WarnL << "不是音频Track:" << track->getCodecName();
				return;
			}

			auto track_id = fmp4_writer_add_audio(m_writer,
				mp4_object,
				audio_track->getAudioChannel(),
				audio_track->getAudioSampleBit() * audio_track->getAudioChannel(),
				audio_track->getAudioSampleRate(),
				nullptr, 0);
			if (track_id < 0) {
				WarnL << "添加Track[" << track->getCodecName() << "]失败:" << track_id;
				return;
			}
			_codec_to_trackid[track->getCodecId()].track_id = track_id;
		}
						break;

		case CodecAAC: {
			auto audio_track = dynamic_pointer_cast<AACTrack>(track);
			if (!audio_track) {
				WarnL << "不是AAC Track";
				return;
			}

			auto track_id = fmp4_writer_add_audio(m_writer,
				mp4_object,
				audio_track->getAudioChannel(),
				audio_track->getAudioSampleBit() * audio_track->getAudioChannel(),
				audio_track->getAudioSampleRate(),
				audio_track->getAacCfg().data(),
				audio_track->getAacCfg().size());
			if (track_id < 0) {
				WarnL << "添加AAC Track失败:" << track_id;
				return;
			}
			_codec_to_trackid[track->getCodecId()].track_id = track_id;
		}
					   break;
		case CodecH264: {
			auto h264_track = dynamic_pointer_cast<H264Track>(track);
			if (!h264_track) {
				WarnL << "不是H264 Track";
				return;
			}

			struct mpeg4_avc_t avc = { 0 };
			string sps_pps = string("\x00\x00\x00\x01", 4) + h264_track->getSps() +
				string("\x00\x00\x00\x01", 4) + h264_track->getPps();
			h264_annexbtomp4(&avc, sps_pps.data(), (int)sps_pps.size(), NULL, 0, NULL, NULL);

			uint8_t extra_data[1024];
			int extra_data_size = mpeg4_avc_decoder_configuration_record_save(&avc, extra_data, sizeof(extra_data));
			if (extra_data_size == -1) {
				WarnL << "生成H264 extra_data 失败";
				return;
			}

			auto track_id = fmp4_writer_add_video(m_writer,
				mp4_object,
				h264_track->getVideoWidth(),
				h264_track->getVideoHeight(),
				extra_data,
				extra_data_size);

			if (track_id < 0) {
				WarnL << "添加H264 Track失败:" << track_id;
				return;
			}
			_codec_to_trackid[track->getCodecId()].track_id = track_id;
			_have_video = true;
		}
						break;
		case CodecH265: {
			auto h265_track = dynamic_pointer_cast<H265Track>(track);
			if (!h265_track) {
				WarnL << "不是H265 Track";
				return;
			}

			struct mpeg4_hevc_t hevc = { 0 };
			string vps_sps_pps = string("\x00\x00\x00\x01", 4) + h265_track->getVps() +
				string("\x00\x00\x00\x01", 4) + h265_track->getSps() +
				string("\x00\x00\x00\x01", 4) + h265_track->getPps();
			h265_annexbtomp4(&hevc, vps_sps_pps.data(), (int)vps_sps_pps.size(), NULL, 0, NULL, NULL);

			uint8_t extra_data[1024];
			int extra_data_size = mpeg4_hevc_decoder_configuration_record_save(&hevc, extra_data, sizeof(extra_data));
			if (extra_data_size == -1) {
				WarnL << "生成H265 extra_data 失败";
				return;
			}

			auto track_id = fmp4_writer_add_video(m_writer,
				mp4_object,
				h265_track->getVideoWidth(),
				h265_track->getVideoHeight(),
				extra_data,
				extra_data_size);
			if (track_id < 0) {
				WarnL << "添加H265 Track失败:" << track_id;
				return;
			}
			_codec_to_trackid[track->getCodecId()].track_id = track_id;
			_have_video = true;
		}
						break;

		default: WarnL << "MP4录制不支持该编码格式:" << track->getCodecName(); break;
		}

		stampSync();
	}

	void Fmp4Muxer::stampSync() {
		if (_codec_to_trackid.size() < 2) {
			return;
		}

		Stamp *audio = nullptr, *video = nullptr;
		for (auto &pr : _codec_to_trackid) {
			switch (getTrackType((CodecId)pr.first)) {
			case TrackAudio: audio = &pr.second.stamp; break;
			case TrackVideo: video = &pr.second.stamp; break;
			default: break;
			}
		}

		if (audio && video) {
			//音频时间戳同步于视频，因为音频时间戳被修改后不影响播放
			audio->syncTo(*video);
		}
	}
}
