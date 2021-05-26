/*
 * Copyright (c) 2016 The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/xia-chu/ZLMediaKit).
 *
 * Use of this source code is governed by MIT license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#ifndef ZLMEDIAKIT_UDPTSSELECTOR_H
#define ZLMEDIAKIT_UDPTSSELECTOR_H

#if defined(ENABLE_UDPTS)
#include <stdint.h>
#include <mutex>
#include <unordered_map>
#include "UdpTsProcess.h"
#include "Common/MediaSource.h"

namespace mediakit{

class UdpTsSelector;
class UdpTsProcessHelper : public MediaSourceEvent , public std::enable_shared_from_this<UdpTsProcessHelper> {
public:
    typedef std::shared_ptr<UdpTsProcessHelper> Ptr;
    UdpTsProcessHelper(const string &stream_id, const weak_ptr<UdpTsSelector > &parent);
    ~UdpTsProcessHelper();
    void attachEvent();
    UdpTsProcess::Ptr & getProcess();

protected:
    // 通知其停止推流
    bool close(MediaSource &sender,bool force) override;
    // 观看总人数
    int totalReaderCount(MediaSource &sender) override;

private:
    weak_ptr<UdpTsSelector > _parent;
    UdpTsProcess::Ptr _process;
    string _stream_id;
};

class UdpTsSelector : public std::enable_shared_from_this<UdpTsSelector>{
public:
    UdpTsSelector();
    ~UdpTsSelector();

    static UdpTsSelector &Instance();

    /**
     * 清空所有对象
     */
    void clear();

    /**
     * 获取一个rtp处理器
     * @param stream_id 流id
     * @param makeNew 不存在时是否新建
     * @return rtp处理器
     */
    UdpTsProcess::Ptr getProcess(const string &stream_id, bool makeNew = true);

    /**
     * 删除rtp处理器
     * @param stream_id 流id
     * @param ptr rtp处理器指针
     */
    void delProcess(const string &stream_id, const UdpTsProcess *ptr);

private:
    void onManager();
    void createTimer();

private:
    Timer::Ptr _timer;
    recursive_mutex _mtx_map;
    unordered_map<string,UdpTsProcessHelper::Ptr> _map_process;
};

}//namespace mediakit
#endif//defined(ENABLE_UDPTS)
#endif //ZLMEDIAKIT_UDPTSSELECTOR_H
