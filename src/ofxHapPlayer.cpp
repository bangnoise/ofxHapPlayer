/*
ofxHapPlayer.cpp
ofxHapPlayer

Copyright (c) 2013, Tom Butterworth. All rights reserved.
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

/*
 ofxHapPlayer
 
 A Hap player for OpenFrameworks

 */
#include "ofxHapPlayer.h"
#include <ofxHap/Common.h>
extern "C" {
#include <libavformat/avformat.h>
#include <libavutil/time.h>
#include <libswresample/swresample.h>
#include <hap.h>
}

// This amount will be bufferred before and after the playhead
#define kofxHapPlayerBufferUSec INT64_C(250000)
#define kofxHapPlayerUSecPerSec 1000000L

const string ofxHapPlayerVertexShader = "void main(void)\
                                        {\
                                        gl_Position = ftransform();\
                                        gl_TexCoord[0] = gl_MultiTexCoord0;\
                                        }";

const string ofxHapPlayerFragmentShader = "uniform sampler2D cocgsy_src;\
                                          const vec4 offsets = vec4(-0.50196078431373, -0.50196078431373, 0.0, 0.0);\
                                          void main()\
                                          {\
                                          vec4 CoCgSY = texture2D(cocgsy_src, gl_TexCoord[0].xy);\
                                          CoCgSY += offsets;\
                                          float scale = ( CoCgSY.z * ( 255.0 / 8.0 ) ) + 1.0;\
                                          float Co = CoCgSY.x / scale;\
                                          float Cg = CoCgSY.y / scale;\
                                          float Y = CoCgSY.w;\
                                          vec4 rgba = vec4(Y + Co - Cg, Y + Cg, Y - Co - Cg, 1.0);\
                                          gl_FragColor = rgba;\
                                          }";

// TODO:
// 1. Thread safety here
// 1.a What about AudioThread when paused? stopped?
// push to github here and then:
// 2. Fix sound_sync_test_hap.mov frequent audio position reset
// 3. Windows
// 4. Linux
// 5. Pause in palindrome(low priority)

/*
Utility to round up to a multiple of 4 for DXT dimensions
*/
static int roundUpToMultipleOf4( int n )
{
    if( 0 != ( n & 3 ) )
        n = ( n + 3 ) & ~3;
    return n;
}

ofxHapPlayer::ofxHapPlayer() :
    _loaded(false), _hadFirstVideoFrame(false), _playing(false),
    _wantsUpload(false),
    _demuxer(), _buffer(nullptr), _audioThread(nullptr), _audioOut(), _volume(1.0), _timeout(30000)
{
}

ofxHapPlayer::~ofxHapPlayer()
{
    /*
    Close any loaded movie
    */
    close();
}

bool ofxHapPlayer::load(string name)
{
    //    std::lock_guard<std::mutex> guard(_lock);
	_moviePath = name;
	
    /*
    Close any open movie
    */
    close();


    /*
    Load the file or URL
    */
    if (name.compare(0, 7, "http://") != 0 &&
        name.compare(0, 8, "https://") != 0 &&
        name.compare(0, 7, "rtsp://") != 0)
    {
        name = ofToDataPath(name);
    }

    _demuxer = std::make_shared<ofxHap::Demuxer>(name, *this);

    /*
    Apply our current state to the movie
    */
    if (_playing) play();

    return true;
}

void ofxHapPlayer::foundMovie(int64_t duration)
{
    std::lock_guard<std::mutex> guard(_lock);
    _clock.period = duration;
}

