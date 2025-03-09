/*
 AudioResampler.h
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

#ifndef AudioResampler_h
#define AudioResampler_h

#include <cstdint>
#include "AudioParameters.h"

typedef struct SwrContext SwrContext;
typedef struct AVFrame AVFrame;
#if OFX_HAP_HAS_CHANNEL_LAYOUT
typedef struct AVChannelLayout AVChannelLayout;
#endif

namespace ofxHap {
    class AudioResampler {
    public:
        AudioResampler(const AudioParameters& params, int outrate);
        ~AudioResampler();
        float getVolume() const;
        void setVolume(float v); // harmless to call repeatedly with same value
        float getRate() const;
        void setRate(float r); // harmless to call repeatedly with same value
        // returns an AVERROR or 0 on success
        int resample(const AVFrame *src, int offset, int srcLength, float *dst, int dstLength, int& outSamplesWritten, int& outSamplesRead);
    private:
        float       _volume;
        float       _rate;
        SwrContext *_resampler;
        bool        _reconfigure;
#if OFX_HAP_HAS_CHANNEL_LAYOUT
        AVChannelLayout *_layout;
#else
        uint64_t    _layout;
#endif
        int         _sampleRateIn;
        int         _format;
        int         _sampleRateOut;
    };
}

#endif /* AudioResampler_h */
