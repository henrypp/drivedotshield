// Drive Dot Shield
// Copyright (c) 2012-2023 Henry++


#include "routine.h"

#include "app.h"
#include "rapp.h"
#include "main.h"

#include "resource.h"

DRIVE_LOCK dl[PR_DEVICE_COUNT] = {0};

ULONG_PTR _app_getdrivenumber (
	_In_ LPCWSTR drive
)
{
	return (ULONG_PTR)(drive[0] - 65);
}

BOOLEAN _app_driveislocked (
	_In_ LPCWSTR drive
)
{
	ULONG_PTR i;

	i = _app_getdrivenumber (drive);

	if (dl[i].hdrive)
		return TRUE;

	return FALSE;
}

BOOLEAN _app_driveisready (
	_In_ LPCWSTR drive
)
{
	WCHAR buffer[64];
	ULONG old_error_mode = 0;
	ULONG error_mode;
	NTSTATUS status;

	if (_app_driveislocked (drive))
		return TRUE;

	_r_str_printf (buffer, RTL_NUMBER_OF (buffer), L"%C:\\", drive[0]);

	NtQueryInformationProcess (NtCurrentProcess (), ProcessDefaultHardErrorMode, &old_error_mode, sizeof (old_error_mode), NULL);

	error_mode = SEM_FAILCRITICALERRORS;
	NtSetInformationProcess (NtCurrentProcess (), ProcessDefaultHardErrorMode, &error_mode, sizeof (error_mode));

	status = _r_fs_getdiskinformation (NULL, drive, NULL, NULL, NULL, NULL);

	NtSetInformationProcess (NtCurrentProcess (), ProcessDefaultHardErrorMode, &old_error_mode, sizeof (old_error_mode));

	return NT_SUCCESS (status);
}

DRIVE_STATUS _app_getdrivestatus (
	_In_ LPCWSTR drive,
	_Out_writes_z_ (buffer_length) LPWSTR buffer,
	_In_ _In_range_ (1, PR_SIZE_MAX_STRING_LENGTH) ULONG_PTR buffer_length
)
{
	WCHAR path[64];
	ULONG attributes;

	if (_app_driveislocked (drive))
	{
		_r_str_copy (buffer, buffer_length, L"Locked");

		return DS_LOCKED;
	}

	_r_str_printf (path, RTL_NUMBER_OF (path), L"%C:\\autorun.inf", drive[0]);

	if (_r_fs_exists (path))
	{
		_r_fs_getattributes (path, &attributes);

		if (attributes & FILE_ATTRIBUTE_DIRECTORY)
		{
			_r_str_copy (buffer, buffer_length, L"Protected");

			return DS_PROTECTED;
		}
		else
		{
			_r_str_copy (buffer, buffer_length, L"Infected");

			return DS_INFECTED;
		}
	}

	_r_str_copy (buffer, buffer_length, L"Normal");

	return DS_NORMAL;
}

