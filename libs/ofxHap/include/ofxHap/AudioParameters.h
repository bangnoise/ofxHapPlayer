/*
 AudioParameters.h
 ofxHapPlayer

 Copyright (c) 2017, Tom Butterworth. All rights reserved.
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

#ifndef AudioParameters_h
#define AudioParameters_h

#include <cstdint>
#include "Common.h"

#if OFX_HAP_HAS_CODECPAR
typedef struct AVCodecParameters AVCodecParameters;
#else
typedef struct AVCodecContext AVCodecContext;
#endif

namespace ofxHap {

class AudioParameters
{
public:
    /*
     Audio params - from stream, plus length to cache either side of now in usec
     */
#if OFX_HAP_HAS_CODECPAR
	AudioParameters(AVCodecParameters *p, int cache, int64_t start, int64_t duration);
	AVCodecParameters* parameters;
#else
	AudioParameters(AVCodecContext *c, int cache, int64_t start, int64_t duration);
	AVCodecContext *context;
#endif
	~AudioParameters();
	AudioParameters(const AudioParameters& o);
	AudioParameters& operator=(const AudioParameters& o);
    int cache;
    int64_t start;
    int64_t duration;
};

}

#endif
