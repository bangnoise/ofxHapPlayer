//
//  ofxHapPlayer.h
//
//  Created by Tom Butterworth on 24/04/2013.
//
//

#ifndef __ofxHapPlayer__
#define __ofxHapPlayer__

#include "ofMain.h"

class ofxHapPlayer : public ofQTKitPlayer {
public:
    bool    loadMovie(string path, ofQTKitDecodeMode mode);
};

#endif /* defined(__ofxHapPlayer__) */
