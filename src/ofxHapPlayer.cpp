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
#include <ofxHap/AudioThread.h>
#include <ofxHap/RingBuffer.h>
#include <ofxHap/MovieTime.h>
extern "C" {
#include <libavformat/avformat.h>
#include <libavutil/time.h>
#include <libswresample/swresample.h>
#include <hap.h>
}
#if defined(TARGET_WIN32)
#include <ppl.h>
#elif defined(TARGET_LINUX)
#include <dispatch/dispatch.h>
#endif

// This amount will be bufferred before and after the playhead
#define kofxHapPlayerBufferUSec INT64_C(250000)
#define kofxHapPlayerUSecPerSec 1000000L

namespace ofxHapPY {
    static const string vertexShader = "void main(void)\
    {\
    gl_FrontColor = gl_Color;\
    gl_Position = ftransform();\
    gl_TexCoord[0] = gl_MultiTexCoord0;\
    }";

    static const string fragmentShader = "uniform sampler2D cocgsy_src;\
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
    gl_FragColor = rgba * gl_Color;\
    }";

    /*
     Utility to round up to a multiple of 4 for DXT dimensions
     */
    static int roundUpToMultipleOf4( int n )
    {
        if( 0 != ( n & 3 ) )
            n = ( n + 3 ) & ~3;
        return n;
    }

#if defined(TARGET_LINUX)
    struct Work {
        void *p;
        HapDecodeWorkFunction function;
    };
    static void decodeWrapper(void *p, size_t i)
    {
        struct Work *w = static_cast<struct Work *>(p);
        w->function(w->p, i);
    }
#endif

    static void doDecode(HapDecodeWorkFunction function, void *p, unsigned int count, void *info)
    {
#if defined(TARGET_OSX)
        dispatch_apply(count, dispatch_get_global_queue(QOS_CLASS_USER_INITIATED, 0), ^(size_t i) {
            function(p, i);
        });
#elif defined(TARGET_WIN32)
        concurrency::parallel_for(0U, count, [&](unsigned int i) {
            function(p, i);
        });
#else
        struct Work w = {p, function};
        dispatch_apply_f(count, dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0), &w, decodeWrapper);
#endif
    }

    static bool frameMatchesStream(unsigned int frame, uint32_t stream)
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
}

// TODO:
// 1.a What about AudioThread when paused? stopped?
// 2. Fix sound_sync_test_hap.mov frequent audio position reset
// 3. Pause in palindrome(low priority)

ofxHapPlayer::ofxHapPlayer() :
    _loaded(false), _videoStream(nullptr), _audioStreamIndex(-1), _frameTime(av_gettime_relative()), _playing(false),
    _wantsUpload(false),
    _demuxer(), _buffer(nullptr), _audioThread(nullptr), _audioOut(), _volume(1.0), _timeout(30000),
    _positionOnLoad(0.0)
{
    _clock.setPausedAt(true, 0);
    ofAddListener(ofEvents().update, this, &ofxHapPlayer::update);
}

ofxHapPlayer::~ofxHapPlayer()
{
    /*
    Close any loaded movie
    */
    close();
    ofRemoveListener(ofEvents().update, this, &ofxHapPlayer::update);
}

bool ofxHapPlayer::load(string name)
{
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

    _positionOnLoad = 0.0;

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
    AVMediaType type = params->codec_type;
    AVCodecID codecID = params->codec_id;
#else
    AVCodecContext *codec = stream->codec;
    AVMediaType type = codec->codec_type;
    AVCodecID codecID = codec->codec_id;
#endif
    if (type == AVMEDIA_TYPE_VIDEO && codecID == AV_CODEC_ID_HAP)
    {
        _videoStream = stream;
    }
    else if (type == AVMEDIA_TYPE_AUDIO)
    {
        // We will output silence until we have samples to play
#if OFX_HAP_HAS_CODECPAR
        int channels = params->channels;
        int sampleRate = params->sample_rate;
        ofxHap::AudioParameters parameters(params, kofxHapPlayerBufferUSec, stream->start_time, stream->duration);
#else
        int channels = codec->channels;
        int sampleRate = codec->sample_rate;
        ofxHap::AudioParameters parameters(codec, kofxHapPlayerBufferUSec, stream->start_time, stream->duration);
#endif
        sampleRate = _audioOut.getBestRate(sampleRate);
        _audioStreamIndex = stream->index;
        _buffer = std::make_shared<ofxHap::RingBuffer>(channels, sampleRate / 8);

        _audioOut.configure(channels, sampleRate, _buffer);

        _audioThread = std::make_shared<ofxHap::AudioThread>(parameters, sampleRate, _buffer, *this);
        _audioThread->setVolume(_volume);
        _audioThread->sync(_clock, false);
    }
}

