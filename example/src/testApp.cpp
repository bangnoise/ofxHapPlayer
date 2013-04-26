/*
 testApp.cpp
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

#include "testApp.h"

//--------------------------------------------------------------
void testApp::setup(){
    // Don't let OF hammer the system with pointless never-seen screen updates
    ofSetVerticalSync(true);
    
    ofBackground(240, 240, 240);
    
    // Load a movie file
    player.loadMovie("movies/SampleHap.mov", OF_QTKIT_DECODE_TEXTURE_ONLY);
    
    // Start playback
    player.play();
}

//--------------------------------------------------------------
void testApp::update(){
    // Signal the player to update
    player.update();
}

//--------------------------------------------------------------
void testApp::draw(){
    if (player.isLoaded())
    {
        // Draw the frame
        ofSetColor(255, 255, 255);
        player.draw(20, 20);
    }
    ofSetColor(255,0,0);
    ofDrawBitmapString(ofToString(ofGetFrameRate()),20,20);
}

//--------------------------------------------------------------
void testApp::keyPressed(int key){

}

//--------------------------------------------------------------
void testApp::keyReleased(int key){

}

//--------------------------------------------------------------
void testApp::mouseMoved(int x, int y){

}

//--------------------------------------------------------------
void testApp::mouseDragged(int x, int y, int button){

}

//--------------------------------------------------------------
void testApp::mousePressed(int x, int y, int button){

}

//--------------------------------------------------------------
void testApp::mouseReleased(int x, int y, int button){

}

//--------------------------------------------------------------
void testApp::windowResized(int w, int h){

}

//--------------------------------------------------------------
void testApp::gotMessage(ofMessage msg){

}

//--------------------------------------------------------------
void testApp::dragEvent(ofDragInfo dragInfo){ 
	vector< string > fileList = dragInfo.files;
    cout << fileList[0] << endl;
    player.loadMovie(fileList[0], OF_QTKIT_DECODE_TEXTURE_ONLY);
}