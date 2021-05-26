/*
 * Copyright (c) 2016 The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/xia-chu/ZLMediaKit).
 *
 * Use of this source code is governed by MIT license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#if defined(ENABLE_UDPTS)
#include "UdpTsProcess.h"
#include "Util/File.h"
#include "Http/HttpTSPlayer.h"
#include "Extension/CommonRtp.h"
#include "Extension/H264Rtp.h"

#define UDP_APP_NAME "udp"

namespace mediakit{

//判断是否为ts负载
static inline bool checkTS(const uint8_t *packet, size_t bytes){
    return bytes % TS_PACKET_SIZE == 0 && packet[0] == TS_SYNC_BYTE;
}

UdpTsProcess::UdpTsProcess(const string &stream_id) {
	//assert(interface);
	_media_info._schema = UDP_APP_NAME;
	_media_info._vhost = DEFAULT_VHOST;
	_media_info._app = UDP_APP_NAME;
	_media_info._streamid = stream_id;

	GET_CONFIG(string, dump_dir, UdpTs::kDumpDir);
	{
		FILE *fp = !dump_dir.empty() ? File::create_file(File::absolutePath(_media_info._streamid + ".video.ts", dump_dir).data(), "wb") : nullptr;
		if (fp) {
			_save_file_ts.reset(fp, [](FILE *fp) {
				fclose(fp);
			});
		}
	}

	_decoder = DecoderImp::createDecoder(DecoderImp::decoder_ts, this);
}

UdpTsProcess::~UdpTsProcess() 
{
	uint64_t duration = (_last_frame_time.createdTime() - _last_frame_time.elapsedTime()) / 1000;
	WarnP(this) << "UDP推流器("
		<< _media_info._vhost << "/"
		<< _media_info._app << "/"
		<< _media_info._streamid
		<< ")断开,耗时(s):" << duration;

	//流量统计事件广播
	GET_CONFIG(uint32_t, iFlowThreshold, General::kFlowThreshold);
	if (_total_bytes >= iFlowThreshold * 1024) {
		NoticeCenter::Instance().emitEvent(Broadcast::kBroadcastFlowReport, _media_info, _total_bytes, duration, false, static_cast<SockInfo &>(*this));
	}
}

//mpeg-ts传输的时 1316(188*7)固定大小数据。每个udp包中有7个ts媒体数据
bool UdpTsProcess::inputUdp(const Socket::Ptr &sock, const char *data, size_t len, const struct sockaddr *addr, uint32_t *dts_out)
{
	//ErrorL << " input udp " << len;
	if (!_sock)
	{//收到第一帧媒体数据,鉴权并创建rtsp,rtmp等资源
		_sock = sock;
		_addr = *addr;
		emitOnPublish();
	}

	if (!_muxer)
	{//鉴权失败		
		return false;
	}

	_total_bytes += len;

	if (_save_file_ts && len>0) {
		fwrite((uint8_t *)data, len, 1, _save_file_ts.get());
	}

	//把data拆分成188大小的ts包,拆分成功后进入onSearchPacketTail
	HttpRequestSplitter::input(data, len);
	return true;
}


/// SockInfo override
string UdpTsProcess::get_local_ip()
{
	if (_sock)
	{
		return _sock->get_local_ip();
	}
	return "";
}

uint16_t UdpTsProcess::get_local_port()
{
	if (_sock)
	{
		return _sock->get_local_port();
	}
	return 0;
}

string UdpTsProcess::get_peer_ip() 
{
	return SockUtil::inet_ntoa(((struct sockaddr_in &) _addr).sin_addr);
}

uint16_t UdpTsProcess::get_peer_port() 
{
	return ntohs(((struct sockaddr_in &) _addr).sin_port);
}


string UdpTsProcess::getIdentifier() const
{
	return _media_info._streamid;
}

//是否需要对UDP收到的TS包做排序??
//void UdpTsProcess::onRtpSorted(RtpPacket::Ptr rtp, int) {
//    auto pt = rtp->getHeader()->pt;
//    if (!_rtp_decoder) {
//        switch (pt) {
//            case 98: {
//                //H264负载
//                _rtp_decoder = std::make_shared<H264RtpDecoder>();
//                _interface->addTrack(std::make_shared<H264Track>());
//                break;
//            }
//            default: {
//                if (pt != 33 && pt != 96) {
//                    WarnL << "rtp payload type未识别(" << (int) pt << "),已按ts或ps负载处理";
//                }
//                //ts或ps负载
//                _rtp_decoder = std::make_shared<CommonRtpDecoder>(CodecInvalid, 32 * 1024);
//                //设置dump目录
//                GET_CONFIG(string, dump_dir, UdpTs::kDumpDir);
//                if (!dump_dir.empty()) {
//                    auto save_path = File::absolutePath(_media_info._streamid + ".mp2", dump_dir);
//                    _save_file_ps.reset(File::create_file(save_path.data(), "wb"), [](FILE *fp) {
//                        if (fp) {
//                            fclose(fp);
//                        }
//                    });
//                }
//                break;
//            }
//        }
//
//        //设置frame回调
//        _rtp_decoder->addDelegate(std::make_shared<FrameWriterInterfaceHelper>([this](const Frame::Ptr &frame) {
//            onRtpDecode(frame);
//        }));
//    }
//
//    //解码rtp
//    _rtp_decoder->inputRtp(rtp, false);
//}

const char *UdpTsProcess::onSearchPacketTail(const char *packet,size_t bytes){
    try {
		//从ts包中提取音视频帧数据,提取到音视频数据后会分别接入addTrack, addTrackCompleted, inputFrame
        auto ret = _decoder->input((uint8_t *) packet, bytes);
        if (ret >= 0) {
            //解析成功全部或部分
            return packet + ret;
        }
        //解析失败，丢弃所有数据
        return packet + bytes;
    } catch (std::exception &ex) {
        InfoL << "解析ps或ts异常: bytes=" << bytes
              << " ,exception=" << ex.what()
              << " ,hex=" << hexdump((uint8_t *) packet, MIN(bytes,32));
        if (remainDataSize() > 256 * 1024) {
            //缓存太多数据无法处理则上抛异常
            throw;
        }
        return nullptr;
    }
}

void UdpTsProcess::emitOnPublish()
{
	weak_ptr<UdpTsProcess> weak_self = shared_from_this();
	Broadcast::PublishAuthInvoker invoker = [weak_self](const string &err, bool enableHls, bool enableMP4) {
		auto strongSelf = weak_self.lock();
		if (!strongSelf) {
			return;
		}
		if (err.empty()) {
			strongSelf->_muxer = std::make_shared<MultiMediaSourceMuxer>(strongSelf->_media_info._vhost,
				strongSelf->_media_info._app,
				strongSelf->_media_info._streamid, 0.0f,
				true, true, enableHls, enableMP4);
			strongSelf->_muxer->setMediaListener(strongSelf);
			InfoP(strongSelf) << "允许UDP推流";
		}
		else {
			WarnP(strongSelf) << "禁止UDP推流:" << err;
		}
	};

	//触发推流鉴权事件
	auto flag = NoticeCenter::Instance().emitEvent(Broadcast::kBroadcastMediaPublish, _media_info, invoker, static_cast<SockInfo &>(*this));
	if (!flag) {
		//该事件无人监听,默认不鉴权
		GET_CONFIG(bool, toHls, General::kPublishToHls);
		GET_CONFIG(bool, toMP4, General::kPublishToMP4);
		invoker("", toHls, toMP4);
	}
}


void UdpTsProcess::inputFrame(const Frame::Ptr &frame) {
	_last_frame_time.resetTime();
	_dts = frame->dts();
	_muxer->inputFrame(frame);
}

void UdpTsProcess::addTrack(const Track::Ptr &track) {
	_muxer->addTrack(track);
}

void UdpTsProcess::addTrackCompleted() {
	_muxer->addTrackCompleted();
}

//// MediaSourceEvent override ////
MediaOriginType UdpTsProcess::getOriginType(MediaSource &sender) const
{
	return MediaOriginType::rtp_push;
}

string UdpTsProcess::getOriginUrl(MediaSource &sender) const
{
	return _media_info._full_url;
}

std::shared_ptr<SockInfo> UdpTsProcess::getOriginSock(MediaSource &sender) const
{
	return const_cast<UdpTsProcess *>(this)->shared_from_this();
}

bool UdpTsProcess::alive()
{
	if (_stop_check.load()) {
		if (_last_check_alive.elapsedTime() > 5 * 60 * 1000) {
			//最多暂停5分钟的rtp超时检测，因为NAT映射有效期一般不会太长
			_stop_check = false;
		}
		else {
			return true;
		}
	}

	_last_check_alive.resetTime();
	GET_CONFIG(uint64_t, timeoutSec, UdpTs::kTimeoutSec)
	if (_last_frame_time.elapsedTime() / 1000 < timeoutSec) {
		return true;
	}

	return false;
}

int UdpTsProcess::getTotalReaderCount()
{
	return _muxer ? _muxer->totalReaderCount() : 0;
}

void UdpTsProcess::setListener(const std::weak_ptr<MediaSourceEvent> &listener)
{
	//listener被设置为 UdpTsProcessHelper,捕获close等事件,交给UdpTsProcessHelper处理
	setDelegate(listener);
}

void UdpTsProcess::setStopCheck(bool is_check) {
	_stop_check = is_check;
	if (!is_check) {
		_last_frame_time.resetTime();
	}
}

void UdpTsProcess::onDetach() {

	if (_on_detach) {
		_on_detach();
	}
}

void UdpTsProcess::setOnDetach(const function<void()> &cb) {
	_on_detach = cb;
}




}//namespace mediakit
#endif//defined(ENABLE_RTPPROXY)
