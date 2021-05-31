/*
 * Copyright (c) 2016 The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/xia-chu/ZLMediaKit).
 *
 * Use of this source code is governed by MIT license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#if defined(ENABLE_UDPTS)
#include "UdpTsServer.h"
#include "UdpTsSelector.h"
#include "Rtsp/UDPServer.h"


namespace mediakit{

UDPTsServer::UDPTsServer() {
}

UDPTsServer::~UDPTsServer() {
    if(_on_clearup){
        _on_clearup();
    }
}

void UDPTsServer::start(uint16_t local_port, const string &stream_id, const string &multi_caster_ip, const string &local_ip)
{
    //创建udp服务器
    Socket::Ptr udp_server = Socket::createSocket(nullptr, true);
	string _local_ip = local_ip.empty() ? "0.0.0.0" : local_ip;
    if (local_port == 0) {
		if (!udp_server->bindUdpSock(0, _local_ip)) {
			//分配端口失败
			throw runtime_error(StrPrinter << "绑定本地地址 " << _local_ip << "失败");
		}
    } else if (!udp_server->bindUdpSock(local_port, _local_ip)) {
        //用户指定端口
        throw std::runtime_error(StrPrinter << "创建rtp端口 " << _local_ip << ":" << local_port << " 失败:" << get_uv_errmsg(true));
    }

	if (!multi_caster_ip.empty())
	{//加入组播组
		if (SockUtil::joinMultiAddr(udp_server->rawFD(), multi_caster_ip.c_str(), udp_server->get_local_ip().c_str()) != 0)
		{
			throw runtime_error(StrPrinter << "加入组播组 " << multi_caster_ip << " 失败!");
		}
	}

    //设置udp socket读缓存
    SockUtil::setRecvBuf(udp_server->rawFD(), 4 * 1024 * 1024);

	UdpTsProcess::Ptr process;
    if (!stream_id.empty()) 
	{
		process = UdpTsSelector::Instance().getProcess(stream_id);
        udp_server->setOnRead([udp_server, process](const Buffer::Ptr &buf, struct sockaddr *addr, int addr_len) {
            process->inputUdp(udp_server, buf->data(), buf->size(), addr);
        });
    } else 
	{
		throw runtime_error("open udp socket failed");
    }

    _on_clearup = [udp_server, process, stream_id]() {
        //去除循环引用
        udp_server->setOnRead(nullptr);
		if (process) {
			//删除rtp处理器
			UdpTsSelector::Instance().delProcess(stream_id, process.get());
		}
    };

    _udp_server = udp_server;
	_udp_process = process;
}

void UDPTsServer::setOnDetach(const function<void()> &cb){
	_udp_process->setOnDetach(cb);
}

EventPoller::Ptr UDPTsServer::getPoller() {
    return _udp_server->getPoller();
}

uint16_t UDPTsServer::getPort() {
    return _udp_server ? _udp_server->get_local_port() : 0;
}

}//namespace mediakit
#endif//defined(ENABLE_UDPTS)