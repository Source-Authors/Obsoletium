//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//
//=============================================================================//
#include "tier1/utlbuffer.h"
#include "cmdlib.h"
#include "scriplib.h"
#include "mathlib/mathlib.h"
#include "studio.h"
#include "tier1/characterset.h"
#include "studiomdl.h"

bool IsEnd( char const* pLine );
int SortAndBalanceBones( int iCount, int iMaxCount, int bones[], float weights[] );

int AddToVlist( int v, int m, int n, int t, int firstref );
void DecrementReferenceVlist( int uv, int numverts );
int faceCompare( const void *elem1, const void *elem2 );


void UnifyIndices( s_source_t *psource );

struct MtlInfo_t
{
	CUtlString m_MtlName;
	CUtlString m_TgaName;
};

static CUtlVector<MtlInfo_t> g_MtlLib;

void ParseMtlLib( CUtlBuffer &buf )
{
	intp nCurrentMtl = -1;
	while ( buf.IsValid() )
	{
		buf.GetLine( g_szLine );

		if ( !Q_strnicmp( g_szLine, "newmtl ", 7 ) )
		{
			char mtlName[1024];
			// dimhotepus: Prevent buffer overflow.
			if ( sscanf( g_szLine, "newmtl %1023s", mtlName ) == 1 )
			{
				mtlName[ssize(mtlName) - 1] = '\0';
				nCurrentMtl = g_MtlLib.AddToTail( );
				g_MtlLib[nCurrentMtl].m_MtlName = mtlName;
				g_MtlLib[nCurrentMtl].m_TgaName = "debugempty";
			}
			continue;
		}

		if ( !Q_strnicmp( g_szLine, "map_Kd ", 7 ) )
		{
			if ( nCurrentMtl < 0 )
				continue;

			char tgaPath[MAX_PATH];
			char tgaName[1024];
			// dimhotepus: Prevent buffer overflow.
			if ( sscanf( g_szLine, "map_Kd %259s", tgaPath ) == 1 )
			{
				tgaPath[ssize(tgaPath) - 1] = '\0';
				Q_FileBase( tgaPath, tgaName );
				g_MtlLib[nCurrentMtl].m_TgaName = tgaName;
			}
			continue;
		}
	}
}

const char *FindMtlEntry( const char *pTgaName )
{
	intp nCount = g_MtlLib.Count();
	for ( intp i = 0; i < nCount; ++i )
	{
		if ( !Q_stricmp( g_MtlLib[i].m_MtlName, pTgaName ) )
			return g_MtlLib[i].m_TgaName;
	}
	return pTgaName;
}									 

static bool ParseVertex( CUtlBuffer& bufParse, const characterset_t &breakSet, int &v, int &t, int &n )
{
	char	cmd[1024];
	intp nLen = bufParse.ParseToken( &breakSet, cmd, false );
	if ( nLen <= 0 )
		return false;

	v = atoi( cmd );
	n = 0;
	t = 0;

	char c = *(const char*)bufParse.PeekGet();
	bool bHasTexCoord = breakSet.HasChar( c );
	bool bHasNormal = false;
	if ( bHasTexCoord )
	{
		// Snag the '/'
		nLen = bufParse.ParseToken( &breakSet, cmd, sizeof(cmd), false );
		Assert( nLen == 1 );

		c = *(const char*)bufParse.PeekGet();
		if ( !IN_CHARACTERSET( breakSet, c ) )
		{
			nLen = bufParse.ParseToken( &breakSet, cmd, sizeof(cmd), false );
			Assert( nLen > 0 );
			t = atoi( cmd );

			c = *(const char*)bufParse.PeekGet();
			bHasNormal = breakSet.HasChar( c );
		}
		else
		{
			bHasNormal = true;
			bHasTexCoord = false;
		}

		if ( bHasNormal )
		{
			// Snag the '/'
			nLen = bufParse.ParseToken( &breakSet, cmd, sizeof(cmd), false );
			Assert( nLen == 1 );

			nLen = bufParse.ParseToken( &breakSet, cmd, sizeof(cmd), false );
			Assert( nLen > 0 );
			n = atoi( cmd );
		}
	}
	return true;
}


