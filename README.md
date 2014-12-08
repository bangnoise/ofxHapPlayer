ofxHapPlayer
============

A Hap player for OpenFrameworks on MacOS X and Windows.

Hap is a codec for fast video playback and is available for free [here](https://github.com/Vidvox/hap-qt-codec) - it is required for accelerated playback using this addon.

The [master](https://github.com/bangnoise/ofxHapPlayer/tree/master) branch of this repo aims to keep up to date with OpenFrameworks releases. An [of_head](https://github.com/bangnoise/ofxHapPlayer/tree/of_head) branch aims to keep up to date with development work on OpenFrameworks' master branch.

Usage
-----

    #import "ofxHapPlayer.h"

ofxHapPlayer inherits from ofBaseVideoPlayer

    player.loadMovie("movies/MyMovieName.mov");

When you want to draw:

	player.draw(20, 20);

Note that there is no direct access to pixels and calls to getPixels() will return NULL.

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
