#pragma once


#include <memory>
#include <map>
#include "Extension/Frame.h"
#include "Extension/Track.h"
#include "Fmp4Muxer.h"

namespace mediakit
{
	class Fmp4Muxer;

	class Fmp4MuxerClient
	{
	public:
		/**
		 * 构造h264类型的媒体
		 * @param frameRate 外部不传入frame时,打包时
		 */
		Fmp4MuxerClient(int width = 1920, int height = 1080);
		~Fmp4MuxerClient();
		MSEErrCode muxer(unsigned char* nalu, int len, bool keyFrame, unsigned int pts, unsigned int dts, CodecId type);
		MSEErrCode inputFrame(const Frame::Ptr &frame, bool keyFrame);
		void freshFrameRate(int frameRate = 0) {};
		void addTracks(vector<Track::Ptr> tracks);
		

	public:
		std::shared_ptr<Fmp4Muxer> m_Fmp4;
		bool m_FirstKeyFrame = false;
		bool m_SendFmp4Header = false;
		const unsigned char* m_Fmp4Header = nullptr;
		int m_HeaderLen = 0;
		const unsigned char* m_Fmp4Frame = nullptr;
		int m_FrameLen = 0;
		const FILE* m_File = nullptr;
		bool m_WriteHeader = false;
		int m_width = 0;
		int m_height = 0;
	};
}

