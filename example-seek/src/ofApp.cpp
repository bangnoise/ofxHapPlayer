#include "ofApp.h"

#define BarInset 20
#define BarHeight 40

void ofApp::setup() {
	ofSetVerticalSync(true);
	ofBackground(0);
	load("movies/test-pattern.hap.mov");
	player.setLoopState(OF_LOOP_NORMAL);
	inScrub = false;
}

void ofApp::update() {
	if (ofGetSystemTimeMillis() - lastMovement < 3000) {
		drawBar = true;
	} else {
		drawBar = false;
	}
	ofRectangle window = ofGetWindowRect();
	if (!drawBar && window.inside(ofGetMouseX(), ofGetMouseY())) {
		ofHideCursor();
	} else {
		ofShowCursor();
	}
}

void ofApp::draw() {
	if (player.isLoaded()) {
		ofSetColor(255, 255, 255);
		player.draw(0, 0, 640, 480);
		drawFrameNumber();
		drawHotkeyHelp();
		if (drawBar) {
			drawStatusBar();
		}
	} else {
		if (player.getError().length()) {
			ofDrawBitmapStringHighlight(player.getError(), 20, 20);
		} else {
			ofDrawBitmapStringHighlight("Movie is loading...", 20, 20);
		}
	}
}

void ofApp::drawFrameNumber() {
	int currentFrame = player.getCurrentFrame();

	ofDrawBitmapStringHighlight(
		ofToString(currentFrame),
		ofGetWidth() / 2 - 20,
		ofGetHeight() / 2,
		ofColor(0, 0, 0, 180),
		ofColor::white);
}

void ofApp::drawStatusBar() {
	int currentFrame = player.getCurrentFrame();
	int totalFrames = player.getTotalNumFrames();
	float pos = player.getPosition();
	float dur = player.getDuration();

	ofNoFill();
	ofRectangle bar = getBarRectangle();
	ofSetColor(244, 66, 234);
	ofDrawRectangle(bar);
	ofFill();
	ofSetColor(244, 66, 234, 180);
	bar.width *= pos;
	ofDrawRectangle(bar);

	ofDrawBitmapStringHighlight(
		"Frame " + ofToString(currentFrame) + "/" + ofToString(totalFrames)
			+ "  Pos " + ofToString(pos, 3) + "  Time " + ofToString(pos * dur, 1) + "s",
		BarInset,
		bar.getY() - 16,
		ofColor(0, 0, 0, 180),
		ofColor::lime);
}

void ofApp::drawHotkeyHelp() {
	ofDrawBitmapStringHighlight(
		"[0:First]  [1:Mid]  [2:Last]  [Space:Pause]  [\x1c\x1d:Step]",
		10,
		20,
		ofColor(0, 0, 0, 180),
		ofColor::yellow);
}

void ofApp::load(std::string movie) {
	ofSetWindowTitle(ofFilePath::getBaseName(movie));
	player.load(movie);
	player.play();
	lastMovement = ofGetSystemTimeMillis();
}

void ofApp::keyPressed(int key) {
	if (key == ' ') {
		player.setPaused(!player.isPaused());
	} else if (key == '0') {
		player.setPaused(true);
		player.setFrame(0);
		lastMovement = ofGetSystemTimeMillis();
	} else if (key == '1') {
		player.setPaused(true);
		player.setFrame(1800);
		lastMovement = ofGetSystemTimeMillis();
	} else if (key == '2') {
		player.setPaused(true);
		player.setFrame(3599);
		lastMovement = ofGetSystemTimeMillis();
	} else if (key == OF_KEY_UP) {
		player.setVolume(player.getVolume() + 0.1);
	} else if (key == OF_KEY_DOWN) {
		player.setVolume(player.getVolume() - 0.1);
	} else if (key == OF_KEY_RIGHT) {
		player.setPaused(true);
		player.nextFrame();
		lastMovement = ofGetSystemTimeMillis();
	} else if (key == OF_KEY_LEFT) {
		player.setPaused(true);
		player.previousFrame();
		lastMovement = ofGetSystemTimeMillis();
	}
}

void ofApp::keyReleased(int key) {
}

void ofApp::mouseEntered(int x, int y) {
}

void ofApp::mouseExited(int x, int y) {
}

void ofApp::mouseMoved(int x, int y) {
	if (ofGetWindowRect().inside(x, y)) {
		lastMovement = ofGetSystemTimeMillis();
	}
}

void ofApp::mouseDragged(int x, int y, int button) {
	if (inScrub) {
		float position = static_cast<float>(x - BarInset) / getBarRectangle().width;
		position = std::max(0.0f, std::min(position, 1.0f));
		player.setPosition(position);
		lastMovement = ofGetSystemTimeMillis();
	}
}

void ofApp::mousePressed(int x, int y, int button) {
	ofRectangle bar = getBarRectangle();
	if (bar.inside(x, y)) {
		inScrub = true;
		wasPaused = player.isPaused() || player.getIsMovieDone();
		player.setPaused(true);
		mouseDragged(x, y, button);
	}
	lastMovement = ofGetSystemTimeMillis();
}

void ofApp::mouseReleased(int x, int y, int button) {
	if (inScrub) {
		inScrub = false;
		player.setPaused(wasPaused);
	}
}

void ofApp::windowResized(int w, int h) {
}

void ofApp::gotMessage(ofMessage msg) {
}

void ofApp::dragEvent(ofDragInfo dragInfo) {
	if (!dragInfo.files.empty()) {
		load(dragInfo.files[0]);
	}
}

ofRectangle ofApp::getBarRectangle() const {
	return ofRectangle(BarInset, ofGetWindowHeight() - BarInset - BarHeight, ofGetWindowWidth() - (2 * BarInset), BarHeight);
}
