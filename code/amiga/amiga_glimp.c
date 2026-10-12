/*
===========================================================================
Copyright (C) 1999-2005 Id Software, Inc.

This file is part of Quake III Arena source code.

Quake III Arena source code is free software; you can redistribute it
and/or modify it under the terms of the GNU General Public License as
published by the Free Software Foundation; either version 2 of the License,
or (at your option) any later version.

Quake III Arena source code is distributed in the hope that it will be
useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with Foobar; if not, write to the Free Software
Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
===========================================================================
*/

#include "../renderer/tr_local.h"
#include "../client/client.h"
#include "amiga_local.h"

#pragma pack(push,2)

#include <exec/exec.h>
#include <exec/memory.h>
#include <exec/execbase.h>
#include <intuition/intuition.h>
#include <graphics/gfx.h>
#include <cybergraphx/cybergraphics.h>
#include <clib/alib_protos.h>
#include <proto/exec.h>
#include <proto/intuition.h>
#include <proto/cybergraphics.h>

#ifdef __PPC__
#if defined(__GNUC__)
#include <powerpc/powerpc_protos.h>
#else
#include <powerpc/powerpc.h>
#include <proto/powerpc.h>
#endif
#endif

#pragma pack(pop)

#include <mgl/gl.h>
#ifdef USE_MINIGL_LIBRARY
/* Routes every gl and mgl name through minigl.library. Must follow
 * mgl/gl.h, whose direct-call macros would otherwise win. */
#include "minigl_library_compat.h"
#endif

#ifdef USE_MINIGL_LIBRARY
/* The driver defines these when it is linked in. A library client links no
 * driver objects, so it owns them -- same as glquakelib (gl_vidamiga.c). */
struct IntuitionBase *IntuitionBase = NULL;
static qboolean minigl_library_open = qfalse;
#endif

extern cvar_t *r_finish;
cvar_t *r_closeworkbench;
cvar_t *r_guardband;
cvar_t *r_vertexbuffersize;
cvar_t *r_glbuffers;
cvar_t *r_perspective_fast;
cvar_t *r_mintriarea;

extern cvar_t *in_nograb;

struct MsgPort *Sys_EventPort = 0;
struct Window *win = NULL;

void (APIENTRYP qglActiveTextureARB) (GLenum texture);
void (APIENTRYP qglClientActiveTextureARB) (GLenum texture);
void (APIENTRYP qglMultiTexCoord2fARB) (GLenum target, GLfloat s, GLfloat t);

void (APIENTRYP qglLockArraysEXT) (GLint first, GLsizei count);
void (APIENTRYP qglUnlockArraysEXT) (void);

void GLimp_RenderThreadWrapper( void *stub ) {}
void GLimp_SetGamma( unsigned char red[256], unsigned char green[256], unsigned char blue[256] ) {}
void GLW_InitGamma(void) {}
void GLW_RestoreGamma(void) {}

extern qboolean mouse_avail;
extern qboolean mhandler;
qboolean windowmode; // IN_Frame check

extern void mglv3d_log(const char *msg);