void ofxHapPlayer::foundStream(AVStream *stream)
{
    std::lock_guard<std::mutex> guard(_lock);
#if OFX_HAP_HAS_CODECPAR
    AVCodecParameters *params = stream->codecpar;
    if (params->codec_type == AVMEDIA_TYPE_VIDEO && params->codec_id == AV_CODEC_ID_HAP)
    {
        _videoStream = stream;
    }
    else if (params->codec_type == AVMEDIA_TYPE_AUDIO)
    {
        // We will output silence until we have samples to play
        _audioStream = stream;
        _buffer = std::make_shared<ofxHap::RingBuffer>(params->channels, params->sample_rate / 8);

        _audioOut.start(params->channels, params->sample_rate, _buffer);
        if (_clock.getPaused())
        {
            _audioOut.stop();
        }

        _audioThread = std::make_shared<ofxHap::AudioThread>(ofxHap::AudioParameters(params), _audioOut.getBestRate(params->sample_rate), _buffer, *this, stream->start_time, stream->duration);
        _audioThread->setVolume(_volume);
        _audioThread->sync(_clock);
    }
#else
    AVCodecContext *codec = stream->codec;
    if (codec->codec_type == AVMEDIA_TYPE_VIDEO && codec->codec_id == AV_CODEC_ID_HAP)
    {
        _videoStream = stream;
    }
    else if (codec->codec_type == AVMEDIA_TYPE_AUDIO)
    {
        ofxHap::AudioParameters p(codec);

        // TODO: we almost don't need to store this, only to identify packet index
        _audioStream = stream;
        _buffer = std::make_shared<ofxHap::RingBuffer>(codec->channels, codec->sample_rate / 8);

        _audioOut.start(codec->channels, codec->sample_rate, _buffer);
        if (_clock.getPaused())
        {
            _audioOut.stop();
        }

        _audioThread = std::make_shared<ofxHap::AudioThread>(p, _audioOut.getBestRate(codec->sample_rate), _buffer, *this, stream->start_time, stream->duration);
        _audioThread->setVolume(_volume);
        _audioThread->sync(_clock);
    }
#endif
}

void ofxHapPlayer::foundAllStreams()
{
    std::lock_guard<std::mutex> guard(_lock);
    _loaded = true;
}

void ofxHapPlayer::readPacket(AVPacket *packet)
{
    // No need to lock
    if (_videoStream && packet->stream_index == _videoStream->index)
    {
        _videoPackets.store(packet);
    }
    else if (_audioStream && packet->stream_index == _audioStream->index)
    {
        _audioThread->send(packet);
    }

}

void ofxHapPlayer::discontinuity()
{
    // No need to lock
    _videoPackets.cache();
    if (_audioThread)
    {
        _audioThread->flush();
    }
}

void ofxHapPlayer::endMovie()
{
    // No need to lock
    if (_audioThread)
    {
        // signal end of stream
        _audioThread->endOfStream();
    }
}

void ofxHapPlayer::error(int averror)
{
    std::lock_guard<std::mutex> guard(_lock);
    char err[AV_ERROR_MAX_STRING_SIZE];
    av_strerror(averror, err, AV_ERROR_MAX_STRING_SIZE);
    _error = err;
    ofLogError("ofxHapPlayer", _error);
}

void ofxHapPlayer::close()
{
    std::lock_guard<std::mutex> guard(_lock);
    _demuxer.reset();
    _audioThread.reset();
    _audioOut.close();
    _buffer.reset();
    _videoPackets.clear();
    _clock.period = 0;
    _wantsUpload = false;
    _hadFirstVideoFrame = false;
    _videoStream = _audioStream = nullptr;
    _shader.unload();
    _texture.clear();
    _decodedFrame.clear();
    _loaded = false;
    _error.clear();
}

static void DoHapDecode(HapDecodeWorkFunction function, void *p, unsigned int count, void *info)
{
#ifdef __APPLE__
    dispatch_apply(count, dispatch_get_global_queue(QOS_CLASS_USER_INITIATED, 0), ^(size_t i) {
        function(p, i);
    });
#else
    unsigned int i;
    for (i = 0; i < count; i++) {
        // TODO: MT
        function(p, i);
    }
#endif
}

static bool FrameMatchesStream(unsigned int frame, uint32_t stream)
{
    switch (stream) {
        case MKTAG('H', 'a', 'p', '1'):
            if (frame == HapTextureFormat_RGB_DXT1)
                return true;
            break;
        case MKTAG('H', 'a', 'p', '5'):
            if (frame == HapTextureFormat_RGBA_DXT5)
                return true;
            break;
        case MKTAG('H', 'a', 'p', 'Y'):
            if (frame == HapTextureFormat_YCoCg_DXT5)
                return true;
        default:
            break;
    }
    return false;
}

