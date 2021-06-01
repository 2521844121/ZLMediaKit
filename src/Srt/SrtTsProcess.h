/*
 * Copyright (c) 2016 The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/xia-chu/ZLMediaKit).
 *
 * Use of this source code is governed by MIT license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#ifndef ZLMEDIAKIT_SRTTSROCESS_H
#define ZLMEDIAKIT_SRTTSROCESS_H

#if defined(ENABLE_SRTTS)

#include "Rtp/Decoder.h"
#include "Rtsp/RtpCodec.h"
#include "Rtsp/RtpReceiver.h"
#include "Http/HttpRequestSplitter.h"
#include "Common/MultiMediaSourceMuxer.h"
#include "srt/srt.h"

namespace mediakit{

class SrtTsProcess : public SockInfo,
	public MediaSinkInterface,
	public MediaSourceEventInterceptor, 
	public HttpRequestSplitter,
	public std::enable_shared_from_this<SrtTsProcess>{
public:
    typedef std::shared_ptr<SrtTsProcess> Ptr;
    SrtTsProcess(const string &vhost, const string &app, const string &streamId);
    ~SrtTsProcess() override;

    /**
     * 输入TS数据
     * @param data ts数据
     * @param data_len 数据长度
     * @return 是否解析成功
     */
    bool inputTs(const SRTSOCKET srtSocket, const char *data, size_t len);


	/// SockInfo override
	string get_local_ip() override;
	uint16_t get_local_port() override;
	string get_peer_ip() override;
	uint16_t get_peer_port() override;
	string getIdentifier() const override;

	/**
	 * 是否超时，用于超时移除对象
	 */
	bool alive();

	int getTotalReaderCount();

	void setListener(const std::weak_ptr<MediaSourceEvent> &listener);

	/**
	 * 超时时被UdpTsSelector移除时触发
	 */
	void onDetach();

	/**
	 * 设置onDetach事件回调
	 */
	void setOnDetach(const function<void()> &cb);

	/**
	 * 设置onDetach事件回调,false检查RTP超时，true停止
	 */
	void setStopCheck(bool is_check = false);

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
	uint32_t _dts = 0;
	uint64_t _total_bytes = 0;
	Ticker _last_frame_time;

	atomic_bool _stop_check{ false };
	atomic_flag _busy_flag{ false };
	Ticker _last_check_alive;

	function<void()> _on_detach = nullptr;

	SRTSOCKET _srtSocket = -1;
};

}//namespace mediakit
#endif//defined(ENABLE_SRTTS)
#endif //ZLMEDIAKIT_SRTTSROCESS_H
