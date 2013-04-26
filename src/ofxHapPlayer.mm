/*
 ofxHapPlayer.c
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

#include "ofxHapPlayer.h"
#include "Poco/String.h"
#import "HapMovieRenderer.h"

const string mofxHapPlayerVertexShader = "void main(void)\
{\
gl_Position = ftransform();\
gl_TexCoord[0] = gl_MultiTexCoord0;\
}";

const string mofxHapPlayerFragmentShader = "uniform sampler2D cocgsy_src;\
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

bool ofxHapPlayer::loadMovie(string movieFilePath, ofQTKitDecodeMode mode) {
    /*
     Most of this is lifted verbatim from ofxQTKitPlayer
     */
	if(mode != OF_QTKIT_DECODE_PIXELS_ONLY && mode != OF_QTKIT_DECODE_TEXTURE_ONLY && mode != OF_QTKIT_DECODE_PIXELS_AND_TEXTURE){
		ofLogError("ofxHapPlayer") << "Invalid ofQTKitDecodeMode mode specified while loading movie.";
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
		ofLogError("ofxHapPlayer") << "Loading file " << movieFilePath << " failed.";
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
		data.tex_h = ((HapMovieRenderer *)moviePlayer).textureHeight;
        if (data.textureTarget == GL_TEXTURE_2D)
        {
            data.tex_t = data.width / data.tex_w;
            data.tex_u = data.height / data.tex_h;
        }
        else
        {
            data.tex_t = data.width;
            data.tex_u = data.height;
        }
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
            bool success = shader.setupShaderFromSource(GL_VERTEX_SHADER, mofxHapPlayerVertexShader);
            if (success)
            {
                success = shader.setupShaderFromSource(GL_FRAGMENT_SHADER, mofxHapPlayerFragmentShader);
            }
            if (success)
            {
                success = shader.linkProgram();
            }
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
