ofxHapPlayer
============

A Hap player for OpenFrameworks.

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
    
