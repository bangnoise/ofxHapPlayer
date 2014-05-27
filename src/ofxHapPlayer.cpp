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
 
 This is not a general example of how to use Hap via QuickTime.
 
 OpenFrameworks on Windows builds against QuickTime 6.0.2, which severely limits the APIs available.

 */
#include "ofxHapPlayer.h"
#include "HapSupport.h"
#if defined(TARGET_WIN32)
#include <QTML.h>
#include <Movies.h>
#define ofxHapPlayerFloatToFixed(x) X2Fix(x)
#elif defined(TARGET_OSX)
#include <QuickTime/QuickTime.h>
/*
 Much of QuickTime is deprecated in recent MacOS but no equivalent functionality exists in modern APIs,
 so we ignore these warnings.
 */
#pragma GCC push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
/*
 These functions have been excised from modern MacOS headers but remain available
 */
#if !defined(__QDOFFSCREEN__)
extern "C" {
    void DisposeGWorld(GWorldPtr offscreenGWorld);
    void GetGWorld(CGrafPtr *port, GDHandle *gdh);
    PixMapHandle GetGWorldPixMap(GWorldPtr offscreenGWorld);
    void SetGWorld(CGrafPtr port, GDHandle gdh);
    Boolean LockPixels(PixMapHandle pm);
    void UnlockPixels(PixMapHandle pm);

    enum {
        useTempMem = 1L << 2,
        keepLocal = 1L << 3
    };
}
#endif
#define ofxHapPlayerFloatToFixed(x) FloatToFixed(x)
#endif

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

/*
Utility to round up to a multiple of 4 for DXT dimensions
*/
static int roundUpToMultipleOf4( int n )
{
    if( 0 != ( n & 3 ) )
        n = ( n + 3 ) & ~3;
    return n;
}

/*
Utility to know if we are working with DXT data
*/
static bool isDXTPixelFormat(OSType fmt)
{
    switch (fmt)
    {
    case kHapPixelFormatTypeRGB_DXT1:
    case kHapPixelFormatTypeRGBA_DXT5:
    case kHapPixelFormatTypeYCoCg_DXT5:
        return true;
    default:
        return false;
    }
}

/*
Called by QuickTime when we have a new frame
*/
OSErr ofxHapPlayerDrawComplete(Movie theMovie, long refCon){

    *(bool *)refCon = true;
    return noErr;
}

ofxHapPlayer::ofxHapPlayer() : _movie(NULL), _buffer(NULL), _gWorld(NULL), _shaderLoaded(false), _playing(false), _speed(1.0), _loopState(OF_LOOP_NORMAL), _wantsUpdate(false), _hapAvailable(false), _totalNumFrames(-1), _lastKnownFrameNumber(0), _lastKnownFrameTime(0)
{
    /*
    http://developer.apple.com/library/mac/#documentation/QuickTime/Conceptual/QT_InitializingQT/InitializingQT/InitializingQTfinal.html
    */
#if defined(TARGET_WIN32)
    /*
    Calls to InitializeQTML() must be balanced by calls to TerminateQTML()
    */
    InitializeQTML(0);
#endif
    /*
    There is no harm or cost associated with calling EnterMovies() after the first call, and
    it should NOT be balanced by calls to ExitMovies()
    */
    EnterMovies();
}

ofxHapPlayer::~ofxHapPlayer()
{
    /*
    Close any loaded movie
    */
    close();
    /*
    Dispose of our GWorld and buffer if we created them
    */
    if (_gWorld)
    {
        DisposeGWorld((GWorldPtr)_gWorld);
        _gWorld = NULL;
    }
    if (_buffer)
    {
        delete[] _buffer;
        _buffer = NULL;
    }
#if defined(TARGET_WIN32)
    /*
    Balance our call to InitializeQTML()
    */
    TerminateQTML();
#endif
}

