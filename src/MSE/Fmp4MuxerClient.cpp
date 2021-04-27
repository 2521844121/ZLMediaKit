#include "Fmp4MuxerClient.h"

//#define WRITE_FILE
namespace mediakit
{
	Fmp4MuxerClient::Fmp4MuxerClient(int frameRate)
	{
		m_HeaderLen = 0;
		m_FrameLen = 0;
		m_FirstKeyFrame = false;
		m_Fmp4Header = NULL;
		m_Fmp4Frame = NULL;
		m_File = NULL;
		m_SendFmp4Header = false;
		m_WriteHeader = false;
		m_pts = 0;
		m_Interval = 0;
		m_AudioPts = 0;
		m_AudioInterval = 128;
		freshFrameRate(frameRate);
#ifdef WRITE_FILE
		char buf[20];
		static int i = 1;
		sprintf(buf, "D:\\fmp4-%d.mp4", i++);
		m_File = fopen(buf, "wb");
#endif
	}

	void Fmp4MuxerClient::freshFrameRate(int frameRate)
	{
		frameRate = (frameRate == 0 ? 25 : frameRate);
		m_Interval = 1000 / frameRate;
		m_Fmp4.reset(new Fmp4Muxer(1920, 1080, FRAGMENT_PER_FRAME, 10, m_Interval));
	}

	Fmp4MuxerClient::~Fmp4MuxerClient()
	{
#ifdef WRITE_FILE
		if (m_File)
			fclose((FILE*)m_File);
#endif
	}

	MSEErrCode Fmp4MuxerClient::muxer(unsigned char* nalu, int len, bool keyFrame, unsigned int pts, CodecId type)
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
			mutexRs = m_Fmp4->mux(CodecH264, nalu, len, m_pts);
			if (MSEErrCode_Success == mutexRs)
				m_pts += m_Interval;
		}
		else if (type == CodecH265)
		{
			mutexRs = m_Fmp4->mux(CodecH265, nalu, len, m_pts);
			if (MSEErrCode_Success == mutexRs)
				m_pts += m_Interval;
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
			ErrorL << "Fmp4MuxerClient::muxer mux error! " << type;
			return mutexRs;
		}		
	}

}