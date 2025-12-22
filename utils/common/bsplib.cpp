//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $Revision: $
// $NoKeywords: $
//=============================================================================//

#include "bsplib.h"

#include <cmath>
#include <cstring>
#include <cstdlib>

#include "bspfile.h"
#include "bspflags.h"
#include "datamap.h"
#include "cmdlib.h"
#include "filesystem.h"
#include "filesystem_tools.h"
#include "mathlib/compressed_light_cube.h"
#include "mathlib/mathlib.h"
#include "mathlib/vector.h"
#include "mathlib/vector2d.h"
#include "zip_utils.h"
#include "scriplib.h"
#include "bsptreedata.h"
#include "cmodel.h"
#include "gamebspfile.h"
#include "materialsystem/hardwareverts.h"
#include "tier1/byteswap.h"
#include "tier1/utlbuffer.h"
#include "tier1/utllinkedlist.h"
#include "tier1/strtools.h"
#include "tier1/utlmemory.h"
#include "tier1/lzmaDecoder.h"
#include "tier1/utlvector.h"
#include "tier1/utlstring.h"
#include "tier1/checksum_crc.h"
#include "tier1/interface.h"
#include "tier0/basetypes.h"
#include "tier0/commonmacros.h"
#include "tier0/platform.h"
#include "tier0/wchartypes.h"
#include "tier0/dbg.h"
#include "vphysics_interface.h"
#include "vcollide.h"
#include "lumpfiles.h"
#include "lzma/lzma.h"
#include "posix_file_stream.h"

#include <memory>

#include "tier0/memdbgon.h"

//=============================================================================

// Boundary each lump should be aligned to
constexpr int LUMP_ALIGNMENT{4};

// Data descriptions for byte swapping - only needed
// for structures that are written to file for use by the game.
BEGIN_BYTESWAP_DATADESC( dheader_t )
	DEFINE_FIELD( ident, FIELD_INTEGER ),
	DEFINE_FIELD( version, FIELD_INTEGER ),
	DEFINE_EMBEDDED_ARRAY( lumps, HEADER_LUMPS ),
	DEFINE_FIELD( mapRevision, FIELD_INTEGER ),
END_BYTESWAP_DATADESC()

BEGIN_BYTESWAP_DATADESC( lump_t )
	DEFINE_FIELD( fileofs, FIELD_INTEGER ),
	DEFINE_FIELD( filelen, FIELD_INTEGER ),
	DEFINE_FIELD( version, FIELD_INTEGER ),
	DEFINE_FIELD( uncompressedSize, FIELD_INTEGER ),
END_BYTESWAP_DATADESC()

BEGIN_BYTESWAP_DATADESC( dflagslump_t )
	DEFINE_FIELD( m_LevelFlags, FIELD_INTEGER ),
END_BYTESWAP_DATADESC()

BEGIN_BYTESWAP_DATADESC( dplane_t )
	DEFINE_FIELD( normal, FIELD_VECTOR ),
	DEFINE_FIELD( dist, FIELD_FLOAT ),
	DEFINE_FIELD( type, FIELD_INTEGER ),
END_BYTESWAP_DATADESC()

BEGIN_BYTESWAP_DATADESC( dleaf_version_0_t )
	DEFINE_FIELD( contents, FIELD_INTEGER ),
	DEFINE_FIELD( cluster, FIELD_SHORT ),
	DEFINE_BITFIELD( bf, FIELD_SHORT, 16 ),
	DEFINE_ARRAY( mins, FIELD_SHORT, 3 ),
	DEFINE_ARRAY( maxs, FIELD_SHORT, 3 ),
	DEFINE_FIELD( firstleafface, FIELD_SHORT ),
	DEFINE_FIELD( numleaffaces, FIELD_SHORT ),
	DEFINE_FIELD( firstleafbrush, FIELD_SHORT ),
	DEFINE_FIELD( numleafbrushes, FIELD_SHORT ),
	DEFINE_FIELD( leafWaterDataID, FIELD_SHORT ),
	DEFINE_EMBEDDED( m_AmbientLighting ),
END_BYTESWAP_DATADESC()

BEGIN_BYTESWAP_DATADESC( dleaf_t )
	DEFINE_FIELD( contents, FIELD_INTEGER ),
	DEFINE_FIELD( cluster, FIELD_SHORT ),
	DEFINE_BITFIELD( bf, FIELD_SHORT, 16 ),
	DEFINE_ARRAY( mins, FIELD_SHORT, 3 ),
	DEFINE_ARRAY( maxs, FIELD_SHORT, 3 ),
	DEFINE_FIELD( firstleafface, FIELD_SHORT ),
	DEFINE_FIELD( numleaffaces, FIELD_SHORT ),
	DEFINE_FIELD( firstleafbrush, FIELD_SHORT ),
	DEFINE_FIELD( numleafbrushes, FIELD_SHORT ),
	DEFINE_FIELD( leafWaterDataID, FIELD_SHORT ),
END_BYTESWAP_DATADESC()

BEGIN_BYTESWAP_DATADESC( CompressedLightCube )	// array of 6 ColorRGBExp32 (3 bytes and 1 char)
	DEFINE_ARRAY( m_Color, FIELD_CHARACTER, 6 * sizeof(ColorRGBExp32) ),
END_BYTESWAP_DATADESC()

BEGIN_BYTESWAP_DATADESC( dleafambientindex_t )
	DEFINE_FIELD( ambientSampleCount, FIELD_SHORT ),
	DEFINE_FIELD( firstAmbientSample, FIELD_SHORT ),
END_BYTESWAP_DATADESC()

BEGIN_BYTESWAP_DATADESC( dleafambientlighting_t )	// array of 6 ColorRGBExp32 (3 bytes and 1 char)
	DEFINE_EMBEDDED( cube ),
	DEFINE_FIELD( x, FIELD_CHARACTER ),
	DEFINE_FIELD( y, FIELD_CHARACTER ),
	DEFINE_FIELD( z, FIELD_CHARACTER ),
	DEFINE_FIELD( pad, FIELD_CHARACTER ),
END_BYTESWAP_DATADESC()

BEGIN_BYTESWAP_DATADESC( dvertex_t )
	DEFINE_FIELD( point, FIELD_VECTOR ),
END_BYTESWAP_DATADESC()

BEGIN_BYTESWAP_DATADESC( dnode_t )
	DEFINE_FIELD( planenum, FIELD_INTEGER ),
	DEFINE_ARRAY( children, FIELD_INTEGER, 2 ),
	DEFINE_ARRAY( mins, FIELD_SHORT, 3 ),
	DEFINE_ARRAY( maxs, FIELD_SHORT, 3 ),
	DEFINE_FIELD( firstface, FIELD_SHORT ),
	DEFINE_FIELD( numfaces, FIELD_SHORT ),
	DEFINE_FIELD( area, FIELD_SHORT ),
END_BYTESWAP_DATADESC()

BEGIN_BYTESWAP_DATADESC( texinfo_t )
	DEFINE_ARRAY( textureVecsTexelsPerWorldUnits, FIELD_FLOAT, 2 * 4 ),
	DEFINE_ARRAY( lightmapVecsLuxelsPerWorldUnits, FIELD_FLOAT, 2 * 4 ),
	DEFINE_FIELD( flags, FIELD_INTEGER ),
	DEFINE_FIELD( texdata, FIELD_INTEGER ),
END_BYTESWAP_DATADESC()

BEGIN_BYTESWAP_DATADESC( dtexdata_t )
	DEFINE_FIELD( reflectivity, FIELD_VECTOR ),
	DEFINE_FIELD( nameStringTableID, FIELD_INTEGER ),
	DEFINE_FIELD( width, FIELD_INTEGER ),
	DEFINE_FIELD( height, FIELD_INTEGER ),
	DEFINE_FIELD( view_width, FIELD_INTEGER ),
	DEFINE_FIELD( view_height, FIELD_INTEGER ),
END_BYTESWAP_DATADESC()

BEGIN_BYTESWAP_DATADESC( ddispinfo_t )
	DEFINE_FIELD( startPosition, FIELD_VECTOR ),
	DEFINE_FIELD( m_iDispVertStart, FIELD_INTEGER ),
	DEFINE_FIELD( m_iDispTriStart, FIELD_INTEGER ),
	DEFINE_FIELD( power, FIELD_INTEGER ),
	DEFINE_FIELD( minTess, FIELD_INTEGER ),
	DEFINE_FIELD( smoothingAngle, FIELD_FLOAT ),
	DEFINE_FIELD( contents, FIELD_INTEGER ),
	DEFINE_FIELD( m_iMapFace, FIELD_SHORT ),
	DEFINE_FIELD( m_iLightmapAlphaStart, FIELD_INTEGER ),
	DEFINE_FIELD( m_iLightmapSamplePositionStart, FIELD_INTEGER ),
	DEFINE_EMBEDDED_ARRAY( m_EdgeNeighbors, 4 ),
	DEFINE_EMBEDDED_ARRAY( m_CornerNeighbors, 4 ),
	DEFINE_ARRAY( m_AllowedVerts, FIELD_INTEGER, ddispinfo_t::ALLOWEDVERTS_SIZE ),	// uint32
END_BYTESWAP_DATADESC()

BEGIN_BYTESWAP_DATADESC( CDispNeighbor )
	DEFINE_EMBEDDED_ARRAY( m_SubNeighbors, 2 ),
END_BYTESWAP_DATADESC()

BEGIN_BYTESWAP_DATADESC( CDispCornerNeighbors )
	DEFINE_ARRAY( m_Neighbors, FIELD_SHORT, MAX_DISP_CORNER_NEIGHBORS ),
	DEFINE_FIELD( m_nNeighbors, FIELD_CHARACTER ),
END_BYTESWAP_DATADESC()

BEGIN_BYTESWAP_DATADESC( CDispSubNeighbor )
	DEFINE_FIELD( m_iNeighbor, FIELD_SHORT ),
	DEFINE_FIELD( m_NeighborOrientation, FIELD_CHARACTER ),
	DEFINE_FIELD( m_Span, FIELD_CHARACTER ),
	DEFINE_FIELD( m_NeighborSpan, FIELD_CHARACTER ),
END_BYTESWAP_DATADESC()

BEGIN_BYTESWAP_DATADESC( CDispVert )
	DEFINE_FIELD( m_vVector, FIELD_VECTOR ),
	DEFINE_FIELD( m_flDist, FIELD_FLOAT ),
	DEFINE_FIELD( m_flAlpha, FIELD_FLOAT ),
END_BYTESWAP_DATADESC()

BEGIN_BYTESWAP_DATADESC( CDispTri )
	DEFINE_FIELD( m_uiTags, FIELD_SHORT ),
END_BYTESWAP_DATADESC()

BEGIN_BYTESWAP_DATADESC( CFaceMacroTextureInfo )
	DEFINE_FIELD( m_MacroTextureNameID, FIELD_SHORT ),
END_BYTESWAP_DATADESC()

BEGIN_BYTESWAP_DATADESC( dprimitive_t )
	DEFINE_FIELD( type, FIELD_CHARACTER ),
	DEFINE_FIELD( firstIndex, FIELD_SHORT ),
	DEFINE_FIELD( indexCount, FIELD_SHORT ),
	DEFINE_FIELD( firstVert, FIELD_SHORT ),
	DEFINE_FIELD( vertCount, FIELD_SHORT ),
END_BYTESWAP_DATADESC()

BEGIN_BYTESWAP_DATADESC( dprimvert_t )
	DEFINE_FIELD( pos, FIELD_VECTOR ),
END_BYTESWAP_DATADESC()

BEGIN_BYTESWAP_DATADESC( dface_t )
	DEFINE_FIELD( planenum, FIELD_SHORT ),
	DEFINE_FIELD( side, FIELD_CHARACTER ),
	DEFINE_FIELD( onNode, FIELD_CHARACTER ),
	DEFINE_FIELD( firstedge, FIELD_INTEGER ),
	DEFINE_FIELD( numedges, FIELD_SHORT ),
	DEFINE_FIELD( texinfo, FIELD_SHORT ),
	DEFINE_FIELD( dispinfo, FIELD_SHORT ),
	DEFINE_FIELD( surfaceFogVolumeID, FIELD_SHORT ),
	DEFINE_ARRAY( styles, FIELD_CHARACTER, MAXLIGHTMAPS ),
	DEFINE_FIELD( lightofs, FIELD_INTEGER ),
	DEFINE_FIELD( area, FIELD_FLOAT ),
	DEFINE_ARRAY( m_LightmapTextureMinsInLuxels, FIELD_INTEGER, 2 ),
	DEFINE_ARRAY( m_LightmapTextureSizeInLuxels, FIELD_INTEGER, 2 ),
	DEFINE_FIELD( origFace, FIELD_INTEGER ),
	DEFINE_FIELD( m_NumPrims, FIELD_SHORT ),
	DEFINE_FIELD( firstPrimID, FIELD_SHORT ),
	DEFINE_FIELD( smoothingGroups, FIELD_INTEGER ),
END_BYTESWAP_DATADESC()

BEGIN_BYTESWAP_DATADESC( dfaceid_t )
	DEFINE_FIELD( hammerfaceid, FIELD_SHORT ),
END_BYTESWAP_DATADESC()

BEGIN_BYTESWAP_DATADESC( dbrush_t )
	DEFINE_FIELD( firstside, FIELD_INTEGER ),
	DEFINE_FIELD( numsides, FIELD_INTEGER ),
	DEFINE_FIELD( contents, FIELD_INTEGER ),
END_BYTESWAP_DATADESC()

BEGIN_BYTESWAP_DATADESC( dbrushside_t )
	DEFINE_FIELD( planenum, FIELD_SHORT ),
	DEFINE_FIELD( texinfo, FIELD_SHORT ),
	DEFINE_FIELD( dispinfo, FIELD_SHORT ),
	DEFINE_FIELD( bevel, FIELD_SHORT ),
END_BYTESWAP_DATADESC()

BEGIN_BYTESWAP_DATADESC( dedge_t )
	DEFINE_ARRAY( v, FIELD_SHORT, 2 ),
END_BYTESWAP_DATADESC()

BEGIN_BYTESWAP_DATADESC( dmodel_t )
	DEFINE_FIELD( mins, FIELD_VECTOR ),
	DEFINE_FIELD( maxs, FIELD_VECTOR ),
	DEFINE_FIELD( origin, FIELD_VECTOR ),
	DEFINE_FIELD( headnode, FIELD_INTEGER ),
	DEFINE_FIELD( firstface, FIELD_INTEGER ),
	DEFINE_FIELD( numfaces, FIELD_INTEGER ),
END_BYTESWAP_DATADESC()

BEGIN_BYTESWAP_DATADESC( dphysmodel_t )
	DEFINE_FIELD( modelIndex, FIELD_INTEGER ),
	DEFINE_FIELD( dataSize, FIELD_INTEGER ),
	DEFINE_FIELD( keydataSize, FIELD_INTEGER ),
	DEFINE_FIELD( solidCount, FIELD_INTEGER ),
END_BYTESWAP_DATADESC()

BEGIN_BYTESWAP_DATADESC( dphysdisp_t )
	DEFINE_FIELD( numDisplacements, FIELD_SHORT ),
END_BYTESWAP_DATADESC()

BEGIN_BYTESWAP_DATADESC( darea_t )
	DEFINE_FIELD( numareaportals, FIELD_INTEGER ),
	DEFINE_FIELD( firstareaportal, FIELD_INTEGER ),
END_BYTESWAP_DATADESC()

BEGIN_BYTESWAP_DATADESC( dareaportal_t )
	DEFINE_FIELD( m_PortalKey, FIELD_SHORT ),
	DEFINE_FIELD( otherarea, FIELD_SHORT ),
	DEFINE_FIELD( m_FirstClipPortalVert, FIELD_SHORT ),
	DEFINE_FIELD( m_nClipPortalVerts, FIELD_SHORT ),
	DEFINE_FIELD( planenum, FIELD_INTEGER ),
END_BYTESWAP_DATADESC()

BEGIN_BYTESWAP_DATADESC( dworldlight_t )
	DEFINE_FIELD( origin, FIELD_VECTOR ),
	DEFINE_FIELD( intensity, FIELD_VECTOR ),
	DEFINE_FIELD( normal, FIELD_VECTOR ),
	DEFINE_FIELD( cluster, FIELD_INTEGER ),
	DEFINE_FIELD( type, FIELD_INTEGER ),	// enumeration
	DEFINE_FIELD( style, FIELD_INTEGER ),
	DEFINE_FIELD( stopdot, FIELD_FLOAT ),
	DEFINE_FIELD( stopdot2, FIELD_FLOAT ),
	DEFINE_FIELD( exponent, FIELD_FLOAT ),
	DEFINE_FIELD( radius, FIELD_FLOAT ),
	DEFINE_FIELD( constant_attn, FIELD_FLOAT ),
	DEFINE_FIELD( linear_attn, FIELD_FLOAT ),
	DEFINE_FIELD( quadratic_attn, FIELD_FLOAT ),
	DEFINE_FIELD( flags, FIELD_INTEGER ),
	DEFINE_FIELD( texinfo, FIELD_INTEGER ),
	DEFINE_FIELD( owner, FIELD_INTEGER ),
END_BYTESWAP_DATADESC()

BEGIN_BYTESWAP_DATADESC( dleafwaterdata_t )
	DEFINE_FIELD( surfaceZ, FIELD_FLOAT ),
	DEFINE_FIELD( minZ, FIELD_FLOAT ),
	DEFINE_FIELD( surfaceTexInfoID, FIELD_SHORT ),
END_BYTESWAP_DATADESC()

BEGIN_BYTESWAP_DATADESC( doccluderdata_t )
	DEFINE_FIELD( flags, FIELD_INTEGER ),
	DEFINE_FIELD( firstpoly, FIELD_INTEGER ),
	DEFINE_FIELD( polycount, FIELD_INTEGER ),
	DEFINE_FIELD( mins, FIELD_VECTOR ),
	DEFINE_FIELD( maxs, FIELD_VECTOR ),
	DEFINE_FIELD( area, FIELD_INTEGER ),
END_BYTESWAP_DATADESC()

BEGIN_BYTESWAP_DATADESC( doccluderpolydata_t )
	DEFINE_FIELD( firstvertexindex, FIELD_INTEGER ),
	DEFINE_FIELD( vertexcount, FIELD_INTEGER ),
	DEFINE_FIELD( planenum, FIELD_INTEGER ),
END_BYTESWAP_DATADESC()

BEGIN_BYTESWAP_DATADESC( dcubemapsample_t )
	DEFINE_ARRAY( origin, FIELD_INTEGER, 3 ),
	DEFINE_FIELD( size, FIELD_CHARACTER ),
END_BYTESWAP_DATADESC()

BEGIN_BYTESWAP_DATADESC( doverlay_t )
	DEFINE_FIELD( nId, FIELD_INTEGER ),
	DEFINE_FIELD( nTexInfo, FIELD_SHORT ),
	DEFINE_FIELD( m_nFaceCountAndRenderOrder, FIELD_SHORT ),
	DEFINE_ARRAY( aFaces, FIELD_INTEGER, OVERLAY_BSP_FACE_COUNT ),
	DEFINE_ARRAY( flU, FIELD_FLOAT, 2 ),
	DEFINE_ARRAY( flV, FIELD_FLOAT, 2 ),
	DEFINE_ARRAY( vecUVPoints, FIELD_VECTOR, 4 ),
	DEFINE_FIELD( vecOrigin, FIELD_VECTOR ),
	DEFINE_FIELD( vecBasisNormal, FIELD_VECTOR ),
END_BYTESWAP_DATADESC()

BEGIN_BYTESWAP_DATADESC( dwateroverlay_t )
	DEFINE_FIELD( nId, FIELD_INTEGER ),
	DEFINE_FIELD( nTexInfo, FIELD_SHORT ),
	DEFINE_FIELD( m_nFaceCountAndRenderOrder, FIELD_SHORT ),
	DEFINE_ARRAY( aFaces, FIELD_INTEGER, WATEROVERLAY_BSP_FACE_COUNT ),
	DEFINE_ARRAY( flU, FIELD_FLOAT, 2 ),
	DEFINE_ARRAY( flV, FIELD_FLOAT, 2 ),
	DEFINE_ARRAY( vecUVPoints, FIELD_VECTOR, 4 ),
	DEFINE_FIELD( vecOrigin, FIELD_VECTOR ),
	DEFINE_FIELD( vecBasisNormal, FIELD_VECTOR ),
END_BYTESWAP_DATADESC()

BEGIN_BYTESWAP_DATADESC( doverlayfade_t )
	DEFINE_FIELD( flFadeDistMinSq, FIELD_FLOAT ),
	DEFINE_FIELD( flFadeDistMaxSq, FIELD_FLOAT ),
END_BYTESWAP_DATADESC()

BEGIN_BYTESWAP_DATADESC( dgamelumpheader_t )
	DEFINE_FIELD( lumpCount, FIELD_INTEGER ),
END_BYTESWAP_DATADESC()

BEGIN_BYTESWAP_DATADESC( dgamelump_t )
	DEFINE_FIELD( id, FIELD_INTEGER ),	// GameLumpId_t
	DEFINE_FIELD( flags, FIELD_SHORT ),
	DEFINE_FIELD( version, FIELD_SHORT ),
	DEFINE_FIELD( fileofs, FIELD_INTEGER ),
	DEFINE_FIELD( filelen, FIELD_INTEGER ),
END_BYTESWAP_DATADESC()

// From gamebspfile.h
BEGIN_BYTESWAP_DATADESC( StaticPropDictLump_t )
	DEFINE_ARRAY( m_Name, FIELD_CHARACTER, STATIC_PROP_NAME_LENGTH ),
END_BYTESWAP_DATADESC()

BEGIN_BYTESWAP_DATADESC( StaticPropLump_t )
	DEFINE_FIELD( m_Origin, FIELD_VECTOR ),
	DEFINE_FIELD( m_Angles, FIELD_VECTOR ),	// QAngle
	DEFINE_FIELD( m_PropType, FIELD_SHORT ),
	DEFINE_FIELD( m_FirstLeaf, FIELD_SHORT ),
	DEFINE_FIELD( m_LeafCount, FIELD_SHORT ),
	DEFINE_FIELD( m_Solid, FIELD_CHARACTER ),
	DEFINE_FIELD( m_Flags, FIELD_CHARACTER ),
	DEFINE_FIELD( m_Skin, FIELD_INTEGER ),
	DEFINE_FIELD( m_FadeMinDist, FIELD_FLOAT ),
	DEFINE_FIELD( m_FadeMaxDist, FIELD_FLOAT ),
	DEFINE_FIELD( m_LightingOrigin, FIELD_VECTOR ),
	DEFINE_FIELD( m_flForcedFadeScale, FIELD_FLOAT ),
	DEFINE_FIELD( m_nMinDXLevel, FIELD_SHORT ),
	DEFINE_FIELD( m_nMaxDXLevel, FIELD_SHORT ),
END_BYTESWAP_DATADESC()

BEGIN_BYTESWAP_DATADESC( StaticPropLumpV4_t )
	DEFINE_FIELD( m_Origin, FIELD_VECTOR ),
	DEFINE_FIELD( m_Angles, FIELD_VECTOR ),	// QAngle
	DEFINE_FIELD( m_PropType, FIELD_SHORT ),
	DEFINE_FIELD( m_FirstLeaf, FIELD_SHORT ),
	DEFINE_FIELD( m_LeafCount, FIELD_SHORT ),
	DEFINE_FIELD( m_Solid, FIELD_CHARACTER ),
	DEFINE_FIELD( m_Flags, FIELD_CHARACTER ),
	DEFINE_FIELD( m_Skin, FIELD_INTEGER ),
	DEFINE_FIELD( m_FadeMinDist, FIELD_FLOAT ),
	DEFINE_FIELD( m_FadeMaxDist, FIELD_FLOAT ),
	DEFINE_FIELD( m_LightingOrigin, FIELD_VECTOR ),
END_BYTESWAP_DATADESC()

BEGIN_BYTESWAP_DATADESC( StaticPropLumpV5_t )
	DEFINE_FIELD( m_Origin, FIELD_VECTOR ),
	DEFINE_FIELD( m_Angles, FIELD_VECTOR ),	// QAngle
	DEFINE_FIELD( m_PropType, FIELD_SHORT ),
	DEFINE_FIELD( m_FirstLeaf, FIELD_SHORT ),
	DEFINE_FIELD( m_LeafCount, FIELD_SHORT ),
	DEFINE_FIELD( m_Solid, FIELD_CHARACTER ),
	DEFINE_FIELD( m_Flags, FIELD_CHARACTER ),
	DEFINE_FIELD( m_Skin, FIELD_INTEGER ),
	DEFINE_FIELD( m_FadeMinDist, FIELD_FLOAT ),
	DEFINE_FIELD( m_FadeMaxDist, FIELD_FLOAT ),
	DEFINE_FIELD( m_LightingOrigin, FIELD_VECTOR ),
	DEFINE_FIELD( m_flForcedFadeScale, FIELD_FLOAT ),
END_BYTESWAP_DATADESC()

BEGIN_BYTESWAP_DATADESC( StaticPropLeafLump_t )
	DEFINE_FIELD( m_Leaf, FIELD_SHORT ),
END_BYTESWAP_DATADESC()

BEGIN_BYTESWAP_DATADESC( DetailObjectDictLump_t )
	DEFINE_ARRAY( m_Name, FIELD_CHARACTER, DETAIL_NAME_LENGTH ),
END_BYTESWAP_DATADESC()

BEGIN_BYTESWAP_DATADESC( DetailObjectLump_t )
	DEFINE_FIELD( m_Origin, FIELD_VECTOR ),
	DEFINE_FIELD( m_Angles, FIELD_VECTOR ),			// QAngle
	DEFINE_FIELD( m_DetailModel, FIELD_SHORT ),
	DEFINE_FIELD( m_Leaf, FIELD_SHORT ),
	DEFINE_ARRAY( m_Lighting, FIELD_CHARACTER, 4 ),	// ColorRGBExp32
	DEFINE_FIELD( m_LightStyles, FIELD_INTEGER ),
	DEFINE_FIELD( m_LightStyleCount, FIELD_CHARACTER ),
	DEFINE_FIELD( m_SwayAmount, FIELD_CHARACTER ),
	DEFINE_FIELD( m_ShapeAngle, FIELD_CHARACTER ),
	DEFINE_FIELD( m_ShapeSize, FIELD_CHARACTER ),
	DEFINE_FIELD( m_Orientation, FIELD_CHARACTER ),
	DEFINE_ARRAY( m_Padding2, FIELD_CHARACTER, 3 ),
	DEFINE_FIELD( m_Type, FIELD_CHARACTER ),
	DEFINE_ARRAY( m_Padding3, FIELD_CHARACTER, 3 ),
	DEFINE_FIELD( m_flScale, FIELD_FLOAT ),
