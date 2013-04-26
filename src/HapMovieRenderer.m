/*
 HapMovieRenderer.m
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

#import "HapMovieRenderer.h"
#import "HapOFPixelBufferTexture.h"
#include "HapSupport.h"

@interface QTMovie (QTFrom763)
- (QTTime)frameStartTime: (QTTime)atTime;
- (QTTime)frameEndTime: (QTTime)atTime;
- (QTTime)keyframeStartTime:(QTTime)atTime;
@end

/*
 This is static in QTKitMovieRenderer so we have to implement it ourselves
 */
//This method is called whenever a new frame comes in from the visual context
//it's called on the back thread so locking is performed in Renderer class
static void frameAvailable(QTVisualContextRef _visualContext, const CVTimeStamp *frameTime, void *refCon)
{
    
	NSAutoreleasePool	*pool		= [[NSAutoreleasePool alloc] init];
	CVImageBufferRef	currentFrame;
	OSStatus			err;
	QTKitMovieRenderer		*renderer	= (QTKitMovieRenderer *)refCon;
	
	if ((err = QTVisualContextCopyImageForTime(_visualContext, NULL, frameTime, &currentFrame)) == kCVReturnSuccess) {
		[renderer frameAvailable:currentFrame];
	}
	else{
		[renderer frameFailed];
	}
	
	[pool release];
}

@implementation HapMovieRenderer
- (BOOL) loadMovie:(NSString*)moviePath pathIsURL:(BOOL)isURL allowTexture:(BOOL)doUseTexture allowPixels:(BOOL)doUsePixels allowAlpha:(BOOL)doUseAlpha
{
    /*
     Start with the superclass's implementation until we have loaded the movie
     */
    // if the path is local, make sure the file exists before proceeding
    if (!isURL && ![[NSFileManager defaultManager] fileExistsAtPath:moviePath])
    {
		NSLog(@"No movie file found at %@", moviePath);
		return NO;
	}
	
	//create visual context
	useTexture = doUseTexture;
	usePixels = doUsePixels;
	useAlpha = doUseAlpha;
    
    
    // build the movie URL
    NSString *movieURL;
    if (isURL) {
        movieURL = [NSURL URLWithString:moviePath];
    }
    else {
        movieURL = [NSURL fileURLWithPath:[moviePath stringByStandardizingPath]];
    }
    
	NSError* error;
	NSMutableDictionary* movieAttributes = [NSMutableDictionary dictionaryWithObjectsAndKeys:
                                            movieURL, QTMovieURLAttribute,
                                            [NSNumber numberWithBool:NO], QTMovieOpenAsyncOKAttribute,
                                            nil];
    
	_movie = [[QTMovie alloc] initWithAttributes:movieAttributes
										   error: &error];
	
	if(error || _movie == NULL){
		NSLog(@"Error Loading Movie: %@", error);
		return NO;
	}
    
    /*
     Now we have a movie, check if it is Hap
     */
    
    isHap = HapQTMovieHasHapTrackPlayable(_movie);
    
    if (isHap && doUseTexture && !doUsePixels)
    {
        /*
         We pick up the superclass's implementation again here to get the necessary information
         */
        lastMovieTime = QTMakeTime(0,1);
        movieSize = [[_movie attributeForKey:QTMovieNaturalSizeAttribute] sizeValue];
        //	NSLog(@"movie size %f %f", movieSize.width, movieSize.height);
        
        movieDuration = [_movie duration];
        
        [_movie gotoBeginning];
        
        [_movie gotoEnd];
        QTTime endTime = [_movie currentTime];
        
        [_movie gotoBeginning];
        QTTime curTime = [_movie currentTime];
        
        long numFrames = 0;
        NSMutableArray* timeValues = [NSMutableArray array];
        while(true) {
            //        % get the end time of the current frame
            [timeValues addObject:[NSNumber numberWithLongLong:curTime.timeValue]];
            
            curTime = [_movie frameEndTime:curTime];
            numFrames++;
            //        int time = curTime.timeValue;
            //        NSLog(@" num frames %ld, %lld/%ld , dif %lld, current time %f", numFrames,curTime.timeValue,curTime.timeScale, curTime.timeValue - time, 1.0*curTime.timeValue/curTime.timeScale);
            if (QTTimeCompare(curTime, endTime) == NSOrderedSame ||
                QTTimeCompare(curTime, [_movie frameEndTime:curTime])  == NSOrderedSame ){ //this will happen for audio files since they have no frames.
                break;
            }
        }
        
        if(frameTimeValues != NULL){
            [frameTimeValues release];
        }
        frameTimeValues = [[NSArray arrayWithArray:timeValues] retain];
        
        frameCount = numFrames;
        //	frameStep = round((double)(movieDuration.timeValue/(double)(numFrames)));
        //NSLog(@" movie has %d frames and frame step %d", frameCount, frameStep);
        
        /*
         Now we create a pixel-buffer context suitable for Hap playback
         */
        
        CFDictionaryRef pixelBufferAttributes = HapQTCreateCVPixelBufferOptionsDictionary();
        
        NSMutableDictionary *ctxAttributes = [NSMutableDictionary dictionaryWithObject:(NSDictionary *)pixelBufferAttributes
                                                                                forKey:(NSString*)kQTVisualContextPixelBufferAttributesKey];
        
        CFRelease(pixelBufferAttributes);
        
        /*
         Create our texture object
         */
        hapTexture = [[HapOFPixelBufferTexture alloc] initWithContext:CGLGetCurrentContext()];
        
        /*
         The rest is as done in the superclass...
         */
        OSStatus err = QTPixelBufferContextCreate(kCFAllocatorDefault, (CFDictionaryRef)ctxAttributes, &_visualContext);
        if(err){
            NSLog(@"error %ld creating OpenPixelBufferContext", err);
            return NO;
        }
        
        [_movie setVisualContext:_visualContext];
        
        QTVisualContextSetImageAvailableCallback(_visualContext, frameAvailable, self);
        synchronousSeekLock = [[NSCondition alloc] init];
        
        //borrowed from WebCore:
        // http://opensource.apple.com/source/WebCore/WebCore-1298/platform/graphics/win/QTMovie.cpp
        hasVideo = (NULL != GetMovieIndTrackType([_movie quickTimeMovie], 1, VisualMediaCharacteristic, movieTrackCharacteristic | movieTrackEnabledOnly));
        hasAudio = (NULL != GetMovieIndTrackType([_movie quickTimeMovie], 1, AudioMediaCharacteristic,  movieTrackCharacteristic | movieTrackEnabledOnly));
        //	NSLog(@"has video? %@ has audio? %@", (hasVideo ? @"YES" : @"NO"), (hasAudio ? @"YES" : @"NO") );
        loadedFirstFrame = NO;
        self.volume = 1.0;
        self.loops = YES;
        self.palindrome = NO;
        
        return YES;
    }
    else
    {
        /*
         If the movie isn't Hap, we release it and call the superclass's implementation to deal with it
         */
        [_movie release];
        _movie = nil;
        return [super loadMovie:moviePath pathIsURL:isURL allowTexture:doUseTexture allowPixels:doUsePixels allowAlpha:doUseAlpha];
    }
}

