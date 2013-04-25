/*
 HapPixelBufferTexture.m
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

#import "HapPixelBufferTexture.h"
#import <OpenGL/CGLMacro.h>
#import "HapSupport.h"

// Utility to round up to a multiple of 4.
static int roundUpToMultipleOf4( int n )
{
	if( 0 != ( n & 3 ) )
		n = ( n + 3 ) & ~3;
	return n;
}

@interface HapPixelBufferTexture (Shader)
- (GLhandleARB)loadShaderOfType:(GLenum)type;
@end

@implementation HapPixelBufferTexture
- (id)initWithContext:(CGLContextObj)context
{
    self = [super init];
    if (self)
    {
        cgl_ctx = CGLRetainContext(context);
    }
    return self;
}

- (void)dealloc
{
    if (texture != 0) glDeleteTextures(1, &texture);
    if (shader != NULL) glDeleteObjectARB(shader);
    CVBufferRelease(buffer);
    CGLReleaseContext(cgl_ctx);
    [super dealloc];
}

- (void)setBuffer:(CVPixelBufferRef)newBuffer
{
    CVBufferRetain(newBuffer);
    
    CVPixelBufferUnlockBaseAddress(buffer, kCVPixelBufferLock_ReadOnly);
    CVBufferRelease(buffer);
    
    buffer = newBuffer;
    
    CVPixelBufferLockBaseAddress(buffer, kCVPixelBufferLock_ReadOnly);
    
    width = CVPixelBufferGetWidth(buffer);
    height = CVPixelBufferGetHeight(buffer);
    
    if (buffer == NULL)
    {
        valid = NO;
        return;
    }
    
    // Check the buffer padding
    
    size_t extraRight, extraBottom;
    
    CVPixelBufferGetExtendedPixels(buffer, NULL, &extraRight, NULL, &extraBottom);
    GLuint roundedWidth = width + extraRight;
    GLuint roundedHeight = height + extraBottom;
    
    // Valid DXT will be a multiple of 4 wide and high
    
    if (roundedWidth % 4 != 0 || roundedHeight % 4 != 0)
    {
        valid = NO;
        return;
    }
    
    OSType newPixelFormat = CVPixelBufferGetPixelFormatType(buffer);
    
    GLenum newInternalFormat;
    unsigned int bitsPerPixel;
    
    switch (newPixelFormat) {
        case kHapPixelFormatTypeRGB_DXT1:
            newInternalFormat = GL_COMPRESSED_RGB_S3TC_DXT1_EXT;
            bitsPerPixel = 4;
            break;
        case kHapPixelFormatTypeRGBA_DXT5:
        case kHapPixelFormatTypeYCoCg_DXT5:
            newInternalFormat = GL_COMPRESSED_RGBA_S3TC_DXT5_EXT;
            bitsPerPixel = 8;
            break;
        default:
            // we don't support non-DXT pixel buffers
            valid = NO;
            return;
            break;
    }
    
    // Ignore the value for CVPixelBufferGetBytesPerRow()
    
    size_t bytesPerRow = (roundedWidth * bitsPerPixel) / 8;
    GLsizei newDataLength = bytesPerRow * roundedHeight; // usually not the full length of the buffer
    
    size_t actualBufferSize = CVPixelBufferGetDataSize(buffer);
    
    // Check the buffer is as large as we expect it to be
    
    if (newDataLength > actualBufferSize)
    {
        valid = NO;
        return;
    }
    
    // If we got this far we're good to go
    
    valid = YES;
    
    glPushAttrib(GL_ENABLE_BIT | GL_TEXTURE_BIT);
    glPushClientAttrib(GL_CLIENT_PIXEL_STORE_BIT);
    glEnable(GL_TEXTURE_2D);
            
    GLvoid *baseAddress = CVPixelBufferGetBaseAddress(buffer);
    
    // Create a new texture if our current one isn't adequate
    
    if (texture == 0 || roundedWidth > backingWidth || roundedHeight > backingHeight || newInternalFormat != internalFormat)
    {
        if (texture != 0)
        {
            glDeleteTextures(1, &texture);
        }
        
        glGenTextures(1, &texture);
        
        glBindTexture(GL_TEXTURE_2D, texture);
        
        // On NVIDIA hardware there is a massive slowdown if DXT textures aren't POT-dimensioned, so we use POT-dimensioned backing
        
        backingWidth = 1;
        while (backingWidth < roundedWidth) backingWidth <<= 1;
        
        backingHeight = 1;
        while (backingHeight < roundedHeight) backingHeight <<= 1;
        
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_STORAGE_HINT_APPLE , GL_STORAGE_SHARED_APPLE);
        
        // We allocate the texture with no pixel data, then use CompressedTexSubImage to update the content region
        
        glTexImage2D(GL_TEXTURE_2D, 0, newInternalFormat, backingWidth, backingHeight, 0, GL_BGRA, GL_UNSIGNED_INT_8_8_8_8_REV, NULL);        
        
        internalFormat = newInternalFormat;
    }
    else
    {
        glBindTexture(GL_TEXTURE_2D, texture);
    }
    
    glTextureRangeAPPLE(GL_TEXTURE_2D, newDataLength, baseAddress);
    glPixelStorei(GL_UNPACK_CLIENT_STORAGE_APPLE, GL_TRUE);
    
    glCompressedTexSubImage2D(GL_TEXTURE_2D,
                              0,
                              0,
                              0,
                              roundedWidth,
                              roundedHeight,
                              newInternalFormat,
                              newDataLength,
                              baseAddress);
    
    glPopClientAttrib();
    glPopAttrib();
}

- (CVPixelBufferRef)buffer { return buffer; }

- (GLuint)textureName
{
    if (valid) return texture;
    else return 0;
}

- (GLuint)width
{
    if (valid) return width;
    else return 0;
}

- (GLuint)height
{
    if (valid) return height;
    else return 0;
}

- (GLuint)textureWidth
{
    if (valid) return backingWidth;
    else return 0;
}

- (GLuint)textureHeight
{
    if (valid) return backingHeight;
    else return 0;
}

- (GLhandleARB)shaderProgramObject
{
    if (valid && buffer && CVPixelBufferGetPixelFormatType(buffer) == kHapPixelFormatTypeYCoCg_DXT5)
    {
        if (shader == NULL)
        {
            GLhandleARB vert = [self loadShaderOfType:GL_VERTEX_SHADER_ARB];
            GLhandleARB frag = [self loadShaderOfType:GL_FRAGMENT_SHADER_ARB];
            GLint programLinked = 0;
            if (frag && vert)
            {
                shader = glCreateProgramObjectARB();
                glAttachObjectARB(shader, vert);
                glAttachObjectARB(shader, frag);
                glLinkProgramARB(shader);
                glGetObjectParameterivARB(shader,
                                          GL_OBJECT_LINK_STATUS_ARB,
                                          &programLinked);
                if(programLinked == 0 )
                {
                    glDeleteObjectARB(shader);
                    shader = NULL;
                }
            }
            if (frag) glDeleteObjectARB(frag);
            if (vert) glDeleteObjectARB(vert);
        }
        return shader;
    }
    return NULL;
}

- (GLhandleARB)loadShaderOfType:(GLenum)type
{
    NSString *extension = (type == GL_VERTEX_SHADER_ARB ? @"vert" : @"frag");
    
    NSString  *path = [[NSBundle bundleForClass:[self class]] pathForResource:@"ScaledCoCgYToRGBA"
                                                                       ofType:extension];
    NSString *source = nil;
    if (path) source = [NSString stringWithContentsOfFile:path usedEncoding:nil error:nil];
    
    GLint       shaderCompiled = 0;
	GLhandleARB shaderObject = NULL;
    
	if(source != nil)
	{
        const GLcharARB *glSource = [source cStringUsingEncoding:NSASCIIStringEncoding];
		
		shaderObject = glCreateShaderObjectARB(type);
		glShaderSourceARB(shaderObject, 1, &glSource, NULL);
		glCompileShaderARB(shaderObject);
		glGetObjectParameterivARB(shaderObject,
								  GL_OBJECT_COMPILE_STATUS_ARB,
								  &shaderCompiled);
		
		if(shaderCompiled == 0 )
		{
            glDeleteObjectARB(shaderObject);
			shaderObject = NULL;
		}
	}
	return shaderObject;
}
@end