int Load_OBJ( s_source_t *psource )
{
	char	cmd[1024];
	int		i;
	int		material = -1;

	g_MtlLib.RemoveAll();

	if ( !OpenGlobalFile( psource->filename ) )
		return 0;

	char pFullPath[MAX_PATH];
	if ( !GetGlobalFilePath( psource->filename, pFullPath ) )
		return 0;

	char pFullDir[MAX_PATH];
	V_ExtractFilePath( pFullPath, pFullDir );

	if( !g_quiet )
	{
		printf( "grabbing %s\n", psource->filename );
	}

	g_iLinecount = 0;

	psource->numbones = 1;
	V_strcpy_safe( psource->localBone[0].name, "default" );
	psource->localBone[0].parent = -1;
	Assert( psource->m_Animations.Count() == 0 );
    s_sourceanim_t *pSourceAnim = FindOrAddSourceAnim( psource, "BindPose" );
	pSourceAnim->numframes = 1;
	pSourceAnim->startframe = 0;
	pSourceAnim->endframe = 0;
	pSourceAnim->rawanim[0] = (s_bone_t *)kalloc( 1, sizeof( s_bone_t ) );
	pSourceAnim->rawanim[0][0].pos.Init();
	pSourceAnim->rawanim[0][0].rot.Init();
	Build_Reference( psource, "BindPose" );

	constexpr characterset_t breakSet{"/\\"};

	while ( GetLineInput() ) 
	{
		if ( strncmp( g_szLine, "v ", 2 ) == 0 )
		{
			i = g_numverts++;

			sscanf( g_szLine, "v %f %f %f", &g_vertex[i].x, &g_vertex[i].y, &g_vertex[i].z );
			g_bone[i].numbones = 1;
			g_bone[i].bone[0] = 0;
			g_bone[i].weight[0] = 1.0;
			continue;
		}
		
		if (strncmp( g_szLine, "vn ", 3 ) == 0)
		{
			i = g_numnormals++;
			sscanf( g_szLine, "vn %f %f %f", &g_normal[i].x, &g_normal[i].y, &g_normal[i].z );
			continue;
		}
		
		if (strncmp( g_szLine, "vt ", 3 ) == 0)
		{
			i = g_numtexcoords++;
			sscanf( g_szLine, "vt %f %f", &g_texcoord[i].x, &g_texcoord[i].y );
			g_texcoord[i].y = 1.0f - g_texcoord[i].y;
			continue;
		}
		
		if ( !Q_strncmp( g_szLine, "mtllib ", 7 ) )
		{
			// dimhotepus: Prevent buffer overflow.
			sscanf( g_szLine, "mtllib %1023s", cmd );
			cmd[ssize(cmd) - 1] = '\0';

			CUtlBuffer buf( (intp)0, 0, CUtlBuffer::TEXT_BUFFER );

			char pFullMtlLibPath[MAX_PATH];
			V_ComposeFileName( pFullDir, cmd, pFullMtlLibPath );
			if ( g_pFullFileSystem->ReadFile( pFullMtlLibPath, NULL, buf ) )
			{
				ParseMtlLib( buf );
			}
			continue;
		}

		if (strncmp( g_szLine, "usemtl ", 7 ) == 0)
		{
			// dimhotepus: Prevent buffer overflow.
			sscanf( g_szLine, "usemtl %1023s", cmd );
			cmd[ssize(cmd) - 1] = '\0';

			const char *pTexture = FindMtlEntry( cmd );
			int texture = LookupTexture( pTexture );
			psource->texmap[texture] = texture;	// hack, make it 1:1
			material = UseTextureAsMaterial( texture );
			continue;
		}

		if (strncmp( g_szLine, "f ", 2 ) == 0)
		{
			if ( material < 0 )
			{
				int texture = LookupTexture( "debugempty.tga" );
				psource->texmap[texture] = texture;
				material = UseTextureAsMaterial( texture );
			}

			int v0, n0, t0;
			int v1, n1, t1;
			int v2, n2, t2;

			s_tmpface_t f;

			// Are we specifying p only, p and t only, p and n only, or p and n and t?
			char *pData = g_szLine + 2;
			intp nLen = Q_strlen( pData );

			CUtlBuffer bufParse( pData, nLen, CUtlBuffer::TEXT_BUFFER | CUtlBuffer::READ_ONLY );

			ParseVertex( bufParse, breakSet, v0, t0, n0 );
			ParseVertex( bufParse, breakSet, v1, t1, n1 );
			Assert( v0 <= g_numverts && t0 <= g_numtexcoords && n0 <= g_numnormals );
			Assert( v1 <= g_numverts && t1 <= g_numtexcoords && n1 <= g_numnormals );
			while ( bufParse.IsValid() )
			{
				if ( !ParseVertex( bufParse, breakSet, v2, t2, n2 ) )
					break;

				Assert( v2 <= g_numverts && t2 <= g_numtexcoords && n2 <= g_numnormals );
	
				i = g_numfaces++;
				f.material = material;
				f.a = v0 - 1; f.na = (n0 > 0) ? n0 - 1 : 0, f.ta = (t0 > 0) ? t0 - 1 : 0;
				f.b = v2 - 1; f.nb = (n2 > 0) ? n2 - 1 : 0, f.tb = (t2 > 0) ? t2 - 1 : 0;
				f.c = v1 - 1; f.nc = (n1 > 0) ? n1 - 1 : 0, f.tc = (t1 > 0) ? t1 - 1 : 0;
				g_face[i] = f;

				v1 = v2; t1 = t2; n1 = n2;
			}
			continue;
		}
	}

	UnifyIndices( psource );

	BuildIndividualMeshes( psource );

	fclose( g_fpInput );

	return 1;
}



