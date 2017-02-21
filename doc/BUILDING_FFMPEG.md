You do not need to read this file to use ofxHapPlayer.

Currently ofxHapPlayer on macOS and Windows uses libraries from FFmpeg 3.2.4. On Linux, system libraries are used.

Building FFmpeg libraries on macOS
----------------------------------

The only way to build the FFmpeg libraries to support an older macOS version is to build on that OS version. FFmpeg's configure script makes decisions about available libraries based on the system used to build. For this reason, the libraries are built on a clean 10.10 system with Xcode 7.2.1 installed, and pkg-config and yasm installed via [Homebrew](https://brew.sh).

32-bit and 64-bit builds are produced, and then universal binaries are made using lipo.

This script assumes a file system layout thus:

    ffmpeg/
        repo/
            ...
        build/
            this_script.sh
            install/
                ...


```bash
#!/bin/sh
OF_INSTALL_DST="install"
OF_FFMPEG_COMMON="--extra-cflags=\"-mmacosx-version-min=10.10\" --extra-ldflags=\"-mmacosx-version-min=10.10\" --enable-shared --disable-static --disable-programs --disable-avdevice --disable-avfilter --disable-swscale --disable-postproc --disable-everything --enable-protocol=file --enable-protocol=http --enable-protocol=https --enable-demuxer=avi --enable-demuxer=mov --enable-decoder=aac --enable-decoder=mp3 --enable-decoder=pcm_u8 --enable-decoder=pcm_s8 --enable-decoder=pcm_s16le --enable-decoder=pcm_s16be --enable-decoder=pcm_u16le --enable-decoder=pcm_u16be --enable-decoder=pcm_s24le --enable-decoder=pcm_s24be --enable-decoder=pcm_u24le --enable-decoder=pcm_u24be --enable-decoder=pcm_s32le --enable-decoder=pcm_s32be --enable-decoder=pcm_u32le --enable-decoder=pcm_u32be --enable-decoder=pcm_f32be --enable-decoder=pcm_s64le --enable-decoder=pcm_s64be --enable-decoder=pcm_f64be --enable-decoder=pcm_alaw --enable-decoder=pcm_mulaw --enable-decoder=pcm_s8_planar --enable-decoder=pcm_s16le_planar --enable-decoder=pcm_s16be_planar --enable-decoder=pcm_s24le_planar --enable-decoder=pcm_s32le_planar --enable-rpath --install-name-dir=\"@rpath/ffmpeg/lib/osx\""

set -x
rm -rf $OF_INSTALL_DST/*
make clean
../repo/configure $OF_FFMPEG_COMMON --arch=i386 --cc="clang -m32" --prefix="$OF_INSTALL_DST/i386"
make install -j8
make clean
../repo/configure $OF_FFMPEG_COMMON --cc=clang --prefix="$OF_INSTALL_DST/x64"
make install -j8
make clean

pushd $OF_INSTALL_DST

mkdir lib

lipo -create i386/lib/libavcodec.57.64.101.dylib x64/lib/libavcodec.57.64.101.dylib -o lib/libavcodec.57.64.101.dylib
lipo -create i386/lib/libavformat.57.56.101.dylib x64/lib/libavformat.57.56.101.dylib -o lib/libavformat.57.56.101.dylib
lipo -create i386/lib/libavutil.55.34.101.dylib x64/lib/libavutil.55.34.101.dylib -o lib/libavutil.55.34.101.dylib
lipo -create i386/lib/libswresample.2.3.100.dylib x64/lib/libswresample.2.3.100.dylib -o lib/libswresample.2.3.100.dylib

pushd lib

ln -s libavcodec.57.64.101.dylib libavcodec.57.dylib
ln -s libavcodec.57.64.101.dylib libavcodec.dylib
ln -s libavformat.57.56.101.dylib libavformat.57.dylib
ln -s libavformat.57.56.101.dylib libavformat.dylib
ln -s libavutil.55.34.101.dylib libavutil.55.dylib
ln -s libavutil.55.34.101.dylib libavutil.dylib
ln -s libswresample.2.3.100.dylib libswresample.2.dylib
ln -s libswresample.2.3.100.dylib libswresample.dylib

popd

popd
```

Building FFmpeg libraries on Windows
------------------------------------

Visual Studio 2015, [MSYS2](https://pracucci.com/%E2%80%8Bhttp://msys2.github.io) and [YASM](http://yasm.tortall.net/Download.html) are required.

 1. Create a unique MSYS2 installation for building ffmpeg at eg c:\ffmpegbuild\msys64
 2. Install make and diffutils in MSYS2
 3. Rename /ffmpegbuild/msys64/usr/bin/link.exe to /ffmpegbuild/msys64/usr/bin/link_old.exe to avoid conflict with MSVC's link.exe
 4. Install YASM and rename it to yasm.exe, adding its location to your $PATH
 5. Run the VS2015 x86 Native Tools Command Prompt
 6. Run c:\ffmpegbuild\msys64\msys2_shell.cmd -msys -use-full-path
 7. cd to your ffmpeg build location and run the following script
         ```bash
        #!/bin/sh
        OF_INSTALL_DST="install"
        OF_FFMPEG_COMMON="--disable-shared --enable-static --toolchain=msvc --enable-yasm --disable-nvenc --disable-debug --disable-programs --disable-avdevice --disable-avfilter --disable-swscale --disable-postproc --disable-everything --enable-protocol=file --enable-protocol=http --enable-protocol=https --enable-demuxer=avi --enable-demuxer=mov --enable-decoder=aac --enable-decoder=mp3 --enable-decoder=pcm_u8 --enable-decoder=pcm_s8 --enable-decoder=pcm_s16le --enable-decoder=pcm_s16be --enable-decoder=pcm_u16le --enable-decoder=pcm_u16be --enable-decoder=pcm_s24le --enable-decoder=pcm_s24be --enable-decoder=pcm_u24le --enable-decoder=pcm_u24be --enable-decoder=pcm_s32le --enable-decoder=pcm_s32be --enable-decoder=pcm_u32le --enable-decoder=pcm_u32be --enable-decoder=pcm_f32be --enable-decoder=pcm_s64le --enable-decoder=pcm_s64be --enable-decoder=pcm_f64be --enable-decoder=pcm_alaw --enable-decoder=pcm_mulaw --enable-decoder=pcm_s8_planar --enable-decoder=pcm_s16le_planar --enable-decoder=pcm_s16be_planar --enable-decoder=pcm_s24le_planar --enable-decoder=pcm_s32le_planar"

        set -x
        ../repo/configure $OF_FFMPEG_COMMON --prefix="$OF_INSTALL_DST/x86"
        make clean
        make install -j8
         ```
 8. Rename the resulting libraries from .a to .lib so the OpenFrameworks Project Generator treats them as static libraries.
 9. Run the VS2015 x64 Native Tools Command Prompt and repeat steps 6 to 8 to build the x64 libraries
