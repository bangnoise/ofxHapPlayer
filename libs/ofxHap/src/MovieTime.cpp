/*
 MovieTime.cpp
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

#include "MovieTime.h"
#include <cstdlib>
#include <algorithm>
#include <libavformat/avformat.h>

ofxHap::TimeRangeSequence ofxHap::MovieTime::flatten(ofxHap::TimeRangeSequence sequence)
{
    TimeRangeSequence flattened;
    while (sequence.size()) {
        TimeRange next = (*sequence.begin()).abs();
        flattened.add(next);
        sequence.remove(next);
    }
    return flattened;
}

ofxHap::TimeRange ofxHap::MovieTime::nextRange(const ofxHap::Clock& clock, int64_t absolute, int64_t limit)
{
    int64_t start = clock.getTimeAt(absolute);
    Clock::Direction direction = clock.getDirectionAt(absolute);
    if (direction == Clock::Direction::Backwards)
    {
        int64_t duration = std::min(start + 1, limit);
        return TimeRange(start, -duration);
    }
    else
    {
        int64_t duration = std::min(clock.period - start, limit);
        return TimeRange(start, duration);
    }
}

ofxHap::TimeRangeSequence ofxHap::MovieTime::nextRanges(const Clock& clock, int64_t absolute, int64_t duration)
{
    TimeRangeSequence result;
    while (duration > 0) {
        TimeRange next = nextRange(clock, absolute, duration);
        if (next.length == 0)
        {
            break;
        }
        else
        {
            duration -= std::abs(next.length);
            absolute += std::abs(next.length);
            result.add(next);
        }
    }
    return result;
}
