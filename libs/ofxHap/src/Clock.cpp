/*
 Clock.cpp
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

#include "Clock.h"

namespace ofxHap {
    static int64_t clockMod(int64_t k, int64_t n)
    {
        return ((k %= n) < 0) ? k+n : k;
    }
}

ofxHap::Clock::Clock() : period(0), mode(Mode::Loop), _rate(1.0), _start(0), _time(-1), _paused(false)
{

}

void ofxHap::Clock::syncAt(int64_t pos, int64_t t)
{
    _start = t - (pos / _rate);
    _time = pos;
    _wall = t;
}

int64_t ofxHap::Clock::getTime() const
{
    return _time;
}

int64_t ofxHap::Clock::getTimeAt(int64_t t) const
{
    t = (t - _start) * _rate;

    if (_paused)
    {
        return _time;
    }
    else if (mode == Mode::Once)
    {
        if (t > period)
        {
            return period;
        }
        else if (t < 0)
        {
            // Once backwards
            if (t < -period)
            {
                return 0;
            }
            else
            {
                period + t;
            }
        }
        else
        {
            return t;
        }
    }
    else if (period == 0)
    {
        return 0;
    }
    else if (mode == Mode::Palindrome && clockMod((t / period), 2) == 1)
    {
        return period - clockMod(t, period);
    }
    else
    {
        return clockMod(t, period);
    }
}

int64_t ofxHap::Clock::setTimeAt(int64_t t)
{
    _wall = t;
    return _time = getTimeAt(t);
}

void ofxHap::Clock::setPausedAt(bool p, int64_t t)
{
    if (_paused != p)
    {
        if (p)
        {
            setTimeAt(t);
        }
        else
        {
            syncAt(_time, t);
        }
        _paused = p;
    }
}

bool ofxHap::Clock::getPaused() const
{
    return _paused;
}

int ofxHap::Clock::getDirection() const
{
    return getDirectionAt(_wall);
}

int ofxHap::Clock::getDirectionAt(int64_t t) const
{
    if (_paused)
    {
        t = _time;
    }
    else
    {
        t = static_cast<int64_t>((t - _start) * _rate);
    }
    if (period == 0)
    {
        return 1;
    }
    else if (mode == Mode::Palindrome && clockMod((t / period), 2) == 1)
    {
        if (_rate > 0)
        {
            return -1;
        }
        else
        {
            return 1;
        }
    }
    else
    {
        if (_rate > 0)
        {
            return 1;
        }
        else
        {
            return -1;
        }
    }
}

float ofxHap::Clock::getRate() const
{
    return _rate;
}

void ofxHap::Clock::setRate(float r)
{
    _rate = r;
    syncAt(_time, _wall);
}
