#include "testApp.h"

//--------------------------------------------------------------
void testApp::setup(){
    ofBackground(255, 255, 255);
    
    player.setPixelFormat(OF_PIXELS_RGBA);
    
    player.loadMovie("movies/SampleHap.mov", OF_QTKIT_DECODE_TEXTURE_ONLY);
    
    player.play();
}

//--------------------------------------------------------------
void testApp::update(){
    player.update();
}

//--------------------------------------------------------------
void testApp::draw(){
    
    ofSetColor(255, 255, 255);
    
    if (player.isLoaded())
    {
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