static qboolean GLW_StartDriverAndSetMode( int mode, int colorbits, qboolean fullscreen )
{
	//BOOL useStencil = /*TRUE*/ FALSE;
	int depth;

	ri.Printf(PRINT_ALL, "...setting mode %d:", mode);

	if (!R_GetModeInfo( &glConfig.vidWidth, &glConfig.vidHeight, &glConfig.windowAspect, mode))
	{
		ri.Printf(PRINT_ALL, " invalid mode\n");
		return qfalse;
	}

	depth = colorbits;

	if(depth != 15 && depth != 16 && depth != 24 && depth != 32)
		depth = 16;
	
	ri.Printf( PRINT_ALL, " %d %d %s\n", glConfig.vidWidth, glConfig.vidHeight, fullscreen ? "fullscreen" : "windowed" );

	r_vertexbuffersize  = Cvar_Get("r_vertexbuffersize", "4096", CVAR_ARCHIVE | CVAR_LATCH);
	r_glbuffers  = Cvar_Get("r_glbuffers", "3", CVAR_ARCHIVE);
	r_guardband  = Cvar_Get("r_guardband", "0", CVAR_ARCHIVE | CVAR_LATCH);
	r_closeworkbench = Cvar_Get("r_closeworkbench", "0", CVAR_ARCHIVE);
	r_perspective_fast = Cvar_Get("r_perspective_fast", "0", CVAR_ARCHIVE | CVAR_LATCH);
	r_mintriarea = Cvar_Get("r_mintriarea", "0", CVAR_ARCHIVE | CVAR_LATCH);

	mglChoosePixelDepth(depth); // default 16

	if(!fullscreen)
	{
		mglChooseWindowMode(GL_TRUE);
		Cvar_Set("r_glbuffers", "2");
	}

	else
	{
		mglChooseWindowMode(GL_FALSE);
		Cvar_Set("r_glbuffers", "3");

		if (r_closeworkbench->value)
			mglProposeCloseDesktop(GL_TRUE);
	}

	mglChooseNumberOfBuffers( (int)r_glbuffers->value );

	if(r_guardband->value)
	{
		mglChooseGuardBand(GL_TRUE);
		ri.Printf(PRINT_ALL, "guardband on\n");
	}

	else
	{
		mglChooseGuardBand(GL_FALSE);
		ri.Printf(PRINT_ALL, "guardband off\n");
	}

	/*
	** surgeon: The highest number of verts is 4096
	** (max point-particles) and in that case no clipping-space
	** is needed. However, plenty of clippingspace is needed for
	** locked vertexarrays which offsets the transformation to
	** buffersize/4
	**/

	/*
	** MAX_VERTS is 2048 (for alias models) 
	** MiniGL offsets clip/transform to either 
	** buffersize/2 or buffersize/4 for glDrawElements 
	** Therefore 4096 is enough for the vertexbuffer 
	*/

	/*
	** The multitexture buffer is able to store
	** buffersize/4 polygons after backface-culling, which
	** means that 4096 is able to store 1024 polys with
	** max 4096 verts in total (tightly packed)
	*/

	mglChooseVertexBufferSize( (int)r_vertexbuffersize->value ); // 2048 - 4096 - 16384 //

	ri.Printf(PRINT_ALL, "vertexbuffersize %d\n", (int)r_vertexbuffersize->value );

	/* Real hardware polygon offset needs no opt-in any more (2026-09-10):
	 * tr_shade.c's glEnable(GL_POLYGON_OFFSET_FILL) + glPolygonOffset(
	 * r_offsetFactor, r_offsetUnits) drive MiniGLV3D directly. The stall that
	 * once gated it is fixed (leftover reserved bits in replayed CL packets). */

	if(!mglCreateContext(0, 0, glConfig.vidWidth, glConfig.vidHeight))
	{
		return qfalse;
	}

	if (fullscreen) // && r_glbuffers->value == 3)
	{
		mglEnableSync(GL_FALSE);
		ri.Printf(PRINT_ALL, "triplebuffer enabled\n");
		ri.Printf(PRINT_ALL, "sync disabled\n");
		windowmode = qfalse;
	}

	else
	{
		mglEnableSync(GL_TRUE);
		windowmode = qtrue;
	}

	mglLockMode(MGL_LOCK_SMART);

	/*
	if (r_finish)
		mglEnableSync(r_finish->integer);

	else
		mglEnableSync(GL_TRUE);
	*/

	glConfig.colorBits = depth; // colorbits;
	/* 16 -> 32 on 2026-09-05: MiniGLV3D's depth buffer moved from D16 to D32F
	 * after milestone_l26_depth_precision measured D16 losing a 1.0
	 * world-unit separation between z=300 and z=1000 under a Quake-shaped
	 * frustum -- well inside real map scale, and exactly the scale decals and
	 * shadows sit at. This field is purely a REPORT of what the driver has;
	 * there is no MiniGL pixel-format negotiation to request it, so changing
	 * it here does not select anything. It is user-visible in two places
	 * (the console's PIXELFORMAT line, tr_init.c, and the video menu's
	 * Z(%d-bits), ui_video.c), and leaving it at 16 would simply have been
	 * false. Note r_depthbits is registered (tr_init.c) and honoured by the
	 * SDL backend, but has never done anything on this port. */
	glConfig.depthBits = 32;
	glConfig.stencilBits = 0;

	/*
	if (glConfig.hardwareType == GLHW_3DFX_2D3D)
	{
		glConfig.depthBits = 16;
		glConfig.stencilBits = 0;
	}

	else
	{
		glConfig.depthBits = 16;
		glConfig.stencilBits = 8;
	}
	*/

	glConfig.isFullscreen = fullscreen;
	
	// test - Cowcat
	if(r_perspective_fast->value)
	{
		glHint (GL_PERSPECTIVE_CORRECTION_HINT, GL_FASTEST);
		ri.Printf(PRINT_ALL, "GL_Perspective_Correction_hint: fast\n");
	}

	if(r_mintriarea->value) // screenwidth *screenheight/75000 (rounded)
	{
		mglMinTriArea(r_mintriarea->value);
		ri.Printf(PRINT_ALL, "MinTriArea: %1.1f\n", r_mintriarea->value);
	}
	//

	// The GL clear is issued in GLimp_Init, after ModifyIDCMP -- doing it
	// here would mean real GPU clear/submit work before window/IDCMP
	// setup is finished, unlike glquake's proven-safe ordering (only a
	// trivial glEnable(GL_DITHER) in the equivalent spot).

	return qtrue;
}
	