void ofxHapPlayer::foundAllStreams()
{
    std::lock_guard<std::mutex> guard(_lock);
    _loaded = true;
    setPositionLoaded(_positionOnLoad);
}

void ofxHapPlayer::readPacket(AVPacket *packet)
{
    // No need to lock
    if (_videoStream && packet->stream_index == _videoStream->index)
    {
        _videoPackets.store(packet);
    }
    else if (_audioThread && packet->stream_index == _audioStreamIndex)
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
    if (averror == AVERROR_INVALIDDATA)
    {
        _error += " (may not be a Hap movie)";
    }
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
    _clock.setPausedAt(true, 0);
    _wantsUpload = false;
    _videoStream = nullptr;
    _audioStreamIndex = -1;
    _shader.unload();
    _texture.clear();
    _decodedFrame.clear();
    _loaded = false;
    _error.clear();
}

void ofxHapPlayer::read(ofxHap::TimeRangeSequence& sequence)
{
    ofxHap::TimeRangeSequence flattened = ofxHap::MovieTime::flatten(sequence);
    flattened.remove(_active);

    for (const ofxHap::TimeRange& range : flattened)
    {
        int64_t lastRead = _demuxer->getLastReadTime();
        if (lastRead != AV_NOPTS_VALUE && range.earliest() > lastRead && range.earliest() - lastRead < kofxHapPlayerUSecPerSec / 4)
        {
            _demuxer->read(range.latest());
            _active.add(lastRead + 1, range.latest() - lastRead);
        }
        else
        {
            _demuxer->seekTime(range.earliest());
            _demuxer->read(range.latest());
            _active.add(range);
        }
    }
}

