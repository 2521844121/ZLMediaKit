#include "Fmp4MuxerClient.h"
#include "Extension/AAC.h"
//#define WRITE_FILE

namespace mediakit
{
	Fmp4MuxerClient::Fmp4MuxerClient(int width, int height)
		:m_width(width), m_height(height)
	{
		m_Fmp4.reset(new Fmp4Muxer(m_width, m_height, FRAGMENT_PER_FRAME, 10));

#ifdef WRITE_FILE
		char buf[20];
		static int i = 1;
		sprintf(buf, "D:\\fmp4-%d.mp4", i++);
		m_File = fopen(buf, "wb");
#endif
	}

	Fmp4MuxerClient::~Fmp4MuxerClient()
	{
#ifdef WRITE_FILE
		if (m_File)
			fclose((FILE*)m_File);
#endif
	}

	MSEErrCode Fmp4MuxerClient::muxer(unsigned char* nalu, int len, bool keyFrame, unsigned int pts, unsigned int dts, CodecId type)
	{
		MSEErrCode mutexRs = MSEErrCode_Success;
		if (!m_FirstKeyFrame)
		{
			m_FirstKeyFrame = keyFrame;
			if (!m_FirstKeyFrame)
			{
				ErrorL << "Fmp4MuxerClient::muxer wait i frame! " << type;
				return MSEErrCode_NoKeyFrame;
			}
		}

		if (type == CodecH264)
		{
			mutexRs = m_Fmp4->mux(CodecH264, nalu, len, pts, dts);
		}
		else if (type == CodecH265)
		{
			mutexRs = m_Fmp4->mux(CodecH265, nalu, len, pts, dts);
		}
		else if (type == CodecAAC)
		{
			//r = m_Fmp4->mux(AACFrame, nalu, len, m_AudioPts);
			//if (MSEErrCode_Success == r)
			//	m_AudioPts += m_AudioInterval;
			ErrorL << "Fmp4MuxerClient::muxer aac not support!";
			mutexRs = MSEErrCode_UnknowCodecId;
		}
		else
		{
			ErrorL << "Fmp4MuxerClient::muxer unknow codecid! " << type;
		}

		if (MSEErrCode_Success == mutexRs)
		{
			if (m_HeaderLen <= 0)
				m_HeaderLen = m_Fmp4->getFtype((unsigned char**)&m_Fmp4Header);

			if (m_HeaderLen <= 0)
			{
				WarnL << "Fmp4MuxerClient::muxer m_HeaderLen < 0! ";
				return MSEErrCode_NoFmp4Header;
			}
				


			m_FrameLen = m_Fmp4->getMoofFragment((unsigned char**)&m_Fmp4Frame);
			if (m_FrameLen <= 0)
			{
				WarnL << "Fmp4MuxerClient::muxer getMoofFragment error! ";
				return MSEErrCode_NoMediaFrame;
			}				

#ifdef WRITE_FILE
			//write file for test
			if (m_HeaderLen > 0 && !m_WriteHeader)
			{
				fwrite((const char*)m_Fmp4Header, m_HeaderLen, 1, (FILE*)m_File);
				m_WriteHeader = true;
			}

			if (m_FrameLen > 0)
				fwrite((const char*)m_Fmp4Frame, m_FrameLen, 1, (FILE*)m_File);
#endif
			//end write file

			return MSEErrCode_Success;
		}
		else
		{
			ErrorL << "Fmp4MuxerClient::muxer mux error! " << type;
			return mutexRs;
		}		
	}

	MSEErrCode Fmp4MuxerClient::inputFrame(const Frame::Ptr &frame, bool keyFrame)
	{
		if (!m_FirstKeyFrame)
		{
			m_FirstKeyFrame = keyFrame;
			if (!m_FirstKeyFrame)
			{
				ErrorL << "Fmp4MuxerClient::muxer wait i frame! " << frame->getCodecId();
				return MSEErrCode_NoKeyFrame;
			}
		}

		MSEErrCode mutexRs = m_Fmp4->inputFrame(frame);

		if (MSEErrCode_Success == mutexRs)
		{
			if (m_HeaderLen <= 0)
				m_HeaderLen = m_Fmp4->getFtype((unsigned char**)&m_Fmp4Header);

			if (m_HeaderLen <= 0)
			{
				ErrorL << "Fmp4MuxerClient::muxer m_HeaderLen < 0! ";
				return MSEErrCode_NoFmp4Header;
			}



			m_FrameLen = m_Fmp4->getMoofFragment((unsigned char**)&m_Fmp4Frame);
			if (m_FrameLen <= 0)
			{
				ErrorL << "Fmp4MuxerClient::muxer getMoofFragment error! ";
				return MSEErrCode_NoMediaFrame;
			}

#ifdef WRITE_FILE
			//write file for test
			if (m_HeaderLen > 0 && !m_WriteHeader)
			{
				fwrite((const char*)m_Fmp4Header, m_HeaderLen, 1, (FILE*)m_File);
				m_WriteHeader = true;
			}

			if (m_FrameLen > 0)
				fwrite((const char*)m_Fmp4Frame, m_FrameLen, 1, (FILE*)m_File);
#endif
			//end write file

			return MSEErrCode_Success;
		}
		else
		{
			ErrorL << "Fmp4MuxerClient::muxer mux error! " << frame->getCodecId();
			return mutexRs;
		}
	}

	void Fmp4MuxerClient::addTracks(vector<Track::Ptr> tracks)
	{
		for (auto track : tracks)
		{
			m_Fmp4->addTrack(track);
		
		}
	}
}