INT _app_getdriveimage (
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

VOID _app_refreshdrives (
	_In_ HWND hwnd
)
{
	WCHAR buffer[128];
	WCHAR drive[8] = {0};
	PR_STRING label = NULL;
	PR_STRING file_system = NULL;
	ULONG_PTR drive_number;
	ULONG drives;
	LONG dpi_value;
	DRIVE_STATUS status;
	INT protected_count = 0;
	INT infected_count = 0;
	INT locked_count = 0;
	INT parts[4] = {0};
	INT image_id;

	dpi_value = _r_dc_getwindowdpi (hwnd);

	parts[0] = _r_dc_getdpi (110, dpi_value);
	parts[1] = _r_dc_getdpi (210, dpi_value);
	parts[2] = _r_dc_getdpi (350, dpi_value);
	parts[3] = -1;

	_r_status_setparts (hwnd, IDC_STATUSBAR, parts, RTL_NUMBER_OF (parts));

	_r_listview_setcolumn (hwnd, IDC_DRIVES, 0, NULL, _r_dc_getdpi (50, dpi_value));
	_r_listview_setcolumn (hwnd, IDC_DRIVES, 1, NULL, _r_dc_getdpi (90, dpi_value));
	_r_listview_setcolumn (hwnd, IDC_DRIVES, 2, NULL, _r_dc_getdpi (90, dpi_value));
	_r_listview_setcolumn (hwnd, IDC_DRIVES, 3, NULL, _r_dc_getdpi (90, dpi_value));
	_r_listview_setcolumn (hwnd, IDC_DRIVES, 4, NULL, _r_dc_getdpi (85, dpi_value));

	_r_listview_deleteallitems (hwnd, IDC_DRIVES);

	_r_fs_getdisklist (&drives);

	for (INT i = 0, j = 0; i < PR_DEVICE_COUNT; i++)
	{
		if (!((drives >> i) & 0x00000001))
			continue;

		_r_str_printf (drive, RTL_NUMBER_OF (drive), L"%C:", (65 + i));

		if (!_app_driveisready (drive))
			continue;

		drive_number = _app_getdrivenumber (drive);

		status = _app_getdrivestatus (drive, buffer, RTL_NUMBER_OF (buffer));

		image_id = _app_getdriveimage (status);

		_r_listview_additem_ex (hwnd, IDC_DRIVES, j, drive, image_id, I_GROUPIDNONE, 0);

		drive[2] = L'\\';

		if (!_app_driveislocked (drive))
		{
			_r_fs_getdiskinformation (NULL, drive, &label, &file_system, NULL, &dl[drive_number].serial_number);
		}
		else
		{
			label = _r_obj_referenceemptystring ();
			file_system = _r_obj_referenceemptystring ();
		}

		_r_listview_setitem (hwnd, IDC_DRIVES, j, 1, _app_driveislocked (drive) ? dl[drive_number].label->buffer : _r_obj_getstringordefault (label, L"<empty>"));
		_r_listview_setitem (hwnd, IDC_DRIVES, j, 3, _app_driveislocked (drive) ? dl[drive_number].file_system->buffer : file_system->buffer);

		switch (GetDriveTypeW (drive))
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
		}

		j += 1;
	}

	_r_status_settextformat (hwnd, IDC_STATUSBAR, 0, L"Total drives: %d", _r_listview_getitemcount (hwnd, IDC_DRIVES));
	_r_status_settextformat (hwnd, IDC_STATUSBAR, 1, L"Protected: %d", protected_count);
	_r_status_settextformat (hwnd, IDC_STATUSBAR, 2, L"Infected: %d", infected_count);
	_r_status_settextformat (hwnd, IDC_STATUSBAR, 3, L"Locked: %d", locked_count);

	if (label)
		_r_obj_dereference (label);

	if (file_system)
		_r_obj_dereference (file_system);
}

NTSTATUS _app_protectdrive (
	_In_ HWND hwnd,
	_In_ LPCWSTR drive
)
{
	HANDLE hfile;
	WCHAR buffer[64];
	WCHAR random[9];
	NTSTATUS status;

	_r_str_printf (buffer, RTL_NUMBER_OF (buffer), L"\\\\.\\%C:\\autorun.inf", drive[0]);

	status = _r_fs_createdirectory (buffer, FILE_ATTRIBUTE_HIDDEN | FILE_ATTRIBUTE_READONLY | FILE_ATTRIBUTE_SYSTEM);

	if (!NT_SUCCESS (status) && status != STATUS_OBJECT_NAME_COLLISION)
		return status;

	_r_str_generaterandom (random, RTL_NUMBER_OF (random), TRUE);

	_r_str_printf (buffer, RTL_NUMBER_OF (buffer), L"\\\\.\\%C:\\autorun.inf\\%s.", drive[0], random);

	status = _r_fs_createfile (
		buffer,
		FILE_OVERWRITE_IF,
		GENERIC_WRITE,
		FILE_SHARE_WRITE,
		FILE_ATTRIBUTE_HIDDEN | FILE_ATTRIBUTE_READONLY | FILE_ATTRIBUTE_SYSTEM,
		0,
		FALSE,
		NULL,
		&hfile
	);

	if (NT_SUCCESS (status))
		NtClose (hfile);

	return status;
}

NTSTATUS _app_unprotectdrive (
	_In_ HWND hwnd,
	_In_ LPCWSTR drive
)
{
	WCHAR autorun_file[64];
	ULONG attributes;
	NTSTATUS status;

	_r_str_printf (autorun_file, RTL_NUMBER_OF (autorun_file), L"%C:\\autorun.inf", drive[0]);

	_r_fs_setattributes (autorun_file, NULL, FILE_ATTRIBUTE_NORMAL);

	status = _r_fs_getattributes (autorun_file, &attributes);

	if (NT_SUCCESS (status))
	{
		if (attributes & FILE_ATTRIBUTE_DIRECTORY)
		{
			status = _r_fs_deletedirectory (autorun_file, TRUE);
		}
		else
		{
			status = _r_fs_deletefile (autorun_file, NULL);
		}
	}

	if (status == STATUS_OBJECT_NAME_NOT_FOUND)
		status = STATUS_SUCCESS;

	return status;
}

