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
    bool        loadMovie(string path, ofQTKitDecodeMode mode);
    ofTexture * getTexture();
    ofShader *  getShader();
    void        draw(float x, float y);
    void        draw(float x, float y, float w, float h);
protected:
    void        updateTexture();
    ofShader    shader;
    bool        shaderLoaded;
};

#endif /* defined(__ofxHapPlayer__) */
