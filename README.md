ofxHapPlayer
============

A Hap player for OpenFrameworks on MacOS X, Windows and Linux.

THIS BRANCH IS A WORK IN PROGRESS

Hap is a codec for fast video playback. You can learn more about Hap, and find codecs for encoding, at the [main Hap project](https://github.com/Vidvox/hap).


Installation
------------

This repo has branches for major OF versions. Use the branch which matches the version of OF you are using. The [master](https://github.com/bangnoise/ofxHapPlayer/tree/master) branch matches the current OF release. An [of_head](https://github.com/bangnoise/ofxHapPlayer/tree/of_head) branch aims to keep up to date with development work on OpenFrameworks' master branch.

For example, if you want to use the addon with OpenFrameworks 0.8.x:

    $ cd addons/ofxHapPlayer
    $ git checkout OpenFrameworks-0.8


Linux Requirements
------------------

This step is only necessary on Linux. On macOS and Windows, the required libraries are bundled with the addon.

On Linux, ofxHapPlayer uses system libraries. For Ubuntu, the following packages are required:

libsnappy-dev, libswresample-dev, libavcodec-dev, libavformat-dev

    sudo apt-get install libsnappy-dev libswresample-dev libavcodec-dev libavformat-dev

Pull-requests with instructions for other distributions are welcomed.


Usage
-----

Use the OF Project Generator to generate build files for your project, selecting ofxHapPlayer as an addon.

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

ofxHapPlayer was written by [Tom Butterworth](http://kriss.cx/tom), initially in April 2013, supported by [Igloo Vision](http://www.igloovision.com/) and James Sheridan. Since then it has been supported by [Vidvox](http://vidvox.net/). It is released under a [FreeBSD License](http://github.com/bangnoise/ofxHapPlayer/blob/master/LICENSE).