NTSTATUS _app_lockdrive (
	_In_ LPCWSTR drive
)
{
	WCHAR buffer[64];
	HANDLE hdevice;
	ULONG_PTR i;
	NTSTATUS status;

	_r_str_printf (buffer, RTL_NUMBER_OF (buffer), L"\\\\.\\%C:", drive[0]);

	status = _r_fs_openfile (buffer, FILE_GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, 0, FALSE, &hdevice);

	if (!NT_SUCCESS (status))
		return status;

	i = _app_getdrivenumber (drive);

	_r_fs_getdiskinformation (hdevice, NULL, &dl[i].label, &dl[i].file_system, NULL, &dl[i].serial_number);

	_r_fs_getdiskspace (hdevice, NULL, &dl[i].free_space, &dl[i].total_space);

	status = _r_fs_deviceiocontrol (hdevice, FSCTL_LOCK_VOLUME, NULL, 0, NULL, 0, NULL);

	if (NT_SUCCESS (status))
	{
		dl[i].hdrive = hdevice;
	}
	else
	{
		NtClose (hdevice);
	}

	return status;
}

NTSTATUS _app_unlockdrive (
	_In_ LPCWSTR drive
)
{
	ULONG_PTR drive_number;
	NTSTATUS status;

	drive_number = _app_getdrivenumber (drive);

	if (!dl[drive_number].hdrive)
		return STATUS_INVALID_HANDLE;

	status = _r_fs_deviceiocontrol (dl[drive_number].hdrive, FSCTL_UNLOCK_VOLUME, NULL, 0, NULL, 0, NULL);

	NtClose (dl[drive_number].hdrive);

	dl[drive_number].hdrive = NULL;

	return status;
}

VOID _app_unlockalldrives (
	_In_ HWND hwnd
)
{
	WCHAR buffer[64];

	for (INT i = 0; i < PR_DEVICE_COUNT; i++)
	{
		if (!dl[i].hdrive)
			continue;

		_r_str_printf (buffer, RTL_NUMBER_OF (buffer), L"%C:\\", i + 65);

		_app_unlockdrive (buffer);

		break;
	}

	_app_refreshdrives (hwnd);
}

NTSTATUS _app_ejectdrive (
	_In_ LPCWSTR drive
)
{
	WCHAR buffer[64];
	HANDLE hdevice;
	NTSTATUS status;

	_r_str_printf (buffer, RTL_NUMBER_OF (buffer), L"\\\\.\\%C:", drive[0]);

	status = _r_fs_openfile (buffer, FILE_GENERIC_READ | FILE_GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, FILE_WRITE_THROUGH, FALSE, &hdevice);

	if (!NT_SUCCESS (status))
		return status;

	_r_fs_flushfile (hdevice);

	status = _r_fs_deviceiocontrol (hdevice, IOCTL_DISK_EJECT_MEDIA, NULL, 0, NULL, 0, NULL);

	NtClose (hdevice);

	return status;
}

