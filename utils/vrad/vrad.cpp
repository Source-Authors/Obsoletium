//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//
//=============================================================================//

// vrad.c

#include "vrad.h"
#include "physdll.h"
#include "lightmap.h"
#include "tier1/strtools.h"
#include "vmpi.h"
#include "macro_texture.h"
#include "vmpi_tools_shared.h"
#include "leaf_ambient_lighting.h"
#include "tools_minidump.h"
#include "loadcmdline.h"
#include "byteswap.h"
#include "bspflags.h"

#include "winlite.h"

#define ALLOWDEBUGOPTIONS (0 || _DEBUG)

static FileHandle_t pFpTrans = NULL;

/*

NOTES
-----

every surface must be divided into at least two patches each axis

*/

CUtlVector<CPatch>		g_Patches;			
CUtlVector<intp>		g_FacePatches;		// contains all patches, children first
CUtlVector<intp>		faceParents;		// contains only root patches, use next parent to iterate
CUtlVector<intp>		clusterChildren;
CUtlVector<Vector>		emitlight;
CUtlVector<bumplights_t>	addlight;

int num_sky_cameras;
sky_camera_t sky_cameras[MAX_MAP_AREAS];
int area_sky_cameras[MAX_MAP_AREAS];

entity_t	*face_entity[MAX_MAP_FACES];
Vector		face_offset[MAX_MAP_FACES];		// for rotating bmodels
int			fakeplanes;

unsigned	numbounce = 100; // 25; /* Originally this was 8 */

float		maxchop = 4; // coarsest allowed number of luxel widths for a patch
float		minchop = 4; // "-chop" tightest number of luxel widths for a patch, used on edges
float		dispchop = 8.0f;	// number of luxel widths for a patch
float		g_MaxDispPatchRadius = 1500.0f;			// Maximum radius allowed for displacement patches
qboolean	g_bDumpPatches;
bool	    bDumpNormals = false;
bool		g_bDumpRtEnv = false;
bool		bRed2Black = true;
bool		g_bFastAmbient = false;
bool        g_bNoSkyRecurse = false;
bool		g_bDumpPropLightmaps = false;


int			junk;

Vector		ambient( 0, 0, 0 );

float		lightscale = 1.0;
float		dlight_threshold = 0.1;  // was DIRECT_LIGHT constant

char		source[MAX_PATH] = "";

char		level_name[MAX_PATH] = "";	// map filename, without extension or path info

char		global_lights[MAX_PATH] = "";
char		designer_lights[MAX_PATH] = "";
char		level_lights[MAX_PATH] = "";

char		vismatfile[_MAX_PATH] = "";
char		incrementfile[_MAX_PATH] = "";

IIncremental *g_pIncremental = 0;
std::atomic_bool	g_bInterrupt = false;	// Used with background lighting in WC. Tells VRAD
									// to stop lighting.
float g_SunAngularExtent=0.0;

float g_flSkySampleScale = 1.0;

bool g_bLargeDispSampleRadius = false;

bool g_bOnlyStaticProps = false;
bool g_bShowStaticPropNormals = false;


float		gamma = 0.5;
float		indirect_sun = 1.0;
float		reflectivityScale = 1.0;
qboolean	do_extra = true;
bool		debug_extra = false;
qboolean	do_fast = false;
qboolean	do_centersamples = false;
int			extrapasses = 4;
float		smoothing_threshold = 0.7071067; // cos(45.0*(M_PI/180)) 
// Cosine of smoothing angle(in radians)
float		coring = 1.0;	// Light threshold to force to blackness(minimizes lightmaps)
qboolean	texscale = true;
int			dlight_map = 0; // Setting to 1 forces direct lighting into different lightmap than radiosity

float		luxeldensity = 1.0;
unsigned	num_degenerate_faces;

qboolean	g_bLowPriority = false;
qboolean	g_bLogHashData = false;
bool		g_bNoDetailLighting = false;
double		g_flStartTime;
bool		g_bStaticPropLighting = false;
bool        g_bStaticPropPolys = false;
bool        g_bTextureShadows = false;
bool        g_bDisablePropSelfShadowing = false;


CUtlVector<byte> g_FacesVisibleToLights;

RayTracingEnvironment g_RtEnv;

dface_t *g_pFaces=0;

// this is a list of material names used on static props which shouldn't cast shadows.  a
// sequential search is used since we allow substring matches. its not time critical, and this
// functionality is a stopgap until vrad starts reading .vmt files.
CUtlVector<char const *> g_NonShadowCastingMaterialStrings;
/*
===================================================================

MISC

===================================================================
*/


int		leafparents[MAX_MAP_LEAFS];
int		nodeparents[MAX_MAP_NODES];

void MakeParents (int nodenum, int parent)
{
	int		i, j;
	dnode_t	*node;

	nodeparents[nodenum] = parent;
	node = &dnodes[nodenum];

	for (i=0 ; i<2 ; i++)
	{
		j = node->children[i];
		if (j < 0)
			leafparents[-j - 1] = nodenum;
		else
			MakeParents (j, nodenum);
	}
}


/*
===================================================================

  TEXTURE LIGHT VALUES

===================================================================
*/

struct texlight_t
{
	char	name[256];
	Vector	value;
	char	*filename;
};

#define	MAX_TEXLIGHTS	128

texlight_t	texlights[MAX_TEXLIGHTS];
int			num_texlights;

/*
============
ReadLightFile
============
*/
void ReadLightFile (char *filename)
{
	char	buf[1024], NoShadName[1024];
	int file_texlights = 0;

	double start = Plat_FloatTime();

	FileHandle_t f = g_pFileSystem->Open( filename, "r" );
	if (!f)
	{
		Warning("Couldn't open texlight file %s.\n", filename);
		return;
	}

	Msg("Reading texlights from '%s'.\n", filename);
	while ( CmdLib_FGets( buf, sizeof( buf ), f ) )
	{
		// check ldr/hdr
		char * scan = buf;
		if ( !strnicmp( "hdr:", scan, 4) )
		{
			scan += 4;
			if ( ! g_bHDR )
			{
				continue;
			}
		}
		if ( !strnicmp( "ldr:", scan, 4) )
		{
			scan += 4;
			if (  g_bHDR )
			{
				continue;
			}
		}

		scan += strspn( scan, " \t" );
		if ( sscanf(scan,"noshadow %1023s",NoShadName)==1)
		{
			// dimhotepus: Fix null termination.
			NoShadName[std::size(NoShadName) - 1] = '\0';
			char * dot = strchr( NoShadName, '.' );
			if ( dot )										// if they specify .vmt, kill it
				* dot = '\0';

			qprintf( "Added '%s' as a non shadow casting material.\n",NoShadName);

			g_NonShadowCastingMaterialStrings.AddToTail( strdup( NoShadName ));
		}
		// dimhotepus: Prevent overflow,
		else if ( sscanf( scan, "forcetextureshadow %1023s", NoShadName ) == 1 )
		{
			NoShadName[std::size(NoShadName) - 1] = '\0';
			// dimhotepus: Add verbose log.
			qprintf( "Added '%s' as a force texture shadows model.\n",NoShadName);

			ForceTextureShadowsOnModel( NoShadName );
		}
		else
		{
			char szTexlight[256];
			Vector value;

			if ( num_texlights == MAX_TEXLIGHTS )
				Error("Too many texlights, max allowed = %d.\n", MAX_TEXLIGHTS);

			int argCnt = sscanf(scan, "%255s ",szTexlight );
			// dimhotepus: Ensure null termination.
			szTexlight[std::size(szTexlight) - 1] = '\0';

			if( argCnt != 1 )
			{
				if ( strlen( scan ) > 4 )
					Msg( "ignoring bad texlight '%s' in %s.\n", scan, filename );
				continue;
			}

			LightForString( scan + strlen( szTexlight ) + 1, value );

			int j = 0;
			for( j; j < num_texlights; j ++ )
			{
				if ( strcmp( texlights[j].name, szTexlight ) == 0 )
				{
					if ( strcmp( texlights[j].filename, filename ) == 0 )
					{
						Warning( "\aDuplication of '%s' in file '%s'!\n",
							 texlights[j].name, texlights[j].filename );
					}
					else if ( texlights[j].value != value )
					{
						Warning( "Warning: Overriding '%s' from '%s' with '%s'!\n",
								texlights[j].name, texlights[j].filename, filename );
					}
					else
					{
						Warning( "Warning: Redundant '%s' def in '%s' AND '%s'!\n",
								 texlights[j].name, texlights[j].filename, filename );
					}
					break;
				}
			}

			V_strcpy_safe( texlights[j].name, szTexlight );
			VectorCopy( value, texlights[j].value );

			texlights[j].filename = filename;

			file_texlights ++;
			
			num_texlights = max( num_texlights, j + 1 );
		}
	}
	qprintf ( "%d texlights parsed from '%s' (%.2fs)\n\n", file_texlights, filename, Plat_FloatTime() - start);
	g_pFileSystem->Close( f );
}


/*
============
LightForTexture
============
*/
void LightForTexture( const char *name, Vector& result )
{
	int		i;

	result[ 0 ] = result[ 1 ] = result[ 2 ] = 0;

	char baseFilename[ MAX_PATH ];

	if ( Q_strncmp( "maps/", name, 5 ) == 0 )
	{
		// this might be a patch texture for cubemaps.  try to parse out the original filename.
		if ( Q_strncmp( level_name, name + 5, Q_strlen( level_name ) ) == 0 )
		{
			const char *base = name + 5 + Q_strlen( level_name );
			if ( *base == '/' )
			{
				++base; // step past the path separator

				// now we've gotten rid of the 'maps/level_name/' part, so we're left with
				// 'originalName_%d_%d_%d'.
				V_strcpy_safe( baseFilename, base );
				bool foundSeparators = true;
				for ( int i=0; i<3; ++i )
				{
					char *underscore = Q_strrchr( baseFilename, '_' );
					if ( underscore && *underscore )
					{
						*underscore = '\0';
					}
					else
					{
						foundSeparators = false;
					}
				}

				if ( foundSeparators )
				{
					name = baseFilename;
				}
			}
		}
	}

	for (i=0 ; i<num_texlights ; i++)
	{
		if (!Q_strcasecmp (name, texlights[i].name))
		{
			VectorCopy( texlights[i].value, result );
			return;
		}
	}
}

/*
=======================================================================

MAKE FACES

=======================================================================
*/

/*
=============
WindingFromFace
=============
*/
winding_t	*WindingFromFace (dface_t *f, const Vector& origin )
{
	int			i;
	int			se;
	dvertex_t	*dv;
	int			v;
	winding_t	*w;

	w = AllocWinding (f->numedges);
	w->numpoints = f->numedges;

	for (i=0 ; i<f->numedges ; i++)
	{
		se = dsurfedges[f->firstedge + i];
		if (se < 0)
			v = dedges[-se].v[1];
		else
			v = dedges[se].v[0];

		dv = &dvertexes[v];
		VectorAdd (dv->point, origin, w->p[i]);
	}

	RemoveColinearPoints (w);

	return w;
}

/*
=============
BaseLightForFace
=============
*/
void BaseLightForFace( dface_t *f, Vector& light, float *parea, Vector& reflectivity )
{
	texinfo_t	*tx;
	dtexdata_t	*texdata;

	//
	// check for light emited by texture
	//
	tx = &texinfo[f->texinfo];
	texdata = &dtexdata[tx->texdata];

	LightForTexture (TexDataStringTable_GetString( texdata->nameStringTableID ), light);


	*parea = texdata->height * texdata->width;

	VectorScale( texdata->reflectivity, reflectivityScale, reflectivity );
	
	// always keep this less than 1 or the solution will not converge
	for ( int i = 0; i < 3; i++ )
	{
		if ( reflectivity[i] > 0.99 )
			reflectivity[i] = 0.99;
	}
}

