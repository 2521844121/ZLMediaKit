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
#include "SrtServer.h"
#include "SrtHandle.h"
#include <srt/udt.h>
#include <thread>
#include <string.h>
#include <stdexcept>
#include "Util/logger.h"
#include "Common/config.h"

using namespace toolkit;
using namespace mediakit;
SrtServer::SrtServer(unsigned short port):_listen_port(port)
    ,_server_socket(-1)
{
}

SrtServer::~SrtServer()
{

}

int SrtServer::init_srt_parameter() {
    const int DEF_RECV_LATENCY = 120;
    const int DEF_PEER_LATENCY = 0;

    int opt_len = sizeof(int);

    if (_server_socket == -1) {
        return -1;
    }
    int maxbw = mINI::Instance()[Srt::kSrtOMaxBW].as<int>();
    srt_setsockopt(_server_socket, 0, SRTO_MAXBW, &maxbw, opt_len);

    int mss = mINI::Instance()[Srt::kSrtOMSS].as<int>(); 
    srt_setsockopt(_server_socket, 0, SRTO_MSS, &mss, opt_len);

    bool tlpkdrop = mINI::Instance()[Srt::kSrtOTlpkDrop].as<bool>();
    int tlpkdrop_i = tlpkdrop ? 1 : 0;
    srt_setsockopt(_server_socket, 0, SRTO_TLPKTDROP, &tlpkdrop_i, opt_len);

    int connection_timeout = mINI::Instance()[Srt::kSrtOConnTimeout].as<int>();
    srt_setsockopt(_server_socket, 0, SRTO_CONNTIMEO, &connection_timeout, opt_len);
    
    int send_buff = mINI::Instance()[Srt::kSrtSendbuf].as<int>();
    srt_setsockopt(_server_socket, 0, SRTO_SNDBUF, &send_buff, opt_len);
    int recv_buff = mINI::Instance()[Srt::kSrtRecvbuf].as<int>();
    srt_setsockopt(_server_socket, 0, SRTO_RCVBUF, &recv_buff, opt_len);
    int payload_size = mINI::Instance()[Srt::kSrtPayloadSize].as<int>();
    srt_setsockopt(_server_socket, 0, SRTO_PAYLOADSIZE, &payload_size, opt_len);

    int latency = mINI::Instance()[Srt::kSrtOLatency].as<int>();
    if (DEF_RECV_LATENCY != latency) {
        srt_setsockopt(_server_socket, 0, SRTO_LATENCY, &latency, opt_len);
    }
    
    int recv_latency = mINI::Instance()[Srt::kSrtORecvLatency].as<int>();
    if (DEF_RECV_LATENCY != recv_latency) {
        srt_setsockopt(_server_socket, 0, SRTO_RCVLATENCY, &recv_latency, opt_len);
    }
    
    int peer_latency = mINI::Instance()[Srt::kSrtOPeerLatency].as<int>();
    if (DEF_PEER_LATENCY != peer_latency) {
        srt_setsockopt(_server_socket, 0, SRTO_PEERLATENCY, &recv_latency, opt_len);
    }
    
    printf("init srt parameter, maxbw:%d, mss:%d, tlpkdrop:%d, connect timeout:%d, \
send buff:%d, recv buff:%d, payload size:%d, latency:%d, recv latency:%d, peer latency:%d \n",
        maxbw, mss, tlpkdrop, connection_timeout, send_buff, recv_buff, payload_size,
        latency, recv_latency, peer_latency);
    return 0;
}
int SrtServer::init_srt() {
    if (_server_socket != -1) {
        return -1;
    }

    _server_socket = srt_create_socket();
    sockaddr_in sa;
    memset(&sa, 0, sizeof sa);
    sa.sin_family = AF_INET;
    sa.sin_port = htons(_listen_port);

    sockaddr* psa = (sockaddr*)&sa;

    int ret = srt_bind(_server_socket, psa, sizeof(sa));
    if ( ret == SRT_ERROR )
    {
        srt_close(_server_socket);
        printf("srt bind error: %d \n", ret);
        return -1;
    }

    ret = srt_listen(_server_socket, 5);
    if (ret == SRT_ERROR)
    {
        srt_close(_server_socket);
        printf("srt listen error: %d \n", ret);
        return -2;
    }

    init_srt_parameter();

    _pollid = srt_epoll_create();
    if (_pollid < -1) {
        printf("srt server srt_epoll_create error, port=%d \n", _listen_port);
        return -1;
    }
    _handle_ptr = std::make_shared<SrtHandle>(_pollid);

    int events = SRT_EPOLL_IN | SRT_EPOLL_ERR;
    ret = srt_epoll_add_usock(_pollid, _server_socket, &events);
    if (ret < 0) {
        printf("srt server run add epoll error:%d \n", ret);
        return ret;
    }

    printf("srt server listen port=%d, server_fd=%d \n", _listen_port, _server_socket);
    
    return 0;
}

int SrtServer::start()
{
    int ret;

    if ((ret = init_srt()) < 0) {
        return ret;
    }

    run_flag = true;
    printf("srt server is starting... port(%d) \n", _listen_port);
    thread_run_ptr = std::make_shared<std::thread>(&SrtServer::on_work, this);
    return 0;
}

void SrtServer::stop()
{
    run_flag = false;
    if (!thread_run_ptr) {
        return;
    }
    thread_run_ptr->join();

    return;
}

