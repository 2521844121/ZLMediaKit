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

#include "RawMuxer.h"
#include "RawMediaSource.h"

namespace mediakit {

class RawMediaSourceMuxer : public RawMuxer {
public:
    typedef std::shared_ptr<RawMediaSourceMuxer> Ptr;

    RawMediaSourceMuxer(const string &vhost,
                         const string &strApp,
                         const string &strId,
                         const TitleSdp::Ptr &title = nullptr) : RawMuxer(title){
        _mediaSouce = std::make_shared<RawMediaSource>(vhost,strApp,strId);
        getRawRing()->setDelegate(_mediaSouce);
    }
    virtual ~RawMediaSourceMuxer(){}

    void setListener(const std::weak_ptr<MediaSourceEvent> &listener){
        _mediaSouce->setListener(listener);
    }

    int readerCount() const{
        return _mediaSouce->readerCount();
    }

    void setTimeStamp(uint32_t stamp){
        _mediaSouce->setTimeStamp(stamp);
    }

    void onAllTrackReady(){
        //_mediaSouce->setSdp(getSdp());
    }

    // 设置TrackSource
    void setTrackSource(const std::weak_ptr<TrackSource> &track_src){
        //_mediaSouce->setTrackSource(track_src);
    }
private:
    RawMediaSource::Ptr _mediaSouce;
};


}//namespace mediakit
