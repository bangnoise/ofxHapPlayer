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

ofxHap::AudioDecoder::AudioDecoder(const AudioParameters& params, int& result)
: _codec_ctx(nullptr)
#if !OFX_HAP_HAS_CODECPAR
#if OFX_HAP_HAS_PACKET_ALLOC
, _packet(av_packet_alloc()),
#else
, _packet(static_cast<AVPacket *>(av_malloc(sizeof(AVPacket)))),
#endif
  _frame(av_frame_alloc())
#endif
{
    result = 0;
    _codec_ctx = avcodec_alloc_context3(nullptr);
    if (_codec_ctx == nullptr)
    {
        result = AVERROR(ENOMEM);
    }
#if OFX_HAP_HAS_CODECPAR
    AVCodec *decoder = avcodec_find_decoder(params.parameters->codec_id);
#else
#if !OFX_HAP_HAS_PACKET_ALLOC
    av_init_packet(_packet);
#endif
    _packet->data = nullptr;
    _packet->size = 0;
    AVCodec *decoder = avcodec_find_decoder(static_cast<AVCodecID>(params.context->codec_id));
#endif
    if (!decoder && result >= 0)
    {
        result = AVERROR_DECODER_NOT_FOUND;
    }
    if (result >= 0)
    {
#if OFX_HAP_HAS_CODECPAR
        result = avcodec_parameters_to_context(_codec_ctx, params.parameters);
#else
        result = avcodec_copy_context(_codec_ctx, params.context);
#endif
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
#if !OFX_HAP_HAS_CODECPAR
#if OFX_HAP_HAS_PACKET_ALLOC
    av_packet_free(&_packet);
#else
    av_packet_unref(_packet);
    av_freep(&_packet);
#endif
    av_frame_free(&_frame);
#endif
}

#if !OFX_HAP_HAS_CODECPAR
int ofxHap::AudioDecoder::decode(AVPacket *packet)
{
    if (!packet)
    {
        packet = _packet;
    }

    int got;
    int result = avcodec_decode_audio4(_codec_ctx, _frame, &got, packet);

    if (result == AVERROR(EAGAIN))
    {
        result = packet->size;
    }
    if (result < 0)
    {
        return result;
    }

    if (result >= packet->size)
    {
        av_packet_unref(_packet);
    }
    else
    {
        if (packet != _packet)
        {
            av_packet_unref(_packet);
            int reffed = av_packet_ref(_packet, packet);
            if (reffed < 0)
            {
                return reffed;
            }
        }

        _packet->data += result;
        _packet->size -= result;
        _packet->pts = AV_NOPTS_VALUE;
        _packet->dts = AV_NOPTS_VALUE;
    }

    return 0;
}
#endif

int ofxHap::AudioDecoder::send(AVPacket *packet)
{
#if OFX_HAP_HAS_CODECPAR
    return avcodec_send_packet(_codec_ctx, packet);
#else
    if (_packet->size > 0 || _frame->data[0])
    {
        return AVERROR(EAGAIN);
    }
    else
    {
        return decode(packet);
    }
#endif
}

int ofxHap::AudioDecoder::receive(AVFrame *frame)
{
#if OFX_HAP_HAS_CODECPAR
    return avcodec_receive_frame(_codec_ctx, frame);
#else

    av_frame_unref(frame);

    if (!_frame->data[0])
    {
        if (_packet->size == 0)
        {
            return AVERROR(EAGAIN);
        }

        do
        {
            int result = decode(_packet);
            if (result < 0)
            {
                av_packet_unref(_packet);
                return result;
            }
        } while (!_frame->data[0] && _packet->size > 0);
    }

    if (!_frame->data[0])
    {
        // TODO: what about EOF? should we check for previous EOF return
        // then return that after
        return AVERROR(EAGAIN);
    }

    av_frame_move_ref(frame, _frame);
    return 0;
#endif
}

void ofxHap::AudioDecoder::flush()
{
    avcodec_flush_buffers(_codec_ctx);
}
