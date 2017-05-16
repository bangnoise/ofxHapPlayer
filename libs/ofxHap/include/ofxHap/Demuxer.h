/*
 Demuxer.h
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

#ifndef __ofxHap_Demuxer__
#define __ofxHap_Demuxer__

#include <cstdint>
#include <string>
#include <thread>
#include <condition_variable>
#include <queue>
#include "ErrorReceiving.h"

typedef struct AVStream AVStream;
typedef struct AVPacket AVPacket;

namespace ofxHap {
    class PacketReceiver : public ErrorReceiving {
    public:
        virtual void foundMovie(int64_t duration) = 0; // duration in AV_TIME_BASE, called first after creation
        virtual void foundStream(AVStream *stream) = 0; // will be called for each stream before readPacket() starts
        virtual void foundAllStreams() = 0; // called once after done calling foundStream()
        virtual void readPacket(AVPacket *packet) = 0;
        virtual void discontinuity() = 0;
        virtual void endMovie() = 0;
    };
    class Demuxer {
    public:
        Demuxer(const std::string& movie, PacketReceiver& receiver);
        ~Demuxer();
        Demuxer(Demuxer const &) = delete;
        void operator=(Demuxer const &x) = delete;
        void read(int64_t pts); // read at least up to pts in AV_TIME_BASE
        int64_t getLastReadTime() const; // not thread-safe, use only from the thread calling read()
        void cancel(); // stop any active read
        void seekTime(int64_t time);
        int64_t getLastSeekTime() const; // not thread-safe, use only from the thread calling seekTime()
        void seekFrame(int64_t frame);
        /* // TODO:
        void readFrame(int64_t start, int64_t end); // read at least up to frame number in
         */
        bool isActive() const; // true if currently seeking or reading
    private:
        void threadMain(const std::string movie, PacketReceiver& receiver);
        class Action {
        public:
            enum class Kind {
                SeekTime,
                SeekFrame,
                Read,
                Cancel
            };
            Action(Kind k, int64_t p);
            Kind kind;
            int64_t pts;
        };
        int64_t                 _lastRead;
        int64_t                 _lastSeek;
        std::thread             _thread;
        std::condition_variable _condition;
        mutable std::mutex      _lock;
        bool                    _finish;
        std::queue<Action>      _actions;
        bool                    _active;
    };
}

#endif
