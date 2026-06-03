//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Add a specially formatted string to each debug library of the form 
//          "%LIBNAME%.lib is built debug!". We can search for this string via
//			a Perforce trigger to ensure that debug LIBs are not checked in.
//
//			Also added a global int that is emitted only in release builds so
//			that release DLLs and EXEs can ensure that they are linking to 
//			release versions of LIBs at link time. The global int adhere to the
//			naming convention "%LIBNAME%_lib_is_a_release_build".
//
//			To take advantage of this pattern a DLL or EXE should add code 
//			at global scope similar to the following in one of its modules:
//
//			#if !defined(_DEBUG)
//			extern int bitmap_lib_is_a_release_build;
//
//			void DebugTestFunction()
//			{
//				bitmap_lib_is_a_release_build = 0;
//			}
//			#endif
//
//=============================================================================//

#if defined(DEBUG) || defined(_DEBUG)
#define SE_DEBUGONLYSTRING_WORKER(x) #x
#define SE_DEBUGONLYSTRING(x) SE_DEBUGONLYSTRING_WORKER(x)
[[maybe_unused]] static volatile char const pDebugString[] = SE_DEBUGONLYSTRING(LIBNAME) ".lib is built debug!";
#else
#define SE_RELONLYINT_WORKER(x) int x ## _lib_is_a_release_build = 0 
#define SE_RELONLYINT(x) SE_RELONLYINT_WORKER(x)
SE_RELONLYINT(LIBNAME);
#endif
