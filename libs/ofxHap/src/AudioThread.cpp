/*
 AudioThread.cpp
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

#include "AudioThread.h"
extern "C" {
#include <libavformat/avformat.h>
#include <libavutil/time.h>
}
#include <map>
#include <cstdlib>
#include <iostream> // TODO: NO
#include "AudioDecoder.h"
#include "AudioResampler.h"


ofxHap::AudioThread::AudioThread(const AudioParameters& params ,
                                 int outRate,
                                 std::shared_ptr<ofxHap::RingBuffer> buffer,
                                 ErrorReceiving& receiver,
                                 int64_t start,
                                 int64_t duration)
: _receiver(receiver), _buffer(buffer), _thread(&ofxHap::AudioThread::threadMain, this, params, outRate, buffer, start, duration),
  _finish(false), _sync(false)
{

}

ofxHap::AudioThread::~AudioThread()
{
    { // scope for lock
        std::unique_lock<std::mutex> locker(_lock);
        _finish = true;
        _condition.notify_one();
    }
    _thread.join();
}

void ofxHap::AudioThread::threadMain(AudioParameters params, int outRate, std::shared_ptr<ofxHap::RingBuffer> buffer, int64_t streamStart, int64_t streamDuration)
{
    if (streamDuration == 0)
    {
        return;
    }

    int result = 0;

    // Configure decoder
    AudioDecoder decoder(params, result);

    if (result < 0)
    {
        _receiver.error(result);
    }

    if (result >= 0)
    {
#if OFX_HAP_HAS_CODECPAR
        int sampleRate = params.parameters->sample_rate;
        int channels = params.parameters->channels;
#else
        int sampleRate = params.context->sample_rate;
        int channels = params.context->channels;
#endif
        AudioResampler resampler(params, outRate);
        int64_t maxDelayScaled = av_rescale_q(buffer->getSamplesPerChannel(), AV_TIME_BASE_Q, {1, sampleRate});
        bool finish = false;
        bool flush = false;
        std::queue<Action> queue;
        std::map<int64_t, AVFrame *> cache;
        AVFrame *reversed = nullptr;
        Clock clock;

        Playhead playhead(clock, sampleRate, outRate, buffer->getSamplesPerChannel(), streamStart, streamDuration);

        while (!finish) {

            while (queue.size() > 0) {

                const Action& action = queue.front();
                if (action.kind == Action::Kind::Send)
                {
                    result = decoder.send(action.packet);
                    while (result >= 0) {
                        AVFrame *frame = av_frame_alloc();
                        result = decoder.receive(frame);
                        if (result >= 0)
                        {
                            int64_t ts = av_frame_get_best_effort_timestamp(frame);
                            auto r = cache.insert(std::map<int64_t, AVFrame *>::value_type(ts, frame));
                            if (r.second == false)
                            {
                                // TODO: could we be refusing to add one with more samples than another?
                                av_frame_free(&frame);
                            }
                            // TODO: we might be waiting to send samples onwards immediately at this point
                            // so should do that before finishing the loops
                        }
                        else
                        {
                            av_frame_free(&frame);
                        }
                    }

                    if (result < 0 && result != AVERROR(EAGAIN) && result != AVERROR_EOF)
                    {
                        _receiver.error(result);
                    }
                }
                result = 0;

                if (action.kind == Action::Kind::Flush || !action.packet)
                {
                    // null is queued to signal end of stream, which requires a flush
                    decoder.flush();
                }

                queue.pop();
            }

            int64_t now = av_gettime_relative();

            clock.setTimeAt(now);

            if (!clock.getPaused())
            {
                float *dst[2];
                int count[2];
                int filled = 0;

                _buffer->writeBegin(dst[0], count[0], dst[1], count[1]);

                // TODO: HERE HERE HERE
                // 1. skipped some at movie loop, and audible click, are we playing all the samples?
                // 4. wake thread for future writes (time limit on condition.wait())

                //            std::cout << "---- writeBegin" << std::endl;
                for (int i = 0; i < 2; i++) {
                    while (count[i] > 0)
                    {
                        int64_t start;
                        int lengthIn;
                        bool forwards;

                        playhead.start(now, start, lengthIn, forwards);

                        int lengthOut = av_rescale_q(lengthIn, {1, sampleRate}, {1, static_cast<int>(outRate / clock.getRate())});
                        if (lengthOut > count[i])
                        {
                            lengthOut = count[i];
                            // Only queue (roughly) as many samples as we need to fill lengthOut, to avoid choking the resampler
                            lengthIn = av_rescale_q_rnd(lengthOut, {1, static_cast<int>(outRate / clock.getRate())}, {1, sampleRate}, AV_ROUND_UP);
                        }

                        if (start < streamStart || start > streamStart + streamDuration)
                        {
                            av_samples_set_silence((uint8_t **)&dst[i], 0, lengthOut, channels, AV_SAMPLE_FMT_FLT);
                        }
                        else
                        {
                            AVFrame *frame = nullptr;
                            int64_t pts;
                            for (const auto pair : cache) {
                                if (pair.first <= start && pair.first + pair.second->nb_samples > start)
                                {
                                    frame = pair.second;
                                    pts = pair.first;
                                    break;
                                }
                            }

                            if (frame)
                            {
                                if (forwards)
                                {
                                    // Fill forwards
                                    lengthIn = std::min(lengthIn, static_cast<int>(frame->nb_samples - (start - pts)));

                                    // AAAAND so we'll need to know about discontinuities and flush the resampler
                                    result = resampler.resample(frame, start - pts, lengthIn, dst[i], count[i], lengthOut, lengthIn);
                                }
                                else
                                {
                                    // Fill backwards
                                    lengthIn = std::min(lengthIn, static_cast<int>(start - pts) + 1);
                                    if (reversed == nullptr)
                                    {
                                        reversed = av_frame_alloc();
                                        if (!reversed)
                                        {
                                            result = AVERROR(ENOMEM);
                                        }
                                    }

                                    int rpts = av_frame_get_best_effort_timestamp(reversed);
                                    if (result >= 0 && rpts != pts)
                                    {
                                        result = reverse(reversed, frame);
                                    }

                                    // maybe resampler knows pts, detects non-contiguous blocks, resets itself

                                    // TODO: sounds like repeated samples when eg the long horror movie plays backwards
                                    // check that
                                    if (result >= 0)
                                    {
                                        int offset = rpts + reversed->nb_samples - 1 - start;
                                        result = resampler.resample(reversed, offset, lengthIn, dst[i], count[i], lengthOut, lengthIn);
                                    }
                                }
                            }
                            else
                            {
                                lengthIn = lengthOut = 0;
                                break;
                            }
                        }

                        if (lengthOut > 0)
                        {
                            count[i] -= lengthOut;
                            dst[i] += lengthOut * channels;
                            filled += lengthOut;
                        }
                        if (lengthIn > 0)
                        {
                            now = av_add_stable(AV_TIME_BASE_Q, now, {1, outRate}, lengthOut);
                            playhead.advance(now, forwards ? start + lengthIn: start - lengthIn);
                        }

                        if (result < 0)
                        {
                            _receiver.error(result);
                            break;
                        }
                        result = 0;

                        if (lengthOut == 0)
                        {
                            // TODO: ?
                            break;
                        }
                    }
                    if (count[i] > 0)
                    {
                        // don't leave gaps
                        break;
                    }
                }
                _buffer->writeEnd(filled);
            }




                // TODO: HERE HERE HERE HERE HERE HERE
                // 2. What is behaviour in reverse? Do we screw up decoding? For MP3? [YES]
                // 3. We may have to write samples forwards to end of palindrome and then backwards

            if (flush)
            {
                decoder.flush();
            }

            // Only hold the lock in this { scope }
            {
                std::unique_lock<std::mutex> locker(_lock);
                // TODO: check paused and stopped behaviour
                int64_t wake = av_rescale(_buffer->getSamplesPerChannel() / 2, outRate, AV_TIME_BASE) - (av_gettime_relative() - now);
                if (wake > 0)
                {
                    _condition.wait_for(locker, std::chrono::microseconds(wake));
                }

                queue.swap(_queue);

                finish = _finish;

                if (_sync)
                {
                    clock = _clock;
                    resampler.setRate(_clock.getRate());
                    _sync = false;
                }

                resampler.setVolume(_volume);
            }
        }

        for (auto pair : cache)
        {
            av_frame_free(&pair.second);
        }

        if (reversed)
        {
            av_frame_free(&reversed);
        }
    }
}

void ofxHap::AudioThread::send(AVPacket *p)
{
    std::lock_guard<std::mutex> guard(_lock);
    _queue.emplace(p);
    _condition.notify_one();
}

void ofxHap::AudioThread::sync(const Clock& clock)
{
    std::lock_guard<std::mutex> guard(_lock);
    _clock = clock;
    _sync = true;
    _condition.notify_one();
}

void ofxHap::AudioThread::flush()
{
    std::lock_guard<std::mutex> guard(_lock);
    _queue.emplace();
    _condition.notify_one();
}

void ofxHap::AudioThread::endOfStream()
{
    std::lock_guard<std::mutex> guard(_lock);
    // Send null to signal end
    _queue.emplace(nullptr);
    _condition.notify_one();
}

void ofxHap::AudioThread::setVolume(float v)
{
    std::lock_guard<std::mutex> guard(_lock);
    _volume = v;
    _condition.notify_one();
}

int ofxHap::AudioThread::reverse(AVFrame *dst, const AVFrame *src)
{
    int result = 0;
    // TODO: we should check the underlying buffers so we can grow again
    // if we shrank
    if (dst->nb_samples < src->nb_samples)
    {
        av_frame_unref(dst);
        if (result >= 0)
        {
            dst->channel_layout = src->channel_layout;
            dst->channels = src->channels;
            dst->format = src->format;
            dst->nb_samples = src->nb_samples;
            result = av_frame_get_buffer(dst, 1);
            if (result < 0)
            {
                dst->nb_samples = 0;
            }
        }
    }
    else
    {
        dst->nb_samples = src->nb_samples;
    }

    if (result >= 0)
    {
        result = av_frame_copy_props(dst, src);
        if (av_sample_fmt_is_planar(static_cast<AVSampleFormat>(src->format)))
        {
            // TODO: these needn't use memcpy if we deal with different formats
            int bps = av_get_bytes_per_sample(static_cast<AVSampleFormat>(src->format));
            for (int i = 0; i < dst->channels; i++) {
                for (int j = 0, k = dst->nb_samples - 1; j < dst->nb_samples; j++, k--) {
                    memcpy(&dst->extended_data[i][j * bps], &src->extended_data[i][k * bps], bps);
                }
            }
        }
        else
        {
            int bpcs = av_get_bytes_per_sample(static_cast<AVSampleFormat>(src->format)) * src->channels;
            for (int i = 0, j = dst->nb_samples - 1; i < dst->nb_samples; i++, j--) {
                memcpy(&dst->data[0][i * bpcs], &src->data[0][j * bpcs], bpcs);
            }
        }
    }
    return result;
}

ofxHap::AudioThread::Playhead::Playhead(Clock& movieClock, int rateIn, int rateOut, int bufferSamples, int64_t streamStart, int64_t streamDuration)
: _clock(movieClock), _samplerateIn(rateIn), _samplerateOut(rateOut), _bufferSamples(bufferSamples), _start(streamStart), _duration(streamDuration),
    _prevTime(AV_NOPTS_VALUE), _prevSample(AV_NOPTS_VALUE) { }

void ofxHap::AudioThread::Playhead::advance(int64_t time, int64_t sample)
{
    _prevTime = time;
    _prevSample = sample;
}

void ofxHap::AudioThread::Playhead::start(int64_t now, int64_t &startSample, int &length, bool& forwards) const
{
    // TODO: see av_compare_ts
    
    int64_t startTime;

    if (_prevTime == AV_NOPTS_VALUE || (now - _prevTime) > av_rescale_q(_bufferSamples, {1, _samplerateOut}, AV_TIME_BASE_Q))
    {
        if (_prevTime != AV_NOPTS_VALUE)
        {
            std::cout << "reset / prev:" << _prevTime << " now:" << now << " elapsed:" << (now - _prevTime) << std::endl;
        }
        startTime = now;
    }
    else
    {
        startTime = _prevTime + 1; // TODO: or don't ++ here but do it when we rescale to keep precision?
    }
    int direction = _clock.getDirectionAt(startTime);
    int64_t movieStart = _clock.getTimeAt(startTime);

    if (direction < 0)
        forwards = false;
    else
        forwards = true;

    startSample = av_rescale_q_rnd(movieStart, AV_TIME_BASE_Q, {1, _samplerateIn}, AV_ROUND_DOWN);
    if (_prevSample != AV_NOPTS_VALUE && std::abs(_prevSample - startSample) < _bufferSamples)
    {
        if (forwards)
        {
            startSample = _prevSample + 1;
            if (startSample > _start + _duration && startSample > av_rescale_q_rnd(_clock.period, AV_TIME_BASE_Q, {1, _samplerateIn}, AV_ROUND_DOWN))
            {
                startSample = 0;
            }
        }
        else
        {
            startSample = _prevSample - 1;
            if (startSample < 0)
            {
                startSample = av_rescale_q_rnd(_clock.period, AV_TIME_BASE_Q, {1, _samplerateIn}, AV_ROUND_DOWN);
            }
        }
    }
    else
    {
        //        std::cout << "skipped some" << std::endl;
    }
    if (startSample < _start || startSample >= _start + _duration)
    {
        // Outwith this track's time-range
        if (!forwards)
        {
            if (startSample < _start)
            {
                length = _start - startSample;
            }
            else
            {
                length = startSample - (_start + _duration);
            }
        }
        else
        {
            if (startSample < _start)
            {
                length = _start - startSample;
            }
            else
            {
                length = av_rescale_q(_clock.period, AV_TIME_BASE_Q, {1, _samplerateIn}) - startSample;
            }
        }
    }
    else
    {
        // Within the track's time-range
        if (!forwards)
        {
            length = startSample - _start;
        }
        else
        {
            length = (_start + _duration) - startSample;
        }
    }
}

ofxHap::AudioThread::Action::Action()
: kind(Kind::Flush), packet(nullptr)
{ }

ofxHap::AudioThread::Action::Action(AVPacket *p)
: kind(Kind::Send),
#if OFX_HAP_HAS_PACKET_ALLOC
packet(p ? av_packet_clone(p) : p)
#else
packet(static_cast<AVPacket *>(av_malloc(sizeof(AVPacket))))
#endif
{
#if !OFX_HAP_HAS_PACKET_ALLOC
    if (p)
    {
        av_init_packet(packet);
        packet->data = nullptr;
        packet->size = 0;
        av_packet_ref(packet, p);
    }
#endif
}

ofxHap::AudioThread::Action::~Action()
{
    if (packet)
    {
#if OFX_HAP_HAS_PACKET_ALLOC
        av_packet_free(&packet);
#else
        av_packet_unref(packet);
        av_freep(&packet);
#endif
    }
}