void ofxHapPlayer::limit(ofxHap::TimeRangeSet &set) const
{
    if (set.earliest() < 0)
    {
        int64_t overflow = std::min(-set.earliest(), _clock.period);

        if (_clock.mode == ofxHap::Clock::Mode::Palindrome)
        {
            set.add(0, overflow);
        }
        else if (_clock.mode == ofxHap::Clock::Mode::Loop)
        {
            set.add(_clock.period - overflow, overflow);
        }

        set.remove(set.earliest(), -set.earliest());
    }
    if (set.latest() > _clock.period)
    {
        int64_t overflow = std::min(set.latest() - _clock.period, _clock.period);

        if (_clock.mode == ofxHap::Clock::Mode::Palindrome)
        {
            set.add(_clock.period - overflow, overflow);
        }
        else if (_clock.mode == ofxHap::Clock::Mode::Loop)
        {
            set.add(0, overflow);
        }

        set.remove(_clock.period, set.latest() - _clock.period + 1);
    }
}

static void describeTRS(const ofxHap::TimeRangeSet& set)
{
    std::cout << "TimeRangeSet " << &set << " ";
    bool first = true;
    for (const auto& range : set)
    {
        if (!first)
        {
            std::cout << ", ";
        }
        std::cout << "[" << range.start << " : " << range.latest() << "]";
        first = false;
    }
    std::cout << std::endl;
}

void ofxHapPlayer::read(const ofxHap::TimeRangeSet& wanted, bool seek)
{
    for (auto range : wanted)
    {
        int64_t lastRead = _demuxer->getLastReadTime();
        if (lastRead != AV_NOPTS_VALUE && range.start > lastRead && range.start - lastRead < kofxHapPlayerUSecPerSec / 4)
        {

//            std::cout << "read " <<
//            av_rescale_q(range.latest(), AV_TIME_BASE_Q, {_videoTimeBase.first, _videoTimeBase.second})
//            << " (" << range.latest() << ")" << std::endl;

            _demuxer->read(range.latest());
            _active.add(lastRead + 1, range.latest() - lastRead);
        }
        else if (seek)
        {
//            std::cout << "seek " <<
//            av_rescale_q(range.start, AV_TIME_BASE_Q, {_videoTimeBase.first, _videoTimeBase.second})
//            << " (" << range.start << ") " << " read " <<
//            av_rescale_q(range.latest(), AV_TIME_BASE_Q, {_videoTimeBase.first, _videoTimeBase.second})
//            << " (" << range.latest() << ")" << std::endl;
            _demuxer->seekTime(range.start);
            _demuxer->read(range.latest());
            _active.add(range);
        }
    }
}