qboolean IsSky (dface_t *f)
{
	texinfo_t	*tx;

	tx = &texinfo[f->texinfo];
	if (tx->flags & SURF_SKY)
		return true;
	return false;
}

#ifdef STATIC_FOG
/*=============
IsFog
=============*/
qboolean IsFog( dface_t *f )
{
	texinfo_t	*tx;

	tx = &texinfo[f->texinfo];

    // % denotes a fog texture
    if( tx->texture[0] == '%' )
		return true;

	return false;
}
#endif


void ProcessSkyCameras()
{
	int i;
	num_sky_cameras = 0;
	for (i = 0; i < numareas; ++i)
	{
		area_sky_cameras[i] = -1;
	}

	for (i = 0; i < num_entities; ++i)
	{
		entity_t *e = &entities[i];
		const char *name = ValueForKey (e, "classname");
		if (stricmp (name, "sky_camera"))
			continue;

		Vector origin;
		GetVectorForKey( e, "origin", origin );
		int node = PointLeafnum( origin );
		int area = -1;
		if (node >= 0 && node < numleafs) area = dleafs[node].area;
		float scale = FloatForKey( e, "scale" );

		if (scale > 0.0f)
		{
			sky_cameras[num_sky_cameras].origin = origin;
			sky_cameras[num_sky_cameras].sky_to_world = scale;
			sky_cameras[num_sky_cameras].world_to_sky = 1.0f / scale;
			sky_cameras[num_sky_cameras].area = area;

			if (area >= 0 && area < numareas)
			{
				area_sky_cameras[area] = num_sky_cameras;
			}

			++num_sky_cameras;
		}
	}

}


/*
=============
MakePatchForFace
=============
*/
float	totalarea;
void MakePatchForFace (int fn, winding_t *w)
{
	dface_t     *f = g_pFaces + fn;
	float	    area;
	CPatch		*patch;
	Vector		centroid(0,0,0);
	int			i, j;
	texinfo_t	*tx;

    // get texture info
    tx = &texinfo[f->texinfo];

	// No patches at all for fog!
#ifdef STATIC_FOG
	if ( IsFog( f ) )
		return;
#endif

	// the sky needs patches or the form factors don't work out correctly
	// if (IsSky( f ) )
	// 	return;

	area = WindingArea (w);
	if (area <= 0)
	{
		num_degenerate_faces++;
		// Msg("degenerate face\n");
		return;
	}

	totalarea += area;

	// get a patch
	intp ndxPatch = g_Patches.AddToTail();
	patch = &g_Patches[ndxPatch];
	memset( patch, 0, sizeof( CPatch ) );
	patch->ndxNext = g_Patches.InvalidIndex();
	patch->ndxNextParent = g_Patches.InvalidIndex();
	patch->ndxNextClusterChild = g_Patches.InvalidIndex();
	patch->child1 = g_Patches.InvalidIndex();
	patch->child2 = g_Patches.InvalidIndex();
	patch->parent = g_Patches.InvalidIndex();
	patch->needsBumpmap = tx->flags & SURF_BUMPLIGHT ? true : false;

	// link and save patch data
	patch->ndxNext = g_FacePatches.Element( fn );
	g_FacePatches[fn] = ndxPatch;
//	patch->next = face_g_Patches[fn];
//	face_g_Patches[fn] = patch;

	// compute a separate scale for chop - since the patch "scale" is the texture scale
	// we want textures with higher resolution lighting to be chopped up more
	float chopscale[2];
	chopscale[0] = chopscale[1] = 16.0f;
    if ( texscale )
    {
        // Compute the texture "scale" in s,t
        for( i=0; i<2; i++ )
        {
            patch->scale[i] = 0.0f;
			chopscale[i] = 0.0f;
            for( j=0; j<3; j++ )
			{
                patch->scale[i] += 
					tx->textureVecsTexelsPerWorldUnits[i][j] * 
					tx->textureVecsTexelsPerWorldUnits[i][j];
                chopscale[i] += 
					tx->lightmapVecsLuxelsPerWorldUnits[i][j] * 
					tx->lightmapVecsLuxelsPerWorldUnits[i][j];
			}
            patch->scale[i] = sqrt( patch->scale[i] );
			chopscale[i] = sqrt( chopscale[i] );
        }
	}
    else
	{
		patch->scale[0] = patch->scale[1] = 1.0f;
	}

	patch->area = area;
 
	patch->sky = IsSky( f );

	// chop scaled up lightmaps coarser
	patch->luxscale = ((chopscale[0]+chopscale[1])/2);
	patch->chop = maxchop;


#ifdef STATIC_FOG
    patch->fog = FALSE;
#endif

	patch->winding = w;

	patch->plane = &dplanes[f->planenum];

	// make a new plane to adjust for origined bmodels
	if (face_offset[fn][0] || face_offset[fn][1] || face_offset[fn][2] )
	{	
		dplane_t	*pl;

		// origin offset faces must create new planes
		if (numplanes + fakeplanes >= MAX_MAP_PLANES)
		{
			Error ("numplanes + fakeplanes >= MAX_MAP_PLANES");
		}
		pl = &dplanes[numplanes + fakeplanes];
		fakeplanes++;

		*pl = *(patch->plane);
		pl->dist += DotProduct (face_offset[fn], pl->normal);
		patch->plane = pl;
	}

	patch->faceNumber = fn;
	WindingCenter (w, patch->origin);

	// Save "center" for generating the face normals later.
	VectorSubtract( patch->origin, face_offset[fn], face_centroids[fn] ); 

	VectorCopy( patch->plane->normal, patch->normal );

	WindingBounds (w, patch->face_mins, patch->face_maxs);
	VectorCopy( patch->face_mins, patch->mins );
	VectorCopy( patch->face_maxs, patch->maxs );

	BaseLightForFace( f, patch->baselight, &patch->basearea, patch->reflectivity );

	// Chop all texlights very fine.
	if ( !VectorCompare( patch->baselight, vec3_origin ) )
	{
		// patch->chop = do_extra ? maxchop / 2 : maxchop;
		tx->flags |= SURF_LIGHT;
	}

	// get rid of do extra functionality on displacement surfaces
	if( ValidDispFace( f ) )
	{
		patch->chop = maxchop;
	}

	// FIXME: If we wanted to add a dependency from vrad to the material system,
	// we could do this. It would add a bunch of file accesses, though:

	/*
	// Check for a material var which would override the patch chop
	bool bFound;
	const char *pMaterialName = TexDataStringTable_GetString( dtexdata[ tx->texdata ].nameStringTableID );
	MaterialSystemMaterial_t hMaterial = FindMaterial( pMaterialName, &bFound, false );
	if ( bFound )
	{
		const char *pChopValue = GetMaterialVar( hMaterial, "%chop" );
		if ( pChopValue )
		{
			float flChopValue;
			if ( sscanf( pChopValue, "%f", &flChopValue ) > 0 )
			{
				patch->chop = flChopValue;
			}
		}
	}
	*/
}


entity_t *EntityForModel (int modnum)
{
	char	name[16];
	V_sprintf_safe (name, "*%i", modnum);

	// search the entities for one using modnum
	for (int i=0 ; i<num_entities ; i++)
	{
		const char *s = ValueForKey (&entities[i], "model");
		if (!strcmp (s, name))
			return &entities[i];
	}

	return &entities[0];
}

/*
=============
MakePatches
=============
*/
void MakePatches (void)
{
	int		    i, j;
	dface_t	    *f;
	int		    fn;
	winding_t	*w;
	dmodel_t	*mod;
	Vector		origin;
	entity_t	*ent;

	ParseEntities ();
	qprintf ("%i faces\n", numfaces);

	for (i=0 ; i<nummodels ; i++)
	{
		mod = dmodels+i;
		ent = EntityForModel (i);
		VectorCopy (vec3_origin, origin);

		// bmodels with origin brushes need to be offset into their
		// in-use position
		GetVectorForKey (ent, "origin", origin);

		for (j=0 ; j<mod->numfaces ; j++)
		{
			fn = mod->firstface + j;
			face_entity[fn] = ent;
			VectorCopy (origin, face_offset[fn]);
			f = &g_pFaces[fn];
			if( f->dispinfo == -1 )
			{
	            w = WindingFromFace (f, origin );
		        MakePatchForFace( fn, w );
			}
		}
	}

	if (num_degenerate_faces > 0)
	{
		qprintf("%d degenerate faces\n", num_degenerate_faces );
	}

	qprintf ("%i square feet [%.2f square inches]\n", (int)(totalarea/144), totalarea );

	// make the displacement surface patches
	StaticDispMgr()->MakePatches();
}

/*
=======================================================================

SUBDIVIDE

=======================================================================
*/


//-----------------------------------------------------------------------------
// Purpose: does this surface take/emit light
//-----------------------------------------------------------------------------
bool PreventSubdivision( CPatch *patch )
{
	dface_t *f = g_pFaces + patch->faceNumber;
	texinfo_t *tx = &texinfo[f->texinfo];

	if (tx->flags & SURF_NOCHOP)
		return true;

	if (tx->flags & SURF_NOLIGHT && !(tx->flags & SURF_LIGHT))
		return true;

	return false;
}


//-----------------------------------------------------------------------------
// Purpose: subdivide the "parent" patch
//-----------------------------------------------------------------------------
intp CreateChildPatch( int nParentIndex, winding_t *pWinding, float flArea, const Vector &vecCenter )
{
	intp nChildIndex = g_Patches.AddToTail();

	CPatch *child = &g_Patches[nChildIndex];
	CPatch *parent = &g_Patches[nParentIndex];

	// copy all elements of parent patch to children
	*child = *parent;

	// Set up links
	child->ndxNext = g_Patches.InvalidIndex();
	child->ndxNextParent = g_Patches.InvalidIndex();
	child->ndxNextClusterChild = g_Patches.InvalidIndex();
	child->child1 = g_Patches.InvalidIndex();
	child->child2 = g_Patches.InvalidIndex();
	child->parent = nParentIndex;
	child->m_IterationKey = 0;

	child->winding = pWinding;
	child->area = flArea;

	VectorCopy( vecCenter, child->origin );
	if ( ValidDispFace( g_pFaces + child->faceNumber ) )
	{
		// shouldn't get here anymore!!
		Msg( "SubdividePatch: Error - Should not be here!\n" );
		StaticDispMgr()->GetDispSurfNormal( child->faceNumber, child->origin, child->normal, true );
	}
	else
	{
		GetPhongNormal( child->faceNumber, child->origin, child->normal );
	}

	child->planeDist = child->plane->dist;
	WindingBounds(child->winding, child->mins, child->maxs);

	if ( !VectorCompare( child->baselight, vec3_origin ) )
	{
		// don't check edges on surf lights
		return nChildIndex;
	}

	// Subdivide patch towards minchop if on the edge of the face
	Vector total;
	VectorSubtract( child->maxs, child->mins, total );
	VectorScale( total, child->luxscale, total );
	if ( child->chop > minchop && (total[0] < child->chop) && (total[1] < child->chop) && (total[2] < child->chop) )
	{
		for ( int i=0; i<3; ++i )
		{
			if ( (child->face_maxs[i] == child->maxs[i] || child->face_mins[i] == child->mins[i] )
			  && total[i] > minchop )
			{
				child->chop = max( minchop, child->chop / 2 );
				break;
			}
		}
	}

	return nChildIndex;
}


