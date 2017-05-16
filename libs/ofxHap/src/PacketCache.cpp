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

ofxHap::TimeRange ofxHap::PacketQuery(AVPacket *p)
{
    return TimeRange(p->pts, p->duration);
}

AVPacket *ofxHap::PacketClone(AVPacket *p)
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
    return packet;
}

void ofxHap::PacketFree(AVPacket *p)
{
#if OFX_HAP_HAS_PACKET_ALLOC
    av_packet_free(&p);
#else
    av_packet_unref(p);
    av_freep(&p);
#endif
}

void ofxHap::LockingPacketCache::store(AVPacket *p)
{
    std::lock_guard<std::mutex> guard(_lock);
    Cache::store(p);
    _condition.notify_one();
}

void ofxHap::LockingPacketCache::cache()
{
    std::lock_guard<std::mutex> guard(_lock);
    Cache::cache();
}

bool ofxHap::LockingPacketCache::fetch(int64_t pts, AVPacket *p) const
{
    std::lock_guard<std::mutex> guard(_lock);
    AVPacket *found = Cache::fetch(pts);
    if (found)
    {
        av_packet_ref(p, found);
    }
    return found == nullptr ? false : true;
}

bool ofxHap::LockingPacketCache::fetch(int64_t pts, AVPacket *p, std::chrono::microseconds timeout) const
{
    std::unique_lock<std::mutex> locker(_lock);
    std::chrono::time_point<std::chrono::steady_clock> now = std::chrono::steady_clock::now();
    std::chrono::time_point<std::chrono::steady_clock> end = now + timeout;
    AVPacket *found;
    do {
        found = Cache::fetch(pts);
        if (found)
        {
            av_packet_ref(p, found);
        }
        else
        {
            _condition.wait_for(locker, end - now);
            now = std::chrono::steady_clock::now();
        }
    } while (found == nullptr && now < end);
    return found == nullptr ? false : true;
}

void ofxHap::LockingPacketCache::limit(const TimeRangeSet& ranges)
{
    std::lock_guard<std::mutex> guard(_lock);
    Cache::limit(ranges);
}

void ofxHap::LockingPacketCache::clear()
{
    std::lock_guard<std::mutex> guard(_lock);
    Cache::clear();
}

AVFrame *ofxHap::FrameClone(AVFrame *f)
{
    return av_frame_clone(f);
}

void ofxHap::FrameFree(AVFrame *f)
{
    av_frame_free(&f);
}

ofxHap::TimeRange ofxHap::FrameQuery(AVFrame *f)
{
    return TimeRange(av_frame_get_best_effort_timestamp(f), f->nb_samples);
}