END_BYTESWAP_DATADESC()

BEGIN_BYTESWAP_DATADESC( DetailSpriteDictLump_t )
	DEFINE_FIELD( m_UL, FIELD_VECTOR2D ),
	DEFINE_FIELD( m_LR, FIELD_VECTOR2D ),
	DEFINE_FIELD( m_TexUL, FIELD_VECTOR2D ),
	DEFINE_FIELD( m_TexLR, FIELD_VECTOR2D ),
END_BYTESWAP_DATADESC()

BEGIN_BYTESWAP_DATADESC( DetailPropLightstylesLump_t )
	DEFINE_ARRAY( m_Lighting, FIELD_CHARACTER, 4 ),	// ColorRGBExp32
	DEFINE_FIELD( m_Style, FIELD_CHARACTER ),
END_BYTESWAP_DATADESC()

// From vradstaticprops.h
namespace HardwareVerts
{
BEGIN_BYTESWAP_DATADESC( MeshHeader_t )
	DEFINE_FIELD( m_nLod, FIELD_INTEGER ),
	DEFINE_FIELD( m_nVertexes, FIELD_INTEGER ),
	DEFINE_FIELD( m_nOffset, FIELD_INTEGER ),
	DEFINE_ARRAY( m_nUnused, FIELD_INTEGER, 4 ),
END_BYTESWAP_DATADESC()

BEGIN_BYTESWAP_DATADESC( FileHeader_t )
	DEFINE_FIELD( m_nVersion, FIELD_INTEGER ),
	DEFINE_FIELD( m_nChecksum, FIELD_INTEGER ),
	DEFINE_FIELD( m_nVertexFlags, FIELD_INTEGER ),
	DEFINE_FIELD( m_nVertexSize, FIELD_INTEGER ),
	DEFINE_FIELD( m_nVertexes, FIELD_INTEGER ),
	DEFINE_FIELD( m_nMeshes, FIELD_INTEGER ),
	DEFINE_ARRAY( m_nUnused, FIELD_INTEGER, 4 ),
END_BYTESWAP_DATADESC()
} // end namespace

static const char *s_LumpNames[] = {
	"LUMP_ENTITIES",						// 0
	"LUMP_PLANES",							// 1
	"LUMP_TEXDATA",							// 2
	"LUMP_VERTEXES",						// 3
	"LUMP_VISIBILITY",						// 4
	"LUMP_NODES",							// 5
	"LUMP_TEXINFO",							// 6
	"LUMP_FACES",							// 7
	"LUMP_LIGHTING",						// 8
	"LUMP_OCCLUSION",						// 9
	"LUMP_LEAFS",							// 10
	"LUMP_FACEIDS",							// 11
	"LUMP_EDGES",							// 12
	"LUMP_SURFEDGES",						// 13
	"LUMP_MODELS",							// 14
	"LUMP_WORLDLIGHTS",						// 15
	"LUMP_LEAFFACES",						// 16
	"LUMP_LEAFBRUSHES",						// 17
	"LUMP_BRUSHES",							// 18
	"LUMP_BRUSHSIDES",						// 19
	"LUMP_AREAS",							// 20
	"LUMP_AREAPORTALS",						// 21
	"LUMP_UNUSED0",							// 22
	"LUMP_UNUSED1",							// 23
	"LUMP_UNUSED2",							// 24
	"LUMP_UNUSED3",							// 25
	"LUMP_DISPINFO",						// 26
	"LUMP_ORIGINALFACES",					// 27
	"LUMP_PHYSDISP",						// 28
	"LUMP_PHYSCOLLIDE",						// 29
	"LUMP_VERTNORMALS",						// 30
	"LUMP_VERTNORMALINDICES",				// 31
	"LUMP_DISP_LIGHTMAP_ALPHAS",			// 32
	"LUMP_DISP_VERTS",						// 33
	"LUMP_DISP_LIGHTMAP_SAMPLE_POSITIONS",	// 34
	"LUMP_GAME_LUMP",						// 35
	"LUMP_LEAFWATERDATA",					// 36
	"LUMP_PRIMITIVES",						// 37
	"LUMP_PRIMVERTS",						// 38
	"LUMP_PRIMINDICES",						// 39
	"LUMP_PAKFILE",							// 40
	"LUMP_CLIPPORTALVERTS",					// 41
	"LUMP_CUBEMAPS",						// 42
	"LUMP_TEXDATA_STRING_DATA",				// 43
	"LUMP_TEXDATA_STRING_TABLE",			// 44
	"LUMP_OVERLAYS",						// 45
	"LUMP_LEAFMINDISTTOWATER",				// 46
	"LUMP_FACE_MACRO_TEXTURE_INFO",			// 47
	"LUMP_DISP_TRIS",						// 48
	"LUMP_PHYSCOLLIDESURFACE",				// 49
	"LUMP_WATEROVERLAYS",					// 50
	"LUMP_LEAF_AMBIENT_INDEX_HDR",			// 51
	"LUMP_LEAF_AMBIENT_INDEX",				// 52
	"LUMP_LIGHTING_HDR",					// 53
	"LUMP_WORLDLIGHTS_HDR",					// 54
	"LUMP_LEAF_AMBIENT_LIGHTING_HDR",		// 55
	"LUMP_LEAF_AMBIENT_LIGHTING",			// 56
	"LUMP_XZIPPAKFILE",						// 57
	"LUMP_FACES_HDR",						// 58
	"LUMP_MAP_FLAGS",						// 59
	"LUMP_OVERLAY_FADES",					// 60
};

[[nodiscard]] static const char *GetLumpName( unsigned int lumpnum )
{
	if ( lumpnum >= std::size( s_LumpNames ) )
	{
		return "UNKNOWN";
	}
	return s_LumpNames[lumpnum];
}

// "-hdr" tells us to use the HDR fields (if present) on the light sources.  Also, tells us to write
// out the HDR lumps for lightmaps, ambient leaves, and lights sources.
bool g_bHDR = false;

// Set to true to generate Xbox360 native output files
static bool g_bSwapOnLoad = false;
static bool g_bSwapOnWrite = false;

static VTFConvertFunc_t		g_pVTFConvertFunc;
static VHVFixupFunc_t		g_pVHVFixupFunc;
static CompressFunc_t		g_pCompressFunc;

static CUtlVector< CUtlString >	g_StaticPropNames;
static CUtlVector< int >		g_StaticPropInstances;

CByteswap	g_Swap;

uint32 g_LevelFlags = 0;

int			nummodels;
dmodel_t	dmodels[MAX_MAP_MODELS];

int			visdatasize;
alignas(dvis_t) byte dvisdata[MAX_MAP_VISIBILITY];
dvis_t		*dvis = (dvis_t *)dvisdata;

CUtlVector<byte> dlightdataHDR;
CUtlVector<byte> dlightdataLDR;
CUtlVector<byte> *pdlightdata = &dlightdataLDR;

CUtlVector<char> dentdata;

int			numleafs;
#if !defined( BSP_USE_LESS_MEMORY )
dleaf_t		dleafs[MAX_MAP_LEAFS];
#else
dleaf_t		*dleafs;
#endif

CUtlVector<dleafambientindex_t> g_LeafAmbientIndexLDR;
CUtlVector<dleafambientindex_t> g_LeafAmbientIndexHDR;
CUtlVector<dleafambientindex_t> *g_pLeafAmbientIndex = NULL;
CUtlVector<dleafambientlighting_t> g_LeafAmbientLightingLDR;
CUtlVector<dleafambientlighting_t> g_LeafAmbientLightingHDR;
CUtlVector<dleafambientlighting_t> *g_pLeafAmbientLighting = NULL;

unsigned short  g_LeafMinDistToWater[MAX_MAP_LEAFS];

int			numplanes;
dplane_t	dplanes[MAX_MAP_PLANES];

int			numvertexes;
dvertex_t	dvertexes[MAX_MAP_VERTS];

int				g_numvertnormalindices;	// dfaces reference these. These index g_vertnormals.
unsigned short	g_vertnormalindices[MAX_MAP_VERTNORMALS];

int				g_numvertnormals;	
Vector			g_vertnormals[MAX_MAP_VERTNORMALS];

int			numnodes;
dnode_t		dnodes[MAX_MAP_NODES];

CUtlVector<texinfo_t> texinfo( MAX_MAP_TEXINFO );

int			numtexdata;
dtexdata_t	dtexdata[MAX_MAP_TEXDATA];

//
// displacement map bsp file info: dispinfo
//
CUtlVector<ddispinfo_t> g_dispinfo;
CUtlVector<CDispVert> g_DispVerts;
CUtlVector<CDispTri> g_DispTris;
CUtlVector<unsigned char> g_DispLightmapSamplePositions; // LUMP_DISP_LIGHTMAP_SAMPLE_POSITIONS

int         numorigfaces;
dface_t     dorigfaces[MAX_MAP_FACES];

int				g_numprimitives = 0;
dprimitive_t	g_primitives[MAX_MAP_PRIMITIVES];

int				g_numprimverts = 0;
dprimvert_t		g_primverts[MAX_MAP_PRIMVERTS];

int				g_numprimindices = 0;
unsigned short	g_primindices[MAX_MAP_PRIMINDICES];

int			numfaces;
dface_t		dfaces[MAX_MAP_FACES];

int			numfaceids;
CUtlVector<dfaceid_t>	dfaceids;

int			numfaces_hdr;
dface_t		dfaces_hdr[MAX_MAP_FACES];

int			numedges;
dedge_t		dedges[MAX_MAP_EDGES];

int			numleaffaces;
unsigned short		dleaffaces[MAX_MAP_LEAFFACES];

int			numleafbrushes;
unsigned short		dleafbrushes[MAX_MAP_LEAFBRUSHES];

int			numsurfedges;
int			dsurfedges[MAX_MAP_SURFEDGES];

int			numbrushes;
dbrush_t	dbrushes[MAX_MAP_BRUSHES];

int			numbrushsides;
dbrushside_t	dbrushsides[MAX_MAP_BRUSHSIDES];

int			numareas;
darea_t		dareas[MAX_MAP_AREAS];

int			numareaportals;
dareaportal_t	dareaportals[MAX_MAP_AREAPORTALS];

int			numworldlightsLDR;
dworldlight_t dworldlightsLDR[MAX_MAP_WORLDLIGHTS];

int			numworldlightsHDR;
dworldlight_t dworldlightsHDR[MAX_MAP_WORLDLIGHTS];

int			*pNumworldlights = &numworldlightsLDR;
dworldlight_t *dworldlights = dworldlightsLDR;

int			numleafwaterdata = 0;
dleafwaterdata_t dleafwaterdata[MAX_MAP_LEAFWATERDATA]; 

CUtlVector<CFaceMacroTextureInfo>	g_FaceMacroTextureInfos;

Vector				g_ClipPortalVerts[MAX_MAP_PORTALVERTS];
int					g_nClipPortalVerts;

dcubemapsample_t	g_CubemapSamples[MAX_MAP_CUBEMAPSAMPLES];
int					g_nCubemapSamples = 0;

int					g_nOverlayCount;
doverlay_t			g_Overlays[MAX_MAP_OVERLAYS];
doverlayfade_t		g_OverlayFades[MAX_MAP_OVERLAYS];

int					g_nWaterOverlayCount;
dwateroverlay_t		g_WaterOverlays[MAX_MAP_WATEROVERLAYS];

CUtlVector<char>	g_TexDataStringData;
CUtlVector<int>		g_TexDataStringTable;

byte				*g_pPhysCollide = NULL;
int					g_PhysCollideSize = 0;
int					g_MapRevision = 0;

byte				*g_pPhysDisp = NULL;
int					g_PhysDispSize = 0;

CUtlVector<doccluderdata_t>	g_OccluderData( 256, 256 );
CUtlVector<doccluderpolydata_t>	g_OccluderPolyData( 1024, 1024 );
CUtlVector<int>	g_OccluderVertexIndices( 2048, 2048 );
 
template <class T> static void WriteData( FileHandle_t file, T *pData, int count = 1 );
template <class T> static void WriteData( FileHandle_t file, int fieldType, T *pData, int count = 1 );
template< class T > static void AddLump( dheader_t *header, FileHandle_t file, int lumpnum, T *pData, intp count, int version = 0 );
template< class T > static void AddLump( dheader_t *header, FileHandle_t file, int lumpnum, CUtlVector<T> &data, int version = 0 );

struct Lump_t
{
	void	*pLumps[HEADER_LUMPS];
	int		size[HEADER_LUMPS];
	bool	bLumpParsed[HEADER_LUMPS];
} g_Lumps;

CGameLump	g_GameLumps;

static IZip *s_pakFile = 0;

//-----------------------------------------------------------------------------
// Keep the file position aligned to an arbitrary boundary.
// Returns updated file position.
//-----------------------------------------------------------------------------
static uintp AlignFilePosition( FileHandle_t hFile, unsigned alignment )
{
	uint currPosition = g_pFileSystem->Tell( hFile );

	if ( alignment >= 2 )
	{
		uint newPosition = AlignValue( currPosition, alignment );
		uint count = newPosition - currPosition;
		if ( count )
		{
			char smallBuffer[4096];
			char *pBuffer{count > sizeof( smallBuffer ) ? (char *)malloc( count ) : smallBuffer};

			memset( pBuffer, 0, count );
			SafeWrite( hFile, pBuffer, count );

			if ( pBuffer != smallBuffer )
			{
				free( pBuffer );
			}

			currPosition = newPosition;
		}
	}

	return currPosition;
}

//-----------------------------------------------------------------------------
// Purpose: // Get a pakfile instance
// Output : IZip*
//-----------------------------------------------------------------------------
IZip* GetPakFile( void )
{
	if ( !s_pakFile )
	{
		s_pakFile = IZip::CreateZip();
	}
	return s_pakFile;
}

//-----------------------------------------------------------------------------
// Purpose: Free the pak files
//-----------------------------------------------------------------------------
void ReleasePakFileLumps( void )
{
	// Release the pak files
	IZip::ReleaseZip( s_pakFile );
	s_pakFile = NULL;
}

//-----------------------------------------------------------------------------
// Purpose: Set the sector alignment for all subsequent zip operations
//-----------------------------------------------------------------------------
void ForceAlignment( IZip *pak, bool bAlign, bool bCompatibleFormat, unsigned int alignmentSize )
{
	pak->ForceAlignment( bAlign, bCompatibleFormat, alignmentSize );
}

//-----------------------------------------------------------------------------
// Purpose: Store data back out to .bsp file
//-----------------------------------------------------------------------------
static void WritePakFileLump( dheader_t *header, FileHandle_t file )
{
	CUtlBuffer buf( (intp)0, 0 );
	GetPakFile()->ActivateByteSwapping( IsX360() );
	GetPakFile()->SaveToBuffer( buf );

	// must respect pak file alignment
	// pad up and ensure lump starts on same aligned boundary
	AlignFilePosition( file, GetPakFile()->GetAlignment() );
	
	// Now store final buffers out to file
	AddLump( header, file, LUMP_PAKFILE, buf.Base<byte>(), buf.TellPut() );
}

//-----------------------------------------------------------------------------
// Purpose: Remove all entries
//-----------------------------------------------------------------------------
void ClearPakFile( IZip *pak )
{
	pak->Reset();
}

//-----------------------------------------------------------------------------
// Purpose: Add file from disk to .bsp PAK lump
// Input  : *relativename - 
//			*fullpath - 
//-----------------------------------------------------------------------------
void AddFileToPak( IZip *pak, const char *relativename, const char *fullpath, IZip::eCompressionType compressionType )
{
	DevMsg( "Adding file to pakfile [ %s ]\n", fullpath );
	pak->AddFileToZip( relativename, fullpath, compressionType );
}

//-----------------------------------------------------------------------------
// Purpose: Add buffer to .bsp PAK lump as named file
// Input  : *relativename - 
//			*data - 
//			length - 
//-----------------------------------------------------------------------------
void AddBufferToPak( IZip *pak, const char *pRelativeName, void *data, int length, bool bTextMode, IZip::eCompressionType compressionType )
{
	pak->AddBufferToZip( pRelativeName, data, length, bTextMode, compressionType );
}

//-----------------------------------------------------------------------------
// Purpose: Add entire directory to .bsp PAK lump as named file
// Input  : *relativename - 
//			*data - 
//			length - 
//-----------------------------------------------------------------------------
void AddDirToPak( IZip *pak, const char *pDirPath, const char *pPakPrefix )
{
	if ( !g_pFullFileSystem->IsDirectory( pDirPath ) )
	{
		Warning( "Passed non-directory to AddDirToPak [ %s ]\n", pDirPath );
		return;
	}

	DevMsg( "Adding directory to pakfile [ %s ]\n", pDirPath );

	// Enumerate dir
	char szEnumerateDir[MAX_PATH] = { 0 };
	V_snprintf( szEnumerateDir, sizeof( szEnumerateDir ), "%s/*.*", pDirPath );
	V_FixSlashes( szEnumerateDir );

	FileFindHandle_t handle = FILESYSTEM_INVALID_FIND_HANDLE;
	const char *szFindResult = g_pFullFileSystem->FindFirst( szEnumerateDir, &handle );
	do
	{
		if ( szFindResult[0] != '.' )
		{
			char szPakName[MAX_PATH] = { 0 };
			char szFullPath[MAX_PATH] = { 0 };
			if ( pPakPrefix )
			{
				V_snprintf( szPakName, sizeof( szPakName ), "%s/%s", pPakPrefix, szFindResult );
			}
			else
			{
				V_strncpy( szPakName, szFindResult, sizeof( szPakName ) );
			}
			V_snprintf( szFullPath, sizeof( szFullPath ), "%s/%s", pDirPath, szFindResult );
			V_FixDoubleSlashes( szFullPath );
			V_FixDoubleSlashes( szPakName );

			if ( g_pFullFileSystem->FindIsDirectory( handle ) )
			{
				// Recurse
				AddDirToPak( pak, szFullPath, szPakName );
			}
			else
			{
				// Just add this file
				AddFileToPak( pak, szPakName, szFullPath );
			}
		}
		szFindResult = g_pFullFileSystem->FindNext( handle );
	} while ( szFindResult);
}

//-----------------------------------------------------------------------------
// Purpose: Check if a file already exists in the pack file.
// Input  : *relativename - 
//-----------------------------------------------------------------------------
bool FileExistsInPak( IZip *pak, const char *pRelativeName )
{
	return pak->FileExistsInZip( pRelativeName );
}


//-----------------------------------------------------------------------------
// Read a file from the pack file
//-----------------------------------------------------------------------------
bool ReadFileFromPak( IZip *pak, const char *pRelativeName, bool bTextMode, CUtlBuffer &buf )
{
	return pak->ReadFileFromZip( pRelativeName, bTextMode, buf );
}


//-----------------------------------------------------------------------------
// Purpose: Remove file from .bsp PAK lump
// Input  : *relativename - 
//-----------------------------------------------------------------------------
void RemoveFileFromPak( IZip *pak, const char *relativename )
{
	pak->RemoveFileFromZip( relativename );
}


//-----------------------------------------------------------------------------
// Purpose: Get next filename in directory
// Input  : id, -1 to start, returns next id, or -1 at list conclusion 
//-----------------------------------------------------------------------------
int GetNextFilename( IZip *pak, int id, char *pBuffer, int bufferSize, int &fileSize )
{
	return pak->GetNextFilename( id, pBuffer, bufferSize, fileSize );
}

//-----------------------------------------------------------------------------
// Convert four-CC code to a handle	+ back
//-----------------------------------------------------------------------------
GameLumpHandle_t CGameLump::GetGameLumpHandle( GameLumpId_t id )
{
	// NOTE: I'm also expecting game lump id's to be four-CC codes
	Assert( id > HEADER_LUMPS );

	FOR_EACH_LL(m_GameLumps, i)
	{
		if (m_GameLumps[i].m_Id == id)
			return i;
	}

	return InvalidGameLump();
}

GameLumpId_t CGameLump::GetGameLumpId( GameLumpHandle_t handle )
{
	return m_GameLumps[handle].m_Id;
}

unsigned short CGameLump::GetGameLumpFlags( GameLumpHandle_t handle )
{
	return m_GameLumps[handle].m_Flags;
}

unsigned short CGameLump::GetGameLumpVersion( GameLumpHandle_t handle )
{
	return m_GameLumps[handle].m_Version;
}


//-----------------------------------------------------------------------------
// Game lump accessor methods 
//-----------------------------------------------------------------------------

void*	CGameLump::GetGameLump( GameLumpHandle_t id )
{
	return m_GameLumps[id].m_Memory.Base();
}

intp	CGameLump::GameLumpSize( GameLumpHandle_t id )
{
	return m_GameLumps[id].m_Memory.NumAllocated();
}


//-----------------------------------------------------------------------------
// Game lump iteration methods 
//-----------------------------------------------------------------------------

GameLumpHandle_t	CGameLump::FirstGameLump()
{
	return (m_GameLumps.Count()) ? m_GameLumps.Head() : InvalidGameLump();
}

GameLumpHandle_t	CGameLump::NextGameLump( GameLumpHandle_t handle )
{

	return (m_GameLumps.IsValidIndex(handle)) ? m_GameLumps.Next(handle) : InvalidGameLump();
}

GameLumpHandle_t	CGameLump::InvalidGameLump()
{
	return 0xFFFF;
}


//-----------------------------------------------------------------------------
// Game lump creation/destruction method
//-----------------------------------------------------------------------------

GameLumpHandle_t	CGameLump::CreateGameLump( GameLumpId_t id, int size, unsigned short flags, unsigned short version )
{
	Assert( GetGameLumpHandle(id) == InvalidGameLump() );
	GameLumpHandle_t handle = m_GameLumps.AddToTail();
	m_GameLumps[handle].m_Id = id;
	m_GameLumps[handle].m_Flags = flags;
	m_GameLumps[handle].m_Version = version;
	m_GameLumps[handle].m_Memory.EnsureCapacity( size );
	return handle;
}

void	CGameLump::DestroyGameLump( GameLumpHandle_t handle )
{
	m_GameLumps.Remove( handle );
}

void	CGameLump::DestroyAllGameLumps()
{
	m_GameLumps.RemoveAll();
}

//-----------------------------------------------------------------------------
// Compute file size and clump count
//-----------------------------------------------------------------------------

void CGameLump::ComputeGameLumpSizeAndCount( intp& size, intp& clumpCount )
{
	// Figure out total size of the client lumps
	size = 0;
	clumpCount = 0;

	for( auto h = FirstGameLump(); h != InvalidGameLump(); h = NextGameLump( h ) )
	{
		++clumpCount;
		size += GameLumpSize( h );
	}

	// Add on headers
	size += sizeof( dgamelumpheader_t ) + clumpCount * sizeof( dgamelump_t );
}


void CGameLump::SwapGameLump( GameLumpId_t id, unsigned short version, byte *dest, byte *src, int length )
{
	int count = 0;
	switch( id )
	{
	case GAMELUMP_STATIC_PROPS:
		// Swap the static prop model dict
		count = *(int*)src;
		g_Swap.SwapBufferToTargetEndian( (int*)dest, (int*)src );
		count = g_bSwapOnLoad ? *(int*)dest : count;
		src += sizeof(int);
		dest += sizeof(int);

		g_Swap.SwapFieldsToTargetEndian( (StaticPropDictLump_t*)dest, (StaticPropDictLump_t*)src, count );
		src += sizeof(StaticPropDictLump_t) * count;
		dest += sizeof(StaticPropDictLump_t) * count;

		// Swap the leaf list
		count = *(int*)src;
		g_Swap.SwapBufferToTargetEndian( (int*)dest, (int*)src );
		count = g_bSwapOnLoad ? *(int*)dest : count;
		src += sizeof(int);
		dest += sizeof(int);

		g_Swap.SwapFieldsToTargetEndian( (StaticPropLeafLump_t*)dest, (StaticPropLeafLump_t*)src, count );
		src += sizeof(StaticPropLeafLump_t) * count;
		dest += sizeof(StaticPropLeafLump_t) * count;

		// Swap the models
		count = *(int*)src;
		g_Swap.SwapBufferToTargetEndian( (int*)dest, (int*)src );
		count = g_bSwapOnLoad ? *(int*)dest : count;
		src += sizeof(int);
		dest += sizeof(int);

		// The one-at-a-time swap is to compensate for these structures 
		// possibly being misaligned, which crashes the Xbox 360.
		if ( version == 4 )
		{
			StaticPropLumpV4_t lump;
			for ( int i = 0; i < count; ++i )
			{
				Q_memcpy( &lump, src, sizeof(StaticPropLumpV4_t) );
				g_Swap.SwapFieldsToTargetEndian( &lump, &lump );
				Q_memcpy( dest, &lump, sizeof(StaticPropLumpV4_t) );
				src += sizeof( StaticPropLumpV4_t );
				dest += sizeof( StaticPropLumpV4_t );
			}
		}
		else if ( version == 5 )
		{
			StaticPropLumpV5_t lump;
			for ( int i = 0; i < count; ++i )
			{
				Q_memcpy( &lump, src, sizeof(StaticPropLumpV5_t) );
				g_Swap.SwapFieldsToTargetEndian( &lump, &lump );
				Q_memcpy( dest, &lump, sizeof(StaticPropLumpV5_t) );
				src += sizeof( StaticPropLumpV5_t );
				dest += sizeof( StaticPropLumpV5_t );
			}
		}
		else
		{
			if ( version != 6 )
			{
				Error( "Unknown Static Prop Lump version %hu didn't get swapped!\n", version );
			}

			StaticPropLump_t lump;
			for ( int i = 0; i < count; ++i )
			{
				Q_memcpy( &lump, src, sizeof(StaticPropLump_t) );
				g_Swap.SwapFieldsToTargetEndian( &lump, &lump );
				Q_memcpy( dest, &lump, sizeof(StaticPropLump_t) );
				src += sizeof( StaticPropLump_t );
				dest += sizeof( StaticPropLump_t );
			}
		}
		break;

	case GAMELUMP_DETAIL_PROPS:
		// Swap the detail prop model dict
		count = *(int*)src;
		g_Swap.SwapBufferToTargetEndian( (int*)dest, (int*)src );
		count = g_bSwapOnLoad ? *(int*)dest : count;
		src += sizeof(int);
		dest += sizeof(int);

		g_Swap.SwapFieldsToTargetEndian( (DetailObjectDictLump_t*)dest, (DetailObjectDictLump_t*)src, count );
		src += sizeof(DetailObjectDictLump_t) * count;
		dest += sizeof(DetailObjectDictLump_t) * count;

		if ( version == 4 )
		{
			// Swap the detail sprite dict
			count = *(int*)src;
			g_Swap.SwapBufferToTargetEndian( (int*)dest, (int*)src );
			count = g_bSwapOnLoad ? *(int*)dest : count;
			src += sizeof(int);
			dest += sizeof(int);

			DetailSpriteDictLump_t spritelump;
			for ( int i = 0; i < count; ++i )
			{
				Q_memcpy( &spritelump, src, sizeof(DetailSpriteDictLump_t) );
				g_Swap.SwapFieldsToTargetEndian( &spritelump, &spritelump );
				Q_memcpy( dest, &spritelump, sizeof(DetailSpriteDictLump_t) );
				src += sizeof(DetailSpriteDictLump_t);
				dest += sizeof(DetailSpriteDictLump_t);
			}

			// Swap the models
			count = *(int*)src;
			g_Swap.SwapBufferToTargetEndian( (int*)dest, (int*)src );
			count = g_bSwapOnLoad ? *(int*)dest : count;
			src += sizeof(int);
			dest += sizeof(int);

			DetailObjectLump_t objectlump;
			for ( int i = 0; i < count; ++i )
			{
				Q_memcpy( &objectlump, src, sizeof(DetailObjectLump_t) );
				g_Swap.SwapFieldsToTargetEndian( &objectlump, &objectlump );
				Q_memcpy( dest, &objectlump, sizeof(DetailObjectLump_t) );
				src += sizeof(DetailObjectLump_t);
				dest += sizeof(DetailObjectLump_t);
			}
		}
		break;

	case GAMELUMP_DETAIL_PROP_LIGHTING:
		// Swap the LDR light styles
		count = *(int*)src;
		g_Swap.SwapBufferToTargetEndian( (int*)dest, (int*)src );
		count = g_bSwapOnLoad ? *(int*)dest : count;
		src += sizeof(int);
		dest += sizeof(int);

		g_Swap.SwapFieldsToTargetEndian( (DetailPropLightstylesLump_t*)dest, (DetailPropLightstylesLump_t*)src, count );
		src += sizeof(DetailObjectDictLump_t) * count;
		dest += sizeof(DetailObjectDictLump_t) * count;
		break;

	case GAMELUMP_DETAIL_PROP_LIGHTING_HDR:
		// Swap the HDR light styles
		count = *(int*)src;
		g_Swap.SwapBufferToTargetEndian( (int*)dest, (int*)src );
		count = g_bSwapOnLoad ? *(int*)dest : count;
		src += sizeof(int);
		dest += sizeof(int);

		g_Swap.SwapFieldsToTargetEndian( (DetailPropLightstylesLump_t*)dest, (DetailPropLightstylesLump_t*)src, count );
		src += sizeof(DetailObjectDictLump_t) * count;
		dest += sizeof(DetailObjectDictLump_t) * count;
		break;

	default:
		char idchars[5] = {0};
		Q_memcpy( idchars, &id, 4 );
		Warning( "Unknown game lump '%s' didn't get swapped!\n", idchars );
		memcpy ( dest, src, length);
		break;
	}
}

