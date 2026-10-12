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

/*
=================
R_CullTriSurf

Returns true if the grid is completely culled away.
Also sets the clipped hint bit in tess
=================
*/
static qboolean	R_CullTriSurf( srfTriangles_t *cv )
{
	int 	boxCull;

	boxCull = R_CullLocalBox( cv->bounds );

	if ( boxCull == CULL_OUT )
	{
		return qtrue;
	}

	return qfalse;
}

/*
=================
R_CullGrid

Returns true if the grid is completely culled away.
Also sets the clipped hint bit in tess
=================
*/
static qboolean	R_CullGrid( srfGridMesh_t *cv )
{
	int 	boxCull;
	int 	sphereCull;

	if ( r_nocurves->integer ) {
		return qtrue;
	}

	if ( tr.currentEntityNum != ENTITYNUM_WORLD )
	{
		sphereCull = R_CullLocalPointAndRadius( cv->localOrigin, cv->meshRadius );
	}

	else
	{
		sphereCull = R_CullPointAndRadius( cv->localOrigin, cv->meshRadius );
	}
	
	// check for trivial reject
	if ( sphereCull == CULL_OUT )
	{
		tr.pc.c_sphere_cull_patch_out++;
		return qtrue;
	}

	// check bounding box if necessary
	else if ( sphereCull == CULL_CLIP )
	{
		tr.pc.c_sphere_cull_patch_clip++;

		boxCull = R_CullLocalBox( cv->meshBounds );

		if ( boxCull == CULL_OUT ) 
		{
			tr.pc.c_box_cull_patch_out++;
			return qtrue;
		}

		else if ( boxCull == CULL_IN )
		{
			tr.pc.c_box_cull_patch_in++;
		}

		else
		{
			tr.pc.c_box_cull_patch_clip++;
		}
	}

	else
	{
		tr.pc.c_sphere_cull_patch_in++;
	}

	return qfalse;
}


/*
================
R_CullSurface

Tries to back face cull surfaces before they are lighted or
added to the sorting list.

This will also allow mirrors on both sides of a model without recursion.
================
*/
static qboolean R_CullSurface( surfaceType_t *surface, shader_t *shader )
{
	srfSurfaceFace_t 	*sface;
	float			d;

	if ( r_nocull->integer )
		return qfalse;

	if ( *surface == SF_GRID )
		return R_CullGrid( (srfGridMesh_t *)surface );

	if ( *surface == SF_TRIANGLES )
		return R_CullTriSurf( (srfTriangles_t *)surface );

	if ( *surface != SF_FACE )
		return qfalse;

	if ( shader->cullType == CT_TWO_SIDED )
		return qfalse;

	// face culling
	if ( !r_facePlaneCull->integer )
		return qfalse;

	sface = ( srfSurfaceFace_t * ) surface;
	d = DotProduct (tr.or.viewOrigin, sface->plane.normal);

	// don't cull exactly on the plane, because there are levels of rounding
	// through the BSP, ICD, and hardware that may cause pixel gaps if an
	// epsilon isn't allowed here

	if ( shader->cullType == CT_FRONT_SIDED )
	{
		if ( d < sface->plane.dist - 8 )
			return qtrue;
	}

	else
	{
		if ( d > sface->plane.dist + 8 )
			return qtrue;
	}

	return qfalse;
}


static int R_DlightFace( srfSurfaceFace_t *face, int dlightBits )
{
	float		d;
	int		i;
	dlight_t	*dl;

	for ( i = 0 ; i < tr.refdef.num_dlights ; i++ )
	{
		if ( ! ( dlightBits & ( 1 << i ) ) )
		{
			continue;
		}

		dl = &tr.refdef.dlights[i];
		d = DotProduct( dl->origin, face->plane.normal ) - face->plane.dist;

		if ( d < -dl->radius || d > dl->radius )
		{
			// dlight doesn't reach the plane
			dlightBits &= ~( 1 << i );
		}
	}

	if ( !dlightBits )
	{
		tr.pc.c_dlightSurfacesCulled++;
	}

	face->dlightBits = dlightBits;
	return dlightBits;
}

