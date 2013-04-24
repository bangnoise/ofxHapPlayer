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