//-----------------------------------------------------------------------------
// Game lump file I/O
//-----------------------------------------------------------------------------
void CGameLump::ParseGameLump( dheader_t* pHeader )
{
	g_GameLumps.DestroyAllGameLumps();

	g_Lumps.bLumpParsed[LUMP_GAME_LUMP] = true;

	int length = pHeader->lumps[LUMP_GAME_LUMP].filelen;
	int ofs = pHeader->lumps[LUMP_GAME_LUMP].fileofs;
	
	if (length > 0)
	{
		// Read dictionary...
		dgamelumpheader_t* pGameLumpHeader = (dgamelumpheader_t*)((byte *)pHeader + ofs);
		if ( g_bSwapOnLoad )
		{
			g_Swap.SwapFieldsToTargetEndian( pGameLumpHeader );
		}
		dgamelump_t* pGameLump = (dgamelump_t*)(pGameLumpHeader + 1);
		for (int i = 0; i < pGameLumpHeader->lumpCount; ++i )
		{
			if ( g_bSwapOnLoad )
			{
				g_Swap.SwapFieldsToTargetEndian( &pGameLump[i] );
			}

			int filelen = pGameLump[i].filelen;
			GameLumpHandle_t lump = g_GameLumps.CreateGameLump( pGameLump[i].id, filelen, pGameLump[i].flags, pGameLump[i].version );
			if ( g_bSwapOnLoad )
			{
				SwapGameLump( pGameLump[i].id, pGameLump[i].version, (byte*)g_GameLumps.GetGameLump(lump), (byte *)pHeader + pGameLump[i].fileofs, filelen );
			}
			else
			{
				memcpy( g_GameLumps.GetGameLump(lump), (byte *)pHeader + pGameLump[i].fileofs, filelen );
			}
		}
	}
}


//-----------------------------------------------------------------------------
// String table methods
//-----------------------------------------------------------------------------
const char *TexDataStringTable_GetString( intp stringID )
{
	return &g_TexDataStringData[g_TexDataStringTable[stringID]];
}

intp	TexDataStringTable_AddOrFindString( const char *pString )
{
	// garymcthack: Make this use an RBTree!
	for( intp i = 0; i < g_TexDataStringTable.Count(); i++ )
	{
		if( stricmp( pString, &g_TexDataStringData[g_TexDataStringTable[i]] ) == 0 )
		{
			return i;
		}
	}

	intp len = V_strlen( pString );
	intp outOffset = g_TexDataStringData.AddMultipleToTail( len+1, pString );
	intp outIndex = g_TexDataStringTable.AddToTail( outOffset );
	return outIndex;
}

//-----------------------------------------------------------------------------
// Adds all game lumps into one big block
//-----------------------------------------------------------------------------

static void AddGameLumps( dheader_t *dheader, FileHandle_t file )
{
	// Figure out total size of the client lumps
	intp size, clumpCount;
	g_GameLumps.ComputeGameLumpSizeAndCount( size, clumpCount );

	// Set up the main lump dictionary entry
	g_Lumps.size[LUMP_GAME_LUMP] = 0;	// mark it written

	lump_t* lump = &dheader->lumps[LUMP_GAME_LUMP];
	
	lump->fileofs = g_pFileSystem->Tell( file );
	lump->filelen = size;

	// write header
	dgamelumpheader_t header = {0};
	header.lumpCount = clumpCount;
	WriteData( file, &header );

	// write dictionary
	dgamelump_t dict;
	int offset = lump->fileofs + sizeof(header) + clumpCount * sizeof(dgamelump_t);
	GameLumpHandle_t h;
	for( h = g_GameLumps.FirstGameLump(); h != g_GameLumps.InvalidGameLump(); h = g_GameLumps.NextGameLump( h ) )
	{
		dict.id = g_GameLumps.GetGameLumpId(h);
		dict.version = g_GameLumps.GetGameLumpVersion(h);
		dict.flags = g_GameLumps.GetGameLumpFlags(h);
		dict.fileofs = offset;
		dict.filelen = g_GameLumps.GameLumpSize( h );
		offset += dict.filelen;

		WriteData( file, &dict );
	}

	// write lumps..
	for( h = g_GameLumps.FirstGameLump(); h != g_GameLumps.InvalidGameLump(); h = g_GameLumps.NextGameLump( h ) )
	{
		int lumpsize = g_GameLumps.GameLumpSize(h);
		if ( g_bSwapOnWrite )
		{
			g_GameLumps.SwapGameLump( g_GameLumps.GetGameLumpId(h), g_GameLumps.GetGameLumpVersion(h), (byte*)g_GameLumps.GetGameLump(h), (byte*)g_GameLumps.GetGameLump(h), lumpsize );
		}
		SafeWrite( file, g_GameLumps.GetGameLump(h), lumpsize );
	}

	// align to doubleword
	AlignFilePosition( file, 4u );
}


//-----------------------------------------------------------------------------
// Adds the occluder lump...
//-----------------------------------------------------------------------------
static void AddOcclusionLump( dheader_t *dheader, FileHandle_t file )
{
	g_Lumps.size[LUMP_OCCLUSION] = 0;	// mark it written

	intp nOccluderCount = g_OccluderData.Count();
	intp nOccluderPolyDataCount = g_OccluderPolyData.Count();
	intp nOccluderVertexIndices = g_OccluderVertexIndices.Count();

	intp nLumpLength = nOccluderCount * sizeof(doccluderdata_t) +
		nOccluderPolyDataCount * sizeof(doccluderpolydata_t) +
		nOccluderVertexIndices * sizeof(int) +
		3 * sizeof(int);

	lump_t *lump = &dheader->lumps[LUMP_OCCLUSION];

	lump->fileofs = g_pFileSystem->Tell( file );
	lump->filelen = nLumpLength;
	lump->version = LUMP_OCCLUSION_VERSION;
	lump->uncompressedSize = 0;

	// Data is swapped in place, so the 'Count' variables aren't safe to use after they're written
	WriteData( file, FIELD_INTEGER, &nOccluderCount );
	WriteData( file, g_OccluderData.Base(), g_OccluderData.Count() );
	WriteData( file, FIELD_INTEGER, &nOccluderPolyDataCount );
	WriteData( file, g_OccluderPolyData.Base(), g_OccluderPolyData.Count() );
	WriteData( file, FIELD_INTEGER, &nOccluderVertexIndices );
	WriteData( file, FIELD_INTEGER, g_OccluderVertexIndices.Base(), g_OccluderVertexIndices.Count() );
}


//-----------------------------------------------------------------------------
// Loads the occluder lump...
//-----------------------------------------------------------------------------
static void UnserializeOcclusionLumpV2( CUtlBuffer &buf )
{
	int nCount = buf.GetInt();
	if ( nCount )
	{
		g_OccluderData.SetCount( nCount );
		buf.GetObjects( g_OccluderData.Base(), nCount );
	}

	nCount = buf.GetInt();
	if ( nCount )
	{
		g_OccluderPolyData.SetCount( nCount );
		buf.GetObjects( g_OccluderPolyData.Base(), nCount );
	}

	nCount = buf.GetInt();
	if ( nCount )
	{
		if ( g_bSwapOnLoad )
		{
			g_Swap.SwapBufferToTargetEndian( (int*)buf.PeekGet(), (const int*)buf.PeekGet(), nCount );
		}
		g_OccluderVertexIndices.SetCount( nCount );
		buf.Get( g_OccluderVertexIndices.Base(), nCount * sizeof(g_OccluderVertexIndices[0]) );
	}
}


static void LoadOcclusionLump( dheader_t *dheader )
{
	g_OccluderData.RemoveAll();
	g_OccluderPolyData.RemoveAll();
	g_OccluderVertexIndices.RemoveAll();

	int		length, ofs;

	g_Lumps.bLumpParsed[LUMP_OCCLUSION] = true;

	length = dheader->lumps[LUMP_OCCLUSION].filelen;
	ofs = dheader->lumps[LUMP_OCCLUSION].fileofs;
	
	CUtlBuffer buf( (byte *)dheader + ofs, length, CUtlBuffer::READ_ONLY );
	buf.ActivateByteSwapping( g_bSwapOnLoad );
	int version = dheader->lumps[LUMP_OCCLUSION].version;
	switch ( version )
	{
	case 2:
		UnserializeOcclusionLumpV2( buf );
		break;

	case 0:
		break;

	default:
		Error("Unknown occlusion lump version %d!\n", version);
		break;
	}
}


/*
===============
CompressVis

===============
*/
int CompressVis (byte *vis, byte *dest)
{
	byte *dest_p = dest;
//	visrow = (r_numvisleafs + 7)>>3;
	int visrow = (dvis->numclusters + 7)>>3;
	
	for (int j=0 ; j<visrow ; j++)
	{
		*dest_p++ = vis[j];
		if (vis[j])
			continue;

		// dimhotepus: Use byte instead of int to fix truncation warnings.
		byte rep = 1;
		for ( j++; j<visrow ; j++)
			if (vis[j] || rep == std::numeric_limits<byte>::max())
				break;
			else
				rep++;

		*dest_p++ = rep;
		j--;
	}
	
	return dest_p - dest;
}


/*
===================
DecompressVis
===================
*/
void DecompressVis (byte *in, byte *decompressed)
{
	intp	c;
	byte	*out;
	int		row;

//	row = (r_numvisleafs+7)>>3;	
	row = (dvis->numclusters+7)>>3;	
	out = decompressed;

	do
	{
		if (*in)
		{
			*out++ = *in++;
			continue;
		}
	
		c = in[1];
		if (!c)
			Error("Vis decompression: 0 repeat.\n");
		in += 2;
		if ((out - decompressed) + c > row)
		{
			c = row - (out - decompressed);
			Warning( "Vis decompression row %d overrun!\n", row );
		}
		while (c)
		{
			*out++ = 0;
			c--;
		}
	} while (out - decompressed < row);
}

//-----------------------------------------------------------------------------
//	Lump-specific swap functions
//-----------------------------------------------------------------------------
struct swapcollideheader_t
{
	DECLARE_BYTESWAP_DATADESC();
	int		size;
	int		vphysicsID;
	short	version;
	short	modelType;
};

struct swapcompactsurfaceheader_t : swapcollideheader_t
{
	DECLARE_BYTESWAP_DATADESC();
	int		surfaceSize;
	Vector	dragAxisAreas;
	int		axisMapSize;
};

struct swapmoppsurfaceheader_t : swapcollideheader_t
{
	DECLARE_BYTESWAP_DATADESC();
	int moppSize;
};

BEGIN_BYTESWAP_DATADESC( swapcollideheader_t )
	DEFINE_FIELD( size, FIELD_INTEGER ),
	DEFINE_FIELD( vphysicsID, FIELD_INTEGER ),
	DEFINE_FIELD( version, FIELD_SHORT ),
	DEFINE_FIELD( modelType, FIELD_SHORT ),
END_BYTESWAP_DATADESC()

BEGIN_BYTESWAP_DATADESC_( swapcompactsurfaceheader_t, swapcollideheader_t )
	DEFINE_FIELD( surfaceSize, FIELD_INTEGER ),
	DEFINE_FIELD( dragAxisAreas, FIELD_VECTOR ),
	DEFINE_FIELD( axisMapSize, FIELD_INTEGER ),
END_BYTESWAP_DATADESC()

BEGIN_BYTESWAP_DATADESC_( swapmoppsurfaceheader_t, swapcollideheader_t )
	DEFINE_FIELD( moppSize, FIELD_INTEGER ),
END_BYTESWAP_DATADESC()


static void SwapPhyscollideLump( byte *pDestBase, byte *pSrcBase, unsigned int &count )
{
	IPhysicsCollision *physcollision = NULL;
	CSysModule *pPhysicsModule = g_pFullFileSystem->LoadModule( "vphysics" DLL_EXT_STRING );
	if ( pPhysicsModule )
	{
		CreateInterfaceFnT<IPhysicsCollision> physicsFactory = Sys_GetFactory<IPhysicsCollision>( pPhysicsModule );
		if ( physicsFactory )
		{
			physcollision = physicsFactory( VPHYSICS_COLLISION_INTERFACE_VERSION, NULL );
		}
	}

	if ( !physcollision )
	{
		Warning("VPhysics missed '%s' interface. Can't swap the physcollide lump!\n",
			VPHYSICS_COLLISION_INTERFACE_VERSION );
		return;
	}

	// physics data is variable length.  The last physmodel is a NULL pointer
	// with modelIndex -1, dataSize -1
	dphysmodel_t *pPhysModel;
	byte *pSrc = pSrcBase;

	// first the src chunks have to be aligned properly
	// swap increases size, allocate enough expansion room
	byte *pSrcAlignedBase = (byte*)malloc( 2*count );
	byte *basePtr = pSrcAlignedBase;
	byte *pSrcAligned = pSrcAlignedBase;

	do
	{
		if ( g_bSwapOnLoad )
		{
			g_Swap.SwapFieldsToTargetEndian( (dphysmodel_t*)pSrcAligned, (dphysmodel_t*)pSrc );
		}
		else
		{
			Q_memcpy( pSrcAligned, pSrc, sizeof(dphysmodel_t) );
		}
		pPhysModel = (dphysmodel_t*)pSrcAligned;

		pSrc += sizeof(dphysmodel_t);
		pSrcAligned += sizeof(dphysmodel_t);

		if ( pPhysModel->dataSize > 0 )
		{		
			// Align the collide headers
			for ( int i = 0; i < pPhysModel->solidCount; ++i )
			{
				// Get data size
				int size;
				Q_memcpy( &size, pSrc, sizeof(int) );
				if ( g_bSwapOnLoad )
					size = SwapLong( size );

				// Fixup size
				int padBytes = 0;
				if ( size % 4 != 0 )
				{
					padBytes = ( 4 - size % 4 );
					count += padBytes;
					pPhysModel->dataSize += padBytes;
				}

				// Copy data and size into alligned buffer
				int newsize = size + padBytes;
				if ( g_bSwapOnLoad )
					newsize = SwapLong( newsize );

				Q_memcpy( pSrcAligned, &newsize, sizeof(int) );
				Q_memcpy( pSrcAligned + sizeof(int), pSrc + sizeof(int), size );
				pSrcAligned += size + padBytes + sizeof(int);
				pSrc += size + sizeof(int);
			}

			int padBytes = 0;
			int dataSize = pPhysModel->dataSize + pPhysModel->keydataSize;
			Q_memcpy( pSrcAligned, pSrc, pPhysModel->keydataSize );
			pSrc += pPhysModel->keydataSize;
			pSrcAligned += pPhysModel->keydataSize;
			if ( dataSize % 4 != 0 )
			{
				// Next chunk will be unaligned
				padBytes = ( 4 - dataSize % 4 );
				pPhysModel->keydataSize += padBytes;
				count += padBytes;
				Q_memset( pSrcAligned, 0, padBytes );
				pSrcAligned += padBytes;
			}
		}
	} while ( pPhysModel->dataSize > 0 );

	// Now the data can be swapped properly
	pSrcBase = pSrcAlignedBase;
	pSrc = pSrcBase;
	byte *pDest = pDestBase;

	do
	{
		// src headers are in native format
		pPhysModel = (dphysmodel_t*)pSrc;
		if ( g_bSwapOnWrite )
		{
			g_Swap.SwapFieldsToTargetEndian( (dphysmodel_t*)pDest, (dphysmodel_t*)pSrc );
		}
		else
		{
			Q_memcpy( pDest, pSrc, sizeof(dphysmodel_t) );
		}

		pSrc += sizeof(dphysmodel_t);
		pDest += sizeof(dphysmodel_t);

		pSrcBase = pSrc;
		pDestBase = pDest;

		if ( pPhysModel->dataSize > 0 )
		{		
			vcollide_t collide = {};
			int dataSize = pPhysModel->dataSize + pPhysModel->keydataSize;

			if ( g_bSwapOnWrite )
			{
				// Load the collide data
				physcollision->VCollideLoad( &collide, pPhysModel->solidCount, (const char *)pSrc, dataSize, false );
			}

			std::unique_ptr<int[]> offsets = std::make_unique<int[]>( pPhysModel->solidCount );

			// Swap the collision data headers
			for ( int i = 0; i < pPhysModel->solidCount; ++i )
			{
				int headerSize = 0;
				swapcollideheader_t *baseHdr = (swapcollideheader_t*)pSrc;
				short modelType = baseHdr->modelType;
				if ( g_bSwapOnLoad )
				{
					g_Swap.SwapBufferToTargetEndian( &modelType );
				}

				if ( modelType == 0 ) // COLLIDE_POLY
				{
					headerSize = sizeof(swapcompactsurfaceheader_t);
					swapcompactsurfaceheader_t swapHdr;
					Q_memcpy( &swapHdr, pSrc, headerSize );
					g_Swap.SwapFieldsToTargetEndian( &swapHdr, &swapHdr );
					Q_memcpy( pDest, &swapHdr, headerSize );
				}
				else if ( modelType == 1 ) // COLLIDE_MOPP
				{
					// The PC still unserializes these, but we don't support them 
					if ( g_bSwapOnWrite )
					{
						collide.solids[i] = NULL;
					}

					headerSize = sizeof(swapmoppsurfaceheader_t);
					swapmoppsurfaceheader_t swapHdr;
					Q_memcpy( &swapHdr, pSrc, headerSize );
					g_Swap.SwapFieldsToTargetEndian( &swapHdr, &swapHdr );
					Q_memcpy( pDest, &swapHdr, headerSize );

				}
				else
				{
					// Shouldn't happen
					Assert( 0 );
				}

				if ( g_bSwapOnLoad )
				{
					// src needs the native header data to load the vcollides
					Q_memcpy( pSrc, pDest, headerSize );
				}
				// HACK: Need either surfaceSize or moppSize - both sit at the same offset in the structure
				swapmoppsurfaceheader_t *hdr = (swapmoppsurfaceheader_t*)pSrc;
				pSrc += hdr->size + sizeof(int);
				pDest += hdr->size + sizeof(int);
				offsets[i] = hdr->size;
			}

			pSrc = pSrcBase;
			pDest = pDestBase;
			if ( g_bSwapOnLoad )
			{
				physcollision->VCollideLoad( &collide, pPhysModel->solidCount, (const char *)pSrc, dataSize, true );
			}

			// Write out the ledge tree data
			for ( int i = 0; i < pPhysModel->solidCount; ++i )
			{
				if ( collide.solids[i] )
				{
					// skip over the size member
					pSrc += sizeof(int);
					pDest += sizeof(int);
					size_t offset = physcollision->CollideWrite( (char*)pDest, collide.solids[i], g_bSwapOnWrite );
					pSrc += offset;
					pDest += offset;
				}
				else
				{
					pSrc += offsets[i] + sizeof(int);
					pDest += offsets[i] + sizeof(int);
				}
			}

			// copy the keyvalues data
			Q_memcpy( pDest, pSrc, pPhysModel->keydataSize );
			pDest += pPhysModel->keydataSize;
			pSrc += pPhysModel->keydataSize;

			// Free the memory
			physcollision->VCollideUnload( &collide );
		}

		// avoid infinite loop on badly formed file
		if ( (pSrc - basePtr) > static_cast<intp>(count) )
			break;

	} while ( pPhysModel->dataSize > 0 );

	free( pSrcAlignedBase );
}


// UNDONE: This code is not yet tested.
static void SwapPhysdispLump( byte *pDest, byte *pSrc, int count )
{
	// the format of this lump is one unsigned short dispCount, then dispCount unsigned shorts of sizes
	// followed by an array of variable length (each element is the length of the corresponding entry in the
	// previous table) byte-stream data structure of the displacement collision models
	// these byte-stream structs are endian-neutral because each element is byte-sized
	unsigned short dispCount = *(unsigned short*)pSrc;
	if ( g_bSwapOnLoad )
	{
		g_Swap.SwapBufferToTargetEndian( &dispCount );
	}
	g_Swap.SwapBufferToTargetEndian( (unsigned short*)pDest, (unsigned short*)pSrc, dispCount + 1 );

	const int nBytes = (dispCount + 1) * sizeof( unsigned short );
	pSrc += nBytes;
	pDest += nBytes;
	count -= nBytes;

	g_Swap.SwapBufferToTargetEndian( pDest, pSrc, count );
}


static void SwapVisibilityLump( byte *pDest, byte *pSrc, int count )
{
	int firstInt = *(int*)pSrc;
	if ( g_bSwapOnLoad )
	{
		g_Swap.SwapBufferToTargetEndian( &firstInt );
	}
	int intCt = firstInt * 2 + 1;
	const int hdrSize = intCt * sizeof(int);
	g_Swap.SwapBufferToTargetEndian( (int*)pDest, (int*)pSrc, intCt );
	g_Swap.SwapBufferToTargetEndian( pDest + hdrSize, pSrc + hdrSize, count - hdrSize  );
}

//=============================================================================
void Lumps_Init( void )
{
	memset( &g_Lumps, 0, sizeof(g_Lumps) );
}

int LumpVersion( const dheader_t *header, int lump )
{
	return header->lumps[lump].version;
}

bool HasLump( const dheader_t *header, int lump )
{
	return header->lumps[lump].filelen > 0;
}

void ValidateLump( const dheader_t *header, int lump, int length, int size, int forceVersion )
{
	if ( length % size )
	{
		Error( "ValidateLump: odd size %d for lump %d.\n", size, lump );
	}

	int version = header->lumps[lump].version;
	if ( forceVersion >= 0 && forceVersion != version )
	{
		Error( "ValidateLump: old version %d for lump %d in map!\n", version, lump );
	}
}

//-----------------------------------------------------------------------------
//	Add Lumps of integral types without datadescs
//-----------------------------------------------------------------------------
template< class T >
int CopyLumpInternal( dheader_t *header, int fieldType, int lump, T *dest, int forceVersion )
{
	g_Lumps.bLumpParsed[lump] = true;

	// Vectors are passed in as floats
	int fieldSize = ( fieldType == FIELD_VECTOR ) ? sizeof(Vector) : sizeof(T);
	unsigned int length = header->lumps[lump].filelen;
	unsigned int ofs = header->lumps[lump].fileofs;

	// count must be of the integral type
	unsigned int count = length / sizeof(T);
	
	ValidateLump( header, lump, length, fieldSize, forceVersion );

	if ( g_bSwapOnLoad )
	{
		switch( lump )
		{
		case LUMP_VISIBILITY:
			SwapVisibilityLump( (byte*)dest, ((byte*)header + ofs), count );
			break;
		
		case LUMP_PHYSCOLLIDE:
			// SwapPhyscollideLump may change size
			SwapPhyscollideLump( (byte*)dest, ((byte*)header + ofs), count );
			length = count;
			break;

		case LUMP_PHYSDISP:
			SwapPhysdispLump( (byte*)dest, ((byte*)header + ofs), count );
			break;

		default:
			g_Swap.SwapBufferToTargetEndian( dest, (T*)((byte*)header + ofs), count );
			break;
		}
	}
	else
	{
		memcpy( dest, (byte*)header + ofs, length );
	}

	// Return actual count of elements
	return length / fieldSize;
}

template< class T >
int CopyLump( dheader_t *header, int fieldType, int lump, T *dest, int forceVersion = -1 )
{
	return CopyLumpInternal( header, fieldType, lump, dest, forceVersion );
}

template< class T >
void CopyLump( dheader_t *header, int fieldType, int lump, CUtlVector<T> &dest, int forceVersion = -1 )
{
	Assert( fieldType != FIELD_VECTOR ); // TODO: Support this if necessary
	dest.SetSize( header->lumps[lump].filelen / sizeof(T) );
	CopyLumpInternal( header, fieldType, lump, dest.Base(), forceVersion );
}

