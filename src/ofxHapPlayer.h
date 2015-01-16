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

class ofxHapPlayer : public ofBaseVideoPlayer {
public:
    ofxHapPlayer();
    virtual ~ofxHapPlayer();
    
    virtual bool                loadMovie(string name);
    virtual void                close();
    virtual void                update();
    
    virtual void                play();
    virtual void                stop();     
    
    virtual bool                isFrameNew() const;
    virtual bool                isFrameNew();
    virtual unsigned char *     getPixels() {return NULL;};
    virtual ofPixelsRef         getPixelsRef();
    virtual const ofPixels&     getPixelsRef() const;
    virtual ofTexture *         getTexture();
    virtual ofShader *          getShader();
    virtual float               getWidth() const;
    virtual float               getWidth();
    virtual float               getHeight() const;
    virtual float               getHeight();
    
    virtual bool                isPaused() const;
    virtual bool                isPaused();
    virtual bool                isLoaded() const;
    virtual bool                isLoaded();
    virtual bool                isPlaying() const;
    virtual bool                isPlaying();
    
    virtual bool                setPixelFormat(ofPixelFormat pixelFormat) {return false;};
    virtual ofPixelFormat       getPixelFormat() const;
    virtual ofPixelFormat       getPixelFormat();
    virtual string              getMoviePath() const;
    virtual bool				getHapAvailable() const;
	
    virtual float               getPosition() const;
    virtual float               getPosition();
    virtual float               getSpeed() const;
    virtual float               getSpeed();
    virtual float               getDuration() const;
    virtual float               getDuration();
    virtual bool                getIsMovieDone();

    virtual void                setPaused(bool pause);
    virtual void                setPosition(float pct);
    virtual void                setVolume(float volume); // 0..1
    virtual void                setLoopState(ofLoopType state);
    virtual void                setSpeed(float speed);
    virtual void                setFrame(int frame);  // frame 0 = first frame...
    virtual int                 getCurrentFrame() const;
    virtual int                 getCurrentFrame();
    virtual int                 getTotalNumFrames() const;
    virtual int                 getTotalNumFrames();
    virtual ofLoopType          getLoopState() const;
    virtual ofLoopType          getLoopState();
    /*
    virtual void                firstFrame();
    virtual void                nextFrame();
    virtual void                previousFrame();
    */
    //
    virtual void                draw(float x, float y);
    virtual void                draw(float x, float y, float width, float height);

protected:
    void *          _movie;
private:
    void *          _gWorld;
    unsigned char * _buffer;
    ofShader        _shader;
    ofTexture       _texture;
    bool            _shaderLoaded;
    bool            _playing;
    float           _speed;
    bool            _paused;
    ofLoopType      _loopState;
    bool            _wantsUpdate;
	string			_moviePath;
	bool			_hapAvailable;
    int             _totalNumFrames;
    int             _lastKnownFrameNumber;
    int             _lastKnownFrameTime;
};

#endif /* defined(__ofxHapPlayer__) */
