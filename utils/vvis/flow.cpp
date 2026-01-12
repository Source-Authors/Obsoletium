//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//
//=============================================================================//
#include "vis.h"
#include "vmpi.h"

int g_TraceClusterStart = -1;
int g_TraceClusterStop = -1;
/*

  each portal will have a list of all possible to see from first portal

  if (!thread->portalmightsee[portalnum])

  portal mightsee

  for p2 = all other portals in leaf
	get sperating planes
	for all portals that might be seen by p2
		mark as unseen if not present in seperating plane
	flood fill a new mightsee
	save as passagemightsee


  void CalcMightSee (leaf_t *leaf, 
*/

int CountBits (byte *bits, int numbits)
{
	int c = 0;
	for (int i=0 ; i<numbits ; i++)
		if ( CheckBit( bits, i ) )
			c++;

	return c;
}

int		c_fullskip;
int		c_portalskip, c_leafskip;
int		c_vistest, c_mighttest;

int		c_chop, c_nochop;

int		active;

#ifdef MPI
extern bool g_bVMPIEarlyExit;
#endif


void CheckStack (leaf_t *leaf, threaddata_t *thread)
{
	for (pstack_t *p = thread->pstack_head.next; p; p = p->next)
	{
		if (p->leaf == leaf)
			Error ("CheckStack: leaf 0x%p recursion detected!", leaf);
		for (pstack_t *p2=thread->pstack_head.next ; p2 != p ; p2=p2->next)
			if (p2->leaf == p->leaf)
				Error ("CheckStack: late leaf 0x%p recursion detected!", p->leaf);
	}
}


winding_t *AllocStackWinding (pstack_t *stack)
{
	for (int i=0 ; i<3 ; i++)
	{
		if (stack->freewindings[i])
		{
			stack->freewindings[i] = 0;
			return &stack->windings[i];
		}
	}

	Error ("Out of memory. AllocStackWinding: failed");

	return nullptr;
}

void FreeStackWinding (winding_t *w, pstack_t *stack)
{
	int i = w - stack->windings;

	if (i<0 || i>2)
		return;		// not from local

	if (stack->freewindings[i])
		Error ("FreeStackWinding: allready free");

	stack->freewindings[i] = 1;
}

/*
==============
ChopWinding

==============
*/

winding_t	*ChopWinding (winding_t *in, pstack_t *stack, plane_t *split)
{
	vec_t	dists[128];
	SideType		sides[128];
	int		counts[3];
	vec_t	dot;
	int		i, j;
	Vector	mid;
	winding_t	*neww;

	counts[0] = counts[1] = counts[2] = 0;

// determine sides for each point
	for (i=0 ; i<in->numpoints ; i++)
	{
		dot = DotProduct (in->points[i], split->normal);
		dot -= split->dist;
		dists[i] = dot;
		if (dot > ON_VIS_EPSILON)
			sides[i] = SIDE_FRONT;
		else if (dot < -ON_VIS_EPSILON)
			sides[i] = SIDE_BACK;
		else
		{
			sides[i] = SIDE_ON;
		}
		counts[sides[i]]++;
	}

	if (!counts[1])
		return in;		// completely on front side
	
	if (!counts[0])
	{
		FreeStackWinding (in, stack);
		return NULL;
	}

	sides[i] = sides[0];
	dists[i] = dists[0];
	
	neww = AllocStackWinding (stack);

	neww->numpoints = 0;

	for (i=0 ; i<in->numpoints ; i++)
	{
		Vector& p1 = in->points[i];

		if (neww->numpoints == MAX_POINTS_ON_FIXED_WINDING)
		{
			FreeStackWinding (neww, stack);
			return in;		// can't chop -- fall back to original
		}

		if (sides[i] == SIDE_ON)
		{
			VectorCopy (p1, neww->points[neww->numpoints]);
			neww->numpoints++;
			continue;
		}
	
		if (sides[i] == SIDE_FRONT)
		{
			VectorCopy (p1, neww->points[neww->numpoints]);
			neww->numpoints++;
		}
		
		if (sides[i+1] == SIDE_ON || sides[i+1] == sides[i])
			continue;
			
		if (neww->numpoints == MAX_POINTS_ON_FIXED_WINDING)
		{
			FreeStackWinding (neww, stack);
			return in;		// can't chop -- fall back to original
		}

	// generate a split point
		Vector& p2 = in->points[(i+1)%in->numpoints];
		
		dot = dists[i] / (dists[i]-dists[i+1]);
		for (j=0 ; j<3 ; j++)
		{	// avoid round off error when possible
			if (split->normal[j] == 1)
				mid[j] = split->dist;
			else if (split->normal[j] == -1)
				mid[j] = -split->dist;
			else
				mid[j] = p1[j] + dot*(p2[j]-p1[j]);
		}
			
		VectorCopy (mid, neww->points[neww->numpoints]);
		neww->numpoints++;
	}
	
// free the original winding
	FreeStackWinding (in, stack);
	
	return neww;
}

