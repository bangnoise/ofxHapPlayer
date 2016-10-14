/*
 RingBuffer.h
 ofxHapPlayer

 Copyright (c) 2016, Tom Butterworth. All rights reserved.
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


#ifndef RingBuffer_h
#define RingBuffer_h

#include <atomic>
#include <vector>

namespace ofxHap {
    class RingBuffer {
    public:
        /*
         A lock-free ring buffer for interleaved float audio samples
         */
        RingBuffer(int channels, int samplesPerChannel);
        int getSamplesPerChannel() const;
        // On return first and second are pointers to positions to write samples to
        // firstCount and secondCount are the size, in samples per channel, of each buffer
        void writeBegin(float * &first, int &firstCount, float * &second, int &secondCount);
        // numSamples is the number of samples per channel actually written
        void writeEnd(int numSamples);
        // On return first and second are pointers to positions to read samples from
        // firstCount and secondCount are the size, in samples per channel, of each buffer
        void readBegin(const float * &first, int &firstCount, const float * &second, int &secondCount);
        // numSamples is the number of samples per channel actually read
        void readEnd(int numSamples);
    private:
        std::atomic<int>    _readStart;
        std::atomic<int>    _writeStart;
        std::vector<float>  _buffer;
        int                 _channels;
        int                 _samples;
    };
}

#endif /* RingBuffer_h */
