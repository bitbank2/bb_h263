## bb_h263
A portable C library to play H.263 video files on constrained systems.<br>

### Why did you write it?
Many projects would like to display video and animations on constrained devices. A typical solution is to use GIF animations or Motion-JPEG video. Both of these choices work with slow CPUs and small memories, but come at the expense of poorly compressed data. The large data size creates challenges in storing and transmitting these animations and videos. H.263 is an old video codec that is a good fit to replace both GIF and MJPEG. H.263 is mostly obsolete as a video codec, but is still supported by FFMPEG, so it's easy to convert any video into a compatible file.<br>

### What's special about it?
The entire library is in the form of a single .H file that can be easily added to any project. It can read H.263 video data from both AVI and QuickTime files. The code has been optimized for speed any memory size to allow it to run well on humble microcontrollers such as the ESP32-S3.<br>

### How difficult is it to use?
The library includes a simple C API as well as a C++ wrapper which makes it extremely easy to use in any environment. The video data can come from a pointer to memory or read from an external file system.<br>

### Does it support audio output?
The focus of the project is for video and animation playback; the current version does not support audio. Partial support for audio is in the code and may be offered in a future version.

### How are the frames generated?
The user can ask the library to allocate or provide a pointer to a RGB565 framebuffer. Each call to decodeFrame() updates the changing pixels in that framebuffer.<br>