/*
==============
ClipToSeperators

Source, pass, and target are an ordering of portals.

Generates seperating planes canidates by taking two points from source and one
point from pass, and clips target by them.

If target is totally clipped away, that portal can not be seen through.

Normal clip keeps target on the same side as pass, which is correct if the
order goes source, pass, target.  If the order goes pass, source, target then
flipclip should be set.
==============
*/
winding_t	*ClipToSeperators (winding_t *source, winding_t *pass, winding_t *target, bool flipclip, pstack_t *stack)
{
	int			i, j, k, l;
	plane_t		plane;
	Vector		v1, v2;
	float		d;
	vec_t		length;
	int			counts[3];
	bool		fliptest;

// check all combinations	
	for (i=0 ; i<source->numpoints ; i++)
	{
		l = (i+1)%source->numpoints;
		VectorSubtract (source->points[l] , source->points[i], v1);

	// fing a vertex of pass that makes a plane that puts all of the
	// vertexes of pass on the front side and all of the vertexes of
	// source on the back side
		for (j=0 ; j<pass->numpoints ; j++)
		{
			VectorSubtract (pass->points[j], source->points[i], v2);

			plane.normal[0] = v1[1]*v2[2] - v1[2]*v2[1];
			plane.normal[1] = v1[2]*v2[0] - v1[0]*v2[2];
			plane.normal[2] = v1[0]*v2[1] - v1[1]*v2[0];
			
		// if points don't make a valid plane, skip it

			length = plane.normal[0] * plane.normal[0]
			+ plane.normal[1] * plane.normal[1]
			+ plane.normal[2] * plane.normal[2];
			
			if (length < ON_VIS_EPSILON)
				continue;

			length = 1/sqrt(length);
			
			plane.normal[0] *= length;
			plane.normal[1] *= length;
			plane.normal[2] *= length;

			plane.dist = DotProduct (pass->points[j], plane.normal);

		//
		// find out which side of the generated seperating plane has the
		// source portal
		//
#if 1
			fliptest = false;
			for (k=0 ; k<source->numpoints ; k++)
			{
				if (k == i || k == l)
					continue;
				d = DotProduct (source->points[k], plane.normal) - plane.dist;
				if (d < -ON_VIS_EPSILON)
				{	// source is on the negative side, so we want all
					// pass and target on the positive side
					fliptest = false; //-V1048
					break;
				}
				else if (d > ON_VIS_EPSILON)
				{	// source is on the positive side, so we want all
					// pass and target on the negative side
					fliptest = true;
					break;
				}
			}
			if (k == source->numpoints)
				continue;		// planar with source portal
#else
			fliptest = flipclip;
#endif
		//
		// flip the normal if the source portal is backwards
		//
			if (fliptest)
			{
				VectorSubtract (vec3_origin, plane.normal, plane.normal);
				plane.dist = -plane.dist;
			}
#if 1
		//
		// if all of the pass portal points are now on the positive side,
		// this is the seperating plane
		//
			counts[0] = counts[1] = counts[2] = 0;
			for (k=0 ; k<pass->numpoints ; k++)
			{
				if (k==j)
					continue;
				d = DotProduct (pass->points[k], plane.normal) - plane.dist;
				if (d < -ON_VIS_EPSILON)
					break;
				else if (d > ON_VIS_EPSILON)
					counts[0]++;
				else
					counts[2]++;
			}
			if (k != pass->numpoints)
				continue;	// points on negative side, not a seperating plane
				
			if (!counts[0])
				continue;	// planar with seperating plane
#else
			k = (j+1)%pass->numpoints;
			d = DotProduct (pass->points[k], plane.normal) - plane.dist;
			if (d < -ON_VIS_EPSILON)
				continue;
			k = (j+pass->numpoints-1)%pass->numpoints;
			d = DotProduct (pass->points[k], plane.normal) - plane.dist;
			if (d < -ON_VIS_EPSILON)
				continue;			
#endif
		//
		// flip the normal if we want the back side
		//
			if (flipclip)
			{
				VectorSubtract (vec3_origin, plane.normal, plane.normal);
				plane.dist = -plane.dist;
			}
			
		//
		// clip target by the seperating plane
		//
			target = ChopWinding (target, stack, &plane);
			if (!target)
				return NULL;		// target is not visible

			// JAY: End the loop, no need to find additional separators on this edge ?
//			j = pass->numpoints;
		}
	}
	
	return target;
}


