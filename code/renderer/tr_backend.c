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
along with Quake III Arena source code; if not, write to the Free Software
Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
===========================================================================
*/
#include "tr_local.h"

#ifdef AMIGAOS
#include <mgl/mglmacros.h>
#endif

backEndData_t	*backEndData;
backEndState_t	backEnd;

#if 0
static float s_flipMatrix[16] = 
{
	// convert from our coordinate system (looking down X)
	// to OpenGL's coordinate system (looking down -Z)
	0, 0, -1, 0,
	-1, 0, 0, 0,
	0, 1, 0, 0,
	0, 0, 0, 1
};
#endif

/*
** GL_Bind
*/
void GL_Bind( image_t *image )
{
	int texnum;

	if ( !image )
	{
		ri.Printf( PRINT_WARNING, "GL_Bind: NULL image\n" );
		texnum = tr.defaultImage->texnum;
	}

	else
	{
		texnum = image->texnum;
	}

	if ( r_nobind->integer && tr.dlightImage ) // performance evaluation option
	{	
		texnum = tr.dlightImage->texnum;
	}

	if ( glState.currenttextures[glState.currenttmu] != texnum )
	{
		if ( image )
			image->frameUsed = tr.frameCount;

		glState.currenttextures[glState.currenttmu] = texnum;
		qglBindTexture (GL_TEXTURE_2D, texnum);
	}
}

/*
** GL_SelectTexture
*/
void GL_SelectTexture( int unit )
{
	if ( glState.currenttmu == unit )
	{
		return;
	}

	if ( unit == 0 )
	{
		qglActiveTextureARB( GL_TEXTURE0_ARB );
		//GLimp_LogComment( "glActiveTextureARB( GL_TEXTURE0_ARB )\n" ); // Cowcat
		qglClientActiveTextureARB( GL_TEXTURE0_ARB );
		//GLimp_LogComment( "glClientActiveTextureARB( GL_TEXTURE0_ARB )\n" ); // Cowcat
	}

	else if ( unit == 1 )
	{
		qglActiveTextureARB( GL_TEXTURE1_ARB );
		//GLimp_LogComment( "glActiveTextureARB( GL_TEXTURE1_ARB )\n" ); // Cowcat
		qglClientActiveTextureARB( GL_TEXTURE1_ARB );
		//GLimp_LogComment( "glClientActiveTextureARB( GL_TEXTURE1_ARB )\n" ); // Cowcat
	}

	else
	{
		ri.Error( ERR_DROP, "GL_SelectTexture: unit = %i", unit );
	}

	glState.currenttmu = unit;
}


/*
** GL_BindMultitexture
*/
void GL_BindMultitexture( image_t *image0, GLuint env0, image_t *image1, GLuint env1 )
{
	int	texnum0, texnum1;

	texnum0 = image0->texnum;
	texnum1 = image1->texnum;

	if ( r_nobind->integer && tr.dlightImage )
	{
		// performance evaluation option
		texnum0 = texnum1 = tr.dlightImage->texnum;
	}

	if ( glState.currenttextures[1] != texnum1 )
	{
		GL_SelectTexture( 1 );
		image1->frameUsed = tr.frameCount;
		glState.currenttextures[1] = texnum1;
		qglBindTexture( GL_TEXTURE_2D, texnum1 );
	}

	if ( glState.currenttextures[0] != texnum0 )
	{
		GL_SelectTexture( 0 );
		image0->frameUsed = tr.frameCount;
		glState.currenttextures[0] = texnum0;
		qglBindTexture( GL_TEXTURE_2D, texnum0 );
	}
}


/*
** GL_Cull
*/
void GL_Cull( int cullType )
{
	if ( glState.faceCulling == cullType )
	{
		return;
	}

	glState.faceCulling = cullType;

	if ( cullType == CT_TWO_SIDED ) 
	{
		qglDisable( GL_CULL_FACE );
	}

	else 
	{
		qboolean cullFront;
		qglEnable( GL_CULL_FACE );
	
		cullFront = (cullType == CT_FRONT_SIDED);

		if ( backEnd.viewParms.isMirror )
		{
			cullFront = !cullFront;
		}

		qglCullFace( cullFront ? GL_FRONT : GL_BACK );
	}
}


/*
** GL_TexEnv
*/
void GL_TexEnv( GLint env ) // Quake3e cleanup
{
	if ( env == glState.texEnv[glState.currenttmu] )
	{
		return;
	}

	glState.texEnv[glState.currenttmu] = env;

	switch ( env )
	{
		case GL_MODULATE:
		case GL_REPLACE:
		case GL_DECAL:
		//case GL_ADD: // Cowcat
			qglTexEnvi( GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, env );
			break;

		default:
			/* Real vertex-array multitexture (2026-08-25, r_vertexLight=0
			 * stall/menu-hang investigation): print the offending shader's
			 * name -- tess.shader is the shader currently being drawn
			 * wherever GL_TexEnv gets called (DrawMultitextured, tr_shade.c,
			 * is the only caller that can pass a non-constant env), so this
			 * pinpoints exactly which shader has an invalid/unset
			 * multitextureEnv instead of just the bad value itself. */
			ri.Error( ERR_DROP, "GL_TexEnv: invalid env '%d' passed (shader: %s)\n", env,
				tess.shader ? tess.shader->name : "(null)" );
			break;
	}
}

/*
** GL_State
**
** This routine is responsible for setting the most commonly changed state
** in Q3.
*/

#if defined(__VBCC__) // && defined(__PPC__)
qboolean smoothshade = qfalse; // Cowcat
#endif