void ofxHapPlayer::update()
{
    std::lock_guard<std::mutex> guard(_lock);
    // Calculate our current position for video and audio (if present)
    if (!_playing || !_videoStream)
    {
        // TODO: or something - what about stream end in play-once?
        return;
    }
    updatePTS();

    int64_t pts = _clock.getTime();

    ofxHap::TimeRangeSet wanted, cache;

    if (_clock.getDirection() > 0)
    {
        wanted.add(pts, kofxHapPlayerBufferUSec);
    }
    else
    {
        wanted.add(pts - kofxHapPlayerBufferUSec, kofxHapPlayerBufferUSec);
    }

    // Keep cache either side of pts (handles wrap-around)
    cache.add(pts - kofxHapPlayerBufferUSec, kofxHapPlayerBufferUSec);
    cache.add(pts, kofxHapPlayerBufferUSec);

    limit(wanted);
    limit(cache);

    ofxHap::TimeRangeSet vcache;
    for (auto& range : cache)
    {
        ofxHap::TimeRangeSet::TimeRange vrange(av_rescale_q(range.start, AV_TIME_BASE_Q, _videoStream->time_base),
                                               av_rescale_q(range.length, AV_TIME_BASE_Q, _videoStream->time_base));
        vcache.add(vrange);
    }

//    std::cout << "----start----" <<
//    av_rescale_q(pts, AV_TIME_BASE_Q, {_videoTimeBase.first, _videoTimeBase.second}) << "----" << pts << "----" << std::endl;

    _videoPackets.limit(vcache);

    _active = _active.intersection(cache);

    wanted.remove(_active);

    //    std::cout << "direction: " << _clock.getDirection() << std::endl;
//    std::cout << "wanted (size: " << wanted.size() << ") ";
//    describeTRS(wanted);

//    std::cout << "_videoPackets ";
//    describeTRS(_videoPackets.times());

//    std::cout << "_active ";
//    describeTRS(_active);

    // Read what we can without seeking first...
    read(wanted, false);
    // ...update our wanted set...
    wanted.remove(_active);
    // ...then seek and read the remainder
    read(wanted, true);

    int64_t vidPosition = av_rescale_q(pts, AV_TIME_BASE_Q, _videoStream->time_base);
    if (pts == _clock.period)
    {
        vidPosition--;
    }
    // Retreive the video frame if necessary
    if (vidPosition > _videoStream->duration || (_videoStream->start_time != AV_NOPTS_VALUE && vidPosition < _videoStream->start_time))
    {
        _decodedFrame.invalidate();
    }
    else
    {
        bool inBuffer = (_decodedFrame.isValid() && _decodedFrame.pts <= vidPosition && _decodedFrame.pts + _decodedFrame.duration > vidPosition) ? true : false;
        if (!inBuffer)
        {
            AVPacket packet;
            av_init_packet(&packet);
            packet.data = NULL;
            packet.size = 0;
            if (_videoPackets.fetch(vidPosition, &packet, std::chrono::microseconds(_timeout)))
            {
                //            std::cout << packet.pts << std::endl;

                unsigned int textureCount;
                unsigned int hapResult = HapGetFrameTextureCount(packet.data, packet.size, &textureCount);
                if (hapResult == HapResult_No_Error && textureCount == 1) // TODO: Hap Q+A
                {
                    unsigned int textureFormat;
                    hapResult = HapGetFrameTextureFormat(packet.data, packet.size, 0, &textureFormat);
#if OFX_HAP_HAS_CODECPAR
                    if (hapResult == HapResult_No_Error && !FrameMatchesStream(textureFormat, _videoStream->codecpar->codec_tag))
#else
                    if (hapResult == HapResult_No_Error && !FrameMatchesStream(textureFormat, _videoStream->codec->codec_tag))
#endif
                    {
                        hapResult = HapResult_Bad_Frame;
                    }
                    if (hapResult == HapResult_No_Error)
                    {
#if OFX_HAP_HAS_CODECPAR
                        size_t length = roundUpToMultipleOf4(_videoStream->codecpar->width) * roundUpToMultipleOf4(_videoStream->codecpar->height);
#else
                        size_t length = roundUpToMultipleOf4(_videoStream->codec->width) * roundUpToMultipleOf4(_videoStream->codec->height);
#endif
                        if (textureFormat == HapTextureFormat_RGB_DXT1)
                        {
                            length /= 2;
                        }
                        if (_decodedFrame.buffer.size() != length)
                        {
                            _decodedFrame.buffer.resize(length);
                        }
                        unsigned long bytesUsed;
                        hapResult = HapDecode(packet.data,
                                              packet.size,
                                              0,
                                              DoHapDecode,
                                              NULL,
                                              _decodedFrame.buffer.data(),
                                              _decodedFrame.buffer.size(),
                                              &bytesUsed,
                                              &textureFormat);
                    }
                }
                if (hapResult == HapResult_No_Error)
                {
                    if (_hadFirstVideoFrame == false)
                    {
                        _hadFirstVideoFrame = true; // TODO: mightn't need this
                    }
                    _decodedFrame.pts = packet.pts;
                    _decodedFrame.duration = packet.duration;
                    _wantsUpload = true;
                }
                else
                {
                    _decodedFrame.invalidate();
                }
                av_packet_unref(&packet);
            }
            else
            {
                std::cout << "MISSED PACKET " << vidPosition << " (" << pts << ")" << std::endl;
            }
        }
    }
}

