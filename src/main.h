// Drive Dot Shield
// Copyright (c) 2012-2023 Henry++

#ifndef __DRIVEDOTSHIELD_H__
#define __DRIVEDOTSHIELD_H__

#include "routine.h"

#include "resource.h"
#include "app.h"

typedef struct _DRIVE_LOCK
{
	HANDLE hdrive;

	WCHAR label[MAX_PATH];
	WCHAR file_system[MAX_PATH];

	ULARGE_INTEGER total_space;
	ULARGE_INTEGER free_space;

	ULONG serial_number;
} DRIVE_LOCK;

typedef enum _DRIVE_STATUS
{
	DS_NORMAL,
	DS_PROTECTED,
	DS_INFECTED, // maybe
	DS_LOCKED
} DRIVE_STATUS;

#define IOCTL_VOLUME_QUERY_VOLUME_NUMBER        CTL_CODE(IOCTL_VOLUME_BASE, 7, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define IOCTL_VOLUME_LOGICAL_TO_PHYSICAL        CTL_CODE(IOCTL_VOLUME_BASE, 8, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define IOCTL_VOLUME_PHYSICAL_TO_LOGICAL        CTL_CODE(IOCTL_VOLUME_BASE, 9, METHOD_BUFFERED, FILE_ANY_ACCESS)

//
// IOCTL_VOLUME_LOGICAL_TO_PHYSICAL
//
// Input Buffer:
//     Structure of type VOLUME_LOGICAL_OFFSET
//
// Output Buffer:
//     Structure of type VOLUME_PHYSICAL_OFFSETS
//

typedef struct _VOLUME_LOGICAL_OFFSET {

    //
    // Specifies the logical location
    // that needs to be translated to
    // a physical location.
    //
    LONGLONG LogicalOffset;

} VOLUME_LOGICAL_OFFSET, *PVOLUME_LOGICAL_OFFSET;

typedef struct _VOLUME_PHYSICAL_OFFSET {

    //
    // Specifies the physical location
    // that needs to be  translated to
    // a logical location.
    //
    ULONG DiskNumber;
    LONGLONG Offset;

} VOLUME_PHYSICAL_OFFSET, *PVOLUME_PHYSICAL_OFFSET;

typedef struct _VOLUME_PHYSICAL_OFFSETS {

    //
    // Specifies one or more physical
    // locations on which the logical
    // location resides.
    //
    ULONG NumberOfPhysicalOffsets;
    VOLUME_PHYSICAL_OFFSET PhysicalOffset[ANYSIZE_ARRAY];

} VOLUME_PHYSICAL_OFFSETS, *PVOLUME_PHYSICAL_OFFSETS;

#endif // __DRIVEDOTSHIELD_H__