void GL_State( unsigned long stateBits )
{
	unsigned long diff = stateBits ^ glState.glStateBits;

	if ( !diff )
	{
		return;
	}

	//
	// check depthFunc bits
	//
	if ( diff & GLS_DEPTHFUNC_EQUAL )
	{
		if ( stateBits & GLS_DEPTHFUNC_EQUAL )
		{
			qglDepthFunc( GL_EQUAL );
		}

		else
		{
			qglDepthFunc( GL_LEQUAL );
		}
	}

	//
	// check blend bits
	//
	if ( diff & ( GLS_SRCBLEND_BITS | GLS_DSTBLEND_BITS ) )
	{
		GLenum srcFactor = GL_ONE, dstFactor = GL_ONE;

		#if defined(__VBCC__) // && defined(__PPC__) // minigl vbcc blending workaround - Cowcat

		qboolean flat = qfalse;

		qboolean smoothcheck = smoothshade;
		
		if(!smoothcheck) // already done (rb_surfacerailcore/rb_renderflares)
		{
			qglShadeModel(GL_SMOOTH);
		}

		#endif
	
		if ( stateBits & ( GLS_SRCBLEND_BITS | GLS_DSTBLEND_BITS ) )
		{
			switch ( stateBits & GLS_SRCBLEND_BITS )
			{
				case GLS_SRCBLEND_ZERO:
					srcFactor = GL_ZERO;
					break;

				case GLS_SRCBLEND_ONE:
					srcFactor = GL_ONE;
				#if defined(__VBCC__) // && defined(__PPC__)
					flat = qtrue;
				#endif
					break;

				case GLS_SRCBLEND_DST_COLOR:
					srcFactor = GL_DST_COLOR;
					break;

				case GLS_SRCBLEND_ONE_MINUS_DST_COLOR:
					srcFactor = GL_ONE_MINUS_DST_COLOR;
					break;

				case GLS_SRCBLEND_SRC_ALPHA:
					srcFactor = GL_SRC_ALPHA;
					break;

				case GLS_SRCBLEND_ONE_MINUS_SRC_ALPHA:
					srcFactor = GL_ONE_MINUS_SRC_ALPHA;
					break;

				case GLS_SRCBLEND_DST_ALPHA:
					srcFactor = GL_DST_ALPHA;
					break;

				case GLS_SRCBLEND_ONE_MINUS_DST_ALPHA:
					srcFactor = GL_ONE_MINUS_DST_ALPHA;
					break;

				case GLS_SRCBLEND_ALPHA_SATURATE:
					srcFactor = GL_SRC_ALPHA_SATURATE;
					break;

				default:
					ri.Error( ERR_DROP, "GL_State: invalid src blend state bits\n" );
					break;
			}

			switch ( stateBits & GLS_DSTBLEND_BITS )
			{
				case GLS_DSTBLEND_ZERO:
					dstFactor = GL_ZERO;
					break;

				case GLS_DSTBLEND_ONE:
					dstFactor = GL_ONE;
					break;

				case GLS_DSTBLEND_SRC_COLOR:
					dstFactor = GL_SRC_COLOR;
					break;

				case GLS_DSTBLEND_ONE_MINUS_SRC_COLOR:
					dstFactor = GL_ONE_MINUS_SRC_COLOR;
					break;

				case GLS_DSTBLEND_SRC_ALPHA:
					dstFactor = GL_SRC_ALPHA;
					break;

				case GLS_DSTBLEND_ONE_MINUS_SRC_ALPHA:
					dstFactor = GL_ONE_MINUS_SRC_ALPHA;
					break;

				case GLS_DSTBLEND_DST_ALPHA:
					dstFactor = GL_DST_ALPHA;
					break;

				case GLS_DSTBLEND_ONE_MINUS_DST_ALPHA:
					dstFactor = GL_ONE_MINUS_DST_ALPHA;
					break;

				default:
					ri.Error( ERR_DROP, "GL_State: invalid dst blend state bits\n" );
					break;
			}

			#if defined(__VBCC__) // && defined(__PPC__) // minigl vbcc blending workaround - Cowcat

			// railgun sync - Cowcat
			if ( flat != smoothcheck && dstFactor == GL_ONE )
			{
				qglShadeModel(GL_FLAT);
			}

			smoothshade = qfalse;

			#endif

			qglEnable( GL_BLEND );
			qglBlendFunc( srcFactor, dstFactor );
		}

		else
		{
			qglDisable( GL_BLEND );
		}
	}

	//
	// check depthmask
	//
	if ( diff & GLS_DEPTHMASK_TRUE )
	{
		if ( stateBits & GLS_DEPTHMASK_TRUE )
		{
			qglDepthMask( GL_TRUE );
		}

		else
		{
			qglDepthMask( GL_FALSE );
		}
	}

	//
	// fill/line mode
	//
	#if 0 // Cowcat
	if ( diff & GLS_POLYMODE_LINE )
	{
		if ( stateBits & GLS_POLYMODE_LINE )
		{
			qglPolygonMode( GL_FRONT_AND_BACK, GL_LINE );
		}

		else
		{
			qglPolygonMode( GL_FRONT_AND_BACK, GL_FILL );
		}
	}
	#endif

	//
	// depthtest
	//
	if ( diff & GLS_DEPTHTEST_DISABLE )
	{
		if ( stateBits & GLS_DEPTHTEST_DISABLE )
		{
			qglDisable( GL_DEPTH_TEST );
		}

		else
		{
			qglEnable( GL_DEPTH_TEST );
		}
	}

	//
	// alpha test
	//
	if ( diff & GLS_ATEST_BITS )
	{
		switch ( stateBits & GLS_ATEST_BITS )
		{
			case 0:
				qglDisable( GL_ALPHA_TEST );
				break;

			case GLS_ATEST_GT_0:
				qglEnable( GL_ALPHA_TEST );
				qglAlphaFunc( GL_GREATER, 0.0f );
				break;

			case GLS_ATEST_LT_80:
				qglEnable( GL_ALPHA_TEST );
				qglAlphaFunc( GL_LESS, 0.5f );
				break;

			case GLS_ATEST_GE_80:
				qglEnable( GL_ALPHA_TEST );
				qglAlphaFunc( GL_GEQUAL, 0.5f );
				break;

			default:
				assert( 0 );
				break;
		}
	}

	glState.glStateBits = stateBits;
}