static void GLW_Shutdown(void)
{
	mglDeleteContext();

#ifdef USE_MINIGL_LIBRARY
	/* MGLTerm is deliberately absent from the dispatch table -- the resident
	 * library owns its own teardown. Releasing the library is this client's
	 * whole job here. */
	if (minigl_library_open)
	{
		MiniGLClose();
		minigl_library_open = qfalse;
	}
	if (IntuitionBase)
	{
		CloseLibrary((struct Library *)IntuitionBase);
		IntuitionBase = NULL;
	}
#else
	MGLTerm();
#endif

	Sys_EventPort = NULL;
}

static qboolean GLW_LoadOpenGL()
{
	qboolean fullscreen;

#ifdef USE_MINIGL_LIBRARY
	/* A library client must bind the dispatch table before any gl call, and
	 * MiniGLOpen validates the ABI while doing it, so a mismatched
	 * minigl.library is refused here rather than producing wrong results
	 * later. IntuitionBase comes with it: the driver owns that global in the
	 * static build, and no driver objects are linked here. */
	if (!IntuitionBase)
		IntuitionBase = (struct IntuitionBase *)OpenLibrary("intuition.library", 36);
	if ( !MiniGLOpen() )
	{
		ri.Printf(PRINT_ALL, "GLW_LoadOpenGL: MiniGLOpen failed -- minigl.library missing or ABI mismatch\n");
		return qfalse;
	}
	minigl_library_open = qtrue;
#endif

#ifdef USE_MINIGL_LIBRARY
	/* No MGLInit here. It is not in the dispatch table: the resident library
	 * initialises itself, and MiniGLOpen above is this client's whole entry
	 * point. glquakelib calls neither MGLInit nor MGLTerm for the same
	 * reason. */
	if ( 1 )
#else
	if ( MGLInit() )
#endif
	{
		fullscreen = r_fullscreen->integer;
		
		if (!GLW_StartDriverAndSetMode(r_mode->integer, r_colorbits->integer, fullscreen))
		{
			if (r_mode->integer != 3)
			{
				if (!GLW_StartDriverAndSetMode(3, r_colorbits->integer, fullscreen))
				{
#ifndef USE_MINIGL_LIBRARY
					MGLTerm();
#endif
					return qfalse;
				}
			}
		}
		
		return qtrue;
	}

	ri.Printf(PRINT_ALL, "failed\n");

#ifndef USE_MINIGL_LIBRARY
	MGLTerm();
#endif

	return qfalse;
}

