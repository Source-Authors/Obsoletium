// Copyright Valve Corporation, All rights reserved.

#ifndef SE_PUBLIC_TIER0_IOCTLCODES_H_
#define SE_PUBLIC_TIER0_IOCTLCODES_H_

// Define the IOCTL codes we will use.  The IOCTL code contains a command
// identifier, plus other information about the device, the type of access
// with which the file must have been opened, and the type of buffering.

// Device type           -- in the "User Defined" range."
#define DEVICE_FILE_TYPE 40000
#define SE_METHOD_BUFFERED 0
#define SE_FILE_READ_ACCESS (0x0001)  // file & pipe

// The IOCTL function codes from 0x800 to 0xFFF are for customer use.
//
// Macro definition for defining IOCTL and FSCTL function control codes.  Note
// that function codes 0-2047 are reserved for Microsoft Corporation, and
// 2048-4095 are reserved for customers.
//

#define SE_CTL_CODE(DeviceType, Function, Method, Access) \
  (((DeviceType) << 16) | ((Access) << 14) | ((Function) << 2) | (Method))

#define IOCTL_WRITE_MSR \
    SE_CTL_CODE( DEVICE_FILE_TYPE, 0x900, SE_METHOD_BUFFERED, SE_FILE_READ_ACCESS )

#define IOCTL_READ_MSR \
    SE_CTL_CODE( DEVICE_FILE_TYPE, 0x901, SE_METHOD_BUFFERED, SE_FILE_READ_ACCESS )

#endif  // !SE_PUBLIC_TIER0_IOCTLCODES_H_