//-----------------------------------------------------------------------------
// Purpose: subdivide the "parent" patch
//-----------------------------------------------------------------------------
void SubdividePatch( intp ndxPatch )
{
	winding_t *w, *o1, *o2;
	Vector	total;
	Vector	split;
	vec_t	dist;
	vec_t	widest = -1;
	int		i, widest_axis = -1;
	bool	bSubdivide = false;

	// get the current patch
	CPatch *patch = &g_Patches.Element( ndxPatch );
	if ( !patch )
		return;

	// never subdivide sky patches
	if ( patch->sky )
		return;

	// get the patch winding
	w = patch->winding;

	// subdivide along the widest axis
	VectorSubtract (patch->maxs, patch->mins, total);
	VectorScale( total, patch->luxscale, total );
	for (i=0 ; i<3 ; i++)
	{
		if ( total[i] > widest )
		{
			widest_axis = i;
			widest = total[i];
		}

		if ( (total[i] >= patch->chop) && (total[i] >= minchop) )
		{
			bSubdivide = true;
		}
	}

	if ((!bSubdivide) && widest_axis != -1)
	{
		// make more square
		if (total[widest_axis] > total[(widest_axis + 1) % 3] * 2 && total[widest_axis] > total[(widest_axis + 2) % 3] * 2)
		{
			if (patch->chop > minchop)
			{
				bSubdivide = true;
				patch->chop = max( minchop, patch->chop / 2 );
			}
		}
	}

	if ( !bSubdivide )
		return;

	// split the winding
	VectorCopy (vec3_origin, split);
	split[widest_axis] = 1;
	dist = (patch->mins[widest_axis] + patch->maxs[widest_axis])*0.5f;
	ClipWindingEpsilon (w, split, dist, ON_EPSILON, &o1, &o2);

	// calculate the area of the patches to see if they are "significant"
	Vector center1, center2;
	float area1 = WindingAreaAndBalancePoint( o1, center1 );
	float area2 = WindingAreaAndBalancePoint( o2, center2 );

	if( area1 == 0 || area2 == 0 )
	{
		Msg( "zero area child patch\n" );
		return;
	}

	// create new child patches
	intp ndxChild1Patch = CreateChildPatch( ndxPatch, o1, area1, center1 );
	intp ndxChild2Patch = CreateChildPatch( ndxPatch, o2, area2, center2 );

	// FIXME: This could go into CreateChildPatch if child1, child2 were stored in the patch as child[0], child[1]
	patch = &g_Patches.Element( ndxPatch );
	patch->child1 = ndxChild1Patch;
	patch->child2 = ndxChild2Patch;		

	SubdividePatch( ndxChild1Patch );
	SubdividePatch( ndxChild2Patch );
}


/*
=============
SubdividePatches
=============
*/
void SubdividePatches (void)
{
	unsigned		i, num;

	if (numbounce == 0)
		return;

	unsigned int uiPatchCount = g_Patches.Count();
	qprintf ("%u patches before subdivision\n", uiPatchCount);

	for (i = 0; i < uiPatchCount; i++)
	{
		CPatch *pCur = &g_Patches.Element( i );
		pCur->planeDist = pCur->plane->dist;

		pCur->ndxNextParent = faceParents.Element( pCur->faceNumber );
		faceParents[pCur->faceNumber] = pCur - g_Patches.Base();
	}

	for (i=0 ; i< uiPatchCount; i++)
	{
		CPatch *patch = &g_Patches.Element( i );
		patch->parent = -1;
		if ( PreventSubdivision(patch) )
			continue;

		if (!do_fast)
		{
			if( g_pFaces[patch->faceNumber].dispinfo == -1 )
			{
				SubdividePatch( i );
			}
			else
			{
				StaticDispMgr()->SubdividePatch( i );
			}
		}
	}

	// fixup next pointers
	for (i = 0; i < (unsigned)numfaces; i++)
	{
		g_FacePatches[i] = g_FacePatches.InvalidIndex();
	}

	uiPatchCount = g_Patches.Count();
	for (i = 0; i < uiPatchCount; i++)
	{
		CPatch *pCur = &g_Patches.Element( i );
		pCur->ndxNext = g_FacePatches.Element( pCur->faceNumber );
		g_FacePatches[pCur->faceNumber] = pCur - g_Patches.Base();

#if 0
		CPatch *prev;
		prev = face_g_Patches[g_Patches[i].faceNumber];
		g_Patches[i].next = prev;
		face_g_Patches[g_Patches[i].faceNumber] = &g_Patches[i];
#endif
	}

	// Cache off the leaf number:
	// We have to do this after subdivision because some patches span leaves.
	// (only the faces for model #0 are split by it's BSP which is what governs the PVS, and the leaves we're interested in)
	// Sub models (1-255) are only split for the BSP that their model forms.
	// When those patches are subdivided their origins can end up in a different leaf.
	// The engine will split (clip) those faces at run time to the world BSP because the models
	// are dynamic and can be moved.  In the software renderer, they must be split exactly in order
	// to sort per polygon.
	for ( i = 0; i < uiPatchCount; i++ )
	{
		g_Patches[i].clusterNumber = ClusterFromPoint( g_Patches[i].origin );

		//
		// test for point in solid space (can happen with detail and displacement surfaces)
		//
		if( g_Patches[i].clusterNumber == -1 )
		{
			for( int j = 0; j < g_Patches[i].winding->numpoints; j++ )
			{
				int clusterNumber = ClusterFromPoint( g_Patches[i].winding->p[j] );
				if( clusterNumber != -1 )
				{
					g_Patches[i].clusterNumber = clusterNumber;
					break;
				}
			}
		}
	}

	// build the list of patches that need to be lit
	for ( num = 0; num < uiPatchCount; num++ )
	{
		// do them in reverse order
		i = uiPatchCount - num - 1;

		// skip patches with children
		CPatch *pCur = &g_Patches.Element( i );
		if( pCur->child1 == g_Patches.InvalidIndex() )
		{
			if( pCur->clusterNumber != - 1 )
			{
				pCur->ndxNextClusterChild = clusterChildren.Element( pCur->clusterNumber );
				clusterChildren[pCur->clusterNumber] = pCur - g_Patches.Base();
			}
		}

#if 0
		if (g_Patches[i].child1 == g_Patches.InvalidIndex() )
		{
			if( g_Patches[i].clusterNumber != -1 )
			{
				g_Patches[i].nextclusterchild = cluster_children[g_Patches[i].clusterNumber];
				cluster_children[g_Patches[i].clusterNumber] = &g_Patches[i];
			}
		}
#endif
	}

	qprintf ("%i patches after subdivision\n", uiPatchCount);
}


//=====================================================================

/*
=============
MakeScales

  This is the primary time sink.
  It can be run multi threaded.
=============
*/
std::atomic_int	total_transfer;
std::atomic_int max_transfer;


//-----------------------------------------------------------------------------
// Purpose: Computes the form factor from a polygon patch to a differential patch
//          using formula 81 of Philip Dutre's Global Illumination Compendium,
//          phil@graphics.cornell.edu, http://www.graphics.cornell.edu/~phil/GI/
//-----------------------------------------------------------------------------
float FormFactorPolyToDiff ( CPatch *pPolygon, CPatch* pDifferential )
{
	winding_t *pWinding = pPolygon->winding;

	float flFormFactor = 0.0f;

	for ( int iPoint = 0; iPoint < pWinding->numpoints; iPoint++ )
	{
		int iNextPoint = ( iPoint < pWinding->numpoints - 1 ) ? iPoint + 1 : 0;

		Vector vGammaVector, vVector1, vVector2;
		VectorSubtract( pWinding->p[ iPoint ],		pDifferential->origin, vVector1 );
		VectorSubtract( pWinding->p[ iNextPoint ],	pDifferential->origin, vVector2 );
		VectorNormalize( vVector1 );
		VectorNormalize( vVector2 );
		CrossProduct( vVector1, vVector2, vGammaVector );
		float flSinAlpha = VectorNormalize( vGammaVector );
		if (flSinAlpha < -1.0f || flSinAlpha > 1.0f)
			return 0.0f;
		vGammaVector *= asin( flSinAlpha );

		flFormFactor += DotProduct( vGammaVector, pDifferential->normal );
	}

	flFormFactor *= ( 0.5f / pPolygon->area ); // divide by pi later, multiply by area later

	return flFormFactor;
}


//-----------------------------------------------------------------------------
// Purpose: Computes the form factor from a differential element to a differential
//          element.  This is okay when the distance between patches is 5 times
//          greater than patch size.  Lecture slides by Pat Hanrahan,
//          http://graphics.stanford.edu/courses/cs348b-00/lectures/lecture17/radiosity.2.pdf
//-----------------------------------------------------------------------------
float FormFactorDiffToDiff ( CPatch *pDiff1, CPatch* pDiff2 )
{
	Vector vDelta;
	VectorSubtract( pDiff1->origin, pDiff2->origin, vDelta );
	float flLength = VectorNormalize( vDelta );

	return -DotProduct( vDelta, pDiff1->normal ) * DotProduct( vDelta, pDiff2->normal ) / ( flLength * flLength );
}



void MakeTransfer( int ndxPatch1, int ndxPatch2, transfer_t *all_transfers )
{
	vec_t	scale;
	float	trans;
	transfer_t *transfer;

	//
	// get patches
	//
	if( ndxPatch1 == g_Patches.InvalidIndex() || ndxPatch2 == g_Patches.InvalidIndex() )
		return;

	CPatch *pPatch1 = &g_Patches.Element( ndxPatch1 );
	CPatch *pPatch2 = &g_Patches.Element( ndxPatch2 );

	if (IsSky( &g_pFaces[ pPatch2->faceNumber ] ) )
		return;

	// overflow check!
	if ( pPatch1->numtransfers >= MAX_PATCHES)
	{
		return;
	}

	// hack for patch areas that area <= 0 (degenerate)
	if ( pPatch2->area <= 0)
	{
		return;
	}

	transfer = &all_transfers[pPatch1->numtransfers];

	scale = FormFactorDiffToDiff( pPatch2, pPatch1 );

	// patch normals may be > 90 due to smoothing groups
	if (scale <= 0)
	{
		//Msg("scale <= 0\n");
		return;
	}

	// Test 5 times rule
	Vector vDelta;
	VectorSubtract( pPatch1->origin, pPatch2->origin, vDelta );
	float flThreshold = ( M_PI_F * 0.04f ) * DotProduct( vDelta, vDelta );

	if (flThreshold < pPatch2->area)
	{
		scale = FormFactorPolyToDiff( pPatch2, pPatch1 );
		if (scale <= 0.0)
			return;
	}

	trans = (pPatch2->area*scale);

	if (trans <= TRANSFER_EPSILON)
	{
		return;
	}

	transfer->patch = pPatch2 - g_Patches.Base();

	// FIXME: why is this not trans?
	transfer->transfer = trans;

#if 0
	// DEBUG! Dump patches and transfer connection for displacements.  This creates a lot of data, so only
	// use it when you really want it - that is why it is #if-ed out.
	if ( g_bDumpPatches )
	{
		if ( !pFpTrans )
		{
			pFpTrans = g_pFileSystem->Open( "trans.txt", "w" );
		}
		Vector light = pPatch1->totallight.light[0] + pPatch1->directlight;
		WriteWinding( pFpTrans, pPatch1->winding, light );
		light = pPatch2->totallight.light[0] + pPatch2->directlight;
		WriteWinding( pFpTrans, pPatch2->winding, light );
		WriteLine( pFpTrans, pPatch1->origin, pPatch2->origin, Vector( 255, 0, 255 ) );
	}
#endif

	pPatch1->numtransfers++;
}


