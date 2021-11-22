# VirtualCameraFilter
Virtual Camera Filter for Direct Show.  
You can use this project as a template, now retrieving and buffering algorithms are not well done.

# Prerequisites
## Direct Show Base Classses
Download from [baseclasses](https://github.com/microsoft/Windows-classic-samples/tree/main/Samples/Win7Samples/multimedia/directshow/baseclasses), then copy like as below,

```
VirtualCameraFilter/
 |- baseclasses/
```

# Build
Can build for both x86-64 and x86 dlls, because the dll's architectures should match to the application's architectures. For example, Teams is a x86-64 architecture application, so x86-64 filter dll is needed.
For, `Visual Studio 16 2019`,

```
make build32 & cd build32 & cmake -G"Visual Studio 16 2019" -A Win32 ..
```

```
make build64 & cd build64 & cmake -G"Visual Studio 16 2019" ..
```

# Register or Unregister
Run install.bat or uninstall.bat as an administrator.

# Push Frame Data from Your Application
For example, push frame buffer data form an OpenGL application.

```cpp
int main(void)
{
    if(!glfwInit()) {
        return 0;
    }

    vcam::VCamPipe vcamPipe;
    vcamPipe.openWrite();
    vcam::u32 vcam_width=0;
    vcam::u32 vcam_height=0;
    vcam::u32 vcam_bpp = 0;
    vcamPipe.getFormat(vcam_width, vcam_height, vcam_bpp);

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 6);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_DOUBLEBUFFER, GLFW_TRUE);
    int width = (0 < vcam_width) ? static_cast<int>(vcam_width) : 640;
    int height = (0 < vcam_height) ? static_cast<int>(vcam_height) : 480;
    GLFWwindow* window = glfwCreateWindow(width, height, "App", nullptr, nullptr);
    if(nullptr == window) {
        glfwTerminate();
        return 0;
    }
    glfwMakeContextCurrent(window);
    GLenum err = glewInit();
    if(GLEW_OK != err) {
        fprintf(stderr, "Error: %s\n", glewGetErrorString(err));
        glfwDestroyWindow(window);
        glfwTerminate();
        return 0;
    }
    glfwSwapInterval(1);

    glfwGetFramebufferSize(window, &width, &height);
    float ratio = static_cast<float>(width) / height;

    glViewport(0, 0, width, height);
    vcam::u8* framebuffer = reinterpret_cast<vcam::u8*>(malloc(sizeof(vcam::u8)*width*height*3));

    while(!glfwWindowShouldClose(window)) {
        glClear(GL_COLOR_BUFFER_BIT);
        glReadBuffer(GL_BACK);
        glReadPixels(0, 0, width, height, GL_RGB, GL_UNSIGNED_BYTE, framebuffer);
        vcamPipe.push(width, height, 3, framebuffer);
        glfwSwapBuffers(window);
    }
    free(framebuffer);
    glfwDestroyWindow(window);
    glfwTerminate();
    vcamPipe.close();
    return 0;
}
```