template< class T >
void CopyOptionalLump( dheader_t* header, int fieldType, int lump, CUtlVector<T> &dest, int forceVersion = -1 )
{
	// not fatal if not present
	if ( !HasLump( header, lump ) )
		return;

	dest.SetSize( header->lumps[lump].filelen / sizeof(T) );
	CopyLumpInternal( header, fieldType, lump, dest.Base(), forceVersion );
}

template< class T >
int CopyVariableLump( dheader_t* header, int fieldType, int lump, void **dest, int forceVersion = -1 )
{
	int length = header->lumps[lump].filelen;
	*dest = malloc( length );

	return CopyLumpInternal<T>( header, fieldType, lump, static_cast<T*>(*dest), forceVersion );
}

//-----------------------------------------------------------------------------
//	Add Lumps of object types with datadescs
//-----------------------------------------------------------------------------
template< class T >
int CopyLumpInternal( dheader_t* header, int lump, T *dest, int forceVersion )
{
	g_Lumps.bLumpParsed[lump] = true;

	unsigned int length = header->lumps[lump].filelen;
	unsigned int ofs = header->lumps[lump].fileofs;
	unsigned int count = length / sizeof(T);
	
	ValidateLump( header, lump, length, sizeof(T), forceVersion );

	if ( g_bSwapOnLoad )
	{
		g_Swap.SwapFieldsToTargetEndian( dest, (T*)((byte*)header + ofs), count );
	}
	else
	{
		memcpy( dest, (byte*)header + ofs, length );
	}

	return count;
}

template< class T >
int CopyLump( dheader_t *header, int lump, T *dest, int forceVersion = -1 )
{
	return CopyLumpInternal( header, lump, dest, forceVersion );
}

template< class T >
void CopyLump( dheader_t *header, int lump, CUtlVector<T> &dest, int forceVersion = -1 )
{
	dest.SetSize( header->lumps[lump].filelen / sizeof(T) );
	CopyLumpInternal( header, lump, dest.Base(), forceVersion );
}

template< class T >
void CopyOptionalLump( dheader_t *header, int lump, CUtlVector<T> &dest, int forceVersion = -1 )
{
	// not fatal if not present
	if ( !HasLump( header, lump ) )
		return;

	dest.SetSize( header->lumps[lump].filelen / sizeof(T) );
	CopyLumpInternal( header, lump, dest.Base(), forceVersion );
}

template< class T >
int CopyVariableLump( dheader_t *header, int lump, void **dest, int forceVersion = -1 )
{
	int length = header->lumps[lump].filelen;
	*dest = malloc( length );

	return CopyLumpInternal<T>( header, lump, (T*)*dest, forceVersion );
}

//-----------------------------------------------------------------------------
//	Add/Write unknown lumps
//-----------------------------------------------------------------------------
void Lumps_Parse( dheader_t *header )
{
	for ( int i = 0; i < HEADER_LUMPS; i++ )
	{
		if ( !g_Lumps.bLumpParsed[i] && header->lumps[i].filelen )
		{
			g_Lumps.size[i] = CopyVariableLump<byte>( header, FIELD_CHARACTER, i, &g_Lumps.pLumps[i], -1 );
			Msg( "Reading unknown lump #%d (%d bytes)\n", i, g_Lumps.size[i] );
		}
	}
}

void Lumps_Write( dheader_t *header, FileHandle_t file )
{
	for ( int i = 0; i < HEADER_LUMPS; i++ )
	{
		if ( g_Lumps.size[i] )
		{
			Msg( "Writing unknown lump #%d (%d bytes)\n", i, g_Lumps.size[i] );
			AddLump( header, file, i, (byte*)g_Lumps.pLumps[i], g_Lumps.size[i] );
		}
		if ( g_Lumps.pLumps[i] )
		{
			free( g_Lumps.pLumps[i] );
			g_Lumps.pLumps[i] = NULL;
		}
	}
}

int LoadLeafs( dheader_t *header )
{
#if defined( BSP_USE_LESS_MEMORY )
	dleafs = (dleaf_t*)malloc( header->lumps[LUMP_LEAFS].filelen );
#endif
	int version = LumpVersion( header, LUMP_LEAFS );
	switch ( version )
	{
	case 0:
		{
			g_Lumps.bLumpParsed[LUMP_LEAFS] = true;
			int length = header->lumps[LUMP_LEAFS].filelen;
			int size = sizeof( dleaf_version_0_t );
			if ( length % size )
			{
				Error( "Odd size %d for LUMP_LEAFS!\n", size );
			}
			int count = length / size;

			void *pSrcBase = ( ( byte * )header + header->lumps[LUMP_LEAFS].fileofs );
			dleaf_version_0_t *pSrc = (dleaf_version_0_t *)pSrcBase;
			dleaf_t *pDst = dleafs;

			// version 0 predates HDR, build the LDR
			g_LeafAmbientLightingLDR.SetCount( count );
			g_LeafAmbientIndexLDR.SetCount( count );

			dleafambientlighting_t *pDstLeafAmbientLighting = &g_LeafAmbientLightingLDR[0];
			for ( int i = 0; i < count; i++ )
			{
				g_LeafAmbientIndexLDR[i].ambientSampleCount = 1;
				g_LeafAmbientIndexLDR[i].firstAmbientSample = i;
		
				if ( g_bSwapOnLoad )
				{
					g_Swap.SwapFieldsToTargetEndian( pSrc );
				}
				// pDst is a subset of pSrc;
				*pDst = *( ( dleaf_t * )( void * )pSrc );
				pDstLeafAmbientLighting->cube = pSrc->m_AmbientLighting;
				pDstLeafAmbientLighting->x = pDstLeafAmbientLighting->y = pDstLeafAmbientLighting->z = pDstLeafAmbientLighting->pad = 0;
				pDst++;
				pSrc++;
				pDstLeafAmbientLighting++;
			}
			return count;
		}

	case 1:
		return CopyLump( header, LUMP_LEAFS, dleafs );

	default:
		AssertMsg( 0, "Unknown LUMP_LEAFS version %d.", version );
		Error( "Unknown LUMP_LEAFS version %d.\n", version );
		return 0;
	}
}

void LoadLeafAmbientLighting( dheader_t *header, int numLeafs )
{
	if ( LumpVersion( header, LUMP_LEAFS ) == 0 )
	{
		// an older leaf version already built the LDR ambient lighting on load
		return;
	}

	// old BSP with ambient, or new BSP with no lighting, convert ambient light to new format or create dummy ambient
	if ( !HasLump( header,LUMP_LEAF_AMBIENT_INDEX ) )
	{
		// a bunch of legacy maps, have these lumps with garbage versions
		// expect them to be NOT the current version
		if ( HasLump(header, LUMP_LEAF_AMBIENT_LIGHTING) )
		{
			Assert( LumpVersion( header, LUMP_LEAF_AMBIENT_LIGHTING ) != LUMP_LEAF_AMBIENT_LIGHTING_VERSION );
		}
		if ( HasLump(header, LUMP_LEAF_AMBIENT_LIGHTING_HDR) )
		{
			Assert( LumpVersion( header, LUMP_LEAF_AMBIENT_LIGHTING_HDR ) != LUMP_LEAF_AMBIENT_LIGHTING_VERSION );
		}

		void *pSrcBase = ( ( byte * )header + header->lumps[LUMP_LEAF_AMBIENT_LIGHTING].fileofs );
		CompressedLightCube *pSrc = NULL;
		if ( HasLump( header, LUMP_LEAF_AMBIENT_LIGHTING ) )
		{
			pSrc = (CompressedLightCube*)pSrcBase;
		}
		g_LeafAmbientIndexLDR.SetCount( numLeafs );
		g_LeafAmbientLightingLDR.SetCount( numLeafs );

		void *pSrcBaseHDR = ( ( byte * )header + header->lumps[LUMP_LEAF_AMBIENT_LIGHTING_HDR].fileofs );
		CompressedLightCube *pSrcHDR = NULL;
		if ( HasLump( header, LUMP_LEAF_AMBIENT_LIGHTING_HDR ) )
		{
			pSrcHDR = (CompressedLightCube*)pSrcBaseHDR;
		}
		g_LeafAmbientIndexHDR.SetCount( numLeafs );
		g_LeafAmbientLightingHDR.SetCount( numLeafs );

		for ( int i = 0; i < numLeafs; i++ )
		{
			g_LeafAmbientIndexLDR[i].ambientSampleCount = 1;
			g_LeafAmbientIndexLDR[i].firstAmbientSample = i;
			g_LeafAmbientIndexHDR[i].ambientSampleCount = 1;
			g_LeafAmbientIndexHDR[i].firstAmbientSample = i;

			Q_memset( &g_LeafAmbientLightingLDR[i], 0, sizeof(g_LeafAmbientLightingLDR[i]) );
			Q_memset( &g_LeafAmbientLightingHDR[i], 0, sizeof(g_LeafAmbientLightingHDR[i]) );

			if ( pSrc )
			{
				if ( g_bSwapOnLoad )
				{
					g_Swap.SwapFieldsToTargetEndian( &pSrc[i] );
				}
				g_LeafAmbientLightingLDR[i].cube = pSrc[i];
			}
			if ( pSrcHDR )
			{
				if ( g_bSwapOnLoad )
				{
					g_Swap.SwapFieldsToTargetEndian( &pSrcHDR[i] );
				}
				g_LeafAmbientLightingHDR[i].cube = pSrcHDR[i];
			}
		}

		g_Lumps.bLumpParsed[LUMP_LEAF_AMBIENT_LIGHTING] = true;
		g_Lumps.bLumpParsed[LUMP_LEAF_AMBIENT_INDEX] = true;
		g_Lumps.bLumpParsed[LUMP_LEAF_AMBIENT_LIGHTING_HDR] = true;
		g_Lumps.bLumpParsed[LUMP_LEAF_AMBIENT_INDEX_HDR] = true;
	}
	else
	{
		CopyOptionalLump( header, LUMP_LEAF_AMBIENT_LIGHTING, g_LeafAmbientLightingLDR );
		CopyOptionalLump( header, LUMP_LEAF_AMBIENT_INDEX, g_LeafAmbientIndexLDR );
		CopyOptionalLump( header, LUMP_LEAF_AMBIENT_LIGHTING_HDR, g_LeafAmbientLightingHDR );
		CopyOptionalLump( header, LUMP_LEAF_AMBIENT_INDEX_HDR, g_LeafAmbientIndexHDR );
	}
}

void ValidateHeader( const char *filename, const dheader_t *pHeader )
{
	if ( pHeader->ident != IDBSPHEADER )
	{
		Error("%s is not a BSP (0x%x) file but 0x%x one", filename, IDBSPHEADER, pHeader->ident);
	}
	if ( pHeader->version < MINBSPVERSION || pHeader->version > BSPVERSION )
	{
		Error("%s is BSP version %d, expected %d..%d",
			filename, pHeader->version, MINBSPVERSION, BSPVERSION);
	}
}

//-----------------------------------------------------------------------------
//	Low level BSP opener for external parsing. Parses headers, but nothing else.
//	You must close the BSP, via CloseBSPFile().
//-----------------------------------------------------------------------------
dheader_t* OpenBSPFile( const char *filename )
{
	Lumps_Init();

	dheader_t *header = nullptr;

	// load the file header
	if (!LoadFile( filename, (void **)&header ))
	{
		Warning("Unable to load map '%s'.\n", filename);
		return nullptr;
	}

	if ( g_bSwapOnLoad )
	{
		g_Swap.ActivateByteSwapping( true );
		g_Swap.SwapFieldsToTargetEndian( header );
	}

	ValidateHeader( filename, header );

	g_MapRevision = header->mapRevision;

	return header;
}

//-----------------------------------------------------------------------------
//	CloseBSPFile
//-----------------------------------------------------------------------------
void CloseBSPFile( dheader_t *header )
{
	free( header );
}

//-----------------------------------------------------------------------------
//	LoadBSPFile
//-----------------------------------------------------------------------------
bool LoadBSPFile( const char *filename )
{
	dheader_t *header = OpenBSPFile( filename );
	if (!header) return false;

	RunCodeAtScopeExit(CloseBSPFile(header));

	nummodels = CopyLump( header, LUMP_MODELS, dmodels );
	numvertexes = CopyLump( header, LUMP_VERTEXES, dvertexes );
	numplanes = CopyLump( header, LUMP_PLANES, dplanes );
	numleafs = LoadLeafs(header);
	numnodes = CopyLump( header, LUMP_NODES, dnodes );
	CopyLump( header, LUMP_TEXINFO, texinfo );
	numtexdata = CopyLump( header, LUMP_TEXDATA, dtexdata );
    
	CopyLump( header, LUMP_DISPINFO, g_dispinfo );
    CopyLump( header, LUMP_DISP_VERTS, g_DispVerts );
	CopyLump( header, LUMP_DISP_TRIS, g_DispTris );
    CopyLump( header, FIELD_CHARACTER, LUMP_DISP_LIGHTMAP_SAMPLE_POSITIONS, g_DispLightmapSamplePositions );
	CopyLump( header, LUMP_FACE_MACRO_TEXTURE_INFO, g_FaceMacroTextureInfos );
	
	numfaces = CopyLump(header, LUMP_FACES, dfaces, LUMP_FACES_VERSION);
	if ( HasLump( header, LUMP_FACES_HDR ) )
		numfaces_hdr = CopyLump( header, LUMP_FACES_HDR, dfaces_hdr, LUMP_FACES_VERSION );
	else
		numfaces_hdr = 0;

	CopyOptionalLump( header, LUMP_FACEIDS, dfaceids );

	g_numprimitives = CopyLump( header, LUMP_PRIMITIVES, g_primitives );
	g_numprimverts = CopyLump( header, LUMP_PRIMVERTS, g_primverts );
	g_numprimindices = CopyLump( header,  FIELD_SHORT, LUMP_PRIMINDICES, g_primindices );
    numorigfaces = CopyLump( header,  LUMP_ORIGINALFACES, dorigfaces );   // original faces
	numleaffaces = CopyLump( header,  FIELD_SHORT, LUMP_LEAFFACES, dleaffaces );
	numleafbrushes = CopyLump( header,  FIELD_SHORT, LUMP_LEAFBRUSHES, dleafbrushes );
	numsurfedges = CopyLump( header,  FIELD_INTEGER, LUMP_SURFEDGES, dsurfedges );
	numedges = CopyLump( header,  LUMP_EDGES, dedges );
	numbrushes = CopyLump( header,  LUMP_BRUSHES, dbrushes );
	numbrushsides = CopyLump( header,  LUMP_BRUSHSIDES, dbrushsides );
	numareas = CopyLump( header,  LUMP_AREAS, dareas );
	numareaportals = CopyLump( header, LUMP_AREAPORTALS, dareaportals );

	visdatasize = CopyLump ( header, FIELD_CHARACTER, LUMP_VISIBILITY, dvisdata );
	CopyOptionalLump( header, FIELD_CHARACTER, LUMP_LIGHTING, dlightdataLDR, LUMP_LIGHTING_VERSION );
	CopyOptionalLump( header, FIELD_CHARACTER, LUMP_LIGHTING_HDR, dlightdataHDR, LUMP_LIGHTING_VERSION );

	LoadLeafAmbientLighting( header, numleafs );

	CopyLump( header, FIELD_CHARACTER, LUMP_ENTITIES, dentdata );
	numworldlightsLDR = CopyLump( header, LUMP_WORLDLIGHTS, dworldlightsLDR );
	numworldlightsHDR = CopyLump( header, LUMP_WORLDLIGHTS_HDR, dworldlightsHDR );
	
	numleafwaterdata = CopyLump( header, LUMP_LEAFWATERDATA, dleafwaterdata );
	g_PhysCollideSize = CopyVariableLump<byte>( header, FIELD_CHARACTER, LUMP_PHYSCOLLIDE, (void**)&g_pPhysCollide );
	g_PhysDispSize = CopyVariableLump<byte>( header, FIELD_CHARACTER, LUMP_PHYSDISP, (void**)&g_pPhysDisp );

	g_numvertnormals = CopyLump( header, FIELD_VECTOR, LUMP_VERTNORMALS, (float*)g_vertnormals );
	g_numvertnormalindices = CopyLump( header, FIELD_SHORT, LUMP_VERTNORMALINDICES, g_vertnormalindices );

	g_nClipPortalVerts = CopyLump( header, FIELD_VECTOR, LUMP_CLIPPORTALVERTS, (float*)g_ClipPortalVerts );
	g_nCubemapSamples = CopyLump( header, LUMP_CUBEMAPS, g_CubemapSamples );	

	CopyLump( header, FIELD_CHARACTER, LUMP_TEXDATA_STRING_DATA, g_TexDataStringData );
	CopyLump( header, FIELD_INTEGER, LUMP_TEXDATA_STRING_TABLE, g_TexDataStringTable );

	// It's assumed by other code that the data lump is filled with C strings. We need to make sure that 
	// the buffer as a whole ends with a '\0'.
	// dimhotepus: TF2 backport.
	if ( g_TexDataStringData.Count() > 0 && g_TexDataStringData.Tail() != '\0' )
		Error( "BSP file %s is corrupted", filename );
	
	g_nOverlayCount = CopyLump(header, LUMP_OVERLAYS, g_Overlays);
	g_nWaterOverlayCount = CopyLump( header, LUMP_WATEROVERLAYS, g_WaterOverlays );
	CopyLump( header, LUMP_OVERLAY_FADES, g_OverlayFades );
	
	dflagslump_t flags_lump;
	
	if ( HasLump( header, LUMP_MAP_FLAGS ) )
		CopyLump ( header, LUMP_MAP_FLAGS, &flags_lump );
	else
		memset( &flags_lump, 0, sizeof( flags_lump ) );			// default flags to 0

	g_LevelFlags = flags_lump.m_LevelFlags;

	LoadOcclusionLump(header);

	CopyLump( header, FIELD_SHORT, LUMP_LEAFMINDISTTOWATER, g_LeafMinDistToWater );

	// Load PAK file lump into appropriate data structure
	byte *pakbuffer = NULL;
	int paksize = CopyVariableLump<byte>( header, FIELD_CHARACTER, LUMP_PAKFILE, ( void ** )&pakbuffer );
	if ( paksize > 0 )
	{
		GetPakFile()->ActivateByteSwapping( IsX360() );
		GetPakFile()->ParseFromBuffer( pakbuffer, paksize );
	}
	else
	{
		GetPakFile()->Reset();
	}

	free( pakbuffer );

	g_GameLumps.ParseGameLump( header );

	// NOTE: Do NOT call CopyLump after Lumps_Parse() it parses all un-Copied lumps
	// parse any additional lumps
	Lumps_Parse(header);

	g_Swap.ActivateByteSwapping( false );
	return true;
}

//-----------------------------------------------------------------------------
// Reset any state.
//-----------------------------------------------------------------------------
void UnloadBSPFile()
{
	nummodels = 0;
	numvertexes = 0;
	numplanes = 0;

	numleafs = 0;
#if defined( BSP_USE_LESS_MEMORY )
	if ( dleafs )
	{ 
		free( dleafs );
		dleafs = NULL;
	}
#endif

	numnodes = 0;
	texinfo.Purge();
	numtexdata = 0;

	g_dispinfo.Purge();
	g_DispVerts.Purge();
	g_DispTris.Purge();

	g_DispLightmapSamplePositions.Purge();
	g_FaceMacroTextureInfos.Purge();

	numfaces = 0;
	numfaces_hdr = 0;

	dfaceids.Purge();

	g_numprimitives = 0;
	g_numprimverts = 0;
	g_numprimindices = 0;
	numorigfaces = 0;
	numleaffaces = 0;
	numleafbrushes = 0;
	numsurfedges = 0;
	numedges = 0;
	numbrushes = 0;
	numbrushsides = 0;
	numareas = 0;
	numareaportals = 0;

	visdatasize = 0;
	dlightdataLDR.Purge();
	dlightdataHDR.Purge();

	g_LeafAmbientLightingLDR.Purge();
	g_LeafAmbientLightingHDR.Purge();
	g_LeafAmbientIndexHDR.Purge();
	g_LeafAmbientIndexLDR.Purge();

	dentdata.Purge();
	numworldlightsLDR = 0;
	numworldlightsHDR = 0;

	numleafwaterdata = 0;

	if ( g_pPhysCollide )
	{
		free( g_pPhysCollide );
		g_pPhysCollide = NULL;
	}
	g_PhysCollideSize = 0;

	if ( g_pPhysDisp )
	{
		free( g_pPhysDisp );
		g_pPhysDisp = NULL;
	}
	g_PhysDispSize = 0;

	g_numvertnormals = 0;
	g_numvertnormalindices = 0;

	g_nClipPortalVerts = 0;
	g_nCubemapSamples = 0;	

	g_TexDataStringData.Purge();
	g_TexDataStringTable.Purge();

	g_nOverlayCount = 0;
	g_nWaterOverlayCount = 0;

	g_LevelFlags = 0;

	g_OccluderData.Purge();
	g_OccluderPolyData.Purge();
	g_OccluderVertexIndices.Purge();

	g_GameLumps.DestroyAllGameLumps();

	for ( int i = 0; i < HEADER_LUMPS; i++ )
	{
		free( g_Lumps.pLumps[i] );
		g_Lumps.pLumps[i] = NULL;
	}

	ReleasePakFileLumps();
}

//-----------------------------------------------------------------------------
//	LoadBSPFileFilesystemOnly
//-----------------------------------------------------------------------------
bool LoadBSPFile_FileSystemOnly( const char *filename )
{
	Lumps_Init();

	dheader_t *header = nullptr;
	//
	// load the file header
	//
	if (!LoadFile( filename, (void **)&header ))
	{
		Warning("Unable to load map '%s'.\n", filename);
		return false;
	}

	ValidateHeader( filename, header );

	// Load PAK file lump into appropriate data structure
	byte *pakbuffer = NULL;
	int paksize = CopyVariableLump<byte>( header, FIELD_CHARACTER, LUMP_PAKFILE, ( void ** )&pakbuffer, 1 );
	if ( paksize > 0 )
	{
		GetPakFile()->ParseFromBuffer( pakbuffer, paksize );
	}
	else
	{
		GetPakFile()->Reset();
	}

	free( pakbuffer );

	// everything has been copied out
	free( header );
	return true;
}

bool ExtractZipFileFromBSP( char *pBSPFileName, char *pZipFileName )
{
	Lumps_Init();
	
	dheader_t *header = nullptr;
	//
	// load the file header
	//
	if (!LoadFile( pBSPFileName, (void **)&header))
	{
		Warning("Unable to load map '%s'.\n", pBSPFileName);
		return false;
	}

	ValidateHeader( pBSPFileName, header );

	byte *pakbuffer = NULL;
	int paksize = CopyVariableLump<byte>( header, FIELD_CHARACTER, LUMP_PAKFILE, ( void ** )&pakbuffer );
	if ( paksize > 0 )
	{
		auto [fp, errc] = se::posix::posix_file_stream_factory::open( pZipFileName, "wb" );
		if ( errc )
		{
			fprintf( stderr, "Can't open ZIP '%s': %s.\n", pZipFileName, errc.message().c_str() );
			return false;
		}

		std::tie(std::ignore, errc) = fp.write( pakbuffer, 1, paksize );
		if ( errc )
		{
			fprintf( stderr, "Can't write %d bytes to ZIP '%s': %s.\n", paksize, pZipFileName, errc.message().c_str() );
			return false;
		}
	}
	else
	{
		fprintf( stderr, "'%s' zip file is zero length!\n", pBSPFileName );
	}

	return true;
}

/*
=============
LoadBSPFileTexinfo

Only loads the texinfo lump, so qdata can scan for textures
=============
*/
bool LoadBSPFileTexinfo( const char *filename )
{
	auto [f, errc] = se::posix::posix_file_stream_factory::open( filename, "rb" );
	if ( errc )
	{
		Warning( "Unable to open BSP '%s': %s.\n", filename, errc.message().c_str() );
		return false;
	}

	std::unique_ptr<dheader_t> header = std::make_unique<dheader_t>();
	dheader_t *header_ptr = header.get();

	if (!header_ptr)
	{
		return false;
	}

	std::tie(std::ignore, errc) = f.read( header_ptr, sizeof(dheader_t), sizeof(dheader_t), 1 );
	if ( errc )
	{
		Warning( "Unable to read BSP '%s' header: %s.\n", filename, errc.message().c_str() );
		return false;
	}

	ValidateHeader( filename, header_ptr );

	int length = header_ptr->lumps[LUMP_TEXINFO].filelen;
	int offset = header_ptr->lumps[LUMP_TEXINFO].fileofs;

	size_t nCount = length / sizeof(texinfo_t);

	texinfo.Purge();
	texinfo.AddMultipleToTail( nCount );

	std::tie(std::ignore, errc) = f.seek( offset, SEEK_SET );
	if ( errc )
	{
		Warning( "Unable to seek BSP '%s' to offset %d: %s.\n", filename, offset, errc.message().c_str() );
		return false;
	}

	size_t readCount;
	std::tie(readCount, errc) = f.read(texinfo.Base(), length, sizeof(texinfo_t), nCount);
	if ( errc )
	{
		Warning( "Unable to read BSP '%s' %zu texinfos. Read only %zu texinfos: %s.\n",
			filename, nCount, readCount, errc.message().c_str() );
		return false;
	}

	// everything has been copied out
	return true;
}

static void AddLumpInternal( dheader_t *header, FileHandle_t file, int lumpnum, void *data, intp len, int version )
{
	g_Lumps.size[lumpnum] = 0;	// mark it written

	lump_t *lump = &header->lumps[lumpnum];

	lump->fileofs = g_pFileSystem->Tell( file );
	lump->filelen = len;
	lump->version = version;
	lump->uncompressedSize = 0;

	SafeWrite( file, data, len );

	// pad out to the next dword
	AlignFilePosition( file, 4u );
}

template< class T >
static void SwapInPlace( T *pData, int count )
{
	if ( !pData )
		return;

	// use the datadesc to swap the fields in place
	g_Swap.SwapFieldsToTargetEndian<T>( (T*)pData, pData, count );
}

template< class T >
static void SwapInPlace( int fieldType, T *pData, int count )
{
	if ( !pData )
		return;

	// swap the data in place
	g_Swap.SwapBufferToTargetEndian<T>( (T*)pData, (T*)pData, count );
}

//-----------------------------------------------------------------------------
//	Add raw data chunk to file (not a lump)
//-----------------------------------------------------------------------------
template< class T >
static void WriteData( FileHandle_t file, int fieldType, T *pData, int count )
{
	if ( g_bSwapOnWrite )
	{
		SwapInPlace( fieldType, pData, count );
	}
	SafeWrite( file, pData, count * sizeof(T) );
}

