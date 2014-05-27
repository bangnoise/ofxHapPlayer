/*
 HapSupport.h
 Hap QuickTime Playback
 
 Copyright (c) 2012-2013, Tom Butterworth and Vidvox LLC. All rights reserved.
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

#ifndef QTMultiGPUTextureIssue_HapSupport_h
#define QTMultiGPUTextureIssue_HapSupport_h

#if defined(__APPLE__)
#include <CoreFoundation/CoreFoundation.h>
#include <QuickTime/QuickTime.h>
#elif defined(_WIN32)
#include <QTML.h>
#include <Movies.h>
#endif

#if __LP64__

#error Hap QuickTime support requires 32-bit QuickTime APIs but this target is 64-bit

#else

#ifdef __cplusplus
extern "C" {
#endif

/**
 The four-character-codes used to describe the pixel-formats of DXT frames emitted by the Hap QuickTime codec.
 */
#define kHapPixelFormatTypeRGB_DXT1 'DXt1'
#define kHapPixelFormatTypeRGBA_DXT5 'DXT5'
#define kHapPixelFormatTypeYCoCg_DXT5 'DYt5'

/**
 Returns true if any track of movie is a Hap track and the codec is installed to handle it, otherwise false.
 */
Boolean HapQTQuickTimeMovieHasHapTrackPlayable(Movie movie);

/**
 Returns the four-character-code for pixel-format of the first Hap track in a movie, or 0 if there are no Hap tracks.
*/
OSType HapQTGetQuickTimeMovieHapPixelFormat(Movie movie);

#ifdef __OBJC__
/**
 Returns YES if any track of movie is a Hap track and the codec is installed to handle it, otherwise NO.
 */
#define HapQTMovieHasHapTrackPlayable(m) ((BOOL)HapQTQuickTimeMovieHasHapTrackPlayable([(m) quickTimeMovie]))
#endif

#ifdef __COREFOUNDATION__
/**
 Returns a dictionary suitable to pass with the kQTVisualContextPixelBufferAttributesKey in an options dictionary when
 creating a CVPixelBufferContext.
 */
CFDictionaryRef HapQTCreateCVPixelBufferOptionsDictionary();
#endif

/**
 If you use HapQTCreateCVPixelBufferOptionsDictionary() this is called for you and you needn't call it directly yourself
*/
void HapQTRegisterPixelFormats(void);

#ifdef __cplusplus
}
#endif

#endif

#endif
