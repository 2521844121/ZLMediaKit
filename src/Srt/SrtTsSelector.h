/*
 * Copyright (c) 2016 The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/xia-chu/ZLMediaKit).
 *
 * Use of this source code is governed by MIT license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#ifndef ZLMEDIAKIT_SRTTSSELECTOR_H
#define ZLMEDIAKIT_SRTTSSELECTOR_H

#if defined(ENABLE_SRTTS)
#include <stdint.h>
#include <mutex>
#include <unordered_map>
#include "SrtTsProcess.h"
#include "SrtConn.h"
#include "Common/MediaSource.h"

namespace mediakit{

class SrtTsSelector;
class SrtTsProcessHelper : public MediaSourceEvent , public std::enable_shared_from_this<SrtTsProcessHelper> {
public:
    typedef std::shared_ptr<SrtTsProcessHelper> Ptr;
    SrtTsProcessHelper(const string &subPath, const string &vhost, const string &app, const string &streamId, const weak_ptr<SrtTsSelector > &parent);
    ~SrtTsProcessHelper();
    void attachEvent();
    SrtTsProcess::Ptr & getProcess();

protected:
    // 通知其停止推流
    bool close(MediaSource &sender,bool force) override;
    // 观看总人数
    int totalReaderCount(MediaSource &sender) override;

private:
    weak_ptr<SrtTsSelector > _parent;
    SrtTsProcess::Ptr _process;
	string _vhost;
	string _app;
    string _stream_id;
	string _subPath;
};

class SrtTsSelector : public std::enable_shared_from_this<SrtTsSelector>{
public:
    SrtTsSelector();
    ~SrtTsSelector();

    static SrtTsSelector &Instance();

    /**
     * 清空所有对象
     */
    void clear();

    /**
     * 获取一个SRT处理器
     * @param stream_id 流id
     * @param makeNew 不存在时是否新建
     * @return rtp处理器
     */
	SrtTsProcess::Ptr getProcess(SRT_CONN_PTR& srtConn, bool makeNew = true);

    /**
     * 删除SRT处理器
     * @param subPath 流路径 vhost/app/stream
     * @param ptr srt处理器指针
     */
    void delProcess(const string &subPath, const SrtTsProcess *ptr);

private:
    void onManager();
    void createTimer();

private:
    Timer::Ptr _timer;
    recursive_mutex _mtx_map;
    unordered_map<string,SrtTsProcessHelper::Ptr> _map_process;
};

}//namespace mediakit
#endif//defined(ENABLE_UDPTS)
#endif //ZLMEDIAKIT_SrtTsSelector_H
