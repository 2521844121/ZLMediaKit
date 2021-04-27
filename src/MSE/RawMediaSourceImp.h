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


#include "RawMediaSource.h"
#include "Common/MultiMediaSourceMuxer.h"
using namespace toolkit;

namespace mediakit {
class RawMediaSourceImp : public RawMediaSource, public Demuxer::Listener , public MultiMediaSourceMuxer::Listener  {
public:
    typedef std::shared_ptr<RawMediaSourceImp> Ptr;

    /**
     * 构造函数
     * @param vhost 虚拟主机
     * @param app 应用名
     * @param id 流id
     * @param ringSize 环形缓存大小
     */
    RawMediaSourceImp(const string &vhost, const string &app, const string &id, int ringSize = RTP_GOP_SIZE) : RawMediaSource(vhost, app, id,ringSize) 
	{
        //_demuxer = std::make_shared<MseDemuxer>();
        //_demuxer->setTrackListener(this);
    }

    ~RawMediaSourceImp() = default;

    /**
     * 设置sdp
     */
    void setSdp(const string &strSdp) 
	{
        //RawMediaSource::setSdp(strSdp);
    }

    /**
     * 输入rtp并解析
     */
    void onWrite(const Frame::Ptr pkt, bool key_pos) override {
        if(_all_track_ready && !_muxer->isEnabled())
		{
            //获取到所有Track后，并且未开启转协议，那么不需要解复用
            //在关闭解复用后，无法知道是否为关键帧，这样会导致无法秒开，或者开播花屏
            key_pos = pkt->getTrackType() == TrackVideo;
        }else
		{
            //需要解复用
            //key_pos = _demuxer->inputPacket(rtp);
			ErrorL << "onWrite _demuxer unsupport!";
        }
        RawMediaSource::onWrite(pkt, key_pos);
    }

    /**
     * 设置监听器
     * @param listener
     */
    void setListener(const std::weak_ptr<MediaSourceEvent> &listener) override {
        RawMediaSource::setListener(listener);
        if(_muxer){
            _muxer->setMediaListener(listener);
        }
    }

    /**
     * 获取观看总人数，包括(hls/rtsp/rtmp)
     */
    int totalReaderCount() override{
        return readerCount() + (_muxer ? _muxer->totalReaderCount() : 0);
    }

    /**
     * 设置录制状态
     * @param type 录制类型
     * @param start 开始或停止
     * @param custom_path 开启录制时，指定自定义路径
     * @return 是否设置成功
     */
    bool setupRecord(Recorder::type type, bool start, const string &custom_path) override{
        if(_muxer){
            return _muxer->setupRecord(*this,type, start, custom_path);
        }
        return RawMediaSource::setupRecord(type, start, custom_path);
    }

    /**
     * 获取录制状态
     * @param type 录制类型
     * @return 录制状态
     */
    bool isRecording(Recorder::type type) override{
        if(_muxer){
            return _muxer->isRecording(*this,type);
        }
        return RawMediaSource::isRecording(type);
    }

    /**
     * _demuxer触发的添加Track事件
     */
    void onAddTrack(const Track::Ptr &track) override {
        if(_muxer){
            _muxer->addTrack(track);
            track->addDelegate(_muxer);
        }
    }

    /**
     * _muxer触发的所有Track就绪的事件
     */
    void onAllTrackReady() override{
        setTrackSource(_muxer);
        _all_track_ready = true;
    }
private:
    MultiMediaSourceMuxer::Ptr _muxer;
    bool _all_track_ready = false;
};
} /* namespace mediakit */
