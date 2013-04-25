/*
 HapPixelBufferTexture.h
 Hap QuickTime Playback
 
 Copyright (c) 2012-2013, Tom Butterworth and Vidvox LLC. All rights reserved.
 Redistribution and use in source and binary forms, with or without
 modification, are permitted provided that the following conditions are met:
 
 * Redistributions of source code must retain the above copyright
 notice, this list of conditions and the following disclaimer.
 
 * Redistributions in binary form must reproduce the above copyright
 notice, this list of conditions and the following disclaimer in the
 documentation and/or other materials provided with the distribution.
 
 THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
 ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDERS BE LIABLE FOR ANY
 DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#import <Foundation/Foundation.h>
#import <CoreVideo/CoreVideo.h>
#import <OpenGL/OpenGL.h>

/**
 A class to maintain a DXT-compressed texture for upload of DXT frames from CoreVideo pixel-buffers.
 
 Handling Scaled YCoCg DXT5 (Hap Q), requires an accompanying shader in two resource files:
    ScaledCoCgYToRGBA.vert
    ScaledCoCgYToRGBA.frag
 */
@interface HapPixelBufferTexture : NSObject
{
@private
    CGLContextObj   cgl_ctx;
    GLuint          texture;
    CVPixelBufferRef buffer;
    GLuint    backingHeight;
    GLuint     backingWidth;
    GLuint            width;
    GLuint           height;
    BOOL              valid;
    GLenum   internalFormat;
    GLhandleARB      shader;
}
/**
 Returns a HapPixelBufferTexture to draw in the provided CGL context.
 */
- (id)initWithContext:(CGLContextObj)context;

/**
 The pixel-buffer to draw. It must have a pixel-format type (as returned
 by CVPixelBufferGetPixelFormatType()) of one of the DXT formats in HapSupport.h.
 */
@property (readwrite) CVPixelBufferRef buffer;

/**
 The name of the GL_TEXTURE_2D texture.
 */
@property (readonly) GLuint textureName;

/**
 The width of the texture in texels. This may be greater than the image width.
 */
@property (readonly) GLuint textureWidth;

/**
 The height of the texture in texels. This may be greater than the image height.
 */
@property (readonly) GLuint textureHeight;

/**
 The width of the image in texels. The image may not fill the entire texture.
 */
@property (readonly) GLuint width;

/**
 The height of the image in texels. The image may not fill the entire texture.
 */
@property (readonly) GLuint height;

/**
 Scaled YCoCg DXT5 requires a shader to convert color values when it is drawn.
 If the attached pixel-buffer contains Scaled YCoCg DXT5 pixels, the value of this property will be non-NULL
 and should be bound to the GL context prior to drawing the texture.
 */
@property (readonly) GLhandleARB shaderProgramObject;

// @property (readonly) GLenum textureTarget; // is always GL_TEXTURE_2D

// @property (readonly) BOOL textureIsFlipped; // is always YES

@end