static int R_DlightGrid( srfGridMesh_t *grid, int dlightBits )
{
	int		i;
	dlight_t	*dl;

	for ( i = 0 ; i < tr.refdef.num_dlights ; i++ )
	{
		if ( ! ( dlightBits & ( 1 << i ) ) )
		{
			continue;
		}

		dl = &tr.refdef.dlights[i];

		if ( dl->origin[0] - dl->radius > grid->meshBounds[1][0]
			|| dl->origin[0] + dl->radius < grid->meshBounds[0][0]
			|| dl->origin[1] - dl->radius > grid->meshBounds[1][1]
			|| dl->origin[1] + dl->radius < grid->meshBounds[0][1]
			|| dl->origin[2] - dl->radius > grid->meshBounds[1][2]
			|| dl->origin[2] + dl->radius < grid->meshBounds[0][2] )
		{
			// dlight doesn't reach the bounds
			dlightBits &= ~( 1 << i );
		}
	}

	if ( !dlightBits )
	{
		tr.pc.c_dlightSurfacesCulled++;
	}

	grid->dlightBits = dlightBits;
	return dlightBits;
}


static int R_DlightTrisurf( srfTriangles_t *surf, int dlightBits )
{
	// FIXME: more dlight culling to trisurfs...
	surf->dlightBits = dlightBits;
	return dlightBits;
#if 0
	int		i;
	dlight_t	*dl;

	for ( i = 0 ; i < tr.refdef.num_dlights ; i++ )
	{
		if ( ! ( dlightBits & ( 1 << i ) ) )
		{
			continue;
		}

		dl = &tr.refdef.dlights[i];

		if ( dl->origin[0] - dl->radius > grid->meshBounds[1][0]
			|| dl->origin[0] + dl->radius < grid->meshBounds[0][0]
			|| dl->origin[1] - dl->radius > grid->meshBounds[1][1]
			|| dl->origin[1] + dl->radius < grid->meshBounds[0][1]
			|| dl->origin[2] - dl->radius > grid->meshBounds[1][2]
			|| dl->origin[2] + dl->radius < grid->meshBounds[0][2] )
		{
			// dlight doesn't reach the bounds
			dlightBits &= ~( 1 << i );
		}
	}

	if ( !dlightBits )
	{
		tr.pc.c_dlightSurfacesCulled++;
	}

	grid->dlightBits[ tr.smpFrame ] = dlightBits;
	return dlightBits;
#endif
}

/*
====================
R_DlightSurface

The given surface is going to be drawn, and it touches a leaf
that is touched by one or more dlights, so try to throw out
more dlights if possible.
====================
*/
static int R_DlightSurface( msurface_t *surf, int dlightBits )
{
	if ( *surf->data == SF_FACE )
	{
		dlightBits = R_DlightFace( (srfSurfaceFace_t *)surf->data, dlightBits );
	}

	else if ( *surf->data == SF_GRID )
	{
		dlightBits = R_DlightGrid( (srfGridMesh_t *)surf->data, dlightBits );
	}

	else if ( *surf->data == SF_TRIANGLES )
	{
		dlightBits = R_DlightTrisurf( (srfTriangles_t *)surf->data, dlightBits );
	}

	else
	{
		dlightBits = 0;
	}

	if ( dlightBits )
	{
		tr.pc.c_dlightSurfaces++;
	}

	return dlightBits;
}



/*
======================
R_AddWorldSurface
======================
*/
static void R_AddWorldSurface( msurface_t *surf, int dlightBits )
{
	if ( surf->viewCount == tr.viewCount )
	{
		return;		// already in this view
	}

	surf->viewCount = tr.viewCount;
	// FIXME: bmodel fog?

	// try to cull before dlighting or adding
	if ( R_CullSurface( surf->data, surf->shader ) )
	{
		return;
	}

	// check for dlighting
	if ( dlightBits )
	{
		dlightBits = R_DlightSurface( surf, dlightBits );
		dlightBits = ( dlightBits != 0 );
	}

	R_AddDrawSurf( surf->data, surf->shader, surf->fogIndex, dlightBits );
}

/*
=============================================================

	BRUSH MODELS

=============================================================
*/

