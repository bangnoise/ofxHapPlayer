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
#include "TimeRangeSet.h"

typedef struct AVPacket AVPacket;
typedef struct AVFrame AVFrame;

namespace ofxHap {
    template <class T, T (*Clone)(T), void (*Free)(T), TimeRange (*Query)(T)>
    class Cache {
    public:
        /*
         Cache maintains an active set and a cache
         */
        virtual ~Cache()
        {
            clear();
        }
        // Add to the active set
        virtual void store(T p)
        {
            TimeRange range = Query(p);
            if (_active.find(range.start) == _active.end())
            {
                T got = Clone(p);
                _active.insert(std::make_pair(range.start, got));
            }
        }
        // Fetch
        T fetch(int64_t pts) const
        {
            T result = fetch(_active, pts);
            if (!result)
            {
                result = fetch(_cache, pts);
            }
            return result;
        }
        // clears active set and cached
        virtual void clear ()
        {
            clear(_cache);
            clear(_active);
        }
        // Move the active set to the cache
        virtual void cache()
        {
            for (const auto& pair : _active)
            {
                if (_cache.find(pair.first) == _cache.end())
                {
                    _cache.insert(pair);
                }
                else
                {
                    Free(pair.second);
                }
            }
            _active.clear();
        }
        // discards all outwith range from cache, and all from
        // before range from active
        virtual void limit(const TimeRangeSet& ranges)
        {
            limit(_cache, ranges, false);
            limit(_active, ranges, true);
        }
    private:
        static void clear(std::map<int64_t, T>& map)
        {
            for (auto& pair : map)
            {
                Free(pair.second);
            }
            map.clear();
        }
        static void limit(std::map<int64_t, T>& map, const TimeRangeSet& ranges, bool active)
        {
            if (ranges.size() == 0)
            {
                clear(map);
            }
            else
            {
                for (auto itr = map.cbegin(); itr != map.cend();) {
                    T p = itr->second;
                    bool keep = false;
                    if (active && itr->first >= ranges.earliest())
                    {
                        keep = true;
                    }
                    else
                    {
                        TimeRange current = Query(p);
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
                        Free(p);
                        itr = map.erase(itr);
                    }
                    else
                    {
                        ++itr;
                    }
                }
            }
        }
        static T fetch(const std::map<int64_t, T>& map, int64_t pts)
        {
            for (const auto& pair : map)
            {
                TimeRange range = Query(pair.second);
                if (range.start <= pts && range.start + range.length > pts)
                {
                    return pair.second;
                }
            }
            return T();
        }
        std::map<int64_t, T> _active;
        std::map<int64_t, T> _cache;
    };

    AVPacket *PacketClone(AVPacket *p);
    void PacketFree(AVPacket *p);
    TimeRange PacketQuery(AVPacket *p);

    class PacketCache : public Cache<AVPacket *, PacketClone, PacketFree, PacketQuery> {
    };

    class LockingPacketCache : public PacketCache {
    public:
        virtual void store(AVPacket *p) override;
        virtual void cache() override;
        bool fetch(int64_t pts, AVPacket *p) const;
        bool fetch(int64_t pts, AVPacket *p, std::chrono::microseconds timeout) const;
        virtual void limit(const TimeRangeSet& range) override;
        virtual void clear() override;
    private:
        mutable std::mutex              _lock;
        mutable std::condition_variable _condition;
    };

    AVFrame *FrameClone(AVFrame *f);
    void FrameFree(AVFrame *f);
    TimeRange FrameQuery(AVFrame *f);

    class AudioFrameCache : public Cache<AVFrame *, FrameClone, FrameFree, FrameQuery> {
    };
}

#endif /* PacketCache_h */