void GLW_StartOpenGL(void)
{
	GLW_LoadOpenGL();
}

static void stub_glMultiTexCoord2fARB(GLenum target, GLfloat s, GLfloat t)
{
	glMultiTexCoord2fARB(target, s, t);
}

static void stub_glActiveTextureARB(GLenum texture)
{
	glActiveTextureARB(texture);
}

/* Real vertex-array multitexture (2026-08-25, r_vertexLight=0 stall fix):
 * MiniGLV3D now has a real glClientActiveTextureARB (vertexarray.c) --
 * see this file's own GL_ARB_multitexture detection block below for why
 * this was never wired up before. */
static void stub_glClientActiveTextureARB(GLenum texture)
{
	glClientActiveTextureARB(texture);
}

static void stub_glLockArraysEXT(GLint first, GLsizei count)
{
	glLockArrays(first, count);
}

static void stub_glUnlockArraysEXT()
{
	glUnlockArrays();
}

static void GLW_InitExtensions( void )
{
	if ( !r_allowExtensions->integer )
	{
		ri.Printf( PRINT_ALL, "*** IGNORING OPENGL EXTENSIONS ***\n" );
		return;
	}

	ri.Printf( PRINT_ALL, "Initializing OpenGL extensions\n" );

	// GL_S3_s3tc
	glConfig.textureCompression = TC_NONE;

	#if 0
	if ( strstr( glConfig.extensions_string, "GL_S3_s3tc" ) )
	{
		if ( r_ext_compressed_textures->integer )
		{
			glConfig.textureCompression = TC_S3TC;
			ri.Printf( PRINT_ALL, "...using GL_S3_s3tc\n" );
		}

		else
		{
			glConfig.textureCompression = TC_NONE;
			ri.Printf( PRINT_ALL, "...ignoring GL_S3_s3tc\n" );
		}
	}

	else
	#endif
	{
		ri.Printf( PRINT_ALL, "...GL_S3_s3tc not found\n" );
	}

	// GL_EXT_texture_env_add
	glConfig.textureEnvAddAvailable = qfalse;

	#if 0
	if ( strstr( glConfig.extensions_string, "EXT_texture_env_add" ) )
	{
		if ( r_ext_texture_env_add->integer )
		{
			glConfig.textureEnvAddAvailable = qtrue;
			ri.Printf( PRINT_ALL, "...using GL_EXT_texture_env_add\n" );
		}

		else
		{
			glConfig.textureEnvAddAvailable = qfalse;
			ri.Printf( PRINT_ALL, "...ignoring GL_EXT_texture_env_add\n" );
		}
	}

	else
	#endif
	{
		ri.Printf( PRINT_ALL, "...GL_EXT_texture_env_add not found\n" );
	}

	// GL_ARB_multitexture

	/* Real vertex-array multitexture (2026-08-25, r_vertexLight=0 stall
	 * fix): re-enabled -- MiniGLV3D now has a real glClientActiveTextureARB
	 * (vertexarray.c) wired up to real per-unit texcoord arrays, so the
	 * "minigl has no support for varray multitexture" reason this was
	 * disabled no longer applies. The extension-string check itself was
	 * always correct (MiniGLV3D really does advertise
	 * "GL_MGL_ARB_multitexture", others.c) -- only the #if 0 disabled it. */
	if ( strstr( glConfig.extensions_string, "GL_MGL_ARB_multitexture" )  )
	{
		if ( r_ext_multitexture->integer )
		{
			if ( 1 )
			{
				//qglGetIntegerv( GL_MAX_TEXTURE_UNITS_ARB/*GL_MAX_ACTIVE_TEXTURES_ARB*/, &glConfig.maxActiveTextures );
				GLint glint = 0;
				qglGetIntegerv( GL_MAX_TEXTURE_UNITS_ARB, &glint );
				glConfig.numTextureUnits = (int) glint;

				if ( glConfig.numTextureUnits > 1 )
				{
					qglMultiTexCoord2fARB = stub_glMultiTexCoord2fARB;
					qglActiveTextureARB = stub_glActiveTextureARB;
					qglClientActiveTextureARB = stub_glClientActiveTextureARB;

					ri.Printf( PRINT_ALL, "...using GL_ARB_multitexture\n" );
				}

				else
				{
					qglMultiTexCoord2fARB = NULL;
					qglActiveTextureARB = NULL;
					qglClientActiveTextureARB = NULL;

					ri.Printf( PRINT_ALL, "...not using GL_ARB_multitexture, < 2 texture units\n" );
				}
			}
		}

		else
		{
			ri.Printf( PRINT_ALL, "...ignoring GL_ARB_multitexture\n" );
		}
	}

	else
	{
		//ri.Printf( PRINT_ALL, "...GL_ARB_multitexture not found\n" );
		ri.Printf( PRINT_ALL, "...not using GL_ARB_multitexture\n" );
	}

	// GL_EXT_compiled_vertex_array

	#if 0 // hardcoded in tr_shade.c - Cowcat

	if ( strstr( glConfig.extensions_string, "GL_EXT_compiled_vertex_array" ) && ( glConfig.hardwareType != GLHW_RIVA128 ) )
	{
		if ( r_ext_compiled_vertex_array->integer )
		{
			qglLockArraysEXT = stub_glLockArraysEXT; 
			qglUnlockArraysEXT = stub_glUnlockArraysEXT;
 
			ri.Printf( PRINT_ALL, "...using GL_EXT_compiled_vertex_array\n" );
		}

		else
		{
			ri.Printf( PRINT_ALL, "...ignoring GL_EXT_compiled_vertex_array\n" );
		}
	}

	else
	#endif
	{
		//ri.Printf( PRINT_ALL, "...GL_EXT_compiled_vertex_array not found\n" );	
		ri.Printf( PRINT_ALL, "...using GL_EXT_compiled_vertex_array\n" ); 
	}
}

