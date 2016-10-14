/*
 AudioThread.h
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

#ifndef AudioThread_h
#define AudioThread_h

#include <thread>
#include <condition_variable>
#include <queue>
#include <map>
#include "RingBuffer.h"
#include "ErrorReceiving.h"
#include "Clock.h"

typedef struct AVCodecParameters AVCodecParameters;
typedef struct AVPacket AVPacket;
typedef struct AVFrame AVFrame;

namespace ofxHap {
    class AudioThread {
    public:
        AudioThread(AVCodecParameters *params, int outRate, std::shared_ptr<ofxHap::RingBuffer> buffer, ErrorReceiving& receiver, int64_t start, int64_t duration);
        ~AudioThread();
        AudioThread(AudioThread const &) = delete;
        void        operator=(AudioThread const &x) = delete;
        void        send(AVPacket *packet);
        void        sync(const Clock& clock);
        void        endOfStream();
        void        flush();
        void        setVolume(float v);
    private:
        class Playhead {
        public:
            Playhead(Clock& movieClock, int inputRate, int outputRate, int bufferSamples, int64_t streamStart, int64_t streamDuration);
            void start(int64_t time, int64_t& startSample, int& length, bool& forwards) const;
            void advance(int64_t time, int64_t lastSample);
        private:
            Clock&  _clock;
            int     _samplerateIn;
            int     _samplerateOut;
            int     _bufferSamples;
            int64_t _start;
            int64_t _duration;
            int64_t _prevTime;
            int64_t _prevSample;
        };
        class Action {
        public:
            enum class Kind {
                Send,
                Flush
            };
            Action(AVPacket *p);
            Action();
            ~Action();
            Action(Action const &) = delete;
            void operator=(Action const &x) = delete;
            Kind kind;
            AVPacket *packet;
        };
        static AVCodecParameters            *copyParameters(AVCodecParameters *params);
        void                                threadMain(AVCodecParameters *params, int ourRate, std::shared_ptr<ofxHap::RingBuffer> buffer, int64_t start, int64_t duration);
        static int                          reverse(AVFrame *dst, const AVFrame *src);
        ErrorReceiving                      &_receiver;
        std::shared_ptr<ofxHap::RingBuffer> _buffer;
        std::thread                         _thread;
        std::condition_variable             _condition;
        std::mutex                          _lock;
        std::queue<Action>                  _queue;
        bool                                _finish;
        bool                                _sync;
        Clock                               _clock;
        float                               _volume;
    };
}

#endif /* AudioThread_h */