/*
================
RB_Hyperspace

A player has predicted a teleport, but hasn't arrived yet
================
*/
static void RB_Hyperspace( void )
{
	float	c;

	if ( !backEnd.isHyperspace )
	{
		// do initialization shit
	}

	c = ( backEnd.refdef.time & 255 ) / 255.0f;
	qglClearColor( c, c, c, 1 );
	qglClear( GL_COLOR_BUFFER_BIT );

	backEnd.isHyperspace = qtrue;
}

static void SetViewportAndScissor( void )
{
	qglMatrixMode(GL_PROJECTION);
	qglLoadMatrixf( backEnd.viewParms.projectionMatrix );
	qglMatrixMode(GL_MODELVIEW);

	// set the window clipping
	qglViewport( backEnd.viewParms.viewportX, backEnd.viewParms.viewportY, backEnd.viewParms.viewportWidth, backEnd.viewParms.viewportHeight );
	qglScissor( backEnd.viewParms.viewportX, backEnd.viewParms.viewportY, backEnd.viewParms.viewportWidth, backEnd.viewParms.viewportHeight );
}

/*
=================
RB_BeginDrawingView

Any mirrored or portaled views have already been drawn, so prepare
to actually render the visible surfaces for this view
=================
*/
static void RB_BeginDrawingView (void)
{
	int clearBits = 0;

	/* A qglMGLForceNewRenderPass() call used to sit here, keyed to
	 * backEnd.viewParms.forceNewRenderPass, to make the view resuming after a
	 * portal/mirror recursion (R_MirrorViewBySurface, tr_main.c) start a
	 * genuine new render pass -- otherwise its depth clear silently merged
	 * into the portal's still-open pass. Removed 2026-09-05 (v247): MiniGLV3D
	 * now splits the pass from glClear itself for any mid-frame clear with
	 * geometry behind it, reusing the very same machinery that call set. This
	 * view's own qglClear below is full-screen (a mirror view's viewport is
	 * the whole screen, so SetViewportAndScissor leaves the scissor at full
	 * drawable), so it takes the split path and the explicit call was
	 * redundant. Hardware-confirmed on Q3DM0: the mirror is unchanged without
	 * it. Portals therefore work through STANDARD glClear now, not a
	 * MiniGLV3D-only entry point.
	 *
	 * viewParms_t.forceNewRenderPass and R_MirrorViewBySurface's assignment
	 * to it are deliberately left in place -- harmless, and they document
	 * which view this is for anyone re-deriving the mechanism later. */

	/*
	 * Read BOTH arms together -- the second one is easy to miss, and misreading
	 * it inverts what this renderer does. RE_BeginFrame clears finishCalled each
	 * frame, and RB_SwapBuffers finishes only `if (!glState.finishCalled)`. So:
	 *
	 *   r_finish 1, 3D frame  -> qglFinish() here, at the start of the view.
	 *   r_finish 0, 3D frame  -> flag set WITHOUT finishing, and the swap then
	 *                            skips its finish too: the frame finishes
	 *                            NOWHERE and runs fully pipelined.
	 *   2D-only frame         -> this function never runs, the flag stays false,
	 *                            so the SWAP drains. Loading screens are 2D,
	 *                            which is the only reason a level load never
	 *                            stalls on a bitmap lock held across the gap.
	 *
	 * r_finish is therefore a runtime pipelining A/B with no rebuild. Measured
	 * 2026-09-28: 45 fps either way, so the GPU render is already done by the
	 * time the next view starts.
	 */
	if ( r_finish->integer == 1 && !glState.finishCalled )
	{
		qglFinish ();
		glState.finishCalled = qtrue;
	}

	if ( r_finish->integer == 0 )
	{
		glState.finishCalled = qtrue;
	}

	// we will need to change the projection matrix before drawing
	// 2D images again
	backEnd.projection2D = qfalse;
	//qglEnable(MGL_PERSPECTIVE_MAPPING); // TEST for minigl - Cowcat

	//
	// set the modelview matrix for the viewer
	//
	SetViewportAndScissor();

	// ensures that depth writes are enabled for the depth clear
	GL_State( GLS_DEFAULT );

	// clear relevant buffers
	clearBits = GL_DEPTH_BUFFER_BIT;

	#if 0 // Cowcat
	if ( r_measureOverdraw->integer || r_shadows->integer == 2 )
	{
		clearBits |= GL_STENCIL_BUFFER_BIT; 
	}
	#endif

	if ( 0 && r_fastsky->integer && !( backEnd.refdef.rdflags & RDF_NOWORLDMODEL ) ) // Quake3e - 0 &&
	{
		clearBits |= GL_COLOR_BUFFER_BIT;	// FIXME: only if sky shaders have been used
#ifdef _DEBUG
		qglClearColor( 0.8f, 0.7f, 0.4f, 1.0f );	// FIXME: get color of sky
#else
		qglClearColor( 0.0f, 0.0f, 0.0f, 1.0f );	// FIXME: get color of sky
#endif
	}

	/* TEMPORARY (2026-08-21), until the ui_atoms.c background quad is
	 * reinstated: RDF_NOWORLDMODEL means "not rendering the world (icons,
	 * menus, etc)" -- no world/backdrop underneath a color clear could
	 * ever clobber. UI_Refresh's own per-frame RenderScene call (the
	 * original smearing fix) resets projection2D every frame, but that
	 * alone never did the actual visual refresh -- the background quad's
	 * own full-screen color draw did, by consuming that reset state.
	 * With the background quad temporarily disabled for a diagnostic,
	 * nothing performs that refresh any more, so do it here instead, via
	 * a real clear. Revert once the background quad is back. */
	/* SCOPED to full-screen views (2026-09-05). The block below was written
	 * when MiniGLV3D's glClear ignored the scissor box entirely, so this
	 * colour clear was always full-screen and applied retroactively at tile
	 * load -- which is why its own comment above could claim no backdrop
	 * underneath could ever be clobbered. The driver now honours the scissor
	 * properly (milestone_l24_scissored_clear), so an RDF_NOWORLDMODEL view
	 * with a SMALL viewport really does clear just its own rect -- and
	 * CG_Draw3DModel's HUD 3D icons are exactly that, several per frame.
	 * Hardware-reported: every icon gained a black box behind it.
	 *
	 * The full-screen menu case this was added for is unaffected: those
	 * views set the refdef to the whole screen (ui_atoms.c, itself marked
	 * TEMPORARY for the same reason). Removing both and reinstating the
	 * ui_atoms.c background quad is still the documented proper cleanup --
	 * this only stops the icons being damaged in the meantime. */
	if ( ( backEnd.refdef.rdflags & RDF_NOWORLDMODEL )
	     && backEnd.viewParms.viewportWidth  >= glConfig.vidWidth
	     && backEnd.viewParms.viewportHeight >= glConfig.vidHeight )
	{
		clearBits |= GL_COLOR_BUFFER_BIT;
		qglClearColor( 0.0f, 0.0f, 0.0f, 1.0f );
	}

	qglClear( clearBits );

	if ( ( backEnd.refdef.rdflags & RDF_HYPERSPACE ) )
	{
		RB_Hyperspace();
		return;
	}

	else
	{
		backEnd.isHyperspace = qfalse;
	}

	glState.faceCulling = -1;		// force face culling to set next time

	// we will only draw a sun if there was sky rendered in this view
	backEnd.skyRenderedThisView = qfalse;

	#if 0 // Cowcat - workaround in tr_main/R_SetupProjectionZ

	// clip to the plane of the portal
	if ( backEnd.viewParms.isPortal )
	{
		float	plane[4];
		double	plane2[4];

		plane[0] = backEnd.viewParms.portalPlane.normal[0];
		plane[1] = backEnd.viewParms.portalPlane.normal[1];
		plane[2] = backEnd.viewParms.portalPlane.normal[2];
		plane[3] = backEnd.viewParms.portalPlane.dist;

		plane2[0] = DotProduct (backEnd.viewParms.or.axis[0], plane);
		plane2[1] = DotProduct (backEnd.viewParms.or.axis[1], plane);
		plane2[2] = DotProduct (backEnd.viewParms.or.axis[2], plane);
		plane2[3] = DotProduct (plane, backEnd.viewParms.or.origin) - plane[3];

		qglLoadMatrixf( s_flipMatrix );
		qglClipPlane (GL_CLIP_PLANE0, plane2);
		qglEnable (GL_CLIP_PLANE0);
	}

	else
	{
		qglDisable (GL_CLIP_PLANE0);
	}

	#endif
}


