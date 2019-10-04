/*
 ofxHapPlayer.h
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

#ifndef __ofxHapPlayer__
#define __ofxHapPlayer__

#include "ofMain.h"
#include <vector>
#include <ofxHap/Clock.h>
#include <ofxHap/PacketCache.h>
#include <ofxHap/Demuxer.h>
#include <ofxHap/AudioThread.h>
#include <ofxHap/TimeRangeSet.h>

namespace ofxHap {
    class AudioThread;
    class RingBuffer;
}

class ofxHapPlayer : public ofBaseVideoPlayer, public ofxHap::PacketReceiver, public ofxHap::AudioThread::Receiver {
public:
    ofxHapPlayer();
    virtual ~ofxHapPlayer();

    // TODO: allow these
    ofxHapPlayer(ofxHapPlayer const &) = delete;
    ofxHapPlayer& operator=(ofxHapPlayer const &x) = delete;

    virtual bool                load(std::string name) override;
    virtual void                close() override;
    virtual void                update() override {};
    
    virtual void                play() override;
    virtual void                stop() override;
    
    virtual bool                isFrameNew() const override;
    virtual ofPixels&           getPixels() override;
    virtual const ofPixels&     getPixels() const override;

    virtual ofTexture *         getTexture();
    virtual ofShader *          getShader();
    virtual float               getWidth() const override;
    virtual float               getHeight() const override;
    
    virtual bool                isPaused() const override;
    virtual bool                isLoaded() const override;
    virtual bool                isPlaying() const override;
    std::string                 getError() const;
    
    virtual bool                setPixelFormat(ofPixelFormat pixelFormat) override {return false;};

    /*
    Returns OF_PIXELS_RGBA, OF_PIXELS_RGB or OF_PIXELS_UNKNOWN
    */
    virtual ofPixelFormat       getPixelFormat() const override;
    virtual string              getMoviePath() const;
    virtual bool				getHapAvailable() const; // TODO: delete (and mvar)?
	
    virtual float               getPosition() const override;
    virtual float               getSpeed() const override;
    virtual float               getDuration() const override;
    virtual bool                getIsMovieDone() const override;

    virtual void                setPaused(bool pause) override;
    virtual void                setPosition(float pct) override;
    float                       getVolume() const;
    virtual void                setVolume(float volume) override; // 0..1
    virtual void                setLoopState(ofLoopType state) override;
    virtual void                setSpeed(float speed) override;
/*  virtual void                setFrame(int frame);  // frame 0 = first frame... // TODO: */
/*  virtual int                 getCurrentFrame() const; // TODO: */
    virtual int                 getTotalNumFrames() const override;
    virtual ofLoopType          getLoopState() const override;

    virtual void                firstFrame() override;
    virtual void                nextFrame() override;
    virtual void                previousFrame() override;

    //
    virtual void                draw(float x, float y);
    virtual void                draw(float x, float y, float width, float height);

    /*
     The timeout value determines how long calls to update() wait for
     a frame to be ready before giving up.
     */
    int                         getTimeout() const;
    void                        setTimeout(int microseconds);
private:
    virtual void    foundMovie(int64_t duration) override;
    virtual void    foundStream(AVStream *stream) override;
    virtual void    foundAllStreams() override;
    virtual void    readPacket(AVPacket *packet) override;
    virtual void    discontinuity() override;
    virtual void    endMovie() override;
    virtual void    error(int averror) override;
    virtual void    startAudio() override;
    virtual void    stopAudio() override;
    void            setPaused(bool pause, bool locked);
    void            setVideoPTSLoaded(int64_t pts, bool round_up);
    void            setPTSLoaded(int64_t pts);
    void            setPositionLoaded(float pct);
    void            update(ofEventArgs& args);
    void            updatePTS();
    void            read(ofxHap::TimeRangeSequence& sequence);
    class AudioOutput : public ofBaseSoundOutput {
    public:
        AudioOutput();
        ~AudioOutput();
        void configure(int channels, int sampleRate, std::shared_ptr<ofxHap::RingBuffer> buffer);
        void start();
        void stop();
        void close();
        unsigned int getBestRate(unsigned int rate) const;
        virtual void audioOut(ofSoundBuffer& buffer) override;
    private:
        bool                                _started;
        int                                 _channels;
        int                                 _sampleRate;
        std::shared_ptr<ofxHap::RingBuffer> _buffer;
        ofSoundStream                       _soundStream;
    };
    class DecodedFrame {
    public:
        DecodedFrame();
        bool    isValid() const;
        void    invalidate();
        void    clear();
        std::vector<char>   buffer;
        int64_t             pts;
        int64_t             duration;
    };
    mutable std::mutex  _lock;
    bool                _loaded;
    std::string         _error;
    AVStream            *_videoStream;
    int                 _audioStreamIndex;
    DecodedFrame        _decodedFrame;
    ofxHap::Clock       _clock;
    uint64_t            _frameTime;
    ofShader            _shader;
    ofTexture           _texture;
    bool                _playing;
    bool                _wantsUpload;
	string              _moviePath;
    ofxHap::TimeRangeSet _active;
    ofxHap::LockingPacketCache              _videoPackets;
    std::shared_ptr<ofxHap::Demuxer>        _demuxer;
    std::shared_ptr<ofxHap::RingBuffer>     _buffer;
    std::shared_ptr<ofxHap::AudioThread>   _audioThread;
    AudioOutput         _audioOut;
    float               _volume;
    std::chrono::microseconds               _timeout;
    float               _positionOnLoad;
};

#endif /* defined(__ofxHapPlayer__) */