int AppendVTAtoOBJ( s_source_t *psource, char *filename, int frame )
{
	char	cmd[1024];
	int		i, j;
	int		material = 0;

	Vector tmp;
	matrix3x4_t m;

	AngleMatrix( RadianEuler( M_PI_F / 2, 0, 0 ), m );

	if ( !OpenGlobalFile( filename ) )
		return 0;

	if( !g_quiet )
	{
		printf ("grabbing %s\n", filename );
	}

	g_iLinecount = 0;

	g_numverts = g_numnormals = g_numtexcoords = g_numfaces = 0;

	while ( GetLineInput() ) 
	{
		if (strncmp( g_szLine, "v ", 2 ) == 0)
		{
			i = g_numverts++;

			sscanf( g_szLine, "v %f %f %f", &tmp.x, &tmp.y, &tmp.z );
			VectorTransform( tmp, m, g_vertex[i] );

			// printf("%f %f %f\n", g_vertex[i].x, g_vertex[i].y, g_vertex[i].z );

			g_bone[i].numbones = 1;
			g_bone[i].bone[0] = 0;
			g_bone[i].weight[0] = 1.0;
		}
		else if (strncmp( g_szLine, "vn ", 3 ) == 0)
		{
			i = g_numnormals++;
			sscanf( g_szLine, "vn %f %f %f", &tmp.x, &tmp.y, &tmp.z );
			VectorRotate( tmp, m, g_normal[i] );
		}
		else if (strncmp( g_szLine, "vt ", 3 ) == 0)
		{
			i = g_numtexcoords++;
			sscanf( g_szLine, "vt %f %f", &g_texcoord[i].x, &g_texcoord[i].y );
		}
		else if (strncmp( g_szLine, "usemtl ", 7 ) == 0)
		{
			// dimhotepus: Prevent buffer overflow.
			sscanf( g_szLine, "usemtl %1023s", cmd );
			cmd[ssize(cmd) - 1] = '\0';

			int texture = LookupTexture( cmd );
			psource->texmap[texture] = texture;	// hack, make it 1:1
			material = UseTextureAsMaterial( texture );
		}
		else if (strncmp( g_szLine, "f ", 2 ) == 0)
		{
			int v0, n0, t0;
			int v1, n1, t1;
			int v2, n2, t2;
			int v3, n3, t3;

			s_tmpface_t f;

			i = g_numfaces++;

			j = sscanf( g_szLine, "f %d/%d/%d %d/%d/%d %d/%d/%d %d/%d/%d", &v0, &t0, &n0, &v1, &t1, &n1, &v2, &t2, &n2, &v3, &t3, &n3 );

			f.material = material;
			f.a = v0 - 1; f.na = n0 - 1, f.ta = 0;
			f.b = v2 - 1; f.nb = n2 - 1, f.tb = 0;
			f.c = v1 - 1; f.nc = n1 - 1, f.tc = 0;

			Assert( v0 <= g_numverts && v1 <= g_numverts && v2 <= g_numverts );
			Assert( n0 <= g_numnormals && n1 <= g_numnormals && n2 <= g_numnormals );

			g_face[i] = f;

			if (j == 12)
			{
				i = g_numfaces++;
				f.a = v0 - 1; f.na = n0 - 1, f.ta = 0;
				f.b = v3 - 1; f.nb = n3 - 1, f.tb = 0;
				f.c = v2 - 1; f.nc = n2 - 1, f.tc = 0;
				g_face[i] = f;
			}
		}
	}


	UnifyIndices( psource );

	s_sourceanim_t *pSourceAnim = FindOrAddSourceAnim( psource, "BindPose" );
	if ( frame == 0 )
	{
		psource->numbones = 1;
		V_strcpy_safe( psource->localBone[0].name, "default" );
		psource->localBone[0].parent = -1;
		pSourceAnim->numframes = 1;
		pSourceAnim->startframe = 0;
		pSourceAnim->endframe = 0;
		pSourceAnim->rawanim[0] = (s_bone_t *)kalloc( 1, sizeof( s_bone_t ) );
		pSourceAnim->rawanim[0][0].pos.Init();
		pSourceAnim->rawanim[0][0].rot = RadianEuler( M_PI_F / 2, 0.0f, 0.0f );
		Build_Reference( psource, "BindPose" );

		BuildIndividualMeshes( psource );
	}

	// printf("%d %d : %d\n", g_numverts, g_numnormals, numvlist );

	int t = frame;
	int count = numvlist;

	pSourceAnim->numvanims[t] = count;
	pSourceAnim->vanim[t] = (s_vertanim_t *)kalloc( count, sizeof( s_vertanim_t ) );
	for (i = 0; i < count; i++)
	{
		pSourceAnim->vanim[t][i].vertex = i;
		pSourceAnim->vanim[t][i].pos = g_vertex[v_listdata[i].v];
		pSourceAnim->vanim[t][i].normal = g_normal[v_listdata[i].n];
	}

	fclose( g_fpInput );

	return 1;
}
