/*
 PacketCache.cpp
 ofxHapPlayer

 Copyright (c) 2016, Tom Butterworth. All rights reserved.
 Redistribution and use in source and binary forms, with or without
 modification, are permitted provided that the following conditions are met:

 * Redistributions of source code must retain the above copyright
 notice, this list of conditions and the following disclaimer.

 * Redistributions in binary form must reproduce the above copyright
 notice, this list of conditions and the following disclaimer in the
 documentation and/or other materials provided with the distribution.

 THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
 ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDERS BE LIABLE FOR ANY
 DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "PacketCache.h"
extern "C" {
#include <libavformat/avformat.h>
}
#include "TimeRangeSet.h"
#include "Common.h"

ofxHap::PacketCache::PacketCache()
{

}

ofxHap::PacketCache::~PacketCache()
{
    clear();
}

void ofxHap::PacketCache::store(AVPacket *p)
{
    std::lock_guard<std::mutex> guard(_lock);
    if (_active.find(p->pts) == _active.end())
    {
#if OFX_HAP_HAS_PACKET_ALLOC
        AVPacket *packet = av_packet_clone(p);
#else
        AVPacket *packet = static_cast<AVPacket *>(av_malloc(sizeof(AVPacket)));
        av_init_packet(packet);
        packet->data = nullptr;
        packet->size = 0;
        av_packet_ref(packet, p);
#endif
        _active.insert(std::map<int64_t, AVPacket *>::value_type(packet->pts, packet));
    }
    _condition.notify_one();
}

void ofxHap::PacketCache::cache()
{
    std::lock_guard<std::mutex> guard(_lock);
    for (const auto& pair : _active)
    {
        if (_cache.find(pair.first) == _cache.end())
        {
            _cache.insert(pair);
        }
        else
        {
            AVPacket *p = pair.second;
#if OFX_HAP_HAS_PACKET_ALLOC
            av_packet_free(&p);
#else
            av_packet_unref(p);
            av_freep(&p);
#endif
        }
    }
    _active.clear();
}

bool ofxHap::PacketCache::fetch(int64_t pts, AVPacket *p, std::chrono::microseconds timeout) const
{
    std::unique_lock<std::mutex> locker(_lock);
    std::chrono::time_point<std::chrono::steady_clock> now = std::chrono::steady_clock::now();
    std::chrono::time_point<std::chrono::steady_clock> end = now + timeout;
    bool result;
    do {
        result = fetch(_active, pts, p);
        if (!result)
        {
            result = fetch(_cache, pts, p);
        }
        if (!result)
        {
            _condition.wait_for(locker, end - now);
            now = std::chrono::steady_clock::now();
        }
    } while (result == false && now < end);
    return result;
}

void ofxHap::PacketCache::limit(const TimeRangeSet& ranges)
{
    std::lock_guard<std::mutex> guard(_lock);
    limit(_cache, ranges, false);
    limit(_active, ranges, true);
}

void ofxHap::PacketCache::clear()
{
    std::lock_guard<std::mutex> guard(_lock);
    clear(_cache);
    clear(_active);
}

bool ofxHap::PacketCache::fetch(const std::map<int64_t, AVPacket *> &map, int64_t pts, AVPacket *p)
{
    for (const auto& pair : map)
    {
        if (pair.second->pts <= pts && pair.second->pts + pair.second->duration > pts)
        {
            av_packet_ref(p, pair.second);
            return true;
        }
    }
    return false;
}

void ofxHap::PacketCache::limit(std::map<int64_t, AVPacket *> &map, const ofxHap::TimeRangeSet &ranges, bool active)
{
    for (auto itr = map.cbegin(); itr != map.cend();) {
        AVPacket *p = itr->second;
        TimeRangeSet::TimeRange current(p->pts, p->duration);
        bool keep = false;
        if (active && p->pts >= ranges.earliest())
        {
            keep = true;
        }
        else
        {
            for (auto range: ranges)
            {
                if (range.intersects(current))
                {
                    keep = true;
                    break;
                }
            }
        }
        if (!keep)
        {
#if OFX_HAP_HAS_PACKET_ALLOC
            av_packet_free(&p);
#else
            av_packet_unref(p);
            av_freep(&p);
#endif
            itr = map.erase(itr);
        }
        else
        {
            ++itr;
        }
    }
}

void ofxHap::PacketCache::clear(std::map<int64_t, AVPacket *> &map)
{
    for (auto& pair : map)
    {
#if OFX_HAP_HAS_PACKET_ALLOC
        av_packet_free(&pair.second);
#else
        av_packet_unref(pair.second);
        av_freep(&pair.second);
#endif

    }
    map.clear();
}
