/*
 * Copyright (c) 2016 The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/xia-chu/ZLMediaKit).
 *
 * Use of this source code is governed by MIT license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#ifndef ZLMEDIAKIT_UDPTSSERVER_H
#define ZLMEDIAKIT_UDPTSSERVER_H

#if defined(ENABLE_UDPTS)
#include <memory>
#include "Network/Socket.h"
#include "UdpTsProcess.h"

using namespace std;
using namespace toolkit;

namespace mediakit{

/**
 * Udp服务器,处理UDP-TS单播和UDP-TS组播
 */
class UDPTsServer {
public:
    typedef std::shared_ptr<UDPTsServer> Ptr;
    typedef function<void(const Buffer::Ptr &buf)> onRecv;

	UDPTsServer();
    ~UDPTsServer();

    /**
     * 开启服务器，可能抛异常
     * @param local_port 本地端口，0时为随机端口
     * @param stream_id 流id，置空则使用ssrc
     * @param local_ip 绑定的本地网卡ip
	 * @param multi_caster_ip 组播IP
	 * @param multi_caster_port 组播端口
     */
    void start(uint16_t local_port, const string &stream_id, const char *local_ip = "0.0.0.0", const char *multi_caster_ip = nullptr, uint16_t multi_caster_port=0);

    /**
     * 获取绑定的本地端口
     */
    uint16_t getPort();

    /**
     * 获取绑定的线程
     */
    EventPoller::Ptr getPoller();

    /**
     * 设置RtpProcess onDetach事件回调
     */
    void setOnDetach(const function<void()> &cb);

protected:
    Socket::Ptr _udp_server;
	UdpTsProcess::Ptr _udp_process;
    function<void()> _on_clearup;
};

}//namespace mediakit
#endif//defined(ENABLE_UDPTS)
#endif //ZLMEDIAKIT_UDPTSSERVER_H