class CPortalTrace
{
public:
	CUtlVector<Vector>	m_list;
	CThreadFastMutex	m_mutex;
} g_PortalTrace;

void WindingCenter (winding_t *w, Vector &center)
{
	VectorCopy (vec3_origin, center);
	for (int i=0 ; i<w->numpoints ; i++)
		VectorAdd (w->points[i], center, center);

	float scale = 1.0f/w->numpoints;
	VectorScale (center, scale, center);
}

Vector ClusterCenter( int cluster )
{
	Vector mins, maxs;
	ClearBounds(mins, maxs);
	for ( auto *p : leafs[cluster].portals )
	{
		winding_t *w = p->winding;
		for ( int j = 0; j < w->numpoints; j++ )
		{
			AddPointToBounds( w->points[j], mins, maxs );
		}
	}
	return (mins + maxs) * 0.5f;
}


void DumpPortalTrace( pstack_t *pStack )
{
	AUTO_LOCK(g_PortalTrace.m_mutex);
	if ( g_PortalTrace.m_list.Count() )
		return;

	Warning("Dumped cluster trace!!!\n");
	Vector	mid;
	mid = ClusterCenter( g_TraceClusterStart );
	g_PortalTrace.m_list.AddToTail(mid);
	for ( ; pStack != NULL; pStack = pStack->next )
	{
		winding_t *w = pStack->pass ? pStack->pass : pStack->portal->winding;
		WindingCenter (w, mid);
		g_PortalTrace.m_list.AddToTail(mid);
		for ( int i = 0; i < w->numpoints; i++ )
		{
			g_PortalTrace.m_list.AddToTail(w->points[i]);
			g_PortalTrace.m_list.AddToTail(mid);
		}
		for ( int i = 0; i < w->numpoints; i++ )
		{
			g_PortalTrace.m_list.AddToTail(w->points[i]);
		}
		g_PortalTrace.m_list.AddToTail(w->points[0]);
		g_PortalTrace.m_list.AddToTail(mid);
	}
	mid = ClusterCenter( g_TraceClusterStop );
	g_PortalTrace.m_list.AddToTail(mid);
}

void WritePortalTrace( const char *source )
{
	if ( !g_PortalTrace.m_list.Count() )
	{
		Warning("No trace generated from %d to %d\n", g_TraceClusterStart, g_TraceClusterStop );
		return;
	}
	
	char filename[1024];
	V_sprintf_safe (filename, "%s.lin", source);

	FILE *linefile = fopen (filename, "w");
	if (!linefile)
		Error ("Couldn't open %s\n", filename);

	RunCodeAtScopeExit(fclose(linefile));

	for ( auto &p : g_PortalTrace.m_list )
	{
		fprintf (linefile, "%f %f %f\n", p[0], p[1], p[2]);
	}
	Warning("Wrote %s!!!\n", filename);
}

