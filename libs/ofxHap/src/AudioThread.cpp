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
                                 Receiver& receiver)
: _receiver(receiver), _buffer(buffer), _thread(&ofxHap::AudioThread::threadMain, this, params, outRate, buffer),
  _finish(false), _sync(false), _soft(false)
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

void ofxHap::AudioThread::threadMain(AudioParameters params, int outRate, std::shared_ptr<ofxHap::RingBuffer> buffer)
{
    if (params.duration == 0)
    {
        return;
    }
    if (params.start == AV_NOPTS_VALUE)
    {
        params.start = INT64_C(0);
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
        Fader fader(outRate / 20);
        bool finish = false;
        std::queue<Action> queue;
        AudioFrameCache cache;
        AVFrame *reversed = nullptr;
        Clock clock;
        int64_t last = AV_NOPTS_VALUE;
        TimeRange current(AV_NOPTS_VALUE, 0);

        int cacheusec = static_cast<int>(av_rescale_q(params.cache, {1, AV_TIME_BASE}, {1, sampleRate}));
        bool playing = false;

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
                ofxHap::TimeRangeSequence ranges = ofxHap::MovieTime::nextRanges(clock, expected - cacheusec, std::min(clock.period, cacheusec * INT64_C(2)));
                cache.limit(ranges);
            }


            if (!clock.getPaused())
            {
                float *dst[2];
                int count[2];
                int filled = 0;

                _buffer->writeBegin(dst[0], count[0], dst[1], count[1]);

                // Be more tolerant of being ahead than behind, because some outputs take a while to start consuming samples
                if (last == AV_NOPTS_VALUE || expected - last > _buffer->getSamplesPerChannel() || last - expected > _buffer->getSamplesPerChannel() * 2)
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
                            fader.clear();
                            fader.add(0, 0.0, 1.0);
                            fader.add(av_rescale_q(std::abs(current.length), {1, sampleRate}, {1, static_cast<int>(outRate / std::fabs(clock.getRate()))}) - fader.getFadeDuration(), 1.0, 0.0);
                        }

                        if (current.start < params.start || current.start > params.start + params.duration)
                        {
                            if (current.start < params.start)
                            {
                                if (current.length < 0)
                                {
                                    consumed = static_cast<int>(current.start) + 1;
                                }
                                else
                                {
                                    consumed = static_cast<int>(params.start - current.start);
                                }
                            }
                            else
                            {
                                if (current.length < 0)
                                {
                                    consumed = static_cast<int>(current.start - (params.start + params.duration));
                                }
                                else
                                {
                                    consumed = static_cast<int>(clock.period - current.start);
                                }
                            }
                            consumed = std::min(consumed, static_cast<int>(std::abs(current.length)));
                            written = static_cast<int>(av_rescale_q(consumed, {1, sampleRate}, {1, static_cast<int>(outRate / std::fabs(clock.getRate()))}));
                            if (written > count[i])
                            {
                                written = count[i];
                                consumed = static_cast<int>(av_rescale_q(written, {1, static_cast<int>(outRate / std::fabs(clock.getRate()))}, {1, sampleRate}));
                            }
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

                        fader.apply(dst[i], channels, written);

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
            if (playing == false && clock.getPaused() == false)
            {
                _receiver.startAudio();
                playing = true;
            }
            else if (playing == true && (clock.getPaused() || clock.getDone()))
            {
                // Wait for the buffer to drain before stopping audio
                float *buffers[2];
                int counts[2];
                _buffer->writeBegin(buffers[0], counts[0], buffers[1], counts[1]);
                if (counts[0] + counts[1] == _buffer->getSamplesPerChannel())
                {
                    _receiver.stopAudio();
                    playing = false;
                }
                _buffer->writeEnd(0);
            }

            // Only hold the lock in this { scope }
            {
                std::unique_lock<std::mutex> locker(_lock);
                if (playing == false)
                {
                    _condition.wait(locker);
                }
                else
                {
                    int64_t next = now + av_rescale_q(_buffer->getSamplesPerChannel() / 2, {1, outRate}, {1, AV_TIME_BASE});
                    int64_t wait = next - av_gettime_relative();
                    if (wait > 0)
                    {
                        _condition.wait_for(locker, std::chrono::microseconds(wait));
                    }
                }

                queue.swap(_queue);

                finish = _finish;

                if (_sync)
                {
                    bool started = clock.getPaused() && !_clock.getPaused();
                    clock = _clock;
                    clock.rescale(AV_TIME_BASE, sampleRate);
                    if (_soft)
                    {
                        // Don't lose playhead position
                        last = av_rescale_q(av_gettime_relative(), {1, AV_TIME_BASE}, {1, sampleRate});
                        if (started)
                        {
                            fader.add(0, 0.0, 1.0);
                        }
                    }
                    else
                    {
                        last = AV_NOPTS_VALUE;
                        current.start = AV_NOPTS_VALUE;
                    }
                    resampler.setRate(_clock.getRate());
                    _soft = false;
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

void ofxHap::AudioThread::sync(const Clock& clock, bool soft)
{
    std::lock_guard<std::mutex> guard(_lock);
    _clock = clock;
    if (soft == false)
    {
        _soft = false;
    }
    else if (_sync == false)
    {
        // Don't set _soft to true if there is already a pending hard sync
        _soft = true;
    }
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

void ofxHap::AudioThread::Fader::add(int64_t delay, float start, float end)
{
    _fades.emplace_back(_pos + delay, start, end);
}

void ofxHap::AudioThread::Fader::clear()
{
    _fades.clear();
}

void ofxHap::AudioThread::Fader::apply(float *dst, int channels, int length)
{
    TimeRange dstRange(_pos, length);
    for (auto itr = _fades.begin(); itr != _fades.end();) {
        if (itr->time + _duration < _pos)
        {
            itr = _fades.erase(itr);
        }
        else
        {
            TimeRange fadeRange(itr->time, _duration);
            if (fadeRange.intersects(dstRange))
            {
                int offset = static_cast<int>(itr->time - _pos);
                int start = std::max(0, offset);
                int end = std::min(length, _duration - offset);
                for (int i = start; i < end; i++) {
                    float intensity = itr->valueAt(_pos + i, _duration);
                    for (int j = 0; j < channels; j++) {
                        dst[(i * channels) + j] *= intensity;
                    }
                }
            }
            itr++;
        }
    }
    _pos += length;
}
