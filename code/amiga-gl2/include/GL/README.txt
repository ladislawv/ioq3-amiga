gl.h, glext.h, gl_mangle.h: the desktop OpenGL headers, as gl4es ships them
(github.com/ptitSeb/gl4es, commit ec16bedd, include/GL/). The GLES2 build
takes only types and constants from them (renderergl2's qgl.h, through
../SDL_opengl.h); every GL call goes through ogles2.library.

gl.h and gl_mangle.h: Mesa 3-D graphics library, Copyright (C) 1999-2006
Brian Paul, Copyright (C) 2009 VMware, Inc., MIT licence (in gl.h).
glext.h: Copyright (c) 2007-2010 The Khronos Group Inc., MIT-style licence
(in glext.h).
