/*
 HapSupport.m
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

#include "HapSupport.h"

#if defined(__APPLE__)
#define HAP_BZERO(x,y) bzero((x),(y))
#else
#define WINVER 0x0600
#define _WIN32_WINNT 0x0600
#include <Windows.h>
#define HAP_BZERO(x,y) ZeroMemory((x),(y))
#endif

/*
 These are the four-character-codes used to designate the three Hap codecs
 */
#define kHapCodecSubType 'Hap1'
#define kHapAlphaCodecSubType 'Hap5'
#define kHapYCoCgCodecSubType 'HapY'

/*
 Much of QuickTime is deprecated in recent MacOS but no equivalent functionality exists in modern APIs,
 so we ignore these warnings.
 */
#if !defined(_MSC_VER)
#pragma GCC push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
#endif

/*
 Searches the list of installed codecs for a given codec
 */
static Boolean HapQTCodecIsAvailable(OSType codecType)
{
    CodecNameSpecListPtr list;
    OSStatus error;
    short i;
    
    error = GetCodecNameList(&list, 0);
    if (error) return false;
    
    for (i = 0; i < list->count; i++ )
    {        
        if (list->list[i].cType == codecType) return true;
    }
    
    return false;
}

static OSType HapQTGetQuickTimeMovieHapCodecSubType(Movie movie)
{
    if (movie)
    {
        long i;
        for (i = 1; i <= GetMovieTrackCount(movie); i++) {
            Track track = GetMovieIndTrack(movie, i);
            Media media = GetTrackMedia(track);
            OSType mediaType;
            GetMediaHandlerDescription(media, &mediaType, NULL, NULL);
            if (mediaType == VideoMediaType)
            {
                // Get the codec-type of this track
                OSType codecType;
                ImageDescriptionHandle imageDescription = (ImageDescriptionHandle)NewHandle(0); // GetMediaSampleDescription will resize it
                GetMediaSampleDescription(media, 1, (SampleDescriptionHandle)imageDescription);
                codecType = (*imageDescription)->cType;
                DisposeHandle((Handle)imageDescription);
				switch (codecType) {
				case kHapCodecSubType:
				case kHapAlphaCodecSubType:
				case kHapYCoCgCodecSubType:
					return codecType;
				default:
					break;
				}
            }
        }
    }
    return 0;
}
#if !defined(_MSC_VER)
#pragma GCC pop
#endif

Boolean HapQTQuickTimeMovieHasHapTrackPlayable(Movie movie)
{
	OSType codecType = HapQTGetQuickTimeMovieHapCodecSubType(movie);
	switch (codecType) {
	case kHapCodecSubType:
	case kHapAlphaCodecSubType:
	case kHapYCoCgCodecSubType:
		return HapQTCodecIsAvailable(codecType);
	default:
		return false;
	}
}

OSType HapQTGetQuickTimeMovieHapPixelFormat(Movie movie)
{
	OSType codecType = HapQTGetQuickTimeMovieHapCodecSubType(movie);
	switch (codecType) {
	case kHapCodecSubType:
		return kHapPixelFormatTypeRGB_DXT1;
	case kHapAlphaCodecSubType:
		return kHapPixelFormatTypeRGBA_DXT5;
	case kHapYCoCgCodecSubType:
		return kHapPixelFormatTypeYCoCg_DXT5;
	default:
		return 0;
	}
}

#ifdef __COREFOUNDATION__
/*
 Utility function, does what it says...
 */
static void HapQTAddNumberToDictionary( CFMutableDictionaryRef dictionary, CFStringRef key, SInt32 numberSInt32 )
{
    CFNumberRef number = CFNumberCreate( NULL, kCFNumberSInt32Type, &numberSInt32 );
    if( ! number )
        return;
    CFDictionaryAddValue( dictionary, key, number );
    CFRelease( number );
}
#endif

/*
 Register DXT pixel formats. The codec does this too, but it may not be loaded yet.
 */
