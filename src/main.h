// Drive Dot Shield
// Copyright (c) 2012-2023 Henry++

#pragma once

#include "routine.h"

#include "resource.h"
#include "app.h"

typedef struct _DRIVE_LOCK
{
	PR_STRING label;
	PR_STRING file_system;

	LARGE_INTEGER total_space;
	LARGE_INTEGER free_space;

	HANDLE hdrive;

	ULONG serial_number;
} DRIVE_LOCK;

typedef enum _DRIVE_STATUS
{
	DS_NORMAL,
	DS_PROTECTED,
	DS_INFECTED, // maybe
	DS_LOCKED
} DRIVE_STATUS;
