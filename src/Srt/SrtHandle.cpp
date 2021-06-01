/**
 * The MIT License (MIT)
 *
 * Copyright (c) 2013-2021 Runner365
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy of
 * this software and associated documentation files (the "Software"), to deal in
 * the Software without restriction, including without limitation the rights to
 * use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of
 * the Software, and to permit persons to whom the Software is furnished to do so,
 * subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
 * FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
 * COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
 * IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */
#ifdef ENABLE_SRTTS
#include "SrtHandle.h"
#include "time_help.h"
#include <srt/udt.h>
#include <stdio.h>
#include <vector>
#include <sstream>
#include <iomanip>
#include <assert.h>
#include <list>
#include "Util/util.h"
#include "Util/logger.h"
#include "Common/config.h"
#include "SrtTsSelector.h"

using namespace toolkit;
using namespace mediakit;

static bool MONITOR_STATICS_ENABLE = false;
static long long MONITOR_TIMEOUT = 5000;
const unsigned int DEF_DATA_SIZE = 188*7;
const long long CHECK_ALIVE_INTERVAL = 5*1000;
const long long CHECK_ALIVE_TIMEOUT = 5*1000;

long long srt_now_ms = 0;

SrtHandle::SrtHandle(int pollid):_handle_pollid(pollid)
    ,_last_timestamp(0)
    ,_last_check_alive_ts(0) {
	_data_buf = (unsigned char*)malloc(DEF_DATA_SIZE);
}

SrtHandle::~SrtHandle() {
	if (_data_buf)
	{
		free(_data_buf);
	}

}

void SrtHandle::debug_statics(SRTSOCKET srtsocket, const std::string& streamid) {
    SRT_TRACEBSTATS mon;
    srt_bstats(srtsocket, &mon, 1);
    std::ostringstream output;
    long long now_ul = now_ms();

    if (!MONITOR_STATICS_ENABLE) {
        return;
    }
    if (_last_timestamp == 0) {
        _last_timestamp = now_ul;
        return;
    }

    if ((now_ul - _last_timestamp) < MONITOR_TIMEOUT) {
        return;
    }
    _last_timestamp = now_ul;
    output << "======= SRT STATS: sid=" << streamid << std::endl;
    output << "PACKETS     SENT: " << std::setw(11) << mon.pktSent            << "  RECEIVED:   " << std::setw(11) << mon.pktRecv              << std::endl;
    output << "LOST PKT    SENT: " << std::setw(11) << mon.pktSndLoss         << "  RECEIVED:   " << std::setw(11) << mon.pktRcvLoss           << std::endl;
    output << "REXMIT      SENT: " << std::setw(11) << mon.pktRetrans         << "  RECEIVED:   " << std::setw(11) << mon.pktRcvRetrans        << std::endl;
    output << "DROP PKT    SENT: " << std::setw(11) << mon.pktSndDrop         << "  RECEIVED:   " << std::setw(11) << mon.pktRcvDrop           << std::endl;
    output << "RATE     SENDING: " << std::setw(11) << mon.mbpsSendRate       << "  RECEIVING:  " << std::setw(11) << mon.mbpsRecvRate         << std::endl;
    output << "BELATED RECEIVED: " << std::setw(11) << mon.pktRcvBelated      << "  AVG TIME:   " << std::setw(11) << mon.pktRcvAvgBelatedTime << std::endl;
    output << "REORDER DISTANCE: " << std::setw(11) << mon.pktReorderDistance << std::endl;
    output << "WINDOW      FLOW: " << std::setw(11) << mon.pktFlowWindow      << "  CONGESTION: " << std::setw(11) << mon.pktCongestionWindow  << "  FLIGHT: " << std::setw(11) << mon.pktFlightSize << std::endl;
    output << "LINK         RTT: " << std::setw(9)  << mon.msRTT            << "ms  BANDWIDTH:  " << std::setw(7)  << mon.mbpsBandwidth    << "Mb/s " << std::endl;
    output << "BUFFERLEFT:  SND: " << std::setw(11) << mon.byteAvailSndBuf    << "  RCV:        " << std::setw(11) << mon.byteAvailRcvBuf      << std::endl;

    printf("\r\n%s \n", output.str().c_str());
    return;
}