template< class T >
static void WriteData( FileHandle_t file, T *pData, int count )
{
	if ( g_bSwapOnWrite )
	{
		SwapInPlace( pData, count );
	}
	SafeWrite( file, pData, count * sizeof(T) );
}

//-----------------------------------------------------------------------------
//	Add Lump of object types with datadescs
//-----------------------------------------------------------------------------
template< class T >
static void AddLump( dheader_t* header, FileHandle_t file, int lumpnum, T *pData, intp count, int version )
{
	AddLumpInternal( header, file, lumpnum, pData, count * sizeof(T), version );
}

template< class T >
static void AddLump( dheader_t* header, FileHandle_t file, int lumpnum, CUtlVector<T> &data, int version )
{
	AddLumpInternal( header, file, lumpnum, data.Base(), data.Count() * sizeof(T), version );
}

/*
=============
WriteBSPFile

Swaps the bsp file in place, so it should not be referenced again
=============
*/
void WriteBSPFile( const char *filename, char * )
{		
	if ( texinfo.Count() > MAX_MAP_TEXINFO )
	{
		Error( "Map has too many texinfos (has %zd, can have at most %d).\n", texinfo.Count(), MAX_MAP_TEXINFO );
		return;
	}

	dheader_t outHeader;
	dheader_t *header = &outHeader;
	memset( header, 0, sizeof( dheader_t ) );

	header->ident = IDBSPHEADER;
	header->version = BSPVERSION;
	header->mapRevision = g_MapRevision;

	FileHandle_t bspHanle = SafeOpenWrite( filename );

	RunCodeAtScopeExit(g_pFileSystem->Close(bspHanle));

	WriteData( bspHanle, header );	// overwritten later

	AddLump( header, bspHanle, LUMP_PLANES, dplanes, numplanes );
	AddLump( header, bspHanle, LUMP_LEAFS, dleafs, numleafs, LUMP_LEAFS_VERSION );
	AddLump( header, bspHanle, LUMP_LEAF_AMBIENT_LIGHTING, g_LeafAmbientLightingLDR, LUMP_LEAF_AMBIENT_LIGHTING_VERSION );
	AddLump( header, bspHanle, LUMP_LEAF_AMBIENT_INDEX, g_LeafAmbientIndexLDR );
	AddLump( header, bspHanle, LUMP_LEAF_AMBIENT_INDEX_HDR, g_LeafAmbientIndexHDR );
	AddLump( header, bspHanle, LUMP_LEAF_AMBIENT_LIGHTING_HDR, g_LeafAmbientLightingHDR, LUMP_LEAF_AMBIENT_LIGHTING_VERSION );

	AddLump( header, bspHanle, LUMP_VERTEXES, dvertexes, numvertexes );
	AddLump( header, bspHanle, LUMP_NODES, dnodes, numnodes );
	AddLump( header, bspHanle, LUMP_TEXINFO, texinfo );
	AddLump( header, bspHanle, LUMP_TEXDATA, dtexdata, numtexdata );    

    AddLump( header, bspHanle, LUMP_DISPINFO, g_dispinfo );
    AddLump( header, bspHanle, LUMP_DISP_VERTS, g_DispVerts );
	AddLump( header, bspHanle, LUMP_DISP_TRIS, g_DispTris );
    AddLump( header, bspHanle, LUMP_DISP_LIGHTMAP_SAMPLE_POSITIONS, g_DispLightmapSamplePositions );
	AddLump( header, bspHanle, LUMP_FACE_MACRO_TEXTURE_INFO, g_FaceMacroTextureInfos );
 
	AddLump( header, bspHanle, LUMP_PRIMITIVES, g_primitives, g_numprimitives );
	AddLump( header, bspHanle, LUMP_PRIMVERTS, g_primverts, g_numprimverts );
	AddLump( header, bspHanle, LUMP_PRIMINDICES, g_primindices, g_numprimindices );
    AddLump( header, bspHanle, LUMP_FACES, dfaces, numfaces, LUMP_FACES_VERSION );
    if (numfaces_hdr)
		AddLump( header, bspHanle, LUMP_FACES_HDR, dfaces_hdr, numfaces_hdr, LUMP_FACES_VERSION );
	AddLump ( header, bspHanle, LUMP_FACEIDS, dfaceids, numfaceids );

	AddLump( header, bspHanle, LUMP_ORIGINALFACES, dorigfaces, numorigfaces );     // original faces lump
	AddLump( header, bspHanle, LUMP_BRUSHES, dbrushes, numbrushes );
	AddLump( header, bspHanle, LUMP_BRUSHSIDES, dbrushsides, numbrushsides );
	AddLump( header, bspHanle, LUMP_LEAFFACES, dleaffaces, numleaffaces );
	AddLump( header, bspHanle, LUMP_LEAFBRUSHES, dleafbrushes, numleafbrushes );
	AddLump( header, bspHanle, LUMP_SURFEDGES, dsurfedges, numsurfedges );
	AddLump( header, bspHanle, LUMP_EDGES, dedges, numedges );
	AddLump( header, bspHanle, LUMP_MODELS, dmodels, nummodels );
	AddLump( header, bspHanle, LUMP_AREAS, dareas, numareas );
	AddLump( header, bspHanle, LUMP_AREAPORTALS, dareaportals, numareaportals );

	AddLump( header, bspHanle, LUMP_LIGHTING, dlightdataLDR, LUMP_LIGHTING_VERSION );
	AddLump( header, bspHanle, LUMP_LIGHTING_HDR, dlightdataHDR, LUMP_LIGHTING_VERSION );
	AddLump( header, bspHanle, LUMP_VISIBILITY, dvisdata, visdatasize );
	AddLump( header, bspHanle, LUMP_ENTITIES, dentdata );
	AddLump( header, bspHanle, LUMP_WORLDLIGHTS, dworldlightsLDR, numworldlightsLDR );
	AddLump( header, bspHanle, LUMP_WORLDLIGHTS_HDR, dworldlightsHDR, numworldlightsHDR );
	AddLump( header, bspHanle, LUMP_LEAFWATERDATA, dleafwaterdata, numleafwaterdata );

	AddOcclusionLump(header, bspHanle);

	dflagslump_t flags_lump;
	flags_lump.m_LevelFlags = g_LevelFlags;
	AddLump( header, bspHanle, LUMP_MAP_FLAGS, &flags_lump, 1 );

	// NOTE: This is just for debugging, so it is disabled in release maps
#if 0
	// add the vis portals to the BSP for visualization
	AddLump( header, bspHanle, LUMP_PORTALS, dportals, numportals );
	AddLump( header, bspHanle, LUMP_CLUSTERS, dclusters, numclusters );
	AddLump( header, bspHanle, LUMP_PORTALVERTS, dportalverts, numportalverts );
	AddLump( header, bspHanle, LUMP_CLUSTERPORTALS, dclusterportals, numclusterportals );
#endif

	AddLump( header, bspHanle, LUMP_CLIPPORTALVERTS, (float*)g_ClipPortalVerts, g_nClipPortalVerts * 3 );
	AddLump( header, bspHanle, LUMP_CUBEMAPS, g_CubemapSamples, g_nCubemapSamples );
	AddLump( header, bspHanle, LUMP_TEXDATA_STRING_DATA, g_TexDataStringData );
	AddLump( header, bspHanle, LUMP_TEXDATA_STRING_TABLE, g_TexDataStringTable );
	AddLump( header, bspHanle, LUMP_OVERLAYS, g_Overlays, g_nOverlayCount );
	AddLump( header, bspHanle, LUMP_WATEROVERLAYS, g_WaterOverlays, g_nWaterOverlayCount );
	AddLump( header, bspHanle, LUMP_OVERLAY_FADES, g_OverlayFades, g_nOverlayCount );

	if ( g_pPhysCollide )
	{
		AddLump( header, bspHanle, LUMP_PHYSCOLLIDE, g_pPhysCollide, g_PhysCollideSize );
	}

	if ( g_pPhysDisp )
	{
		AddLump ( header, bspHanle, LUMP_PHYSDISP, g_pPhysDisp, g_PhysDispSize );
	}

	AddLump( header, bspHanle, LUMP_VERTNORMALS, (float*)g_vertnormals, g_numvertnormals * 3 );
	AddLump( header, bspHanle, LUMP_VERTNORMALINDICES, g_vertnormalindices, g_numvertnormalindices );

	AddLump( header, bspHanle, LUMP_LEAFMINDISTTOWATER, g_LeafMinDistToWater, numleafs );

	AddGameLumps(header, bspHanle);

	// Write pakfile lump to disk
	WritePakFileLump(header, bspHanle);

	// NOTE: Do NOT call AddLump after Lumps_Write() it writes all un-Added lumps
	// write any additional lumps
	Lumps_Write(header, bspHanle);

	g_pFileSystem->Seek( bspHanle, 0, FILESYSTEM_SEEK_HEAD );
	WriteData( bspHanle, header );
}

// Generate the next clear lump filename for the bsp file
bool GenerateNextLumpFileName( const char *bspfilename, char *lumpfilename, int buffsize )
{
	for (int i = 0; i < MAX_LUMPFILES; i++)
	{
		GenerateLumpFileName( bspfilename, lumpfilename, buffsize, i );
	
		if ( !g_pFileSystem->FileExists( lumpfilename ) )
			return true;
	}

	return false;
}

bool WriteLumpToFile( dheader_t *header, char *filename, int lump )
{
	if ( !HasLump(header, lump) )
		return false;

	char lumppre[MAX_PATH];	
	if ( !GenerateNextLumpFileName( filename, lumppre, MAX_PATH ) )
	{
		Warning( "Failed to find valid lump filename for bsp %s.\n", filename );
		return false;
	}

	// Open the file
	FileHandle_t lumpfile = g_pFileSystem->Open(lumppre, "wb");
	if ( !lumpfile )
	{
		Error ("Error opening %s! (Check for write enable).\n", lumppre);
		return false;
	}

	RunCodeAtScopeExit(g_pFileSystem->Close(lumpfile));

	int ofs = header->lumps[lump].fileofs;
	int length = header->lumps[lump].filelen;

	// Write the header
	lumpfileheader_t lumpHeader = {};
	lumpHeader.lumpID = lump;
	lumpHeader.lumpVersion = LumpVersion(header, lump);
	lumpHeader.lumpLength = length;
	lumpHeader.mapRevision = LittleLong( g_MapRevision );
	lumpHeader.lumpOffset = sizeof(lumpfileheader_t);	// Lump starts after the header
	SafeWrite (lumpfile, &lumpHeader, sizeof(lumpfileheader_t));

	// Write the lump
	SafeWrite (lumpfile, (byte *)header + ofs, length);

	return true;
}

void	WriteLumpToFile( char *filename, int lump, int nLumpVersion, void *pBuffer, intp nBufLen )
{
	char lumppre[MAX_PATH];	
	if ( !GenerateNextLumpFileName( filename, lumppre, MAX_PATH ) )
	{
		Warning( "Failed to find valid lump filename for bsp %s.\n", filename );
		return;
	}

	// Open the file
	FileHandle_t lumpfile = g_pFileSystem->Open(lumppre, "wb");
	if ( !lumpfile )
	{
		Error ("Error opening %s! (Check for write enable).\n",filename);
		return;
	}

	RunCodeAtScopeExit(g_pFileSystem->Close(lumpfile));

	// Write the header
	lumpfileheader_t lumpHeader = {};
	lumpHeader.lumpID = lump;
	lumpHeader.lumpVersion = nLumpVersion;
	lumpHeader.lumpLength = nBufLen;
	lumpHeader.mapRevision = LittleLong( g_MapRevision );
	lumpHeader.lumpOffset = sizeof(lumpfileheader_t);	// Lump starts after the header
	SafeWrite( lumpfile, &lumpHeader, sizeof(lumpfileheader_t));

	// Write the lump
	SafeWrite( lumpfile, pBuffer, nBufLen );
}


//============================================================================
template<typename T, std::size_t size>
constexpr inline std::size_t ENTRYSIZE(T (&array)[size]) noexcept {
  return sizeof(array[0]);
}

static intp ArrayUsage( const char *szItem, intp items, std::size_t maxitems, std::size_t itemsize )
{
	float	percentage = maxitems ? items * 100.0f / maxitems : 0.0f;

    Msg("%-17.17s %8zd/%-8zu %8zu/%-8zu (%4.1f%%) ", 
		   szItem, items, maxitems, items * itemsize, maxitems * itemsize, percentage );
	if ( percentage > 80.0 )
		Msg( "VERY FULL!\n" );
	else if ( percentage > 95.0 )
		Msg( "SIZE DANGER!\n" );
	else if ( percentage > 99.9 )
		Msg( "SIZE OVERFLOW!!!\n" );
	else
		Msg( "\n" );
	return items * itemsize;
}

static intp GlobUsage( const char *szItem, intp itemstorage, std::size_t maxstorage )
{
	float	percentage = maxstorage ? itemstorage * 100.0f / maxstorage : 0.0f;
    Msg("%-17.17s     [variable]    %8zd/%-8zu (%4.1f%%) ", 
		   szItem, itemstorage, maxstorage, percentage );
	if ( percentage > 80.0 )
		Msg( "VERY FULL!\n" );
	else if ( percentage > 95.0 )
		Msg( "SIZE DANGER!\n" );
	else if ( percentage > 99.9 )
		Msg( "SIZE OVERFLOW!!!\n" );
	else
		Msg( "\n" );
	return itemstorage;
}

/*
=============
PrintBSPFileSizes

Dumps info about current file
=============
*/
void PrintBSPFileSizes (void)
{
	float	totalmemory = 0;

//	if (!num_entities)
//		ParseEntities ();

	Msg("\n");
	Msg( "%-17s %16s %16s %9s \n", "Object names", "Objects/Maxobjs", "Memory / Maxmem", "Fullness" );
	Msg( "%-17s %16s %16s %9s \n",  "------------", "---------------", "---------------", "--------" );

	totalmemory += ArrayUsage( "models",					nummodels,								std::size(dmodels),				ENTRYSIZE(dmodels) );
	totalmemory += ArrayUsage( "brushes",					numbrushes,								std::size(dbrushes),			ENTRYSIZE(dbrushes) );
	totalmemory += ArrayUsage( "brushsides",				numbrushsides,							std::size(dbrushsides),			ENTRYSIZE(dbrushsides) );
	totalmemory += ArrayUsage( "planes",					numplanes,								std::size(dplanes),				ENTRYSIZE(dplanes) );
	totalmemory += ArrayUsage( "vertexes",					numvertexes,							std::size(dvertexes),			ENTRYSIZE(dvertexes) );
	totalmemory += ArrayUsage( "nodes",						numnodes,								std::size(dnodes),				ENTRYSIZE(dnodes) );
	totalmemory += ArrayUsage( "texinfos",					texinfo.Count(),						MAX_MAP_TEXINFO,				sizeof(texinfo_t) );
	totalmemory += ArrayUsage( "texdata",					numtexdata,								std::size(dtexdata),			ENTRYSIZE(dtexdata) );
    
	totalmemory += ArrayUsage( "dispinfos",					g_dispinfo.Count(),						0,								sizeof( ddispinfo_t ) );
    totalmemory += ArrayUsage( "disp_verts",				g_DispVerts.Count(),					0,								sizeof( g_DispVerts[0] ) );
    totalmemory += ArrayUsage( "disp_tris",					g_DispTris.Count(),						0,								sizeof( g_DispTris[0] ) );
    totalmemory += ArrayUsage( "disp_lmsamples",			g_DispLightmapSamplePositions.Count(),	0,								sizeof( g_DispLightmapSamplePositions[0] ) );
	
	totalmemory += ArrayUsage( "faces",						numfaces,								std::size(dfaces),				ENTRYSIZE(dfaces) );
	totalmemory += ArrayUsage( "hdr faces",					numfaces_hdr,							std::size(dfaces_hdr),			ENTRYSIZE(dfaces_hdr) );
    totalmemory += ArrayUsage( "origfaces",					numorigfaces,							std::size(dorigfaces),			ENTRYSIZE(dorigfaces) );    // original faces
	totalmemory += ArrayUsage( "leaves",					numleafs,								std::size(dleafs),				ENTRYSIZE(dleafs) );
	totalmemory += ArrayUsage( "leaffaces",					numleaffaces,							std::size(dleaffaces),			ENTRYSIZE(dleaffaces) );
	totalmemory += ArrayUsage( "leafbrushes",				numleafbrushes,							std::size(dleafbrushes),		ENTRYSIZE(dleafbrushes) );
	totalmemory += ArrayUsage( "areas",						numareas,								std::size(dareas),				ENTRYSIZE(dareas) );
	totalmemory += ArrayUsage( "surfedges",					numsurfedges,							std::size(dsurfedges),			ENTRYSIZE(dsurfedges) );
	totalmemory += ArrayUsage( "edges",						numedges,								std::size(dedges),				ENTRYSIZE(dedges) );
	totalmemory += ArrayUsage( "LDR worldlights",			numworldlightsLDR,						std::size(dworldlightsLDR),		ENTRYSIZE(dworldlightsLDR) );
	totalmemory += ArrayUsage( "HDR worldlights",			numworldlightsHDR,						std::size(dworldlightsHDR),		ENTRYSIZE(dworldlightsHDR) );

	totalmemory += ArrayUsage( "leafwaterdata",				numleafwaterdata,						std::size(dleafwaterdata),		ENTRYSIZE(dleafwaterdata) );
	totalmemory += ArrayUsage( "waterstrips",				g_numprimitives,						std::size(g_primitives),		ENTRYSIZE(g_primitives) );
	totalmemory += ArrayUsage( "waterverts",				g_numprimverts,							std::size(g_primverts),			ENTRYSIZE(g_primverts) );
	totalmemory += ArrayUsage( "waterindices",				g_numprimindices,						std::size(g_primindices),		ENTRYSIZE(g_primindices) );
	totalmemory += ArrayUsage( "cubemapsamples",			g_nCubemapSamples,						std::size(g_CubemapSamples),	ENTRYSIZE(g_CubemapSamples) );
	totalmemory += ArrayUsage( "overlays",					g_nOverlayCount,						std::size(g_Overlays),			ENTRYSIZE(g_Overlays) );
	
	totalmemory += GlobUsage( "LDR lightdata",				dlightdataLDR.Count(),					0 );
	totalmemory += GlobUsage( "HDR lightdata",				dlightdataHDR.Count(),					0 );
	totalmemory += GlobUsage( "visdata",					visdatasize,							sizeof(dvisdata) );
	totalmemory += GlobUsage( "entdata",					dentdata.Count(),						384*1024 );	// goal is <384K

	totalmemory += ArrayUsage( "LDR ambient table",			g_LeafAmbientIndexLDR.Count(),			MAX_MAP_LEAFS,					sizeof( g_LeafAmbientIndexLDR[0] ) );
	totalmemory += ArrayUsage( "HDR ambient table",			g_LeafAmbientIndexHDR.Count(),			MAX_MAP_LEAFS,					sizeof( g_LeafAmbientIndexHDR[0] ) );
	totalmemory += ArrayUsage( "LDR leaf ambient lighting", g_LeafAmbientLightingLDR.Count(),		MAX_MAP_LEAFS,					sizeof( g_LeafAmbientLightingLDR[0] ) );
	totalmemory += ArrayUsage( "HDR leaf ambient lighting", g_LeafAmbientLightingHDR.Count(),		MAX_MAP_LEAFS,					sizeof( g_LeafAmbientLightingHDR[0] ) );

	totalmemory += ArrayUsage( "occluders",					g_OccluderData.Count(),					0,								sizeof( g_OccluderData[0] ) );
    totalmemory += ArrayUsage( "occluder polygons",			g_OccluderPolyData.Count(),				0,								sizeof( g_OccluderPolyData[0] ) );
    totalmemory += ArrayUsage( "occluder vert ind",			g_OccluderVertexIndices.Count(),		0,								sizeof( g_OccluderVertexIndices[0] ) );

	GameLumpHandle_t h = g_GameLumps.GetGameLumpHandle( GAMELUMP_DETAIL_PROPS );
	if (h != g_GameLumps.InvalidGameLump())
		totalmemory += GlobUsage( "detail props",	1,	g_GameLumps.GameLumpSize(h) );
	h = g_GameLumps.GetGameLumpHandle( GAMELUMP_DETAIL_PROP_LIGHTING );
	if (h != g_GameLumps.InvalidGameLump())
		totalmemory += GlobUsage( "dtl prp lght",	1,	g_GameLumps.GameLumpSize(h) );
	h = g_GameLumps.GetGameLumpHandle( GAMELUMP_DETAIL_PROP_LIGHTING_HDR );
	if (h != g_GameLumps.InvalidGameLump())
		totalmemory += GlobUsage( "HDR dtl prp lght",	1,	g_GameLumps.GameLumpSize(h) );
	h = g_GameLumps.GetGameLumpHandle( GAMELUMP_STATIC_PROPS );
	if (h != g_GameLumps.InvalidGameLump())
		totalmemory += GlobUsage( "static props",	1,	g_GameLumps.GameLumpSize(h) );

	totalmemory += GlobUsage( "pakfile",		GetPakFile()->EstimateSize(), 0 );
	// HACKHACK: Set physics limit at 4MB, in reality this is totally dynamic
	totalmemory += GlobUsage( "physics",		g_PhysCollideSize, 4*1024*1024 );
	totalmemory += GlobUsage( "physics terrain",		g_PhysDispSize, 1*1024*1024 );

	Msg( "\nLevel flags = %x\n", g_LevelFlags );

	Msg( "Memory used = %s\n", V_pretifymem( totalmemory, 2, true ) );

	int triangleCount = 0;

	for ( int i = 0; i < numfaces; i++ )
	{
		// face tris = numedges - 2
		triangleCount += dfaces[i].numedges - 2;
	}
	Msg("Total triangle count: %d\n", triangleCount );

	// UNDONE: 
	// areaportals, portals, texdata, clusters, worldlights, portalverts
}

/*
=============
PrintBSPPackDirectory

Dumps a list of files stored in the bsp pack.
=============
*/
void PrintBSPPackDirectory( void )
{
	GetPakFile()->PrintDirectory();	
}


//============================================

int			num_entities;
entity_t	entities[MAX_MAP_ENTITIES];

void StripTrailing (char *e)
{
	char	*s = e + strlen(e)-1;
	while (s >= e && *s <= 32)
	{
		*s = '\0';
		s--;
	}
}

/*
=================
ParseEpair
=================
*/
epair_t *ParseEpair (void)
{
	epair_t	*e;

	e = (epair_t*)malloc (sizeof(epair_t));
	memset (e, 0, sizeof(epair_t));
	
	if (strlen(token) >= MAX_KEY-1)
		Error ("ParseEpar: token key too long (%zu > %u)", strlen(token), MAX_KEY);
	e->key = copystring(token);

	GetToken (false);
	if (strlen(token) >= MAX_VALUE-1)
		Error ("ParseEpar: token value too long (%zu > %u)", strlen(token), MAX_VALUE);
	e->value = copystring(token);

	// strip trailing spaces
	StripTrailing (e->key);
	StripTrailing (e->value);

	return e;
}


/*
================
ParseEntity
================
*/
qboolean	ParseEntity (void)
{
	epair_t		*e;
	entity_t	*mapent;

	if (!GetToken (true))
		return false;

	if (Q_stricmp (token, "{") )
		Error ("ParseEntity: { not found");
	
	if (num_entities == MAX_MAP_ENTITIES)
		Error ("num_entities == MAX_MAP_ENTITIES (%d)", MAX_MAP_ENTITIES);

	mapent = &entities[num_entities];
	num_entities++;

	do
	{
		if (!GetToken (true))
			Error ("ParseEntity: EOF without closing brace");
		if (!Q_stricmp (token, "}") )
			break;
		e = ParseEpair ();
		e->next = mapent->epairs;
		mapent->epairs = e;
	} while (1);
	
	return true;
}

/*
================
ParseEntities

Parses the dentdata string into entities
================
*/
void ParseEntities (void)
{
	num_entities = 0;
	ParseFromMemory (dentdata.Base(), dentdata.Count());

	while (ParseEntity ())
	{
	}	
}


/*
================
UnparseEntities

Generates the dentdata string from all the entities
================
*/
void UnparseEntities (void)
{
	epair_t	*ep;
	// dimhotepus: +16 is from TF2 backport.
	char	line[2048 + 16];
	int		i;
	char	key[1020], value[1020];

	CUtlBuffer buffer( (intp)0, 0, CUtlBuffer::TEXT_BUFFER );
	buffer.EnsureCapacity( 256 * 1024 );
	
	for (i=0 ; i<num_entities ; i++)
	{
		ep = entities[i].epairs;
		if (!ep)
			continue;	// ent got removed
		
		buffer.PutString( "{\n" );
				
		for (ep = entities[i].epairs ; ep ; ep=ep->next)
		{
			V_strcpy_safe (key, ep->key);
			StripTrailing (key);
			V_strcpy_safe (value, ep->value);
			StripTrailing (value);
				
			V_sprintf_safe(line, "\"%s\" \"%s\"\n", key, value);
			buffer.PutString( line );
		}
		buffer.PutString("}\n");
	}
	int entdatasize = buffer.TellPut()+1;

	dentdata.SetSize( entdatasize );
	memcpy( dentdata.Base(), buffer.Base(), entdatasize-1 );
	dentdata[entdatasize-1] = 0;
}

void PrintEntity (entity_t *ent)
{
	epair_t	*ep;
	
	Msg ("------- entity %p -------\n", ent);
	for (ep=ent->epairs ; ep ; ep=ep->next)
	{
		Msg ("%s = %s\n", ep->key, ep->value);
	}

}

void SetKeyValue(entity_t *ent, const char *key, const char *value)
{
	epair_t	*ep;
	
	for (ep=ent->epairs ; ep ; ep=ep->next)
		if (!Q_stricmp (ep->key, key) )
		{
			free (ep->value);
			ep->value = copystring(value);
			return;
		}
	ep = (epair_t*)malloc (sizeof(*ep));
	ep->next = ent->epairs;
	ent->epairs = ep;
	ep->key = copystring(key);
	ep->value = copystring(value);
}

const char 	*ValueForKey (entity_t *ent, const char *key)
{
	for (epair_t *ep=ent->epairs ; ep ; ep=ep->next)
		if (!Q_stricmp (ep->key, key) )
			return ep->value;
	return "";
}

vec_t	FloatForKey (entity_t *ent, const char *key)
{
	const char *k = ValueForKey (ent, key);
	return strtof(k, nullptr);
}

vec_t	FloatForKeyWithDefault (entity_t *ent, const char *key, float default_value)
{
	for (epair_t *ep=ent->epairs ; ep ; ep=ep->next)
		if (!Q_stricmp (ep->key, key) )
			return strtof( ep->value, nullptr );
	return default_value;
}



