/*
 * Copyright (c) 2016 The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/xiongziliang/ZLMediaKit).
 *
 * Use of this source code is governed by MIT license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#include "MseSession.h"
#include "Extension/H264.h"
#include "Extension/SPSParser.h"
#include "Extension/AAC.h"
//#define WRITE_FILE

static uint8_t s_mute_adts[] = { 0xff, 0xf1, 0x6c, 0x40, 0x2d, 0x3f, 0xfc, 0x00, 0xe0, 0x34, 0x20, 0xad, 0xf2, 0x3f, 0xb5, 0xdd,
								0x73, 0xac, 0xbd, 0xca, 0xd7, 0x7d, 0x4a, 0x13, 0x2d, 0x2e, 0xa2, 0x62, 0x02, 0x70, 0x3c, 0x1c,
								0xc5, 0x63, 0x55, 0x69, 0x94, 0xb5, 0x8d, 0x70, 0xd7, 0x24, 0x6a, 0x9e, 0x2e, 0x86, 0x24, 0xea,
								0x4f, 0xd4, 0xf8, 0x10, 0x53, 0xa5, 0x4a, 0xb2, 0x9a, 0xf0, 0xa1, 0x4f, 0x2f, 0x66, 0xf9, 0xd3,
								0x8c, 0xa6, 0x97, 0xd5, 0x84, 0xac, 0x09, 0x25, 0x98, 0x0b, 0x1d, 0x77, 0x04, 0xb8, 0x55, 0x49,
								0x85, 0x27, 0x06, 0x23, 0x58, 0xcb, 0x22, 0xc3, 0x20, 0x3a, 0x12, 0x09, 0x48, 0x24, 0x86, 0x76,
								0x95, 0xe3, 0x45, 0x61, 0x43, 0x06, 0x6b, 0x4a, 0x61, 0x14, 0x24, 0xa9, 0x16, 0xe0, 0x97, 0x34,
								0xb6, 0x58, 0xa4, 0x38, 0x34, 0x90, 0x19, 0x5d, 0x00, 0x19, 0x4a, 0xc2, 0x80, 0x4b, 0xdc, 0xb7,
								0x00, 0x18, 0x12, 0x3d, 0xd9, 0x93, 0xee, 0x74, 0x13, 0x95, 0xad, 0x0b, 0x59, 0x51, 0x0e, 0x99,
								0xdf, 0x49, 0x98, 0xde, 0xa9, 0x48, 0x4b, 0xa5, 0xfb, 0xe8, 0x79, 0xc9, 0xe2, 0xd9, 0x60, 0xa5,
								0xbe, 0x74, 0xa6, 0x6b, 0x72, 0x0e, 0xe3, 0x7b, 0x28, 0xb3, 0x0e, 0x52, 0xcc, 0xf6, 0x3d, 0x39,
								0xb7, 0x7e, 0xbb, 0xf0, 0xc8, 0xce, 0x5c, 0x72, 0xb2, 0x89, 0x60, 0x33, 0x7b, 0xc5, 0xda, 0x49,
								0x1a, 0xda, 0x33, 0xba, 0x97, 0x9e, 0xa8, 0x1b, 0x6d, 0x5a, 0x77, 0xb6, 0xf1, 0x69, 0x5a, 0xd1,
								0xbd, 0x84, 0xd5, 0x4e, 0x58, 0xa8, 0x5e, 0x8a, 0xa0, 0xc2, 0xc9, 0x22, 0xd9, 0xa5, 0x53, 0x11,
								0x18, 0xc8, 0x3a, 0x39, 0xcf, 0x3f, 0x57, 0xb6, 0x45, 0x19, 0x1e, 0x8a, 0x71, 0xa4, 0x46, 0x27,
								0x9e, 0xe9, 0xa4, 0x86, 0xdd, 0x14, 0xd9, 0x4d, 0xe3, 0x71, 0xe3, 0x26, 0xda, 0xaa, 0x17, 0xb4,
								0xac, 0xe1, 0x09, 0xc1, 0x0d, 0x75, 0xba, 0x53, 0x0a, 0x37, 0x8b, 0xac, 0x37, 0x39, 0x41, 0x27,
								0x6a, 0xf0, 0xe9, 0xb4, 0xc2, 0xac, 0xb0, 0x39, 0x73, 0x17, 0x64, 0x95, 0xf4, 0xdc, 0x33, 0xbb,
								0x84, 0x94, 0x3e, 0xf8, 0x65, 0x71, 0x60, 0x7b, 0xd4, 0x5f, 0x27, 0x79, 0x95, 0x6a, 0xba, 0x76,
								0xa6, 0xa5, 0x9a, 0xec, 0xae, 0x55, 0x3a, 0x27, 0x48, 0x23, 0xcf, 0x5c, 0x4d, 0xbc, 0x0b, 0x35,
								0x5c, 0xa7, 0x17, 0xcf, 0x34, 0x57, 0xc9, 0x58, 0xc5, 0x20, 0x09, 0xee, 0xa5, 0xf2, 0x9c, 0x6c,
								0x39, 0x1a, 0x77, 0x92, 0x9b, 0xff, 0xc6, 0xae, 0xf8, 0x36, 0xba, 0xa8, 0xaa, 0x6b, 0x1e, 0x8c,
								0xc5, 0x97, 0x39, 0x6a, 0xb8, 0xa2, 0x55, 0xa8, 0xf8 };
#define MUTE_ADTS_DATA s_mute_adts
#define MUTE_ADTS_DATA_LEN sizeof(s_mute_adts)
#define MUTE_ADTS_DATA_MS 130

namespace mediakit
{
	MSESession::MSESession(const Socket::Ptr &pSock) :
		WebSocketSession(pSock)
	{
#ifdef WRITE_FILE
		char buf[20];
		static int i = 1;
		sprintf(buf, "d://fmp4-%d.mp4", i++);
		_testFile = fopen(buf, "wb");
#endif
	}

	MSESession::~MSESession()
	{
		if (_testFile)
			fclose((FILE*)_testFile);
	}

	void MSESession::attachServer(const TcpServer &server) 
	{
		DebugL << getIdentifier() << " " << TcpSession::getIdentifier();
		auto mseSession = dynamic_pointer_cast<WebSocketSessionTypeImp<MSESession>>(shared_from_this());
		if (mseSession)
		{//校验请求			
			string reqUrl(RAW_SCHEMA);
			reqUrl.append("://127.0.0.1");//FullUrl中过滤掉了 ws://ip:port部分._media_info.parse需要完成url
			reqUrl.append(mseSession->getHeader().FullUrl());
			_media_info.parse(reqUrl);

			if (_media_info._app.empty() || _media_info._streamid.empty())
			{
				shutdown(SockException(Err_shutdown, "invalidate reqeust url!" + mseSession->getHeader().FullUrl()));
				return;
			}
			
			startPlay();
		}
		else
		{
			shutdown(SockException(Err_shutdown, "invalidate reqeust!"));
		}
	}

	void MSESession::onRecv(const Buffer::Ptr &buffer)
	{
		//回显数据
		auto msg = std::make_shared<BufferString>("From MSESession: unsupport now!");
		send(msg, WebSocketHeader::TEXT);
		send(buffer, WebSocketHeader::TEXT);
	}

	void MSESession::onRecv(const Buffer::Ptr &buf, const WebSocketHeader::Type& type)
	{
		//回显数据
		auto msg = std::make_shared<BufferString>("From MSESession: unsupport now!");
		send(msg, WebSocketHeader::TEXT);
		send(buf, WebSocketHeader::TEXT);
	}

	void MSESession::onError(const SockException &err)
	{
		WarnL << err.what();
		stopPlay();
	}

	void MSESession::onClose()
	{
		TcpSession::safeShutdown();
	}

	//每隔一段时间触发，用来做超时管理
	void MSESession::onManager()
	{
		DebugL;
	}


	void MSESession::startPlay()
	{
		TraceP(this);
		weak_ptr<MSESession> weakSelf = dynamic_pointer_cast<WebSocketSessionTypeImp<MSESession>>(shared_from_this());

		//寻找媒体源
		MediaSource::findAsync(_media_info, weakSelf.lock(), [weakSelf](const MediaSource::Ptr &src)
		{
			shared_ptr<MSESession> strongSelf = weakSelf.lock();
			if (!strongSelf) 
			{
				ErrorL << "MseSession Released!";
				return;
			}

			auto mse_src = dynamic_pointer_cast<RawMediaSource>(src);
			if (!mse_src)
			{
				//未找到相应的MediaSource
				string err = StrPrinter << "no such stream:" << strongSelf->_media_info._vhost << " " << strongSelf->_media_info._app << " " << strongSelf->_media_info._streamid;
				//strongSelf->send_StreamNotFound();
				strongSelf->shutdown(SockException(Err_shutdown, err));
				return;
			}

			strongSelf->_play_src = mse_src;

			//Set _play_reader
			//weak_ptr<MSESession> weakSelf = dynamic_pointer_cast<WebSocketSessionTypeImp<MSESession>>(strongSelf);
			strongSelf->_play_reader = mse_src->getRing()->attach(strongSelf->getPoller(), true);

			if (!strongSelf->_play_reader)
			{
				auto play_src = strongSelf->_play_src.lock();
				if (!play_src) {

					strongSelf->shutdown(SockException(Err_shutdown, "mse stream released"));
					return;
				}
			}

			strongSelf->_play_reader->setDetachCB([weakSelf]() {
				auto strongSelf = weakSelf.lock();
				if (!strongSelf) {
					return;
				}
				strongSelf->shutdown(SockException(Err_shutdown, "mse ring buffer detached"));
			});

			strongSelf->_play_reader->setReadCB([weakSelf](const RawMediaSource::RingDataType &pack) {
				auto strongSelf = weakSelf.lock();
				if (!strongSelf) {
					return;
				}

				strongSelf->processAVFrame(pack);
			});

			
		});

	}

	void MSESession::processAVFrame(const RawMediaSource::RingDataType &pack)
	{
		if (!_muxerClient)
		{
			_muxerClient = std::make_shared<Fmp4MuxerClient>();

			auto tracks = _play_src.lock()->getTracks();
			bool hasAudioTrack = false;
			bool hasVideoTrack = false;
			for (auto track : tracks)
			{
				if (track->getTrackType() == TrackVideo);
				{
					hasVideoTrack = true;
				}

				if (track->getTrackType() == TrackAudio)
				{
					hasAudioTrack = true;
				}
			}
			m_addMuteAudio = hasVideoTrack && !hasAudioTrack;

			if (m_addMuteAudio)
			{//如果有音频轨但是没有视频轨,需要生成一个静音的音频轨
				_muteAudioPool = std::make_shared<List<Frame::Ptr>>();
				auto aacTrack = std::make_shared<AACTrack>();
				//插入一个静音帧,生成AAC配置信息
				aacTrack->inputFrame(std::make_shared<FrameFromPtr>(CodecAAC, (char *)MUTE_ADTS_DATA, MUTE_ADTS_DATA_LEN, m_audioIindex * MUTE_ADTS_DATA_MS, 0, ADTS_HEADER_LEN));
				tracks.push_back(aacTrack);
			}

			_muxerClient->addTracks(tracks);
		}

		
		pack->for_each([&](const Frame::Ptr &frame)
		{	
			MSEErrCode muxerRs = _muxerClient->inputFrame(frame, true);
			if (muxerRs == MSEErrCode_Success)
			{
				if (!_muxerClient->m_SendFmp4Header)
				{//还没有发送过fmp4头? 发送一个
					if (_muxerClient->m_HeaderLen > 0)
					{
						_muxerClient->m_SendFmp4Header = true;
						auto msg = _hRawFramePool.obtain();
						msg->setCapacity(_muxerClient->m_HeaderLen);
						msg->setSize(_muxerClient->m_HeaderLen);
						memcpy(msg->data(), _muxerClient->m_Fmp4Header, _muxerClient->m_HeaderLen);
						if (_testFile)
							fwrite(_muxerClient->m_Fmp4Header, _muxerClient->m_HeaderLen, 1, _testFile);
						int sendRs = send(msg, WebSocketHeader::BINARY);
					}
					else
					{
						ErrorL << "processAVFrame send Fmp4Header error! headerLen " << _muxerClient->m_HeaderLen;
						return;
					}
				}


				if (_muxerClient->m_SendFmp4Header)
				{
					if (_muxerClient->m_FrameLen > 0)
					{
						auto msg = _hRawFramePool.obtain();
						msg->setCapacity(_muxerClient->m_FrameLen);
						msg->setSize(_muxerClient->m_FrameLen);
						memcpy(msg->data(), _muxerClient->m_Fmp4Frame, _muxerClient->m_FrameLen);
						if (_testFile)
							fwrite(_muxerClient->m_Fmp4Frame, _muxerClient->m_FrameLen, 1, _testFile);
						int sendRs = send(msg, WebSocketHeader::BINARY);
					}
					else
					{
						ErrorL << "processAVFrame send Fmp4Frame error! m_HeaderLen " << _muxerClient->m_HeaderLen;
						return;
					}
				}

				if (_muteAudioPool)
				{//生成静音
					auto audio_idx = frame->dts() / MUTE_ADTS_DATA_MS;
					if (audio_idx != m_audioIindex)
					{
						_muteAudioPool->emplace_back(std::make_shared<FrameFromPtr>(CodecAAC, (char *)MUTE_ADTS_DATA, MUTE_ADTS_DATA_LEN, m_audioIindex * MUTE_ADTS_DATA_MS, 0, ADTS_HEADER_LEN));
						m_audioIindex = audio_idx;
					}
					
				}
			}

		});

		if (_muteAudioPool)
		{//如果生成了静音轨,打包发送静音轨
			_muteAudioPool->for_each([&](const Frame::Ptr &frame)
			{
				MSEErrCode muxerRs = _muxerClient->inputFrame(frame, true);
				if (muxerRs == MSEErrCode_Success)
				{
					if (_muxerClient->m_FrameLen > 0)
					{
						auto msg = _hRawFramePool.obtain();
						msg->setCapacity(_muxerClient->m_FrameLen);
						msg->setSize(_muxerClient->m_FrameLen);
						memcpy(msg->data(), _muxerClient->m_Fmp4Frame, _muxerClient->m_FrameLen);
						if (_testFile)
							fwrite(_muxerClient->m_Fmp4Frame, _muxerClient->m_FrameLen, 1, _testFile);
						int sendRs = send(msg, WebSocketHeader::BINARY);
					}
					else
					{
						ErrorL << "processAVFrame send Fmp4Frame error! m_HeaderLen " << _muxerClient->m_HeaderLen;
						return;
					}
				}
			});

			_muteAudioPool->clear();
		}
		

	}
	void MSESession::stopPlay()
	{
		//MSESession析构时会自动释放! 不用处理stop事件

	}

	int MSESession::totalReaderCount(MediaSource &sender)
	{
		return 0;
	}

	//Frame 中的数据逐个NAL回掉上来的。比如H264 I Frame会被拆分为 SPS,PPS,IDR三个甚至更多个NAL多次回掉上来
	void MSESession::processH264Frame(const Frame::Ptr &frame)
	{
		auto pcData = frame->data() + frame->prefixSize();
		//auto iLen = frame->size() - frame->prefixSize();
		auto type = H264_TYPE(((uint8_t*)pcData)[0]);
		if (type == H264Frame::NAL_SPS)
		{
			_sps = string(frame->data(), frame->size());
			return;
		}

		if (type == H264Frame::NAL_PPS)
		{
			_pps = string(frame->data(), frame->size());
			return;
		}		

		if (_sps.empty() || _pps.empty())
		{
			WarnL << "processH264Frame waitting i frame!!";
			return;
		}

		if (!_muxerClient)
		{
#if 1		//通过函数getAVCInfo计算出来的帧率有时不正确,需要外部配置
			int width = 1920;
			int height = 1080;
			float frameRate = 25;
			string spsSkipHeader(_sps.c_str()+4);//skil 00 00 00 01
			getAVCInfo(spsSkipHeader, width, height, frameRate);
#endif

			string kStreamId = _media_info._app + "_" + _media_info._streamid+".frameRate";
			if (mINI::Instance().count(kStreamId) > 0)
			{//配置了流信息
				frameRate = mINI::Instance()[kStreamId];
				if (frameRate > 0)
				{
					m_ptsInterval = 1000 / frameRate;
				}				
				WarnL << "MSESession::processH264Frame use frameRatte " << frameRate << " " << m_ptsInterval;
			}
			

			WarnL << "MSESession::processH264Frame " << " FrameRate:" << frameRate;
			_muxerClient = std::make_shared<Fmp4MuxerClient>(width, height);
		}

		if (!_muxerClient)
		{
			ErrorL << "MSESession::processH264Frame create mutexerClient Error";
			return;
		}

		BufferRaw::Ptr frameToMutex;
		//把 SPS,PPS,IDR拼接起来
		if (type == H264Frame::NAL_IDR)
		{
			frameToMutex = _hRawFramePool.obtain();
			frameToMutex->setCapacity(_sps.size() + _pps.size() + frame->size());
			frameToMutex->setSize(_sps.size() + _pps.size() + frame->size());
			char* data = frameToMutex->data();
			memcpy(data, _sps.data(), _sps.size());
			memcpy(data+_sps.size(),_pps.data(), _pps.size());
			memcpy(data+ _sps.size()+_pps.size(), frame->data(), frame->size());
		}
		else if(type == H264Frame::NAL_B_P)
		{
			frameToMutex = _hRawFramePool.obtain();
			frameToMutex->setCapacity(frame->size());
			frameToMutex->setSize(frame->size());
			char* data = frameToMutex->data();
			memcpy(data, frame->data(), frame->size());
		}

		if (type == H264Frame::NAL_IDR || type == H264Frame::NAL_B_P)
		{
			TraceL << " FramePts:" << frame->pts() << " FrameDts" << frame->dts();
			MSEErrCode muxerRs = MSEErrCode_Success;
			if (type == H264Frame::NAL_IDR)
			{
				if ((muxerRs = _muxerClient->muxer((unsigned char*)frameToMutex->data(),
					frameToMutex->size(), 
					true,
					m_ptsInterval > 0 ? (m_pts += 40) : frame->pts(),
					m_ptsInterval > 0 ? (m_pts += 40) : frame->dts(),
					CodecH264)) < 0)
				{
					InfoL << "processAVFrame muxer NAL_IDR error=" << muxerRs; 					
				}
			}
			else
			{
				if ((muxerRs = _muxerClient->muxer((unsigned char*)frameToMutex->data(),
					frameToMutex->size(),
					false,
					m_ptsInterval > 0 ? (m_pts += 40) : frame->pts(),
					m_ptsInterval > 0 ? (m_pts += 40) : frame->dts(),
					CodecH264)) < 0)
				{
					InfoL << "processAVFrame muxer NAL_PORB error=" << muxerRs;
				}
			}

			if (muxerRs != MSEErrCode_Success	//muxer失败
				&&  muxerRs != MSEErrCode_NoFmp4Header) //没有fmp4头，流刚刚开启时必定返回该错误
				//|| muxerRs != MSEErrCode_NoMediaFrame//没有视频分片，如果是GOP模式组帧，没有得到完整GOP之前该逻辑必然出现
			{//fmp4Muxer异常后，丢弃所有非I帧
				_sps.clear();
				_pps.clear();
				return;
			}


			if (!_muxerClient->m_SendFmp4Header)
			{
				if (_muxerClient->m_HeaderLen > 0)
				{
					_muxerClient->m_SendFmp4Header = true;
					auto msg = _hRawFramePool.obtain();
					msg->setCapacity(_muxerClient->m_HeaderLen);
					msg->setSize(_muxerClient->m_HeaderLen);
					memcpy(msg->data(), _muxerClient->m_Fmp4Header, _muxerClient->m_HeaderLen);
					int sendRs = send(msg, WebSocketHeader::BINARY);
				}
				else
				{
					ErrorL << "processAVFrame send Fmp4Header error! headerLen " << _muxerClient->m_HeaderLen;
					return;
				}
			}


			if (_muxerClient->m_SendFmp4Header)
			{
				if (_muxerClient->m_FrameLen > 0)
				{
					auto msg = _hRawFramePool.obtain();
					msg->setCapacity(_muxerClient->m_FrameLen);
					msg->setSize(_muxerClient->m_FrameLen);
					memcpy(msg->data(), _muxerClient->m_Fmp4Frame, _muxerClient->m_FrameLen);
					int sendRs = send(msg, WebSocketHeader::BINARY);
				}
				else
				{
					ErrorL << "processAVFrame send Fmp4Frame error! m_HeaderLen " << _muxerClient->m_HeaderLen;
					return;
				}
			}
		}
		else
		{
			WarnL << "drop unknow nalu type!";
		}
		

		//if (m_pts != notify->pts() / 1000)
		//{
		//	r = calcVideoInfo((std::shared_ptr<void>&)it->first);
		//	if (r != 0)
		//	{
		//		ErrLog(<< "[MediaRequest::dispatch] client=" << ptr.lock() << " sendText error=" << r);
		//	}
		//}
	}

	//不支持
	void MSESession::processH265Frame(const Frame::Ptr &frame)
	{

	}

	//不支持
	void MSESession::processAACFrame(const Frame::Ptr &frame)
	{

	}
}