/*
==================
RecursiveLeafFlow

Flood fill through the leafs
If src_portal is NULL, this is the originating leaf
==================
*/
void RecursiveLeafFlow (int leafnum, threaddata_t *thread, pstack_t *prevstack)
{
	pstack_t	stack;
	plane_t		backplane;
	int			j;
	intp		*test, *might, *vis, more;

#ifdef MPI
	// Early-out if we're a VMPI worker that's told to exit. If we don't do this here, then the
	// worker might spin its wheels for a while on an expensive work unit and not be available to the pool.
	// This is pretty common in vis.
	if ( g_bVMPIEarlyExit )
		return;
#endif

	if ( leafnum == g_TraceClusterStop )
	{
		DumpPortalTrace(&thread->pstack_head);
		return;
	}
	thread->c_chains++;

	leaf_t *leaf = &leafs[leafnum];

	prevstack->next = &stack;

	stack.next = NULL;
	stack.leaf = leaf;
	stack.portal = NULL;

	might = (intp *)stack.mightsee;
	vis = (intp *)thread->base->portalvis;
	
	// check all portals for flowing into other leafs	
	for (intp i=0 ; i<leaf->portals.Count() ; i++)
	{
		portal_t *p = leaf->portals[i];
		int pnum = p - portals;

		if ( ! (prevstack->mightsee[pnum >> 3] & (1<<(pnum&7)) ) )
		{
			continue;	// can't possibly see it
		}

		// if the portal can't see anything we haven't allready seen, skip it
		if (p->status == stat_done)
		{
			test = (intp *)p->portalvis;
		}
		else
		{
			test = (intp *)p->portalflood;
		}

		more = 0;
		for (j=0 ; j<portalarchwords ; j++)
		{
			might[j] = ((intp *)prevstack->mightsee)[j] & test[j];
			more |= (might[j] & ~vis[j]);
		}
		
		if ( !more && CheckBit( thread->base->portalvis, pnum ) )
		{	// can't see anything new
			continue;
		}

		// get plane of portal, point normal into the neighbor leaf
		stack.portalplane = p->plane;
		VectorSubtract (vec3_origin, p->plane.normal, backplane.normal);
		backplane.dist = -p->plane.dist;
		
		stack.portal = p;
		stack.next = NULL;
		stack.freewindings[0] = 1;
		stack.freewindings[1] = 1;
		stack.freewindings[2] = 1;
		
		float d = DotProduct (p->origin, thread->pstack_head.portalplane.normal);
		d -= thread->pstack_head.portalplane.dist;
		if (d < -p->radius)
		{
			continue;
		}
		else if (d > p->radius)
		{
			stack.pass = p->winding;
		}
		else	
		{
			stack.pass = ChopWinding (p->winding, &stack, &thread->pstack_head.portalplane);
			if (!stack.pass)
				continue;
		}


		d = DotProduct (thread->base->origin, p->plane.normal);
		d -= p->plane.dist;
		if (d > thread->base->radius)
		{
			continue;
		}
		else if (d < -thread->base->radius)
		{
			stack.source = prevstack->source;
		}
		else	
		{
			stack.source = ChopWinding (prevstack->source, &stack, &backplane);
			if (!stack.source)
				continue;
		}


		if (!prevstack->pass)
		{	// the second leaf can only be blocked if coplanar

			// mark the portal as visible
			SetBit( thread->base->portalvis, pnum );

			RecursiveLeafFlow (p->leaf, thread, &stack);
			continue;
		}

		stack.pass = ClipToSeperators (stack.source, prevstack->pass, stack.pass, false, &stack);
		if (!stack.pass)
			continue;
		
		stack.pass = ClipToSeperators (prevstack->pass, stack.source, stack.pass, true, &stack);
		if (!stack.pass)
			continue;

		// mark the portal as visible
		SetBit( thread->base->portalvis, pnum );

		// flow through it for real
		RecursiveLeafFlow (p->leaf, thread, &stack);
	}	
}


/*
===============
PortalFlow

generates the portalvis bit vector
===============
*/
void PortalFlow (int iThread, int portalnum)
{
	portal_t *p = sorted_portals[portalnum];
	p->status = stat_working;

	const int c_might = CountBits (p->portalflood, g_numportals*2);
	
	threaddata_t	data;
	memset (&data, 0, sizeof(data));
	data.base = p;
	
	data.pstack_head.portal = p;
	data.pstack_head.source = p->winding;
	data.pstack_head.portalplane = p->plane;
	for (int i=0 ; i<portalarchwords ; i++)
		((intp *)data.pstack_head.mightsee)[i] = ((intp *)p->portalflood)[i];

	RecursiveLeafFlow (p->leaf, &data, &data.pstack_head);

	p->status = stat_done;

	const int c_can = CountBits (p->portalvis, g_numportals*2);

	qprintf ("portal:%4zi  mightsee:%4i  cansee:%4i (%i chains)\n", 
		p - portals, c_might, c_can, data.c_chains);
}


/*
===============================================================================

This is a rough first-order aproximation that is used to trivially reject some
of the final calculations.


Calculates portalfront and portalflood bit vectors

===============================================================================
*/

int		c_flood, c_vis;