void SrtHandle::add_new_puller(SRT_CONN_PTR conn_ptr, std::string stream_id) {
    _conn_map.insert(std::make_pair(conn_ptr->get_conn(), conn_ptr));

    auto iter = _streamid_map.find(stream_id);
    if (iter == _streamid_map.end()) {
        std::unordered_map<SRTSOCKET, SRT_CONN_PTR> srtsocket_map;
        srtsocket_map.insert(std::make_pair(conn_ptr->get_conn(), conn_ptr));

        _streamid_map.insert(std::make_pair(stream_id, srtsocket_map));
        printf("add new puller fd:%d, streamid:%s \n", conn_ptr->get_conn(), stream_id.c_str());
    } else {
        iter->second.insert(std::make_pair(conn_ptr->get_conn(), conn_ptr));
        printf("add new puller fd:%d, streamid:%s, size:%d \n", 
            conn_ptr->get_conn(), stream_id.c_str(), iter->second.size());
    }

    return;
}

void SrtHandle::close_pull_conn(SRTSOCKET srtsocket, std::string stream_id) {
    printf("close_pull_conn read_fd=%d, streamid=%s \n", srtsocket, stream_id.c_str());
    srt_epoll_remove_usock(_handle_pollid, srtsocket);

    auto streamid_iter = _streamid_map.find(stream_id);
    if (streamid_iter != _streamid_map.end()) {
        auto srtsocket_map = streamid_iter->second;
        if (srtsocket_map.size() == 0) {
            _streamid_map.erase(stream_id);
        } else if (srtsocket_map.size() == 1) {
            srtsocket_map.erase(srtsocket);
            _streamid_map.erase(stream_id);
        } else {
            srtsocket_map.erase(srtsocket);
        }
    } else {
        assert(0);
    }

    auto conn_iter = _conn_map.find(srtsocket);
    if (conn_iter != _conn_map.end()) {
        _conn_map.erase(conn_iter);
        return;
    } else {
        assert(0);
    }
    
    return;
}

SRT_CONN_PTR SrtHandle::get_srt_conn(SRTSOCKET conn_srt_socket) {
    SRT_CONN_PTR ret_conn;

    auto iter = _conn_map.find(conn_srt_socket);
    if (iter == _conn_map.end()) {
        return ret_conn;
    }

    ret_conn = iter->second;

    return ret_conn;
}

void SrtHandle::add_newconn(SRT_CONN_PTR conn_ptr, int events) {
    int val_i;
    int opt_len = sizeof(int);

    int64_t val_i64;
    int opt64_len = sizeof(int64_t);

    srt_getsockopt(conn_ptr->get_conn(), 0, SRTO_LATENCY, &val_i, &opt_len);
    printf("srto SRTO_LATENCY=%d \n", val_i);

    srt_getsockopt(conn_ptr->get_conn(), 0, SRTO_PEERLATENCY, &val_i, &opt_len);
    printf("srto SRTO_PEERLATENCY=%d \n", val_i);
    srt_getsockopt(conn_ptr->get_conn(), 0, SRTO_RCVLATENCY, &val_i, &opt_len);
    printf("srto SRTO_RCVLATENCY=%d \n", val_i);

    srt_getsockopt(conn_ptr->get_conn(), 0, SRTO_SNDBUF, &val_i, &opt_len);
    printf("srto SRTO_SNDBUF=%d \n", val_i);
    srt_getsockopt(conn_ptr->get_conn(), 0, SRTO_RCVBUF, &val_i, &opt_len);
    printf("srto SRTO_RCVBUF=%d \n", val_i);
    srt_getsockopt(conn_ptr->get_conn(), 0, SRTO_MAXBW, &val_i64, &opt64_len);
    printf("srto SRTO_MAXBW=%d \n", val_i64);
    printf("srt mix_correct is %s. \n", mINI::Instance()[Srt::kSrtMixCorrect].as<bool> ()? "enable" : "disable");
    printf("srt h264 sei filter is %s. \n", mINI::Instance()[Srt::kSrtSeiFilter].as<bool>() ? "enable" : "disable");

	struct sockaddr_in peerAddr;
	int                peerAddrLen = sizeof(peerAddr);
	struct sockaddr_in localAddr;
	int                localAddrLen = sizeof(localAddr);

	srt_getpeername(conn_ptr->get_conn(), (struct sockaddr*)&peerAddr, &peerAddrLen);
	srt_getsockname(conn_ptr->get_conn(), (struct sockaddr*)&localAddr, &localAddrLen);

	string peer_ip = SockUtil::inet_ntoa(peerAddr.sin_addr);
	int peer_port = ntohs(peerAddr.sin_port);

	string local_ip = SockUtil::inet_ntoa(localAddr.sin_addr);
	int local_port = ntohs(localAddr.sin_port);

	printf("peer[%s:%d] local[%s:%d] \n", peer_ip.c_str(), peer_port, local_ip.c_str(), local_port);

	

    if (conn_ptr->get_mode() == PULL_SRT_MODE) {
        add_new_puller(conn_ptr, conn_ptr->get_subpath());
    } else {
        if(add_new_pusher(conn_ptr) == false) {
            printf("push connection is repeated and rejected, fd:%d, streamid:%s \n",
                conn_ptr->get_conn(), conn_ptr->get_streamid().c_str());
            conn_ptr->close();
            return;
        }
    }
    printf("new conn added fd:%d, event:0x%08x \n", conn_ptr->get_conn(), events);
    int ret = srt_epoll_add_usock(_handle_pollid, conn_ptr->get_conn(), &events);
    if (ret < 0) {
        printf("srt handle run add epoll error:%d \n", ret);
        return;
    }

    return;
}

