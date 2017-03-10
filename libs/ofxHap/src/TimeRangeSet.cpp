/*
 TimeRangeSet.cpp
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

#include "TimeRangeSet.h"
#include <algorithm>
#include <cstdlib>

ofxHap::TimeRangeSet::TimeRangeSet()
{

}

ofxHap::TimeRangeSet::TimeRangeSet(const TimeRangeSequence& seq)
{
    for (const auto& range : seq)
    {
        add(range);
    }
}

int64_t ofxHap::TimeRangeSet::earliest() const
{
    return _ranges.begin()->start;
}

int64_t ofxHap::TimeRangeSet::latest() const
{
    return _ranges.back().latest();
}

bool ofxHap::TimeRangeSet::includes(int64_t t) const
{
    for (const auto& range : _ranges)
    {
        if (range.includes(t))
            return true;
    }
    return false;
}

void ofxHap::TimeRangeSet::add(int64_t start, int64_t length)
{
    add(TimeRange(start, length));
}

void ofxHap::TimeRangeSet::add(const ofxHap::TimeRange &r)
{
    TimeRange range = r.abs();
    if (range.length)
    {
        for (auto itr = _ranges.begin(); itr != _ranges.end(); ++itr)
        {
            if (itr->intersects(range))
            {
                int64_t end = std::max(itr->latest(), range.latest());
                itr->start = std::min(itr->start, range.start);
                itr->length = end - itr->start + 1;
                return;
            }
            else if (itr->latest() == range.start - 1)
            {
                itr->length += range.length;
                return;
            }
            else if (itr->start == range.latest() + 1)
            {
                itr->start = range.start;
                itr->length += range.length;
                return;
            }
            else if (itr->latest() > range.start)
            {
                _ranges.insert(itr, range);
                return;
            }
        }
        _ranges.emplace_back(range);
    }
}

void ofxHap::TimeRangeSet::remove(int64_t start, int64_t length)
{
    remove(TimeRange(start, length));
}

void ofxHap::TimeRangeSet::remove(const ofxHap::TimeRange &range)
{
    if (range.length)
    {
        for (auto itr = _ranges.begin(); itr != _ranges.end();) {
            if (itr->intersects(range))
            {
                if (itr->latest() > range.latest())
                {
                    TimeRange remainder(range.latest() + 1, itr->latest() - range.latest());
                    itr->length -= remainder.length;
                    _ranges.insert(itr, remainder);
                }
                if (itr->start < range.start)
                {
                    itr->length = range.start - itr->start;
                }
                if (itr->start >= range.start && itr->latest() <= range.latest())
                {
                    itr = _ranges.erase(itr);
                }
                else
                {
                    ++itr;
                }
            }
            else if (itr->latest() > range.start)
            {
                return;
            }
            else
            {
                ++itr;
            }
        }
    }
}

void ofxHap::TimeRangeSet::remove(const ofxHap::TimeRangeSet &o)
{
    for (const TimeRange& range : o)
    {
        remove(range);
    }
}

ofxHap::TimeRangeSet ofxHap::TimeRangeSet::intersection(const ofxHap::TimeRangeSet &o) const
{
    TimeRangeSet result;
    for (const TimeRange& orange : o)
    {
        for (const TimeRange& range : _ranges)
        {
            result.add(range.intersection(orange));
        }
    }
    return result;
}

ofxHap::TimeRangeSet ofxHap::TimeRangeSet::intersection(const TimeRangeSequence& o) const
{
    TimeRangeSet result;
    for (const TimeRange& orange : o)
    {
        for (const TimeRange& range : _ranges)
        {
            result.add(range.intersection(orange));
        }
    }
    return result;
}

void ofxHap::TimeRangeSet::clear()
{
    _ranges.clear();
}

ofxHap::TimeRange::TimeRange(int64_t s, int64_t l)
: start(s), length(l) {}

ofxHap::TimeRange ofxHap::TimeRange::abs() const
{
    return TimeRange(earliest(), std::abs(length));
}

int64_t ofxHap::TimeRange::earliest() const
{
    return length < 0 ? start + length + 1 : start;
}

int64_t ofxHap::TimeRange::latest() const
{
    return length < 0 ? start : start + length - 1;
}

void ofxHap::TimeRange::setEarliest(int64_t e)
{
    if (e > latest())
    {
        start = e;
        length = 0;
    }
    else
    {
        if (length > 0)
        {
            length += start - e;
            start = e;
        }
        else if (length < 0)
        {
            length = - (start - e + 1);
        }
    }
}

void ofxHap::TimeRange::setLatest(int64_t l)
{
    if (l < earliest())
    {
        start = l;
        length = 0;
    }
    else
    {
        if (length > 0)
        {
            length = l - start + 1;
        }
        else
        {
            length -= l - start;
            start = l;
        }
    }
}

bool ofxHap::TimeRange::intersects(const ofxHap::TimeRange &o) const
{
    if (includes(o.earliest()) || o.includes(earliest()))
    {
        return true;
    }
    return false;
}

bool ofxHap::TimeRange::includes(int64_t t) const
{
    if (t >= earliest() && t <= latest())
    {
        return true;
    }
    return false;
}

ofxHap::TimeRange ofxHap::TimeRange::intersection(const ofxHap::TimeRange &o) const
{
    int64_t s = std::max(earliest(), o.earliest());
    int64_t e = std::min(latest(), o.latest());
    int64_t l = std::max(INT64_C(0), 1 + e - s);
    return TimeRange(s, l);
}

void ofxHap::TimeRangeSequence::add(const ofxHap::TimeRange &range)
{
    _ranges.push_back(range);
}

void ofxHap::TimeRangeSequence::remove(const ofxHap::TimeRange &range)
{
    if (range.length)
    {
        for (auto itr = _ranges.begin(); itr != _ranges.end();) {
            if (itr->intersects(range))
            {
                if (itr->earliest() >= range.earliest() && itr->latest() <= range.latest())
                {
                    // Is entirely within the range to be removed
                    itr = _ranges.erase(itr);
                }
                else
                {
                    if (itr->earliest() >= range.earliest() && itr->latest() > range.latest())
                    {
                        // Starts after or at the same time and ends after the range to be removed
                        // Move the start:
                        itr->setEarliest(range.latest() + 1);
                    }
                    else if (itr->includes(range.earliest()))
                    {
                        // Starts before the range to be removed
                        if (itr->latest() > range.latest())
                        {
                            // Ends after the range to be removed
                            // Split
                            TimeRange remainder(range.latest() + 1, itr->latest() - range.latest());
                            if (itr->length < 0)
                            {
                                remainder.start = remainder.latest();
                                remainder.length = -remainder.length;
                            }
                            _ranges.insert(itr, remainder);
                        }
                        // Shorten
                        itr->setLatest(range.earliest() - 1);
                    }
                    ++itr;
                }
            }
            else
            {
                ++itr;
            }
        }
    }
}

void ofxHap::TimeRangeSequence::remove(const ofxHap::TimeRangeSet &set)
{
    for (const TimeRange& range : set)
    {
        remove(range);
    }
}