/* BUILD VERSION MARKER: bumped by hand on every rebuild, same permanent
 * convention as glquake's own MGLV3D_GLQUAKE_BUILD_VERSION (gl_vidamiga.c)
 * and Quake2's own REF_GL_BUILD_VERSION (gl_rmain.c) -- logged
 * unconditionally, first thing GLimp_Init does, so a captured
 * mglv3d_debug.log (or Sashimi/console output) can always be checked
 * against the exact source state that produced it. Must NOT be gated by
 * any logging-disable flag. Also baked into the output binary's own
 * filename (link_p5_main_gcc.sh) so the two always agree. */
#define QUAKE3_BUILD_VERSION 296

void GLimp_Init(void)
{
	char buf[1024];
	char buildbuf[64];

	sprintf(buildbuf, "===== quake3-68k-gcc BUILD VERSION %d =====\n", QUAKE3_BUILD_VERSION);
	mglv3d_log(buildbuf);
	ri.Printf(PRINT_ALL, "%s", buildbuf);

	ri.Printf(PRINT_ALL, "Initializing OpenGL subsystem\n");

	GLW_StartOpenGL();

	// get our config strings
	Q_strncpyz( glConfig.vendor_string, (char *)qglGetString (GL_VENDOR), sizeof( glConfig.vendor_string ) );
	Q_strncpyz( glConfig.renderer_string, (char *)qglGetString (GL_RENDERER), sizeof( glConfig.renderer_string ) );
	Q_strncpyz( glConfig.version_string, (char *)qglGetString (GL_VERSION), sizeof( glConfig.version_string ) );
	Q_strncpyz( glConfig.extensions_string, (char *)qglGetString (GL_EXTENSIONS), sizeof( glConfig.extensions_string ) );

	//
	// chipset specific configuration
	//

	Q_strncpyz( buf, glConfig.renderer_string, sizeof(buf) );
	Q_strlwr( buf );

	glConfig.driverType = GLDRV_ICD;
	glConfig.hardwareType = GLHW_GENERIC;

	if ( strstr( buf, "voodoo avenger" ) )
	{
		glConfig.hardwareType = GLHW_3DFX_2D3D;
	}
	
	if ( strstr( buf, "permedia2" ) )
	{
		glConfig.hardwareType = GLHW_PERMEDIA2;
	}
	
	GLW_InitExtensions();
	GLW_InitGamma();

	// mglGetWindowHandle() returns NULL in fullscreen mode (confirmed
	// twice already in this project: Quake2's MGLMainLoop crash, glquake's
	// keyboard-not-working bug) -- this call site dereferenced it
	// unchecked (win->Flags below). mglGetInputWindowHandle() is the
	// already-existing fix for this.
	win = mglGetInputWindowHandle();

	ModifyIDCMP(win, IDCMP_RAWKEY|IDCMP_MOUSEBUTTONS|IDCMP_MOUSEMOVE|IDCMP_DELTAMOVE);
	win->Flags |= WFLG_REPORTMOUSE;

	Sys_EventPort = win->UserPort;

	// Clear issued here, after window/IDCMP setup is fully done, matching
	// glquake's own proven-safe ordering (not at the end of
	// GLW_StartDriverAndSetMode, before the window/IDCMP exists).
	qglClearColor(0,0,0,1);
	qglClear(GL_COLOR_BUFFER_BIT);

	IN_Init(); // was in amiga_main/sys_init ---

	ri.Printf(PRINT_ALL, "... Sys_EventPort at %p\n", Sys_EventPort);
}

