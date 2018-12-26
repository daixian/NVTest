#pragma once
#include "d3d9.h"
#include <stdio.h>
#include "nvapi.h"

typedef struct struct_3dvBuffers {
    int fbo_left, fbo_right, texture_left, texture_right, render_buffer;
    int width, height;
    bool initialized, stereo;

    // Internal
    void *d3dLibrary;
    void *d3dDevice;
    void *d3dLeftColorTexture;
    void *d3dRightColorTexture;
    void *d3dLeftColorBuffer;
    void *d3dRightColorBuffer;
    void *d3dStereoColorBuffer;
    void *d3dDepthBuffer;
    void *d3dBackBuffer;
    void *d3dDeviceInterop;
    void *d3dLeftColorInterop;
    void *d3dRightColorInterop;
    void *d3dDepthInterop;
    void *nvStereo;
} d3dvBuffers;


class dx3dv9
{
public:
    dx3dv9();
    virtual ~dx3dv9();

    void GLD3DBuffers_create(d3dvBuffers *gl_d3d_buffers, void *window_handle, bool vsync, bool stereo);
};

