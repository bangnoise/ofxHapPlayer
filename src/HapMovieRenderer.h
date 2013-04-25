//
//  HapMovieRenderer.h
//  ofxHapPlayerExample
//
//  Created by Tom Butterworth on 24/04/2013.
//
//

#import "ofQTKitMovieRenderer.h"

@class HapOFPixelBufferTexture;

@interface HapMovieRenderer : QTKitMovieRenderer {
@private
    BOOL isHap;
    BOOL textureNeedsUpdate;
    HapOFPixelBufferTexture *hapTexture;
}
@property (readonly) GLuint textureWidth;
@property (readonly) GLuint textureHeight;
@property (readonly) BOOL textureWantsShader;
@end
