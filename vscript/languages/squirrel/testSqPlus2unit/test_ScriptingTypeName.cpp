#include "testEnv.hpp"

namespace Scripting {
class ScriptEntity {
public:
    ScriptEntity() {
        Bind();
    }
    static void Bind(void) {
        SqPlus::SQClassDef<ScriptEntity>(_SC("ScriptEntity"))
            .var(&ScriptEntity::m_strName, _SC("name"))
            ;
    }
    SqPlus::ScriptStringVar64 m_strName;
};
}
DECLARE_INSTANCE_TYPE_NAME(Scripting::ScriptEntity, ScriptEntity);


SQPLUS_TEST(Test_ScriptingTypeName)
{
    SQPLUS_TEST_TRACE();
    
    // === BEGIN from a forum post by jkleinecke. 8/23/06 jcs ===

    Scripting::ScriptEntity entity ;
    SqPlus::BindVariable(&entity, _SC("instance"));
      
    RUN_SCRIPT(_SC("\n\
instance.name = \"Testing an instance variable bind: member assignment.\";\n\
print(instance.name);\n\
"));
    CHECK_EQUAL((const SQChar*)(entity.m_strName),
                _SC("Testing an instance variable bind: member assignment."));
    // === END from a forum post by jkleinecke. 8/23/06 jcs ===
}


