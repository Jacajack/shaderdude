# shaderdude

A painfully simple GLSL shader viewer meant to be at least partially compatible with ShaderToy. I suppose that someday I might implement loading textures and meshes from files and/or add some UI for controlling uniforms to make it more fun.

<img src=img/ss1.png />

Usage: `shaderdude FILENAME [TEXTURES]` where `FILENAME` is the name of the fragment shader file and `TEXTURES` are the names of textures to be bound to subsequent channels.

The preview is automatically updated whenever the shader source code is modified.

Currently these uniform variables are passed to the fragment shader:

|Uniform|Description|
|:---|:---|
|`vec3 iResolution`|viewport resolution in pixels|
|`float iTime`|playback time in seconds|
|`int iFrame`|current frame number|
|`vec4 iMouse`|mouse position and buttons|
|`sampler2D iChannelX`|input texture `X`|
|`vec3 iChannelResolution[N]`|input textures resolutions|
 
The shader must define function `void mainImage(out vec4 fragColor, in vec2 fragCoord)`. `fragCoord` is in pixels.

