/*
 ofApp.cpp
 ofxHapPlayerExample
 
 Copyright (c) 2013, Tom Butterworth. All rights reserved.
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

#include "ofApp.h"

#define BarInset 20
#define BarHeight 40

//--------------------------------------------------------------
void ofApp::setup(){
    // Limit drawing to a sane rate
    ofSetVerticalSync(true);
    
    ofBackground(0);
    
    // Load a movie file
    load("movies/SampleHap.mov");
    
    player.setLoopState(OF_LOOP_NORMAL);

    inScrub = false;
}

//--------------------------------------------------------------
void ofApp::update(){
    // Show or hide the cursor and position bar
    if (ofGetSystemTimeMillis() - lastMovement < 3000)
    {
        drawBar = true;
    }
    else
    {
        drawBar = false;
    }
    ofRectangle window = ofGetWindowRect();
    if (!drawBar && window.inside(ofGetMouseX(), ofGetMouseY()))
    {
        ofHideCursor();
    }
    else
    {
        ofShowCursor();
    }
}

//--------------------------------------------------------------
void ofApp::draw(){
    if (player.isLoaded())
    {
        // Draw the video frame
        ofSetColor(255, 255, 255);
        ofRectangle videoRect(0, 0, player.getWidth(), player.getHeight());
        videoRect.scaleTo(ofGetWindowRect());
        player.draw(videoRect.x, videoRect.y, videoRect.width, videoRect.height);

        // Draw the position bar if appropriate
        if (drawBar)
        {
            ofNoFill();
            ofRectangle bar = getBarRectangle();
            ofSetColor(244, 66, 234);
            ofDrawRectangle(bar);
            ofFill();
            ofSetColor(244, 66, 234, 180);
            bar.width *= player.getPosition();
            ofDrawRectangle(bar);
        }
    }
    else
    {
        if (player.getError().length())
        {
            ofDrawBitmapStringHighlight(player.getError(), 20, 20);
        }
        else
        {
            ofDrawBitmapStringHighlight("Movie is loading...", 20, 20);
        }
    }
}

void ofApp::load(std::string movie)
{
    ofSetWindowTitle(ofFilePath::getBaseName(movie));
    player.load(movie);
    player.play();
    lastMovement = ofGetSystemTimeMillis();
}

//--------------------------------------------------------------
void ofApp::keyPressed(int key){
    if (key == ' ')
    {
        player.setPaused(!player.isPaused());
    }
    else if (key == OF_KEY_UP)
    {
        player.setVolume(player.getVolume() + 0.1);
    }
    else if (key == OF_KEY_DOWN)
    {
        player.setVolume(player.getVolume() - 0.1);
    }
}

//--------------------------------------------------------------
void ofApp::keyReleased(int key){

}

void ofApp::mouseEntered(int x, int y) {

}

void ofApp::mouseExited(int x, int y) {

}

//--------------------------------------------------------------
void ofApp::mouseMoved(int x, int y){
    if (ofGetWindowRect().inside(x, y))
    {
        lastMovement = ofGetSystemTimeMillis();
    }
}

//--------------------------------------------------------------
void ofApp::mouseDragged(int x, int y, int button){
    if (inScrub)
    {
        float position = static_cast<float>(x - BarInset) / getBarRectangle().width;
        position = std::max(0.0f, std::min(position, 1.0f));
        player.setPosition(position);
        lastMovement = ofGetSystemTimeMillis();
    }
}

//--------------------------------------------------------------
void ofApp::mousePressed(int x, int y, int button){
    ofRectangle bar = getBarRectangle();
    if (bar.inside(x, y))
    {
        inScrub = true;
        wasPaused = player.isPaused() || player.getIsMovieDone();
        player.setPaused(true);
        mouseDragged(x, y, button);
    }
    lastMovement = ofGetSystemTimeMillis();
}

//--------------------------------------------------------------
void ofApp::mouseReleased(int x, int y, int button){
    if (inScrub)
    {
        inScrub = false;
        player.setPaused(wasPaused);
    }
}

//--------------------------------------------------------------
void ofApp::windowResized(int w, int h){

}

//--------------------------------------------------------------
void ofApp::gotMessage(ofMessage msg){

}

//--------------------------------------------------------------
void ofApp::dragEvent(ofDragInfo dragInfo){
	vector< string > fileList = dragInfo.files;
    load(fileList[0]);
}

ofRectangle ofApp::getBarRectangle() const
{
    return ofRectangle(BarInset, ofGetWindowHeight() - BarInset - BarHeight, ofGetWindowWidth() - (2 * BarInset), BarHeight);
}
