/*
 * Copyright (c) 2016 The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/xiongziliang/ZLMediaKit).
 *
 * Use of this source code is governed by MIT license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#include "RawMuxer.h"
namespace mediakit {

RawMuxer::RawMuxer(const TitleSdp::Ptr &title)
{
    _rawRing = std::make_shared<RawRing::RingType>();
}

void RawMuxer::addTrack(const Track::Ptr &track) {
	//RawMuxer的音视频数据直接透传,不需要解复用
    //auto &encoder = _encoder[track->getTrackType()];
    //encoder = FactoryMse::getMseCodecByTrack(track, true);
    //if (!encoder) {
    //    return;
    //}

    ////设置MSE流的输出缓存输出环形缓存
    //encoder->setMseRing(_rawRing);
}

void RawMuxer::inputFrame(const Frame::Ptr &frame) 
{
	if (_rawRing)
	{
		_rawRing->write(frame);
	}
}

RawRing::RingType::Ptr RawMuxer::getRawRing() const {
    return _rawRing;
}

void RawMuxer::resetTracks() {

}


} /* namespace mediakit */