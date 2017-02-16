/*
 AudioDecoder.h
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


#ifndef AudioDecoder_h
#define AudioDecoder_h
#include "AudioParameters.h"

typedef struct AVCodecContext AVCodecContext;
typedef struct AVPacket AVPacket;
typedef struct AVFrame AVFrame;

namespace ofxHap {
    class AudioDecoder {
    public:
        AudioDecoder(const AudioParameters& params, int& result);
        ~AudioDecoder();
        int send(AVPacket *packet);
        int receive(AVFrame *frame);
        void flush();
    private:
        AVCodecContext *_codec_ctx;
#if !OFX_HAP_HAS_CODECPAR
        int decode(AVPacket *packet);
        AVPacket *_packet;
        AVFrame *_frame;
#endif
    };
}

#endif /* AudioDecoder_h */