int		IntForKey (entity_t *ent, const char *key)
{
	const char *k = ValueForKey (ent, key);
	return atol(k);
}

int		IntForKeyWithDefault(entity_t *ent, char *key, int nDefault )
{
	const char *k = ValueForKey (ent, key);
	if ( !k[0] )
		return nDefault;
	return atol(k);
}

// dimhotepus: Dispatch vec_t type.
using vector_type_t = std::conditional_t<
	std::is_same_v<float, vec_t>,
	float,
	std::conditional_t<
		std::is_same_v<double, vec_t>,
		double,
		void
	>
>;
static constexpr const char* GetVector2FormatSpecifier() {
	if constexpr (std::is_same_v<float, vector_type_t>)
		return "%f %f";

	if constexpr (std::is_same_v<double, vector_type_t>)
		return "%lf %lf";

	return "";
};
static constexpr const char* GetVector3FormatSpecifier() {
	if constexpr (std::is_same_v<float, vector_type_t>)
		return "%f %f %f";

	if constexpr (std::is_same_v<double, vector_type_t>)
		return "%lf %lf %lf";

	return "";
};
static constexpr const char *GetAnglesFormatSpecifier() { //-V524
	if constexpr (std::is_same_v<float, vector_type_t>)
		return "%f %f %f";

	if constexpr (std::is_same_v<double, vector_type_t>)
		return "%lf %lf %lf";

	return "";
};

void 	GetVectorForKey (entity_t *ent, const char *key, Vector& vec)
{
	const char *k = ValueForKey (ent, key);
	vector_type_t v1 = 0, v2 = 0, v3 = 0;
	constexpr auto format = GetVector3FormatSpecifier();
	if ( !Q_isempty( k ) && sscanf (k, format, &v1, &v2, &v3) != 3 )
	{
		Warning( "Key '%s' has value '%s' which is not a vector3.\n", key, k );
	}

	vec[0] = v1;
	vec[1] = v2;
	vec[2] = v3;
}

void 	GetVector2DForKey (entity_t *ent, const char *key, Vector2D& vec)
{
	const char *k = ValueForKey (ent, key);
	vector_type_t v1 = 0, v2 = 0;
	constexpr auto format = GetVector2FormatSpecifier();
	if ( sscanf (k, format, &v1, &v2) != 2 )
	{
		Warning( "key '%s' has value '%s' which is not a vector2.\n", key, k );
	}

	vec[0] = v1;
	vec[1] = v2;
}