void MakeScales ( int ndxPatch, transfer_t *all_transfers )
{
	int		j;
	float	total;
	transfer_t	*t, *t2;
	total = 0;

	if( ndxPatch == g_Patches.InvalidIndex() )
		return;
	CPatch *patch = &g_Patches.Element( ndxPatch );

	// copy the transfers out
	if (patch->numtransfers)
	{
		if (patch->numtransfers > max_transfer)
		{
			max_transfer = patch->numtransfers;
		}


		patch->transfers = ( transfer_t* )calloc (1, patch->numtransfers * sizeof(transfer_t));
		if (!patch->transfers)
			Error ("Memory allocation failure");

		// get total transfer energy
		t2 = all_transfers;

		// overflow check!
		for (j=0 ; j<patch->numtransfers ; j++, t2++)
		{
			total += t2->transfer;
		}

		// the total transfer should be PI, but we need to correct errors due to overlaping surfaces
		if (total > M_PI)
			total = 1.0f/total;
		else	
			total = 1.0f/M_PI;

		t = patch->transfers;
		t2 = all_transfers;
		for (j=0 ; j<patch->numtransfers ; j++, t++, t2++)
		{
			t->transfer = t2->transfer*total;
			t->patch = t2->patch;
		}
		if (patch->numtransfers > max_transfer)
		{
			max_transfer = patch->numtransfers;
		}
	}
	else
	{
		// Error - patch has no transfers
		// patch->totallight[2] = 255;
	}

	total_transfer += patch->numtransfers;
}

/*
=============
WriteWorld
=============
*/
void WriteWorld (char *name, int iBump)
{
	unsigned	j;
	FileHandle_t out;
	CPatch		*patch;

	out = g_pFileSystem->Open( name, "w" );
	if (!out)
		Error ("Couldn't open %s", name);

	unsigned int uiPatchCount = g_Patches.Count();
	for (j=0; j<uiPatchCount; j++)
	{
		patch = &g_Patches.Element( j );

		// skip parent patches
		if (patch->child1 != g_Patches.InvalidIndex() )
			continue;

		if( patch->clusterNumber == -1 )
		{
			Vector vGreen;
			VectorClear( vGreen );
			vGreen[1] = 256.0f;
			WriteWinding( out, patch->winding, vGreen );
		}
		else
		{
			Vector light = patch->totallight.light[iBump] + patch->directlight;
			WriteWinding( out, patch->winding, light );
			if( bDumpNormals )
			{
				WriteNormal( out, patch->origin, patch->plane->normal, 15.0f, patch->plane->normal * 255.0f );
			}
		}
	}

	g_pFileSystem->Close( out );
}

void WriteRTEnv (char *name)
{
	FileHandle_t out;

	out = g_pFileSystem->Open( name, "w" );
	if (!out)
		Error ("Couldn't open %s", name);

	winding_t *triw = AllocWinding( 3 );
	triw->numpoints = 3;

	for( int i = 0; i < g_RtEnv.OptimizedTriangleList.Count(); i++ )
	{
		const auto &v = g_RtEnv.OptimizedTriangleList[i];
		triw->p[0] = v.Vertex( 0);
		triw->p[1] = v.Vertex( 1);
		triw->p[2] = v.Vertex( 2);

		int id = v.m_Data.m_GeometryData.m_nTriangleID;

		Vector color(0, 0, 0);
		if (id & TRACE_ID_OPAQUE) color.Init(0, 255, 0);
		if (id & TRACE_ID_SKY) color.Init(0, 0, 255);
		if (id & TRACE_ID_STATICPROP) color.Init(255, 0, 0);

		WriteWinding(out, triw, color);
	}
	FreeWinding(triw);

	g_pFileSystem->Close( out );
}

void WriteWinding (FileHandle_t out, winding_t *w, Vector& color )
{
	int			i;

	CmdLib_FPrintf (out, "%i\n", w->numpoints);
	for (i=0 ; i<w->numpoints ; i++)
	{
		CmdLib_FPrintf (out, "%5.2f %5.2f %5.2f %5.3f %5.3f %5.3f\n",
			w->p[i][0],
			w->p[i][1],
			w->p[i][2],
			color[ 0 ] / 256,
			color[ 1 ] / 256,
			color[ 2 ] / 256 );
	}
}


void WriteNormal( FileHandle_t out, Vector const &nPos, Vector const &nDir, 
				  float length, Vector const &color )
{
	CmdLib_FPrintf( out, "2\n" );
	CmdLib_FPrintf( out, "%5.2f %5.2f %5.2f %5.3f %5.3f %5.3f\n", 
		nPos.x, nPos.y, nPos.z,
		color.x / 256, color.y / 256, color.z / 256 );
	CmdLib_FPrintf( out, "%5.2f %5.2f %5.2f %5.3f %5.3f %5.3f\n", 
		nPos.x + ( nDir.x * length ), 
		nPos.y + ( nDir.y * length ), 
		nPos.z + ( nDir.z * length ),
		color.x / 256, color.y / 256, color.z / 256 );
}

void WriteLine( FileHandle_t out, const Vector &vecPos1, const Vector &vecPos2, const Vector &color )
{
	CmdLib_FPrintf( out, "2\n" );
	CmdLib_FPrintf( out, "%5.2f %5.2f %5.2f %5.3f %5.3f %5.3f\n", 
		vecPos1.x, vecPos1.y, vecPos1.z,
		color.x / 256, color.y / 256, color.z / 256 );
	CmdLib_FPrintf( out, "%5.2f %5.2f %5.2f %5.3f %5.3f %5.3f\n", 
		vecPos2.x, vecPos2.y, vecPos2.z,
		color.x / 256, color.y / 256, color.z / 256 );
}

void WriteTrace( const char *pFileName, const FourRays &rays, const RayTracingResult& result )
{
	FileHandle_t out;

	out = g_pFileSystem->Open( pFileName, "a" );
	if (!out)
		Error ("Couldn't open %s", pFileName);

	// Draws rays
	for ( int i = 0; i < 4; ++i )
	{
		Vector vecOrigin = rays.origin.Vec(i);
		Vector vecEnd = rays.direction.Vec(i);
		VectorNormalize( vecEnd );
		vecEnd *= SubFloat( result.HitDistance, i );
		vecEnd += vecOrigin;
		WriteLine( out, vecOrigin, vecEnd, Vector( 256, 0, 0 ) );
		WriteNormal( out, vecEnd, result.surface_normal.Vec(i), 10.0f, Vector( 256, 265, 0 ) );
	}

	g_pFileSystem->Close( out );
}


/*
=============
CollectLight
=============
*/
// patch's totallight += new light received to each patch
// patch's emitlight = addlight (newly received light from GatherLight)
// patch's addlight = 0
// pull received light from children.
void CollectLight( Vector& total )
{
	int i, j;
	CPatch	*patch;

	VectorFill( total, 0 );

	// process patches in reverse order so that children are processed before their parents
	unsigned int uiPatchCount = g_Patches.Count();
	for( i = uiPatchCount - 1; i >= 0; i-- )
	{
		patch = &g_Patches.Element( i );
		int normalCount = patch->needsBumpmap ? NUM_BUMP_VECTS+1 : 1;
		// sky's never collect light, it is just dropped
		if (patch->sky)
		{
			VectorFill( emitlight[ i ], 0 );
		}
		else if ( patch->child1 == g_Patches.InvalidIndex() )
		{
			// This is a leaf node.
			for ( j = 0; j < normalCount; j++ )
			{
				VectorAdd( patch->totallight.light[j], addlight[i].light[j], patch->totallight.light[j] );
			}
			VectorCopy( addlight[i].light[0], emitlight[i] );
			VectorAdd( total, emitlight[i], total );
		}
		else
		{
			// This is an interior node.
			// Pull received light from children.
			CPatch *child1 = &g_Patches[patch->child1];
			CPatch *child2 = &g_Patches[patch->child2];

			float s1 = child1->area / (child1->area + child2->area);
			float s2 = child2->area / (child1->area + child2->area);

			// patch->totallight = s1 * child1->totallight + s2 * child2->totallight
			for ( j = 0; j < normalCount; j++ )
			{
				VectorScale( child1->totallight.light[j], s1, patch->totallight.light[j] );
				VectorMA( patch->totallight.light[j], s2, child2->totallight.light[j], patch->totallight.light[j] );
			}

			// patch->emitlight = s1 * child1->emitlight + s2 * child2->emitlight
			VectorScale( emitlight[patch->child1], s1, emitlight[i] );
			VectorMA( emitlight[i], s2, emitlight[patch->child2], emitlight[i] );
		}
		for ( j = 0; j < NUM_BUMP_VECTS+1; j++ )
		{
			VectorFill( addlight[ i ].light[j], 0 );
		}
	}
}

/*
=============
GatherLight

Get light from other patches
  Run multi-threaded
=============
*/

extern void GetBumpNormals( const float* sVect, const float* tVect, const Vector& flatNormal, 
					 const Vector& phongNormal, Vector (&bumpNormals)[NUM_BUMP_VECTS] );


void PreGetBumpNormalsForDisp( texinfo_t *pTexinfo, Vector &vecU, Vector &vecV, Vector &vecNormal )
{
	Vector vecTexU( pTexinfo->textureVecsTexelsPerWorldUnits[0][0], pTexinfo->textureVecsTexelsPerWorldUnits[0][1], pTexinfo->textureVecsTexelsPerWorldUnits[0][2] );
	Vector vecTexV( pTexinfo->textureVecsTexelsPerWorldUnits[1][0], pTexinfo->textureVecsTexelsPerWorldUnits[1][1], pTexinfo->textureVecsTexelsPerWorldUnits[1][2] );
	Vector vecLightU( pTexinfo->lightmapVecsLuxelsPerWorldUnits[0][0], pTexinfo->lightmapVecsLuxelsPerWorldUnits[0][1], pTexinfo->lightmapVecsLuxelsPerWorldUnits[0][2] );
	Vector vecLightV( pTexinfo->lightmapVecsLuxelsPerWorldUnits[1][0], pTexinfo->lightmapVecsLuxelsPerWorldUnits[1][1], pTexinfo->lightmapVecsLuxelsPerWorldUnits[1][2] );

	VectorNormalize( vecTexU );
	VectorNormalize( vecTexV );
	VectorNormalize( vecLightU );
	VectorNormalize( vecLightV );

	bool bDoConversion = false;
	if ( fabs( vecTexU.Dot( vecLightU ) ) < 0.999f )
	{
		bDoConversion = true;
	}

	if ( fabs( vecTexV.Dot( vecLightV ) ) < 0.999f )
	{
		bDoConversion = true;
	}

	if ( bDoConversion )
	{
		matrix3x4_t matTex( vecTexU, vecTexV, vecNormal, vec3_origin );
		matrix3x4_t matLight( vecLightU, vecLightV, vecNormal, vec3_origin );
		matrix3x4_t matTmp;
		ConcatTransforms ( matLight, matTex, matTmp );
		MatrixGetColumn( matTmp, 0, vecU );
		MatrixGetColumn( matTmp, 1, vecV );
		MatrixGetColumn( matTmp, 2, vecNormal );

		Assert( fabs( vecTexU.Dot( vecTexV ) ) <= 0.001f );
		return;
	}

	vecU = vecTexU;
	vecV = vecTexV;
}

