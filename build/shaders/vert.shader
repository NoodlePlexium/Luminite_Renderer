#version 440 core

layout (location = 0) in vec2 aPos;
layout (location = 1) in vec2 aTextureCoord;
out vec2 TextureCoord;

void main()
{
    gl_Position = vec4(aPos, 0.0, 1.0);
    TextureCoord = aTextureCoord;
}