/*
=================
R_AddBrushModelSurfaces
=================
*/
void R_AddBrushModelSurfaces ( trRefEntity_t *ent )
{
	bmodel_t	*bmodel;
	int		clip;
	model_t		*pModel;
	int		i;

	pModel = R_GetModelByHandle( ent->e.hModel );

	bmodel = pModel->bmodel;

	clip = R_CullLocalBox( bmodel->bounds );

	if ( clip == CULL_OUT )
		return;

	/* FIX (2026-08-23, walls-disappear-at-distance investigation, part
	 * 3): R_SetFarClip (tr_main.c) measures its zFar distance purely
	 * against tr.viewParms.visBounds -- which, before this fix, was
	 * ONLY ever expanded by R_RecursiveWorldNode (this file, above) for
	 * the static WORLD model's own BSP leaf traversal. Brush entities
	 * (func_wall/func_group/moving platforms/large decorative walls
	 * mappers sometimes group as a separate bmodel -- reached via THIS
	 * function, not R_RecursiveWorldNode at all) never contributed to
	 * visBounds, regardless of the leaf/node bounds fixes already made
	 * (parts 1 and 2, same investigation) -- those only ever helped the
	 * subset of this wall's geometry that's genuinely part of
	 * worldspawn's own tree. R_CullLocalBox just above already
	 * transforms bmodel->bounds' 8 corners into world space via
	 * tr.or.origin/tr.or.axis for its own cull test -- same exact
	 * pattern repeated here to fold those same corners into visBounds
	 * instead of discarding them. See
	 * project_minigl_v3d_ioq3_walls_disappear_distance_investigation
	 * memory for the full account. */
	{
		int	i;

		for ( i = 0 ; i < 8 ; i++ )
		{
			vec3_t	v, corner;

			v[0] = bmodel->bounds[i&1][0];
			v[1] = bmodel->bounds[(i>>1)&1][1];
			v[2] = bmodel->bounds[(i>>2)&1][2];

			VectorCopy( tr.or.origin, corner );
			VectorMA( corner, v[0], tr.or.axis[0], corner );
			VectorMA( corner, v[1], tr.or.axis[1], corner );
			VectorMA( corner, v[2], tr.or.axis[2], corner );

			AddPointToBounds( corner, tr.viewParms.visBounds[0], tr.viewParms.visBounds[1] );
		}
	}

	R_SetupEntityLighting ( &tr.refdef, ent );
	R_DlightBmodel( bmodel );

	for ( i = 0 ; i < bmodel->numSurfaces ; i++ )
	{
		R_AddWorldSurface( bmodel->firstSurface + i, tr.currentEntity->needDlights );
	}
}

/*
=============================================================

	WORLD MODEL

=============================================================
*/

/*
================
R_RecursiveWorldNode
================
*/

#if 0
static void R_AddLeafSurfaces( mnode_t *node, int dlightBits)
{
	int		c;
	msurface_t	*surf, **mark;

	tr.pc.c_leafs++;

	// add to z buffer bounds
	if ( node->mins[0] < tr.viewParms.visBounds[0][0] )
		tr.viewParms.visBounds[0][0] = node->mins[0];

	if ( node->mins[1] < tr.viewParms.visBounds[0][1] )
		tr.viewParms.visBounds[0][1] = node->mins[1];

	if ( node->mins[2] < tr.viewParms.visBounds[0][2] )
		tr.viewParms.visBounds[0][2] = node->mins[2];

	if ( node->maxs[0] > tr.viewParms.visBounds[1][0] )
		tr.viewParms.visBounds[1][0] = node->maxs[0];

	if ( node->maxs[1] > tr.viewParms.visBounds[1][1] )
		tr.viewParms.visBounds[1][1] = node->maxs[1];

	if ( node->maxs[2] > tr.viewParms.visBounds[1][2] )
		tr.viewParms.visBounds[1][2] = node->maxs[2];

	// add the individual surfaces
	mark = node->firstmarksurface;
	c = node->nummarksurfaces;

	while (c--)
	{
		// the surface may have already been added if it
		// spans multiple leafs
		surf = *mark;
		R_AddWorldSurface( surf, dlightBits );
		mark++;
	}
}
#endif

