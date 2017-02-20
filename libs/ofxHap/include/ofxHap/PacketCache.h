/*
 PacketCache.h
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

#ifndef PacketCache_h
#define PacketCache_h

#include <cstdint>
#include <map>
#include <mutex>
#include <condition_variable>

typedef struct AVPacket AVPacket;

namespace ofxHap {
    class TimeRangeSet;
    class PacketCache {
    public:
        /*
         PacketCache maintains an active set and a cache
         */
        PacketCache();
        ~PacketCache();
        // Add packets to the active set
        void store(AVPacket *p);
        // Move the active set to the cache
        void cache();
        // Fetch
        bool fetch(int64_t pts, AVPacket *p, std::chrono::microseconds timeout) const;
        // discards all outwith range from cache, and all from
        // before range from active
        void limit(const TimeRangeSet& range);
        // clears active set and cached
        void clear();
    private:
        static bool fetch(const std::map<int64_t, AVPacket *>& map, int64_t pts, AVPacket *p);
        static void limit(std::map<int64_t, AVPacket *>& map, const TimeRangeSet& range, bool active);
        static void clear(std::map<int64_t, AVPacket *>& map);
        std::map<int64_t, AVPacket *>  _active;
        std::map<int64_t, AVPacket *>  _cache;
        mutable std::mutex              _lock;
        mutable std::condition_variable _condition;
    };
}

#endif /* PacketCache_h */
