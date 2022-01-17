#pragma once

#define NUMBER_OF_ELEMENTS(X) (sizeof X / sizeof X[0])


#include <vector>

#include "mpastream.h"
#include "tag.h"

class CTags
{
public:
	CTags(CMPAStream* pStream);
	~CTags(void);

	CTag* GetNextTag(unsigned int& nIndex) const;
	template <class TagClass> bool FindTag(TagClass*& pTag) const;

	// get begin offset after prepended tags
	DWORD GetBegin() const { return m_dwBegin; };
	// get end offset before appended tags
	DWORD GetEnd() const { return m_dwEnd; };

	
private:
	bool FindAppendedTag(CMPAStream* pStream);
	bool FindPrependedTag(CMPAStream* pStream);

	// definition of function pointer type
	typedef CTag* (*FindTagFunctionPtr) (CMPAStream*, bool, DWORD, DWORD);
	bool FindTag(FindTagFunctionPtr pFindTag, CMPAStream* pStream, bool bAppended);

	std::vector <CTag*> m_Tags;
	DWORD m_dwBegin, m_dwEnd;
	static const FindTagFunctionPtr m_appendedTagFactories[];
	static const FindTagFunctionPtr m_prependedTagFactories[];
};


// you need to compile with runtime information to use this method
template <class TagClass>
bool CTags::FindTag(TagClass*& pTag) const
{
	for (unsigned int nIndex = 0; nIndex < m_Tags.size(); nIndex++)
	{
		pTag = dynamic_cast<TagClass*>(m_Tags[nIndex]);
		if (pTag)
			return true;
	}
	return false;
}