/* FIX (2026-08-30, Q3DM0/Q3DM1 opposite-wall-disappears investigation,
 * on top of the earlier walls-disappear-at-distance fixes): visBounds
 * was still gated on BOTH PVS (node->visframe) AND the per-node
 * frustum-cull test below -- but that test is a tight box-vs-plane check
 * that can flip its verdict on a borderline node from a tiny camera
 * position/angle change, silently shrinking visBounds -- and therefore
 * R_SetFarClip's zFar -- even though nothing about the geometry beyond
 * that node changed. Fold a culled node's own bounds in before it gets
 * pruned.
 *
 * v1 of this fix (same day) restricted this to LEAVES only, reasoning
 * that an internal decision node whose box fully fails the test means
 * every leaf inside it also fails it individually -- true, but beside
 * the point: when an ANCESTOR node fails this test, R_RecursiveWorldNode
 * returns right there and never visits the leaves underneath it at all,
 * so a leaf-only fix never gets a chance to run for anything pruned at
 * an ancestor level. Hardware-confirmed still reproducing after v1
 * (Q3DM1, pure rotation at a fixed origin, visBounds min-Y flipping
 * between 960/1152 depending on view angle) -- the exclusion was
 * happening one or more tree levels above any leaf. Broadened to apply
 * at every node level, not just leaves: a decision node's own
 * borderline cull-test flip is exactly as marginal as a leaf's, by the
 * same reasoning, it's just pruning more at once. Mirrors what
 * R_AddBrushModelSurfaces already does for brush-entity bounds (folds
 * unconditionally unless fully culled, no leaf-only special case).
 * Trade-off, accepted: a large borderline decision node can make
 * visBounds/zFar more generous than strictly needed for that one frame
 * -- preferred over losing geometry. */
static void R_AddCulledNodeBounds( mnode_t *node )
{
	if ( node->mins[0] < tr.viewParms.visBounds[0][0] )
		tr.viewParms.visBounds[0][0] = node->mins[0];

	if ( node->mins[1] < tr.viewParms.visBounds[0][1] )
		tr.viewParms.visBounds[0][1] = node->mins[1];

	if ( node->mins[2] < tr.viewParms.visBounds[0][2] )
		tr.viewParms.visBounds[0][2] = node->mins[2];

	if ( node->maxs[0] > tr.viewParms.visBounds[1][0] )
		tr.viewParms.visBounds[1][0] = node->maxs[0];

	if ( node->maxs[1] > tr.viewParms.visBounds[1][1] )
		tr.viewParms.visBounds[1][1] = node->maxs[1];

	if ( node->maxs[2] > tr.viewParms.visBounds[1][2] )
		tr.viewParms.visBounds[1][2] = node->maxs[2];
}

