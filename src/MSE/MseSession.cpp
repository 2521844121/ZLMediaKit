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
namespace mediakit
{
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
		pack->for_each([&](const Frame::Ptr &frame)
		{			
			CodecId codec = frame->getCodecId();
			switch (codec)
			{
			case CodecH264:
				processH264Frame(frame);
				break;
			case CodecH265:
				processH265Frame(frame);
				break;
			case CodecAAC:
				processAACFrame(frame);
				break;
			default:
				WarnL << "processAVFrame unknow codec type " << codec;
				break;
			}

		});

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
			ErrorL << "processH264Frame waitting i frame!!";
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
			MSEErrCode muxerRs = MSEErrCode_Success;
			if (type == H264Frame::NAL_IDR)
			{
				if ((muxerRs = _muxerClient.muxer((unsigned char*)frameToMutex->data(), frameToMutex->size(), true, m_pts += 40, CodecH264)) < 0)
				{
					InfoL << "processAVFrame muxer NAL_IDR error=" << muxerRs; 					
				}
			}
			else
			{
				if ((muxerRs = _muxerClient.muxer((unsigned char*)frameToMutex->data(), frameToMutex->size(), false, m_pts += 40, CodecH264)) < 0)
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


			if (!_muxerClient.m_SendFmp4Header)
			{
				if (_muxerClient.m_HeaderLen > 0)
				{
					_muxerClient.m_SendFmp4Header = true;
					auto msg = _hRawFramePool.obtain();
					msg->setCapacity(_muxerClient.m_HeaderLen);
					msg->setSize(_muxerClient.m_HeaderLen);
					memcpy(msg->data(), _muxerClient.m_Fmp4Header, _muxerClient.m_HeaderLen);
					int sendRs = send(msg, WebSocketHeader::BINARY);
				}
				else
				{
					ErrorL << "processAVFrame send Fmp4Header error! headerLen " << _muxerClient.m_HeaderLen;
					return;
				}
			}


			if (_muxerClient.m_SendFmp4Header)
			{
				if (_muxerClient.m_FrameLen > 0)
				{
					auto msg = _hRawFramePool.obtain();
					msg->setCapacity(_muxerClient.m_FrameLen);
					msg->setSize(_muxerClient.m_FrameLen);
					memcpy(msg->data(), _muxerClient.m_Fmp4Frame, _muxerClient.m_FrameLen);
					int sendRs = send(msg, WebSocketHeader::BINARY);
				}
				else
				{
					ErrorL << "processAVFrame send Fmp4Frame error! m_HeaderLen " << _muxerClient.m_HeaderLen;
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


