/*
 AudioDecoder.cpp
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


#include "AudioDecoder.h"
extern "C" {
#include <libavformat/avformat.h>
#include <libavutil/time.h>
}

ofxHap::AudioDecoder::AudioDecoder(const AVCodecParameters *params, int& result)
: _codec_ctx(nullptr)
{
    result = 0;
    _codec_ctx = avcodec_alloc_context3(nullptr);
    if (_codec_ctx == nullptr)
    {
        result = AVERROR(ENOMEM);
    }
    AVCodec *decoder = avcodec_find_decoder(params->codec_id);
    if (!decoder && result >= 0)
    {
        result = AVERROR_DECODER_NOT_FOUND;
    }
    if (result >= 0)
    {
        result = avcodec_parameters_to_context(_codec_ctx, params);
        if (result >= 0)
        {
            AVDictionary *opts = NULL;
            av_dict_set(&opts, "refcounted_frames", "1", 0);
            av_dict_set(&opts, "threads", "auto", 0);

            result = avcodec_open2(_codec_ctx, decoder, &opts);

            av_dict_free(&opts);
        }
    }
}

ofxHap::AudioDecoder::~AudioDecoder()
{
    if (_codec_ctx)
    {
        avcodec_free_context(&_codec_ctx);
    }
}

int ofxHap::AudioDecoder::send(AVPacket *packet)
{
    return avcodec_send_packet(_codec_ctx, packet);
}

int ofxHap::AudioDecoder::receive(AVFrame *frame)
{
    int result = avcodec_receive_frame(_codec_ctx, frame);
    return result;
}

void ofxHap::AudioDecoder::flush()
{
    avcodec_flush_buffers(_codec_ctx);
}
