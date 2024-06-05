// Copyright Valve Corporation, All rights reserved.
//
// Purpose:	CValObject is used for tracking individual objects that report
// in to CValidator.  Whenever a new object reports in (via CValidator::Push),
// we create a new CValObject to aggregate stats for it.

#ifndef TIER0_VALOBJECT_H_
#define TIER0_VALOBJECT_H_

#ifdef DBGFLAG_VALIDATE
class CValObject
{
public:
	// Constructors & destructors
	CValObject() { };
	~CValObject();

	void Init( tchar *pchType, void *pvObj, tchar *pchName, CValObject *pValObjectParent,
		CValObject *pValObjectPrev );

	// Our object has claimed ownership of a memory block
	void ClaimMemoryBlock( void *pvMem );

	// A child of ours has claimed ownership of a memory block
	void ClaimChildMemoryBlock( int cubUser );

	// Accessors
	tchar *PchType() { return m_rgchType; };
	void *PvObj() { return m_pvObj; };
	tchar *PchName() { return m_rgchName; };
	CValObject *PValObjectParent() { return m_pValObjectParent; };
	int NLevel() { return m_nLevel; };
	CValObject *PValObjectNext() { return m_pValObjectNext; };
	int CpubMemSelf() { return m_cpubMemSelf; };
	int CubMemSelf() { return m_cubMemSelf; };
	int CpubMemTree() { return m_cpubMemTree; };
	int CubMemTree() { return m_cubMemTree; };
	int NUser() { return m_nUser; };
	void SetNUser( int nUser ) { m_nUser = nUser; };
	void SetBNewSinceSnapshot( bool bNewSinceSnapshot ) { m_bNewSinceSnapshot = bNewSinceSnapshot; }
	bool BNewSinceSnapshot() { return m_bNewSinceSnapshot; }

private:
	bool  m_bNewSinceSnapshot;				// If this block is new since the snapshot.
	tchar m_rgchType[64];					// Type of the object we represent
	tchar m_rgchName[64];					// Name of this particular object
	void *m_pvObj;							// Pointer to the object we represent

	CValObject *m_pValObjectParent;			// Our parent object in the tree.
	int m_nLevel;							// Our depth in the tree

	CValObject *m_pValObjectNext;			// Next ValObject in the linked list

	int m_cpubMemSelf;						// # of memory blocks we own directly
	int m_cubMemSelf;						// Total size of the memory blocks we own directly

	int m_cpubMemTree;						// # of memory blocks owned by us and our children
	int m_cubMemTree;						// Total size of the memory blocks owned by us and our children

	int m_nUser;							// Field provided for use by our users
};
#endif  // DBGFLAG_VALIDATE


#endif  // TIER0_VALOBJECT_H_
