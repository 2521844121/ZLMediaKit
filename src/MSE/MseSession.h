/*
 * Copyright (c) 2016 The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/xiongziliang/ZLMediaKit).
 *
 * Use of this source code is governed by MIT license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#pragma once
#include "WebSocket.h"
#include "RawMediaSource.h"
#include "Fmp4MuxerClient.h"
#include "Util/ResourcePool.h"
#include "Extension/H264.h"
#include "Extension/H265.h"

using namespace std;
using namespace toolkit;
using namespace mediakit;

namespace mediakit 
{

	//TcpSessionTypeImp 继承EchoSessionWithUrl， 继承TCPSession
	class MSESession : public WebSocketSession, public MediaSourceEvent
	{
	public:
		MSESession(const Socket::Ptr &pSock) : 
			WebSocketSession(pSock) {}

		virtual ~MSESession() {	}

		void attachServer(const TcpServer &server) override;

		void onRecv(const Buffer::Ptr &buffer) override;

		void onRecv(const Buffer::Ptr &buf, const WebSocketHeader::Type& type) override;

		void onError(const SockException &err) override;

		void onClose() override;

		void onManager() override;

		void startPlay();

		void processAVFrame(const RawMediaSource::RingDataType &pack);

		void stopPlay();

		int totalReaderCount(MediaSource &sender) override;

	private:
		void processH264Frame(const Frame::Ptr &frame);
		void processH265Frame(const Frame::Ptr &frame);
		void processAACFrame(const Frame::Ptr &frame);

	private:
		//url解析后保存的相关信息
		MediaInfo _media_info;

		//rtsp播放器绑定的直播源
		std::weak_ptr<RawMediaSource> _play_src;

		//直播源读取器
		RawMediaSource::RingType::RingReader::Ptr _play_reader;

		Fmp4MuxerClient _muxerClient;

		unsigned int m_pts = 0;

		//H264
		string _sps;

		string _pps;

		//ResourcePoolHelper<H264Frame> _h264FramePool;

		//ResourcePoolHelper<H265Frame> _h265FramePool;

		ResourcePool_l<BufferRaw> _hRawFramePool;
	};

	struct MseSessionCreator
	{
		//返回的TcpSession必须派生于SendInterceptor，可以返回null(拒绝连接)
		WebSocketSession::Ptr operator()(const Parser &header, const HttpSession &parent, const Socket::Ptr &pSock)
		{
			//校验URL合法性
			if (header.Url() == "/")
			{
				return nullptr;
				//return std::make_shared<WebSocketSessionTypeImp<EchoSession> >(header, parent, pSock);
			}
			//检查请求
			return std::make_shared<WebSocketSessionTypeImp<MSESession> >(header, parent, pSock);
		}
	};
}//namespace mediakit