bool ofxHapPlayer::loadMovie(string name)
{
    OSStatus result = noErr;
	_moviePath = name;
	
    /*
    Close any open movie
    */
    close();

    /*
     Opening a movie requires an active graphics world, so set a dummy one
     */
    CGrafPtr previousGWorld;
    GDHandle previousGDH;
    
    GetGWorld(&previousGWorld, &previousGDH);

    GWorldPtr tempGWorld;
    const Rect bounds = {0, 0, 1, 1};
    QTNewGWorld(&tempGWorld, 32, &bounds, NULL, NULL, useTempMem | keepLocal);
    
    SetGWorld(tempGWorld, NULL);

    bool isURL;
    /*
    Load the file or URL
    */
    if (name.compare(0, 7, "http://") == 0 ||
        name.compare(0, 8, "https://") == 0 ||
        name.compare(0, 7, "rtsp://") == 0)
    {
        isURL = true;
    }
    else
    {
        name = ofToDataPath(name);
        isURL = false;
    }

#if defined(TARGET_WIN32)
    if (isURL)
    {
        // TODO: enable once tested on Windows
#if 0
        Handle dataRef;
        const char *url = (char *)name.c_str();

        dataRef = NewHandle(strlen(url) + 1);
        result = MemError();

        if (result == noErr)
        {
            BlockMoveData(url, *dataRef, strlen(url) + 1);

            result = NewMovieFromDataRef((Movie *)&_movie, 0, NULL, dataRef, URLDataHandlerSubType);
            DisposeHandle(dataRef);
        }
#endif
    }
    else
    {
        FSSpec fsSpec;
        short fileRefNum;
        result = NativePathNameToFSSpec((char *)name.c_str(), &fsSpec, 0);
        if (result == noErr)
        {
            result = OpenMovieFile(&fsSpec, &fileRefNum, fsRdPerm);
        }
        if (result == noErr)
        {
            short resID = 0;
            result = NewMovieFromFile((Movie *)&_movie, fileRefNum, &resID, NULL, 0, NULL);
            CloseMovieFile(fileRefNum);
        }
    }
#else
    // Create the property array and store the movie properties
    QTNewMoviePropertyElement movieProps[2] = {0};

    // Location

    CFStringRef path = CFStringCreateWithCStringNoCopy(kCFAllocatorDefault, name.c_str(), kCFStringEncodingUTF8, kCFAllocatorNull);
    CFURLRef url = NULL;

    if (path == NULL) result = 1;

    movieProps[0].propClass = kQTPropertyClass_DataLocation;

    if (result == noErr && isURL)
    {
        url = CFURLCreateWithString(kCFAllocatorDefault, path, NULL);

        if (url == NULL) result = 1;

        movieProps[0].propID = kQTDataLocationPropertyID_CFURL;
        movieProps[0].propValueSize = sizeof(url);
        movieProps[0].propValueAddress = (void*)&url;
    }
    else
    {
        movieProps[0].propID = kQTDataLocationPropertyID_CFStringNativePath;
        movieProps[0].propValueSize = sizeof(path);
        movieProps[0].propValueAddress = (void*)&path;
    }

    movieProps[0].propStatus = 0;

    Boolean boolFalse = false;

    // Don't make the Movie active

    movieProps[1].propClass = kQTPropertyClass_NewMovieProperty;
    movieProps[1].propID = kQTNewMoviePropertyID_Active;
    movieProps[1].propValueSize = sizeof(boolFalse);
    movieProps[1].propValueAddress = &boolFalse;
    movieProps[1].propStatus = 0;

    // Create the movie
    
    if (result == noErr)
    {
        result = NewMovieFromProperties(2, movieProps, 0, NULL, (Movie *)&_movie);
    }
    
    if (path) CFRelease(path);
    if (url) CFRelease(url);
#endif

    if (result != noErr)
    {
        ofLog(OF_LOG_ERROR, "Error loading movie");
    }

    OSType wantedPixelFormat;
    Rect renderRect;
    size_t bitsPerPixel;
	
    if (result == noErr)
    {
        /*
        Determine the movie rectangle
        */
        GetMovieBox((Movie)_movie, &renderRect);
        /*
        Determine the pixel-format to use and adjust dimensions for Hap
        */
        if (HapQTQuickTimeMovieHasHapTrackPlayable((Movie)_movie))
        {
			_hapAvailable = true;
            wantedPixelFormat = HapQTGetQuickTimeMovieHapPixelFormat((Movie)_movie);
            renderRect.bottom = roundUpToMultipleOf4(renderRect.bottom);
            renderRect.right = roundUpToMultipleOf4(renderRect.right);
        }
        else
        {
            /*
             For now we fail for non-Hap movies
             */
            result = 1;
        }
    }
    
    /*
    Check any existing GWorld is appropriate for this movie
    */
    if (result == noErr && _gWorld != NULL)
    {
        PixMapHandle pmap = GetGWorldPixMap((GWorldPtr)_gWorld);
        if ((*pmap)->pixelFormat != wantedPixelFormat ||
            (*pmap)->bounds.bottom != renderRect.bottom ||
            (*pmap)->bounds.right != renderRect.right)
        {
            DisposeGWorld((GWorldPtr)_gWorld);
            _gWorld = NULL;
            delete[] _buffer;
            _buffer = NULL;
        }
    }

    if (result == noErr && _gWorld == NULL)
    {
        /*
        Create a GWorld to render into
        */
        size_t bitsPerPixel;
        GLint internalFormat;
        switch (wantedPixelFormat) {
        case kHapPixelFormatTypeRGB_DXT1:
            bitsPerPixel = 4;
            internalFormat = GL_COMPRESSED_RGB_S3TC_DXT1_EXT;
            break;
        case kHapPixelFormatTypeRGBA_DXT5:
        case kHapPixelFormatTypeYCoCg_DXT5:
            bitsPerPixel = 8;
            internalFormat = GL_COMPRESSED_RGBA_S3TC_DXT5_EXT;
            break;
        default:
            bitsPerPixel = 32;
            internalFormat = GL_RGBA;
            break;
        }
        size_t bytesPerRow = renderRect.right * bitsPerPixel / 8;
        _buffer = new unsigned char[renderRect.bottom * bytesPerRow];
        QTNewGWorldFromPtr((GWorldPtr *)&_gWorld, wantedPixelFormat, &renderRect, NULL, NULL, 0, (Ptr)_buffer, bytesPerRow);

        /*
        Create our texture for DXT upload
        */
        ofTextureData texData;

        texData.width = renderRect.right;
        texData.height = renderRect.bottom;
        texData.textureTarget = GL_TEXTURE_2D;
        texData.glTypeInternal = internalFormat;
#if (OF_VERSION_MAJOR == 0) && (OF_VERSION_MINOR < 8)
        texData.glType = GL_BGRA;
        texData.pixelType = GL_UNSIGNED_INT_8_8_8_8_REV;

        _texture.allocate(texData);
#else
        _texture.allocate(texData, GL_BGRA, GL_UNSIGNED_INT_8_8_8_8_REV);
#endif

#if defined(TARGET_OSX)
        _texture.bind();
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_STORAGE_HINT_APPLE , GL_STORAGE_SHARED_APPLE);
        _texture.unbind();
#endif     
    }

    /*
    Associate the GWorld with the movie
    */
    if (result == noErr)
    {
        SetMovieGWorld((Movie)_movie, (GWorldPtr)_gWorld, NULL);

        static MovieDrawingCompleteUPP drawCompleteProc = NewMovieDrawingCompleteUPP(ofxHapPlayerDrawComplete);

        SetMovieDrawingCompleteProc((Movie)_movie, movieDrawingCallWhenChanged, drawCompleteProc, (long)&_wantsUpdate);
    }

    /*
    Restore the previous GWorld and destroy the temporary one
    */
    SetGWorld(previousGWorld, previousGDH);
    DisposeGWorld(tempGWorld);
    
    if (result == noErr)
    {
        /*
        Apply our current state to the movie
        */
        this->setLoopState(_loopState);
        if (_playing) this->play();
        this->setSpeed(_speed);

        return true;
    }
    else
    {
        this->close();
        return false;
    }
}