/*
==================
RB_RenderDrawSurfList
==================
*/
static void RB_RenderDrawSurfList( drawSurf_t *drawSurfs, int numDrawSurfs )
{
	shader_t	*shader, *oldShader;
	int		fogNum, oldFogNum;
	int		entityNum, oldEntityNum;
	int		dlighted, oldDlighted;
	qboolean	depthRange, oldDepthRange, isCrosshair, wasCrosshair;
	int		i;
	drawSurf_t	*drawSurf;
	int		oldSort;
	float		originalTime;

	// save original time for entity shader offsets
	originalTime = backEnd.refdef.floatTime;

	// clear the z buffer, set the modelview, etc
	RB_BeginDrawingView ();

	// draw everything
	oldEntityNum = -1;
	backEnd.currentEntity = &tr.worldEntity;
	oldShader = NULL;
	oldFogNum = -1;
	oldDepthRange = qfalse;
	wasCrosshair = qfalse;
	oldDlighted = qfalse;
	oldSort = -1;
	depthRange = qfalse;

	backEnd.pc.c_surfaces += numDrawSurfs;

	for (i = 0, drawSurf = drawSurfs ; i < numDrawSurfs ; i++, drawSurf++)
	{
		if ( drawSurf->sort == oldSort )
		{
			// fast path, same as previous sort
			rb_surfaceTable[ *drawSurf->surface ]( drawSurf->surface );
			continue;
		}

		oldSort = drawSurf->sort;
		R_DecomposeSort( drawSurf->sort, &entityNum, &shader, &fogNum, &dlighted );

		//
		// change the tess parameters if needed
		// a "entityMergable" shader is a shader that can have surfaces from seperate
		// entities merged into a single batch, like smoke and blood puff sprites

		if ( shader != NULL && ( shader != oldShader || fogNum != oldFogNum || dlighted != oldDlighted 
			|| ( entityNum != oldEntityNum && !shader->entityMergable ) ) )
		{
			if (oldShader != NULL)
			{
				RB_EndSurface();
			}

			RB_BeginSurface( shader, fogNum );
			oldShader = shader;
			oldFogNum = fogNum;
			oldDlighted = dlighted;
		}

		//
		// change the modelview matrix if needed
		//
		if ( entityNum != oldEntityNum )
		{
			depthRange = isCrosshair = qfalse;

			if ( entityNum != ENTITYNUM_WORLD )
			{
				backEnd.currentEntity = &backEnd.refdef.entities[entityNum];
				backEnd.refdef.floatTime = originalTime - backEnd.currentEntity->e.shaderTime;

				// we have to reset the shaderTime as well otherwise image animations start
				// from the wrong frame
				tess.shaderTime = backEnd.refdef.floatTime - tess.shader->timeOffset;

				// set up the transformation matrix
				R_RotateForEntity( backEnd.currentEntity, &backEnd.viewParms, &backEnd.or );

				// set up the dynamic lighting if needed
				if ( backEnd.currentEntity->needDlights )
				{
					R_TransformDlights( backEnd.refdef.num_dlights, backEnd.refdef.dlights, &backEnd.or );
				}

				if(backEnd.currentEntity->e.renderfx & RF_DEPTHHACK)
				{
					// hack the depth range to prevent view model from poking into walls
					depthRange = qtrue;
					
					if(backEnd.currentEntity->e.renderfx & RF_CROSSHAIR)
						isCrosshair = qtrue;
				}
			}

			else
			{
				backEnd.currentEntity = &tr.worldEntity;
				backEnd.refdef.floatTime = originalTime;
				backEnd.or = backEnd.viewParms.world;

				// we have to reset the shaderTime as well otherwise image animations on
				// the world (like water) continue with the wrong frame
				tess.shaderTime = backEnd.refdef.floatTime - tess.shader->timeOffset;

				R_TransformDlights( backEnd.refdef.num_dlights, backEnd.refdef.dlights, &backEnd.or );
			}

			qglLoadMatrixf( backEnd.or.modelMatrix );

			//
			// change depthrange. Also change projection matrix so first person weapon does not look like coming
			// out of the screen.
			//
			if (oldDepthRange != depthRange || wasCrosshair != isCrosshair)
			{
				if (depthRange)
				{
					if(backEnd.viewParms.stereoFrame != STEREO_CENTER)
					{
						if(isCrosshair)
						{
							if(oldDepthRange)
							{
								// was not a crosshair but now is, change back proj matrix
								qglMatrixMode(GL_PROJECTION);
								qglLoadMatrixf(backEnd.viewParms.projectionMatrix);
								qglMatrixMode(GL_MODELVIEW);
							}
						}

						else
						{
							viewParms_t temp = backEnd.viewParms;

							R_SetupProjection(&temp, r_znear->value, qfalse);

							qglMatrixMode(GL_PROJECTION);
							qglLoadMatrixf(temp.projectionMatrix);
							qglMatrixMode(GL_MODELVIEW);
						}
					}

					if(!oldDepthRange)
						qglDepthRange (0, 0.3);
				}

				else
				{
					if(!wasCrosshair && backEnd.viewParms.stereoFrame != STEREO_CENTER)
					{
						qglMatrixMode(GL_PROJECTION);
						qglLoadMatrixf(backEnd.viewParms.projectionMatrix);
						qglMatrixMode(GL_MODELVIEW);
					}

					qglDepthRange (0, 1);
				}

				oldDepthRange = depthRange;
				wasCrosshair = isCrosshair;
			}

			oldEntityNum = entityNum;
		}

		// add the triangles for this surface
		rb_surfaceTable[ *drawSurf->surface ]( drawSurf->surface );
	}

	backEnd.refdef.floatTime = originalTime;

	// draw the contents of the last shader batch
	if (oldShader != NULL)
	{
		RB_EndSurface();
	}

	// go back to the world modelview matrix
	qglLoadMatrixf( backEnd.viewParms.world.modelMatrix );

	if ( depthRange )
	{
		qglDepthRange (0, 1);
	}

	if ( r_drawSun->integer )
	{
		RB_DrawSun(0.1, tr.sunShader);
	}

	// darken down any stencil shadows
	RB_ShadowFinish();		

	// add light flares on lights that aren't obscured
	RB_RenderFlares();
}