void GatherLight (int threadnum, void *pUserData)
{
	int			i, j, k;
	transfer_t	*trans;
	int			num;
	CPatch		*patch;
	Vector		sum, v;

	while (1)
	{
		j = GetThreadWork ();
		if (j == -1)
			break;

		patch = &g_Patches[j];

		trans = patch->transfers;
		num = patch->numtransfers;
		if ( patch->needsBumpmap )
		{
			Vector delta;
			Vector bumpSum[NUM_BUMP_VECTS+1];
			Vector normals[NUM_BUMP_VECTS+1];

			// Disps
			bool bDisp = ( g_pFaces[patch->faceNumber].dispinfo != -1 ); 
			if ( bDisp )
			{
				normals[0] = patch->normal;
				texinfo_t *pTexinfo = &texinfo[g_pFaces[patch->faceNumber].texinfo];
				Vector vecTexU, vecTexV;
				PreGetBumpNormalsForDisp( pTexinfo, vecTexU, vecTexV, normals[0] );
				
				Vector bumpNormals[NUM_BUMP_VECTS];
				// use facenormal along with the smooth normal to build the three bump map vectors
				GetBumpNormals( vecTexU, vecTexV, normals[0], normals[0], bumpNormals );
				memcpy( &normals[1], bumpNormals, sizeof(bumpNormals) );
			}
			else
			{
				GetPhongNormal( patch->faceNumber, patch->origin, normals[0] );

				texinfo_t *pTexinfo = &texinfo[g_pFaces[patch->faceNumber].texinfo];
				// use facenormal along with the smooth normal to build the three bump map vectors
				
				Vector bumpNormals[NUM_BUMP_VECTS];
				GetBumpNormals( pTexinfo->textureVecsTexelsPerWorldUnits[0], 
					pTexinfo->textureVecsTexelsPerWorldUnits[1], patch->normal, 
					normals[0], bumpNormals );
				memcpy( &normals[1], bumpNormals, sizeof(bumpNormals) );
			}

			// force the base lightmap to use the flat normal instead of the phong normal
			// FIXME: why does the patch not use the phong normal?
			normals[0] = patch->normal;

			for ( i = 0; i < NUM_BUMP_VECTS+1; i++ )
			{
				VectorFill( bumpSum[i], 0 );
			}

			float dot;
			for (k=0 ; k<num ; k++, trans++)
			{
				CPatch *patch2 = &g_Patches[trans->patch];

				// get vector to other patch
				VectorSubtract (patch2->origin, patch->origin, delta);
				VectorNormalize (delta);
				// find light emitted from other patch
				for(i=0; i<3; i++)
				{
					v[i] = emitlight[trans->patch][i] * patch2->reflectivity[i];
				}
				// remove normal already factored into transfer steradian
				float scale = 1.0f / DotProduct (delta, patch->normal);
				VectorScale( v, trans->transfer * scale, v );
				
				Vector bumpTransfer;
				for ( i = 0; i < NUM_BUMP_VECTS+1; i++ )
				{
					dot = DotProduct( delta, normals[i] );
					if ( dot <= 0 )
					{
//						Assert( i > 0 ); // if this hits, then the transfer shouldn't be here.  It doesn't face the flat normal of this face!
						continue;
					}
					bumpTransfer = v * dot;
					VectorAdd( bumpSum[i], bumpTransfer, bumpSum[i] );
				}
			}
			for ( i = 0; i < NUM_BUMP_VECTS+1; i++ )
			{
				VectorCopy( bumpSum[i], addlight[j].light[i] );
			}
		}
		else
		{
			VectorFill( sum, 0 );
			for (k=0 ; k<num ; k++, trans++)
			{
				for(i=0; i<3; i++)
				{
					v[i] = emitlight[trans->patch][i] * g_Patches[trans->patch].reflectivity[i];
				}
				VectorScale( v, trans->transfer, v );
				VectorAdd( sum, v, sum );
			}
			VectorCopy( sum, addlight[j].light[0] );
		}
	}
}


/*
=============
BounceLight
=============
*/
void BounceLight (void)
{
	unsigned i;
	Vector	added;
	char		name[64];
	qboolean	bouncing = numbounce > 0;

	unsigned int uiPatchCount = g_Patches.Count();
	for (i=0 ; i<uiPatchCount; i++)
	{
		// totallight has a copy of the direct lighting.  Move it to the emitted light and zero it out (to integrate bounces only)
		VectorCopy( g_Patches[i].totallight.light[0], emitlight[i] );

		// NOTE: This means that only the bounced light is integrated into totallight!
		VectorFill( g_Patches[i].totallight.light[0], 0 );
	}

#if 0
	FileHandle_t dFp = g_pFileSystem->Open( "lightemit.txt", "w" );

	unsigned int uiPatchCount = g_Patches.Size();
	for (i=0 ; i<uiPatchCount; i++)
	{
		CmdLib_FPrintf( dFp, "Emit %d: %f %f %f\n", i, emitlight[i].x, emitlight[i].y, emitlight[i].z );
	}

	g_pFileSystem->Close( dFp );

	for (i=0; i<num_patches ; i++)
	{
		Vector total;

		VectorSubtract (g_Patches[i].maxs, g_Patches[i].mins, total);
		Msg("%4d %4zd %4zd %4d (%d) %.0f", i, g_Patches[i].parent, g_Patches[i].child1, g_Patches[i].child2, g_Patches[i].samples, g_Patches[i].area );
		Msg(" [%.0f %.0f %.0f]", total[0], total[1], total[2] );
		if (g_Patches[i].child1 != g_Patches.InvalidIndex() )
		{
			Vector tmp;
			VectorScale( g_Patches[i].totallight.light[0], g_Patches[i].area, tmp );

			VectorMA( tmp, -g_Patches[g_Patches[i].child1].area, g_Patches[g_Patches[i].child1].totallight.light[0], tmp );
			VectorMA( tmp, -g_Patches[g_Patches[i].child2].area, g_Patches[g_Patches[i].child2].totallight.light[0], tmp );
			// Msg("%.0f ", VectorLength( tmp ) );
			// Msg("%d ", g_Patches[i].samples - g_Patches[g_Patches[i].child1].samples - g_Patches[g_Patches[i].child2].samples );
			// Msg("%d ", g_Patches[i].samples );
		}
		Msg("\n");
	}
#endif

	i = 0;
	while ( bouncing )
	{
		// transfer light from to the leaf patches from other patches via transfers
		// this moves shooter->emitlight to receiver->addlight
		unsigned int uiPatchCount = g_Patches.Count();
		RunThreadsOn (uiPatchCount, true, GatherLight);
		// move newly received light (addlight) to light to be sent out (emitlight)
		// start at children and pull light up to parents
		// light is always received to leaf patches
		CollectLight( added );

		qprintf ("\tBounce #%i added RGB(%.0f, %.0f, %.0f)\n", i+1, added[0], added[1], added[2] );

		if ( i+1 == numbounce || (added[0] < 1.0 && added[1] < 1.0 && added[2] < 1.0) )
			bouncing = false;

		i++;
		if ( g_bDumpPatches && !bouncing && i != 1)
		{
			// dimhotepus: %i -> %u
			V_sprintf_safe (name, "bounce%u.txt", i);
			WriteWorld (name, 0);
		}
	}
}



//-----------------------------------------------------------------------------
// Purpose: Counts the number of clusters in a map with no visibility
// Output : int
//-----------------------------------------------------------------------------
int CountClusters( void )
{
	int clusterCount = 0;

	for ( int i = 0; i < numleafs; i++ )
	{
		if ( dleafs[i].cluster > clusterCount )
			clusterCount = dleafs[i].cluster;
	}

	return clusterCount + 1;
}


/*
=============
RadWorld
=============
*/
void RadWorld_Start()
{
	if (luxeldensity < 1.0f)
	{
		// Remember the old lightmap vectors.
		float oldLightmapVecs[MAX_MAP_TEXINFO][2][4];
		for (intp i = 0; i < texinfo.Count(); i++)
		{
			for( int j=0; j < 2; j++ )
			{
				for( int k=0; k < 3; k++ )
				{
					oldLightmapVecs[i][j][k] = texinfo[i].lightmapVecsLuxelsPerWorldUnits[j][k];
				}
			}
		}

		// rescale luxels to be no denser than "luxeldensity"
		for (intp i = 0; i < texinfo.Count(); i++)
		{
			texinfo_t	*tx = &texinfo[i];

			for (int j = 0; j < 2; j++ )
			{
				Vector tmp( tx->lightmapVecsLuxelsPerWorldUnits[j][0], tx->lightmapVecsLuxelsPerWorldUnits[j][1], tx->lightmapVecsLuxelsPerWorldUnits[j][2] );
				float scale = VectorNormalize( tmp );
				// only rescale them if the current scale is "tighter" than the desired scale
				// FIXME: since this writes out to the BSP file every run, once it's set high it can't be reset
				// to a lower value.
				if (fabs( scale ) > luxeldensity)
				{
					if (scale < 0)
					{
						scale = -luxeldensity;
					}
					else
					{
						scale = luxeldensity;
					}
					VectorScale( tmp, scale, tmp );
					tx->lightmapVecsLuxelsPerWorldUnits[j][0] = tmp.x;
					tx->lightmapVecsLuxelsPerWorldUnits[j][1] = tmp.y;
					tx->lightmapVecsLuxelsPerWorldUnits[j][2] = tmp.z;
				}
			}
		}
		
		UpdateAllFaceLightmapExtents();
	}

	MakeParents (0, -1);

	BuildClusterTable();

	// turn each face into a single patch
	MakePatches ();
	PairEdges ();

	// store the vertex normals calculated in PairEdges
	// so that the can be written to the bsp file for 
	// use in the engine
	SaveVertexNormals();

	// subdivide patches to a maximum dimension
	SubdividePatches ();

	// add displacement faces to cluster table
	AddDispsToClusterTable();

	// create directlights out of patches and lights
	CreateDirectLights ();

	// set up sky cameras
	ProcessSkyCameras();
}


// This function should fill in the indices into g_pFaces[] for the faces
// with displacements that touch the specified leaf.
void STUB_GetDisplacementsTouchingLeaf( int iLeaf, CUtlVector<int> &dispFaces )
{
}


void BuildFacesVisibleToLights( bool bAllVisible )
{
	g_FacesVisibleToLights.SetSize( numfaces/8 + 1 );

	if( bAllVisible )
	{
		memset( g_FacesVisibleToLights.Base(), 0xFF, g_FacesVisibleToLights.Count() );
		return;
	}

	// First merge all the light PVSes.
	CUtlVector<byte> aggregate;
	aggregate.SetSize( (dvis->numclusters/8) + 1 );
	memset( aggregate.Base(), 0, aggregate.Count() );

	int nDWords = aggregate.Count() / 4;
	int nBytes = aggregate.Count() - nDWords*4;

	for( directlight_t *dl = activelights; dl != NULL; dl = dl->next )
	{
		byte *pIn  = dl->pvs;
		byte *pOut = aggregate.Base();
		for( int iDWord=0; iDWord < nDWords; iDWord++ )
		{
			*((unsigned long*)pOut) |= *((unsigned long*)pIn);
			pIn  += 4;
			pOut += 4;
		}

		for( int iByte=0; iByte < nBytes; iByte++ )
		{
			*pOut |= *pIn;
			++pOut;
			++pIn;
		}
	}


	// Now tag any faces that are visible to this monster PVS.
	for( int iCluster=0; iCluster < dvis->numclusters; iCluster++ )
	{
		if( g_ClusterLeaves[iCluster].leafCount )
		{
			if( aggregate[iCluster>>3] & (1 << (iCluster & 7)) )
			{
				for ( int i = 0; i < g_ClusterLeaves[iCluster].leafCount; i++ )
				{
					int iLeaf = g_ClusterLeaves[iCluster].leafs[i];

					// Tag all the faces.
					int iFace;
					for( iFace=0; iFace < dleafs[iLeaf].numleaffaces; iFace++ )
					{
						int index = dleafs[iLeaf].firstleafface + iFace;
						index = dleaffaces[index];
						
						Assert( index < numfaces );
						g_FacesVisibleToLights[index >> 3] |= (1 << (index & 7));
					}

					// Fill in STUB_GetDisplacementsTouchingLeaf when it's available
					// so displacements get relit.
					CUtlVector<int> dispFaces;
					STUB_GetDisplacementsTouchingLeaf( iLeaf, dispFaces );
					for( iFace=0; iFace < dispFaces.Count(); iFace++ )
					{
						int index = dispFaces[iFace];
						g_FacesVisibleToLights[index >> 3] |= (1 << (index & 7));
					}
				}
			}
		}
	}

	// For stats.. figure out how many faces it's going to touch.
	int nFacesToProcess = 0;
	for( int i=0; i < numfaces; i++ )
	{
		if( g_FacesVisibleToLights[i>>3] & (1 << (i & 7)) )
			++nFacesToProcess;
	}
}