static void R_RecursiveWorldNode( mnode_t *node, unsigned int planeBits, unsigned int dlightBits )
{
	do
	{
		unsigned int	newDlights[2];

		// if the node wasn't marked as potentially visible, exit
		/* REVERTED (2026-08-30, user-directed, hardware-measured):
		 * folding PVS-excluded node bounds here (mirroring the frustum-
		 * cull fold) DID fix the courtyard->corridor PVS case, confirmed
		 * via r_novis 1 -- but cost 1-2fps on a 25fps target (4-8%
		 * relative, "huge") and did NOT fix the other direction (corridor
		 * end -> courtyard, a separate leaf-bounds-accuracy issue,
		 * neither PVS nor frustum). Incomplete fix + real perf cost ->
		 * not worth it. Back to plain PVS gate; zFar padding restored in
		 * tr_main.c/R_SetFarClip to cover the gap this leaves instead. */
		if (node->visframe != tr.visCount)
			return;

		// if the bounding volume is outside the frustum, nothing
		// inside can be visible OPTIMIZE: don't do this all the way to leafs?

		if ( !r_nocull->integer )
		{
			int	r;

			if ( planeBits & 1 )
			{
				r = BoxOnPlaneSide(node->mins, node->maxs, &tr.viewParms.frustum[0]);

				if (r == 2)
				{
					R_AddCulledNodeBounds( node );
					return;			// culled
				}

				if ( r == 1 )
					planeBits &= ~1;	// all descendants will also be in front
			}

			if ( planeBits & 2 )
			{
				r = BoxOnPlaneSide(node->mins, node->maxs, &tr.viewParms.frustum[1]);

				if (r == 2)
				{
					R_AddCulledNodeBounds( node );
					return;			// culled
				}

				if ( r == 1 )
					planeBits &= ~2;	// all descendants will also be in front
			}

			if ( planeBits & 4 )
			{
				r = BoxOnPlaneSide(node->mins, node->maxs, &tr.viewParms.frustum[2]);

				if (r == 2)
				{
					R_AddCulledNodeBounds( node );
					return;			// culled
				}

				if ( r == 1 )
					planeBits &= ~4;	// all descendants will also be in front
			}

			if ( planeBits & 8 )
			{
				r = BoxOnPlaneSide(node->mins, node->maxs, &tr.viewParms.frustum[3]);

				if (r == 2)
				{
					R_AddCulledNodeBounds( node );
					return;			// culled
				}

				if ( r == 1 )
					planeBits &= ~8;	// all descendants will also be in front
			}
		}

		if ( node->contents != -1 )
			break;

		// node is just a decision point, so go down both sides
		// since we don't care about sort orders, just go positive to negative

		// determine which dlights are needed
		newDlights[0] = 0;
		newDlights[1] = 0;

		if ( dlightBits )
		{
			int	i;

			for ( i = 0 ; i < tr.refdef.num_dlights ; i++ )
			{
				dlight_t	*dl;
				float		dist;

				if ( dlightBits & ( 1 << i ) )
				{
					dl = &tr.refdef.dlights[i];
					dist = DotProduct( dl->origin, node->plane->normal ) - node->plane->dist;
					
					if ( dist > -dl->radius )
						newDlights[0] |= ( 1 << i );

					if ( dist < dl->radius )
						newDlights[1] |= ( 1 << i );
				}
			}
		}

		// recurse down the children, front side first
		R_RecursiveWorldNode (node->children[0], planeBits, newDlights[0] );

		// tail recurse
		node = node->children[1];
		dlightBits = newDlights[1];

	} while ( 1 );

	#if 1
	{
		// leaf node, so add mark surfaces
		int		c;
		msurface_t	*surf, **mark;

		tr.pc.c_leafs++;

		/* FIX (2026-08-23, walls-disappear-at-distance investigation):
		 * the "spearmint - test Cowcat" nummarksurfaces==0 early return
		 * used to sit above this block and skip it (including the vis
		 * bounds update below) for any leaf with no surfaces of its own.
		 * That's a real bug, not just an optimization: R_SetFarClip's
		 * dynamic far-clip distance is measured against
		 * tr.viewParms.visBounds, and an empty (but visible, in-frustum)
		 * leaf sitting between the camera and a distant wall was never
		 * contributing to those bounds, undercounting the far clip plane
		 * below the real distance to walls that should still be visible.
		 * Stock id-software code (see the #if 0'd R_AddLeafSurfaces
		 * above) always updates bounds for every visited leaf regardless
		 * of surface count -- restored that here. The surface loop below
		 * already handles nummarksurfaces==0 correctly on its own (zero
		 * iterations), so the early return bought nothing but this bug. */

		// add to z buffer bounds
		if ( node->mins[0] < tr.viewParms.visBounds[0][0] )
			tr.viewParms.visBounds[0][0] = node->mins[0];

		if ( node->mins[1] < tr.viewParms.visBounds[0][1] )
			tr.viewParms.visBounds[0][1] = node->mins[1];

		if ( node->mins[2] < tr.viewParms.visBounds[0][2] )
			tr.viewParms.visBounds[0][2] = node->mins[2];

		if ( node->maxs[0] > tr.viewParms.visBounds[1][0] )
			tr.viewParms.visBounds[1][0] = node->maxs[0];

		if ( node->maxs[1] > tr.viewParms.visBounds[1][1] )
			tr.viewParms.visBounds[1][1] = node->maxs[1];

		if ( node->maxs[2] > tr.viewParms.visBounds[1][2] )
			tr.viewParms.visBounds[1][2] = node->maxs[2];

		// add the individual surfaces
		mark = node->firstmarksurface;
		c = node->nummarksurfaces;

		while (c--)
		{
			// the surface may have already been added if it
			// spans multiple leafs
			surf = *mark;
			R_AddWorldSurface( surf, dlightBits );
			mark++;
		}
	}

	#else // spearmint - test Cowcat

	if(node->nummarksurfaces == 0) 
	{
		//ri.Printf(PRINT_ALL, "nummarksurfaces = 0\n");
		return;
	}

	R_AddLeafSurfaces( node, dlightBits );

	#endif
}


