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
#include <vector>
#include "AudioParameters.h"
#include "RingBuffer.h"
#include "ErrorReceiving.h"
#include "Clock.h"

typedef struct AVPacket AVPacket;
typedef struct AVFrame AVFrame;

namespace ofxHap {
    class AudioThread {
    public:
        class Receiver : public ErrorReceiving {
        public:
            virtual void startAudio() = 0;
            virtual void stopAudio() = 0;
        };
        AudioThread(const AudioParameters& params, int outRate, std::shared_ptr<ofxHap::RingBuffer> buffer, Receiver& receiver);
        ~AudioThread();
        AudioThread(AudioThread const &) = delete;
        void        operator=(AudioThread const &x) = delete;
        void        send(AVPacket *packet);
        // sync() send soft == true if the playhead position is unaffected (eg pause) 
        void        sync(const Clock& clock, bool soft);
        void        endOfStream();
        void        flush();
        void        setVolume(float v);
    private:
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
        class Fader {
        public:
            Fader(int fadeDuration) : _pos(0), _duration(fadeDuration) {}
            int getFadeDuration() const { return _duration; }
            void add(int64_t delay, float start, float end);
            void apply(float *dst, int channels, int length);
            void clear();
        private:
            class Fade {
            public:
                Fade(int64_t t, float s, float e) : time(t), start(s), end(e) {}
                float valueAt(int64_t t, int duration)
                {
                    if (t < time)
                    {
                        return start;
                    }
                    else if (t > time + duration)
                    {
                        return end;
                    }
                    else
                    {
                        float m = static_cast<float>(end - start) / duration;
                        return (m * (t - time)) + start;
                    }
                }
                int64_t time;
                float start;
                float end;
            };
            int64_t _pos;
            std::vector<Fade> _fades;
            int _duration;
        };
        void                                threadMain(AudioParameters params, int ourRate, std::shared_ptr<ofxHap::RingBuffer> buffer);
        static int                          reverse(AVFrame *dst, const AVFrame *src);
        Receiver                            &_receiver;
        std::shared_ptr<ofxHap::RingBuffer> _buffer;
        std::thread                         _thread;
        std::condition_variable             _condition;
        std::mutex                          _lock;
        std::queue<Action>                  _queue;
        bool                                _finish;
        bool                                _sync;
        bool                                _soft;
        Clock                               _clock;
        float                               _volume;
    };
}

#endif /* AudioThread_h */
