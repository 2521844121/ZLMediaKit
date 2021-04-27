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

#include "Extension/Frame.h"
#include "Common/MediaSink.h"
#include "Util/RingBuffer.h"

namespace mediakit{
	
class RawRing{
public:
    typedef std::shared_ptr<RawRing> Ptr;
    typedef RingBuffer<Frame::Ptr> RingType;

    RawRing(){}
    virtual ~RawRing(){}

    /**
     * 获取rtmp环形缓存
     * @return
     */
    virtual RingType::Ptr getMseRing() const{
        return _rawRing;
    }

    /**
     * 设置音视频环形缓存
     * @param ring
     */
    virtual void setRawRing(const RingType::Ptr &ring){
		_rawRing = ring;
    }

    /**
     * 输入音视频包
     * @param pkt 视频包
     * @param key_pos 是否为关键帧
     * @return 是否为关键帧
     */
    virtual bool inputRaw(const Frame::Ptr &pkt, bool key_pos){
        if(_rawRing){
			_rawRing->write(pkt,key_pos);
        }
        return key_pos;
    }
protected:
    RingType::Ptr _rawRing;
};

/**
* 原始数据生成器
*/
class RawMuxer : public MediaSinkInterface{
public:
    typedef std::shared_ptr<RawMuxer> Ptr;

    /**
     * 构造函数
     */
    RawMuxer(const TitleSdp::Ptr &title = nullptr);

    virtual ~RawMuxer(){}

    /**
     * 获取rtp环形缓存
     * @return
     */
    RawRing::RingType::Ptr getRawRing() const;

    /**
     * 添加ready状态的track
     */
    void addTrack(const Track::Ptr & track) override;

    /**
     * 写入帧数据
     * @param frame 帧
     */
    void inputFrame(const Frame::Ptr &frame) override;

    /**
     * 重置所有track
     */
    void resetTracks() override ;
private:
    //string _sdp;
    //MseCodec::Ptr _encoder[TrackMax];
    RawRing::RingType::Ptr _rawRing;
};


} /* namespace mediakit */
