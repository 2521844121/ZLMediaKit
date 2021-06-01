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
#if defined(ENABLE_SRTTS)

#include "SrtConn.h"
#include "time_help.h"
#include "stringex.hpp"
#include <vector>

bool is_streamid_valid(const std::string& streamid) {
    if (streamid.empty()) {
        return false;
    }

    size_t pos = streamid.find(" ");
    if (pos != streamid.npos) {
        return false;
    }

    int mode;
    std::string subpath;

    bool ret = get_streamid_info(streamid, mode, subpath);
    if (!ret) {
        return false;
    }
    
    if ((mode != PUSH_SRT_MODE) && (mode != PULL_SRT_MODE)) {
        return false;
    }

    std::vector<std::string> info_vec;
    string_split(subpath, "/", info_vec);

    if (info_vec.size() < 2) {//it must be appname/stream at least.
        return false;
    }

    for (auto item : info_vec) {
        if (item.empty()) {
            return false;
        }
        pos = item.find(" ");
        if (pos != item.npos) {
            return false;
        }
    }
    return true;
}

bool get_key_value(const std::string& info, std::string& key, std::string& value) {
    size_t pos = info.find("=");

    if (pos == info.npos) {
        return false;
    }

    key = info.substr(0, pos);
    value = info.substr(pos+1);

    if (key.empty() || value.empty()) {
        return false;
    }
    return true;
}

//eg. streamid=#!::h:live/livestream,m:publish
bool get_streamid_info(const std::string& streamid, int& mode, std::string& url_subpath) {
    std::vector<std::string> info_vec;
    std::string real_streamid;

    mode = PUSH_SRT_MODE;

    size_t pos = streamid.find("#!::");
    if (pos != 0) {
        pos = streamid.find("/");
        if (pos == streamid.npos) {
            url_subpath = std::string("test") + "/" + streamid;
            return true;
        }
        url_subpath = streamid;
        return true;
    }
    real_streamid = streamid.substr(4);

    string_split(real_streamid, ",", info_vec);
    if (info_vec.size() < 2) {
        return false;
    }

    for (size_t index = 0; index < info_vec.size(); index++) {
        std::string key;
        std::string value;

        bool ret = get_key_value(info_vec[index], key, value);
        if (!ret) {
            continue;
        }
        
        if (key == "h") {
            url_subpath = value;//eg. h=live/stream
        } else if (key == "m") {
            std::string mode_str = string_lower(value);//m=publish or m=request
            if (mode_str == "publish") {
                mode = PUSH_SRT_MODE;
            } else if (mode_str == "request") {
                mode = PULL_SRT_MODE;
            } else {
                mode = PUSH_SRT_MODE;
            }
        } else {//not suport
            continue;
        }
    }

    return true;
}

//eg. streamid=#!::h=live/livestream,m=publish
bool get_streamid_info(const std::string& streamPath, std::string& subPath, int& mode, std::string& vhost, std::string& app, std::string& streamId)
{
	std::vector<std::string> info_vec;
	std::string real_streamPath;
	vhost = "__defaultVhost__";

	mode = PUSH_SRT_MODE;

	size_t pos = streamPath.find("#!::");
	if (pos != 0) {
		std::cout << "get_streamid_info stream path must start with #!::" << std::endl;
		return false;
	}


	pos = streamPath.find("/");
	if (pos == streamPath.npos) {
		std::cout << "get_streamid_info stream path must match format  #!::h=/app/stream,m=mode" << std::endl;
		return false;
	}

	real_streamPath = streamPath.substr(4);//skip #!::

	string_split(real_streamPath, ",", info_vec);//split stream path and stream mode
	if (info_vec.size() < 2) {
		return false;
	}

	for (size_t index = 0; index < info_vec.size(); index++) {
		std::string key;
		std::string value;

		bool ret = get_key_value(info_vec[index], key, value);
		if (!ret) {
			continue;
		}

		if (key == "h") {
			subPath = value;
			std::vector<std::string> path_vec;
			string_split(value, "/", path_vec);
			if (path_vec.size() >= 3) {//当路径深度>=3时,第一个部分为vhost,最后一部分为streamId,中间部分为app
				vhost = path_vec[0]; //h=vhost/app/streamId
				streamId = path_vec[path_vec.size()-1];
				for (int index=1; index< path_vec.size()-1; index++)
				{
					app.append(path_vec[index]);
					if (index != path_vec.size() - 2)
					{
						app.append("/");
					}					
				}

			}
			else if(path_vec.size() == 2){//路径深度=2时，第一部分是app,第二部分部分是streamId,
				vhost = "__default_host__";
				app = path_vec[0];
				streamId = path_vec[1];
			}
			else if(path_vec.size() == 1)
			{//只有一分部作为streamId
				vhost = "__default_host__";
				app = "test";
				streamId = path_vec[0];
			}
			else
			{
				std::cout << "get_streamid_info invalidate stream path" << streamPath  << std::endl;
				return false;
			}

		}
		else if (key == "m") {
			std::string mode_str = string_lower(value);//m=publish or m=request
			if (mode_str == "publish") {
				mode = PUSH_SRT_MODE;
			}
			else if (mode_str == "request") {
				mode = PULL_SRT_MODE;
			}
			else {
				mode = PUSH_SRT_MODE;
			}
		}
		else {//not suport
			continue;
		}
	}

	return true;
}

SrtConn::SrtConn(SRTSOCKET conn_fd, const std::string& streamPath):_conn_fd(conn_fd),
	_streamPath(streamPath) {

    //get_streamid_info(streamPath, _mode, _url_subpath);
	get_streamid_info(streamPath, _url_subpath, _mode, _vhost, _app, _streamid);
    
    _update_timestamp = now_ms();
    
    std::vector<std::string> path_vec;
    
    printf("srt connect construct streamPath:%s, mode:%d, _vhost:%s, _app:%s _streamid:%s\n", 
		_streamPath.c_str(), _mode, _vhost.c_str(), _app.c_str(), _streamid.c_str());
}

SrtConn::~SrtConn() {
    close();
}

std::string SrtConn::get_vhost() {
    return _vhost;
}

std::string SrtConn::get_app()
{
	return _app;
}

void SrtConn::update_timestamp(long long now_ts) {
    _update_timestamp = now_ts;
}

long long SrtConn::get_last_ts() {
    return _update_timestamp;
}

void SrtConn::close() {
    if (_conn_fd == SRT_INVALID_SOCK) {
        return;
    }
    srt_close(_conn_fd);
    _conn_fd = SRT_INVALID_SOCK;
}

SRTSOCKET SrtConn::get_conn() {
    return _conn_fd;
}
int SrtConn::get_mode() {
    return _mode;
}

std::string SrtConn::get_streamid() {
    return _streamid;
}

std::string SrtConn::get_subpath() {
    return _url_subpath;
}

int SrtConn::read(unsigned char* data, int len) {
    int ret = 0;

    ret = srt_recv(_conn_fd, (char*)data, len);
    if (ret <= 0) {
        printf("srt read error:%d, socket fd:%d \n", ret, _conn_fd);
        return ret;
    }
    return ret;
}

int SrtConn::write(unsigned char* data, int len) {
    int ret = 0;

    ret = srt_send(_conn_fd, (char*)data, len);
    if (ret <= 0) {
        printf("srt write error:%d, socket fd:%d \n", ret, _conn_fd);
        return ret;
    }
    return ret;
}
#endif