VOID _app_refreshdriveinfo (
	_In_ HWND hwnd,
	_In_ PR_STRING drive
)
{
	WCHAR buffer[64];
	WCHAR status[64];
	PR_STRING label;
	PR_STRING file_system;
	LARGE_INTEGER total_space;
	LARGE_INTEGER free_space;
	ULONG_PTR drive_number;
	ULONG serial_number;

	if (!_app_driveisready (drive->buffer))
	{
		for (INT i = 0; i < _r_listview_getitemcount (hwnd, IDC_PROPERTIES); i++)
			_r_listview_setitem (hwnd, IDC_PROPERTIES, i, 1, i ? L"n/a" : drive->buffer);

		return;
	}

	drive_number = _app_getdrivenumber (drive->buffer);

	_r_listview_setitem (hwnd, IDC_PROPERTIES, 0, 1, drive->buffer);

	_app_getdrivestatus (drive->buffer, status, RTL_NUMBER_OF (status));

	_r_listview_setitem (hwnd, IDC_PROPERTIES, 1, 1, status);

	if (!_app_driveislocked (drive->buffer))
	{
		_r_fs_getdiskinformation (NULL, drive->buffer, &label, &file_system, NULL, &serial_number);

		_r_fs_getdiskspace (NULL, drive->buffer, &free_space, &total_space);
	}
	else
	{
		label = _r_obj_createstring (L"unknown");
		file_system = _r_obj_reference (label);

		serial_number = dl[drive_number].serial_number;

		total_space = dl[drive_number].total_space;
		free_space = dl[drive_number].free_space;
	}

	_r_listview_setitem (hwnd, IDC_PROPERTIES, 2, 1, _app_driveislocked (drive->buffer) ? dl[drive_number].label->buffer : label->buffer);

	switch (GetDriveTypeW (drive->buffer))
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

	_r_listview_setitem (hwnd, IDC_PROPERTIES, 4, 1, _app_driveislocked (drive->buffer) ? dl[drive_number].file_system->buffer : _r_obj_getstringordefault (file_system, L"<unknown>"));
	_r_str_printf (buffer, RTL_NUMBER_OF (buffer), L"%04X-%04X", HIWORD (serial_number), LOWORD (serial_number));

	_r_listview_setitem (hwnd, IDC_PROPERTIES, 5, 1, buffer);

	_r_format_bytesize64 (buffer, RTL_NUMBER_OF (buffer), free_space.QuadPart);
	_r_listview_setitem (hwnd, IDC_PROPERTIES, 6, 1, buffer);

	_r_format_bytesize64 (buffer, RTL_NUMBER_OF (buffer), total_space.QuadPart - free_space.QuadPart);
	_r_listview_setitem (hwnd, IDC_PROPERTIES, 7, 1, buffer);

	_r_format_bytesize64 (buffer, RTL_NUMBER_OF (buffer), total_space.QuadPart);
	_r_listview_setitem (hwnd, IDC_PROPERTIES, 8, 1, buffer);

	if (label)
		_r_obj_dereference (label);

	if (file_system)
		_r_obj_dereference (file_system);
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

			_r_wnd_center (hwnd, GetParent (hwnd));

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

			_app_refreshdriveinfo (hwnd, drive);

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

			hmenu = LoadMenuW (NULL, MAKEINTRESOURCEW (IDM_PROPERTIES));

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

			_r_menu_popup (hsubmenu, hwnd, NULL, TRUE);

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
					_app_refreshdriveinfo (hwnd, drive);
					break;
				}

				case IDM_COPY:
				{
					R_STRINGBUILDER sb;
					PR_STRING string;
					INT item_id = -1;

					_r_obj_initializestringbuilder (&sb, 256);

					while ((item_id = _r_listview_getnextselected (hwnd, IDC_DRIVES, item_id)) != -1)
					{
						string = _r_listview_getitemtext (hwnd, IDC_PROPERTIES, item_id, 0);

						if (!string)
							continue;

						_r_obj_appendstringbuilder2 (&sb, string);

						_r_obj_dereference (string);

						_r_obj_appendstringbuilder (&sb, L" = ");

						string = _r_listview_getitemtext (hwnd, IDC_PROPERTIES, item_id, 1);

						if (!string)
							continue;

						_r_obj_appendstringbuilder2 (&sb, string);

						_r_obj_appendstringbuilder (&sb, L"\r\n");
					}

					string = _r_obj_finalstringbuilder (&sb);

					_r_clipboard_set (hwnd, &string->sr);

					_r_obj_deletestringbuilder (&sb);

					break;
				}
			}

			break;
		}
	}

	return 0;
}

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
			LONG width;
			LONG dpi_value;

			_r_listview_setstyle (hwnd, IDC_DRIVES, LVS_EX_FULLROWSELECT | LVS_EX_INFOTIP | LVS_EX_DOUBLEBUFFER, FALSE);

			dpi_value = _r_dc_getwindowdpi (hwnd);

			_r_listview_addcolumn (hwnd, IDC_DRIVES, 0, L"Drive", _r_dc_getdpi (50, dpi_value), 0);
			_r_listview_addcolumn (hwnd, IDC_DRIVES, 1, L"Label", _r_dc_getdpi (90, dpi_value), 0);
			_r_listview_addcolumn (hwnd, IDC_DRIVES, 2, L"Type", _r_dc_getdpi (90, dpi_value), 0);
			_r_listview_addcolumn (hwnd, IDC_DRIVES, 3, L"Filesystem", _r_dc_getdpi (90, dpi_value), 0);
			_r_listview_addcolumn (hwnd, IDC_DRIVES, 4, L"Status", _r_dc_getdpi (85, dpi_value), 0);

			width = _r_dc_getsystemmetrics (SM_CXSMICON, dpi_value);

			himglist = ImageList_Create (width, width, ILC_COLOR32, 0, 5);

			for (INT i = IDI_NORMAL, j = 0; i < (IDI_LOCKED + 1); i++, j++)
			{
				_r_sys_loadicon (_r_sys_getimagebase (), MAKEINTRESOURCEW (i), width, &hicon);

				ImageList_ReplaceIcon (himglist, -1, hicon);

				_r_wnd_sendmessage (hwnd, IDC_STATUSBAR, SB_SETICON, j, (LPARAM)ImageList_GetIcon (himglist, j, ILD_NORMAL));

				DestroyIcon (hicon);
			}

			hmenu = GetMenu (hwnd);

			if (hmenu)
			{
				_r_menu_checkitem (hmenu, IDM_ALWAYSONTOP_CHK, 0, MF_BYCOMMAND, _r_config_getboolean (L"AlwaysOnTop", FALSE));
				_r_menu_checkitem (hmenu, IDM_CHECKUPDATES_CHK, 0, MF_BYCOMMAND, _r_update_isenabled (FALSE));
			}

			_r_wnd_sendmessage (hwnd, IDC_DRIVES, LVM_SETIMAGELIST, LVSIL_SMALL, (LPARAM)himglist);

			_app_refreshdrives (hwnd);

			_r_layout_initializemanager (&layout_manager, hwnd);

			break;
		}

		case WM_SIZE:
		{
			_r_layout_resize (&layout_manager, wparam);
			break;
		}

		case WM_DEVICECHANGE:
		{
			_app_refreshdrives (hwnd);
			break;
		}

		case WM_CLOSE:
		{
			_app_unlockalldrives (hwnd);

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

			hmenu = LoadMenuW (NULL, MAKEINTRESOURCEW (IDM_DRIVES));

			if (!hmenu)
				break;

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
						_r_wnd_sendmessage (hwnd, 0, WM_COMMAND, MAKELPARAM (IDM_OPEN, 0), 0);

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
					DestroyWindow (hwnd);
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
					PR_STRING string;
					INT item_id = -1;

					if (!_r_listview_getselectedcount (hwnd, IDC_DRIVES))
						break;

					while ((item_id = _r_listview_getnextselected (hwnd, IDC_DRIVES, item_id)) != -1)
					{
						string = _r_listview_getitemtext (hwnd, IDC_DRIVES, item_id, 0);

						if (!string)
							continue;

						if (!_app_driveislocked (string->buffer))
							_r_shell_showfile (string->buffer);

						_r_obj_dereference (string);
					}

					break;
				}

				case IDM_REFRESH:
				{
					_app_refreshdrives (hwnd);
					break;
				}

				case IDM_PROTECT:
				{
					PR_STRING string;
					INT item_id = -1;
					NTSTATUS status;

					if (!_r_listview_getselectedcount (hwnd, IDC_DRIVES))
						break;

					while ((item_id = _r_listview_getnextselected (hwnd, IDC_DRIVES, item_id)) != -1)
					{
						string = _r_listview_getitemtext (hwnd, IDC_DRIVES, item_id, 0);

						if (!string)
							continue;

						status = _app_protectdrive (hwnd, string->buffer);

						if (!NT_SUCCESS (status))
							_r_show_errormessage (hwnd, NULL, status, string->buffer, TRUE);

						_r_obj_dereference (string);
					}

					_app_refreshdrives (hwnd);

					break;
				}

				case IDM_UNPROTECT:
				{
					PR_STRING string;
					INT item_id = -1;
					NTSTATUS status;

					if (!_r_listview_getselectedcount (hwnd, IDC_DRIVES))
						break;

					while ((item_id = _r_listview_getnextselected (hwnd, IDC_DRIVES, item_id)) != -1)
					{
						string = _r_listview_getitemtext (hwnd, IDC_DRIVES, item_id, 0);

						if (!string)
							continue;

						status = _app_unprotectdrive (hwnd, string->buffer);

						if (!NT_SUCCESS (status))
							_r_show_errormessage (hwnd, NULL, status, string->buffer, TRUE);

						_r_obj_dereference (string);
					}

					_app_refreshdrives (hwnd);

					break;
				}

				case IDM_LOCK:
				{
					PR_STRING string;
					INT item_id = -1;
					NTSTATUS status;

					if (!_r_listview_getselectedcount (hwnd, IDC_DRIVES))
						break;

					while ((item_id = _r_listview_getnextselected (hwnd, IDC_DRIVES, item_id)) != -1)
					{
						string = _r_listview_getitemtext (hwnd, IDC_DRIVES, item_id, 0);

						if (!string)
							continue;

						status = _app_lockdrive (string->buffer);

						if (!NT_SUCCESS (status))
							_r_show_errormessage (hwnd, NULL, status, string->buffer, TRUE);

						_r_obj_dereference (string);
					}

					_app_refreshdrives (hwnd);

					break;
				}

				case IDM_UNLOCK:
				{
					PR_STRING string;
					INT item_id = -1;
					NTSTATUS status;

					if (!_r_listview_getselectedcount (hwnd, IDC_DRIVES))
						break;

					while ((item_id = _r_listview_getnextselected (hwnd, IDC_DRIVES, item_id)) != -1)
					{
						string = _r_listview_getitemtext (hwnd, IDC_DRIVES, item_id, 0);

						if (!string)
							continue;

						status = _app_unlockdrive (string->buffer);

						if (!NT_SUCCESS (status))
							_r_show_errormessage (hwnd, NULL, status, string->buffer, TRUE);

						_r_obj_dereference (string);
					}

					_app_refreshdrives (hwnd);

					break;
				}

				case IDM_UNLOCK_ALL:
				{
					if (_r_show_confirmmessage (hwnd, NULL, L"Are you sure you want to unlock all drives?", NULL))
						_app_unlockalldrives (hwnd);

					break;
				}

				case IDM_EJECT:
				{
					PR_STRING string;
					INT item_id = -1;
					NTSTATUS status;

					if (!_r_listview_getselectedcount (hwnd, IDC_DRIVES))
						break;

					while ((item_id = _r_listview_getnextselected (hwnd, IDC_DRIVES, item_id)) != -1)
					{
						string = _r_listview_getitemtext (hwnd, IDC_DRIVES, item_id, 0);

						if (!string)
							continue;

						status = _app_ejectdrive (string->buffer);

						if (!NT_SUCCESS (status))
							_r_show_errormessage (hwnd, NULL, status, string->buffer, TRUE);

						_r_obj_dereference (string);
					}

					_app_refreshdrives (hwnd);

					break;
				}

				case IDM_FORMAT:
				{
					PR_STRING string;
					ULONG_PTR drive_number;
					INT item_id;

					if (!_r_listview_getselectedcount (hwnd, IDC_DRIVES))
						break;

					item_id = _r_listview_getnextselected (hwnd, IDC_DRIVES, -1);

					if (item_id == -1)
						break;

					string = _r_listview_getitemtext (hwnd, IDC_DRIVES, item_id, 0);

					if (!string)
						break;

					drive_number = _app_getdrivenumber (string->buffer);

					SHFormatDrive (hwnd, (INT)drive_number, SHFMT_ID_DEFAULT, 0);

					_app_refreshdrives (hwnd);

					_r_obj_dereference (string);

					break;
				}

				case IDM_PROPERTY:
				{
					PR_STRING string;
					INT item_id;

					if (!_r_listview_getselectedcount (hwnd, IDC_DRIVES))
						break;

					item_id = _r_listview_getnextselected (hwnd, IDC_DRIVES, -1);

					if (item_id == -1)
						break;

					string = _r_listview_getitemtext (hwnd, IDC_DRIVES, item_id, 0);

					if (!string)
						break;

					if (!_app_driveislocked (string->buffer))
						DialogBoxParamW (NULL, MAKEINTRESOURCEW (IDD_PROPERTIES), hwnd, &PropertiesDlgProc, (LPARAM)string);

					_r_obj_dereference (string);
				}

				case IDM_ZOOM:
				{
					ShowWindow (hwnd, IsZoomed (hwnd) ? SW_RESTORE : SW_MAXIMIZE);
					break;
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

	if (!_r_app_initialize (NULL))
		return ERROR_APP_INIT_FAILURE;

	hwnd = _r_app_createwindow (hinst, MAKEINTRESOURCEW (IDD_MAIN), MAKEINTRESOURCEW (IDI_MAIN), &DlgProc);

	if (!hwnd)
		return ERROR_APP_INIT_FAILURE;

	return _r_wnd_message_callback (hwnd, MAKEINTRESOURCEW (IDA_MAIN));
}
