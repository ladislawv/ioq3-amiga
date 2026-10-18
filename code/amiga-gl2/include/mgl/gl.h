/*
 * The OpenGL ES 2.0 build (code/amiga-gl2): stands in for MiniGL's
 * <mgl/gl.h>, which code/amiga/amiga_input.c includes for the mouse pointer
 * calls. The renderer is renderergl2 and needs nothing else from MiniGL;
 * amiga_glimp_gl2.c defines the two calls.
 *
 * GPL v2 or later, as ioquake3.
 */
#ifndef AMIGA_GL2_MGL_GL_H
#define AMIGA_GL2_MGL_GL_H
void mglSetPointer(void);
void mglClearPointer(void);
#endif
