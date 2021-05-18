#pragma once

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include "Extension/Frame.h"
#include "Extension/Track.h"
#include "Common/Stamp.h"

#define GOP_BUF_LEN	 40 * 1024 * 1024
#define FRAME_BUF_LEN	1024 * 1024 * 4
#define FTYP_LEN 2048

struct fmp4_writer_t;
namespace mediakit
{
	typedef enum
	{
		FRAGMENT_PER_GOP,
		FRAGMENT_PER_FRAME,
		FRAGMENT_PER_GROUP
	}FragmentType;

	enum MSEErrCode
	{
		MSEErrCode_Success = 0,
		MSEErrCode_NoKeyFrame = -20000,//no key frame
		MSEErrCode_InvalidParam = -20001,//param invalid
		MSEErrCode_NoFmp4Header = -20002,//no fmp4header,ftyp and moov
		MSEErrCode_NoMediaFrame = -20003,//no media frame ,moof and mdat
		MSEErrCode_MuxerFailed = -20004,//muxer failed
		MSEErrCode_NoAnyFrame = -20005,//no any frame
		MSEErrCode_InviteFailed = -20006,//request video failure
		MSEErrCode_InvalidStartCode = -20007, //�Ƿ���ʼ�롣������00 00 01����00 00 00 01��ͷ
		MSEErrCode_NoSPS = -20008, //ȱ��SPS
		MSEErrCode_NoPPS = -20009, //ȱ��PPS
		MSEErrCode_NoIDR = -20010, //ȱ��IDR
		MSEErrCode_UnknowNalType = -20011, //δ֪NAL����
		MSEErrCode_UnknowCodecId = -20012, //δ֪Codec����
		MSEErrCode_Max = -20999
	};

	class Fmp4Muxer
	{
	public:
		Fmp4Muxer(int width, int height, FragmentType type, int groupNum);
		virtual ~Fmp4Muxer();

		void addTrack(const Track::Ptr &track);

		MSEErrCode inputFrame(const Frame::Ptr &frame);

		//0 �ɹ�  -1ʧ��. nalu����������֡�������ǵ���NALU
		MSEErrCode mux(CodecId frameType, unsigned char* nalu, int len, unsigned int pts, unsigned int dts=0);

		//�ɹ����س���  -1ʧ��
		int getFtype(unsigned char **data) ;

		//���س���
		int getMoofFragment(unsigned char **data);

		int handleWrite(const void* data, uint64_t bytes);

		int handleSeek(uint64_t offset);

		uint64_t handleTell();

		void handleFtypwriteCompleted();

		void stampSync();

	private:
		int getStartCodeLen(unsigned char* nalu, int len);

	private:
		int m_avc_track;
		int m_hevc_track;
		int m_aac_track;
		uint8_t *m_buffer;
		uint64_t m_maxBufSize;
		uint8_t *m_fType_buffer;
		fmp4_writer_t* m_writer;

		int m_width;
		int m_height;

		bool isGotI;
		bool gotFtyp;

		uint64_t m_curIndex;
		uint64_t m_FtypLen;
		//key track type
		//value track id
		bool _have_video = false;
		bool _started = false;
		struct TrackInfo {
			int track_id = -1;
			Stamp stamp;
		};
		unordered_map<int, TrackInfo> _codec_to_trackid;

		FrameMerger _frame_merger{ FrameMerger::mp4_nal_size };
	};
}