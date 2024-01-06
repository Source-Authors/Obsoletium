// Copyright Valve Corporation, All rights reserved.

#include "vpc.h"

#include <algorithm>

#include "tier0/memdbgon.h"

void CVPC::SetupDefaultConditionals() {
  //
  // PLATFORM Conditionals
  //
  {
    FindOrCreateConditional("WIN32", true, CONDITIONAL_PLATFORM);
    FindOrCreateConditional("WIN64", true, CONDITIONAL_PLATFORM);

    // LINUX is the platform but the VPC scripts use $LINUX and $DEDICATED
    // (which we automatically create later).
    FindOrCreateConditional("LINUX32", true, CONDITIONAL_PLATFORM);
    FindOrCreateConditional("LINUX64", true, CONDITIONAL_PLATFORM);

    FindOrCreateConditional("OSX32", true, CONDITIONAL_PLATFORM);
    FindOrCreateConditional("OSX64", true, CONDITIONAL_PLATFORM);

    FindOrCreateConditional("X360", true, CONDITIONAL_PLATFORM);
    FindOrCreateConditional("PS3", true, CONDITIONAL_PLATFORM);
  }

  //
  // CUSTOM conditionals
  //
  {
    // setup default custom conditionals
    FindOrCreateConditional("PROFILE", true, CONDITIONAL_CUSTOM);
    FindOrCreateConditional("RETAIL", true, CONDITIONAL_CUSTOM);
    FindOrCreateConditional("CALLCAP", true, CONDITIONAL_CUSTOM);
    FindOrCreateConditional("FASTCAP", true, CONDITIONAL_CUSTOM);
    FindOrCreateConditional("CERT", true, CONDITIONAL_CUSTOM);
    FindOrCreateConditional("MEMTEST", true, CONDITIONAL_CUSTOM);
    FindOrCreateConditional("NOFPO", true, CONDITIONAL_CUSTOM);
    FindOrCreateConditional("POSIX", true, CONDITIONAL_CUSTOM);
    FindOrCreateConditional("LV", true, CONDITIONAL_CUSTOM);
    FindOrCreateConditional("DEMO", true, CONDITIONAL_CUSTOM);
    FindOrCreateConditional("NO_STEAM", true, CONDITIONAL_CUSTOM);
    FindOrCreateConditional("DVDEMU", true, CONDITIONAL_CUSTOM);
    FindOrCreateConditional("QTDEBUG", true, CONDITIONAL_CUSTOM);
    FindOrCreateConditional("NO_CEG", true, CONDITIONAL_CUSTOM);
    FindOrCreateConditional("UPLOAD_CEG", true, CONDITIONAL_CUSTOM);
  }
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
const char *CVPC::GetTargetPlatformName() {
  const auto *c =
      std::find_if(std::begin(m_Conditionals), std::end(m_Conditionals),
                   [](const conditional_t &c) noexcept {
                     return c.type == CONDITIONAL_PLATFORM && c.m_bDefined;
                   });
  if (c != std::end(m_Conditionals)) return c->name.String();

  // fatal - should have already been default set
  Assert(0);
  VPCError("Unspecified platform.");
}

//-----------------------------------------------------------------------------
//	Case Insensitive. Returns true if platform conditional has been marked
//	as defined.
//-----------------------------------------------------------------------------
bool CVPC::IsPlatformDefined(const char *name) {
  const auto *c =
      std::find_if(std::begin(m_Conditionals), std::end(m_Conditionals),
                   [name](const conditional_t &c) noexcept {
                     return c.type == CONDITIONAL_PLATFORM &&
                            !V_stricmp(name, c.name.String()) && c.m_bDefined;
                   });
  return c != std::end(m_Conditionals);
}

//-----------------------------------------------------------------------------
//	Case Insensitive
//-----------------------------------------------------------------------------
conditional_t *CVPC::FindOrCreateConditional(const char *name,
                                             bool should_create,
                                             conditionalType_e type) {
  auto *c = std::find_if(std::begin(m_Conditionals), std::end(m_Conditionals),
                         [name](const conditional_t &c) noexcept {
                           return !V_stricmp(name, c.name.String());
                         });
  if (c != std::end(m_Conditionals)) return c;

  if (!should_create) return nullptr;

  intp index = m_Conditionals.AddToTail();

  char tmp_name[256];
  V_strncpy(tmp_name, name, sizeof(tmp_name));

  // primary internal use as lower case, but spewed to user as upper for style
  // consistency
  auto &cd = m_Conditionals[index];
  cd.name = V_strlower(tmp_name);
  cd.upperCaseName = V_strupr(tmp_name);
  cd.type = type;
  return &cd;
}

void CVPC::SetConditional(const char *value, bool should_set) {
  VPCStatus(false, "Set Conditional: $%s = %s", value,
            (should_set ? "1" : "0"));

  conditional_t *c{FindOrCreateConditional(value, true, CONDITIONAL_CUSTOM)};
  if (!c) {
    VPCError("Failed to find or create $%s conditional", value);
  }

  c->m_bDefined = should_set;
}

//-----------------------------------------------------------------------------
//	Returns true if string has a conditional of the specified type
//-----------------------------------------------------------------------------
bool CVPC::ConditionHasDefinedType(const char *condition,
                                   conditionalType_e type) {
  char symbol[MAX_SYSTOKENCHARS];

  for (auto &&c : m_Conditionals) {
    if (c.type != type) continue;

    sprintf(symbol, "$%s", c.name.String());

    if (V_stristr(condition, symbol)) {
      // a define of expected type occurs in the conditional expression
      return true;
    }
  }

  return false;
}

//-----------------------------------------------------------------------------
//	Callback for expression evaluator.
//-----------------------------------------------------------------------------
bool CVPC::ResolveConditionalSymbol(const char *symbol) {
  if (!V_stricmp(symbol, "$0") || !V_stricmp(symbol, "0")) {
    return false;
  }

  if (!V_stricmp(symbol, "$1") || !V_stricmp(symbol, "1")) {
    return true;
  }

  const int offset{symbol[0] == '$' ? 1 : 0};
  const conditional_t *c{
      FindOrCreateConditional(symbol + offset, false, CONDITIONAL_NULL)};
  if (c) {
    // game conditionals only resolve true when they are 'defined' and 'active'
    // only one game conditional is expected to be active at a time
    if (c->type == CONDITIONAL_GAME) {
      if (!c->m_bDefined) return false;

      return c->m_bGameConditionActive;
    }

    // all other type of conditions are gated by their 'defined' state
    return c->m_bDefined;
  }

  // unknown conditional, defaults to false
  return false;
}

//-----------------------------------------------------------------------------
//	Callback for expression evaluator.
//-----------------------------------------------------------------------------
static bool ResolveSymbol(const char *symbol) {
  return g_pVPC->ResolveConditionalSymbol(symbol);
}

//-----------------------------------------------------------------------------
//	Callback for expression evaluator.
//-----------------------------------------------------------------------------
static void SymbolSyntaxError(const char *reason) {
  // invoke internal syntax error hndling which spews script stack as well
  g_pVPC->VPCSyntaxError(reason);
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
bool CVPC::EvaluateConditionalExpression(const char *expression) {
  char buffer[MAX_SYSTOKENCHARS];
  ResolveMacrosInConditional(expression, buffer, sizeof(buffer));

  if (!buffer[0]) {
    // empty string, same as not having a conditional
    return true;
  }

  bool rc{false};

  CExpressionEvaluator expression_eval;
  const bool is_valid{expression_eval.Evaluate(rc, buffer, ::ResolveSymbol,
                                               ::SymbolSyntaxError)};
  if (!is_valid) {
    g_pVPC->VPCSyntaxError("VPC Conditional Evaluation Error");
  }

  return rc;
}