void ofxHapPlayer::update(ofEventArgs & args)
{
    std::lock_guard<std::mutex> guard(_lock);

    // Calculate our current position for video and audio (if present)
    updatePTS();

    if (!_loaded)
    {
        return;
    }

    int64_t pts = _clock.getTime();

    // Sequences ahead of us (to request from the demuxer) and to keep cached
    ofxHap::TimeRangeSequence future = ofxHap::MovieTime::nextRanges(_clock, _frameTime, std::min(_clock.period, kofxHapPlayerBufferUSec));
    ofxHap::TimeRangeSequence cache = ofxHap::MovieTime::nextRanges(_clock, _frameTime - kofxHapPlayerBufferUSec, std::min(_clock.period, kofxHapPlayerBufferUSec * INT64_C(2)));
    // Rescale the cache for video
    ofxHap::TimeRangeSet vcache;
    for (auto& range : cache)
    {
        // Careful rounding: don't discard needed samples - important at low rates when timebase is framerate
        ofxHap::TimeRange vrange(av_rescale_q_rnd(range.start, { 1, AV_TIME_BASE }, _videoStream->time_base, AV_ROUND_DOWN),
                                 av_rescale_q_rnd(range.length, { 1, AV_TIME_BASE }, _videoStream->time_base, AV_ROUND_UP));
        vcache.add(vrange);
    }
    // Release any packets we no longer need
    _videoPackets.limit(vcache);

    _active = _active.intersection(cache);

    read(future);

    int64_t vidPosition;
    if (_clock.getDone())
    {
        // Don't use pts from the clock which is duration - we want the last frame time
        vidPosition = _videoStream->duration - 1;
        // Stop if we have got to the end of the movie and aren't looping
        _playing = false;
    }
    else
    {
        vidPosition = av_rescale_q_rnd(pts, { 1, AV_TIME_BASE }, _videoStream->time_base, AV_ROUND_DOWN);
    }
    // No frame if the movie position outlies the video track length
    if (vidPosition > _videoStream->duration || (_videoStream->start_time != AV_NOPTS_VALUE && vidPosition < _videoStream->start_time))
    {
        _decodedFrame.invalidate();
    }
    else
    {
        // Retreive the video frame if necessary
        bool inBuffer = (_decodedFrame.isValid() && _decodedFrame.pts <= vidPosition && _decodedFrame.pts + _decodedFrame.duration > vidPosition) ? true : false;
        if (!inBuffer)
        {
            AVPacket packet;
            av_init_packet(&packet);
            packet.data = NULL;
            packet.size = 0;
            // Fetch a stored packet, blocking until our timeout only if necessary
            bool found = _videoPackets.fetch(vidPosition, &packet);
            if (!found && _demuxer->isActive())
            {
                found = _videoPackets.fetch(vidPosition, &packet, _timeout);
            }
            if (found)
            {
                unsigned int textureCount;
                unsigned int hapResult = HapGetFrameTextureCount(packet.data, packet.size, &textureCount);
                if (hapResult == HapResult_No_Error && textureCount == 1) // TODO: Hap Q+A
                {
                    unsigned int textureFormat;
                    hapResult = HapGetFrameTextureFormat(packet.data, packet.size, 0, &textureFormat);
#if OFX_HAP_HAS_CODECPAR
                    if (hapResult == HapResult_No_Error && !ofxHapPY::frameMatchesStream(textureFormat, _videoStream->codecpar->codec_tag))
#else
                    if (hapResult == HapResult_No_Error && !ofxHapPY::frameMatchesStream(textureFormat, _videoStream->codec->codec_tag))
#endif
                    {
                        hapResult = HapResult_Bad_Frame;
                    }
                    if (hapResult == HapResult_No_Error)
                    {
#if OFX_HAP_HAS_CODECPAR
                        size_t length = ofxHapPY::roundUpToMultipleOf4(_videoStream->codecpar->width) * ofxHapPY::roundUpToMultipleOf4(_videoStream->codecpar->height);
#else
                        size_t length = ofxHapPY::roundUpToMultipleOf4(_videoStream->codec->width) * ofxHapPY::roundUpToMultipleOf4(_videoStream->codec->height);
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
                                              ofxHapPY::doDecode,
                                              NULL,
                                              _decodedFrame.buffer.data(),
                                              static_cast<unsigned long>(_decodedFrame.buffer.size()),
                                              &bytesUsed,
                                              &textureFormat);
                    }
                }
                if (hapResult == HapResult_No_Error)
                {
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

        _texture.bind();

#if defined(TARGET_OSX)
        if (ofGetGLRenderer()->getGLVersionMajor() < 3)
        {
            glPushClientAttrib(GL_CLIENT_PIXEL_STORE_BIT);
        }
        glPixelStorei(GL_UNPACK_CLIENT_STORAGE_APPLE, GL_TRUE);
        glTextureRangeAPPLE(GL_TEXTURE_2D, _decodedFrame.buffer.size(), _decodedFrame.buffer.data());
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
            static_cast<GLsizei>(_decodedFrame.buffer.size()),
            _decodedFrame.buffer.data());

#if defined(TARGET_OSX)
        if (ofGetGLRenderer()->getGLVersionMajor() < 3)
        {
            glPopClientAttrib();
        }
        else
        {
            glPixelStorei(GL_UNPACK_CLIENT_STORAGE_APPLE, GL_FALSE);
        }
#endif
        _texture.unbind();
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
            bool success = _shader.setupShaderFromSource(GL_VERTEX_SHADER, ofxHapPY::vertexShader);
            if (success)
            {
                success = _shader.setupShaderFromSource(GL_FRAGMENT_SHADER, ofxHapPY::fragmentShader);
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
    if (_clock.getDone())
    {
        _clock.syncAt(0, _frameTime);
        if (_audioThread)
        {
            _audioThread->sync(_clock, false);
        }
    }
    if (_clock.getPaused())
    {
        setPaused(false, true);
    }
}

void ofxHapPlayer::stop()
{
    std::lock_guard<std::mutex> guard(_lock);
    _playing = false;
    setPaused(true, true);
}

void ofxHapPlayer::setPaused(bool pause)
{
    std::lock_guard<std::mutex> guard(_lock);
    setPaused(pause, true);
}

void ofxHapPlayer::setPaused(bool pause, bool locked)
{
    assert(locked);
    if (_clock.getPaused() != pause)
    {
        if (!pause)
        {
            _playing = true;
        }
        _clock.setPausedAt(pause, _frameTime);
        if (_audioThread)
        {
            _audioThread->sync(_clock, true);
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
    std::lock_guard<std::mutex> guard(_lock);
    if (_videoStream)
    {
#if OFX_HAP_HAS_CODECPAR
        switch (_videoStream->codecpar->codec_tag) {
#else
        switch (_videoStream->codec->codec_tag) {
#endif
            case MKTAG('H', 'a', 'p', '5'):
                return OF_PIXELS_RGBA;
            default:
                return OF_PIXELS_RGB;

        }
    }
    return OF_PIXELS_UNKNOWN;
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
            _audioThread->sync(_clock, false);
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
    _clock.setRateAt(speed, _frameTime);
    if (_audioThread)
    {
        _audioThread->sync(_clock, true);
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
    return _clock.getDone();
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
    if (_loaded)
    {
        setPositionLoaded(pct);
    }
    else
    {
        _positionOnLoad = pct;
    }
}

void ofxHapPlayer::setPositionLoaded(float pct)
{
    int64_t time = ofClamp(pct, 0.0f, 1.0f) * (_clock.period - 1);
    setPTSLoaded(time);
}

void ofxHapPlayer::setVideoPTSLoaded(int64_t pts, bool round_up)
{
    pts = std::max(std::min(av_rescale_q_rnd(pts, _videoStream->time_base, { 1, AV_TIME_BASE }, round_up ? AV_ROUND_UP : AV_ROUND_DOWN), _clock.period - 1), INT64_C(0));
    setPTSLoaded(pts);
}

void ofxHapPlayer::setPTSLoaded(int64_t pts)
{
    _clock.syncAt(pts, _frameTime);
    if (_audioThread)
    {
        _audioThread->sync(_clock, false);
    }
}

void ofxHapPlayer::firstFrame()
{
    std::lock_guard<std::mutex> guard(_lock);
    if (_loaded)
    {
        int64_t time = _videoStream->start_time;
        if (time == AV_NOPTS_VALUE)
        {
            time = 0;
        }
        setVideoPTSLoaded(time, false);
    }
    else
    {
        _positionOnLoad = 0.0f;
    }
}

void ofxHapPlayer::nextFrame()
{
    std::lock_guard<std::mutex> guard(_lock);
    if (_loaded && _decodedFrame.isValid())
    {
        setVideoPTSLoaded(std::min(_decodedFrame.pts + _decodedFrame.duration, _videoStream->duration - 1), true);
    }
}

void ofxHapPlayer::previousFrame()
{
    std::lock_guard<std::mutex> guard(_lock);
    if (_loaded && _decodedFrame.isValid())
    {
        setVideoPTSLoaded(_decodedFrame.pts - 1, false);
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
    _frameTime = av_gettime_relative();
    _clock.setTimeAt(_frameTime);
    assert(_clock.getTime() <= _clock.period || _clock.period == 0);
}

int ofxHapPlayer::getTimeout() const
{
    return _timeout.count();
}

void ofxHapPlayer::setTimeout(int microseconds)
{
    _timeout = std::chrono::microseconds(microseconds);
}

ofxHapPlayer::AudioOutput::AudioOutput()
: _started(false), _channels(0), _sampleRate(0)
{
    
}

ofxHapPlayer::AudioOutput::~AudioOutput()
{

}

unsigned int ofxHapPlayer::AudioOutput::getBestRate(unsigned int r) const
{
    auto devices = _soundStream.getDeviceList();
    for (const auto& device : devices) {
        if (device.isDefaultOutput)
        {
            auto rates = device.sampleRates;
            unsigned int bestRate = 0;
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

void ofxHapPlayer::AudioOutput::configure(int channels, int sampleRate, std::shared_ptr<ofxHap::RingBuffer> buffer)
{
    _buffer = buffer;
    _channels = channels;
    _sampleRate = sampleRate;
}

void ofxHapPlayer::AudioOutput::start()
{
    if (!_started)
    {
        ofSoundStreamSettings settings;
        settings.numInputChannels = 0;
        settings.numOutputChannels = _channels;
        settings.sampleRate = _sampleRate;
        settings.setOutListener(this);

        // TODO: best values for last 2 params?
        settings.bufferSize = 128;
        settings.numBuffers = 2;

        _started = _soundStream.setup(settings);
        if (!_started)
        {
            ofLogError("ofxHapPlayer", "Error starting audio playback.");
        }
    }
    else
    {
        _soundStream.start();
    }
}

void ofxHapPlayer::AudioOutput::stop()
{
    _soundStream.stop();
}

void ofxHapPlayer::AudioOutput::close()
{
    _soundStream.stop();
    _soundStream.close();
    _started = false;
}

void ofxHapPlayer::AudioOutput::audioOut(ofSoundBuffer& buffer)
{
    int wanted = static_cast<int>(buffer.getNumFrames());
    int filled = 0;

    const float *src[2];
    int count[2];

    _buffer->readBegin(src[0], count[0], src[1], count[1]);

    for (int i = 0; i < 2; i++)
    {
        int todo = std::min(wanted - filled, count[i]);
        if (todo > 0)
        {
            size_t copy = todo * sizeof(float) * buffer.getNumChannels();
            float *out = &buffer.getBuffer()[filled * buffer.getNumChannels()];
            memcpy(out, src[i], copy);
            filled += todo;
        }
    }

    _buffer->readEnd(filled);

    if (filled < wanted)
    {
        float *out = &buffer.getBuffer()[filled * buffer.getNumChannels()];
        av_samples_set_silence((uint8_t **)&out,
                               0,
                               wanted - filled,
                               static_cast<int>(buffer.getNumChannels()),
                               AV_SAMPLE_FMT_FLT);
    }
}

void ofxHapPlayer::startAudio()
{
    _audioOut.start();
}

void ofxHapPlayer::stopAudio()
{
    _audioOut.stop();
}

ofxHapPlayer::DecodedFrame::DecodedFrame() :
    pts(AV_NOPTS_VALUE), duration(0)
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
    // Force deallocation of the vector's storage
    // (std::vector::clear() is not required to deallocate storage)
    std::vector<char>().swap(buffer);
}