/*
===============
R_PointInLeaf
===============
*/
static mnode_t *R_PointInLeaf( const vec3_t p )
{
	mnode_t		*node;
	float		d;
	cplane_t	*plane;
	
	if ( !tr.world )
		ri.Error (ERR_DROP, "R_PointInLeaf: bad model");

	node = tr.world->nodes;

	while( 1 )
	{
		if (node->contents != -1)
			break;

		plane = node->plane;
		d = DotProduct (p,plane->normal) - plane->dist;

		if (d > 0)
			node = node->children[0];

		else
			node = node->children[1];
	}
	
	return node;
}

/*
==============
R_ClusterPVS
==============
*/
static const byte *R_ClusterPVS (int cluster)
{
	if ( !tr.world->vis || cluster < 0 || cluster >= tr.world->numClusters )
		return tr.world->novis;

	return tr.world->vis + cluster * tr.world->clusterBytes;
}

/*
=================
R_inPVS
=================
*/
qboolean R_inPVS( const vec3_t p1, const vec3_t p2 )
{
	mnode_t *leaf;
	byte	*vis;

	leaf = R_PointInLeaf( p1 );
	vis = CM_ClusterPVS( leaf->cluster ); // why not R_ClusterPVS ??
	leaf = R_PointInLeaf( p2 );

	if ( !(vis[leaf->cluster>>3] & (1<<(leaf->cluster&7))) )
		return qfalse;

	return qtrue;
}

/*
===============
R_MarkLeaves

Mark the leaves and nodes that are in the PVS for the current
cluster
===============
*/
static void R_MarkLeaves (void)
{
	const byte	*vis;
	mnode_t		*leaf, *parent;
	int		i;
	int		cluster;

	// lockpvs lets designers walk around to determine the
	// extent of the current pvs
	if ( r_lockpvs->integer )
		return;

	// current viewcluster
	leaf = R_PointInLeaf( tr.viewParms.pvsOrigin );
	cluster = leaf->cluster;

	// if the cluster is the same and the area visibility matrix
	// hasn't changed, we don't need to mark everything again

	// if r_showcluster was just turned on, remark everything 
	if ( tr.viewCluster == cluster && !tr.refdef.areamaskModified && !r_showcluster->modified )
		return;

	if ( r_showcluster->modified || r_showcluster->integer )
	{
		r_showcluster->modified = qfalse;

		if ( r_showcluster->integer )
			ri.Printf( PRINT_ALL, "cluster:%i  area:%i\n", cluster, leaf->area );
	}

	tr.visCount++;
	tr.viewCluster = cluster;

	if ( r_novis->integer || tr.viewCluster == -1 )
	{
		for (i=0 ; i<tr.world->numnodes ; i++)
		{
			if (tr.world->nodes[i].contents != CONTENTS_SOLID)
				tr.world->nodes[i].visframe = tr.visCount;
		}

		return;
	}

	vis = R_ClusterPVS (tr.viewCluster);
	
	for (i=0, leaf=tr.world->nodes ; i<tr.world->numnodes ; i++, leaf++)
	{
		cluster = leaf->cluster;

		if ( cluster < 0 || cluster >= tr.world->numClusters )
			continue;

		// check general pvs
		if ( !(vis[cluster>>3] & (1<<(cluster&7))) )
			continue;

		// check for door connection
		if ( (tr.refdef.areamask[leaf->area>>3] & (1<<(leaf->area&7)) ) )
			continue;	// not visible

		parent = leaf;

		do
		{
			if (parent->visframe == tr.visCount)
				break;

			parent->visframe = tr.visCount;
			parent = parent->parent;

		} while (parent);
	}
}


/*
=============
R_AddWorldSurfaces
=============
*/
void R_AddWorldSurfaces (void)
{
	if ( !r_drawworld->integer )
		return;

	if ( tr.refdef.rdflags & RDF_NOWORLDMODEL )
		return;

	tr.currentEntityNum = ENTITYNUM_WORLD;
	tr.shiftedEntityNum = tr.currentEntityNum << QSORT_ENTITYNUM_SHIFT;

	// determine which leaves are in the PVS / areamask
	R_MarkLeaves ();

	// clear out the visible min/max
	ClearBounds( tr.viewParms.visBounds[0], tr.viewParms.visBounds[1] );

	// perform frustum culling and add all the potentially visible surfaces
	if ( tr.refdef.num_dlights > MAX_DLIGHTS )
		tr.refdef.num_dlights = MAX_DLIGHTS;

	R_RecursiveWorldNode( tr.world->nodes, 15, ( 1ULL << tr.refdef.num_dlights ) - 1 );
}