/*
==================
SimpleFlood

==================
*/
void SimpleFlood (portal_t *srcportal, int leafnum)
{
	leaf_t *leaf = &leafs[leafnum];
	
	for (auto *p : leaf->portals)
	{
		int pnum = p - portals;

		if ( !CheckBit( srcportal->portalfront, pnum ) )
			continue;

		if ( CheckBit( srcportal->portalflood, pnum ) )
			continue;

		SetBit( srcportal->portalflood, pnum );
		
		SimpleFlood (srcportal, p->leaf);
	}
}

/*
==============
BasePortalVis
==============
*/
void BasePortalVis (int iThread, int portalnum)
{
	int			j, k;
	portal_t	*tp;
	float		d;
	winding_t	*w;
	Vector		segment;
	double		dist2, minDist2;

	// get the portal
	portal_t *p = portals+portalnum;

	//
	// allocate memory for bitwise vis solutions for this portal
	//
	p->portalfront = (byte*)calloc (portalbytes, sizeof(byte));
	p->portalflood = (byte*)calloc (portalbytes, sizeof(byte));
	p->portalvis = (byte*)calloc (portalbytes, sizeof(byte));
	
	//
	// test the given portal against all of the portals in the map
	//
	for (j=0, tp = portals ; j<g_numportals*2 ; j++, tp++)
	{
		// don't test against itself
		if (j == portalnum)
			continue;

		//
		//
		//
		w = tp->winding;
		for (k=0 ; k<w->numpoints ; k++)
		{
			d = DotProduct (w->points[k], p->plane.normal) - p->plane.dist;
			if (d > ON_VIS_EPSILON)
				break;
		}
		if (k == w->numpoints)
			continue;	// no points on front

		//
		//
		//
		w = p->winding;
		for (k=0 ; k<w->numpoints ; k++)
		{
			d = DotProduct (w->points[k], tp->plane.normal) - tp->plane.dist;
			if (d < -ON_VIS_EPSILON)
				break;
		}
		if (k == w->numpoints)
			continue;	// no points on front

		//
		// if using radius visibility -- check to see if any portal points lie inside of the
		// radius given
		//
		if( g_bUseRadius )
		{
			w = tp->winding;
			minDist2 = 1024000000.0;			// 32000^2
			for( k = 0; k < w->numpoints; k++ )
			{
				VectorSubtract( w->points[k], p->origin, segment );
				dist2 = ( segment[0] * segment[0] ) + ( segment[1] * segment[1] ) + ( segment[2] * segment[2] );
				if( dist2 < minDist2 )
				{
					minDist2 = dist2;
				}
			}

			if( minDist2 > g_VisRadius )
				continue;
		}

		// add current portal to given portal's list of visible portals
		SetBit( p->portalfront, j );
	}
	
	SimpleFlood (p, p->leaf);

	p->nummightsee = CountBits (p->portalflood, g_numportals*2);
//	Msg ("portal %i: %i mightsee\n", portalnum, p->nummightsee);
	c_flood += p->nummightsee;
}





/*
===============================================================================

This is a second order aproximation 

Calculates portalvis bit vector

WAAAAAAY too slow.

===============================================================================
*/

/*
==================
RecursiveLeafBitFlow

==================
*/
void RecursiveLeafBitFlow (int leafnum, byte *mightsee, byte *cansee)
{
	alignas(intp) byte		newmight[MAX_PORTALS/8];

	leaf_t *leaf = &leafs[leafnum];
	
	// check all portals for flowing into other leafs
	for (auto *p : leaf->portals)
	{
		int pnum = p - portals;

		// if some previous portal can't see it, skip
		if ( !CheckBit( mightsee, pnum ) )
			continue;

		// if this portal can see some portals we mightsee, recurse
		intp more = 0;
		for (int j=0 ; j<portalarchwords ; j++)
		{
			((intp *)newmight)[j] = ((intp *)mightsee)[j] 
				& ((intp *)p->portalflood)[j];
			more |= ((intp *)newmight)[j] & ~((intp *)cansee)[j];
		}

		if (!more)
			continue;	// can't see anything new

		SetBit( cansee, pnum );

		RecursiveLeafBitFlow (p->leaf, newmight, cansee);
	}
}

/*
==============
BetterPortalVis
==============
*/
void BetterPortalVis (int portalnum)
{
	portal_t *p = portals + portalnum;

	RecursiveLeafBitFlow (p->leaf, p->portalflood, p->portalvis);

	// build leaf vis information
	p->nummightsee = CountBits (p->portalvis, g_numportals*2);
	c_vis += p->nummightsee;
}