static void HapQTRegisterDXTPixelFormat(OSType fmt, short bits_per_pixel, SInt32 open_gl_internal_format, Boolean has_alpha)
{
    /*
     * See http://developer.apple.com/legacy/mac/library/#qa/qa1401/_index.html
     */
    
    ICMPixelFormatInfo pixelInfo;
#ifdef __COREFOUNDATION__
    CFMutableDictionaryRef dict = CFDictionaryCreateMutable(kCFAllocatorDefault,
                                                            0,
                                                            &kCFTypeDictionaryKeyCallBacks,
                                                            &kCFTypeDictionaryValueCallBacks);
#endif
    HAP_BZERO(&pixelInfo, sizeof(pixelInfo));
    pixelInfo.size  = sizeof(ICMPixelFormatInfo);
    pixelInfo.formatFlags = (has_alpha ? kICMPixelFormatHasAlphaChannel : 0);
    pixelInfo.bitsPerPixel[0] = bits_per_pixel;
#ifdef __COREFOUNDATION__
	// This should really be a proper check for QuickTime 6.5+
    pixelInfo.cmpCount = 4;
    pixelInfo.cmpSize = bits_per_pixel / 4;
#endif
    // Ignore any errors here as this could be a duplicate registration
    ICMSetPixelFormatInfo(fmt, &pixelInfo);
#ifdef __COREFOUNDATION__
    HapQTAddNumberToDictionary(dict, kCVPixelFormatConstant, fmt);
    
    // CV has a bug where it disregards kCVPixelFormatBlockHeight, so we lie about block size
    // (4x1 instead of actual 4x4) and add a vertical block-alignment key instead
    HapQTAddNumberToDictionary(dict, kCVPixelFormatBitsPerBlock, bits_per_pixel * 4);
    HapQTAddNumberToDictionary(dict, kCVPixelFormatBlockWidth, 4);
    HapQTAddNumberToDictionary(dict, kCVPixelFormatBlockVerticalAlignment, 4);
    
    HapQTAddNumberToDictionary(dict, kCVPixelFormatOpenGLInternalFormat, open_gl_internal_format);
        
    // kCVPixelFormatContainsAlpha is only defined in the SDK for 10.7 plus
    CFDictionarySetValue(dict, CFSTR("ContainsAlpha"), (has_alpha ? kCFBooleanTrue : kCFBooleanFalse));
    
    CVPixelFormatDescriptionRegisterDescriptionWithPixelFormatType(dict, fmt);
    CFRelease(dict);
#endif
}

void HapQTRegisterPixelFormats(void)
{
    static Boolean registered = false;
    if (!registered)
    {
        // Register our DXT pixel buffer types if they're not already registered
        // arguments are: OSType, OpenGL internalFormat, alpha
        HapQTRegisterDXTPixelFormat(kHapPixelFormatTypeRGB_DXT1, 4, 0x83F0, false);
        HapQTRegisterDXTPixelFormat(kHapPixelFormatTypeRGBA_DXT5, 8, 0x83F3, true);
        HapQTRegisterDXTPixelFormat(kHapPixelFormatTypeYCoCg_DXT5, 8, 0x83F3, false);
        registered = true;
    }
}

#ifdef __COREFOUNDATION__
CFDictionaryRef HapQTCreateCVPixelBufferOptionsDictionary()
{
    // The pixel formats we want.
    SInt32 rgb_dxt1 = kHapPixelFormatTypeRGB_DXT1;
    SInt32 rgba_dxt5 = kHapPixelFormatTypeRGBA_DXT5;
    SInt32 ycocg_dxt5 = kHapPixelFormatTypeYCoCg_DXT5;
    
    const void *formatNumbers[3];
    
    CFDictionaryRef dictionary = NULL;
    
    // The pixel formats must be registered before requesting them for a QTPixelBufferContext.
    // The codec does this but it is possible it may not be loaded yet.
    HapQTRegisterPixelFormats();
    
    formatNumbers[0] = CFNumberCreate(kCFAllocatorDefault, kCFNumberSInt32Type, &rgb_dxt1);
    formatNumbers[1] = CFNumberCreate(kCFAllocatorDefault, kCFNumberSInt32Type, &rgba_dxt5);
    formatNumbers[2] = CFNumberCreate(kCFAllocatorDefault, kCFNumberSInt32Type, &ycocg_dxt5);
    
    if (formatNumbers[0] && formatNumbers[1] && formatNumbers[2])
    {
        CFArrayRef formats = CFArrayCreate(kCFAllocatorDefault, formatNumbers, 3, &kCFTypeArrayCallBacks);
        if (formats)
        {
            const void *keys[1] = { kCVPixelBufferPixelFormatTypeKey };
            const void *values[1] = { formats };
            
            dictionary = CFDictionaryCreate(kCFAllocatorDefault, keys, values, 1, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
            
            CFRelease(formats);
        }
    }
    if (formatNumbers[0]) CFRelease(formatNumbers[0]);
    if (formatNumbers[1]) CFRelease(formatNumbers[1]);
    if (formatNumbers[2]) CFRelease(formatNumbers[2]);
    
    return dictionary;
}
#endif