void SrtHandle::handle_push_data(SRT_SOCKSTATUS status, const std::string& subpath, SRTSOCKET conn_fd) {
    SRT_CONN_PTR srt_conn_ptr;	
    int ret;
    srt_conn_ptr = get_srt_conn(conn_fd);

    if (!srt_conn_ptr) {
        printf("handle_push_data fd:%d fail to find srt connection. \n", conn_fd);
        return;
    }

    if (status != SRTS_CONNECTED) {
        printf("handle_push_data error status:%d fd:%d \n", status, conn_fd);
        close_push_conn(conn_fd);
        return;
    }

    ret = srt_conn_ptr->read(_data_buf, DEF_DATA_SIZE);
    if (ret <= 0) {
        printf("handle_push_data srt connect read error:%d, fd:%d \n", ret, conn_fd);
        close_push_conn(conn_fd);
        return;
    }

    srt_conn_ptr->update_timestamp(srt_now_ms);

    /*srt2rtmp::get_instance()->insert_data_message(data, ret, subpath);*/

	auto processer = SrtTsSelector::Instance().getProcess(srt_conn_ptr);
	if (processer)
	{
		processer->inputTs(conn_fd, (const char *)_data_buf, ret);
	}

    //send data to subscriber(players)
    //streamid, play map<SRTSOCKET, SRT_CONN_PTR>
    auto streamid_iter = _streamid_map.find(subpath);
    if (streamid_iter == _streamid_map.end()) {//no puler
        //printf("receive data size(%d) from pusher(%d) but no puller \n", ret, conn_fd);
        return;
    }
    printf("receive data size(%d) from pusher(%d) to pullers, count:%d \n", 
        ret, conn_fd, streamid_iter->second.size());

    for (auto puller_iter = streamid_iter->second.begin();
        puller_iter != streamid_iter->second.end();
        puller_iter++) {
        auto player_conn = puller_iter->second;
        if (!player_conn) {
            printf("handle_push_data get srt connect error from fd:%d \n", puller_iter->first);
            continue;
        }
        int write_ret = player_conn->write(_data_buf, ret);
        printf("send data size(%d) to puller fd:%d \n", write_ret, puller_iter->first);
        if (write_ret > 0) {
            puller_iter->second->update_timestamp(srt_now_ms);
        }
    }

    return;
}

void SrtHandle::check_alive() {
    long long diff_t;
    std::list<SRT_CONN_PTR> conn_list;

    if (_last_check_alive_ts == 0) {
        _last_check_alive_ts = srt_now_ms;
        return;
    }
    diff_t = srt_now_ms - _last_check_alive_ts;
    if (diff_t < CHECK_ALIVE_INTERVAL) {
        return;
    }

    for (auto conn_iter = _conn_map.begin();
        conn_iter != _conn_map.end();
        conn_iter++)
    {
        long long timeout = srt_now_ms - conn_iter->second->get_last_ts();
        if (timeout > CHECK_ALIVE_TIMEOUT) {
            conn_list.push_back(conn_iter->second);
        }
    }

    for (auto del_iter = conn_list.begin();
        del_iter != conn_list.end();
        del_iter++)
    {
        SRT_CONN_PTR conn_ptr = *del_iter;
        if (conn_ptr->get_mode() == PUSH_SRT_MODE) {
            printf("check alive close pull connection fd:%d, streamid:%s \n",
                conn_ptr->get_conn(), conn_ptr->get_subpath().c_str());
            close_push_conn(conn_ptr->get_conn());
        } else if (conn_ptr->get_mode() == PULL_SRT_MODE) {
            printf("check alive close pull connection fd:%d, streamid:%s \n",
                conn_ptr->get_conn(), conn_ptr->get_subpath().c_str());
            close_pull_conn(conn_ptr->get_conn(), conn_ptr->get_subpath());
        } else {
            printf("check_alive get unkown srt mode:%d, fd:%d \n", 
                conn_ptr->get_mode(), conn_ptr->get_conn());
            assert(0);
        }
    }
}