void MakeAllScales (void)
{
	// determine visibility between patches
	BuildVisMatrix ();
	
	// release visibility matrix
	FreeVisMatrix ();

	Msg("transfers %d, max %d\n", static_cast<int>(total_transfer), static_cast<int>(max_transfer) );

	qprintf ("transfer lists: %s\n"
		, V_pretifymem( (float)total_transfer * sizeof(transfer_t), 2, true ) );
}


// Helper function. This can be useful to visualize the world and faces and see which face
// corresponds to which dface.
#if 0
	#include "iscratchpad3d.h"
	void ScratchPad_DrawWorld()
	{
		IScratchPad3D *pPad = ScratchPad3D_Create();
		pPad->SetAutoFlush( false );

		for ( int i=0; i < numfaces; i++ )
		{
			dface_t *f = &g_pFaces[i];

			// Draw the face's outline, then put text for its face index on it too.
			CUtlVector<Vector> points;
			for ( int iEdge = 0; iEdge < f->numedges; iEdge++ )
			{
				int v;
				int se = dsurfedges[f->firstedge + iEdge];
				if ( se < 0 )
					v = dedges[-se].v[1];
				else
					v = dedges[se].v[0];
			
				dvertex_t *dv = &dvertexes[v];
				points.AddToTail( dv->point );
			}

			// Draw the outline.
			Vector vCenter( 0, 0, 0 );
			for ( iEdge=0; iEdge < points.Count(); iEdge++ )
			{
				pPad->DrawLine( CSPVert( points[iEdge] ), CSPVert( points[(iEdge+1)%points.Count()] ) );
				vCenter += points[iEdge];
			}
			vCenter /= points.Count();

			// Draw the text.
			char str[512];
			Q_snprintf( str, sizeof( str ), "%d", i );

			CTextParams params;

			params.m_bCentered = true;
			params.m_bOutline = true;
			params.m_flLetterWidth = 2;
			params.m_vColor.Init( 1, 0, 0 );
			
			VectorAngles( dplanes[f->planenum].normal, params.m_vAngles );
			params.m_bTwoSided = true;

			params.m_vPos = vCenter;
			
			pPad->DrawText( str, params );
		}

		pPad->Release();
	}
#endif


bool RadWorld_Go()
{
	g_iCurFace.store(0, std::memory_order::memory_order_relaxed);

	InitMacroTexture( source );

	if( g_pIncremental )
	{
		g_pIncremental->PrepareForLighting();

		// Cull out faces that aren't visible to any of the lights that we're updating with.
		BuildFacesVisibleToLights( false );
	}
	else
	{
		// Mark all faces visible.. when not doing incremental lighting, it's highly
		// likely that all faces are going to be touched by at least one light so don't
		// waste time here.
		BuildFacesVisibleToLights( true );
	}

	// build initial facelights
	if (g_bUseMPI) 
	{
		RunMPIBuildFacelights();
	}
	else 
	{
		RunThreadsOnIndividual (numfaces, true, BuildFacelights);
	}

	// Was the process interrupted?
	if( g_pIncremental && (g_iCurFace.load(std::memory_order::memory_order_relaxed) != numfaces) )
		return false;

	// Figure out the offset into lightmap data for each face.
	PrecompLightmapOffsets();
	
	// If we're doing incremental lighting, stop here.
	if( g_pIncremental )
	{
		g_pIncremental->Finalize();
	}
	else
	{
		// free up the direct lights now that we have facelights
		ExportDirectLightsToWorldLights();

		if ( g_bDumpPatches )
		{
			for( int iBump = 0; iBump < 4; ++iBump )
			{
				char szName[64];
				V_sprintf_safe ( szName, "bounce0_%d.txt", iBump );
				WriteWorld( szName, iBump );
			}
		}

		if (numbounce > 0)
		{
			// allocate memory for emitlight/addlight
			emitlight.SetSize( g_Patches.Count() );
			memset( emitlight.Base(), 0, g_Patches.Count() * sizeof( Vector ) );
			addlight.SetSize( g_Patches.Count() );
			memset( addlight.Base(), 0, g_Patches.Count() * sizeof( bumplights_t ) );

			MakeAllScales ();

			// spread light around
			BounceLight ();
		}

		//
		// displacement surface luxel accumulation (make threaded!!!)
		//
		StaticDispMgr()->StartTimer( "Build Patch/Sample Hash Table(s)..." );
		StaticDispMgr()->InsertSamplesDataIntoHashTable();
		StaticDispMgr()->InsertPatchSampleDataIntoHashTable();
		StaticDispMgr()->EndTimer();

		// blend bounced light into direct light and save
		VMPI_SetCurrentStage( "FinalLightFace" );
		if ( !g_bUseMPI || g_bMPIMaster )
			RunThreadsOnIndividual (numfaces, true, FinalLightFace);
		
		// Distribute the lighting data to workers.
		VMPI_DistributeLightData();
			
		Msg("FinalLightFace Done\n"); fflush(stdout);
	}

	return true;
}

// declare the sample file pointer -- the whole debug print system should
// be reworked at some point!!
FileHandle_t pFileSamples[4][4];

void LoadPhysicsDLL( void )
{
	PhysicsDLLPath( "VPHYSICS.DLL" );
}


void InitDumpPatchesFiles()
{
	for( int iStyle = 0; iStyle < 4; ++iStyle )
	{
		for ( int iBump = 0; iBump < 4; ++iBump )
		{
			char szFilename[MAX_PATH];
			V_sprintf_safe( szFilename, "samples_style%d_bump%d.txt", iStyle, iBump );
			pFileSamples[iStyle][iBump] = g_pFileSystem->Open( szFilename, "w" );
			if( !pFileSamples[iStyle][iBump] )
			{
				Error( "Can't open %s for -dump.\n", szFilename );
			}
		}
	}
}

extern IFileSystem *g_pOriginalPassThruFileSystem;

void VRAD_LoadBSP( char const *pFilename )
{
	ThreadSetDefault ();

	g_flStartTime = Plat_FloatTime();

	if( g_bLowPriority )
	{
		SetLowPriority();
	}

	V_strcpy_safe( level_name, source );

	// This must come after InitFileSystem because the file system pointer might change.
	if ( g_bDumpPatches )
		InitDumpPatchesFiles();

	// This part is just for VMPI. VMPI's file system needs the basedir in front of all filenames,
	// so we prepend qdir here.
	V_strcpy_safe( source, ExpandPath( source ) );

	if ( !g_bUseMPI )
	{
		// Setup the logfile.
		char logFile[MAX_FILEPATH];
		V_sprintf_safe( logFile, "%s.log", source );
		SetSpewFunctionLogFile( logFile );
	}

	LoadPhysicsDLL();

	// Set the required global lights filename and try looking in qproject
	V_strcpy_safe( global_lights, "lights.rad" );
	if ( !g_pFileSystem->FileExists( global_lights ) )
	{
		// Otherwise, try looking in the BIN directory from which we were run from
		Msg( "Could not find lights.rad in %s.\nTrying VRAD BIN directory instead...\n", 
			    global_lights );
		GetModuleFileName( NULL, global_lights, sizeof( global_lights ) );
		V_ExtractFilePath( global_lights, global_lights );
		V_strcat_safe( global_lights, "lights.rad" );
	}

	// Set the optional level specific lights filename
	V_strcpy_safe( level_lights, source );

	Q_DefaultExtension( level_lights, ".rad");
	if ( !g_pFileSystem->FileExists( level_lights ) ) 
		*level_lights = 0;	

	ReadLightFile(global_lights);							// Required
	if ( !Q_isempty(designer_lights) ) ReadLightFile(designer_lights);	// Command-line
	if ( !Q_isempty(level_lights) )	ReadLightFile(level_lights);	// Optional & implied

	V_strcpy_safe(incrementfile, source);
	Q_DefaultExtension(incrementfile, ".r0");
	Q_DefaultExtension(source, ".bsp");

	Msg( "Loading %s.\n", source );
	VMPI_SetCurrentStage( "LoadBSPFile" );
	LoadBSPFile (source);

	// Add this bsp to our search path so embedded resources can be found
	if ( g_bUseMPI && g_bMPIMaster )
	{
		// MPI Master, MPI workers don't need to do anything
		g_pOriginalPassThruFileSystem->AddSearchPath(source, "GAME", PATH_ADD_TO_HEAD);
		g_pOriginalPassThruFileSystem->AddSearchPath(source, "MOD", PATH_ADD_TO_HEAD);
	}
	else if ( !g_bUseMPI )
	{
		// Non-MPI
		g_pFullFileSystem->AddSearchPath(source, "GAME", PATH_ADD_TO_HEAD);
		g_pFullFileSystem->AddSearchPath(source, "MOD", PATH_ADD_TO_HEAD);
	}

	// now, set whether or not static prop lighting is present
	if (g_bStaticPropLighting)
		g_LevelFlags |= g_bHDR? LVLFLAGS_BAKED_STATIC_PROP_LIGHTING_HDR : LVLFLAGS_BAKED_STATIC_PROP_LIGHTING_NONHDR;
	else
	{
		g_LevelFlags &= ~( LVLFLAGS_BAKED_STATIC_PROP_LIGHTING_HDR | LVLFLAGS_BAKED_STATIC_PROP_LIGHTING_NONHDR );
	}

	// now, we need to set our face ptr depending upon hdr, and if hdr, init it
	if (g_bHDR)
	{
		g_pFaces = dfaces_hdr;
		if (numfaces_hdr==0)
		{
			numfaces_hdr = numfaces;
			memcpy( dfaces_hdr, dfaces, numfaces*sizeof(dfaces[0]) );
		}
	}
	else
	{
		g_pFaces = dfaces;
	}


	ParseEntities ();
	ExtractBrushEntityShadowCasters();

	StaticPropMgr()->Init();
	StaticDispMgr()->Init();

	if (!visdatasize)
	{
		Msg("No vis information, direct lighting only.\n");
		numbounce = 0;
		ambient[0] = ambient[1] = ambient[2] = 0.1f;
		dvis->numclusters = CountClusters();
	}

	//
	// patches and referencing data (ensure capacity)
	//
	// TODO: change the maxes to the amount from the bsp!!
	//
//	g_Patches.EnsureCapacity( MAX_PATCHES );

	g_FacePatches.SetSize( MAX_MAP_FACES );
	faceParents.SetSize( MAX_MAP_FACES );
	clusterChildren.SetSize( MAX_MAP_CLUSTERS );

	int ndx;
	for ( ndx = 0; ndx < MAX_MAP_FACES; ndx++ )
	{
		g_FacePatches[ndx] = g_FacePatches.InvalidIndex();
		faceParents[ndx] = faceParents.InvalidIndex();
	}

	for ( ndx = 0; ndx < MAX_MAP_CLUSTERS; ndx++ )
	{
		clusterChildren[ndx] = clusterChildren.InvalidIndex();
	}

	// Setup ray tracer
	AddBrushesForRayTrace();
	StaticDispMgr()->AddPolysForRayTrace();
	StaticPropMgr()->AddPolysForRayTrace();

	// Dump raytracer for glview
	if ( g_bDumpRtEnv )
		WriteRTEnv("trace.txt");

	// Build acceleration structure
	Msg ( "Setting up ray-trace acceleration structure...");
	double start = Plat_FloatTime();
	g_RtEnv.SetupAccelerationStructure();
	Msg ( "Done (%.2fs)", Plat_FloatTime()-start );
	Msg ( "\n" );

#if 0  // To test only k-d build
	exit(0);
#endif

	RadWorld_Start();

	// Setup incremental lighting.
	if( g_pIncremental )
	{
		if( !g_pIncremental->Init( source, incrementfile ) )
		{
			Error( "Unable to load incremental lighting file in %s.\n", incrementfile );
			return;
		}
	}
}


