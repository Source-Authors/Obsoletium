// Copyright Valve Corporation, All rights reserved.
//
// Display a pacifier like:
// ProcessBlock_Thread: 0...1...2...3...4...5...6...7...8...9... (0)

#ifndef SRC_UTILS_COMMON_PACIFIER_H_
#define SRC_UTILS_COMMON_PACIFIER_H_

// Prints the prefix and resets the pacifier
void StartPacifier(char const *prefix);

// percent value between 0 and 1.
void UpdatePacifier(float percent);

// Completes pacifier as if 100% was done
void EndPacifier(bool should_add_new_line = true);

// Suppresses pacifier updates if another thread might still be firing them.
void SuppressPacifier(bool enable = true);

#endif  // SRC_UTILS_COMMON_PACIFIER_H_
