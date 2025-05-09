// Copyright Valve Corporation, All rights reserved.
//
// Vbspinfo is a command-line tool that dumps information and, if specified,
// lumps from a BSP map file.  It dumps information usually seen at the end of
// a Hammer compile log.
//
// See https://developer.valvesoftware.com/wiki/Vbspinfo

#include "bsplib.h"

#include <cinttypes>

#include "mathlib/mathlib.h"
#include "tier0/icommandline.h"
#include "tier1/utlstringmap.h"
#include "tier2/fileutils.h"

#include "iscratchpad3d.h"
#include "filesystem_tools.h"
#include "gamebspfile.h"
#include "tools_minidump.h"
#include "cmdlib.h"

#include "posix_file_stream.h"

#include "tier0/memdbgon.h"

extern int num_entities;
extern entity_t entities[MAX_MAP_ENTITIES];

namespace {

bool g_bTreeInfo = false;
bool g_bDrawTree = false;

float g_nOptimumDepth;
int g_nMinTreeDepth;
int g_nMaxTreeDepth;
int g_TotalTreeDepth;
float g_TotalVariance;

float g_ySpacing = -1;  // (set by code)
// dimhotepus: double -> float.
constexpr inline float g_xSpacing = 1.0f;

void CalculateTreeInfo_R(int iNode, int depth) {
  dnode_t *pNode = &dnodes[iNode];

  // (is this a leaf)
  if (iNode < 0) {
    g_nMinTreeDepth = min(g_nMinTreeDepth, depth);
    g_nMaxTreeDepth = max(g_nMaxTreeDepth, depth);
    g_TotalTreeDepth += depth;
    g_TotalVariance += fabs(depth - g_nOptimumDepth);
  } else {
    CalculateTreeInfo_R(pNode->children[0], depth + 1);
    CalculateTreeInfo_R(pNode->children[1], depth + 1);
  }
}

void DrawTreeToScratchPad_R(
    IScratchPad3D *pPad,
    int iNode,   // Which node we're drawing.
    int iLevel,  // (used to get Y coordinate)
    float flXMin, float flXMax,
    const Vector *pParentPos  // Parent node position to draw connecting line
                              // (if there is a parent).
) {
  float flMyX = (flXMin + flXMax) * 0.5f;

  Vector vMyPos;
  vMyPos.x = 0;
  vMyPos.y = flMyX;
  vMyPos.z = -iLevel * g_ySpacing;

  // Draw the connecting line.
  if (pParentPos) {
    pPad->DrawLine(CSPVert(*pParentPos, Vector(1, 1, 1)),
                   CSPVert(vMyPos, Vector(1, 0, 0)));
  }

  dnode_t *pNode = &dnodes[iNode];
  if (iNode < 0) {
    // This is a leaf.
    pPad->DrawPoint(CSPVert(vMyPos, Vector(1, 0, 0)), 6);
  } else {
    pPad->DrawPoint(CSPVert(vMyPos, Vector(1, 1, 1)), 2);

    DrawTreeToScratchPad_R(pPad, pNode->children[0], iLevel + 1, flXMin, flMyX,
                           &vMyPos);

    DrawTreeToScratchPad_R(pPad, pNode->children[1], iLevel + 1, flMyX, flXMax,
                           &vMyPos);
  }
}

void CalcTreeDepth_R(int iNode, int iLevel, int &iMaxDepth) {
  iMaxDepth = max(iLevel, iMaxDepth);

  if (iNode < 0) return;

  CalcTreeDepth_R(dnodes[iNode].children[0], iLevel + 1, iMaxDepth);
  CalcTreeDepth_R(dnodes[iNode].children[1], iLevel + 1, iMaxDepth);
}

void DrawTreeToScratchPad() {
  IScratchPad3D *pPad = ScratchPad3D_Create();
  pPad->SetAutoFlush(false);

  int maxDepth = 0;
  CalcTreeDepth_R(dmodels[0].headnode, 0, maxDepth);
  float flXSpace = (1u << min(maxDepth, 14)) * g_xSpacing;
  g_ySpacing = (flXSpace / maxDepth) / 4;

  DrawTreeToScratchPad_R(pPad, dmodels[0].headnode,
                         0,  // start on level 0
                         -flXSpace / 2, flXSpace / 2, NULL);

  pPad->Release();
}

struct WorldTextureStats_t {
  int texdataID;
  int refCount;
};

bool WorldTextureCompareFunc(const WorldTextureStats_t &pStat1,
                             const WorldTextureStats_t &pStat2) {
  if (pStat1.refCount < pStat2.refCount) return false;
  if (pStat1.refCount > pStat2.refCount) return true;

  return false;
}

void PrintWorldTextureStats(FILE *fp) {
  static WorldTextureStats_t stats[MAX_MAP_TEXDATA];
  for (int i = 0; i < numtexdata; i++) {
    stats[i].texdataID = i;
    stats[i].refCount = 0;
  }

  for (int i = 0; i < numfaces; i++) {
    dface_t *pFace = &dfaces[i];
    int texinfoID = pFace->texinfo;
    Assert(texinfoID >= 0 && texinfoID < texinfo.Count());
    int texdataID = texinfo[texinfoID].texdata;
    Assert(texdataID >= 0 && texdataID < numtexdata);
    stats[texdataID].refCount++;
  }

  std::sort(stats, stats + numtexdata, WorldTextureCompareFunc);
  for (int i = 0; i < numtexdata; i++) {
    const char *pTextureName = TexDataStringTable_GetString(
        dtexdata[stats[i].texdataID].nameStringTableID);

    fprintf(fp, "%5d surface(s) use material \"%s\"\n", stats[i].refCount,
            pTextureName);
  }
}

void PrintModelStats(FILE *fp) {
  CUtlStringMap<int> modelMap;

  // -------------------------------------------------------
  // Deal with static props
  // -------------------------------------------------------
  GameLumpHandle_t handle =
      g_GameLumps.GetGameLumpHandle(GAMELUMP_STATIC_PROPS);
  //	int nLumpSize = g_GameLumps.GameLumpSize( handle );
  void *pStaticPropLump = g_GameLumps.GetGameLump(handle);
  unsigned char *pScan = (unsigned char *)pStaticPropLump;
  //	fprintf( fp, "nLumpSize: %d\n", nLumpSize );

  // read dictionary
  int nDictCount = ((int *)pScan)[0];
  pScan += sizeof(int);
  StaticPropDictLump_t *pDictLump = (StaticPropDictLump_t *)pScan;
  pScan += nDictCount * sizeof(StaticPropDictLump_t);

  // read leaves
  int nLeafCount = ((int *)pScan)[0];
  pScan += sizeof(int);
  //	StaticPropLeafLump_t *pLeafLump = ( StaticPropLeafLump_t * )pScan;
  pScan += nLeafCount * sizeof(StaticPropLeafLump_t);

  // read objects
  int nObjCount = ((int *)pScan)[0];
  pScan += sizeof(int);
  StaticPropLump_t *pStaticPropLumpData = (StaticPropLump_t *)pScan;
  pScan += nObjCount * sizeof(StaticPropLump_t);

  for (int i = 0; i < nObjCount; i++) {
    StaticPropLump_t &pData = pStaticPropLumpData[i];
    const char *pName = pDictLump[pData.m_PropType].m_Name;
    if (modelMap.Defined(pName)) {
      modelMap[pName]++;
    } else {
      modelMap[pName] = 1;
    }
  }

  ParseEntities();

  for (int i = 0; i < num_entities; i++) {
    const entity_t *pEnt = &entities[i];
    const epair_t *pEPair = pEnt->epairs;
    const char *pClassName = NULL;
    const char *pModelName = NULL;
    for (; pEPair; pEPair = pEPair->next) {
      if (Q_stricmp(pEPair->key, "classname") == 0) {
        pClassName = pEPair->value;
      } else if (Q_stricmp(pEPair->key, "model") == 0) {
        if (StringHasPrefix(pEPair->value, "models")) {
          pModelName = pEPair->value;
        }
      }
    }
    if (pClassName && pModelName) {
      if (modelMap.Defined(pModelName)) {
        modelMap[pModelName]++;
      } else {
        modelMap[pModelName] = 1;
      }
    }
  }

  for (unsigned short i = 0; i < modelMap.GetNumStrings(); i++) {
    printf("%s,%d\n", modelMap.String(i), modelMap[modelMap.String(i)]);
  }
}

void PrintListStaticProps(FILE *fp) {
  // -------------------------------------------------------
  // Deal with static props
  // -------------------------------------------------------
  GameLumpHandle_t handle =
      g_GameLumps.GetGameLumpHandle(GAMELUMP_STATIC_PROPS);
  //	int nLumpSize = g_GameLumps.GameLumpSize( handle );
  void *pStaticPropLump = g_GameLumps.GetGameLump(handle);
  unsigned char *pScan = (unsigned char *)pStaticPropLump;
  //	fprintf( fp, "nLumpSize: %d\n", nLumpSize );

  // read dictionary
  int nDictCount = ((int *)pScan)[0];
  pScan += sizeof(int);
  StaticPropDictLump_t *pDictLump = (StaticPropDictLump_t *)pScan;
  pScan += nDictCount * sizeof(StaticPropDictLump_t);

  // read leaves
  int nLeafCount = ((int *)pScan)[0];
  pScan += sizeof(int);
  //	StaticPropLeafLump_t *pLeafLump = ( StaticPropLeafLump_t * )pScan;
  pScan += nLeafCount * sizeof(StaticPropLeafLump_t);

  // read objects
  int nObjCount = ((int *)pScan)[0];
  pScan += sizeof(int);
  StaticPropLump_t *pStaticPropLumpData = (StaticPropLump_t *)pScan;
  pScan += nObjCount * sizeof(StaticPropLump_t);

  for (int i = 0; i < nObjCount; i++) {
    StaticPropLump_t &pData = pStaticPropLumpData[i];
    const char *pName = pDictLump[pData.m_PropType].m_Name;

    printf("%03d  %s\n", i, pName);
  }
}

void PrintCommandLine(int argc, char **argv) {
  Warning("Command line: ");

  for (int z = 0; z < argc; z++) {
    Warning("\"%s\" ", argv[z]);
  }

  Warning("\n\n");
}

}  // namespace

