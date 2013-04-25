ofxHapPlayer
============

A player for Hap movies.

Usage
-----

    #import "ofxHapPlayer.h"

Use ````OF_QTKIT_DECODE_TEXTURE_ONLY```` to instigate accelerated playback.

    player.loadMovie("movies/MyMovieName.mov", OF_QTKIT_DECODE_TEXTURE_ONLY);

You can draw...

	player.draw(20, 20);

...or access the texture directly

	ofTexture *tex = player.getTexture();
