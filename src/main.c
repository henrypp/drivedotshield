// Drive Dot Shield
// Copyright (c) 2012-2023 Henry++


#include "routine.h"

#include <windows.h>
#include <commctrl.h>
#include <wininet.h>
#include <shlobj.h>
#include <winioctl.h>

#include "app.h"
#include "rapp.h"
#include "main.h"

#include "resource.h"

DRIVE_LOCK dl[26] = {0};

SIZE_T GetDriveNumber (
	_In_ LPCWSTR drive
)
{
	return (SIZE_T)(drive[0] - 65);
}

BOOLEAN DriveIsLocked (
	_In_ LPCWSTR drive
)
{
	SIZE_T drive_number;

	drive_number = GetDriveNumber (drive);

	if (dl[drive_number].hdrive)
		return TRUE;

	return FALSE;
}

BOOLEAN DriveIsReady (
	_In_ LPCWSTR drive
)
{
	WCHAR buffer[64];
	UINT error_mode;
	BOOL volume_info;

	if (DriveIsLocked (drive))
		return TRUE;

	_r_str_printf (buffer, RTL_NUMBER_OF (buffer), L"%c:\\", drive[0]);

	error_mode = SetErrorMode (SEM_FAILCRITICALERRORS);
	volume_info = GetVolumeInformation (buffer, NULL, 0, NULL, NULL, NULL, NULL, 0);
	SetErrorMode (error_mode);

	return volume_info;
}

DRIVE_STATUS GetDriveStatus (
	_In_ LPCWSTR drive,
	_Out_writes_z_ (buffer_size) LPWSTR status,
	_In_ _In_range_ (1, PR_SIZE_MAX_STRING_LENGTH) SIZE_T buffer_size
)
{
	WCHAR buffer[64];

	if (DriveIsLocked (drive))
	{
		_r_str_copy (status, buffer_size, L"Locked");

		return DS_LOCKED;
	}

	_r_str_printf (buffer, RTL_NUMBER_OF (buffer), L"%c:\\autorun.inf", drive[0]);

	if (_r_fs_exists (buffer))
	{
		if (GetFileAttributes (buffer) & FILE_ATTRIBUTE_DIRECTORY)
		{
			_r_str_copy (status, buffer_size, L"Protected");

			return DS_PROTECTED;
		}
		else
		{
			_r_str_copy (status, buffer_size, L"Infected");

			return DS_INFECTED;
		}
	}

	_r_str_copy (status, buffer_size, L"Normal");

	return DS_NORMAL;
}

INT GetDriveImage (
	_In_ DRIVE_STATUS status
)
{
	switch (status)
	{
		case DS_PROTECTED:
		{
			return 1;
		}

		case DS_INFECTED:
		{
			return 2;
		}

		case DS_LOCKED:
		{
			return 3;
		}
	}

	return 0; // normal
}

