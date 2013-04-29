ofxHapPlayer
============

A Hap player for OpenFrameworks 0.7.4 on MacOS X.

Hap is a codec for fast video playback and is available for free [here](https://github.com/Vidvox/hap-qt-codec) - it is required for accelerated playback using this addon.

Usage
-----

ofxHapPlayer inherits from ofQTKitPlayer.

    #import "ofxHapPlayer.h"

Use ````OF_QTKIT_DECODE_TEXTURE_ONLY```` to instigate accelerated playback:

    player.loadMovie("movies/MyMovieName.mov", OF_QTKIT_DECODE_TEXTURE_ONLY);

When you want to draw:

	player.draw(20, 20);

Advanced Usage
--------------

You can access the texture directly:

	ofTexture *texture = player.getTexture();

Note that if you access the texture directly for a Hap Q movie, you will need to use a shader when you draw:

    ofShader *shader = player.getShader();
    // the result of getShader() will be NULL if the movie is not Hap Q
    if (shader)
    {
        shader->begin();
    }
	texture.draw(x,y,w,h);
    if (shader)
    {
        shader->end();
    }
    
Credits and License
-------------------

ofxHapPlayer was written by [Tom Butterworth](http://kriss.cx/tom), April 2013, supported by [Igloo Vision](http://www.igloovision.com/) and James Sheridan. It is released under a [FreeBSD License](http://github.com/bangnoise/ofxHapPlayer/blob/master/LICENSE).