void ofxHapPlayer::close()
{
    if (_movie)
    {
        DisposeMovie((Movie)_movie);
        _movie = NULL;
        _wantsUpdate = false;
        _hapAvailable = false;
        _totalNumFrames = -1;
        _lastKnownFrameNumber = 0;
        _lastKnownFrameTime = 0;
    }
}

void ofxHapPlayer::update()
{
    if (_movie) MoviesTask((Movie)_movie, 0);
}

bool ofxHapPlayer::getHapAvailable()
{
	return _hapAvailable;
}

ofTexture* ofxHapPlayer::getTexture()
{
    if (_wantsUpdate && _gWorld != NULL)
    {
        PixMapHandle pmap = GetGWorldPixMap((GWorldPtr)_gWorld);
        LockPixels(pmap);

        if (isDXTPixelFormat((*pmap)->pixelFormat))
        {
            GLenum internalFormat;
            GLsizei dataLength;
            switch ((*pmap)->pixelFormat)
            {
                case kHapPixelFormatTypeRGB_DXT1:
                    internalFormat = GL_COMPRESSED_RGB_S3TC_DXT1_EXT;
                    dataLength = (*pmap)->bounds.right * (*pmap)->bounds.bottom / 2;
                    break;
                case kHapPixelFormatTypeRGBA_DXT5:
                case kHapPixelFormatTypeYCoCg_DXT5:
                default:
                    internalFormat = GL_COMPRESSED_RGBA_S3TC_DXT5_EXT;
                    dataLength = (*pmap)->bounds.right * (*pmap)->bounds.bottom;
                    break;
            }
            
            glPushClientAttrib(GL_CLIENT_PIXEL_STORE_BIT);
            glEnable(GL_TEXTURE_2D);

            ofTextureData &texData = _texture.getTextureData();
            {
                glBindTexture(GL_TEXTURE_2D, texData.textureID);
            }

#if defined(TARGET_OSX)
            glTextureRangeAPPLE(GL_TEXTURE_2D, dataLength, (*pmap)->baseAddr);
            glPixelStorei(GL_UNPACK_CLIENT_STORAGE_APPLE, GL_TRUE);
#endif

            glCompressedTexSubImage2D(GL_TEXTURE_2D,
                0,
                0,
                0,
                (*pmap)->bounds.right,
                (*pmap)->bounds.bottom,
                internalFormat,
                dataLength,
                (*pmap)->baseAddr);

            glPopClientAttrib();
            glDisable(GL_TEXTURE_2D);
        }
        else
        {
            // TODO: regular upload using ofTexture's load stuff
        }
        UnlockPixels(pmap);
        _wantsUpdate = false;
    }
    return &_texture;
}