void 	GetAnglesForKey (entity_t *ent, const char *key, QAngle& angle)
{
	const char *k = ValueForKey (ent, key);
	vector_type_t v1 = 0, v2 = 0, v3 = 0;
	constexpr auto format = GetAnglesFormatSpecifier();
	if ( sscanf (k, format, &v1, &v2, &v3) != 3 )
	{
		Warning( "key '%s' has value '%s' which is not a qangle.\n", key, k );
	}

	angle[0] = v1;
	angle[1] = v2;
	angle[2] = v3;
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void BuildFaceCalcWindingData( dface_t *pFace, int *points )
{
	for( int i = 0; i < pFace->numedges; i++ )
	{
		int eIndex = dsurfedges[pFace->firstedge+i];
		if( eIndex < 0 )
		{
			points[i] = dedges[-eIndex].v[1];
		}
		else
		{
			points[i] = dedges[eIndex].v[0];
		}
	}
}


void TriStripToTriList( 
	unsigned short const *pTriStripIndices,
	int nTriStripIndices,
	unsigned short **pTriListIndices,
	int *pnTriListIndices )
{
	int nMaxTriListIndices = (nTriStripIndices - 2) * 3;
	*pTriListIndices = new unsigned short[ nMaxTriListIndices ];
	*pnTriListIndices = 0;

	for( int i=0; i < nTriStripIndices - 2; i++ )
	{
		if( pTriStripIndices[i]   == pTriStripIndices[i+1] || 
			pTriStripIndices[i]   == pTriStripIndices[i+2] ||
			pTriStripIndices[i+1] == pTriStripIndices[i+2] )
		{
		}
		else
		{
			// Flip odd numbered tris..
			if( i & 1 )
			{
				(*pTriListIndices)[(*pnTriListIndices)++] = pTriStripIndices[i+2];
				(*pTriListIndices)[(*pnTriListIndices)++] = pTriStripIndices[i+1];
				(*pTriListIndices)[(*pnTriListIndices)++] = pTriStripIndices[i];
			}
			else
			{
				(*pTriListIndices)[(*pnTriListIndices)++] = pTriStripIndices[i];
				(*pTriListIndices)[(*pnTriListIndices)++] = pTriStripIndices[i+1];
				(*pTriListIndices)[(*pnTriListIndices)++] = pTriStripIndices[i+2];
			}
		}
	}
}


void CalcTextureCoordsAtPoints(
	float const texelsPerWorldUnits[2][4],
	int const subtractOffset[2],
	Vector const *pPoints,
	int const nPoints,
	Vector2D *pCoords )
{
	for( int i=0; i < nPoints; i++ )
	{
		for( int iCoord=0; iCoord < 2; iCoord++ )
		{
			float *pDestCoord = &pCoords[i][iCoord];

			*pDestCoord = 0;
			for( int iDot=0; iDot < 3; iDot++ )
				*pDestCoord += pPoints[i][iDot] * texelsPerWorldUnits[iCoord][iDot];

			*pDestCoord += texelsPerWorldUnits[iCoord][3];
			*pDestCoord -= subtractOffset[iCoord];
		}
	}
}


/*
================
CalcFaceExtents

Fills in s->texmins[] and s->texsize[]
================
*/
void CalcFaceExtents(dface_t *s, int lightmapTextureMinsInLuxels[2], int lightmapTextureSizeInLuxels[2])
{
	vec_t	    mins[2], maxs[2], val=0;
	int		    e=0;
	dvertex_t	*v=NULL;
	texinfo_t	*tex=NULL;
	
	mins[0] = mins[1] = 1e24f;
	maxs[0] = maxs[1] = -1e24f;

	tex = &texinfo[s->texinfo];
	
	for (int i=0 ; i<s->numedges ; i++)
	{
		e = dsurfedges[s->firstedge+i];
		if (e >= 0)
			v = dvertexes + dedges[e].v[0];
		else
			v = dvertexes + dedges[-e].v[1];
		
		for (int j=0 ; j<2 ; j++)
		{
			val = v->point[0] * tex->lightmapVecsLuxelsPerWorldUnits[j][0] + 
				  v->point[1] * tex->lightmapVecsLuxelsPerWorldUnits[j][1] + 
				  v->point[2] * tex->lightmapVecsLuxelsPerWorldUnits[j][2] + 
				  tex->lightmapVecsLuxelsPerWorldUnits[j][3];
			if (val < mins[j])
				mins[j] = val;
			if (val > maxs[j])
				maxs[j] = val;
		}
	}

	int nMaxLightmapDim = (s->dispinfo == -1) ? MAX_LIGHTMAP_DIM_WITHOUT_BORDER : MAX_DISP_LIGHTMAP_DIM_WITHOUT_BORDER;
	for (int i=0 ; i<2 ; i++)
	{	
		mins[i] = ( float )floor( mins[i] );
		maxs[i] = ( float )ceil( maxs[i] );

		lightmapTextureMinsInLuxels[i] = ( int )mins[i];
		lightmapTextureSizeInLuxels[i] = ( int )( maxs[i] - mins[i] );
		if( lightmapTextureSizeInLuxels[i] > nMaxLightmapDim + 1 )
		{
			Vector point = vec3_origin;
			for (int j=0 ; j<s->numedges ; j++)
			{
				e = dsurfedges[s->firstedge+j];
				v = (e<0)?dvertexes + dedges[-e].v[1] : dvertexes + dedges[e].v[0];
				point += v->point;
				Warning( "Bad surface extents point: %f %f %f\n", v->point.x, v->point.y, v->point.z );
			}
			point *= 1.0f/s->numedges;
			Error( "Bad surface extents - surface is too big to have a lightmap\n\tmaterial %s around point (%.1f %.1f %.1f)\n\t(dimension: %d, %d>%d)\n", 
				TexDataStringTable_GetString( dtexdata[texinfo[s->texinfo].texdata].nameStringTableID ), 
				point.x, point.y, point.z,
				( int )i,
				( int )lightmapTextureSizeInLuxels[i],
				( int )( nMaxLightmapDim + 1 )
				);
		}
	}
}


void UpdateAllFaceLightmapExtents()
{
	for( int i=0; i < numfaces; i++ )
	{
		dface_t *pFace = &dfaces[i];

		if ( texinfo[pFace->texinfo].flags & (SURF_SKY|SURF_NOLIGHT) )
			continue;		// non-lit texture

		CalcFaceExtents( pFace, pFace->m_LightmapTextureMinsInLuxels, pFace->m_LightmapTextureSizeInLuxels );
	}
}


//-----------------------------------------------------------------------------
//
// Helper class to iterate over leaves, used by tools
//
//-----------------------------------------------------------------------------

constexpr float TEST_EPSILON{0.03125f};


struct CToolBSPTree final : public ISpatialQuery
{
	// Returns the number of leaves
	int LeafCount() const;

	// Enumerates the leaves along a ray, box, etc.
	bool EnumerateLeavesAtPoint( Vector const& pt, ISpatialLeafEnumerator* pEnum, intp context );
	bool EnumerateLeavesInBox( Vector const& mins, Vector const& maxs, ISpatialLeafEnumerator* pEnum, intp context );
	bool EnumerateLeavesInSphere( Vector const& center, float radius, ISpatialLeafEnumerator* pEnum, intp context );
	bool EnumerateLeavesAlongRay( Ray_t const& ray, ISpatialLeafEnumerator* pEnum, intp context );
};


//-----------------------------------------------------------------------------
// Returns the number of leaves
//-----------------------------------------------------------------------------

int CToolBSPTree::LeafCount() const
{
	return numleafs;
}


//-----------------------------------------------------------------------------
// Enumerates the leaves at a point
//-----------------------------------------------------------------------------

bool CToolBSPTree::EnumerateLeavesAtPoint( Vector const& pt, 
									ISpatialLeafEnumerator* pEnum, intp context )
{
	int node = 0;
	while( node >= 0 )
	{
		dnode_t* pNode = &dnodes[node];
		dplane_t* pPlane = &dplanes[pNode->planenum];

		if (DotProduct( pPlane->normal, pt ) <= pPlane->dist)
		{
			node = pNode->children[1];
		}
		else
		{
			node = pNode->children[0];
		}
	}

	return pEnum->EnumerateLeaf( - node - 1, context );
}


//-----------------------------------------------------------------------------
// Enumerates the leaves in a box
//-----------------------------------------------------------------------------

static bool EnumerateLeavesInBox_R( int node, Vector const& mins, 
				Vector const& maxs, ISpatialLeafEnumerator* pEnum, intp context )
{
	Vector cornermin, cornermax;

	while( node >= 0 )
	{
		dnode_t* pNode = &dnodes[node];
		dplane_t* pPlane = &dplanes[pNode->planenum];

		// Arbitrary split plane here
		for (int i = 0; i < 3; ++i)
		{
			if (pPlane->normal[i] >= 0)
			{
				cornermin[i] = mins[i];
				cornermax[i] = maxs[i];
			}
			else
			{
				cornermin[i] = maxs[i];
				cornermax[i] = mins[i];
			}
		}

		if ( (DotProduct( pPlane->normal, cornermax ) - pPlane->dist) <= -TEST_EPSILON )
		{
			node = pNode->children[1];
		}
		else if ( (DotProduct( pPlane->normal, cornermin ) - pPlane->dist) >= TEST_EPSILON )
		{
			node = pNode->children[0];
		}
		else
		{
			if (!EnumerateLeavesInBox_R( pNode->children[0], mins, maxs, pEnum, context ))
			{
				return false;
			}

			return EnumerateLeavesInBox_R( pNode->children[1], mins, maxs, pEnum, context );
		}
	}

	return pEnum->EnumerateLeaf( - node - 1, context );
}

bool CToolBSPTree::EnumerateLeavesInBox( Vector const& mins, Vector const& maxs, 
									ISpatialLeafEnumerator* pEnum, intp context )
{
	return EnumerateLeavesInBox_R( 0, mins, maxs, pEnum, context );
}

//-----------------------------------------------------------------------------
// Enumerate leaves within a sphere
//-----------------------------------------------------------------------------

static bool EnumerateLeavesInSphere_R( int node, Vector const& origin, 
				float radius, ISpatialLeafEnumerator* pEnum, intp context )
{
	while( node >= 0 )
	{
		dnode_t* pNode = &dnodes[node];
		dplane_t* pPlane = &dplanes[pNode->planenum];

		if (DotProduct( pPlane->normal, origin ) + radius - pPlane->dist <= -TEST_EPSILON )
		{
			node = pNode->children[1];
		}
		else if (DotProduct( pPlane->normal, origin ) - radius - pPlane->dist >= TEST_EPSILON )
		{
			node = pNode->children[0];
		}
		else
		{
			if (!EnumerateLeavesInSphere_R( pNode->children[0], 
					origin, radius, pEnum, context ))
			{
				return false;
			}

			return EnumerateLeavesInSphere_R( pNode->children[1],
				origin, radius, pEnum, context );
		}
	}

	return pEnum->EnumerateLeaf( - node - 1, context );
}

bool CToolBSPTree::EnumerateLeavesInSphere( Vector const& center, float radius, ISpatialLeafEnumerator* pEnum, intp context )
{
	return EnumerateLeavesInSphere_R( 0, center, radius, pEnum, context );
}


//-----------------------------------------------------------------------------
// Enumerate leaves along a ray
//-----------------------------------------------------------------------------

static bool EnumerateLeavesAlongRay_R( int node, Ray_t const& ray, 
	Vector const& start, Vector const& end, ISpatialLeafEnumerator* pEnum, intp context )
{
	float front,back;

	while (node >= 0)
	{
		dnode_t* pNode = &dnodes[node];
		dplane_t* pPlane = &dplanes[pNode->planenum];

		if ( pPlane->type <= PLANE_Z )
		{
			front = start[pPlane->type] - pPlane->dist;
			back = end[pPlane->type] - pPlane->dist;
		}
		else
		{
			front = DotProduct(start, pPlane->normal) - pPlane->dist;
			back = DotProduct(end, pPlane->normal) - pPlane->dist;
		}

		if (front <= -TEST_EPSILON && back <= -TEST_EPSILON)
		{
			node = pNode->children[1];
		}
		else if (front >= TEST_EPSILON && back >= TEST_EPSILON)
		{
			node = pNode->children[0];
		}
		else
		{
			// test the front side first
			bool side = front < 0;

			// Compute intersection point based on the original ray
			float splitfrac;
			float denom = DotProduct( ray.m_Delta, pPlane->normal );
			if ( denom == 0.0f )
			{
				splitfrac = 1.0f;
			}
			else
			{
				splitfrac = (	pPlane->dist - DotProduct( ray.m_Start, pPlane->normal ) ) / denom;
				if (splitfrac < 0)
					splitfrac = 0;
				else if (splitfrac > 1)
					splitfrac = 1;
			}

			// Compute the split point
			Vector split;
			VectorMA( ray.m_Start, splitfrac, ray.m_Delta, split );

			bool r = EnumerateLeavesAlongRay_R (pNode->children[side], ray, start, split, pEnum, context );
			if (!r)
				return r;
			return EnumerateLeavesAlongRay_R (pNode->children[!side], ray, split, end, pEnum, context);
		}
	}

	return pEnum->EnumerateLeaf( - node - 1, context );
}

bool CToolBSPTree::EnumerateLeavesAlongRay( Ray_t const& ray, ISpatialLeafEnumerator* pEnum, intp context )
{
	if (!ray.m_IsSwept)
	{
		Vector mins, maxs;
		VectorAdd( ray.m_Start, ray.m_Extents, maxs );
		VectorSubtract( ray.m_Start, ray.m_Extents, mins );

		return EnumerateLeavesInBox_R( 0, mins, maxs, pEnum, context );
	}

	// FIXME: Extruded ray not implemented yet
	Assert( ray.m_IsRay );

	Vector end;
	VectorAdd( ray.m_Start, ray.m_Delta, end );
	return EnumerateLeavesAlongRay_R( 0, ray, ray.m_Start, end, pEnum, context );
}


//-----------------------------------------------------------------------------
// Singleton accessor
//-----------------------------------------------------------------------------

ISpatialQuery* ToolBSPTree()
{
	static CToolBSPTree s_ToolBSPTree;
	return &s_ToolBSPTree;
}



//-----------------------------------------------------------------------------
// Enumerates nodes in front to back order...
//-----------------------------------------------------------------------------

// FIXME: Do we want this in the IBSPTree interface?

static bool EnumerateNodesAlongRay_R( int node, Ray_t const& ray, float start, float end,
	IBSPNodeEnumerator* pEnum, intp context )
{
	float front, back;
	float startDotN, deltaDotN;

	while (node >= 0)
	{
		dnode_t* pNode = &dnodes[node];
		dplane_t* pPlane = &dplanes[pNode->planenum];

		if ( pPlane->type <= PLANE_Z )
		{
			startDotN = ray.m_Start[pPlane->type];
			deltaDotN = ray.m_Delta[pPlane->type];
		}
		else
		{
			startDotN = DotProduct( ray.m_Start, pPlane->normal );
			deltaDotN = DotProduct( ray.m_Delta, pPlane->normal );
		}

		front = startDotN + start * deltaDotN - pPlane->dist;
		back = startDotN + end * deltaDotN - pPlane->dist;

		if (front <= -TEST_EPSILON && back <= -TEST_EPSILON)
		{
			node = pNode->children[1];
		}
		else if (front >= TEST_EPSILON && back >= TEST_EPSILON)
		{
			node = pNode->children[0];
		}
		else
		{
			// test the front side first
			bool side = front < 0;

			// Compute intersection point based on the original ray
			float splitfrac;
			if ( deltaDotN == 0.0f )
			{
				splitfrac = 1.0f;
			}
			else
			{
				splitfrac = ( pPlane->dist - startDotN ) / deltaDotN;
				if (splitfrac < 0.0f)
					splitfrac = 0.0f;
				else if (splitfrac > 1.0f)
					splitfrac = 1.0f;
			}

			bool r = EnumerateNodesAlongRay_R (pNode->children[side], ray, start, splitfrac, pEnum, context );
			if (!r)
				return r;

			// Visit the node...
			if (!pEnum->EnumerateNode( node, ray, splitfrac, context ))
				return false;

			return EnumerateNodesAlongRay_R (pNode->children[!side], ray, splitfrac, end, pEnum, context);
		}
	}

	// Visit the leaf...
	return pEnum->EnumerateLeaf( - node - 1, ray, start, end, context );
}


bool EnumerateNodesAlongRay( Ray_t const& ray, IBSPNodeEnumerator* pEnum, intp context )
{
	Vector end;
	VectorAdd( ray.m_Start, ray.m_Delta, end );
	return EnumerateNodesAlongRay_R( 0, ray, 0.0f, 1.0f, pEnum, context );
}


//-----------------------------------------------------------------------------
// Helps us find all leaves associated with a particular cluster
//-----------------------------------------------------------------------------
CUtlVector<clusterlist_t> g_ClusterLeaves;

void BuildClusterTable( void )
{
	int	leafList[MAX_MAP_LEAFS];

	g_ClusterLeaves.SetCount( dvis->numclusters );
	for ( int i = 0; i < dvis->numclusters; i++ )
	{
		int leafCount = 0;
		for ( int j = 0; j < numleafs; j++ )
		{
			if ( dleafs[j].cluster == i )
			{
				leafList[ leafCount ] = j;
				leafCount++;
			}
		}

		g_ClusterLeaves[i].leafCount = leafCount;
		if ( leafCount )
		{
			g_ClusterLeaves[i].leafs.SetCount( leafCount );
			memcpy( g_ClusterLeaves[i].leafs.Base(), leafList, sizeof(int) * leafCount );
		}
	}
}

// There's a version of this in checksum_engine.cpp!!! Make sure that they match.
static bool CRC_MapFile(dheader_t *header, CRC32_t *crcvalue, const char *pszFileName)
{
	byte chunk[1024];
	lump_t *curLump;

	FileHandle_t fp = g_pFileSystem->Open( pszFileName, "rb" );
	if ( !fp )
		return false;

	RunCodeAtScopeExit(g_pFileSystem->Close(fp));

	// CRC across all lumps except for the Entities lump
	for ( int l = 0; l < HEADER_LUMPS; ++l )
	{
		if (l == LUMP_ENTITIES)
			continue;

		curLump = &header->lumps[l];
		unsigned int nSize = curLump->filelen;

		g_pFileSystem->Seek( fp, curLump->fileofs, FILESYSTEM_SEEK_HEAD );

		// Now read in 1K chunks
		while ( nSize > 0 )
		{
			int nBytesRead = 0;

			if ( nSize > 1024 )
				nBytesRead = g_pFileSystem->Read( chunk, 1024, fp );
			else
				nBytesRead = g_pFileSystem->Read( chunk, nSize, fp );

			// If any data was received, CRC it.
			if ( nBytesRead > 0 )
			{
				nSize -= nBytesRead;
				CRC32_ProcessBuffer( crcvalue, chunk, nBytesRead );
			}
			else
			{
				return false;
			}
		}
	}
	
	return true;
}


bool SetHDRMode( bool bHDR )
{
	bool bOldHDR = std::exchange(g_bHDR, bHDR);
	if ( bHDR ) //-V1051
	{
		pdlightdata = &dlightdataHDR;
		g_pLeafAmbientLighting = &g_LeafAmbientLightingHDR;
		g_pLeafAmbientIndex = &g_LeafAmbientIndexHDR;
		pNumworldlights = &numworldlightsHDR;
		dworldlights = dworldlightsHDR;
	}
	else
	{
		pdlightdata = &dlightdataLDR;
		g_pLeafAmbientLighting = &g_LeafAmbientLightingLDR;
		g_pLeafAmbientIndex = &g_LeafAmbientIndexLDR;
		pNumworldlights = &numworldlightsLDR;
		dworldlights = dworldlightsLDR;
	}
	
#ifdef VRAD
	extern void VRadDetailProps_SetHDRMode( bool bHDR );
	VRadDetailProps_SetHDRMode( bHDR );
#endif

	return bOldHDR;
}

bool SwapVHV( void *pDestBase, void *pSrcBase )
{
	byte *pDest = (byte*)pDestBase;
	byte *pSrc = (byte*)pSrcBase;

	HardwareVerts::FileHeader_t *pHdr = (HardwareVerts::FileHeader_t*)( g_bSwapOnLoad ? pDest : pSrc );
	g_Swap.SwapFieldsToTargetEndian<HardwareVerts::FileHeader_t>( (HardwareVerts::FileHeader_t*)pDest, (HardwareVerts::FileHeader_t*)pSrc );
	pSrc += sizeof(HardwareVerts::FileHeader_t);
	pDest += sizeof(HardwareVerts::FileHeader_t);

	// This swap is pretty format specific
	Assert( pHdr->m_nVersion == VHV_VERSION );
	if ( pHdr->m_nVersion != VHV_VERSION )
		return false;

	HardwareVerts::MeshHeader_t *pSrcMesh = (HardwareVerts::MeshHeader_t*)pSrc;
	HardwareVerts::MeshHeader_t *pDestMesh = (HardwareVerts::MeshHeader_t*)pDest;
	HardwareVerts::MeshHeader_t *pMesh = (HardwareVerts::MeshHeader_t*)( g_bSwapOnLoad ? pDest : pSrc );
	for ( int i = 0; i < pHdr->m_nMeshes; ++i, ++pMesh, ++pSrcMesh, ++pDestMesh )
	{
		g_Swap.SwapFieldsToTargetEndian( pDestMesh, pSrcMesh );

		pSrc = (byte*)pSrcBase + pMesh->m_nOffset;
		pDest = (byte*)pDestBase + pMesh->m_nOffset;

		// Swap as a buffer of integers 
		// (source is bgra for an Intel swap to argb. PowerPC won't swap, so we need argb source. 
		g_Swap.SwapBufferToTargetEndian<int>( (int*)pDest, (int*)pSrc, pMesh->m_nVertexes );
	}
	return true;
}

const char *ResolveStaticPropToModel( const char *pPropName )
{
	// resolve back to static prop
	int iProp = -1;

	// filename should be sp_???.vhv or sp_hdr_???.vhv
	if ( V_strnicmp( pPropName, "sp_", 3 ) )
	{
		return nullptr;
	}
	const char *pPropNumber = V_strrchr( pPropName, '_' );
	if ( pPropNumber )
	{
		sscanf( pPropNumber+1, "%d.vhv", &iProp );
	}
	else
	{
		return nullptr;
	}

	// look up the prop to get to the actual model
	if ( iProp < 0 || iProp >= g_StaticPropInstances.Count() )
	{
		// prop out of range
		return nullptr;
	}
	int iModel = g_StaticPropInstances[iProp];
	if ( iModel < 0 || iModel >= g_StaticPropNames.Count() )
	{
		// model out of range
		return nullptr;
	}

	return g_StaticPropNames[iModel].String();
}

//-----------------------------------------------------------------------------
// Iterate files in pak file, distribute to converters
// pak file will be ready for serialization upon completion
//-----------------------------------------------------------------------------
void ConvertPakFileContents( const char *pInFilename )
{
	IZip *newPakFile = IZip::CreateZip( NULL );

	CUtlBuffer sourceBuf;
	CUtlBuffer targetBuf;
	bool bConverted;
	CUtlVector< CUtlString > hdrFiles;

	int id = -1;
	int fileSize;
	while ( 1 )
	{
		char relativeName[MAX_PATH];
		id = GetNextFilename( GetPakFile(), id, relativeName, sizeof( relativeName ), fileSize );
		if ( id == -1)
			break;

		bConverted = false;
		sourceBuf.Purge();
		targetBuf.Purge();

		const char* pExtension = V_GetFileExtension( relativeName );
		const char* pExt = 0;

		bool bOK = ReadFileFromPak( GetPakFile(), relativeName, false, sourceBuf );
		if ( !bOK )
		{
			Warning( "Failed to load '%s' from lump pak for conversion or copy in '%s'.\n", relativeName, pInFilename );
			continue;
		}

		if ( pExtension && !V_stricmp( pExtension, "vtf" ) )
		{
			bOK = g_pVTFConvertFunc( relativeName, sourceBuf, targetBuf, g_pCompressFunc );
			if ( !bOK )
			{
				Warning( "Failed to convert '%s' in '%s'.\n", relativeName, pInFilename );
				continue;
			}
	
			bConverted = true;
			pExt = ".vtf";
		}
		else if ( pExtension && !V_stricmp( pExtension, "vhv" ) )
		{			
			CUtlBuffer tempBuffer;
			if ( g_pVHVFixupFunc )
			{
				// caller supplied a fixup
				const char *pModelName = ResolveStaticPropToModel( relativeName );
				if ( !pModelName )
				{
					Warning( "Static Prop '%s' failed to resolve actual model in '%s'.\n", relativeName, pInFilename );
					continue;
				}

				// output temp buffer may shrink, must use TellPut() to determine size
				bOK = g_pVHVFixupFunc( relativeName, pModelName, sourceBuf, tempBuffer );
				if ( !bOK )
				{
					Warning( "Failed to convert '%s' in '%s'.\n", relativeName, pInFilename );
					continue;
				}
			}
			else
			{
				// use the source buffer as-is
				tempBuffer.EnsureCapacity( sourceBuf.TellMaxPut() );
				tempBuffer.Put( sourceBuf.Base(), sourceBuf.TellMaxPut() );
			}

			// swap the VHV
			targetBuf.EnsureCapacity( tempBuffer.TellPut() );
			bOK = SwapVHV( targetBuf.Base(), tempBuffer.Base() );
			if ( !bOK )
			{
				Warning( "Failed to swap '%s' in '%s'.\n", relativeName, pInFilename );
				continue;
			}
			targetBuf.SeekPut( CUtlBuffer::SEEK_HEAD, tempBuffer.TellPut() );

			if ( g_pCompressFunc )
			{
				CUtlBuffer compressedBuffer;
				targetBuf.SeekGet( CUtlBuffer::SEEK_HEAD, sizeof( HardwareVerts::FileHeader_t ) );
				bool bCompressed = g_pCompressFunc( targetBuf, compressedBuffer );
				if ( bCompressed )
				{
					// copy all the header data off
					CUtlBuffer headerBuffer;
					headerBuffer.EnsureCapacity( sizeof( HardwareVerts::FileHeader_t ) );
					headerBuffer.Put( targetBuf.Base(), sizeof( HardwareVerts::FileHeader_t ) );

					// reform the target with the header and then the compressed data
					targetBuf.Clear();
					targetBuf.Put( headerBuffer.Base(), sizeof( HardwareVerts::FileHeader_t ) );
					targetBuf.Put( compressedBuffer.Base(), compressedBuffer.TellPut() );
				}

				targetBuf.SeekGet( CUtlBuffer::SEEK_HEAD, 0 );
			}

			bConverted = true;
			pExt = ".vhv";
		}

		if ( !bConverted )
		{
			// straight copy
			AddBufferToPak( newPakFile, relativeName, sourceBuf.Base(), sourceBuf.TellMaxPut(), false, IZip::eCompressionType_None );
		}
		else
		{
			// converted filename
			V_StripExtension( relativeName, relativeName );
			V_strcat_safe( relativeName, ".360" );
			V_strcat_safe( relativeName, pExt );
			AddBufferToPak( newPakFile, relativeName, targetBuf.Base(), targetBuf.TellMaxPut(), false, IZip::eCompressionType_None );
		}

		if ( V_stristr( relativeName, ".hdr" ) || V_stristr( relativeName, "_hdr" ) )
		{
			hdrFiles.AddToTail( relativeName );
		}

		DevMsg( "Created '%s' in lump pak in '%s'.\n", relativeName, pInFilename );
	}

	// strip ldr version of hdr files
	for ( int i=0; i<hdrFiles.Count(); i++ )
	{
		char ldrFileName[MAX_PATH];

		V_strcpy_safe(ldrFileName, hdrFiles[i].String());

		char *pHDRExtension = V_stristr( ldrFileName, ".hdr" );
		if ( !pHDRExtension )
		{
			pHDRExtension = V_stristr( ldrFileName, "_hdr" );
		}

		if ( pHDRExtension )
		{
			// strip .hdr or _hdr to get ldr filename
			memcpy( pHDRExtension, pHDRExtension+4, strlen( pHDRExtension+4 )+1 );

			DevMsg( "Stripping LDR: %s\n", ldrFileName );
			newPakFile->RemoveFileFromZip( ldrFileName );
		}
	}

	// discard old pak in favor of new pak
	IZip::ReleaseZip( s_pakFile );
	s_pakFile = newPakFile;
}

static void SetAlignedLumpPosition
(
	dheader_t *header,
	FileHandle_t file,
	int lumpnum,
	unsigned int alignment = LUMP_ALIGNMENT
)
{
	header->lumps[lumpnum].fileofs = AlignFilePosition( file, alignment );
}

template< class T >
int SwapLumpToDisk( dheader_t *header, FileHandle_t file, int fieldType, int lumpnum )
{
	if ( header->lumps[lumpnum].filelen == 0 )
		return 0;

	DevMsg( "Swapping %s\n", GetLumpName( lumpnum ) );

	// lump swap may expand, allocate enough expansion room
	void *pBuffer = malloc( 2*header->lumps[lumpnum].filelen );

	// CopyLumpInternal will handle the swap on load case
	unsigned int fieldSize = ( fieldType == FIELD_VECTOR ) ? sizeof(Vector) : sizeof(T);
	unsigned int count = CopyLumpInternal<T>( header, fieldType, lumpnum, (T*)pBuffer, header->lumps[lumpnum].version );
	header->lumps[lumpnum].filelen = count * fieldSize;

	if ( g_bSwapOnWrite )
	{
		// Swap the lump in place before writing
		switch( lumpnum )
		{
		case LUMP_VISIBILITY:
			SwapVisibilityLump( (byte*)pBuffer, (byte*)pBuffer, count );
			break;
		
		case LUMP_PHYSCOLLIDE:
			// SwapPhyscollideLump may change size
			SwapPhyscollideLump( (byte*)pBuffer, (byte*)pBuffer, count );
			header->lumps[lumpnum].filelen = count;
			break;

		case LUMP_PHYSDISP:
			SwapPhysdispLump( (byte*)pBuffer, (byte*)pBuffer, count );
			break;

		default:
			g_Swap.SwapBufferToTargetEndian( (T*)pBuffer, (T*)pBuffer, header->lumps[lumpnum].filelen / sizeof(T) );
			break;
		}
	}

	SetAlignedLumpPosition( header, file, lumpnum );
	SafeWrite( file, pBuffer, header->lumps[lumpnum].filelen );

	free( pBuffer );

	return header->lumps[lumpnum].filelen;
}

template< class T >
int SwapLumpToDisk( dheader_t *dheader, FileHandle_t file, int lumpnum )
{
	if ( dheader->lumps[lumpnum].filelen == 0 || g_Lumps.bLumpParsed[lumpnum] )
		return 0;

	DevMsg( "Swapping %s\n", GetLumpName( lumpnum ) );

	// lump swap may expand, allocate enough room
	void *pBuffer = malloc( 2*dheader->lumps[lumpnum].filelen );

	// CopyLumpInternal will handle the swap on load case
	int count = CopyLumpInternal<T>( dheader, lumpnum, (T*)pBuffer, dheader->lumps[lumpnum].version );
	dheader->lumps[lumpnum].filelen = count * sizeof(T);

	if ( g_bSwapOnWrite )
	{
		// Swap the lump in place before writing
		g_Swap.SwapFieldsToTargetEndian( (T*)pBuffer, (T*)pBuffer, count );
	}

	SetAlignedLumpPosition(dheader, file, lumpnum);
	SafeWrite( file, pBuffer, dheader->lumps[lumpnum].filelen );
	free( pBuffer );

	return dheader->lumps[lumpnum].filelen;
}

void SwapLeafAmbientLightingLumpToDisk(dheader_t *header, FileHandle_t file)
{
	if ( HasLump( header, LUMP_LEAF_AMBIENT_INDEX ) || HasLump( header, LUMP_LEAF_AMBIENT_INDEX_HDR ) )
	{
		// current version, swap in place
		if ( HasLump( header, LUMP_LEAF_AMBIENT_INDEX_HDR ) )
		{
			// write HDR
			SwapLumpToDisk< dleafambientlighting_t >( header, file, LUMP_LEAF_AMBIENT_LIGHTING_HDR );
			SwapLumpToDisk< dleafambientindex_t >( header, file, LUMP_LEAF_AMBIENT_INDEX_HDR );

			// cull LDR			
			header->lumps[LUMP_LEAF_AMBIENT_LIGHTING].filelen = 0;
			header->lumps[LUMP_LEAF_AMBIENT_INDEX].filelen = 0;
		}
		else
		{
			// no HDR, keep LDR version
			SwapLumpToDisk< dleafambientlighting_t >( header, file, LUMP_LEAF_AMBIENT_LIGHTING );
			SwapLumpToDisk< dleafambientindex_t >( header, file, LUMP_LEAF_AMBIENT_INDEX );
		}
	}
	else
	{
		// older ambient lighting version (before index)
		// load older ambient lighting into memory and build ambient/index
		// an older leaf version would have already built the new LDR leaf ambient/index
		int numLeafs = header->lumps[LUMP_LEAFS].filelen / sizeof( dleaf_t );
		LoadLeafAmbientLighting( header, numLeafs );

		if ( HasLump( header, LUMP_LEAF_AMBIENT_LIGHTING_HDR ) )
		{
			DevMsg( "Swapping %s\n", GetLumpName( LUMP_LEAF_AMBIENT_LIGHTING_HDR ) );
			DevMsg( "Swapping %s\n", GetLumpName( LUMP_LEAF_AMBIENT_INDEX_HDR ) );

			// write HDR
			if ( g_bSwapOnWrite )
			{
				g_Swap.SwapFieldsToTargetEndian( g_LeafAmbientLightingHDR.Base(), g_LeafAmbientLightingHDR.Count() );
				g_Swap.SwapFieldsToTargetEndian( g_LeafAmbientIndexHDR.Base(), g_LeafAmbientIndexHDR.Count() );
			}

			SetAlignedLumpPosition( header, file, LUMP_LEAF_AMBIENT_LIGHTING_HDR );
			header->lumps[LUMP_LEAF_AMBIENT_LIGHTING_HDR].version = LUMP_LEAF_AMBIENT_LIGHTING_VERSION;
			header->lumps[LUMP_LEAF_AMBIENT_LIGHTING_HDR].filelen = g_LeafAmbientLightingHDR.Count() * sizeof( dleafambientlighting_t );
			SafeWrite( file, g_LeafAmbientLightingHDR.Base(), header->lumps[LUMP_LEAF_AMBIENT_LIGHTING_HDR].filelen );

			SetAlignedLumpPosition( header, file, LUMP_LEAF_AMBIENT_INDEX_HDR );
			header->lumps[LUMP_LEAF_AMBIENT_INDEX_HDR].filelen = g_LeafAmbientIndexHDR.Count() * sizeof( dleafambientindex_t );
			SafeWrite( file, g_LeafAmbientIndexHDR.Base(), header->lumps[LUMP_LEAF_AMBIENT_INDEX_HDR].filelen );

			// mark as processed
			g_Lumps.bLumpParsed[LUMP_LEAF_AMBIENT_LIGHTING_HDR] = true;
			g_Lumps.bLumpParsed[LUMP_LEAF_AMBIENT_INDEX_HDR] = true;

			// cull LDR
			header->lumps[LUMP_LEAF_AMBIENT_LIGHTING].filelen = 0;
			header->lumps[LUMP_LEAF_AMBIENT_INDEX].filelen = 0;
		}
		else
		{
			// no HDR, keep LDR version
			DevMsg( "Swapping %s\n", GetLumpName( LUMP_LEAF_AMBIENT_LIGHTING ) );
			DevMsg( "Swapping %s\n", GetLumpName( LUMP_LEAF_AMBIENT_INDEX ) );

			if ( g_bSwapOnWrite )
			{
				g_Swap.SwapFieldsToTargetEndian( g_LeafAmbientLightingLDR.Base(), g_LeafAmbientLightingLDR.Count() );
				g_Swap.SwapFieldsToTargetEndian( g_LeafAmbientIndexLDR.Base(), g_LeafAmbientIndexLDR.Count() );
			}

			SetAlignedLumpPosition( header, file, LUMP_LEAF_AMBIENT_LIGHTING );
			header->lumps[LUMP_LEAF_AMBIENT_LIGHTING].version = LUMP_LEAF_AMBIENT_LIGHTING_VERSION;
			header->lumps[LUMP_LEAF_AMBIENT_LIGHTING].filelen = g_LeafAmbientLightingLDR.Count() * sizeof( dleafambientlighting_t );
			SafeWrite( file, g_LeafAmbientLightingLDR.Base(), header->lumps[LUMP_LEAF_AMBIENT_LIGHTING].filelen );

			SetAlignedLumpPosition( header, file, LUMP_LEAF_AMBIENT_INDEX );
			header->lumps[LUMP_LEAF_AMBIENT_INDEX].filelen = g_LeafAmbientIndexLDR.Count() * sizeof( dleafambientindex_t );
			SafeWrite( file, g_LeafAmbientIndexLDR.Base(), header->lumps[LUMP_LEAF_AMBIENT_INDEX].filelen );

			// mark as processed
			g_Lumps.bLumpParsed[LUMP_LEAF_AMBIENT_LIGHTING] = true;
			g_Lumps.bLumpParsed[LUMP_LEAF_AMBIENT_INDEX] = true;
		}

		g_LeafAmbientLightingLDR.Purge();
		g_LeafAmbientIndexLDR.Purge();
		g_LeafAmbientLightingHDR.Purge();
		g_LeafAmbientIndexHDR.Purge();
	}
}

void SwapLeafLumpToDisk( dheader_t *header, FileHandle_t file )
{
	DevMsg( "Swapping %s\n", GetLumpName( LUMP_LEAFS ) );

	// load the leafs
	int count = LoadLeafs(header);
	if ( g_bSwapOnWrite )
	{
		g_Swap.SwapFieldsToTargetEndian( dleafs, count );
	}

	bool bOldLeafVersion = ( LumpVersion( header, LUMP_LEAFS ) == 0 );
	if ( bOldLeafVersion )
	{
		// version has been converted in the load process
		// not updating the version ye, SwapLeafAmbientLightingLumpToDisk() can detect
		header->lumps[LUMP_LEAFS].filelen = count * sizeof( dleaf_t );
	}

	SetAlignedLumpPosition( header, file, LUMP_LEAFS );
	SafeWrite( file, dleafs, header->lumps[LUMP_LEAFS].filelen );

	SwapLeafAmbientLightingLumpToDisk(header, file);

	if ( bOldLeafVersion )
	{
		// version has been converted in the load process
		// can now safely change
		header->lumps[LUMP_LEAFS].version = 1;
	}

#if defined( BSP_USE_LESS_MEMORY )
	if ( dleafs )
	{
		free( dleafs );
		dleafs = NULL;
	}
#endif
}

void SwapOcclusionLumpToDisk( dheader_t *header, FileHandle_t file )
{
	DevMsg( "Swapping %s\n", GetLumpName( LUMP_OCCLUSION ) );

	LoadOcclusionLump(header);
	SetAlignedLumpPosition( header, file, LUMP_OCCLUSION );
	AddOcclusionLump(header, file);
}

void SwapPakfileLumpToDisk( dheader_t *header, FileHandle_t file, const char *pInFilename )
{
	DevMsg( "Swapping %s\n", GetLumpName( LUMP_PAKFILE ) );

	byte *pakbuffer = NULL;
	int paksize = CopyVariableLump<byte>( header, FIELD_CHARACTER, LUMP_PAKFILE, ( void ** )&pakbuffer );
	if ( paksize > 0 )
	{
		GetPakFile()->ActivateByteSwapping( IsX360() );
		GetPakFile()->ParseFromBuffer( pakbuffer, paksize );

		ConvertPakFileContents( pInFilename );
	}
	free( pakbuffer );

	SetAlignedLumpPosition( header, file, LUMP_PAKFILE, XBOX_DVD_SECTORSIZE );
	WritePakFileLump(header, file);

	ReleasePakFileLumps();
}

void SwapGameLumpsToDisk( dheader_t *header, FileHandle_t file )
{
	DevMsg( "Swapping %s\n", GetLumpName( LUMP_GAME_LUMP ) );

	g_GameLumps.ParseGameLump( header );
	SetAlignedLumpPosition( header, file, LUMP_GAME_LUMP );
	AddGameLumps(header, file);
}

//-----------------------------------------------------------------------------
// Generate a table of all static props, used for resolving static prop lighting
// files back to their actual mdl.
//-----------------------------------------------------------------------------
void BuildStaticPropNameTable(dheader_t *header)
{
	g_StaticPropNames.Purge();
	g_StaticPropInstances.Purge();

	g_GameLumps.ParseGameLump( header );

	GameLumpHandle_t hGameLump = g_GameLumps.GetGameLumpHandle( GAMELUMP_STATIC_PROPS );
	if ( hGameLump != g_GameLumps.InvalidGameLump() )
	{
		unsigned short nVersion = g_GameLumps.GetGameLumpVersion( hGameLump );
		if ( nVersion < 4 )
		{
			// old unsupported version
			return;
		}

		if ( nVersion != 4 && nVersion != 5 && nVersion != 6 )
		{
			Error( "Unknown Static Prop Lump version %hu!\n", nVersion );
		}

		byte *pGameLumpData = (byte *)g_GameLumps.GetGameLump( hGameLump );
		if ( pGameLumpData && g_GameLumps.GameLumpSize( hGameLump ) )
		{
			// get the model dictionary
			int count = ((int *)pGameLumpData)[0];
			pGameLumpData += sizeof( int );
			StaticPropDictLump_t *pStaticPropDictLump = (StaticPropDictLump_t *)pGameLumpData;
			for ( int i = 0; i < count; i++ )
			{
				g_StaticPropNames.AddToTail( pStaticPropDictLump[i].m_Name );
			}
			pGameLumpData += count * sizeof( StaticPropDictLump_t );

			// skip the leaf list
			count = ((int *)pGameLumpData)[0];
			pGameLumpData += sizeof( int );
			pGameLumpData += count * sizeof( StaticPropLeafLump_t );

			// get the instances
			count = ((int *)pGameLumpData)[0];
			pGameLumpData += sizeof( int );
			for ( int i = 0; i < count; i++ )
			{
				int propType;
				if ( nVersion == 4 )
				{
					propType = ((StaticPropLumpV4_t *)pGameLumpData)->m_PropType;
					pGameLumpData += sizeof( StaticPropLumpV4_t );
				}
				else if ( nVersion == 5 )
				{
					propType = ((StaticPropLumpV5_t *)pGameLumpData)->m_PropType;
					pGameLumpData += sizeof( StaticPropLumpV5_t );
				}
				else
				{
					propType = ((StaticPropLump_t *)pGameLumpData)->m_PropType;
					pGameLumpData += sizeof( StaticPropLump_t );
				}
				g_StaticPropInstances.AddToTail( propType );
			}
		}
	}

	g_GameLumps.DestroyAllGameLumps();
}

intp AlignBuffer( CUtlBuffer &buffer, int alignment )
{
	intp newPosition = AlignValue( buffer.TellPut(), alignment );
	intp padLength = newPosition - buffer.TellPut();
	for ( intp i = 0; i<padLength; i++ )
	{
		buffer.PutChar( '\0' );
	}
	return buffer.TellPut();
}

struct SortedLump_t
{
	int		lumpNum;
	lump_t	*pLump;
};

int SortLumpsByOffset( const SortedLump_t *pSortedLumpA, const SortedLump_t *pSortedLumpB ) 
{
	int fileOffsetA = pSortedLumpA->pLump->fileofs;
	int fileOffsetB = pSortedLumpB->pLump->fileofs;

	int fileSizeA = pSortedLumpA->pLump->filelen;
	int fileSizeB = pSortedLumpB->pLump->filelen;

	// invalid or empty lumps get sorted together
	if ( !fileSizeA )
	{
		fileOffsetA = 0;
	}
	if ( !fileSizeB )
	{
		fileOffsetB = 0;
	}

	// compare by offset, want ascending
	if ( fileOffsetA < fileOffsetB )
	{
		return -1;
	}
	else if ( fileOffsetA > fileOffsetB )
	{
		return 1;
	}

	return 0;
}

bool CompressGameLump( dheader_t *pInBSPHeader, dheader_t *pOutBSPHeader, CUtlBuffer &outputBuffer, CompressFunc_t pCompressFunc )
{
	dgamelumpheader_t* pInGameLumpHeader = (dgamelumpheader_t*)(((byte *)pInBSPHeader) + pInBSPHeader->lumps[LUMP_GAME_LUMP].fileofs);
	dgamelump_t* pInGameLump = (dgamelump_t*)(pInGameLumpHeader + 1);

	intp newOffset = outputBuffer.TellPut();
	// Make room for gamelump header and gamelump structs, which we'll write at the end
	outputBuffer.SeekPut( CUtlBuffer::SEEK_CURRENT, sizeof( dgamelumpheader_t ) );
	outputBuffer.SeekPut( CUtlBuffer::SEEK_CURRENT, pInGameLumpHeader->lumpCount * sizeof( dgamelump_t ) );

	// Start with input lumps, and fixup
	dgamelumpheader_t sOutGameLumpHeader = *pInGameLumpHeader;
	CUtlBuffer sOutGameLumpBuf;
	sOutGameLumpBuf.Put( pInGameLump, pInGameLumpHeader->lumpCount * sizeof( dgamelump_t ) );
	dgamelump_t *sOutGameLump = sOutGameLumpBuf.Base<dgamelump_t>();

	// add a dummy terminal gamelump
	// purposely NOT updating the .filelen to reflect the compressed size, but leaving as original size
	// callers use the next entry offset to determine compressed size
	sOutGameLumpHeader.lumpCount++;
	dgamelump_t dummyLump = {};
	outputBuffer.Put( &dummyLump, sizeof( dgamelump_t ) );

	for ( int i = 0; i < pInGameLumpHeader->lumpCount; i++ )
	{
		CUtlBuffer inputBuffer;
		CUtlBuffer compressedBuffer;

		sOutGameLump[i].fileofs = AlignBuffer( outputBuffer, 4 );

		if ( pInGameLump[i].filelen )
		{
			if ( pInGameLump[i].flags & GAMELUMPFLAG_COMPRESSED )
			{
				byte *pCompressedLump = ((byte *)pInBSPHeader) + pInGameLump[i].fileofs;
				if ( CLZMA::IsCompressed( pCompressedLump ) )
				{
					unsigned int actualSize = CLZMA::GetActualSize( pCompressedLump );
					inputBuffer.EnsureCapacity( actualSize );
					// dimhotepus: Add out size to prevent overflows.
					size_t outSize = CLZMA::Uncompress( pCompressedLump, inputBuffer.Base<unsigned char>(), actualSize );
					inputBuffer.SeekPut( CUtlBuffer::SEEK_CURRENT, outSize );
					if ( outSize != actualSize )
					{
						Warning( "Decompressed size %zu differs from header %u one, BSP may be corrupt.\n",
							outSize, actualSize );
					}
				}
				else
				{
					Assert( CLZMA::IsCompressed( pCompressedLump ) );
					Warning( "Unsupported BSP: Unrecognized compressed game lump.\n" );
				}

			}
			else
			{
				inputBuffer.SetExternalBuffer( ((byte *)pInBSPHeader) + pInGameLump[i].fileofs,
				                               pInGameLump[i].filelen, pInGameLump[i].filelen );
			}

			bool bCompressed = pCompressFunc ? pCompressFunc( inputBuffer, compressedBuffer ) : false;
			if ( bCompressed )
			{
				sOutGameLump[i].flags |= GAMELUMPFLAG_COMPRESSED;

				outputBuffer.Put( compressedBuffer.Base(), compressedBuffer.TellPut() );
				compressedBuffer.Purge();
			}
			else
			{
				// as is, clear compression flag from input lump
				sOutGameLump[i].flags &= ~GAMELUMPFLAG_COMPRESSED;
				outputBuffer.Put( inputBuffer.Base(), inputBuffer.TellPut() );
			}
		}
	}

	// fix the dummy terminal lump
	int lastLump = sOutGameLumpHeader.lumpCount-1;
	sOutGameLump[lastLump].fileofs = outputBuffer.TellPut();

	pOutBSPHeader->lumps[LUMP_GAME_LUMP].fileofs = newOffset;
	pOutBSPHeader->lumps[LUMP_GAME_LUMP].filelen = outputBuffer.TellPut() - newOffset;
	// We set GAMELUMPFLAG_COMPRESSED and handle compression at the sub-lump level, this whole lump is not
	// decompressable as a block.
	pOutBSPHeader->lumps[LUMP_GAME_LUMP].uncompressedSize = 0;

	// Rewind to start and write lump headers
	intp endOffset = outputBuffer.TellPut();
	outputBuffer.SeekPut( CUtlBuffer::SEEK_HEAD, newOffset );
	outputBuffer.Put( &sOutGameLumpHeader, sizeof( dgamelumpheader_t ) );
	outputBuffer.Put( sOutGameLumpBuf.Base(), sOutGameLumpBuf.TellPut() );
	outputBuffer.SeekPut( CUtlBuffer::SEEK_HEAD, endOffset );

	return true;
}

//-----------------------------------------------------------------------------
// Compress callback for RepackBSP
//-----------------------------------------------------------------------------
bool RepackBSPCallback_LZMA( CUtlBuffer &inputBuffer, CUtlBuffer &outputBuffer )
{
	if ( !inputBuffer.TellPut() )
	{
		// nothing to do
		return false;
	}

	unsigned int originalSize = inputBuffer.TellPut() - inputBuffer.TellGet();
	unsigned int compressedSize = 0;
	unsigned char *pCompressedOutput = LZMA_Compress( inputBuffer.Base<unsigned char>() + inputBuffer.TellGet(),
													  originalSize, &compressedSize );
	if ( pCompressedOutput )
	{
		outputBuffer.Put( pCompressedOutput, compressedSize );
		DevMsg( "Compressed bsp lump %u -> %u bytes.\n", originalSize, compressedSize );
		free( pCompressedOutput );
		return true;
	}

	return false;
}


bool RepackBSP( CUtlBuffer &inputBufferIn, CUtlBuffer &outputBuffer, CompressFunc_t pCompressFunc, IZip::eCompressionType packfileCompression )
{
	dheader_t *pInBSPHeader = inputBufferIn.Base<dheader_t>();
	if ( pInBSPHeader->ident != IDBSPHEADER )
	{
		Warning( "RepackBSP given invalid BSP header identifier. Expect 0x%x, got 0x%x.\n",
			IDBSPHEADER, pInBSPHeader->ident );
		return false;
	}

	intp headerOffset = outputBuffer.TellPut();
	outputBuffer.Put( pInBSPHeader, sizeof( dheader_t ) );

	// This buffer grows dynamically, don't keep pointers to it around. Write out header at end.
	dheader_t sOutBSPHeader = *pInBSPHeader;

	// must adhere to input lump's offset order and process according to that, NOT lump num
	// sort by offset order
	CUtlVector< SortedLump_t > sortedLumps;
	for ( int i = 0; i < HEADER_LUMPS; i++ )
	{
		intp iIndex = sortedLumps.AddToTail();
		sortedLumps[iIndex].lumpNum = i;
		sortedLumps[iIndex].pLump = &pInBSPHeader->lumps[i];
	}
	sortedLumps.Sort( SortLumpsByOffset );

	// iterate in sorted order
	for ( int i = 0; i < HEADER_LUMPS; ++i )
	{
		SortedLump_t *pSortedLump = &sortedLumps[i];
		int lumpNum = pSortedLump->lumpNum;

		// Should be set below, don't copy over old data
		sOutBSPHeader.lumps[lumpNum].fileofs = 0;
		sOutBSPHeader.lumps[lumpNum].filelen = 0;
		// Only set by compressed lumps
		sOutBSPHeader.lumps[lumpNum].uncompressedSize = 0;

		if ( pSortedLump->pLump->filelen ) // Otherwise its degenerate
		{
			int alignment = 4;
			if ( lumpNum == LUMP_PAKFILE )
			{
				alignment = 2048;
			}
			int newOffset = AlignBuffer( outputBuffer, alignment );

			CUtlBuffer inputBuffer;
			if ( pSortedLump->pLump->uncompressedSize )
			{
				byte *pCompressedLump = ((byte *)pInBSPHeader) + pSortedLump->pLump->fileofs;
				unsigned int headerSize = static_cast<unsigned>(pSortedLump->pLump->uncompressedSize);
				if ( CLZMA::IsCompressed( pCompressedLump ) && headerSize == CLZMA::GetActualSize( pCompressedLump ) )
				{
					unsigned size = CLZMA::GetActualSize( pCompressedLump );
					inputBuffer.EnsureCapacity( size );
					// dimhotepus: Add out size to prevent overflows.
					size_t outSize = CLZMA::Uncompress( pCompressedLump, inputBuffer.Base<unsigned char>(), size );
					inputBuffer.SeekPut( CUtlBuffer::SEEK_CURRENT, outSize );
					if ( outSize != headerSize )
					{
						Warning( "Decompressed size %zu differs from header %u one, BSP may be corrupt.\n",
							outSize, headerSize );
					}
				}
				else
				{
					Assert( CLZMA::IsCompressed( pCompressedLump ) &&
					        headerSize == CLZMA::GetActualSize( pCompressedLump ) );
					Warning( "Unsupported BSP: Unrecognized compressed lump\n" );
				}
			}
			else
			{
				// Just use input
				inputBuffer.SetExternalBuffer( ((byte *)pInBSPHeader) + pSortedLump->pLump->fileofs,
				                               pSortedLump->pLump->filelen, pSortedLump->pLump->filelen );
			}

			if ( lumpNum == LUMP_GAME_LUMP )
			{
				// the game lump has to have each of its components individually compressed
				CompressGameLump( pInBSPHeader, &sOutBSPHeader, outputBuffer, pCompressFunc );
			}
			else if ( lumpNum == LUMP_PAKFILE )
			{
				IZip *newPakFile = IZip::CreateZip( NULL );
				IZip *oldPakFile = IZip::CreateZip( NULL );
				oldPakFile->ParseFromBuffer( inputBuffer.Base(), inputBuffer.Size() );

				int id = -1;
				int fileSize;
				while ( 1 )
				{
					char relativeName[MAX_PATH];
					id = GetNextFilename( oldPakFile, id, relativeName, sizeof( relativeName ), fileSize );
					if ( id == -1 )
						break;

					CUtlBuffer sourceBuf;
					CUtlBuffer targetBuf;

					bool bOK = ReadFileFromPak( oldPakFile, relativeName, false, sourceBuf );
					if ( !bOK )
					{
						Error( "Failed to load '%s' from lump pak for repacking.\n", relativeName );
						continue;
					}

					AddBufferToPak( newPakFile, relativeName, sourceBuf.Base(), sourceBuf.TellMaxPut(), false, packfileCompression );

					DevMsg( "Repacking BSP: Created '%s' in lump pak\n", relativeName );
				}

				// save new pack to buffer
				newPakFile->SaveToBuffer( outputBuffer );
				sOutBSPHeader.lumps[lumpNum].fileofs = newOffset;
				sOutBSPHeader.lumps[lumpNum].filelen = outputBuffer.TellPut() - newOffset;
				// Note that this *lump* is uncompressed, it just contains a packfile that uses compression, so we're
				// not setting lumps[lumpNum].uncompressedSize

				IZip::ReleaseZip( oldPakFile );
				IZip::ReleaseZip( newPakFile );
			}
			else
			{
				CUtlBuffer compressedBuffer;
				bool bCompressed = pCompressFunc ? pCompressFunc( inputBuffer, compressedBuffer ) : false;
				if ( bCompressed )
				{
					sOutBSPHeader.lumps[lumpNum].uncompressedSize = inputBuffer.TellPut();
					sOutBSPHeader.lumps[lumpNum].filelen = compressedBuffer.TellPut();
					sOutBSPHeader.lumps[lumpNum].fileofs = newOffset;
					outputBuffer.Put( compressedBuffer.Base(), compressedBuffer.TellPut() );
					compressedBuffer.Purge();
				}
				else
				{
					// add as is
					sOutBSPHeader.lumps[lumpNum].fileofs = newOffset;
					sOutBSPHeader.lumps[lumpNum].filelen = inputBuffer.TellPut();
					outputBuffer.Put( inputBuffer.Base(), inputBuffer.TellPut() );
				}
			}
		}
	}

	// Write out header
	intp endOffset = outputBuffer.TellPut();
	outputBuffer.SeekPut( CUtlBuffer::SEEK_HEAD, headerOffset );
	outputBuffer.Put( &sOutBSPHeader, sizeof( sOutBSPHeader ) );
	outputBuffer.SeekPut( CUtlBuffer::SEEK_HEAD, endOffset );

	return true;
}

//-----------------------------------------------------------------------------
//  For all lumps in a bsp: Loads the lump from file A, swaps it, writes it to file B.
//  This limits the memory used for the swap process which helps the Xbox 360.
//
//	NOTE: These lumps will be written to the file in exactly the order they appear here,
//	so they can be shifted around if desired for file access optimization.
//-----------------------------------------------------------------------------
bool SwapBSPFile( const char *pInFilename, const char *pOutFilename, bool bSwapOnLoad, VTFConvertFunc_t pVTFConvertFunc, VHVFixupFunc_t pVHVFixupFunc, CompressFunc_t pCompressFunc )
{
	DevMsg( "Creating %s\n", pOutFilename );

	if ( !g_pFileSystem->FileExists( pInFilename ) )
	{
		Warning( "Error! Couldn't open input file %s - BSP swap failed!\n", pInFilename ); 
		return false;
	}

	FileHandle_t bspHanle = SafeOpenWrite( pOutFilename );
	if ( !bspHanle )
	{
		Warning( "Error! Couldn't open output file %s - BSP swap failed!\n", pOutFilename ); 
		return false;
	}

	if ( !pVTFConvertFunc )
	{
		Warning( "Error! Missing VTF Conversion function\n" ); 
		return false;
	}
	g_pVTFConvertFunc = pVTFConvertFunc;

	// optional VHV fixup
	g_pVHVFixupFunc = pVHVFixupFunc;

	// optional compression callback
	g_pCompressFunc = pCompressFunc;

	// These must be mutually exclusive
	g_bSwapOnLoad = bSwapOnLoad;
	g_bSwapOnWrite = !bSwapOnLoad;

	g_Swap.ActivateByteSwapping( true );

	dheader_t *header = OpenBSPFile(pInFilename);
	if (!header) return false;

	// CRC the bsp first
	CRC32_t mapCRC;
	CRC32_Init(&mapCRC);
	if ( !CRC_MapFile( header, &mapCRC, pInFilename ) )
	{
		Warning( "Failed to CRC the map '%s'.\n", pInFilename );
		// dimhotepus: Do not leak BSP data.
		CloseBSPFile(header);
		return false;
	}

	// hold a dictionary of all the static prop names
	// this is needed to properly convert any VHV files inside the pak lump
	BuildStaticPropNameTable(header);

	// Set the output file pointer after the header
	dheader_t dummyHeader = {};
	SafeWrite( bspHanle, &dummyHeader, sizeof( dheader_t ) );

	// To allow for alignment fixups, the lumps will be written to the
	// output file in the order they appear in this function.

	// NOTE: Flags for 360 !!!MUST!!! be first	
	SwapLumpToDisk< dflagslump_t >(header, bspHanle, LUMP_MAP_FLAGS );

	// complex lump swaps first or for later contingent data
	SwapLeafLumpToDisk(header, bspHanle);
	SwapOcclusionLumpToDisk(header, bspHanle);
	SwapGameLumpsToDisk(header, bspHanle);

	// Strip dead or non relevant lumps
	header->lumps[LUMP_DISP_LIGHTMAP_ALPHAS].filelen = 0;
	header->lumps[LUMP_FACEIDS].filelen = 0;

	// Strip obsolete LDR in favor of HDR
	if ( SwapLumpToDisk<dface_t>( header, bspHanle, LUMP_FACES_HDR ) )
	{
		header->lumps[LUMP_FACES].filelen = 0;
	}
	else
	{
		// no HDR, keep LDR version
		SwapLumpToDisk<dface_t>( header, bspHanle, LUMP_FACES );
	}

	if ( SwapLumpToDisk<dworldlight_t>( header, bspHanle, LUMP_WORLDLIGHTS_HDR ) )
	{
		header->lumps[LUMP_WORLDLIGHTS].filelen = 0;
	}
	else
	{
		// no HDR, keep LDR version
		SwapLumpToDisk<dworldlight_t>( header, bspHanle, LUMP_WORLDLIGHTS );
	}

	// Simple lump swaps
	SwapLumpToDisk<byte>( header, bspHanle, FIELD_CHARACTER, LUMP_PHYSDISP );
	SwapLumpToDisk<byte>( header, bspHanle, FIELD_CHARACTER, LUMP_PHYSCOLLIDE );
	SwapLumpToDisk<byte>( header, bspHanle, FIELD_CHARACTER, LUMP_VISIBILITY );
	SwapLumpToDisk<dmodel_t>( header, bspHanle, LUMP_MODELS );
	SwapLumpToDisk<dvertex_t>( header, bspHanle, LUMP_VERTEXES );
	SwapLumpToDisk<dplane_t>( header, bspHanle, LUMP_PLANES );
	SwapLumpToDisk<dnode_t>( header, bspHanle, LUMP_NODES );
	SwapLumpToDisk<texinfo_t>( header, bspHanle, LUMP_TEXINFO );
	SwapLumpToDisk<dtexdata_t>( header, bspHanle, LUMP_TEXDATA );
	SwapLumpToDisk<ddispinfo_t>( header, bspHanle, LUMP_DISPINFO );
    SwapLumpToDisk<CDispVert>( header, bspHanle, LUMP_DISP_VERTS );
	SwapLumpToDisk<CDispTri>( header, bspHanle,  LUMP_DISP_TRIS );
    SwapLumpToDisk<char>( header, bspHanle,  FIELD_CHARACTER, LUMP_DISP_LIGHTMAP_SAMPLE_POSITIONS );
	SwapLumpToDisk<CFaceMacroTextureInfo>( header, bspHanle,  LUMP_FACE_MACRO_TEXTURE_INFO );
	SwapLumpToDisk<dprimitive_t>( header, bspHanle,  LUMP_PRIMITIVES );
	SwapLumpToDisk<dprimvert_t>( header, bspHanle,  LUMP_PRIMVERTS );
	SwapLumpToDisk<unsigned short>( header, bspHanle,  FIELD_SHORT, LUMP_PRIMINDICES );
    SwapLumpToDisk<dface_t>( header, bspHanle,  LUMP_ORIGINALFACES );
	SwapLumpToDisk<unsigned short>( header, bspHanle,  FIELD_SHORT, LUMP_LEAFFACES );
	SwapLumpToDisk<unsigned short>( header, bspHanle,  FIELD_SHORT, LUMP_LEAFBRUSHES );
	SwapLumpToDisk<int>( header, bspHanle,  FIELD_INTEGER, LUMP_SURFEDGES );
	SwapLumpToDisk<dedge_t>( header, bspHanle,  LUMP_EDGES );
	SwapLumpToDisk<dbrush_t>( header, bspHanle,  LUMP_BRUSHES );
	SwapLumpToDisk<dbrushside_t>( header, bspHanle,  LUMP_BRUSHSIDES );
	SwapLumpToDisk<darea_t>( header, bspHanle,  LUMP_AREAS );
	SwapLumpToDisk<dareaportal_t>( header, bspHanle,  LUMP_AREAPORTALS );
	SwapLumpToDisk<char>( header, bspHanle,  FIELD_CHARACTER, LUMP_ENTITIES );
	SwapLumpToDisk<dleafwaterdata_t>( header, bspHanle,  LUMP_LEAFWATERDATA );
	SwapLumpToDisk<float>( header, bspHanle,  FIELD_VECTOR, LUMP_VERTNORMALS );
	SwapLumpToDisk<short>( header, bspHanle,  FIELD_SHORT, LUMP_VERTNORMALINDICES );
	SwapLumpToDisk<float>( header, bspHanle,  FIELD_VECTOR, LUMP_CLIPPORTALVERTS );
	SwapLumpToDisk<dcubemapsample_t>( header, bspHanle,  LUMP_CUBEMAPS );	
	SwapLumpToDisk<char>( header, bspHanle,  FIELD_CHARACTER, LUMP_TEXDATA_STRING_DATA );
	SwapLumpToDisk<int>( header, bspHanle,  FIELD_INTEGER, LUMP_TEXDATA_STRING_TABLE );
	SwapLumpToDisk<doverlay_t>( header, bspHanle,  LUMP_OVERLAYS );
	SwapLumpToDisk<dwateroverlay_t>( header, bspHanle,  LUMP_WATEROVERLAYS );
	SwapLumpToDisk<unsigned short>( header, bspHanle,  FIELD_SHORT, LUMP_LEAFMINDISTTOWATER );
	SwapLumpToDisk<doverlayfade_t>( header, bspHanle,  LUMP_OVERLAY_FADES );


	// NOTE: this data placed at the end for the sake of 360:
	{
		// NOTE: lighting must be the penultimate lump
		//       (allows 360 to free this memory part-way through map loading)
		if ( SwapLumpToDisk<byte>( header, bspHanle, FIELD_CHARACTER, LUMP_LIGHTING_HDR ) )
		{
			header->lumps[LUMP_LIGHTING].filelen = 0;
		}
		else
		{
			// no HDR, keep LDR version
			SwapLumpToDisk<byte>( header, bspHanle, FIELD_CHARACTER, LUMP_LIGHTING );
		}
		// NOTE: Pakfile for 360 !!!MUST!!! be last	
		SwapPakfileLumpToDisk( header, bspHanle, pInFilename );
	}


	// Store the crc in the flags lump version field
	header->lumps[LUMP_MAP_FLAGS].version = mapCRC;

	// Pad out the end of the file to a sector boundary for optimal IO
	AlignFilePosition( bspHanle, XBOX_DVD_SECTORSIZE );

	// Warn of any lumps that didn't get swapped
	for ( int i = 0; i < HEADER_LUMPS; ++i )
	{
		if ( HasLump( header, i ) && !g_Lumps.bLumpParsed[i] )
		{
			// a new lump got added that needs to have a swap function
			Warning( "Map '%s': Lump %s has no swap or copy function. Discarding!\n", pInFilename, GetLumpName(i) );

			// the data didn't get copied, so don't reference garbage
			header->lumps[i].filelen = 0;
		}
	}

	// Write the updated header
	g_pFileSystem->Seek( bspHanle, 0, FILESYSTEM_SEEK_HEAD );
	WriteData( bspHanle, header );
	g_pFileSystem->Close( bspHanle );
	bspHanle = 0;

	// Cleanup
	g_Swap.ActivateByteSwapping( false );

	CloseBSPFile(header);

	g_StaticPropNames.Purge();
	g_StaticPropInstances.Purge();

	DevMsg( "Finished map '%s' Swap.\n", pInFilename );

	// caller provided compress func will further compress compatible lumps
	if ( pCompressFunc )
	{
		CUtlBuffer inputBuffer;
		if ( !g_pFileSystem->ReadFile( pOutFilename, NULL, inputBuffer ) )
		{
			Warning( "Error! Couldn't read file '%s' - final BSP compression failed!\n", pOutFilename ); 
			return false;
		}

		CUtlBuffer outputBuffer;
		if ( !RepackBSP( inputBuffer, outputBuffer, pCompressFunc, IZip::eCompressionType_None ) )
		{
			Warning( "Error! Failed to compress BSP '%s'!\n", pOutFilename );
			return false;
		}

		bspHanle = SafeOpenWrite( pOutFilename );
		if ( !bspHanle )
		{
			Warning( "Error! Couldn't open output file '%s' - BSP swap failed!\n", pOutFilename ); 
			return false;
		}
		
		RunCodeAtScopeExit(g_pFileSystem->Close(bspHanle));

		SafeWrite( bspHanle, outputBuffer.Base(), outputBuffer.TellPut() );
	}

	return true;
}

//-----------------------------------------------------------------------------
// Get the pak lump from a BSP
//-----------------------------------------------------------------------------
bool GetPakFileLump( const char *pBSPFilename, void **pPakData, int *pPakSize )
{
	*pPakData = NULL;
	*pPakSize = 0;

	if ( !g_pFileSystem->FileExists( pBSPFilename ) )
	{
		Warning( "Error! Couldn't open file %s!\n", pBSPFilename ); 
		return false;
	}

	// determine endian nature
	dheader_t *pHeader;
	if (!LoadFile( pBSPFilename, (void **)&pHeader ))
	{
		Warning("Unable to load map '%s'.\n", pBSPFilename);
		return false;
	}
	bool bSwap = ( pHeader->ident == BigLong( IDBSPHEADER ) );
	free( pHeader );

	g_bSwapOnLoad = bSwap;
	g_bSwapOnWrite = !bSwap;

	dheader_t *header = OpenBSPFile(pBSPFilename);
	if (!header) return false;

	RunCodeAtScopeExit(CloseBSPFile(header));
	
	if ( header->lumps[LUMP_PAKFILE].filelen )
	{
		*pPakSize = CopyVariableLump<byte>( header, FIELD_CHARACTER, LUMP_PAKFILE, pPakData );
	}

	return true;
}

static thread_local dheader_t *s_sort_header;
// compare function for qsort below
static int LumpOffsetCompare( const void *pElem1, const void *pElem2 )
{
	const int lump1 = *(const byte *)pElem1;
	const int lump2 = *(const byte *)pElem2;

	if ( lump1 != lump2 )
	{
		// force LUMP_MAP_FLAGS to be first, always
		if ( lump1 == LUMP_MAP_FLAGS )
		{
			return -1;
		}
		else if ( lump2 == LUMP_MAP_FLAGS )
		{
			return 1;
		}

		// force LUMP_PAKFILE to be last, always
		if ( lump1 == LUMP_PAKFILE )
		{
			return 1;
		}
		else if ( lump2 == LUMP_PAKFILE )
		{
			return -1;
		}
	}

	// thread_local access is slow, cache for performance
	const dheader_t *header = s_sort_header;

	int fileOffset1 = header->lumps[lump1].fileofs;
	int fileOffset2 = header->lumps[lump2].fileofs;

	// invalid or empty lumps will get sorted together
	if ( !header->lumps[lump1].filelen )
	{
		fileOffset1 = 0;
	}

	if ( !header->lumps[lump2].filelen )
	{
		fileOffset2 = 0;
	}

	// compare by offset
	return fileOffset1 - fileOffset2;
}

//-----------------------------------------------------------------------------
// Replace the pak lump in a BSP
//-----------------------------------------------------------------------------
bool SetPakFileLump( const char *pBSPFilename, const char *pNewFilename, void *pPakData, int pakSize )
{
	if ( !g_pFileSystem->FileExists( pBSPFilename ) )
	{
		Warning( "Couldn't open map '%s'. File not exists or has no access to.\n", pBSPFilename );
		return false;
	}

	// determine endian nature
	dheader_t *pHeader;
	if ( !LoadFile( pBSPFilename, (void **)&pHeader ) )
	{
		Warning( "Unable to load map '%s'.\n", pBSPFilename );
		return false;
	}

	bool bSwap = ( pHeader->ident == BigLong( IDBSPHEADER ) );
	free( pHeader );

	g_bSwapOnLoad = bSwap;
	g_bSwapOnWrite = bSwap;

	dheader_t *header = OpenBSPFile(pBSPFilename);
	if (!header) return false;

	RunCodeAtScopeExit(CloseBSPFile(header));

	// save a copy of the old header
	// generating a new bsp is a destructive operation
	dheader_t oldHeader = *header;

	FileHandle_t bspHanle = SafeOpenWrite( pNewFilename );
	if ( !bspHanle )
	{
		return false;
	}

	RunCodeAtScopeExit(g_pFileSystem->Close(bspHanle));

	// placeholder only, reset at conclusion
	WriteData( bspHanle, &oldHeader );

	// lumps must be reserialized in same relative offset order
	// build sorted order table
	int readOrder[HEADER_LUMPS];
	for ( int i=0; i<HEADER_LUMPS; i++ )
	{
		readOrder[i] = i;
	}
	s_sort_header = header;
	qsort( readOrder, HEADER_LUMPS, sizeof( int ), LumpOffsetCompare );
	s_sort_header = nullptr;

	for ( int i = 0; i < HEADER_LUMPS; i++ )
	{
		int lump = readOrder[i];

		if ( lump == LUMP_PAKFILE )
		{
			// pak lump always written last, with special alignment
			continue;
		}

		int length = header->lumps[lump].filelen;
		if ( length )
		{
			// save the lump data
			int offset = header->lumps[lump].fileofs;
			SetAlignedLumpPosition( header, bspHanle, lump );
			SafeWrite( bspHanle, (byte *)header + offset, length );
		}
		else
		{
			header->lumps[lump].fileofs = 0;
		}
	}

	// Always write the pak file at the end
	// Pad out the end of the file to a sector boundary for optimal IO
	header->lumps[LUMP_PAKFILE].fileofs = AlignFilePosition( bspHanle, XBOX_DVD_SECTORSIZE );
	header->lumps[LUMP_PAKFILE].filelen = pakSize;
	SafeWrite( bspHanle, pPakData, pakSize );

	// Pad out the end of the file to a sector boundary for optimal IO
	AlignFilePosition( bspHanle, XBOX_DVD_SECTORSIZE );

	// Write the updated header
	g_pFileSystem->Seek( bspHanle, 0, FILESYSTEM_SEEK_HEAD );
	WriteData( bspHanle, header );
	
	return true;
}

//-----------------------------------------------------------------------------
// Build a list of files that BSP owns, world/cubemap materials, static props, etc.
//-----------------------------------------------------------------------------
bool GetBSPDependants( const char *pBSPFilename, CUtlVector< CUtlString > *pList )
{
	if ( !g_pFileSystem->FileExists( pBSPFilename ) )
	{
		Warning( "Couldn't open map '%s'. File not exists or has no access to.\n", pBSPFilename );
		return false;
	}

	// must be set, but exact hdr not critical for dependant traversal.
	const ScopedHDRMode scoped_hdr_mode( false );

	if ( !LoadBSPFile( pBSPFilename ) )
	{
		return false;
	}

	char szBspName[MAX_PATH];
	V_FileBase( pBSPFilename, szBspName );
	V_SetExtension( szBspName, ".bsp" );

	// get embedded pak files, and internals
	char szFilename[MAX_PATH];
	int fileSize;
	int fileId = -1;
	for ( ;; )
	{
		fileId = GetPakFile()->GetNextFilename( fileId, szFilename, sizeof( szFilename ), fileSize );
		if ( fileId == -1 )
		{
			break;
		}
		pList->AddToTail( szFilename );
	}

	// get all the world materials
	for ( int i = 0; i < numtexdata; i++ )
	{
		const char *pName = TexDataStringTable_GetString( dtexdata[i].nameStringTableID );

		V_ComposeFileName( "materials", pName, szFilename );
		V_SetExtension( szFilename, ".vmt" );

		pList->AddToTail( szFilename );
	}

	// get all the static props
	GameLumpHandle_t hGameLump = g_GameLumps.GetGameLumpHandle( GAMELUMP_STATIC_PROPS );
	if ( hGameLump != g_GameLumps.InvalidGameLump() )
	{
		byte *pGameLumpData = (byte *)g_GameLumps.GetGameLump( hGameLump );
		if ( pGameLumpData && g_GameLumps.GameLumpSize( hGameLump ) )
		{
			int count = ((int *)pGameLumpData)[0];
			pGameLumpData += sizeof( int );

			StaticPropDictLump_t *pStaticPropDictLump = (StaticPropDictLump_t *)pGameLumpData;
			for ( int i=0; i<count; i++ )
			{
				pList->AddToTail( pStaticPropDictLump[i].m_Name );
			}
		}
	}

	// get all the detail props
	hGameLump = g_GameLumps.GetGameLumpHandle( GAMELUMP_DETAIL_PROPS );
	if ( hGameLump != g_GameLumps.InvalidGameLump() )
	{
		byte *pGameLumpData = (byte *)g_GameLumps.GetGameLump( hGameLump );
		if ( pGameLumpData && g_GameLumps.GameLumpSize( hGameLump ) )
		{
			int count = ((int *)pGameLumpData)[0];
			pGameLumpData += sizeof( int );

			DetailObjectDictLump_t *pDetailObjectDictLump = (DetailObjectDictLump_t *)pGameLumpData;
			for ( int i=0; i<count; i++ )
			{
				pList->AddToTail( pDetailObjectDictLump[i].m_Name );
			}
			pGameLumpData += count * sizeof( DetailObjectDictLump_t );

			if ( g_GameLumps.GetGameLumpVersion( hGameLump ) == 4 )
			{
				count = ((int *)pGameLumpData)[0];
				pGameLumpData += sizeof( int );
				if ( count )
				{
					// All detail prop sprites must lie in the material detail/detailsprites
					pList->AddToTail( "materials/detail/detailsprites.vmt" );
				}
			}
		}
	}

	UnloadBSPFile();

	return true;
}

