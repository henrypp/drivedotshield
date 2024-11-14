// Drive Dot Shield
// Copyright (c) 2012-2025 Henry++

#pragma once

#include "routine.h"

#include "resource.h"
#include "app.h"

#define LANG_MENU 4

typedef enum _DRIVE_STATUS
{
	DS_NORMAL,
	DS_PROTECTED,
	DS_INFECTED, // maybe
	DS_LOCKED
} DRIVE_STATUS;

typedef struct _DRIVE_LOCK
{
	PR_STRING file_system;
	PR_STRING label;

	LARGE_INTEGER total_space;
	LARGE_INTEGER free_space;

	HANDLE hdrive;

	ULONG serial_number;
} DRIVE_LOCK;

typedef struct _ITEM_CONTEXT
{
	DRIVE_STATUS drive_status;
	PR_STRING file_system;
	PR_STRING drive;
	PR_STRING label;
	ULONG drive_type;
	LONG icon_id;
} ITEM_CONTEXT, *PITEM_CONTEXT;