ofShader *ofxHapPlayer::getShader()
{
    if (_gWorld != NULL)
    {
        PixMapHandle pmap = GetGWorldPixMap((GWorldPtr)_gWorld);
        if ((*pmap)->pixelFormat == kHapPixelFormatTypeYCoCg_DXT5)
        {
            if (_shaderLoaded == false)
            {
                bool success = _shader.setupShaderFromSource(GL_VERTEX_SHADER, ofxHapPlayerVertexShader);
                if (success)
                {
                    success = _shader.setupShaderFromSource(GL_FRAGMENT_SHADER, ofxHapPlayerFragmentShader);
                }
                if (success)
                {
                    success = _shader.linkProgram();
                }
                if (success)
                {
                    _shaderLoaded = true;
                }
            }
            if (_shaderLoaded) return &_shader;
        }
    }
    return NULL;
}

string ofxHapPlayer::getMoviePath() {
	return _moviePath;
}

void ofxHapPlayer::draw(float x, float y) {
    draw(x,y, getWidth(), getHeight());
}

void ofxHapPlayer::draw(float x, float y, float w, float h) {
    ofTexture *t = getTexture();
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

void ofxHapPlayer::play()
{
    if (_movie)
    {
        StartMovie((Movie)_movie);
        SetMovieRate((Movie)_movie, ofxHapPlayerFloatToFixed(_speed));
    }
    _playing = true;
}

void ofxHapPlayer::stop()
{
    if (_movie)
    {
        StopMovie((Movie)_movie);
    }
    _playing = false;
}

void ofxHapPlayer::setPaused(bool pause)
{
    if (_movie)
    {
        if (pause)
        {
            if (GetMovieActive((Movie)_movie))
            {
                SetMovieRate((Movie)_movie, ofxHapPlayerFloatToFixed(0.0));
            }
        }
        else
        {
            if (!GetMovieActive((Movie)_movie))
            {
                StartMovie((Movie)_movie);
            }
            SetMovieRate((Movie)_movie, ofxHapPlayerFloatToFixed(_speed));
        }
    }
    _paused = pause;
}

bool ofxHapPlayer::isFrameNew()
{
    return _wantsUpdate;
}

float ofxHapPlayer::getWidth()
{
    if (_gWorld && _movie)
    {
        PixMapHandle pmap = GetGWorldPixMap((GWorldPtr)_gWorld);
        return (*pmap)->bounds.right;
    }
    return 0.0;
}

float ofxHapPlayer::getHeight()
{
    if (_gWorld && _movie)
    {
        PixMapHandle pmap = GetGWorldPixMap((GWorldPtr)_gWorld);
        return (*pmap)->bounds.bottom;
    }
    return 0.0;
}

bool ofxHapPlayer::isPaused()
{
    return _paused;
}

bool ofxHapPlayer::isLoaded()
{
    if (_movie) return true;
    else return false;
}

bool ofxHapPlayer::isPlaying()
{
    return _playing;
}

ofPixelsRef ofxHapPlayer::getPixelsRef()
{
    static ofPixels ref;
    return ref;
}

ofPixelFormat ofxHapPlayer::getPixelFormat()
{
    return OF_PIXELS_BGRA;
}

void ofxHapPlayer::setLoopState(ofLoopType state)
{
    if (_movie)
    {
        TimeBase movieTimeBase = GetMovieTimeBase((Movie)_movie);
        long flags = GetTimeBaseFlags(movieTimeBase);

        switch (state) {
        case OF_LOOP_NONE:
            SetMoviePlayHints((Movie)_movie, 0, hintsPalindrome | hintsLoop);
            flags &= ~loopTimeBase;
            flags &= ~palindromeLoopTimeBase;
            break;
        case OF_LOOP_NORMAL:
            SetMoviePlayHints((Movie)_movie, hintsLoop, hintsLoop);
            SetMoviePlayHints((Movie)_movie, 0, hintsPalindrome);
            flags |= loopTimeBase;
            flags &= ~palindromeLoopTimeBase;
            break;
        case OF_LOOP_PALINDROME:
            SetMoviePlayHints((Movie)_movie, hintsLoop, hintsLoop);
            SetMoviePlayHints((Movie)_movie, hintsPalindrome, hintsPalindrome);
            flags |= loopTimeBase;
            flags |= palindromeLoopTimeBase;
            break;
        }
        SetTimeBaseFlags(movieTimeBase, flags);
    }
    _loopState = state;
}

ofLoopType ofxHapPlayer::getLoopState()
{
    return _loopState;
}

float ofxHapPlayer::getSpeed()
{
    return _speed;
}

void ofxHapPlayer::setSpeed(float speed)
{
    if (_movie && _playing)
    {
        SetMovieRate((Movie)_movie, ofxHapPlayerFloatToFixed(speed));
    }
    _speed = speed;
}

float ofxHapPlayer::getDuration()
{
    if (_movie)
    {
        TimeValue duration = GetMovieDuration((Movie)_movie);
        TimeScale timescale = GetMovieTimeScale((Movie)_movie);
        return (float)duration / (float)timescale;
    }
    return 0.0;
}

float ofxHapPlayer::getPosition()
{
    if (_movie)
    {
        TimeValue duration = GetMovieDuration((Movie)_movie);
        if (duration != 0)
        {
            return (float)GetMovieTime((Movie)_movie, NULL) / (float)duration;
        }
    }
    return 0.0;
}

void ofxHapPlayer::setPosition(float pct)
{
    if (_movie)
    {
        pct = ofClamp(pct, 0.0, 1.0);
        TimeValue duration = GetMovieDuration((Movie)_movie);
        
        SetMovieTimeValue((Movie)_movie, (float)duration * pct);
    }
}

void ofxHapPlayer::setVolume(float volume)
{
    if (_movie)
    {
        SetMovieVolume((Movie)_movie, (short) (volume * 256));
    }
}

void ofxHapPlayer::setFrame(int frame)
{
    if (_movie)
    {
        TimeValue time = _lastKnownFrameTime;
        int search = _lastKnownFrameNumber;
        OSType mediaType = VideoMediaType;
        Fixed searchDirection = search > frame ? ofxHapPlayerFloatToFixed(-1.0) : ofxHapPlayerFloatToFixed(1.0);
        int searchIncrement = search > frame ? -1 : 1;
        while (search != frame && time != -1)
        {
            TimeValue nextTime = 0;
            GetMovieNextInterestingTime((Movie)_movie,
                                        nextTimeMediaSample,
                                        1,
                                        &mediaType,
                                        time,
                                        searchDirection,
                                        &nextTime,
                                        NULL);
            if (nextTime == -1)
            {
                break;
            }
            time = nextTime;
            search += searchIncrement;
            _lastKnownFrameTime = time;
            _lastKnownFrameNumber = search;
        }
        SetMovieTimeValue((Movie)_movie, time);
    }
}

int ofxHapPlayer::getCurrentFrame()
{
    int frameNumber = 0;
    if (_movie)
    {
        TimeValue now = GetMovieTime((Movie)_movie, NULL);
        OSType mediaType = VideoMediaType;
        /*
         Find the actual time of the current frame
         */
        GetMovieNextInterestingTime((Movie)_movie,
                                    nextTimeMediaSample | nextTimeEdgeOK,
                                    1,
                                    &mediaType,
                                    now,
                                    (_speed > 0.0 ? ofxHapPlayerFloatToFixed(-1.0) : ofxHapPlayerFloatToFixed(1.0)),
                                    &now,
                                    NULL);
        /*
         Step through frames until we get to the current frame
         */
        TimeValue searched = _lastKnownFrameTime;
        frameNumber = _lastKnownFrameNumber;
        Fixed searchDirection = searched > now ? ofxHapPlayerFloatToFixed(-1.0) : ofxHapPlayerFloatToFixed(1.0);
        int searchIncrement = searched > now ? -1 : 1;
        while (searched != now && searched != -1)
        {
            GetMovieNextInterestingTime((Movie)_movie,
                                        nextTimeMediaSample,
                                        1,
                                        &mediaType,
                                        searched,
                                        searchDirection,
                                        &searched,
                                        NULL);
            if (searched != -1)
            {
                frameNumber += searchIncrement;
            }
        }
    }
    return frameNumber;
}

int ofxHapPlayer::getTotalNumFrames()
{
    int frameCount = 0;
    if (_movie)
    {
        if (_totalNumFrames == -1)
        {
            TimeValue searched = _lastKnownFrameTime;
            frameCount = _lastKnownFrameNumber;
            OSType mediaType = VideoMediaType;
            while (searched != -1)
            {
                GetMovieNextInterestingTime((Movie)_movie,
                                            nextTimeMediaSample,
                                            1,
                                            &mediaType,
                                            searched,
                                            ofxHapPlayerFloatToFixed(1.0),
                                            &searched,
                                            NULL);
                frameCount++;
            }
            _totalNumFrames = frameCount;
        }
        else
        {
            frameCount = _totalNumFrames;
        }
    }
    return frameCount;
}
#if defined(TARGET_OSX)
#pragma GCC pop
#endif
