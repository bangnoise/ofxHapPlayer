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
#include <ofxHap/RingBuffer.h>
#include <ofxHap/AudioThread.h>
#include <ofxHap/TimeRangeSet.h>

class ofxHapPlayer : public ofBaseVideoPlayer, public ofxHap::PacketReceiver {
public:
    ofxHapPlayer();
    virtual ~ofxHapPlayer();

    // TODO: allow these
    ofxHapPlayer(ofxHapPlayer const &) = delete;
    void operator=(ofxHapPlayer const &x) = delete;

    virtual bool                load(std::string name);
    virtual void                close();
    virtual void                update() {};
    
    virtual void                play();
    virtual void                stop();     
    
    virtual bool                isFrameNew() const;
    virtual ofPixels&           getPixels();
    virtual const ofPixels&     getPixels() const;

    virtual ofTexture *         getTexture();
    virtual ofShader *          getShader();
    virtual float               getWidth() const;
    virtual float               getHeight() const;
    
    virtual bool                isPaused() const;
    virtual bool                isLoaded() const;
    virtual bool                isPlaying() const;
    std::string                 getError() const;
    
    virtual bool                setPixelFormat(ofPixelFormat pixelFormat) {return false;};
    virtual ofPixelFormat       getPixelFormat() const;
    virtual string              getMoviePath() const;
    virtual bool				getHapAvailable() const; // TODO: delete (and mvar)?
	
    virtual float               getPosition() const;
    virtual float               getSpeed() const;
    virtual float               getDuration() const;
    virtual bool                getIsMovieDone() const;

    virtual void                setPaused(bool pause);
    virtual void                setPosition(float pct);
    float                       getVolume() const;
    virtual void                setVolume(float volume); // 0..1
    virtual void                setLoopState(ofLoopType state);
    virtual void                setSpeed(float speed);
/*  virtual void                setFrame(int frame);  // frame 0 = first frame... // TODO: */
/*  virtual int                 getCurrentFrame() const; // TODO: */
    virtual int                 getTotalNumFrames() const;
    virtual ofLoopType          getLoopState() const;
    /*
    virtual void                firstFrame();
    virtual void                nextFrame();
    virtual void                previousFrame();
    */
    //
    virtual void                draw(float x, float y);
    virtual void                draw(float x, float y, float width, float height);

    /*
     The timeout value determines how long calls to update() wait for
     a frame to be ready before giving up.
     */
    int                         getTimeout() const;
    void                        setTimeout(int microseconds);
protected:
    virtual void    foundMovie(int64_t duration);
    virtual void    foundStream(AVStream *stream);
    virtual void    foundAllStreams();
    virtual void    readPacket(AVPacket *packet);
    virtual void    discontinuity();
    virtual void    endMovie();
    virtual void    error(int averror);
private:
    void            update(ofEventArgs& args);
    void            updatePTS();
    void            limit(ofxHap::TimeRangeSet& set) const;
    void            read(const ofxHap::TimeRangeSet& range, bool seek);
    class AudioOutput : public ofBaseSoundOutput {
    public:
        AudioOutput();
        ~AudioOutput();
        void start(int channels, int sampleRate, std::shared_ptr<ofxHap::RingBuffer> buffer);
        void start();
        void stop();
        void close();
        unsigned int  getBestRate(unsigned int rate) const;
    private:
        virtual void    audioOut(ofSoundBuffer& buffer) override;
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
    AVStream            *_audioStream;
    DecodedFrame        _decodedFrame;
    ofxHap::Clock       _clock;
    uint64_t            _frameTime;
    bool                _hadFirstVideoFrame;
    ofShader            _shader;
    ofTexture           _texture;
    bool                _playing;
    bool                _wantsUpload;
	string              _moviePath;
    ofxHap::TimeRangeSet _active;
    ofxHap::PacketCache _videoPackets;
    std::shared_ptr<ofxHap::Demuxer>        _demuxer;
    std::shared_ptr<ofxHap::RingBuffer>     _buffer;
    std::shared_ptr<ofxHap::AudioThread>   _audioThread;
    AudioOutput         _audioOut;
    float               _volume;
    int                 _timeout;
};

#endif /* defined(__ofxHapPlayer__) */