void SrtHandle::close_push_conn(SRTSOCKET srtsocket) {
    auto iter = _conn_map.find(srtsocket);

    if (iter != _conn_map.end()) {
        SRT_CONN_PTR conn_ptr = iter->second;
        auto push_iter = _push_conn_map.find(conn_ptr->get_subpath());
        if (push_iter != _push_conn_map.end()) {
            _push_conn_map.erase(push_iter);
        }
        _conn_map.erase(iter);
        /*srt2rtmp::get_instance()->insert_ctrl_message(SRT_MSG_CLOSE_TYPE, conn_ptr->get_subpath());*/

		auto processer = SrtTsSelector::Instance().getProcess(conn_ptr);
		if (processer)
		{
			SrtTsSelector::Instance().delProcess(conn_ptr->get_subpath(), processer.get());
		}
        conn_ptr->close();
    }

    srt_epoll_remove_usock(_handle_pollid, srtsocket);
    
    return;
}

bool SrtHandle::add_new_pusher(SRT_CONN_PTR conn_ptr) {
    auto push_iter = _push_conn_map.find(conn_ptr->get_subpath());
    if (push_iter != _push_conn_map.end()) {
        return false;
    }
    _push_conn_map.insert(std::make_pair(conn_ptr->get_subpath(), conn_ptr));
    _conn_map.insert(std::make_pair(conn_ptr->get_conn(), conn_ptr));
    printf("SrtHandle add new pusher streamid:%s, subpath:%s \n",
        conn_ptr->get_streamid().c_str(), conn_ptr->get_subpath().c_str());
    return true;
}

void SrtHandle::handle_pull_data(SRT_SOCKSTATUS status, const std::string& subpath, SRTSOCKET conn_fd) {
    printf("handle_pull_data status:%d, subpath:%s, fd:%d \n",
        status, subpath.c_str(), conn_fd);
    auto conn_ptr = get_srt_conn(conn_fd);
    if (!conn_ptr) {
        printf("handle_pull_data fail to find fd(%d) \n", conn_fd);
        assert(0);
        return;
    }
    conn_ptr->update_timestamp(srt_now_ms);
}

void SrtHandle::handle_srt_socket(SRT_SOCKSTATUS status, SRTSOCKET conn_fd)
{
    //std::string subpath;
    //int mode;
    auto conn_ptr = get_srt_conn(conn_fd);

    if (!conn_ptr) {
        if (status != SRTS_CLOSED) {
            printf("handle_srt_socket find srt connection error, fd:%d, status:%d \n", 
                conn_fd, status);
        }
        return;
    }
	
#if 0
    bool ret = get_streamid_info(conn_ptr->get_streamid(), mode, subpath);
    if (!ret) {
        conn_ptr->close();
        conn_ptr = nullptr;
        return;
    }
#endif
    
    if (conn_ptr->get_mode() == PUSH_SRT_MODE) {
        switch (status)
        {
            case SRTS_CONNECTED:
            {
                handle_push_data(status, conn_ptr->get_subpath(), conn_fd);
                break;
            }
            case SRTS_BROKEN:
            {
                printf("srt push disconnected event fd:%d, streamid:%s \n",
                    conn_fd, conn_ptr->get_streamid().c_str());
                close_push_conn(conn_fd);
                break;
            }
            default:
                printf("push mode unkown status:%d, fd:%d \n", status, conn_fd);
                break;
        }
    } else if (conn_ptr->get_mode() ==  PULL_SRT_MODE) {
        switch (status)
        {
        case SRTS_CONNECTED:
        {
            handle_pull_data(status, conn_ptr->get_subpath(), conn_fd);
            break;
        }
        case SRTS_BROKEN:
        {
            printf("srt pull disconnected fd:%d, streamid:%s \n",
                conn_fd, conn_ptr->get_streamid().c_str());
            close_pull_conn(conn_fd, conn_ptr->get_subpath());
            break;
        }
        default:
            printf("pull mode unkown status:%d, fd:%d \n", status, conn_fd);
            break;
        }
    } else {
        assert(0);
    }
    return;
}
#endif