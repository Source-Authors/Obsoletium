// Copyright Valve Corporation, All rights reserved.
//
//

#ifndef SRC_UTILS_SHADERCOMPILE_CFGPROCESSOR_H_
#define SRC_UTILS_SHADERCOMPILE_CFGPROCESSOR_H_

#include <cstdio>   // FILE
#include <cstdint>  // uint64_t
#include <memory>   // std::unique_ptr

#include "tier1/smartptr.h"

// clang-format off
// 
// Layout of the internal structures is as follows:
// 
// 
// |-------- shader1.fxc ---------||--- shader2.fxc ---||--------- shader3.fxc -----||-...
// | 0 s s 3 s s s s 8 s 10 s s s || s s 2 3 4 s s s 8 || 0 s s s 4 s s s 8 9 s s s ||-...
// | 0 1 2 3 4 5 6 7 8 9 10 * * *   14 * * * * *20 * *   23 * * *27 * * * * * * *35    * * *
// 
// GetSection( 10 ) -> shader1.fxc
// GetSection( 27 ) -> shader3.fxc
// 
// GetNextCombo(  3,  3, 14 ) -> shader1.fxc : ( riCommandNumber =  8, rhCombo =  "8" )
// GetNextCombo( 10, 10, 14 ) ->   NULL      : ( riCommandNumber = 14, rhCombo = NULL )
// GetNextCombo( 22,  8, 36 ) -> shader3.fxc : ( riCommandNumber = 23, rhCombo =  "0" )
// GetNextCombo( 29, -1, 36 ) -> shader3.fxc : ( riCommandNumber = 31, rhCombo =  "8" )
//
// clang-format on

class CUtlInplaceBuffer;

namespace se::shader_compile::shader_combo_processor {

// Working with configuration
void ReadConfiguration(FILE *fInputStream);
void ReadConfiguration(CUtlInplaceBuffer *fInputStream);

struct CfgEntryInfo {
  char const *m_szName;  // Name of the shader, e.g. "shader_ps20b"
  // Name of the src file, e.g. "shader_psxx.fxc"
  char const *m_szShaderFileName;

  uint64_t m_numCombos;         // Total possible num of combos, e.g. 1024
  uint64_t m_numDynamicCombos;  // Num of dynamic combos, e.g. 4
  uint64_t m_numStaticCombos;   // Num of static combos, e.g. 256
  uint64_t m_iCommandStart;     // Start command, e.g. 0
  uint64_t m_iCommandEnd;       // End command, e.g. 1024
};

void DescribeConfiguration(std::unique_ptr<CfgEntryInfo[]> &rarrEntries);

// Working with combos
using ComboHandle = struct {
} *;

ComboHandle Combo_GetCombo(uint64_t command_no);
ComboHandle Combo_GetNext(uint64_t &command_no, ComboHandle &rhCombo,
                          uint64_t iCommandEnd);
void Combo_FormatCommand(ComboHandle combo, char *buffer);
uint64_t Combo_GetCommandNum(ComboHandle combo);
uint64_t Combo_GetComboNum(ComboHandle combo);
CfgEntryInfo const *Combo_GetEntryInfo(ComboHandle combo);

ComboHandle Combo_Alloc(ComboHandle copy_from);
void Combo_Assign(ComboHandle combo_dst, ComboHandle combo_src);
void Combo_Free(ComboHandle &combo_free);

};  // namespace se::shader_compile::shader_combo_processor

#endif  //  !SRC_UTILS_SHADERCOMPILE_CFGPROCESSOR_H_
