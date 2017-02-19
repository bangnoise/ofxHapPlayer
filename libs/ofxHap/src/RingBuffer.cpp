/*
 RingBuffer.cpp
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

#include "RingBuffer.h"
#include <algorithm>

ofxHap::RingBuffer::RingBuffer(int channels, int samplesPerChannel)
:   _readStart(0), _writeStart(0),
    _buffer(channels * (samplesPerChannel + 1)), // maintain an empty slot to distinguish between empty and full state
    _channels(channels), _samples(samplesPerChannel)
{

}

int ofxHap::RingBuffer::getSamplesPerChannel() const
{
    return _samples;
}

void ofxHap::RingBuffer::writeBegin(float *&first, int &firstCount, float *&second, int &secondCount)
{
    int writeStart = _writeStart;
    int readStart = _readStart;

    int writable = _samples - (writeStart - readStart);

    int writePosition = writeStart % (_samples + 1);

    first = &_buffer[writePosition * _channels];

    firstCount = std::min(writable, (_samples + 1) - writePosition);

    second = &_buffer[0];
    secondCount = writable - firstCount;
}

void ofxHap::RingBuffer::writeEnd(int numSamples)
{
    _writeStart += numSamples;
}

void ofxHap::RingBuffer::readBegin(const float *&first, int &firstCount, const float *&second, int &secondCount)
{
    int writeStart = _writeStart;
    int readStart = _readStart;

    int readable = writeStart - readStart;

    int readPosition = readStart % (_samples + 1);

    first = &_buffer[readPosition * _channels];

    firstCount = std::min(readable, (_samples + 1) - readPosition);

    second = &_buffer[0];

    secondCount = readable - firstCount;
}

void ofxHap::RingBuffer::readEnd(int numSamples)
{
    _readStart += numSamples;
}