bool ofxHapPlayer::getHapAvailable() const
{
    std::lock_guard<std::mutex> guard(_lock);
    if (_videoStream)
    {
#if OFX_HAP_HAS_CODECPAR
        switch (_videoStream->codecpar->codec_tag) {
#else
        switch (_videoStream->codec->codec_tag) {
#endif
            case MKTAG('H', 'a', 'p', '1'):
            case MKTAG('H', 'a', 'p', '5'):
            case MKTAG('H', 'a', 'p', 'Y'):
                return true;
            case MKTAG('H', 'a', 'p', 'M'): // TODO:
            default:
                return false;
        }
    }
    return false;
}

ofTexture* ofxHapPlayer::getTexture()
{
    std::lock_guard<std::mutex> guard(_lock);
    if (_wantsUpload && _videoStream)
    {
        GLenum internalFormat;
#if OFX_HAP_HAS_CODECPAR
        switch (_videoStream->codecpar->codec_tag) {
#else
        switch (_videoStream->codec->codec_tag) {
#endif
            case MKTAG('H', 'a', 'p', '1'):
                internalFormat = GL_COMPRESSED_RGB_S3TC_DXT1_EXT;
                break;
            case MKTAG('H', 'a', 'p', '5'):
            case MKTAG('H', 'a', 'p', 'Y'):
                internalFormat = GL_COMPRESSED_RGBA_S3TC_DXT5_EXT;
                break;
            case MKTAG('H', 'a', 'p', 'M'):
                // TODO: HapM
                // TODO: break;
            default:
                // TODO: fail
                internalFormat = GL_RGBA;
                break;
        }
        if (_texture.isAllocated() == false)
        {
            /*
             Create our texture for DXT upload
             */
            ofTextureData texData;
#if OFX_HAP_HAS_CODECPAR
            texData.width = _videoStream->codecpar->width;
            texData.height = _videoStream->codecpar->height;
#else
            texData.width = _videoStream->codec->width;
            texData.height = _videoStream->codec->height;
#endif
            texData.textureTarget = GL_TEXTURE_2D;
            texData.glInternalFormat = internalFormat;
            _texture.allocate(texData, GL_BGRA, GL_UNSIGNED_INT_8_8_8_8_REV);


#if defined(TARGET_OSX)
            _texture.bind();
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_STORAGE_HINT_APPLE , GL_STORAGE_SHARED_APPLE);
            _texture.unbind();
#endif
        }

        glPushClientAttrib(GL_CLIENT_PIXEL_STORE_BIT);
        glEnable(GL_TEXTURE_2D);

        ofTextureData &texData = _texture.getTextureData();
        {
            glBindTexture(GL_TEXTURE_2D, texData.textureID);
        }

#if defined(TARGET_OSX)
        glTextureRangeAPPLE(GL_TEXTURE_2D, _decodedFrame.buffer.size(), _decodedFrame.buffer.data());
        glPixelStorei(GL_UNPACK_CLIENT_STORAGE_APPLE, GL_TRUE);
#endif

        glCompressedTexSubImage2D(GL_TEXTURE_2D,
            0,
            0,
            0,
#if OFX_HAP_HAS_CODECPAR
            _videoStream->codecpar->width,
            _videoStream->codecpar->height,
#else
            _videoStream->codec->width,
            _videoStream->codec->height,
#endif
            internalFormat,
            _decodedFrame.buffer.size(),
            _decodedFrame.buffer.data());

        glPopClientAttrib();
        glDisable(GL_TEXTURE_2D);
        _wantsUpload = false;
    }
    return &_texture;
}

ofShader *ofxHapPlayer::getShader()
{
    std::lock_guard<std::mutex> guard(_lock);
#if OFX_HAP_HAS_CODECPAR
    if (_videoStream && _videoStream->codecpar->codec_tag == MKTAG('H', 'a', 'p', 'Y'))
#else
    if (_videoStream && _videoStream->codec->codec_tag == MKTAG('H', 'a', 'p', 'Y'))
#endif
    {
        if (_shader.isLoaded() == false)
        {
            bool success = _shader.setupShaderFromSource(GL_VERTEX_SHADER, ofxHapPlayerVertexShader);
            if (success)
            {
                success = _shader.setupShaderFromSource(GL_FRAGMENT_SHADER, ofxHapPlayerFragmentShader);
            }
            if (success)
            {
                _shader.linkProgram();
            }
        }
        if (_shader.isLoaded()) return &_shader;
    }
    return nullptr;
}

string ofxHapPlayer::getMoviePath() const {
	return _moviePath;
}

void ofxHapPlayer::draw(float x, float y) {
    draw(x,y, getWidth(), getHeight());
}

void ofxHapPlayer::draw(float x, float y, float w, float h) {
    ofTexture *t = getTexture();
    if (t->isAllocated())
    {
        ofShader *sh = getShader();
        if (sh)
        {
            sh->begin();
        }
        t->draw(x,y,w,h);
        if (sh)
        {
            sh->end();
        }
    }
}

void ofxHapPlayer::play()
{
    std::lock_guard<std::mutex> guard(_lock);
    _playing = true;
    // TODO: yes? or time with delivery of first audio or video frame
    _clock.syncAt(0, av_gettime_relative());
    if (_audioThread)
    {
        _audioThread->sync(_clock);
    }
    //    if (_codec_ctx)
    {
        // TODO: load() starts the stream
        // so we need to postpone that call to here the first time, then call start() the following times
        //        _soundStream.start();
    }
}

void ofxHapPlayer::stop()
{
    std::lock_guard<std::mutex> guard(_lock);
    _playing = false;
    _audioOut.close();
    _decodedFrame.clear();
}

void ofxHapPlayer::setPaused(bool pause)
{
    std::lock_guard<std::mutex> guard(_lock);
    if (_clock.getPaused() != pause)
    {
        _clock.setPausedAt(pause, _frameTime);
        if (_audioThread)
        {
            _audioThread->sync(_clock);
            if (pause)
            {
                _audioOut.stop();
            }
            else
            {
                _audioOut.start();
            }
        }
    }
}

bool ofxHapPlayer::isFrameNew() const
{
    return _wantsUpload;
}

float ofxHapPlayer::getWidth() const
{
    std::lock_guard<std::mutex> guard(_lock);
    if (_videoStream)
    {
#if OFX_HAP_HAS_CODECPAR
        return _videoStream->codecpar->width;
#else
        return _videoStream->codec->width;
#endif
    }
    else
    {
        return 0;
    }
}

float ofxHapPlayer::getHeight() const
{
    std::lock_guard<std::mutex> guard(_lock);
    if (_videoStream)
    {
#if OFX_HAP_HAS_CODECPAR
        return _videoStream->codecpar->height;
#else
        return _videoStream->codec->height;
#endif
    }
    else
    {
        return 0;
    }
}

bool ofxHapPlayer::isPaused() const
{
    std::lock_guard<std::mutex> guard(_lock);
    return _clock.getPaused();
}

bool ofxHapPlayer::isLoaded() const
{
    std::lock_guard<std::mutex> guard(_lock);
    return _loaded;
}

bool ofxHapPlayer::isPlaying() const
{
    return _playing;
}

std::string ofxHapPlayer::getError() const
{
    std::lock_guard<std::mutex> guard(_lock);
    return _error;
}

ofPixels& ofxHapPlayer::getPixels()
{
    static ofPixels none;
    return none;
}

const ofPixels& ofxHapPlayer::getPixels() const
{
    static ofPixels none;
    return none;
}

ofPixelFormat ofxHapPlayer::getPixelFormat() const
{
    return OF_PIXELS_BGRA; // TODO: could indicate alpha-ness
}

void ofxHapPlayer::setLoopState(ofLoopType state)
{
    std::lock_guard<std::mutex> guard(_lock);
    ofxHap::Clock::Mode mode;
    switch (state) {
        case OF_LOOP_PALINDROME:
            mode = ofxHap::Clock::Mode::Palindrome;
            break;
        case OF_LOOP_NONE:
            mode = ofxHap::Clock::Mode::Once;
            break;
        default:
            mode = ofxHap::Clock::Mode::Loop;
            break;
    }
    if (mode != _clock.mode)
    {
        _clock.mode = mode;
        if (_audioThread)
        {
            _audioThread->sync(_clock);
        }
    }
}

ofLoopType ofxHapPlayer::getLoopState() const
{
    std::lock_guard<std::mutex> guard(_lock);
    switch (_clock.mode) {
        case ofxHap::Clock::Mode::Once:
            return OF_LOOP_NONE;
        case ofxHap::Clock::Mode::Loop:
            return OF_LOOP_NORMAL;
        default:
            return OF_LOOP_PALINDROME;
    }
}

float ofxHapPlayer::getSpeed() const
{
    std::lock_guard<std::mutex> guard(_lock);
    return _clock.getRate();
}

void ofxHapPlayer::setSpeed(float speed)
{
    std::lock_guard<std::mutex> guard(_lock);
    _clock.setRate(speed);
    if (_audioThread)
    {
        _audioThread->sync(_clock);
    }
}

float ofxHapPlayer::getDuration() const
{
    std::lock_guard<std::mutex> guard(_lock);
    return _clock.period / static_cast<float>(AV_TIME_BASE);
}

bool ofxHapPlayer::getIsMovieDone() const
{
    std::lock_guard<std::mutex> guard(_lock);
    if (_clock.mode == ofxHap::Clock::Mode::Once &&
        _clock.getTime() == _clock.period)
    {
        return true;
    }
    return false;
}

float ofxHapPlayer::getPosition() const
{
    std::lock_guard<std::mutex> guard(_lock);
    if (_clock.period)
    {
        return _clock.getTime() / static_cast<float>(_clock.period);
    }
    else
    {
        return 0.0;
    }
}

void ofxHapPlayer::setPosition(float pct)
{
    // TODO: Clock doesn't work if on the reverse phase of a palindrome (skips to forward phase)
    std::lock_guard<std::mutex> guard(_lock);
    int64_t time = std::min(std::max(static_cast<int64_t>(pct * _clock.period), INT64_C(0)), _clock.period);
    _clock.syncAt(time, av_gettime_relative());
    if (_audioThread)
    {
        _audioThread->sync(_clock);
    }
    if (_demuxer != nullptr)
    {
        if (_playing)
        {
            //_playStartTime = av_gettime_relative();
        }

        // TODO: think about behaviour for setPosition(), play() sequence (should we play from 0 or setPosition() position?)
        // TODO: getPTS() will return the wrong time until next call to updatePTS() but we don't want to change the "now" time
        // so we may have to only store the "now' time in updatePTS()  and getPTS() uses that for a calculation
    }
}

float ofxHapPlayer::getVolume() const
{
    return _volume;
}

void ofxHapPlayer::setVolume(float volume)
{
    if (volume != _volume)
    {
        std::lock_guard<std::mutex> guard(_lock);
        _volume = ofClamp(volume, 0.0, 1.0);
        if (_audioThread)
            _audioThread->setVolume(_volume);
    }
}

/*
 // TODO: need clock to understand frame numbers so we have correct position on setFrame()
void ofxHapPlayer::setFrame(int frame)
{
    if (_demuxer != nullptr && _totalNumFrames > 0)
    {
        _demuxer->seekFrame(std::max(0, std::min(frame, _totalNumFrames)));
    }
}
*/
/*
 // TODO:
int ofxHapPlayer::getCurrentFrame() const
{
    int frameNumber = 0;
    return frameNumber;
}
*/

int ofxHapPlayer::getTotalNumFrames() const
{
    std::lock_guard<std::mutex> guard(_lock);
    if (_videoStream)
    {
        return _videoStream->nb_frames;
    }
    else
    {
        return 0;
    }
}

void ofxHapPlayer::updatePTS()
{
    _frameTime = av_gettime_relative(); // TODO: _frameTime duplicates _clock._wall?
    _clock.setTimeAt(_frameTime);
    assert(_clock.getTime() <= _clock.period || _clock.period == 0);
}

int ofxHapPlayer::getTimeout() const
{
    return _timeout;
}

void ofxHapPlayer::setTimeout(int microseconds)
{
    _timeout = microseconds;
}

ofxHapPlayer::AudioOutput::AudioOutput()
{
    
}

ofxHapPlayer::AudioOutput::~AudioOutput()
{

}

int ofxHapPlayer::AudioOutput::getBestRate(int r) const
{
    auto devices = _soundStream.getDeviceList();
    for (const auto& device : devices) {
        if (device.isDefaultOutput)
        {
            auto rates = device.sampleRates;
            int bestRate = 0;
            for (auto rate : rates) {
                if (rate == r)
                {
                    return rate;
                }
                else if (rate < r && rate > bestRate)
                {
                    bestRate = rate;
                }
            }
            if (bestRate == 0)
            {
                bestRate = r;
            }
            return bestRate;
        }
    }
    return r;
}

void ofxHapPlayer::AudioOutput::start(int channels, int sampleRate, std::shared_ptr<ofxHap::RingBuffer> buffer)
{
    _buffer = buffer;
    _soundStream.setOutput(this);
    bool audio = _soundStream.setup(channels, 0, sampleRate, 128, 2); // TODO: best values for last 2 params?
    if (!audio)
    {
        ofLogError("ofxHapPlayer", "Error starting audio playback.");
    }
}

void ofxHapPlayer::AudioOutput::start()
{
    _soundStream.start();
}

void ofxHapPlayer::AudioOutput::stop()
{
    _soundStream.stop();
}

void ofxHapPlayer::AudioOutput::close()
{
    _soundStream.stop();
    _soundStream.close();
}

void ofxHapPlayer::AudioOutput::audioOut(ofSoundBuffer& buffer)
{
    int wanted = buffer.getNumFrames();
    int filled = 0;

    const float *first, *second;
    int firstCount, secondCount;

    _buffer->readBegin(first, firstCount, second, secondCount);

    if (firstCount && wanted)
    {
        int todo = std::min(wanted, firstCount);
        size_t copy = todo * sizeof(float) * buffer.getNumChannels();
        float *out = &buffer.getBuffer()[filled * buffer.getNumChannels()];
        memcpy(out, first, copy);
        filled += todo;
    }
    if (secondCount && filled < wanted)
    {
        int todo = std::min(wanted - filled, secondCount);
        size_t copy = todo * sizeof(float) * buffer.getNumChannels();
        float *out = &buffer.getBuffer()[filled * buffer.getNumChannels()];
        memcpy(out, second, copy);
        filled += todo;
    }

    _buffer->readEnd(filled);

    static bool logged = false;
    if (filled < wanted)
    {
        if (logged == false)
        {
            std::cout << "silence" << std::endl;
            logged = true;
        }
        float *out = &buffer.getBuffer()[filled * buffer.getNumChannels()];
        av_samples_set_silence((uint8_t **)&out,
                               0,
                               wanted - filled,
                               buffer.getNumChannels(),
                               AV_SAMPLE_FMT_FLT);
    }
    else
    {
        logged = false;
    }
}

ofxHapPlayer::DecodedFrame::DecodedFrame() :
    pts(-1), duration(-1)
{

}

bool ofxHapPlayer::DecodedFrame::isValid() const
{
    return (pts != AV_NOPTS_VALUE);
}

void ofxHapPlayer::DecodedFrame::invalidate()
{
    pts = AV_NOPTS_VALUE;
}

void ofxHapPlayer::DecodedFrame::clear()
{
    pts = AV_NOPTS_VALUE;
    duration = 0;
    buffer.clear();
}
