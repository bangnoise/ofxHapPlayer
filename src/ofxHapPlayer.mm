//
//  ofxHapPlayer.cpp
//
//  Created by Tom Butterworth on 24/04/2013.
//
//

#include "ofxHapPlayer.h"
#include "Poco/String.h"
#import "HapMovieRenderer.h"

bool ofxHapPlayer::loadMovie(string movieFilePath, ofQTKitDecodeMode mode) {
    /*
     Most of this is lifted verbatim from ofxQTKitPlayer
     */
	if(mode != OF_QTKIT_DECODE_PIXELS_ONLY && mode != OF_QTKIT_DECODE_TEXTURE_ONLY && mode != OF_QTKIT_DECODE_PIXELS_AND_TEXTURE){
		ofLogError("ofQTKitPlayer") << "Invalid ofQTKitDecodeMode mode specified while loading movie.";
		return false;
	}
	
	if(isLoaded()){
		close(); //auto released
	}
    
	NSAutoreleasePool* pool = [[NSAutoreleasePool alloc] init];
    
    shaderLoaded = false;
    decodeMode = mode;
	bool useTexture = (mode == OF_QTKIT_DECODE_TEXTURE_ONLY || mode == OF_QTKIT_DECODE_PIXELS_AND_TEXTURE);
	bool usePixels  = (mode == OF_QTKIT_DECODE_PIXELS_ONLY  || mode == OF_QTKIT_DECODE_PIXELS_AND_TEXTURE);
	bool useAlpha = (pixelFormat == OF_PIXELS_RGBA);
    
    bool isURL = false;
	
    if (Poco::icompare(movieFilePath.substr(0,7), "http://")  == 0 ||
        Poco::icompare(movieFilePath.substr(0,8), "https://") == 0 ||
        Poco::icompare(movieFilePath.substr(0,7), "rtsp://")  == 0) {
        isURL = true;
    }
    else {
        movieFilePath = ofToDataPath(movieFilePath, false);
    }
    
    /*
     Instead of a QTKitMovieRenderer we use our custom subclass
     */
	moviePlayer = [[HapMovieRenderer alloc] init];
	BOOL success = [moviePlayer loadMovie:[NSString stringWithCString:movieFilePath.c_str() encoding:NSUTF8StringEncoding]
								pathIsURL:isURL
							 allowTexture:useTexture
							  allowPixels:usePixels
                               allowAlpha:useAlpha];
	
	if(success){
		moviePlayer.synchronousSeek = bSynchronousSeek;
        reallocatePixels();
        moviePath = movieFilePath;
		duration = moviePlayer.duration;
        
        setLoopState(currentLoopState);
        setSpeed(1.0f);
		firstFrame(); //will load the first frame
	}
	else {
		ofLogError("ofQTKitPlayer") << "Loading file " << movieFilePath << " failed.";
		[moviePlayer release];
		moviePlayer = NULL;
	}
	
	[pool release];
	
	return success;
}

/*
 We override updateTexture() because our 2D textures for Hap have dimensions beyond
 the image extent.
 */
void ofxHapPlayer::updateTexture(){
	if(moviePlayer.textureAllocated){
        
		tex.setUseExternalTextureID(moviePlayer.textureID);
		
		ofTextureData& data = tex.getTextureData();
		data.textureTarget = moviePlayer.textureTarget;
		data.width = getWidth();
		data.height = getHeight();
		data.tex_w = ((HapMovieRenderer *)moviePlayer).textureWidth;
		data.tex_h = ((HapMovieRenderer *)moviePlayer).textureWidth;
		data.tex_t = data.width / data.tex_w;
		data.tex_u = data.height / data.tex_h;
	}
}

/*
 Unfortunately ofQTKitPlayer doesn't declare these functions virtual so we have to override everything that calls updateTexture()
 */
ofTexture* ofxHapPlayer::getTexture() {
    ofTexture* texPtr = NULL;
	if(moviePlayer.textureAllocated){
		updateTexture();
        return &tex;
	} else {
        return NULL;
    }
}

ofShader *ofxHapPlayer::getShader()
{
    if (moviePlayer.textureAllocated && ((HapMovieRenderer *)moviePlayer).textureWantsShader)
    {
        if (shaderLoaded == false)
        {
            /*
             Is there a better way to find the addons folder?
             */
            string path = ofFilePath::getCurrentWorkingDirectory();
            path += "/../../../../../../../addons/ofxHapPlayer/src/ScaledCoCgYToRGBA";
            bool success = shader.load(path);
            if (success)
            {
                shaderLoaded = true;
            }
        }
        if (shaderLoaded) return &shader;
    }
    return NULL;
}

void ofxHapPlayer::draw(float x, float y) {
	draw(x,y, moviePlayer.movieSize.width, moviePlayer.movieSize.height);
}

void ofxHapPlayer::draw(float x, float y, float w, float h) {
	updateTexture();
    ofShader *sh = getShader();
    if (sh)
    {
        sh->begin();
    }
	tex.draw(x,y,w,h);
    if (sh)
    {
        sh->end();
    }
}
