#pragma once


#include <memory>
#include <map>
#include "Extension/Frame.h"
#include "Fmp4Muxer.h"

namespace mediakit
{
	class Fmp4Muxer;

	class Fmp4MuxerClient
	{
	public:
		Fmp4MuxerClient(int frameRate = 25);
		~Fmp4MuxerClient();
		MSEErrCode muxer(unsigned char* nalu, int len, bool keyFrame, unsigned int pts, CodecId type);
		void freshFrameRate(int frameRate = 25);

	public:
		std::shared_ptr<Fmp4Muxer> m_Fmp4;
		bool m_FirstKeyFrame;
		bool m_SendFmp4Header;
		const unsigned char* m_Fmp4Header;
		int m_HeaderLen;
		const unsigned char* m_Fmp4Frame;
		int m_FrameLen;
		const FILE* m_File;
		bool m_WriteHeader;
		unsigned int m_pts;
		unsigned int m_Interval;
		unsigned int m_AudioPts;
		unsigned int m_AudioInterval;
	};
}

