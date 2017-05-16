/*
 Demuxer.cpp
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

#include "Demuxer.h"
#include "Common.h"
extern "C" {
#include <libavformat/avformat.h>
}
#include <mutex>

ofxHap::Demuxer::Demuxer(const std::string& movie, PacketReceiver& receiver) :
_lastRead(AV_NOPTS_VALUE), _lastSeek(AV_NOPTS_VALUE),
_thread(&ofxHap::Demuxer::threadMain, this, movie, std::ref(receiver)),
_finish(false), _active(false)
{

}

ofxHap::Demuxer::~Demuxer()
{
    { // scope for lock
        std::unique_lock<std::mutex> locker(_lock);
        _finish = true;
        _condition.notify_one();
    }
    _thread.join();
}

void ofxHap::Demuxer::threadMain(const std::string movie, PacketReceiver& receiver)
{
    if (movie.length() > 0)
    {
        static std::once_flag registerFlag;
        std::call_once(registerFlag, [](){
            av_log_set_level(AV_LOG_QUIET);
            av_register_all();
            avformat_network_init();
        });
        AVFormatContext *fmt_ctx = NULL;
        int videoStreamIndex = -1;
        int audioStreamIndex = -1;
        int result = avformat_open_input(&fmt_ctx, movie.c_str(), NULL, NULL);
        if (result == 0)
        {
            result = avformat_find_stream_info(fmt_ctx, NULL);
        }
        if (result == 0)
        {
            receiver.foundMovie(fmt_ctx->duration);
            for (unsigned int i = 0; i < fmt_ctx->nb_streams; i++) {
#if OFX_HAP_HAS_CODECPAR
                if (fmt_ctx->streams[i]->codecpar->codec_id == AV_CODEC_ID_HAP && videoStreamIndex == -1)
#else
                if (fmt_ctx->streams[i]->codec->codec_id == AV_CODEC_ID_HAP && videoStreamIndex == -1)
#endif
                {
                    videoStreamIndex = i;
                }
                else
                {
                    fmt_ctx->streams[i]->discard = AVDISCARD_ALL;
                }
            }

            if (videoStreamIndex == -1)
            {
                result = AVERROR_INVALIDDATA;
            }

            if (result >= 0)
            {
                receiver.foundStream(fmt_ctx->streams[videoStreamIndex]);
            }
        }
        if (result >= 0)
        {
            result = av_find_best_stream(fmt_ctx, AVMEDIA_TYPE_AUDIO, -1, -1, NULL, 0);
            if (result >= 0)
            {
                audioStreamIndex = result;
                fmt_ctx->streams[audioStreamIndex]->discard = AVDISCARD_DEFAULT;
                receiver.foundStream(fmt_ctx->streams[audioStreamIndex]);
            }
            result = 0; // Not an error to have no audio
        }
        if (result < 0)
        {
            receiver.error(result);
        }
        else
        {
            receiver.foundAllStreams();

            // We have local copies of our member variables so we don't hold the lock
            // while we do work
            bool finish = false;
            std::queue<Action> actions;
            int64_t lastReadVideo = AV_NOPTS_VALUE;
            int64_t lastReadAudio = AV_NOPTS_VALUE;

            AVPacket packet;
            av_init_packet(&packet);
            
            while (!finish)
            {
                if (actions.size() > 0)
                {
                    const Action& action = actions.front();

                    switch (action.kind) {
                        case Action::Kind::SeekFrame:
                            result = avformat_seek_file(fmt_ctx, videoStreamIndex, INT64_MIN, action.pts, action.pts, AVSEEK_FLAG_FRAME);
                            lastReadAudio = lastReadVideo = AV_NOPTS_VALUE;
                            receiver.discontinuity();
                            break;
                        case Action::Kind::SeekTime:
                            result = avformat_seek_file(fmt_ctx, -1, INT64_MIN, action.pts, action.pts, 0);
                            lastReadAudio = lastReadVideo = AV_NOPTS_VALUE;
                            receiver.discontinuity();
                            break;
                        case Action::Kind::Read:
                            if ((lastReadVideo == AV_NOPTS_VALUE || lastReadVideo < action.pts) ||
                                (audioStreamIndex >= 0 && (lastReadAudio == AV_NOPTS_VALUE || lastReadAudio < action.pts)))
                            {
                                packet.data = NULL;
                                packet.size = 0;
                                result = av_read_frame(fmt_ctx, &packet);
                                if (result >= 0)
                                {
                                    receiver.readPacket(&packet);
                                    if (packet.stream_index == videoStreamIndex)
                                    {
                                        lastReadVideo = av_rescale_q(packet.pts + packet.duration - 1,
                                                                     fmt_ctx->streams[videoStreamIndex]->time_base, { 1, AV_TIME_BASE });
                                    }
                                    else if (packet.stream_index == audioStreamIndex)
                                    {
                                        lastReadAudio = av_rescale_q(packet.pts + packet.duration - 1,
                                                                     fmt_ctx->streams[audioStreamIndex]->time_base, { 1, AV_TIME_BASE });
                                    }
                                }
                                else if (result == AVERROR_EOF)
                                {
                                    receiver.endMovie();
                                }
                                av_packet_unref(&packet);
                            }
                            break;
                        default:
                            break;
                    }

                    if (action.kind != Action::Kind::Read ||
                        result < 0 ||
                        (lastReadVideo >= action.pts && (audioStreamIndex == -1 || lastReadAudio >= action.pts)))
                    {
                        actions.pop();
                    }

                    if (result < 0 && result != AVERROR_EOF)
                    {
                        receiver.error(result);
                    }
                }

                // Only hold the lock in this { scope }
                {
                    std::unique_lock<std::mutex> locker(_lock);

                    finish = _finish;

                    while (_actions.size() > 0) {
                        const auto action = _actions.front();
                        if (action.kind == Action::Kind::Cancel)
                        {
                            // empty queued actions
                            std::queue<Action>().swap(actions);
                        }
                        else
                        {
                            actions.push(action);
                        }
                        _actions.pop();
                    }

                    result = 0;

                    if (actions.size() == 0)
                    {
                        _active = false;
                        if (finish == false)
                        {
                            _condition.wait(locker);
                        }
                    }
                }
            }
        }

        if (fmt_ctx)
        {
            avformat_close_input(&fmt_ctx);
        }
    }
}

void ofxHap::Demuxer::read(int64_t pts)
{
    std::unique_lock<std::mutex> locker(_lock);
    _lastRead = pts;
    _actions.emplace(Action::Kind::Read, pts);
    _active = true;
    _condition.notify_one();
}

void ofxHap::Demuxer::seekTime(int64_t time)
{
    std::unique_lock<std::mutex> locker(_lock);
    _lastSeek = time;
    _lastRead = AV_NOPTS_VALUE;
    _actions.emplace(Action::Kind::SeekTime, time);
    _active = true;
    _condition.notify_one();
}

void ofxHap::Demuxer::seekFrame(int64_t frame)
{
    std::unique_lock<std::mutex> locker(_lock);
    _actions.emplace(Action::Kind::SeekFrame, frame);
    _active = true;
    _condition.notify_one();
}

void ofxHap::Demuxer::cancel()
{
    std::unique_lock<std::mutex> locker(_lock);
    _actions.emplace(Action::Kind::Cancel, 0);
    _condition.notify_one();
}

int64_t ofxHap::Demuxer::getLastReadTime() const
{
    return _lastRead;
}

int64_t ofxHap::Demuxer::getLastSeekTime() const
{
    return _lastSeek;
}

bool ofxHap::Demuxer::isActive() const
{
    std::unique_lock<std::mutex> locker(_lock);
    return _active;
}

ofxHap::Demuxer::Action::Action(Kind k, int64_t p)
: kind(k), pts(p)
{

}