- (void)frameAvailable:(CVImageBufferRef)image
{
    /*
     Call super's implementation
     */
    [super frameAvailable:image];
    textureNeedsUpdate = YES;
}

- (void)updateHapTextureIfNeeded
{
    if (isHap && textureNeedsUpdate)
    {
        CVPixelBufferRef buffer = NULL;
        // The buffer is updated from the visual context's thread
        // so we retain it using the same lock as the superclass uses
        // to ensure it isn't released while we're updating the texture
        @synchronized(self) {
            buffer = CVBufferRetain(_latestTextureFrame);
        }
        hapTexture.buffer = buffer;
        CVBufferRelease(buffer);
        textureNeedsUpdate = NO;
    }
}

- (GLuint) textureID
{
    if (isHap)
    {
        [self updateHapTextureIfNeeded];
        return hapTexture.textureName;
    }
    else return [super textureID];
}

- (GLenum) textureTarget
{
    if (isHap)
    {
        [self updateHapTextureIfNeeded];
        return GL_TEXTURE_2D;
    }
    else return [super textureTarget];
}

- (GLuint)textureWidth
{
    if (isHap)
    {
        [self updateHapTextureIfNeeded];
        return hapTexture.textureWidth;
    }
    else return self.movieSize.width;
}

- (GLuint)textureHeight
{
    if (isHap)
    {
        [self updateHapTextureIfNeeded];
        return hapTexture.textureHeight;
    }
    else return self.movieSize.height;
}

- (void) bindTexture
{
	// Not used by OF, we just override to stop the superclass's implementation being called
}

- (void) unbindTexture
{
	// Not used by OF, we just override to stop the superclass's implementation being called
}

- (BOOL)textureWantsShader
{
    if (isHap)
    {
        [self updateHapTextureIfNeeded];
        return hapTexture.textureIsYCoCg;
    }
    else return NO;
}

@end