void SrtServer::srt_handle_connection(SRT_SOCKSTATUS status, SRTSOCKET input_fd, const std::string& dscr) {
    SRTSOCKET conn_fd = -1;
    sockaddr_in scl;
    int sclen = sizeof(scl);
    int conn_event;// = SRT_EPOLL_IN |SRT_EPOLL_OUT| SRT_EPOLL_ERR;
    
    switch(status) {
        case SRTS_LISTENING:
        {
            conn_fd = srt_accept(input_fd, (sockaddr*)&scl, &sclen);
            if (conn_fd == -1) {
                return;
            }
            //add new srt connect into srt handle
            std::string streamid = UDT::getstreamid(conn_fd);
            if (!is_streamid_valid(streamid)) {
                printf("srt streamid(%s) error, fd:%d \n", streamid.c_str(), conn_fd);
                srt_close(conn_fd);
                return;
            }
            SRT_CONN_PTR srt_conn_ptr = std::make_shared<SrtConn>(conn_fd, streamid);

            std::string vhost_str = srt_conn_ptr->get_vhost();
            printf("new srt connection streamid:%s, fd:%d, vhost:%s \n", 
                streamid.c_str(), conn_fd, vhost_str.c_str());
            //SrsConfDirective* vhost_p = _srs_config->get_vhost(vhost_str, true);
            //if (!vhost_p) {
            //    printf("srt streamid(%s): no vhost %s, fd:%d", 
            //        streamid.c_str(), vhost_str.c_str(), conn_fd);
            //    srt_conn_ptr->close();
            //    return;
            //}
            if (srt_conn_ptr->get_mode() == PULL_SRT_MODE) {
                //add SRT_EPOLL_IN for information notify
                conn_event = SRT_EPOLL_IN | SRT_EPOLL_ERR;//not inlucde SRT_EPOLL_OUT for save cpu
            } else if (srt_conn_ptr->get_mode() == PUSH_SRT_MODE) {
                conn_event = SRT_EPOLL_IN | SRT_EPOLL_ERR;
            } else {
                printf("stream mode error, it shoulde be m=push or m=pull, streamid:%s \n",
                    srt_conn_ptr->get_streamid().c_str());
                srt_conn_ptr->close();
                return;
            }
            
            _handle_ptr->add_newconn(srt_conn_ptr, conn_event);
            break;
        }
        case SRTS_CONNECTED:
        {
            printf("srt connected: socket=%d, mode:%s \n", input_fd, dscr.c_str());
            break;
        }
        case SRTS_BROKEN:
        {
            srt_epoll_remove_usock(_pollid, input_fd);
            srt_close(input_fd);
            printf("srt close: socket=%d \n", input_fd);
            break;
        }
        default:
        {
            printf("srt server unkown status:%d \n", status);
        }
    }
}

void SrtServer::srt_handle_data(SRT_SOCKSTATUS status, SRTSOCKET input_fd, const std::string& dscr) {
    _handle_ptr->handle_srt_socket(status, input_fd);
    return;
}

void SrtServer::on_work()
{
    const unsigned int SRT_FD_MAX = 100;
    printf("srt server is working port(%d) \n", _listen_port);
    while (run_flag)
    {
        SRTSOCKET read_fds[SRT_FD_MAX];
        SRTSOCKET write_fds[SRT_FD_MAX];
        int rfd_num = SRT_FD_MAX;
        int wfd_num = SRT_FD_MAX;

        int ret = srt_epoll_wait(_pollid, read_fds, &rfd_num, write_fds, &wfd_num, -1,
                        nullptr, nullptr, nullptr, nullptr);
        if (ret < 0) {
            continue;
        }
        _handle_ptr->check_alive();

        for (int index = 0; index < rfd_num; index++) {
            SRT_SOCKSTATUS status = srt_getsockstate(read_fds[index]);
            if (_server_socket == read_fds[index]) {
                srt_handle_connection(status, read_fds[index], "read fd");
            } else {
                srt_handle_data(status, read_fds[index], "read fd");
            }
        }
        
        for (int index = 0; index < wfd_num; index++) {
            SRT_SOCKSTATUS status = srt_getsockstate(write_fds[index]);
            if (_server_socket == write_fds[index]) {
                srt_handle_connection(status, write_fds[index], "write fd");
            } else {
                srt_handle_data(status, write_fds[index], "write fd");
            }
        }
    }

    // New API at 2020-01-28, >1.4.1
    // @see https://github.com/Haivision/srt/commit/b8c70ec801a56bea151ecce9c09c4ebb720c2f68#diff-fb66028e8746fea578788532533a296bR786
#if (SRT_VERSION_MAJOR<<24 | SRT_VERSION_MINOR<<16 | SRT_VERSION_PATCH<<8) > 0x01040100
    srt_epoll_clear_usocks(_pollid);
#endif
}

SrtServerAdapter::SrtServerAdapter()
{
}

SrtServerAdapter::~SrtServerAdapter()
{
}

int  SrtServerAdapter::initialize()
{
    int  err = 0;

    // TODO: FIXME: We could fork processes here, because here only ST is initialized.

    return err;
}

int  SrtServerAdapter::run()
{
	int  err = 0;
	// TODO: FIXME: We could start a coroutine to dispatch SRT task to processes.
	unsigned short srt_port = mINI::Instance()[Srt::kSrtListenPort].as<int>();
	ErrorL << "srt server listen port " << srt_port;

	srt_ptr = std::make_shared<SrtServer>(srt_port);
	if (!srt_ptr) {
		ErrorL << "new  SrtServer failed!";
		return -1;
	}

	srt_ptr->start();
	return err;
}

void SrtServerAdapter::stop()
{
    // TODO: FIXME: If forked processes, we should do cleanup.
}
#endif