void VRAD_ComputeOtherLighting()
{
	// Compute lighting for the bsp file
	if ( !g_bNoDetailLighting )
	{
		ComputeDetailPropLighting( THREADINDEX_MAIN );
	}

	ComputePerLeafAmbientLighting();

	// bake the static props high quality vertex lighting into the bsp
	if ( !do_fast && g_bStaticPropLighting )
	{
		StaticPropMgr()->ComputeLighting( THREADINDEX_MAIN );
	}
}

extern void CloseDispLuxels();

void VRAD_Finish()
{
	Msg( "Ready to Finish\n" ); 
	fflush( stdout );

	if ( verbose )
	{
		PrintBSPFileSizes();
	}

	Msg( "Writing %s\n", source );
	VMPI_SetCurrentStage( "WriteBSPFile" );
	WriteBSPFile(source);

	if ( g_bDumpPatches )
	{
		for ( int iStyle = 0; iStyle < 4; ++iStyle )
		{
			for ( int iBump = 0; iBump < 4; ++iBump )
			{
				g_pFileSystem->Close( pFileSamples[iStyle][iBump] );
			}
		}
	}

	CloseDispLuxels();

	StaticPropMgr()->Shutdown();

	double end = Plat_FloatTime();
	
	char str[512];
	GetHourMinuteSecondsString( (int)( end - g_flStartTime ), str, sizeof( str ) );
	Msg( "%s elapsed\n", str );

	ReleasePakFileLumps();
}


// Run startup code like initialize mathlib (called from main() and from the 
// WorldCraft interface into vrad).
void VRAD_Init()
{
	MathLib_Init( 2.2f, 2.2f, 0.0f, 2.0f, false, false, false, false );
	InstallSpewFunction();
	SpewActivate( "developer", 1 );
}


static int ParseCommandLine( int argc, char **argv, bool *onlydetail )
{
	*onlydetail = false;

	int mapArg = -1;

	// default to LDR
	SetHDRMode( false );

	for( int i=1 ; i<argc ; i++ )
	{
		if ( !Q_stricmp( argv[i], "-StaticPropLighting" ) )
		{
			Msg( "--static-prop-lighting: true\n");
			g_bStaticPropLighting = true;
		}
		else if ( !stricmp( argv[i], "-StaticPropNormals" ) )
		{
			Msg( "--static-prop-normals: true\n");
			g_bShowStaticPropNormals = true;
		}
		else if ( !stricmp( argv[i], "-OnlyStaticProps" ) )
		{
			Msg( "--only-static-props: true\n");
			g_bOnlyStaticProps = true;
		}
		else if ( !Q_stricmp( argv[i], "-StaticPropPolys" ) )
		{
			Msg( "--static-prop-polys: true\n");
			g_bStaticPropPolys = true;
		}
		else if ( !Q_stricmp( argv[i], "-nossprops" ) )
		{
			Msg( "--no-self-shadow-props: true\n");
			g_bDisablePropSelfShadowing = true;
		}
		else if ( !Q_stricmp( argv[i], "-textureshadows" ) )
		{
			Msg( "--texture-shadows: true\n");
			g_bTextureShadows = true;
		}
		else if ( !strcmp(argv[i], "-dump") )
		{
			Msg( "--dump-patches: true\n");
			g_bDumpPatches = true;
		}
		else if ( !Q_stricmp( argv[i], "-nodetaillight" ) )
		{
			Msg( "--no-detail-light: true\n");
			g_bNoDetailLighting = true;
		}
		else if ( !Q_stricmp( argv[i], "-rederrors" ) )
		{
			Msg( "--red-errors: true\n");
			bRed2Black = false;
		}
		else if ( !Q_stricmp( argv[i], "-dumpnormals" ) )
		{
			Msg( "--dump-normals: true\n");
			bDumpNormals = true;
		}
		else if ( !Q_stricmp( argv[i], "-dumptrace" ) )
		{
			Msg( "--dump-trace: true\n");
			g_bDumpRtEnv = true;
		}
		else if ( !Q_stricmp( argv[i], "-LargeDispSampleRadius" ) )
		{
			Msg( "--large-disp-sample-radius: true\n");
			g_bLargeDispSampleRadius = true;
		}
		else if (!Q_stricmp( argv[i], "-dumppropmaps"))
		{
			Msg( "--dump-prop-maps: true\n");
			g_bDumpPropLightmaps = true;
		}
		else if (!Q_stricmp(argv[i],"-bounce"))
		{
			if ( ++i < argc )
			{
				int bounceParam = atoi (argv[i]);
				if ( bounceParam < 0 )
				{
					Error("Expected non-negative value after '-bounce'.\n" );
					return -1;
				}
				numbounce = (unsigned)bounceParam;
				Msg( "--bounce: %u\n", numbounce );
			}
			else
			{
				Error("Expected a value after '-bounce'.\n" );
				return -1;
			}
		}
		else if (!Q_stricmp(argv[i],"-verbose") || !Q_stricmp(argv[i],"-v"))
		{
			Msg( "--verbose: true\n");
			verbose = true;
		}
		else if (!Q_stricmp(argv[i],"-threads"))
		{
			if ( ++i < argc )
			{
				numthreads = atoi (argv[i]);
				if ( numthreads <= 0 )
				{
					Error("Expected positive value after '-threads'.\n" );
					return -1;
				}

				Msg( "--threads: %d\n", numthreads );
			}
			else
			{
				Error("Expected a value after '-threads'.\n" );
				return -1;
			}
		}
		else if ( !Q_stricmp(argv[i], "-lights" ) )
		{
			if ( ++i < argc && *argv[i] )
			{
				V_strcpy_safe( designer_lights, argv[i] );
				Msg( "--lights: %s\n", designer_lights );
			}
			else
			{
				Error("Expected a filepath after '-lights'.\n" );
				return -1;
			}
		}
		else if (!Q_stricmp(argv[i],"-noextra"))
		{
			Msg( "--no-extra: true\n" );
			do_extra = false;
		}
		else if (!Q_stricmp(argv[i],"-debugextra"))
		{
			Msg( "--debug-extra: true\n" );
			debug_extra = true;
		}
		else if ( !Q_stricmp(argv[i], "-fastambient") )
		{
			Msg( "--fast-ambient: true\n" );
			g_bFastAmbient = true;
		}
		else if (!Q_stricmp(argv[i],"-fast"))
		{
			Msg( "--fast: true\n" );
			do_fast = true;
		}
		else if (!Q_stricmp(argv[i],"-noskyboxrecurse"))
		{
			Msg( "--no-skybox-recurse: true\n" );
			g_bNoSkyRecurse = true;
		}
		else if (!Q_stricmp(argv[i],"-final"))
		{
			Msg( "--final: true\n" );
			g_flSkySampleScale = 16.0;
		}
		else if (!Q_stricmp(argv[i],"-extrasky"))
		{
			if ( ++i < argc && *argv[i] )
			{
				// dimhotepus: atof -> strtof.
				g_flSkySampleScale = strtof( argv[i], nullptr );
				Msg( "--extra-sky-scale: %f\n", g_flSkySampleScale );
			}
			else
			{
				Error("Expected a scale factor after '-extrasky'.\n" );
				return -1;
			}
		}
		else if (!Q_stricmp(argv[i],"-centersamples"))
		{
			Msg( "--center-samples: true\n" );
			do_centersamples = true;
		}
		else if (!Q_stricmp(argv[i],"-smooth"))
		{
			if ( ++i < argc )
			{
				float degrees = strtof(argv[i], nullptr);
				if (degrees < 0 || degrees > 360)
				{
					Error("Expected an degrees angle [0...360] after '-smooth'.\n" );
					return -1;
				}
				
				Msg( "--smoothing-angle-degrees: %f\n", degrees );
				smoothing_threshold = cos(DEG2RAD(degrees));
			}
			else
			{
				Error("Expected an degrees angle after '-smooth'.\n" );
				return -1;
			}
		}
		else if (!Q_stricmp(argv[i],"-dlightmap"))
		{
			Msg( "--dlightmap: true\n" );
			dlight_map = 1;
		}
		else if (!Q_stricmp(argv[i],"-luxeldensity"))
		{
			if ( ++i < argc )
			{
				luxeldensity = strtof(argv[i], nullptr);
				if (luxeldensity > 1.0f)
					luxeldensity = 1.0f / luxeldensity;

				Msg( "--luxel-density: %f\n", luxeldensity );
			}
			else
			{
				Error("Expected a value after '-luxeldensity'.\n" );
				return -1;
			}
		}
		else if( !Q_stricmp( argv[i], "-low" ) )
		{
			Msg( "--low: Run worker threads with low priority\n" );
			g_bLowPriority = true;
		}
		else if( !Q_stricmp( argv[i], "-loghash" ) )
		{
			Msg( "--log-hash: true\n" );
			g_bLogHashData = true;
		}
		else if( !Q_stricmp( argv[i], "-onlydetail" ) )
		{
			Msg( "--only-detail: true\n" );
			*onlydetail = true;
		}
		else if (!Q_stricmp(argv[i],"-softsun"))
		{
			if ( ++i < argc )
			{
				g_SunAngularExtent = strtof(argv[i], nullptr);
				g_SunAngularExtent = sin(DEG2RAD(g_SunAngularExtent));

				Msg("--soft-sun-extent: %f\n", g_SunAngularExtent);
			}
			else
			{
				Error("Expected an angular extent value (0..180) after '-softsun'.\n" );
				return -1;
			}
		}
		else if ( !Q_stricmp( argv[i], "-maxdispsamplesize" ) )
		{
			if ( ++i < argc )
			{
				g_flMaxDispSampleSize = strtof( argv[i], nullptr );
				Msg( "--max-disp-sample-size: %f\n", g_flMaxDispSampleSize );
			}
			else
			{
				Error( "Expected a sample size after '-maxdispsamplesize'.\n" );
				return -1;
			}
		}
		else if ( stricmp( argv[i], "-StopOnExit" ) == 0 )
		{
			Msg( "--stop-on-exit: true\n" );
			g_bStopOnExit = true;
		}
		else if ( stricmp( argv[i], "-steam" ) == 0|| stricmp( argv[i], "-allowdebug" ) == 0 )
		{
			Msg( "--allow-debug or --steam: true\n" );
			// Don't need to do anything, just don't error out.
		}
		else if ( !Q_stricmp( argv[i], CMDLINEOPTION_NOVCONFIG ) )
		{
			Msg( "--no-vconfig: true\n" );
		}
		else if ( !Q_stricmp( argv[i], "-vproject" ) || !Q_stricmp( argv[i], "-game" ) || !Q_stricmp( argv[i], "-insert_search_path" ) )
		{
			++i;
		}
		else if ( !Q_stricmp( argv[i], "-FullMinidumps" ) )
		{
			Msg( "--full-minidumps: true\n" );
			se::utils::common::EnableFullMinidumps( true );
		}
		else if ( !Q_stricmp( argv[i], "-hdr" ) )
		{
			Msg( "--hdr: true\n" );
			SetHDRMode( true );
		}
		else if ( !Q_stricmp( argv[i], "-ldr" ) )
		{
			Msg( "--ldr: true\n" );
			SetHDRMode( false );
		}
		else if (!Q_stricmp(argv[i],"-maxchop"))
		{
			if ( ++i < argc )
			{
				maxchop = strtof( argv[i], nullptr );
				if ( maxchop < 1.0f )
				{
					Error( "Expected '-maxchop' value >= 1, got %f.\n", maxchop );
					return -1;
				}
				Msg( "--max-chop: %f\n", maxchop );
			}
			else
			{
				Error("Expected a value after '-maxchop'.\n" );
				return -1;
			}
		}
		else if (!Q_stricmp(argv[i],"-chop"))
		{
			if ( ++i < argc )
			{
				minchop = strtof( argv[i], nullptr );
				if ( minchop < 1.0f )
				{
					Error( "Expected '-chop' value >= 1, got %f.\n", dispchop );
					return -1;
				}
				Msg( "--min-chop: %f\n", minchop );
				minchop = min( minchop, maxchop );
			}
			else
			{
				Error("Expected a value after '-chop'.\n" );
				return -1;
			}
		}
		else if ( !Q_stricmp( argv[i], "-dispchop" ) )
		{
			if ( ++i < argc )
			{
				dispchop = strtof( argv[i], nullptr );
				if ( dispchop < 1.0f )
				{
					Error( "Expected '-dipschop' value >= 1, got %f.\n", dispchop );
					return -1;
				}
				Msg( "--disp-chop: %f\n", dispchop );
			}
			else
			{
				Error( "Expected a value after '-dispchop'.\n" );
				return -1;
			}
		}
		else if ( !Q_stricmp( argv[i], "-disppatchradius" ) )
		{
			if ( ++i < argc )
			{
				g_MaxDispPatchRadius = strtof( argv[i], nullptr );
				if ( g_MaxDispPatchRadius < 10.0f )
				{
					Error( "Expected '-disppatchradius' value >= 10, got %f.\n", g_MaxDispPatchRadius );
					return -1;
				}
				Msg( "--disp-patch-radius: %f\n", g_MaxDispPatchRadius );
			}
			else
			{
				Error( "Expected a value after '-disppatchradius'.\n" );
				return -1;
			}
		}

#if ALLOWDEBUGOPTIONS
		else if (!Q_stricmp(argv[i],"-scale"))
		{
			if ( ++i < argc )
			{
				lightscale = strtof(argv[i], nullptr);
				Msg( "--light-scale: %f\n", lightscale );
			}
			else
			{
				Error("Expected a value after '-scale'.\n" );
				return -1;
			}
		}
		else if (!Q_stricmp(argv[i],"-ambient"))
		{
			if ( i+3 < argc )
			{
 				ambient[0] = strtof(argv[++i], nullptr) * 128;
 				ambient[1] = strtof(argv[++i], nullptr) * 128;
 				ambient[2] = strtof(argv[++i], nullptr) * 128;
				Msg( "--ambient: %f %f %f\n", ambient[0] / 128, ambient[1] / 128, ambient[2] / 128 );
			}
			else
			{
				Error("Expected three color values after '-ambient'.\n" );
				return -1;
			}
		}
		else if (!Q_stricmp(argv[i],"-dlight"))
		{
			if ( ++i < argc )
			{
				dlight_threshold = strtof(argv[i], nullptr);
				Msg( "--dlight: %f\n", dlight_threshold );
			}
			else
			{
				Error("Expected a value after '-dlight'.\n" );
				return -1;
			}
		}
		else if (!Q_stricmp(argv[i],"-sky"))
		{
			if ( ++i < argc )
			{
				indirect_sun = strtof(argv[i], nullptr);
				Msg( "--sky: %f\n", indirect_sun );
			}
			else
			{
				Error( "Expected a value after '-sky'.\n" );
				return -1;
			}
		}
		else if (!Q_stricmp(argv[i],"-notexscale"))
		{
			Msg( "--no-texture-scale: true\n" );
			texscale = false;
		}
		else if (!Q_stricmp(argv[i],"-coring"))
		{
			if ( ++i < argc )
			{
				coring = strtof( argv[i], nullptr );
				Msg( "--coring: %f\n", coring );
			}
			else
			{
				Error("Expected a light threshold after '-coring'.\n" );
				return -1;
			}
		}
#endif
		// NOTE: the -mpi checks must come last here because they allow the previous argument 
		// to be -mpi as well. If it game before something else like -game, then if the previous
		// argument was -mpi and the current argument was something valid like -game, it would skip it.
		else if ( !Q_strncasecmp( argv[i], "-mpi", 4 ) || !Q_strncasecmp( argv[i-1], "-mpi", 4 ) )
		{
			if ( stricmp( argv[i], "-mpi" ) == 0 )
				g_bUseMPI = true;

			Msg("--mpi: %s MPI", g_bUseMPI ? "Enable" : "Disable");
		
			// Any other args that start with -mpi are ok too.
			if ( i == argc - 1 && V_stricmp( argv[i], "-mpi_ListParams" ) != 0 )
				break;
		}
		else if ( mapArg == -1 )
		{
			mapArg = i;
		}
		else
		{
			return -1;
		}
	}

	return mapArg;
}


