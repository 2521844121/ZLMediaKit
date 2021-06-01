/*
 * Copyright (c) 2016 The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/xia-chu/ZLMediaKit).
 *
 * Use of this source code is governed by MIT license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#if defined(ENABLE_SRTTS)
#include <stddef.h>
#include "SrtTsSelector.h"

namespace mediakit{

INSTANCE_IMP(SrtTsSelector);

void SrtTsSelector::clear(){
    lock_guard<decltype(_mtx_map)> lck(_mtx_map);
	_map_process.clear();
}


SrtTsProcess::Ptr SrtTsSelector::getProcess(SRT_CONN_PTR& srtConn, bool makeNew)
{
	lock_guard<decltype(_mtx_map)> lck(_mtx_map);
	auto it = _map_process.find(srtConn->get_subpath());// vhost/app/streamId
	if (it == _map_process.end() && !makeNew) {
		return nullptr;
	}

	SrtTsProcessHelper::Ptr &ref = _map_process[srtConn->get_subpath()];
	if (!ref) {
		ref = std::make_shared<SrtTsProcessHelper>(srtConn->get_subpath(), srtConn->get_vhost(), srtConn->get_app(), srtConn->get_streamid(), shared_from_this());
		ref->attachEvent();
		createTimer();
	}
	return ref->getProcess();
}

void SrtTsSelector::createTimer() {
    if (!_timer) {
        //创建超时管理定时器
        weak_ptr<SrtTsSelector> weakSelf = shared_from_this();
        _timer = std::make_shared<Timer>(3.0f, [weakSelf] {
            auto strongSelf = weakSelf.lock();
            if (!strongSelf) {
                return false;
            }
            strongSelf->onManager();
            return true;
        }, EventPollerPool::Instance().getPoller());
    }
}

void SrtTsSelector::delProcess(const string &subPath,const SrtTsProcess *ptr) {
    SrtTsProcess::Ptr process;
    {
        lock_guard<decltype(_mtx_map)> lck(_mtx_map);
        auto it = _map_process.find(subPath);
        if (it == _map_process.end()) {
            return;
        }
        if (it->second->getProcess().get() != ptr) {
            return;
        }
        process = it->second->getProcess();
		_map_process.erase(it);
    }
    process->onDetach();
}

void SrtTsSelector::onManager() {
    List<SrtTsProcess::Ptr> clear_list;
    {
        lock_guard<decltype(_mtx_map)> lck(_mtx_map);
        for (auto it = _map_process.begin(); it != _map_process.end();) {
            if (it->second->getProcess()->alive()) {
                ++it;
                continue;
            }
            WarnL << "SrtTsProcess timeout:" << it->first;
            clear_list.emplace_back(it->second->getProcess());
            it = _map_process.erase(it);
        }
    }

    clear_list.for_each([](const SrtTsProcess::Ptr &process) {
        process->onDetach();
    });
}

SrtTsSelector::SrtTsSelector() {
}

SrtTsSelector::~SrtTsSelector() {
}

SrtTsProcessHelper::SrtTsProcessHelper(const string &subPath, const string &vhost, const string &app, const string &streamId, const weak_ptr<SrtTsSelector> &parent) {
	_subPath = subPath;
	_vhost = vhost;
    _app = app;
	_stream_id = streamId;
    _process = std::make_shared<SrtTsProcess>(vhost, app, streamId);
}

SrtTsProcessHelper::~SrtTsProcessHelper() {
}

void SrtTsProcessHelper::attachEvent() {
    _process->setListener(shared_from_this());
}

bool SrtTsProcessHelper::close(MediaSource &sender, bool force) {
    //此回调在其他线程触发
    if (!_process || (!force && _process->getTotalReaderCount())) {
        return false;
    }
    auto parent = _parent.lock();
    if (!parent) {
        return false;
    }
    parent->delProcess(_subPath, _process.get());
    WarnL << "close media:" << sender.getSchema() << "/" << sender.getVhost() << "/" << sender.getApp() << "/" << sender.getId() << " " << force;
    return true;
}

int SrtTsProcessHelper::totalReaderCount(MediaSource &sender) {
    return _process ? _process->getTotalReaderCount() : sender.totalReaderCount();
}

SrtTsProcess::Ptr &SrtTsProcessHelper::getProcess() {
    return _process;
}

}//namespace mediakit
#endif//defined(ENABLE_RTPPROXY)