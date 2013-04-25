//
//  HapMovieRenderer.h
//  ofxHapPlayerExample
//
//  Created by Tom Butterworth on 24/04/2013.
//
//

#import "ofQTKitMovieRenderer.h"

@class HapPixelBufferTexture;

@interface HapMovieRenderer : QTKitMovieRenderer {
@private
    BOOL isHap;
    BOOL textureNeedsUpdate;
    HapPixelBufferTexture *hapTexture;
}
@property (readonly) GLuint textureWidth;
@property (readonly) GLuint textureHeight;
@end