void PrintCommandLine( int argc, char **argv )
{
	Warning( "Command line: " );
	for ( int z=0; z < argc; z++ )
	{
		Warning( "\"%s\" ", argv[z] );
	}
	Warning( "\n\n" );
}


void PrintUsage( int argc, char **argv )
{
	PrintCommandLine( argc, argv );

	Warning(	
		"usage  : vrad [options...] bspfile\n"
		"example: vrad c:\\hl2\\hl2\\maps\\test\n"
		"\n"
		"Common options:\n"
		"\n"
		"  -v (or -verbose): Turn on verbose output (also shows more command\n"
		"  -bounce #       : Set max number of bounces (default: 100).\n"
		"  -fast           : Quick and dirty lighting.\n"
		"  -fastambient    : Per-leaf ambient sampling is lower quality to save compute time.\n"
		"  -final          : High quality processing. equivalent to -extrasky 16.\n"
		"  -extrasky n     : trace N times as many rays for indirect light and sky ambient.\n"
		"  -low            : Run as an idle-priority process.\n"
		"  -mpi            : Use VMPI to distribute computations.\n"
		"  -rederror       : Show errors in red.\n"
		"\n"
		"  -vproject <directory> : Override the VPROJECT environment variable.\n"
		"  -game <directory>     : Same as -vproject.\n"
		"\n"
		"Other options:\n"
		"  -novconfig      : Don't bring up graphical UI on vproject errors.\n"
		"  -dump           : Write debugging .txt files.\n"
		"  -dumpnormals    : Write normals to debug files.\n"
		"  -dumptrace      : Write ray-tracing environment to debug files.\n"
		"  -threads        : Control the number of threads vbsp uses (defaults to the #\n"
		"                    or processors on your machine).\n"
		"  -lights <file>  : Load a lights file in addition to lights.rad and the\n"
		"                    level lights file.\n"
		"  -noextra        : Disable supersampling.\n"
		"  -debugextra     : Places debugging data in lightmaps to visualize\n"
		"                    supersampling.\n"
		"  -smooth #       : Set the threshold for smoothing groups, in degrees\n"
		"                    (default 45).\n"
		"  -dlightmap      : Force direct lighting into different lightmap than\n"
		"                    radiosity.\n"
		"  -stoponexit	   : Wait for a keypress on exit.\n"
		"  -mpi_pw <pw>    : Use a password to choose a specific set of VMPI workers.\n"
		"  -nodetaillight  : Don't light detail props.\n"
		"  -centersamples  : Move sample centers.\n"
		"  -luxeldensity # : Rescale all luxels by the specified amount (default: 1.0).\n"
		"                    The number specified must be less than 1.0 or it will be\n"
		"                    ignored.\n"
		"  -loghash        : Log the sample hash table to samplehash.txt.\n"
		"  -onlydetail     : Only light detail props and per-leaf lighting.\n"
		"  -maxdispsamplesize #: Set max displacement sample size (default: 512).\n"
		"  -softsun <n>    : Treat the sun as an area light source of size <n> degrees."
		"                    Produces soft shadows.\n"
		"                    Recommended values are between 0 and 5. Default is 0.\n"
		"  -FullMinidumps  : Write large minidumps on crash.\n"
		"  -chop           : Smallest number of luxel widths for a bounce patch, used on edges\n"
		"  -maxchop		   : Coarsest allowed number of luxel widths for a patch, used in face interiors\n"
		"\n"
		"  -LargeDispSampleRadius: This can be used if there are splotches of bounced light\n"
		"                          on terrain. The compile will take longer, but it will gather\n"
		"                          light across a wider area.\n"
        "  -StaticPropLighting   : generate backed static prop vertex lighting\n"
        "  -StaticPropPolys   : Perform shadow tests of static props at polygon precision\n"
        "  -OnlyStaticProps   : Only perform direct static prop lighting (vrad debug option)\n"
		"  -StaticPropNormals : when lighting static props, just show their normal vector\n"
		"  -textureshadows : Allows texture alpha channels to block light - rays intersecting alpha surfaces will sample the texture\n"
		"  -noskyboxrecurse : Turn off recursion into 3d skybox (skybox shadows on world)\n"
		"  -nossprops      : Globally disable self-shadowing on static props\n"
		"\n"
#if 1 // Disabled for the initial SDK release with VMPI so we can get feedback from selected users.
		);
#else
		"  -mpi_ListParams : Show a list of VMPI parameters.\n"
		"\n"
		);

	// Show VMPI parameters?
	for ( int i=1; i < argc; i++ )
	{
		if ( V_stricmp( argv[i], "-mpi_ListParams" ) == 0 )
		{
			Warning( "VMPI-specific options:\n\n" );

			bool bIsSDKMode = VMPI_IsSDKMode();
			for ( int i=k_eVMPICmdLineParam_FirstParam+1; i < k_eVMPICmdLineParam_LastParam; i++ )
			{
				if ( (VMPI_GetParamFlags( (EVMPICmdLineParam)i ) & VMPI_PARAM_SDK_HIDDEN) && bIsSDKMode )
					continue;
					
				Warning( "[%s]\n", VMPI_GetParamString( (EVMPICmdLineParam)i ) );
				Warning( VMPI_GetParamHelpString( (EVMPICmdLineParam)i ) );
				Warning( "\n\n" );
			}
			break;
		}
	}
#endif
}

int RunVRAD( int argc, char **argv )
{
#ifdef PLATFORM_64BITS
	Msg("Valve Software - vrad SSE4.2+ [64 bit] (" __DATE__ ")\n" );
#else
	Msg("Valve Software - vrad SSE4.2+ (" __DATE__ ")\n");
#endif

	verbose = false;  // Originally FALSE

	// dimhotepus: Need file system first.
	const ScopedFileSystem scopedFileSystem( argv[ argc - 1 ] );

	bool onlydetail;

	const ScopedCmdLine scopedCmdLine( argc, argv, source, "vrad" );
	int i = ParseCommandLine( argc, argv, &onlydetail );
	if (i == -1)
	{
		PrintUsage( argc, argv );
		CmdLib_Exit( 1 );
	}

	// Initialize the filesystem, so additional commandline options can be loaded
	Q_StripExtension( argv[ i ], source );
	Q_FileBase( source, source );

	VRAD_LoadBSP( argv[i] );

	if ( (! onlydetail) && (! g_bOnlyStaticProps ) )
	{
		RadWorld_Go();
	}

	VRAD_ComputeOtherLighting();

	VRAD_Finish();

	VMPI_SetCurrentStage( "master done" );

	CmdLib_Cleanup();
	SpewDeactivate();
	return 0;
}


int VRAD_Main(int argc, char **argv)
{
	g_pFileSystem = NULL;	// Safeguard against using it before it's properly initialized.

	VRAD_Init();

	// This must come first.
	VRAD_SetupMPI( argc, argv );

	if ( g_bUseMPI && !g_bMPIMaster )
	{
		const se::utils::common::ScopedMinidumpHandler minidump{VMPI_ExceptionFilter};
		return RunVRAD( argc, argv );
	}
	
	const se::utils::common::ScopedDefaultMinidumpHandler minidump;
	return RunVRAD( argc, argv );
}
