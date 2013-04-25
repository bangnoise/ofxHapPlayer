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

}