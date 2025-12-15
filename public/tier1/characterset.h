//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Shared code for parsing / searching for characters in a string
//			using lookup tables

#ifndef CHARACTERSET_H
#define CHARACTERSET_H

#include <limits>
#include <string_view>

struct characterset_t
{
	using char_type = unsigned char;

	// +1 for [0, max()] range.
	char set[std::numeric_limits<char_type>::max() + 1];

	constexpr characterset_t() noexcept : set{'\0', '\0'} {}

	explicit constexpr characterset_t(std::string_view char_set) noexcept
		: characterset_t{}
	{
		for (auto ch : char_set)
		{
			set[static_cast<char_type>(ch)] = true;
		}
	 }

	[[nodiscard]] constexpr bool HasChar(const char ch) const noexcept
	{
		return set[static_cast<char_type>(ch)];
	}
};


// This is essentially a strpbrk() using a precalculated lookup table
//-----------------------------------------------------------------------------
// Purpose: builds a simple lookup table of a group of important characters
// Input  : *pSetBuffer - pointer to the buffer for the group
//			*pSetString - list of characters to flag
//-----------------------------------------------------------------------------
[[deprecated("Use characterset_t ctor")]] extern void CharacterSetBuild( characterset_t *pSetBuffer, const char *pSetString );


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *pSetBuffer - pre-build group buffer
//			character - character to lookup
// Output : int - 1 if the character was in the set
//-----------------------------------------------------------------------------
[[deprecated("Use characterset_t::HasChar")]] [[nodiscard]] constexpr inline int IN_CHARACTERSET( const characterset_t& SetBuffer, char character )
{
  return SetBuffer.set[static_cast<unsigned char>(character)];
}


#endif // CHARACTERSET_H