/*
============================================================================

RENDER BACK END THREAD FUNCTIONS

============================================================================
*/

/*
================
RB_SetGL2D

================
*/
void RB_SetGL2D (void)
{
	backEnd.projection2D = qtrue;

	// set 2D virtual screen size
	qglViewport( 0, 0, glConfig.vidWidth, glConfig.vidHeight);
	qglScissor( 0, 0, glConfig.vidWidth, glConfig.vidHeight);

	qglMatrixMode(GL_PROJECTION);
	qglLoadIdentity ();
	qglOrtho (0, glConfig.vidWidth, glConfig.vidHeight, 0, 0, 1);
	qglMatrixMode(GL_MODELVIEW);
	qglLoadIdentity ();

	GL_State( GLS_DEPTHTEST_DISABLE | GLS_SRCBLEND_SRC_ALPHA | GLS_DSTBLEND_ONE_MINUS_SRC_ALPHA );

	GL_Cull(CT_TWO_SIDED);

	//qglDisable( GL_CLIP_PLANE0 ); // Cowcat

	/* FIX (2026-08-18): glquake's own proven, hardware-confirmed AmigaOS
	 * 2D setup (gl_draw.c's GL_Set2D) unconditionally disables
	 * MGL_PERSPECTIVE_MAPPING for all 2D drawing -- Cowcat's own
	 * commented-out line here ("TEST for minigl") suggests this was
	 * already suspected but never enabled. MGL_FLATFAN (used by
	 * RE_StretchRaw for cinematics, and other 2D screen-space quads)
	 * bypasses the normal transform pipeline and expects affine, not
	 * perspective-correct, texture mapping -- leaving perspective
	 * mapping on for 2D draws is exactly the kind of mismatch that
	 * produces garbled/invisible output with no error anywhere. */
	#if defined(AMIGAOS)
	qglDisable(MGL_PERSPECTIVE_MAPPING);
	#endif

	// set time for 2D shaders
	backEnd.refdef.time = ri.Milliseconds();
	backEnd.refdef.floatTime = backEnd.refdef.time * 0.001f;
}