void GLimp_Shutdown(void)
{
	IN_Shutdown();

	GLW_RestoreGamma();

	GLW_Shutdown();
}

#if 0 // Cowcat
void GLimp_LogComment( char *comment ) 
{
	if ( glw_state.log_fp )
	{
		fprintf( glw_state.log_fp, "%s", comment );
	}
}
#endif


void GLimp_EndFrame(void)
{
#ifndef USE_MINIGL_LIBRARY
	/* A driver-internal global, so it exists only when the driver is linked
	 * into this executable. The resident library does not export it, and the
	 * stall it reports is handled inside the library instead. */
	extern int g_mglv3d_last_render_wait_result;
#endif

	if (r_finish->modified)
	{
		mglEnableSync(r_finish->integer);
		r_finish->modified = qfalse;
	}

	//mglUnlockDisplay(); // why if we are using SMART lock ? - Cowcat
	mglSwitchDisplay();

	/* A real V3D render-pipeline stall (control thread genuinely wedged)
	 * never recovers on its own -- every subsequent frame's deferred wait
	 * (MGLFlushPendingRender, context.c) would fail the same way forever,
	 * looking like a soft lockup. Exit through the standard fatal-error path,
	 * as Quake2 (gla_imp.c) and glquake (gl_vidamiga.c) do.
	 *
	 * Until 2026-09-11 this bailed out through S_Shutdown +
	 * mglEmergencyCloseScreen + Sys_Error, because on 2026-08-18 the full
	 * teardown Guru'd (81000005) twice on a real stall. That crash was never
	 * explained; the corruption sources found since (uninitialised CL packet
	 * bits, cold-boot memory) were live then and are fixed, and the same
	 * driver teardown, MGLDeleteContext, exits Quake2 and glquake cleanly on
	 * every stall. The emergency function is gone from the driver. */
#ifndef USE_MINIGL_LIBRARY
	if (g_mglv3d_last_render_wait_result != 0)
	{
		ri.Error(ERR_FATAL, "V3D render pipeline stalled (wait result=%d, 1=error 2=timeout) -- exiting cleanly rather than soft-locking\n",
			g_mglv3d_last_render_wait_result);
	}
#endif

	/*
	if (!r_fullscreen->integer && !in_nograb->integer)
	{
		static uint32 lastGrab = 0;
		uint32 now;

		// Capture the pointer in window mode, unless user wants nograb!
		now = Sys_Milliseconds();

		if (now - lastGrab > 1000)
		{
			IIntuition->SetWindowAttrs((struct Window *) mglGetWindowHandle(), WA_GrabFocus, 100, TAG_DONE);
			lastGrab = now;
		}
	}
	*/
}

