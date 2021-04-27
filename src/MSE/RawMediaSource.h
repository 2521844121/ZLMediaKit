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

#include <mutex>
#include <string>
#include <memory>
#include <functional>
#include <unordered_map>
#include "Common/config.h"
#include "Common/MediaSource.h"
#include "Util/logger.h"
#include "Util/RingBuffer.h"
#include "Util/TimeTicker.h"
#include "Util/ResourcePool.h"
#include "Util/NoticeCenter.h"
#include "Thread/ThreadPool.h"

using namespace std;
using namespace toolkit;
#define RAW_GOP_SIZE 512
namespace mediakit {


//缓存刷新策略类
class FlushPolicyMse:public FlushPolicy
{
	public:
		FlushPolicyMse() = default;
		~FlushPolicyMse() = default;

		uint32_t getStamp(const Frame::Ptr &packet) {
			return packet->pts();
		}
};

/**
 * 音视频裸媒体源
 * 存放ZLMediatKit Server通过RTSP推拉流,RTMP推拉流,DevChannel得到的所有原始音视频数据
 */
class RawMediaSource : public MediaSource, public RingDelegate<Frame::Ptr>, public PacketCache<Frame, FlushPolicyMse> {
public:
    typedef ResourcePool<Frame> PoolType;
    typedef std::shared_ptr<RawMediaSource> Ptr;
    typedef std::shared_ptr<List<Frame::Ptr> > RingDataType;
    typedef RingBuffer<RingDataType> RingType;

    /**
     * 构造函数
     * @param vhost 虚拟主机名
     * @param app 应用名
     * @param stream_id 流id
     * @param ring_size 可以设置固定的环形缓冲大小，0则自适应
     */
    RawMediaSource(const string &vhost,
                    const string &app,
                    const string &stream_id,
                    int ring_size = RAW_GOP_SIZE) :
            MediaSource(RAW_SCHEMA, vhost, app, stream_id), _ring_size(ring_size) {}

    virtual ~RawMediaSource() {}

    /**
     * 获取媒体源的环形缓冲
     */
    const RingType::Ptr &getRing() const {
        return _ring;
    }

    /**
     * 获取播放器个数
     */
    int readerCount() override {
        return _ring ? _ring->readerCount() : 0;
    }


    /**
     * 获取相应轨道的时间戳，单位毫秒
     */
    uint32_t getTimeStamp(TrackType trackType) override 
	{
		//区分track获取时间戳??
		return _time_stamp;
    }

    /**
     * 更新时间戳
     */
     void setTimeStamp(uint32_t uiStamp) override 
	 {
		 _time_stamp = uiStamp;
    }


    /**
     * 输入音视频裸数据
     * @param pkt 音视频数据
     * @param key 该数据是否为I关键帧
     */
    void onWrite(Frame::Ptr pkt, bool key) override 
	{
		//lock_guard<recursive_mutex> lock(_mtx);
		if (pkt->getTrackType() == TrackVideo) 
		{
			//有视频，那么启用GOP缓存
			_have_video = true;
		}


		//保存当前时间戳
		_track_stamps_map[pkt->getTrackType()] = pkt->pts();

		if (!_ring) 
		{
			weak_ptr<RawMediaSource> weakSelf = dynamic_pointer_cast<RawMediaSource>(shared_from_this());
			auto lam = [weakSelf](int size) {
				auto strongSelf = weakSelf.lock();
				if (!strongSelf) {
					return;
				}
				strongSelf->onReaderChanged(size);
			};

			//MSE包缓存最大允许512个，如果是纯视频(25fps)大概为20秒数据
			//但是这个是GOP缓存的上限值，真实的GOP缓存大小等于两个I帧之间的包数的两倍
			//而且每次遇到I帧，则会清空GOP缓存，所以真实的GOP缓存远小于此值
			_ring = std::make_shared<RingType>(_ring_size, std::move(lam));	
			onReaderChanged(0);
			regist();

		}
		PacketCache<Frame, FlushPolicyMse>::inputPacket(pkt->pts(), pkt->getTrackType() == TrackVideo, pkt, key);
    }

private:

    /**
     * 批量flush 数据包时触发该函数
     * @param rtp_list rtp包列表
     * @param key_pos 是否包含关键帧
     */
	virtual void onFlush(std::shared_ptr<List<Frame::Ptr>> pkt_list, bool key_pos) override
	{
        //如果不存在视频，那么就没有存在GOP缓存的意义，所以is_key一直为true确保一直清空GOP缓存
        _ring->write(pkt_list, _have_video ? key_pos : true);
    }

    /**
     * 每次增减消费者都会触发该函数
     */
    void onReaderChanged(int size) 
	{
        if (size == 0) {
            //onNoneReader();
        }
    }
private:
    int _ring_size;
    bool _have_video = false;
    RingType::Ptr _ring;
	unordered_map<int, uint32_t> _track_stamps_map;
	//时间戳，单位毫秒
	uint32_t _time_stamp = 0;
};

} /* namespace mediakit */