/*
=============
RE_StretchRaw

FIXME: not exactly backend
Stretches a raw 32 bit power of 2 bitmap image over the given screen rectangle.
Used for cinematics.
=============
*/
void RE_StretchRaw (int x, int y, int w, int h, int cols, int rows, const byte *data, int client, qboolean dirty)
{
	int	i, j;
	int	start, end;

	if ( !tr.registered )
	{
		return;
	}

	R_IssuePendingRenderCommands();

	if(tess.numIndexes)
		RB_EndSurface();

	// we definately want to sync every frame for the cinematics
	qglFinish();
	
	start = 0;

	if ( r_speeds->integer )
	{
		start = ri.Milliseconds();
	}

	// make sure rows and cols are powers of 2
	for ( i = 0 ; ( 1 << i ) < cols ; i++ ) {
	}

	for ( j = 0 ; ( 1 << j ) < rows ; j++ ) {
	}

	if ( ( 1 << i ) != cols || ( 1 << j ) != rows)
	{
		ri.Error (ERR_DROP, "Draw_StretchRaw: size not a power of 2: %i by %i", cols, rows);
	}

	RE_UploadCinematic( w, h, cols, rows, data, client, dirty);

	if ( r_speeds->integer )
	{
		end = ri.Milliseconds();
		ri.Printf( PRINT_ALL, "qglTexSubImage2D %i, %i: %i msec\n", cols, rows, end - start );
	}

	RB_SetGL2D();

	/* FIX (2026-08-18, black-screen root cause): R_Init (tr_init.c) enables
	 * GL_VERTEX_ARRAY client state once, permanently, for this engine's own
	 * tessellated rendering -- but MiniGLV3D's GLEnd (vertexbuffer_min.c)
	 * treats ANY active GLCS_VERTEX client state as "this glBegin/End
	 * sequence used glArrayElement", dispatching through GLDrawElements/
	 * ElementIndex instead of the normal immediate-mode CurrentDraw path --
	 * for every plain glVertex-based sequence, forever, once R_Init has
	 * run. Disabling it around this function's own plain-glVertex2f
	 * sequences (never glArrayElement) restores the intended immediate-mode
	 * dispatch; re-enabled below before returning so the engine's own
	 * tessellated draws keep working exactly as before. */
	qglDisableClientState( GL_VERTEX_ARRAY );

	qglColor3f( tr.identityLight, tr.identityLight, tr.identityLight );

	/* CHANGED (2026-08-18, cinematic zoom/color investigation): was
	 * MGL_FLATFAN on AMIGAOS -- switched to GL_QUADS unconditionally, per
	 * direct user request. MGL_FLATFAN is a documented recurring
	 * fragility in this project (see
	 * feedback_mgl_flatfan_recurring_fragility) with THREE caller-side
	 * invariants (transform-bypass awareness, perspective-mapping
	 * disabled, LIVE not frozen viewport) -- the first two were already
	 * confirmed satisfied here, but the third (live-vs-frozen viewport,
	 * draw.c's own 2026-08-05/08-08 revert history) was never
	 * specifically checked for this exact call sequence. GL_QUADS
	 * sidesteps all three invariants entirely -- it's already
	 * hardware-confirmed correct in THIS exact investigation (the
	 * earlier green/cyan GL_QUADS test quads both rendered correctly),
	 * going through the normal transform pipeline instead of FLATFAN's
	 * special-cased shortcut. */
	qglBegin (GL_QUADS);

	qglTexCoord2f ( 0.5f / cols,  0.5f / rows );
	qglVertex2f (x, y);
	qglTexCoord2f ( ( cols - 0.5f ) / cols ,  0.5f / rows );
	qglVertex2f (x+w, y);
	qglTexCoord2f ( ( cols - 0.5f ) / cols, ( rows - 0.5f ) / rows );
	qglVertex2f (x+w, y+h);
	qglTexCoord2f ( 0.5f / cols, ( rows - 0.5f ) / rows );
	qglVertex2f (x, y+h);
	qglEnd ();

	/* Restore -- re-enable GL_VERTEX_ARRAY client state so this engine's
	 * own tessellated draws (which expect it permanently on) keep working
	 * exactly as before. See the disable's own comment above. */
	qglEnableClientState( GL_VERTEX_ARRAY );
}

	
void RE_UploadCinematic (int w, int h, int cols, int rows, const byte *data, int client, qboolean dirty)
{
	image_t *image = tr.scratchImage[client];

	GL_Bind( image );

	// if the scratchImage isn't in the format we want, specify it as a new texture
	if ( cols != image->width || rows != image->height )
	{
		image->width = image->uploadWidth = cols;
		image->height = image->uploadHeight = rows;

		#if defined(AMIGAOS)
		qglTexImage2D( GL_TEXTURE_2D, 0, GL_RGBA, cols, rows, 0, GL_RGBA, GL_UNSIGNED_BYTE, data );
		#else
		qglTexImage2D( GL_TEXTURE_2D, 0, GL_RGB8, cols, rows, 0, GL_RGBA, GL_UNSIGNED_BYTE, data );
		#endif
		
		qglTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR );
		qglTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR );
		qglTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP /*_TO_EDGE*/ ); // Cowcat
		qglTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP /*_TO_EDGE*/ ); // Cowcat
	}

	else
	{
		if (dirty)
		{
			// otherwise, just subimage upload it so that drivers can tell we are going to be changing
			// it and don't try and do a texture compression
			qglTexSubImage2D( GL_TEXTURE_2D, 0, 0, 0, cols, rows, GL_RGBA, GL_UNSIGNED_BYTE, data );
		}
	}
}