int main(int argc, char **argv) {
  // Install an exception handler.
  const se::utils::common::ScopedDefaultMinidumpHandler
      scoped_default_minidumps;

  const ScopedCommandLineProgram scoped_command_line_program(argc, argv);

#ifdef PLATFORM_64BITS
  Msg("\nValve Software - vbspinfo [64 bit] (%s)\n", __DATE__);
#else
  Msg("\nValve Software - vbspinfo (%s)\n", __DATE__);
#endif

  char source[1024];

  bool extractlumps[HEADER_LUMPS] = {};
  bool bHaveAnyToExtract = false;

  const ScopedHDRMode scoped_hdr_mode{false};

  g_pFileSystem = g_pFullFileSystem;

  MathLib_Init(2.2f, 2.2f, 0.0f, 2, false, false, false, false);
  PrintCommandLine(argc, argv);

  if (argc == 1) {
    printf("vbspinfo:  Build " __DATE__ "\n");

    printf("usage: vbspinfo [parameters] bspfile [bspfiles]\n");
    printf("   -treeinfo            \n");
    //		printf("   -drawtree            \n"); Remove for now until the
    // option can be fixed
    printf("   -worldtexturestats   \n");
    printf("   -modelstats          \n");
    printf("   -liststaticprops     \n");
    printf(
        "   -X[lump ID]          Extract BSP lump to file. i.e -X0 extracts "
        "entity lump.\n");
    printf(
        "   -size				Show .bsp worldmodel bounds\n");
    Error("Incorrect syntax.");
  }

  bool bWorldTextureStats = false;
  bool bModelStats = false;
  bool bListStaticProps = false;
  bool bShowMapBounds = false;

  for (int i = 1; i < argc; i++) {
    if (stricmp(argv[i], "-treeinfo") == 0) {
      g_bTreeInfo = true;
      continue;
    } else if (stricmp(argv[i], "-drawtree") == 0) {
      g_bDrawTree = true;
      continue;
    } else if (stricmp(argv[i], "-worldtexturestats") == 0) {
      bWorldTextureStats = true;
      continue;
    } else if (stricmp(argv[i], "-modelstats") == 0) {
      bModelStats = true;
      continue;
    } else if (stricmp(argv[i], "-liststaticprops") == 0) {
      bListStaticProps = true;
      continue;
    } else if (stricmp(argv[i], "-steamlocal") == 0) {
      continue;
    } else if (stricmp(argv[i], "-steam") == 0) {
      continue;
    } else if (strnicmp(argv[i], "-X", 2) == 0) {
      int iLump = atoi(argv[i] + 2);
      extractlumps[iLump] = true;
      bHaveAnyToExtract = true;
      continue;
    } else if (stricmp(argv[i], "-size") == 0) {
      bShowMapBounds = true;
      continue;
    }

    if (!bWorldTextureStats && !bModelStats && !bListStaticProps) {
      printf("---------------------\n");
    }

    V_strcpy_safe(source, argv[i]);
    Q_DefaultExtension(source, ".bsp");
    V_strcpy_safe(source, ExpandPath(source));

    int64_t size = 0;
    auto [f, errc] = se::posix::posix_file_stream_factory::open(source, "rb");
    if (!errc) std::tie(size, errc) = f.size();

    if (!errc && !bWorldTextureStats && !bModelStats && !bListStaticProps) {
      Msg("reading %s (%" PRId64 " bytes).\n", source, size);
    }

    // If we're extracting, do that and quit.
    if (bHaveAnyToExtract) {
      dheader_t *bsp = OpenBSPFile(source);
      if (!bsp) return 2;

      // If the filename doesn't have a path, prepend with the current directory
      char fullbspname[MAX_PATH];
      if (!_fullpath(fullbspname, source, sizeof(fullbspname))) {
        Error("Unable to get full path from '%s'.\n", source);
        return 3;
      }

      for (auto &l : extractlumps) {
        if (l) {
          printf("Extracting lump %d.\n", l);

          WriteLumpToFile(bsp, fullbspname, l);
        }
      }

      CloseBSPFile(bsp);

      printf("Finished extraction.\n");
      return 0;
    }

    if (!LoadBSPFile(source)) return 3;

    if (bWorldTextureStats) {
      PrintWorldTextureStats(stdout);
    } else if (bModelStats) {
      PrintModelStats(stdout);
    } else if (bListStaticProps) {
      PrintListStaticProps(stdout);
    } else if (bShowMapBounds) {
      dmodel_t *world = &dmodels[0];
      printf("Full     :  (%8.3f %8.3f %8.3f) - (%8.3f %8.3f %8.3f)\n",
             world->mins.x, world->mins.y, world->mins.z, world->maxs.x,
             world->maxs.y, world->maxs.z);

      if (!num_entities) ParseEntities();

      for (int e = 0; e < num_entities; ++i) {
        const char *pEntity = ValueForKey(&entities[e], "classname");
        if (strcmp(pEntity, "worldspawn")) continue;

        Vector wmins{vec3_origin};
        Vector wmaxs{vec3_origin};

        const char *pchMins = ValueForKey(&entities[e], "world_mins");
        if (sscanf(pchMins, "%f %f %f", &wmins.x, &wmins.y, &wmins.z) != 3) {
          Warning("Missed world_mins vector3 for %s.\n", pEntity);
        }

        const char *pchMaxs = ValueForKey(&entities[e], "world_maxs");
        if (sscanf(pchMaxs, "%f %f %f", &wmaxs.x, &wmaxs.y, &wmaxs.z) != 3) {
          Warning("Missed world_maxs vector3 for %s.\n", pEntity);
        }

        printf("No Skybox:  (%8.3f %8.3f %8.3f) - (%8.3f %8.3f %8.3f)\n",
               wmins.x, wmins.y, wmins.z, wmaxs.x, wmaxs.y, wmaxs.z);
        break;
      }
    } else {
      PrintBSPFileSizes();
    }

    if (g_bTreeInfo) {
      g_nOptimumDepth = (int)(log((float)numnodes) / log(2.0f));
      g_nMinTreeDepth = std::numeric_limits<float>::max();
      g_nMaxTreeDepth = std::numeric_limits<float>::min();
      g_TotalTreeDepth = 0;
      g_TotalVariance = 0;
      CalculateTreeInfo_R(dmodels[0].headnode, 0);

      printf(
          "\n"
          "\t-------------------\n"
          "\tTREE INFO:\n"
          "\t-------------------\n"
          "\tNumber of nodes ------------------ : %d\n"
          "\tOptimum tree depth (logN) -------- : %.3f\n"
          "\tMinimum tree depth --------------- : %d\n"
          "\tMaximum tree depth --------------- : %d\n"
          "\tAverage tree depth --------------- : %.3f\n"
          "\tAverage leaf variance from optimum : %.3f\n\n",
          numnodes, g_nOptimumDepth, g_nMinTreeDepth, g_nMaxTreeDepth,
          (float)g_TotalTreeDepth / numnodes,
          (float)g_TotalVariance / numnodes);
    }

    if (g_bDrawTree) {
      DrawTreeToScratchPad();
    }

    if (!bWorldTextureStats && !bModelStats && !bListStaticProps) {
      printf("---------------------\n");
    }
  }

  return 0;
}