VOID RefreshDrivesList (
	_In_ HWND hwnd
)
{
	WCHAR buffer[128];
	DWORD drive_count;
	WCHAR drive[8];
	WCHAR  label[MAX_PATH];
	WCHAR  file_system[MAX_PATH];
	SIZE_T drive_number;
	DRIVE_STATUS status;
	INT protected_count = 0;
	INT infected_count = 0;
	INT locked_count = 0;
	INT image_id;

	_r_listview_deleteallitems (hwnd, IDC_DRIVES);

	drive_count = GetLogicalDrives ();

	for (INT i = 0, j = 0; i < 26; i++)
	{
		if (!((drive_count >> i) & 0x00000001))
			continue;

		_r_str_printf (drive, RTL_NUMBER_OF (drive), L"%c:", (65 + i));

		if (!DriveIsReady (drive))
			continue;

		drive_number = GetDriveNumber (drive);

		status = GetDriveStatus (drive, buffer, RTL_NUMBER_OF (buffer));

		image_id = GetDriveImage (status);

		_r_listview_additem_ex (hwnd, IDC_DRIVES, j, drive, image_id, I_GROUPIDNONE, 0);

		drive[2] = L'\\';
		drive[3] = L'\0';

		if (!DriveIsLocked (drive))
		{
			GetVolumeInformation (
				drive,
				label,
				RTL_NUMBER_OF (label),
				NULL,
				NULL,
				NULL,
				file_system,
				RTL_NUMBER_OF (file_system)
			);
		}
		else
		{
			_r_str_copy (label, RTL_NUMBER_OF (label), L"");
			_r_str_copy (file_system, RTL_NUMBER_OF (file_system), L"");
		}

		_r_listview_setitem (hwnd, IDC_DRIVES, j, 1, DriveIsLocked (drive) ? dl[drive_number].label : label);
		_r_listview_setitem (hwnd, IDC_DRIVES, j, 3, DriveIsLocked (drive) ? dl[drive_number].file_system : file_system);

		switch (GetDriveType (drive))
		{
			case DRIVE_REMOVABLE:
			{
				_r_listview_setitem (hwnd, IDC_DRIVES, j, 2, L"Removable");
				break;
			}

			case DRIVE_FIXED:
			{
				_r_listview_setitem (hwnd, IDC_DRIVES, j, 2, L"Local");
				break;
			}

			case DRIVE_REMOTE:
			{
				_r_listview_setitem (hwnd, IDC_DRIVES, j, 2, L"Network");
				break;
			}

			case DRIVE_CDROM:
			{
				_r_listview_setitem (hwnd, IDC_DRIVES, j, 2, L"CD-ROM");
				break;
			}

			default:
			{
				_r_listview_setitem (hwnd, IDC_DRIVES, j, 2, L"Unknown");
				break;
			}
		}

		_r_listview_setitem (hwnd, IDC_DRIVES, j, 4, buffer);

		switch (status)
		{
			case DS_PROTECTED:
			{
				protected_count += 1;

				break;
			}

			case DS_INFECTED:
			{
				infected_count += 1;

				break;
			}

			case DS_LOCKED:
			{
				locked_count += 1;

				break;
			}

			default:
			{
				break;
			}
		}

		j += 1;
	}

	_r_str_printf (buffer, RTL_NUMBER_OF (buffer), L"Total drives: %i\0", _r_listview_getitemcount (hwnd, IDC_DRIVES));
	_r_status_settext (hwnd, IDC_STATUSBAR, 0, buffer);

	_r_str_printf (buffer, RTL_NUMBER_OF (buffer), L"Protected: %i\0", protected_count);
	_r_status_settext (hwnd, IDC_STATUSBAR, 1, buffer);

	_r_str_printf (buffer, RTL_NUMBER_OF (buffer), L"Infected: %i\0", infected_count);
	_r_status_settext (hwnd, IDC_STATUSBAR, 2, buffer);

	_r_str_printf (buffer, RTL_NUMBER_OF (buffer), L"Locked: %i\0", locked_count);
	_r_status_settext (hwnd, IDC_STATUSBAR, 3, buffer);
}