/*
=============
RB_SetColor

=============
*/
static const void *RB_SetColor( const void *data )
{
	const setColorCommand_t	*cmd;

	cmd = (const setColorCommand_t *)data;

	backEnd.color2D[0] = cmd->color[0] * 255;
	backEnd.color2D[1] = cmd->color[1] * 255;
	backEnd.color2D[2] = cmd->color[2] * 255;
	backEnd.color2D[3] = cmd->color[3] * 255;

	return (const void *)(cmd + 1);
}

/*
=============
RB_StretchPic
=============
*/
static const void *RB_StretchPic ( const void *data )
{
	const stretchPicCommand_t	*cmd;
	shader_t 			*shader;
	int				numVerts, numIndexes;

	cmd = (const stretchPicCommand_t *)data;

	/* FIX (2026-08-21, screen-not-clearing-on-non-main-menu-screens
	 * investigation): was `if (!backEnd.projection2D) RB_SetGL2D();` --
	 * projection2D only ever resets to qfalse via a real 3D scene render
	 * (RB_BeginDrawingView). Any UI screen that never submits a 3D scene
	 * (i.e. every menu except the main menu, which renders its banner
	 * model every frame) would call RB_SetGL2D() exactly once, ever --
	 * the first 2D draw after startup -- and never again for the rest
	 * of the session, no matter how many other menus got visited after.
	 * Whatever RB_SetGL2D() sets up (viewport/scissor/ortho projection)
	 * needs to be genuinely re-issued every frame on this hardware, not
	 * cached as a one-time mode switch -- unconditional now. */
	RB_SetGL2D();

	shader = cmd->shader;

	if ( shader != tess.shader )
	{
		if ( tess.numIndexes )
		{
			RB_EndSurface();
		}

		backEnd.currentEntity = &backEnd.entity2D;
		RB_BeginSurface( shader, 0 );
	}
	
	RB_CHECKOVERFLOW( 4, 6 );

	numVerts = tess.numVertexes;
	numIndexes = tess.numIndexes;

	tess.numVertexes += 4;
	tess.numIndexes += 6;

	tess.indexes[ numIndexes ] = numVerts + 3;
	tess.indexes[ numIndexes + 1 ] = numVerts + 0;
	tess.indexes[ numIndexes + 2 ] = numVerts + 2;
	tess.indexes[ numIndexes + 3 ] = numVerts + 2;
	tess.indexes[ numIndexes + 4 ] = numVerts + 0;
	tess.indexes[ numIndexes + 5 ] = numVerts + 1;

	*(int *)tess.vertexColors[ numVerts ] =
		*(int *)tess.vertexColors[ numVerts + 1 ] =
		*(int *)tess.vertexColors[ numVerts + 2 ] =
		*(int *)tess.vertexColors[ numVerts + 3 ] = *(int *)backEnd.color2D;

	tess.xyz[ numVerts ][0] = cmd->x;
	tess.xyz[ numVerts ][1] = cmd->y;
	tess.xyz[ numVerts ][2] = 0;

	tess.texCoords[ numVerts ][0][0] = cmd->s1;
	tess.texCoords[ numVerts ][0][1] = cmd->t1;

	tess.xyz[ numVerts + 1 ][0] = cmd->x + cmd->w;
	tess.xyz[ numVerts + 1 ][1] = cmd->y;
	tess.xyz[ numVerts + 1 ][2] = 0;

	tess.texCoords[ numVerts + 1 ][0][0] = cmd->s2;
	tess.texCoords[ numVerts + 1 ][0][1] = cmd->t1;

	tess.xyz[ numVerts + 2 ][0] = cmd->x + cmd->w;
	tess.xyz[ numVerts + 2 ][1] = cmd->y + cmd->h;
	tess.xyz[ numVerts + 2 ][2] = 0;

	tess.texCoords[ numVerts + 2 ][0][0] = cmd->s2;
	tess.texCoords[ numVerts + 2 ][0][1] = cmd->t2;

	tess.xyz[ numVerts + 3 ][0] = cmd->x;
	tess.xyz[ numVerts + 3 ][1] = cmd->y + cmd->h;
	tess.xyz[ numVerts + 3 ][2] = 0;

	tess.texCoords[ numVerts + 3 ][0][0] = cmd->s1;
	tess.texCoords[ numVerts + 3 ][0][1] = cmd->t2;

	return (const void *)(cmd + 1);
}

/*
=============
RB_DrawSurfs

=============
*/
static const void *RB_DrawSurfs( const void *data )
{
	const drawSurfsCommand_t	*cmd;

	// finish any 2D drawing if needed
	if ( tess.numIndexes )
	{
		RB_EndSurface();
	}

	cmd = (const drawSurfsCommand_t *)data;

	backEnd.refdef = cmd->refdef;
	backEnd.viewParms = cmd->viewParms;

	RB_RenderDrawSurfList( cmd->drawSurfs, cmd->numDrawSurfs );

	return (const void *)(cmd + 1);
}


