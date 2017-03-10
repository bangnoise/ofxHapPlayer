/*
 AudioResampler.cpp
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

#include "AudioResampler.h"
extern "C" {
#include <libavformat/avformat.h>
#include <libswresample/swresample.h>
}
#include <algorithm>
#include <cmath>
#include <vector>

ofxHap::AudioResampler::AudioResampler(const AudioParameters& params, int outRate)
: _volume(1.0), _rate(1.0), _resampler(nullptr), _reconfigure(true),
#if OFX_HAP_HAS_CODECPAR
_layout(params.parameters->channel_layout), _sampleRateIn(params.parameters->sample_rate), _sampleRateOut(outRate), _format(params.parameters->format)
#else
_layout(params.context->channel_layout), _sampleRateIn(params.context->sample_rate), _sampleRateOut(outRate), _format(params.context->sample_fmt)
#endif
{

}

ofxHap::AudioResampler::~AudioResampler()
{
    if (_resampler)
    {
        swr_free(&_resampler);
    }
}

float ofxHap::AudioResampler::getVolume() const
{
    return _volume;
}

void ofxHap::AudioResampler::setVolume(float v)
{
    if (_volume != v)
    {
        _volume = v;
        _reconfigure = true;
    }

}

float ofxHap::AudioResampler::getRate() const
{
    return _rate;
}

void ofxHap::AudioResampler::setRate(float r)
{
    if (_rate != r)
    {
        _rate = std::fabs(r);
        _reconfigure = true;
    }
}

int ofxHap::AudioResampler::resample(const AVFrame *frame, int offset, int srcLength, float *dst, int dstLength, int& outSamplesWritten, int& outSamplesRead)
{
    if (_reconfigure)
    {
        // TODO: deal with rates > INT_MAX (the limit)
        // - we'll need to drop samples
        // and 0
        if (_resampler)
        {
            swr_free(&_resampler);
        }
        _resampler = swr_alloc_set_opts(nullptr,
                                        _layout,
                                        AV_SAMPLE_FMT_FLT,
                                        static_cast<int>(_sampleRateOut / _rate),
                                        _layout,
                                        static_cast<AVSampleFormat>(_format),
                                        _sampleRateIn,
                                        0,
                                        nullptr);

        int channels = av_get_channel_layout_nb_channels(_layout);
        std::vector<double> matrix(channels * channels);
        for (int i = 0; i < channels; i++) {
            for (int j = 0; j < channels; j++) {
                if (j == i)
                    matrix[i*channels+j] = _volume;
                else
                    matrix[i*channels+j] = 0.0;
            }
        }
        int result = swr_set_matrix(_resampler, matrix.data(), channels);

        if (result >= 0)
        {
            result = swr_init(_resampler);
        }
        if (result < 0)
        {
            return result;
        }
        _reconfigure = false;
    }

    std::vector<const uint8_t *> src(frame->channels);
    if (av_sample_fmt_is_planar(static_cast<AVSampleFormat>(frame->format)))
    {
        for (int i = 0; i < frame->channels; i++) {
            src[i] = frame->extended_data[i] + (offset * av_get_bytes_per_sample(static_cast<AVSampleFormat>(frame->format)));
        }
    }
    else
    {
        src[0] = frame->data[0] + (offset * av_get_bytes_per_sample(static_cast<AVSampleFormat>(frame->format)) * frame->channels);
    }

    int result = swr_convert(_resampler,
                             (uint8_t **)&dst,
                             dstLength,
                             src.data(),
                             srcLength);
    if (result < 0)
    {
        return result;
    }
    outSamplesWritten = result;
    outSamplesRead = srcLength;
    return 0;
}
