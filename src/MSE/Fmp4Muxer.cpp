#include "mov-format.h"
#include "mov-buffer.h"
#include "mov-writer.h"
#include "fmp4-writer.h"
#include "mpeg4-aac.h"
#include "mpeg4-hevc.h"
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


	Fmp4Muxer::Fmp4Muxer(int width, int height, FragmentType type, int groupNum, int ptsItv)
		:m_width(width), m_height(height), m_ptsItv(ptsItv)
	{
		m_writer = fmp4_writer_create(mov_file_buffer(), this, 0/*, type, groupNum*/);
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

	int Fmp4Muxer::setADTS(unsigned char* adts, int bitsPerSample, int len)
	{
		int rate = 1;
		struct mpeg4_aac_t aac;
		uint8_t extra_data[1024];
		mpeg4_aac_adts_load(adts, len, &aac);
		if (-1 == m_aac_track)
		{
			int extra_data_size = mpeg4_aac_audio_specific_config_save(&aac, extra_data, sizeof(extra_data));
			rate = mpeg4_aac_audio_frequency_to((enum mpeg4_aac_frequency)aac.sampling_frequency_index);
			m_aac_track = fmp4_writer_add_audio(m_writer, MOV_OBJECT_AAC, aac.channel_configuration, bitsPerSample, rate, /*20,*/ extra_data, extra_data_size);
			if (-1 == m_aac_track)
				return -1;
		}
		return 0;
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
		int ret = 0;
		if (frameType == CodecH264)
		{
#if 0
			int type = nalu[4] & 0x1f;

			if (!isGotI && type != 7)
				return  -1;

			if (type == 7)
			{
				int i;
				int iFrameLen;
				unsigned char *iFrame = NULL;
				uint8_t *tmpBuffer = (uint8_t *)malloc(len + 20);

				if (!isGotI)
				{
					isGotI = true;

					int spsLen, ppsLen;
					unsigned char *sps = nalu + 4;
					for (i = 0; i < len; i++)
					{
						if (sps[i] == 0x00 && sps[i + 1] == 0x00 && sps[i + 2] == 0x00 && sps[i + 3] == 0x01)
						{
							spsLen = i;
							break;
						}
					}

					if (i == len)
					{
						ErrLog(<< "[Fmp4Muxer::mux] ONLY SPS!");
						free(tmpBuffer);
						return -1;
					}

					unsigned char *pps = sps + spsLen + 4;
					for (i = 0; i < len; i++)
					{
						if (pps[i] == 0x00 && pps[i + 1] == 0x00 && pps[i + 2] == 0x00 && pps[i + 3] == 0x01)
						{
							ppsLen = i;
							break;
						}
					}

					if (i == len)
					{
						ErrLog(<< "[Fmp4Muxer::mux] ONLY PPS!");
						free(tmpBuffer);
						return -1;
					}

					if (-1 == m_avc_track)
					{
						uint8_t *p = tmpBuffer;
						*p++ = 0x01;
						*p++ = sps[1];
						*p++ = sps[2];
						*p++ = sps[3];
						*p++ = 0xFF; // lengthSizeMinusOne: 3, nalu length is 4 bytes
						*p++ = 0xE1;
						*p++ = (spsLen >> 8) & 0xFF;
						*p++ = spsLen & 0xFF;
						memcpy(p, sps, spsLen);
						p += spsLen;

						*p++ = 0x01;
						*p++ = (ppsLen >> 8) & 0xFF;
						*p++ = ppsLen & 0xFF;
						memcpy(p, pps, ppsLen);

						int bufSize = spsLen + ppsLen + 11;

						m_avc_track = fmp4_writer_add_video(m_writer, MOV_OBJECT_H264, m_width, m_height, m_ptsItv, tmpBuffer, bufSize);
					}
				}

				for (i = 0; i < len; i++)
				{
					if (nalu[i] == 0x00 && nalu[i + 1] == 0x00 && nalu[i + 2] == 0x00 && nalu[i + 3] == 0x01 && (nalu[i + 4] & 0x1f) == 5)
					{
						iFrame = nalu + i + 4;
						iFrameLen = len - i - 4;
						break;
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
					ret = fmp4_writer_write(m_writer, m_avc_track, tmpBuffer, iFrameLen + 4, pts, pts, 1);
				}
				else
				{
					ErrLog(<< "[Fmp4Muxer::mux] NO IDR!");
					free(tmpBuffer);
					return -1;
				}

				free(tmpBuffer);
			}
			else if (type == 1)
			{
				if (nalu[0] == 0x00 && nalu[1] == 0x00 && nalu[2] == 0x00 && nalu[3] == 0x01)
				{
					int pFrameLen = len - 4;
					nalu[0] = (pFrameLen >> 24) & 0xFF;
					nalu[1] = (pFrameLen >> 16) & 0xFF;
					nalu[2] = (pFrameLen >> 8) & 0xFF;
					nalu[3] = pFrameLen & 0xFF;

					ret = fmp4_writer_write(m_writer, m_avc_track, nalu, len, pts, pts, 0);

					nalu[0] = 0x00;
					nalu[1] = 0x00;
					nalu[2] = 0x00;
					nalu[3] = 0x01;
				}
			}
			else
			{
				ErrLog(<< "[Fmp4Muxer::mux] data type unsupport!" << type);
				return -1;
			}
#else
			int startCodeLen = getStartCodeLen(nalu, len);
			if (startCodeLen == 0)
			{
				ErrorL << "Fmp4Muxer::mux invalidate start code!! len[" << len << "][" << (int)nalu[0] << "][" << (int)nalu[1] << "][" << (int)nalu[2] << "][" << (int)nalu[3] << "]";
				return MSEErrCode_InvalidStartCode;
			}

			int type = nalu[startCodeLen] & 0x1f;

			if (!isGotI && type != 7)
			{
				ErrorL << "Fmp4Muxer::mux invalidate waitting i frame!!";
				return  MSEErrCode_NoKeyFrame;
			}
				

			if (type == 7)
			{
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

					if (-1 == m_avc_track)
					{
						uint8_t *p = tmpBuffer;
						*p++ = 0x01;
						*p++ = sps[1];
						*p++ = sps[2];
						*p++ = sps[3];
						*p++ = 0xFF; // lengthSizeMinusOne: 3, nalu length is 4 bytes
						*p++ = 0xE1;
						*p++ = (spsLen >> 8) & 0xFF;
						*p++ = spsLen & 0xFF;
						memcpy(p, sps, spsLen);
						p += spsLen;

						*p++ = 0x01;
						*p++ = (ppsLen >> 8) & 0xFF;
						*p++ = ppsLen & 0xFF;
						memcpy(p, pps, ppsLen);

						int bufSize = spsLen + ppsLen + 11;

						m_avc_track = fmp4_writer_add_video(m_writer, MOV_OBJECT_H264, m_width, m_height, /*m_ptsItv,*/ tmpBuffer, bufSize);
					}
				}

				for (i = 0; i < len; i++)
				{
					if (nalu[i] == 0x00 && nalu[i + 1] == 0x00 && nalu[i + 2] == 0x00 && nalu[i + 3] == 0x01 && (nalu[i + 4] & 0x1f) == 5)
					{
						iFrame = nalu + i + 4;
						iFrameLen = len - i - 4;
						break;
					}
					else if (nalu[i] == 0x00 && nalu[i + 1] == 0x00 && nalu[i + 2] == 0x01 && (nalu[i + 3] & 0x1f) == 5)
					{
						iFrame = nalu + i + 3;
						iFrameLen = len - i - 3;
						break;
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
					ret = fmp4_writer_write(m_writer, m_avc_track, tmpBuffer, iFrameLen + 4, pts, pts, MOV_AV_FLAG_KEYFREAME);
				}
				else
				{
					ErrorL << "[Fmp4Muxer::mux] idr not found!";
					free(tmpBuffer);
					return MSEErrCode_NoIDR;
				}

				free(tmpBuffer);
			}
			else if (type == 1 || type == 5)
			{
			if (type == 5)
			{
				cout << "Fmp4Muxer::mux type is " << endl;
				}
				if (nalu[0] == 0x00 && nalu[1] == 0x00 && nalu[2] == 0x00 && nalu[3] == 0x01)
				{
					int pFrameLen = len - 4;
					nalu[0] = (pFrameLen >> 24) & 0xFF;
					nalu[1] = (pFrameLen >> 16) & 0xFF;
					nalu[2] = (pFrameLen >> 8) & 0xFF;
					nalu[3] = pFrameLen & 0xFF;

					if (type == 1)
						ret = fmp4_writer_write(m_writer, m_avc_track, nalu, len, pts, pts, MOV_AV_FLAG_KEYFREAME);
					else if (type == 5)
						ret = fmp4_writer_write(m_writer, m_avc_track, nalu, len, pts, pts, MOV_AV_FLAG_KEYFREAME);

					nalu[0] = 0x00;
					nalu[1] = 0x00;
					nalu[2] = 0x00;
					nalu[3] = 0x01;
				}
				else if (nalu[0] == 0x00 && nalu[1] == 0x00 && nalu[2] == 0x01)
				{// 00 00 01的方式未处理
					uint8_t *tmpBuffer = (uint8_t *)malloc(len + 20);

					int pFrameLen = len - 3;
					tmpBuffer[0] = (pFrameLen >> 24) & 0xFF;
					tmpBuffer[1] = (pFrameLen >> 16) & 0xFF;
					tmpBuffer[2] = (pFrameLen >> 8) & 0xFF;
					tmpBuffer[3] = pFrameLen & 0xFF;

					memcpy(tmpBuffer + 4, nalu + 3, len - 3);

					if (type == 1)
						ret = fmp4_writer_write(m_writer, m_avc_track, tmpBuffer, len + 1/*去除的nal 00 00 01，但是新增了 4字节fmp4头*/, pts, pts, MOV_AV_FLAG_KEYFREAME);
					else if (type == 5)
						ret = fmp4_writer_write(m_writer, m_avc_track, tmpBuffer, len + 1/*去除的nal 00 00 01，但是新增了 4字节fmp4头*/, pts, pts, MOV_AV_FLAG_KEYFREAME);


					free(tmpBuffer);
				}
			}
			else
			{
				ErrorL << "[Fmp4Muxer::mux] data type unsupport!" << type;
				return  MSEErrCode_UnknowNalType;;
			}
			return MSEErrCode_Success;
#endif
		}
		else if (frameType == CodecH265)
		{
			if (nalu[0] == 0x00 && nalu[1] == 0x00 && nalu[2] == 0x01)
			{
				ErrorL << "[Fmp4Muxer::mux] h265 unsupport 00 00 01";
				return MSEErrCode_InvalidStartCode;
			}
			uint8_t nalutype = (nalu[4] >> 1) & 0x3f;

			if (!isGotI && nalutype != 32)
				return  MSEErrCode_NoKeyFrame;

			if (nalutype == 32)
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
						if (type == 32 || type == 33 || type == 34)
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
						m_hevc_track = fmp4_writer_add_video(m_writer, MOV_OBJECT_HEVC, m_width, m_height, /*m_ptsItv, */data, hdcrSize);
					}
				}

				for (; idx < len; idx++)
				{
					if (nalu[idx] == 0x00 && nalu[idx + 1] == 0x00 && nalu[idx + 2] == 0x00 && nalu[idx + 3] == 0x01)
					{
						uint8_t type = (nalu[idx + 4] >> 1) & 0x3f;
						if (type == 19)
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
					ret = fmp4_writer_write(m_writer, m_hevc_track, tmpBuffer, iFrameLen + 4, pts, pts, MOV_AV_FLAG_KEYFREAME);
				}
				else
				{
					ErrorL << "[Fmp4Muxer::mux] NO h265 IDR!";
					free(tmpBuffer);
					return MSEErrCode_NoIDR;
				}

				free(tmpBuffer);
			}
			else if (nalutype == 1)
			{
				if (nalu[0] == 0x00 && nalu[1] == 0x00 && nalu[2] == 0x00 && nalu[3] == 0x01)
				{
					int pFrameLen = len - 4;
					nalu[0] = (pFrameLen >> 24) & 0xFF;
					nalu[1] = (pFrameLen >> 16) & 0xFF;
					nalu[2] = (pFrameLen >> 8) & 0xFF;
					nalu[3] = pFrameLen & 0xFF;

					ret = fmp4_writer_write(m_writer, m_hevc_track, nalu, len, pts, pts, MOV_AV_FLAG_KEYFREAME);

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

			/*char audBuf[2048] = { 0 };
			int pFrameLen = len - 7;
			audBuf[0] = (pFrameLen >> 24) & 0xFF;
			audBuf[1] = (pFrameLen >> 16) & 0xFF;
			audBuf[2] = (pFrameLen >> 8) & 0xFF;
			audBuf[3] = pFrameLen & 0xFF;
			memcpy(audBuf + 4, nalu + 7, pFrameLen);*/

			ret = fmp4_writer_write(m_writer, m_aac_track, nalu + 7, len - 7, pts, pts, MOV_AV_FLAG_KEYFREAME);
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
}