VOID UnprotectDrive (
	_In_ LPCWSTR drive
)
{
	WCHAR autorun_file[64];
	WCHAR buffer[128];
	WIN32_FIND_DATA wfd;
	HANDLE hfind;

	_r_str_printf (autorun_file, RTL_NUMBER_OF (autorun_file), L"%c:\\autorun.inf", drive[0]);

	SetFileAttributes (autorun_file, FILE_ATTRIBUTE_NORMAL);

	if (GetFileAttributes (autorun_file) & FILE_ATTRIBUTE_DIRECTORY)
	{
		_r_str_printf (buffer, RTL_NUMBER_OF (buffer), L"\\\\?\\%c:\\autorun.inf\\*", drive[0]);

		hfind = FindFirstFile (buffer, &wfd);

		if (hfind != INVALID_HANDLE_VALUE)
		{
			do
			{
				if (_r_str_compare (wfd.cFileName, L".") == 0 || _r_str_compare (wfd.cFileName, L"..") == 0)
					continue;

				_r_str_printf (buffer, RTL_NUMBER_OF (buffer), L"\\\\?\\%s\\%s", autorun_file, wfd.cFileName);

				SetFileAttributes (buffer, FILE_ATTRIBUTE_NORMAL);

				if (wfd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
				{
					RemoveDirectory (buffer);
				}
				else
				{
					DeleteFile (buffer);
				}
			}
			while (FindNextFile (hfind, &wfd));

			FindClose (hfind);
		}

		RemoveDirectory (autorun_file);
	}
	else
	{
		DeleteFile (autorun_file);
	}
}

VOID ProtectDrive (
	_In_ LPCWSTR drive
)
{
	HANDLE hfile;
	WCHAR buffer[64];
	WCHAR random[9];

	UnprotectDrive (drive);

	_r_str_printf (buffer, RTL_NUMBER_OF (buffer), L"\\\\?\\%c:\\autorun.inf", drive[0]);

	CreateDirectory (buffer, NULL);
	SetFileAttributes (buffer, FILE_ATTRIBUTE_HIDDEN | FILE_ATTRIBUTE_READONLY | FILE_ATTRIBUTE_SYSTEM);

	_r_str_generaterandom (random, RTL_NUMBER_OF (random), TRUE);

	_r_str_printf (buffer, RTL_NUMBER_OF (buffer), L"\\\\?\\%c:\\autorun.inf\\%s.", drive[0], random);

	hfile = CreateFile (
		buffer,
		GENERIC_WRITE | GENERIC_READ,
		FILE_SHARE_WRITE | FILE_SHARE_READ,
		NULL,
		CREATE_ALWAYS,
		FILE_ATTRIBUTE_HIDDEN | FILE_ATTRIBUTE_READONLY | FILE_ATTRIBUTE_SYSTEM,
		NULL
	);

	if (hfile != INVALID_HANDLE_VALUE)
		CloseHandle (hfile);
}

BOOLEAN LockDrive (
	_In_ LPCWSTR drive
)
{
	HANDLE hfile;
	WCHAR buffer[64];
	DWORD locked;
	SIZE_T drive_number;
	BOOLEAN result;

	_r_str_printf (buffer, RTL_NUMBER_OF (buffer), L"\\\\?\\%c:", drive[0]);

	hfile = CreateFile (buffer, GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, NULL);

	if (hfile == INVALID_HANDLE_VALUE)
		return FALSE;

	drive_number = GetDriveNumber (drive);

	_r_str_printf (buffer, RTL_NUMBER_OF (buffer), L"%c:\\", drive[0]);

	GetVolumeInformation (
		buffer,
		dl[drive_number].label,
		RTL_NUMBER_OF (dl[drive_number].label),
		&dl[drive_number].serial_number,
		NULL,
		NULL,
		dl[drive_number].file_system,
		RTL_NUMBER_OF (dl[drive_number].file_system)
	);

	GetDiskFreeSpaceEx (buffer, 0, &dl[drive_number].total_space, &dl[drive_number].free_space);

	if (DeviceIoControl (hfile, FSCTL_LOCK_VOLUME, NULL, 0, NULL, 0, &locked, NULL))
	{
		result = TRUE;
		dl[drive_number].hdrive = hfile;
	}
	else
	{
		result = FALSE;
		CloseHandle (hfile);
	}

	return result;
}

BOOLEAN UnlockDrive (
	_In_ LPCWSTR drive
)
{
	DWORD locked;
	SIZE_T drive_number;

	drive_number = GetDriveNumber (drive);

	if (!dl[drive_number].hdrive)
		return FALSE;

	DeviceIoControl (dl[drive_number].hdrive, FSCTL_UNLOCK_VOLUME, NULL, 0, NULL, 0, &locked, NULL);
	CloseHandle (dl[drive_number].hdrive);

	dl[drive_number].hdrive = NULL;

	return TRUE;
}

VOID UnlockAllDrives (
	_In_ HWND hwnd
)
{
	WCHAR buffer[64];

	for (INT i = 0; i < 26; i++)
	{
		if (!dl[i].hdrive)
			continue;

		if (MessageBox (hwnd, L"Are you sure you want to unlock all drives?", APP_NAME, MB_ICONINFORMATION | MB_YESNO) != IDYES)
			break;

		for (i = 0; i < 26; i++)
		{
			_r_str_printf (buffer, RTL_NUMBER_OF (buffer), L"%c:\\", i + 65);
			UnlockDrive (buffer);
		}

		break;
	}

	RefreshDrivesList (hwnd);
}

BOOLEAN EjectDrive (
	_In_ LPCWSTR drive
)
{
	WCHAR buffer[64];
	DWORD dio;
	HANDLE hfile;
	BOOLEAN result;

	_r_str_printf (buffer, RTL_NUMBER_OF (buffer), L"\\\\?\\%c:", drive[0]);

	hfile = CreateFile (
		buffer,
		GENERIC_READ | GENERIC_WRITE,
		FILE_SHARE_READ | FILE_SHARE_WRITE,
		NULL,
		OPEN_EXISTING,
		FILE_FLAG_WRITE_THROUGH,
		NULL
	);

	if (hfile == INVALID_HANDLE_VALUE)
		return FALSE;

	FlushFileBuffers (hfile);

	if (DeviceIoControl (hfile, IOCTL_DISK_EJECT_MEDIA, NULL, 0, NULL, 0, &dio, NULL))
	{
		result = TRUE;
	}
	else
	{
		result = FALSE;
	}

	CloseHandle (hfile);

	return result;
}

VOID RefreshDriveInfo (
	_In_ HWND hwnd,
	_In_ PR_STRING drive
)
{
	WCHAR buffer[64];
	WCHAR status[64];
	WCHAR label[128];
	WCHAR file_system[128];
	ULARGE_INTEGER total_space;
	ULARGE_INTEGER free_space;
	SIZE_T drive_number;
	DWORD serial_number;

	if (!DriveIsReady (drive->buffer))
	{
		for (INT i = 0; i < _r_listview_getitemcount (hwnd, IDC_PROPERTIES); i++)
			_r_listview_setitem (hwnd, IDC_PROPERTIES, i, 1, i ? L"n/a" : drive->buffer);

		return;
	}

	drive_number = GetDriveNumber (drive->buffer);

	_r_listview_setitem (hwnd, IDC_PROPERTIES, 0, 1, drive->buffer);

	GetDriveStatus (drive->buffer, status, RTL_NUMBER_OF (status));

	_r_listview_setitem (hwnd, IDC_PROPERTIES, 1, 1, status);

	if (!DriveIsLocked (drive->buffer))
	{
		GetVolumeInformation (
			drive->buffer,
			label,
			RTL_NUMBER_OF (label),
			&serial_number,
			NULL,
			NULL,
			file_system,
			RTL_NUMBER_OF (file_system)
		);

		GetDiskFreeSpaceEx (drive->buffer, 0, &total_space, &free_space);
	}
	else
	{
		_r_str_copy (
			label,
			RTL_NUMBER_OF (label),
			L"unknown"
		);

		_r_str_copy (
			file_system,
			RTL_NUMBER_OF (file_system),
			L"unknown"
		);

		serial_number = dl[drive_number].serial_number;

		total_space = dl[drive_number].total_space;
		free_space = dl[drive_number].free_space;
	}

	_r_listview_setitem (
		hwnd,
		IDC_PROPERTIES,
		2,
		1,
		DriveIsLocked (drive->buffer) ? dl[drive_number].label : label
	);

	switch (GetDriveType (drive->buffer))
	{
		case DRIVE_REMOVABLE:
		{
			_r_listview_setitem (hwnd, IDC_PROPERTIES, 3, 1, L"Removable");
			break;
		}

		case DRIVE_FIXED:
		{
			_r_listview_setitem (hwnd, IDC_PROPERTIES, 3, 1, L"Local");
			break;
		}

		case DRIVE_REMOTE:
		{
			_r_listview_setitem (hwnd, IDC_PROPERTIES, 3, 1, L"Network");
			break;
		}

		case DRIVE_CDROM:
		{
			_r_listview_setitem (hwnd, IDC_PROPERTIES, 3, 1, L"CD-ROM");
			break;
		}

		default:
		{
			_r_listview_setitem (hwnd, IDC_PROPERTIES, 3, 1, L"Unknown");
			break;
		}
	}

	_r_listview_setitem (
		hwnd,
		IDC_PROPERTIES,
		4,
		1,
		DriveIsLocked (drive->buffer) ? dl[drive_number].file_system : file_system
	);

	_r_str_printf (buffer, RTL_NUMBER_OF (buffer), L"%04X-%04X", HIWORD (serial_number), LOWORD (serial_number));

	_r_listview_setitem (hwnd, IDC_PROPERTIES, 5, 1, buffer);

	_r_format_bytesize64 (buffer, RTL_NUMBER_OF (buffer), free_space.QuadPart);

	_r_listview_setitem (
		hwnd,
		IDC_PROPERTIES,
		6,
		1,
		buffer
	);

	_r_format_bytesize64 (buffer, RTL_NUMBER_OF (buffer), total_space.QuadPart - free_space.QuadPart);

	_r_listview_setitem (
		hwnd,
		IDC_PROPERTIES,
		7,
		1,
		buffer
	);


	_r_format_bytesize64 (buffer, RTL_NUMBER_OF (buffer), total_space.QuadPart);

	_r_listview_setitem (
		hwnd,
		IDC_PROPERTIES,
		8,
		1,
		buffer
	);
}

INT_PTR CALLBACK PropertiesDlgProc (
	_In_ HWND hwnd,
	_In_ UINT msg,
	_In_ WPARAM wparam,
	_In_ LPARAM lparam
)
{
	static PR_STRING drive = NULL;

	switch (msg)
	{
		case WM_INITDIALOG:
		{
			RECT rect;

			_r_wnd_center (hwnd, NULL);

			_r_listview_setstyle (hwnd, IDC_PROPERTIES, LVS_EX_FULLROWSELECT | LVS_EX_INFOTIP | LVS_EX_DOUBLEBUFFER, TRUE);

			GetClientRect (GetDlgItem (hwnd, IDC_PROPERTIES), &rect);

			for (INT i = 0; i < 2; i++)
				_r_listview_addcolumn (hwnd, IDC_PROPERTIES, i, NULL, rect.right / 2, 0);

			_r_listview_addgroup (hwnd, IDC_PROPERTIES, 0, L"General", 0, LVGS_COLLAPSIBLE, LVGS_COLLAPSIBLE);
			_r_listview_addgroup (hwnd, IDC_PROPERTIES, 1, L"Space", 0, LVGS_COLLAPSIBLE, LVGS_COLLAPSIBLE);

			_r_listview_additem_ex (hwnd, IDC_PROPERTIES, 0, L"Drive", I_IMAGENONE, 0, 0);
			_r_listview_additem_ex (hwnd, IDC_PROPERTIES, 1, L"Status", I_IMAGENONE, 0, 0);
			_r_listview_additem_ex (hwnd, IDC_PROPERTIES, 2, L"Label", I_IMAGENONE, 0, 0);
			_r_listview_additem_ex (hwnd, IDC_PROPERTIES, 3, L"Type", I_IMAGENONE, 0, 0);
			_r_listview_additem_ex (hwnd, IDC_PROPERTIES, 4, L"Filesystem", I_IMAGENONE, 0, 0);
			_r_listview_additem_ex (hwnd, IDC_PROPERTIES, 5, L"Serial number", I_IMAGENONE, 0, 0);

			_r_listview_additem_ex (hwnd, IDC_PROPERTIES, 6, L"Free space", I_IMAGENONE, 1, 0);
			_r_listview_additem_ex (hwnd, IDC_PROPERTIES, 7, L"Used space", I_IMAGENONE, 1, 0);
			_r_listview_additem_ex (hwnd, IDC_PROPERTIES, 8, L"Total space", I_IMAGENONE, 1, 0);

			drive = (PR_STRING)lparam;

			RefreshDriveInfo (hwnd, drive);

			break;
		}

		case WM_CLOSE:
		{
			if (drive)
				_r_obj_dereference (drive);

			EndDialog (hwnd, FALSE);

			break;
		}

		case WM_CONTEXTMENU:
		{
			HMENU hmenu;
			HMENU hsubmenu;

			if (GetDlgCtrlID ((HWND)wparam) != IDC_PROPERTIES)
				break;

			hmenu = LoadMenu (NULL, MAKEINTRESOURCE (IDM_PROPERTIES));

			if (!hmenu)
				break;

			hsubmenu = GetSubMenu (hmenu, 0);

			if (!hsubmenu)
			{
				DestroyMenu (hmenu);
				break;
			}

			if (!_r_listview_getselectedcount (hwnd, IDC_PROPERTIES))
				_r_menu_enableitem (hsubmenu, IDM_REFRESH, MF_BYCOMMAND, FALSE);

			_r_menu_popup (
				hsubmenu,
				hwnd,
				NULL,
				TRUE
			);

			DestroyMenu (hmenu);

			break;
		}

		case WM_PAINT:
		{
			PAINTSTRUCT ps = {0};
			HDC hdc;

			hdc = BeginPaint (hwnd, &ps);;

			_r_dc_drawwindow (hdc, hwnd, 0, TRUE);

			EndPaint (hwnd, &ps);

			return 0;
		}

		case WM_CTLCOLORSTATIC:
		case WM_CTLCOLORDLG:
		{
			return (INT_PTR)GetSysColorBrush (COLOR_WINDOW);
		}

		case WM_COMMAND:
		{
			INT ctrl_id = LOWORD (wparam);

			switch (ctrl_id)
			{
				case IDCANCEL: // process Esc key
				case IDC_OK:
				{
					EndDialog (hwnd, FALSE);
					break;
				}

				case IDM_REFRESH:
				{
					RefreshDriveInfo (hwnd, drive);
					break;
				}

				case IDM_COPY:
				{
					PR_STRING text;
					INT item;

					item = _r_listview_getnextselected (hwnd, IDC_PROPERTIES, -1);

					if (item == -1)
						break;

					text = _r_listview_getitemtext (hwnd, IDC_PROPERTIES, item, 0);

					if (!text)
						break;

					_r_clipboard_set (hwnd, &text->sr);

					_r_obj_dereference (text);

					break;
				}
			}

			break;
		}
	}

	return 0;
}

// mode:
//    0 - check "NoDriveTypeAutoRun" values
//    1 - check "IniFileMapping" values
//
// is_switch:
//    0 - check "NoDriveTypeAutoRun" values
//    1 - check "IniFileMapping" values
//
// return values:
//    0 - autorun is enabled
//    1 - autorun is disabled (system protected)

//BOOLEAN AutorunRegistryCheck (
//	_In_ INT mode,
//	_In_ BOOLEAN is_protect,
//	_In_ BOOLEAN is_switch
//)
//{
//	WCHAR buffer[128];
//	HKEY hkey;
//	HKEY hroots[] = {HKEY_LOCAL_MACHINE, HKEY_CURRENT_USER};
//	DWORD size = 0;
//	DWORD value = 0;
//	LSTATUS status;
//
//	if (mode == 0)
//	{
//		for (INT i = 0; i < (sizeof (hroots) / sizeof (hroots[0])); i++)
//		{
//			status = RegOpenKeyEx (
//				hroots[i],
//				L"Software\\Microsoft\\Windows\\CurrentVersion\\Policies\\Explorer",
//				0,
//				KEY_ALL_ACCESS,
//				&hkey
//			);
//
//			if (status == ERROR_SUCCESS)
//			{
//				if (is_protect)
//				{
//					value = is_switch ? 145 : 255;
//					size = sizeof (value);
//
//					RegSetValueEx (hkey, L"NoDriveTypeAutoRun", 0, REG_DWORD, (LPBYTE)&value, size);
//					RegCloseKey (hkey);
//				}
//				else
//				{
//					RegQueryValueEx (hkey, L"NoDriveTypeAutoRun", 0, &size, (LPBYTE)&value, &size);
//					RegCloseKey (hkey);
//
//					if (value != 255)
//						return FALSE;
//				}
//			}
//		}
//	}
//	else if (mode == 1)
//	{
//		status = RegOpenKeyEx (
//			HKEY_LOCAL_MACHINE,
//			L"SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion\\IniFileMapping\\Autorun.inf",
//			0,
//			KEY_ALL_ACCESS,
//			&hkey
//		);
//
//		if (status == ERROR_SUCCESS)
//		{
//			if (is_protect)
//			{
//				if (is_switch)
//				{
//					_r_str_copy (buffer, RTL_NUMBER_OF (buffer), L"@SYS:DoesNotExist");
//					RegSetValueEx (hkey, NULL, 0, REG_DWORD, (LPBYTE)buffer, RTL_NUMBER_OF (buffer));
//				}
//				else
//				{
//					RegDeleteValue (hkey, NULL);
//				}
//
//				RegCloseKey (hkey);
//			}
//			else
//			{
//				size = RTL_NUMBER_OF (buffer);
//
//				RegQueryValueEx (hkey, NULL, 0, &size, (LPBYTE)&buffer, &size);
//				RegCloseKey (hkey);
//
//				if (_r_str_compare (buffer, L"@SYS:DoesNotExist") == 0)
//					return FALSE;
//			}
//		}
//	}
//
//	return TRUE;
//}

LRESULT CALLBACK DlgProc
(
	_In_ HWND hwnd,
	_In_ UINT msg,
	_In_ WPARAM wparam,
	_In_ LPARAM lparam
)
{
	static R_LAYOUT_MANAGER layout_manager = {0};

	switch (msg)
	{
		case WM_INITDIALOG:
		{
			HIMAGELIST himglist;
			HICON hicon;
			HMENU hmenu;
			LONG dpi_value;
			INT parts[4] = {0};

			_r_layout_initializemanager (&layout_manager, hwnd);

			_r_listview_setstyle (hwnd, IDC_DRIVES, LVS_EX_FULLROWSELECT | LVS_EX_INFOTIP | LVS_EX_DOUBLEBUFFER, FALSE);

			dpi_value = _r_dc_getwindowdpi (hwnd);

			_r_listview_addcolumn (hwnd, IDC_DRIVES, 0, L"Drive", _r_dc_getdpi (50, dpi_value), 0);
			_r_listview_addcolumn (hwnd, IDC_DRIVES, 1, L"Label", _r_dc_getdpi (90, dpi_value), 0);
			_r_listview_addcolumn (hwnd, IDC_DRIVES, 2, L"Type", _r_dc_getdpi (90, dpi_value), 0);
			_r_listview_addcolumn (hwnd, IDC_DRIVES, 3, L"Filesystem", _r_dc_getdpi (90, dpi_value), 0);
			_r_listview_addcolumn (hwnd, IDC_DRIVES, 4, L"Status", _r_dc_getdpi (85, dpi_value), 0);

			parts[0] = _r_dc_getdpi (120, dpi_value);
			parts[1] = _r_dc_getdpi (240, dpi_value);
			parts[2] = _r_dc_getdpi (360, dpi_value);
			parts[3] = -1;

			_r_status_setparts (hwnd, IDC_STATUSBAR, parts, RTL_NUMBER_OF (parts));

			himglist = ImageList_Create (
				_r_dc_getsystemmetrics (SM_CXSMICON, dpi_value),
				_r_dc_getsystemmetrics (SM_CYSMICON, dpi_value),
				ILC_COLOR32,
				0,
				5
			);

			for (INT i = IDI_NORMAL, j = 0; i < (IDI_LOCKED + 1); i++, j++)
			{
				hicon = LoadIcon (GetModuleHandle (NULL), MAKEINTRESOURCE (i));
				ImageList_AddIcon (himglist, hicon);
				SendDlgItemMessage (hwnd, IDC_STATUSBAR, SB_SETICON, j, (LPARAM)ImageList_GetIcon (himglist, j, ILD_NORMAL));
				DestroyIcon (hicon);
			}

			hmenu = GetMenu (hwnd);

			if (hmenu)
			{
				_r_menu_checkitem (
					hmenu,
					IDM_ALWAYSONTOP_CHK,
					0,
					MF_BYCOMMAND,
					_r_config_getboolean (L"AlwaysOnTop", FALSE)
				);

				_r_menu_checkitem (
					hmenu,
					IDM_CHECKUPDATES_CHK,
					0,
					MF_BYCOMMAND,
					_r_update_isenabled (FALSE)
				);
			}

			SendDlgItemMessage (hwnd, IDC_DRIVES, LVM_SETIMAGELIST, LVSIL_SMALL, (LPARAM)himglist);

			RefreshDrivesList (hwnd);

			break;
		}

		case WM_SIZE:
		{
			_r_layout_resize (&layout_manager, wparam);
			break;
		}

		case WM_DEVICECHANGE:
		{
			RefreshDrivesList (hwnd);
			break;
		}

		case WM_CLOSE:
		{
			UnlockAllDrives (hwnd);

			DestroyWindow (hwnd);
			PostQuitMessage (0);

			break;
		}

		case WM_CONTEXTMENU:
		{
			HMENU hmenu;
			HMENU hsubmenu;

			if (GetDlgCtrlID ((HWND)wparam) != IDC_DRIVES)
				break;

			hmenu = LoadMenu (NULL, MAKEINTRESOURCE (IDM_DRIVES));
			hsubmenu = GetSubMenu (hmenu, 0);

			if (_r_listview_getselectedcount (hwnd, IDC_DRIVES))
				_r_menu_popup (hsubmenu, hwnd, NULL, TRUE);

			DestroyMenu (hmenu);

			break;
		}

		case WM_NOTIFY:
		{
			LPNMHDR lpnmhdr;

			lpnmhdr = (LPNMHDR)lparam;

			switch (lpnmhdr->code)
			{
				case NM_DBLCLK:
				{
					LPNMITEMACTIVATE lpnmia;

					if (lpnmhdr->idFrom != IDC_DRIVES)
						break;

					lpnmia = (LPNMITEMACTIVATE)lparam;

					if (lpnmia->iItem != -1)
						SendMessage (hwnd, WM_COMMAND, MAKELPARAM (IDM_OPEN, 0), 0);

					break;
				}
			}

			break;
		}

		case WM_COMMAND:
		{
			INT ctrl_id = LOWORD (wparam);

			switch (ctrl_id)
			{
				case IDCANCEL: // process Esc key
				case IDM_EXIT:
				{
					PostMessage (hwnd, WM_CLOSE, 0, 0);
					break;
				}

				case IDM_ALWAYSONTOP_CHK:
				{
					BOOLEAN new_val;

					new_val = !_r_config_getboolean (L"AlwaysOnTop", FALSE);

					_r_menu_checkitem (GetMenu (hwnd), ctrl_id, 0, MF_BYCOMMAND, new_val);
					_r_config_setboolean (L"AlwaysOnTop", new_val);

					_r_wnd_top (hwnd, new_val);

					break;
				}

				case IDM_CHECKUPDATES_CHK:
				{
					BOOLEAN new_val;

					new_val = !_r_update_isenabled (FALSE);

					_r_menu_checkitem (GetMenu (hwnd), ctrl_id, 0, MF_BYCOMMAND, new_val);
					_r_update_enable (new_val);

					break;
				}

				case IDM_WEBSITE:
				{
					_r_shell_opendefault (_r_app_getwebsite_url ());
					break;
				}

				case IDM_CHECK_UPDATES:
				{
					_r_update_check (hwnd);
					break;
				}

				case IDM_ABOUT:
				{
					_r_show_aboutmessage (hwnd);
					break;
				}

				case IDM_OPEN:
				{
					PR_STRING text;
					INT item;

					if (!_r_listview_getselectedcount (hwnd, IDC_DRIVES))
						break;

					item = -1;

					while ((item = (INT)SendDlgItemMessage (hwnd, IDC_DRIVES, LVM_GETNEXTITEM, (WPARAM)item, LVNI_SELECTED)) >= 0)
					{
						text = _r_listview_getitemtext (hwnd, IDC_DRIVES, item, 0);

						if (!text)
							continue;

						if (!DriveIsLocked (text->buffer))
							_r_shell_showfile (text->buffer);

						_r_obj_dereference (text);
					}

					break;
				}

				case IDM_REFRESH:
				{
					RefreshDrivesList (hwnd);
					break;
				}

				case IDM_PROTECT:
				{
					PR_STRING text;
					INT item;

					if (!_r_listview_getselectedcount (hwnd, IDC_DRIVES))
						break;

					item = -1;

					while ((item = (INT)SendDlgItemMessage (hwnd, IDC_DRIVES, LVM_GETNEXTITEM, (WPARAM)item, LVNI_SELECTED)) >= 0)
					{
						text = _r_listview_getitemtext (hwnd, IDC_DRIVES, item, 0);

						if (!text)
							continue;

						ProtectDrive (text->buffer);

						_r_obj_dereference (text);
					}

					RefreshDrivesList (hwnd);

					break;
				}

				case IDM_UNPROTECT:
				{
					PR_STRING text;
					INT item;

					if (!_r_listview_getselectedcount (hwnd, IDC_DRIVES))
						break;

					item = -1;

					while ((item = (INT)SendDlgItemMessage (hwnd, IDC_DRIVES, LVM_GETNEXTITEM, (WPARAM)item, LVNI_SELECTED)) >= 0)
					{
						text = _r_listview_getitemtext (hwnd, IDC_DRIVES, item, 0);

						if (!text)
							continue;

						UnprotectDrive (text->buffer);

						_r_obj_dereference (text);
					}

					RefreshDrivesList (hwnd);

					break;
				}

				case IDM_LOCK:
				{
					PR_STRING text;
					INT item;

					if (!_r_listview_getselectedcount (hwnd, IDC_DRIVES))
						break;

					item = -1;

					while ((item = (INT)SendDlgItemMessage (hwnd, IDC_DRIVES, LVM_GETNEXTITEM, (WPARAM)item, LVNI_SELECTED)) >= 0)
					{
						text = _r_listview_getitemtext (hwnd, IDC_DRIVES, item, 0);

						if (!text)
							continue;

						LockDrive (text->buffer);

						_r_obj_dereference (text);
					}

					RefreshDrivesList (hwnd);

					break;
				}

				case IDM_UNLOCK:
				{
					PR_STRING text;
					INT item;

					if (!_r_listview_getselectedcount (hwnd, IDC_DRIVES))
						break;

					item = -1;

					while ((item = (INT)SendDlgItemMessage (hwnd, IDC_DRIVES, LVM_GETNEXTITEM, (WPARAM)item, LVNI_SELECTED)) >= 0)
					{
						text = _r_listview_getitemtext (hwnd, IDC_DRIVES, item, 0);

						if (!text)
							continue;

						UnlockDrive (text->buffer);

						_r_obj_dereference (text);
					}

					RefreshDrivesList (hwnd);

					break;
				}

				case IDM_UNLOCK_ALL:
				{
					UnlockAllDrives (hwnd);
					break;
				}

				case IDM_EJECT:
				{
					INT item;

					if (!_r_listview_getselectedcount (hwnd, IDC_DRIVES))
						break;

					item = -1;

					while ((item = (INT)SendDlgItemMessage (hwnd, IDC_DRIVES, LVM_GETNEXTITEM, (WPARAM)item, LVNI_SELECTED)) >= 0)
					{
						//buffer = Lv_GetItemText (hwnd, IDC_DRIVES, item, 0)[0];
						//errbuf.Append (EjectDrive (buffer) ? buffer + L" - диск успешно извлечён\r\n" : buffer + L" - ошибка извлечения, возможно диск используется\r\n");
					}

					//if (!errbuf.IsEmpty ())
					{
						//errbuf.Trim ();
						//MessageBox (hwnd, MB_OK | MB_ICONINFORMATION, APP_NAME, errbuf);
					}

					RefreshDrivesList (hwnd);

					break;
				}

				case IDM_FORMAT:
				{
					PR_STRING text;
					SIZE_T drive_number;
					INT item;

					if (!_r_listview_getselectedcount (hwnd, IDC_DRIVES))
						break;

					item = _r_listview_getnextselected (hwnd, IDC_DRIVES, -1);

					if (item == -1)
						break;

					text = _r_listview_getitemtext (hwnd, IDC_DRIVES, item, 0);

					if (!text)
						break;

					drive_number = GetDriveNumber (text->buffer);

					SHFormatDrive (hwnd, (INT)drive_number, SHFMT_ID_DEFAULT, 0);
					RefreshDrivesList (hwnd);

					_r_obj_dereference (text);

					break;
				}

				case IDM_PROPERTY:
				{
					PR_STRING text;
					INT item;

					if (!_r_listview_getselectedcount (hwnd, IDC_DRIVES))
						break;

					item = _r_listview_getnextselected (hwnd, IDC_DRIVES, -1);

					if (item == -1)
						break;

					text = _r_listview_getitemtext (hwnd, IDC_DRIVES, item, 0);

					if (!text)
						break;

					if (!DriveIsLocked (text->buffer))
						DialogBoxParam (NULL, MAKEINTRESOURCE (IDD_PROPERTIES), hwnd, &PropertiesDlgProc, (LPARAM)text);

					_r_obj_dereference (text);
				}

				break;
			}
		}
	}

	return 0;
}

INT APIENTRY wWinMain (
	_In_ HINSTANCE hinst,
	_In_opt_ HINSTANCE prev_hinst,
	_In_ LPWSTR cmdline,
	_In_ INT show_cmd
)
{
	HWND hwnd;

	if (!_r_app_initialize ())
		return ERROR_APP_INIT_FAILURE;

	hwnd = _r_app_createwindow (
		hinst,
		MAKEINTRESOURCE (IDD_MAIN),
		MAKEINTRESOURCE (IDI_MAIN),
		&DlgProc
	);

	if (!hwnd)
		return ERROR_APP_INIT_FAILURE;

	return _r_wnd_message_callback (hwnd, MAKEINTRESOURCE (IDA_MAIN));
}