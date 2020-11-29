# shaderdude

A painfully simple GLSL shader viewer meant to be at least partially compatible with ShaderToy. I suppose that someday I might implement loading textures and meshes from files and/or add some UI for controlling uniforms to make it more fun.

<img src=img/ss1.png />

Usage: `shaderdude FILENAME` where `FILENAME` is the name of the fragment shader file.

The preview is automatically updated whenever the shader source code is modified.

Currently these uniform variables are passed to the fragment shader:
 - `vec3 iResolution` - viewport resolution in pixels
 - `float iTime` - playback time in seconds
 
The shader must define function `void mainImage(out vec4 fragColor, in vec2 fragCoord)`. `fragCoord` is in pixels.

