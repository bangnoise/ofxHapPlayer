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
#include <cmath>
#include "AudioDecoder.h"
#include "AudioResampler.h"
#include "MovieTime.h"
#include "PacketCache.h"

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
    if (streamStart == AV_NOPTS_VALUE)
    {
        streamStart = INT64_C(0);
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
        bool finish = false;
        std::queue<Action> queue;
        AudioFrameCache cache;
        AVFrame *reversed = nullptr;
        Clock clock;
        int64_t last = AV_NOPTS_VALUE;
        TimeRange current(AV_NOPTS_VALUE, 0);

        int bufferusec = static_cast<int>(av_rescale_q(params.cache, {1, AV_TIME_BASE}, {1, sampleRate}));

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
                            cache.store(frame);
                            // TODO: we might be waiting to send samples onwards immediately at this point
                            // so should do that before finishing the loops
                        }
                        av_frame_free(&frame);
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
                    cache.cache();
                }

                queue.pop();
            }

            int64_t now = av_gettime_relative();
            int64_t expected = av_rescale_q(now, {1, AV_TIME_BASE}, {1, sampleRate});

            {
                // Dispose of cached samples we no longer need
                ofxHap::TimeRangeSequence ranges = ofxHap::MovieTime::nextRanges(clock, expected - bufferusec, std::min(clock.period, bufferusec * INT64_C(2)));
                cache.limit(ranges);
            }


            if (!clock.getPaused())
            {
                float *dst[2];
                int count[2];
                int filled = 0;

                _buffer->writeBegin(dst[0], count[0], dst[1], count[1]);

                if (last == AV_NOPTS_VALUE || std::abs(last - expected) > _buffer->getSamplesPerChannel())
                {
                    // Drift, hopefully due to missing packets
                    last = expected;
                    current.start = AV_NOPTS_VALUE;
                }

                for (int i = 0; i < 2; i++) {
                    while (count[i] > 0)
                    {
                        // Only queue (roughly) as many samples as we need to fill the buffer, to avoid choking the resampler
                        int countInMax = static_cast<int>(av_rescale_q(count[i], {1, static_cast<int>(outRate / std::fabs(clock.getRate()))}, {1, sampleRate}));

                        int written = 0;
                        int consumed = 0;

                        if (current.start == AV_NOPTS_VALUE || current.length == 0)
                        {
                            current = MovieTime::nextRange(clock, last, clock.period);
                        }

                        if (current.start < streamStart || current.start > streamStart + streamDuration)
                        {
                            if (current.start < streamStart)
                            {
                                if (current.length < 0)
                                {
                                    consumed = static_cast<int>(current.start) + 1;
                                }
                                else
                                {
                                    consumed = static_cast<int>(streamStart - current.start);
                                }
                            }
                            else
                            {
                                if (current.length < 0)
                                {
                                    consumed = static_cast<int>(current.start - (streamStart + streamDuration));
                                }
                                else
                                {
                                    consumed = static_cast<int>(clock.period - current.start);
                                }
                            }
                            consumed = std::min(consumed, static_cast<int>(std::abs(current.length)));
                            written = static_cast<int>(av_rescale_q(consumed, {1, sampleRate}, {1, static_cast<int>(outRate / std::fabs(clock.getRate()))}));
                            av_samples_set_silence((uint8_t **)&dst[i], 0, written, channels, AV_SAMPLE_FMT_FLT);
                        }
                        else
                        {
                            AVFrame *frame = cache.fetch(current.start);
                            if (frame)
                            {
                                int64_t pts = av_frame_get_best_effort_timestamp(frame);
                                if (current.length > 0)
                                {
                                    // TODO: we could maybe request samples from resampler and only feed it if it's empty
                                    // and then feed it the entire next chunk - then reset obv on reposition, etc
                                    // Fill forwards
                                    consumed = static_cast<int>(std::min(current.length, (frame->nb_samples - (current.start - pts))));
                                    consumed = std::min(consumed, countInMax);
                                    result = resampler.resample(frame, static_cast<int>(current.start - pts), consumed, dst[i], count[i], written, consumed);
                                }
                                else
                                {
                                    // Fill backwards
                                    consumed = static_cast<int>(std::min(std::abs(current.length), (current.start - pts) + 1));
                                    consumed = std::min(consumed, countInMax);
                                    if (reversed == nullptr)
                                    {
                                        reversed = av_frame_alloc();
                                        if (!reversed)
                                        {
                                            result = AVERROR(ENOMEM);
                                        }
                                    }

                                    int64_t rpts = av_frame_get_best_effort_timestamp(reversed);
                                    if (result >= 0 && rpts != pts)
                                    {
                                        result = reverse(reversed, frame);
                                        rpts = av_frame_get_best_effort_timestamp(reversed);
                                    }

                                    if (result >= 0)
                                    {
                                        int offset = static_cast<int>(rpts + reversed->nb_samples - 1 - current.start);
                                        result = resampler.resample(reversed, offset, consumed, dst[i], count[i], written, consumed);
                                    }
                                }
                            }
                            else
                            {
                                consumed = written = 0;
                                break;
                            }
                        }

                        if (written > 0)
                        {
                            count[i] -= written;
                            dst[i] += written * channels;
                            filled += written;
                            now = av_add_stable({1, AV_TIME_BASE}, now, {1, outRate}, written);
                        }
                        if (consumed > 0)
                        {
                            last = av_add_stable({1, sampleRate}, last, {1, static_cast<int>(sampleRate * std::fabs(clock.getRate()))}, consumed);
                            if (current.length > 0)
                            {
                                current.start += consumed;
                                current.length -= consumed;
                            }
                            else
                            {
                                current.start -= consumed;
                                current.length += consumed;
                            }
                        }

                        if (result < 0)
                        {
                            _receiver.error(result);
                            break;
                        }
                        result = 0;

                        if (written == 0)
                        {
                            // If we couldn't write anything, bail
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

            // Only hold the lock in this { scope }
            {
                std::unique_lock<std::mutex> locker(_lock);
                if (clock.getPaused() || clock.getDone())
                {
                    _condition.wait(locker);
                }
                else
                {
                    int64_t next = av_gettime_relative() - av_rescale_q(_buffer->getSamplesPerChannel() / 2, {1, outRate}, {1, AV_TIME_BASE});
                    int64_t wait = next - now;
                    if (wait > 0)
                    {
                        _condition.wait_for(locker, std::chrono::microseconds(wait));
                    }
                }

                queue.swap(_queue);

                finish = _finish;

                if (_sync)
                {
                    clock = _clock;
                    clock.rescale(AV_TIME_BASE, sampleRate);
                    resampler.setRate(_clock.getRate());
                    // TODO: could distinguish between pause/unpause and not reset last on pause
                    last = AV_NOPTS_VALUE;
                    current.start = AV_NOPTS_VALUE;
                    _sync = false;
                }

                resampler.setVolume(_volume);
            }
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