/*
=============
RB_DrawBuffer

=============
*/
static const void *RB_DrawBuffer( const void *data )
{
	const drawBufferCommand_t	*cmd;

	cmd = (const drawBufferCommand_t *)data;

	#if !defined(AMIGAOS)
	qglDrawBuffer( cmd->buffer );
	#endif

	// clear screen for debugging
	if ( r_clear->integer )
	{
		qglClearColor( 1, 0, 0.5, 1 );
		qglClear( GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT );
	}

	return (const void *)(cmd + 1);
}

/*
===============
RB_ShowImages

Draw all the images to the screen, on top of whatever
was there.  This is used to test for texture thrashing.

Also called by RE_EndRegistration
===============
*/
void RB_ShowImages( void )
{
	int	i;
	image_t	*image;
	float	x, y, w, h;
	int	start, end;

	// see RB_StretchPic's own comment on the same fix
	RB_SetGL2D();

	qglClear( GL_COLOR_BUFFER_BIT );

	qglFinish();

	start = ri.Milliseconds();

	for ( i=0 ; i<tr.numImages ; i++ )
	{
		image = tr.images[i];

		w = glConfig.vidWidth / 20;
		h = glConfig.vidHeight / 15;
		x = i % 20 * w;
		y = i / 20 * h;

		// show in proportional size in mode 2
		if ( r_showImages->integer == 2 )
		{
			w *= image->uploadWidth / 512.0f;
			h *= image->uploadHeight / 512.0f;
		}

		GL_Bind( image );

		qglBegin (GL_QUADS);
		qglTexCoord2f( 0, 0 );
		qglVertex2f( x, y );
		qglTexCoord2f( 1, 0 );
		qglVertex2f( x + w, y );
		qglTexCoord2f( 1, 1 );
		qglVertex2f( x + w, y + h );
		qglTexCoord2f( 0, 1 );
		qglVertex2f( x, y + h );
		qglEnd();
	}

	qglFinish();

	end = ri.Milliseconds();
	ri.Printf( PRINT_ALL, "%i msec to draw all images\n", end - start );
}

/*
=============
RB_ColorMask

=============
*/
static const void *RB_ColorMask(const void *data)
{
	const colorMaskCommand_t *cmd = data;
	
	qglColorMask(cmd->rgba[0], cmd->rgba[1], cmd->rgba[2], cmd->rgba[3]);
	
	return (const void *)(cmd + 1);
}

/*
=============
RB_ClearDepth

=============
*/
static const void *RB_ClearDepth(const void *data)
{
	const clearDepthCommand_t *cmd = data;

	if(tess.numIndexes)
		RB_EndSurface();

	// texture swapping test
	if (r_showImages->integer)
		RB_ShowImages();

	qglClear(GL_DEPTH_BUFFER_BIT);
	
	return (const void *)(cmd + 1);
}

/*
=============
RB_SwapBuffers

=============
*/
static const void *RB_SwapBuffers( const void *data )
{
	const swapBuffersCommand_t	*cmd;

	// finish any 2D drawing if needed
	if ( tess.numIndexes )
	{
		RB_EndSurface();
	}

	// texture swapping test
	if ( r_showImages->integer )
	{
		RB_ShowImages();
	}

	cmd = (const swapBuffersCommand_t *)data;

	#if 0 // Cowcat
	// we measure overdraw by reading back the stencil buffer and
	// counting up the number of increments that have happened
	if ( r_measureOverdraw->integer )
	{
		int i;
		long sum = 0;
		unsigned char *stencilReadback;

		stencilReadback = ri.Hunk_AllocateTempMemory( glConfig.vidWidth * glConfig.vidHeight );
		//qglReadPixels( 0, 0, glConfig.vidWidth, glConfig.vidHeight, GL_STENCIL_INDEX, GL_UNSIGNED_BYTE, stencilReadback ); // Cowcat

		for ( i = 0; i < glConfig.vidWidth * glConfig.vidHeight; i++ )
		{
			sum += stencilReadback[i];
		}

		backEnd.pc.c_overDraw += sum;
		ri.Hunk_FreeTempMemory( stencilReadback );
	}
	#endif

	if ( !glState.finishCalled )
	{
		qglFinish();
	}

	//GLimp_LogComment( "***************** RB_SwapBuffers *****************\n\n\n" ); // Cowcat

	GLimp_EndFrame();

	backEnd.projection2D = qfalse;

	return (const void *)(cmd + 1);
}

/*
====================
RB_ExecuteRenderCommands

This function will be called synchronously if running without
smp extensions, or asynchronously by another thread.
====================
*/
void RB_ExecuteRenderCommands( const void *data )
{
	int	t1, t2;

	t1 = ri.Milliseconds ();

	while ( 1 )
	{
		data = PADP(data, sizeof(void *));

		switch ( *(const int *)data )
		{
			case RC_SET_COLOR:
				data = RB_SetColor( data );
				break;

			case RC_STRETCH_PIC:
				data = RB_StretchPic( data );
				break;

			case RC_DRAW_SURFS:
				data = RB_DrawSurfs( data );
				break;

			case RC_DRAW_BUFFER:
				data = RB_DrawBuffer( data );
				break;

			case RC_SWAP_BUFFERS:
				data = RB_SwapBuffers( data );
				break;

			case RC_SCREENSHOT:
				data = RB_TakeScreenshotCmd( data );
				break;

			case RC_VIDEOFRAME:
				data = RB_TakeVideoFrameCmd( data );
				break;

			case RC_COLORMASK:
				data = RB_ColorMask(data);
				break;

			case RC_CLEARDEPTH:
				data = RB_ClearDepth(data);
				break;

			case RC_END_OF_LIST:
			default:
				// stop rendering// on this thread
				t2 = ri.Milliseconds ();
				backEnd.pc.msec = t2 - t1;
				return;
		}
	}

}

