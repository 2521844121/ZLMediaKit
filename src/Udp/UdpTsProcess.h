/*
 * Copyright (c) 2016 The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/xia-chu/ZLMediaKit).
 *
 * Use of this source code is governed by MIT license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#ifndef ZLMEDIAKIT_UDPTSROCESS_H
#define ZLMEDIAKIT_UDPTSROCESS_H

#if defined(ENABLE_UDPTS)

#include "Rtp/Decoder.h"
#include "Rtsp/RtpCodec.h"
#include "Rtsp/RtpReceiver.h"
#include "Http/HttpRequestSplitter.h"
#include "Common/MultiMediaSourceMuxer.h"

namespace mediakit{

class UdpTsProcess : public SockInfo,
	public MediaSinkInterface,
	public MediaSourceEventInterceptor, 
	public HttpRequestSplitter,
	public std::enable_shared_from_this<UdpTsProcess>{
public:
    typedef std::shared_ptr<UdpTsProcess> Ptr;
    UdpTsProcess(const string &stream_id);
    ~UdpTsProcess() override;

    /**
     * 输入rtp
     * @param data rtp数据指针
     * @param data_len rtp数据长度
     * @return 是否解析成功
     */
    bool inputUdp(bool is_udp, const Socket::Ptr &sock, const char *data, size_t len, const struct sockaddr *addr, uint32_t *dts_out = nullptr);


	/// SockInfo override
	string get_local_ip() override;
	uint16_t get_local_port() override;
	string get_peer_ip() override;
	uint16_t get_peer_port() override;
	string getIdentifier() const override;

protected:
    //void onRtpSorted(RtpPacket::Ptr rtp, int track_index) override ; TS 排序??
    const char *onSearchPacketTail(const char *data,size_t len) override;
    ssize_t onRecvHeader(const char *data,size_t len) override { return 0; };

private:
	void emitOnPublish();
	void inputFrame(const Frame::Ptr &frame) override;
	void addTrack(const Track::Ptr & track) override;
	void addTrackCompleted() override;
	void resetTracks() override {};

	//// MediaSourceEvent override ////
	MediaOriginType getOriginType(MediaSource &sender) const  override;
	string getOriginUrl(MediaSource &sender) const override;
	std::shared_ptr<SockInfo> getOriginSock(MediaSource &sender) const override;



private:
    MediaInfo _media_info;
	//解析ts, 组帧
    DecoderImp::Ptr _decoder;
    std::shared_ptr<FILE> _save_file_ts;
    std::shared_ptr<RtpCodec> _rtp_decoder;
	MultiMediaSourceMuxer::Ptr _muxer;
	Socket::Ptr _sock;
	struct sockaddr _addr { 0 };
	uint32_t _dts = 0;
	uint64_t _total_bytes = 0;
	Ticker _last_frame_time;
};

}//namespace mediakit
#endif//defined(ENABLE_UDPTS)
#endif //ZLMEDIAKIT_UDPTSROCESS_H
