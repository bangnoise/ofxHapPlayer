/*
 TimeRangeSet.h
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

#ifndef TimeRangeSet_h
#define TimeRangeSet_h

#include <cstdint>
#include <list>
#include <cstddef>

namespace ofxHap {
    class TimeRangeSet {
    public:
        class TimeRange {
        public:
            TimeRange(int64_t start, int64_t length);
            int64_t latest() const;
            bool intersects(const TimeRange& o) const;
            bool includes(int64_t t) const;
            TimeRange intersection(const TimeRange& o) const;
            int64_t start;
            int64_t length;
        };
        int64_t earliest() const;
        int64_t latest() const;
        bool includes(int64_t t) const;
        void add(int64_t start, int64_t length);
        void add(const TimeRange& range);
        void remove(int64_t start, int64_t length);
        void remove(const TimeRange& range);
        void remove(const TimeRangeSet& o);
        TimeRangeSet intersection(const TimeRangeSet& o) const;
        void clear();
        size_t size() const { // TODO: could probably delete
            return _ranges.size();
        }
        std::list<TimeRange>::const_iterator begin() const {
            return _ranges.begin();
        }
        std::list<TimeRange>::const_iterator end() const {
            return _ranges.end();
        }
    private:
        std::list<TimeRange> _ranges;
    };
}

#endif /* TimeRangeSet_h */
