// Copyright Valve Corporation, All rights reserved.

#ifndef VPC_MEMORY_RESERVATION_X64_H_
#define VPC_MEMORY_RESERVATION_X64_H_

namespace vpc::win {

/**
 * @brief Reserves bottom memory for x64 to catch x64 problems eariler.
 * @return true if success, false otherwise.
 */
bool ReserveBottomMemoryFor64Bit() noexcept;

}  // namespace vpc::win

#endif  // VPC_MEMORY_RESERVATION_X64_H_
