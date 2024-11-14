// Drive Dot Shield
// Copyright (c) 2012-2024 Henry++


#include "routine.h"

#include "app.h"
#include "rapp.h"
#include "main.h"

#include "resource.h"

DRIVE_LOCK dl[PR_DEVICE_COUNT] = {0};

ULONG_PTR _app_getdrivenumber (
	_In_ LPWSTR drive
)
{
	return (ULONG_PTR)(drive[0] - 65);
}

BOOLEAN _app_driveislocked (
	_In_ LPWSTR drive
)
{
	ULONG_PTR i;

	i = _app_getdrivenumber (drive);

	if (dl[i].hdrive)
		return TRUE;

	return FALSE;
}

BOOLEAN _app_driveisready (
	_In_ LPWSTR drive
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
	_In_ LPWSTR drive,
	_Out_writes_z_ (buffer_length) LPWSTR buffer,
	_In_ _In_range_ (1, PR_SIZE_MAX_STRING_LENGTH) ULONG_PTR buffer_length
)
{
	PR_STRING path;

	if (_app_driveislocked (drive))
	{
		_r_str_copy (buffer, buffer_length, L"Locked");

		return DS_LOCKED;
	}

	path = _r_format_string (L"%C:\\autorun.inf", drive[0]);

	if (_r_fs_exists (&path->sr))
	{
		if (_r_fs_isdirectory (&path->sr))
		{
			_r_str_copy (buffer, buffer_length, L"Protected");

			_r_obj_dereference (path);

			return DS_PROTECTED;
		}
		else
		{
			_r_str_copy (buffer, buffer_length, L"Infected");

			_r_obj_dereference (path);

			return DS_INFECTED;
		}
	}

	_r_str_copy (buffer, buffer_length, L"Normal");

	_r_obj_dereference (path);

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

VOID _app_resizecolumns (
	_In_ HWND hwnd
)
{
	LONG dpi_value;

	dpi_value = _r_dc_getwindowdpi (hwnd);

	_r_listview_setcolumn (hwnd, IDC_DRIVES, 0, NULL, -10);
	_r_listview_setcolumn (hwnd, IDC_DRIVES, 1, NULL, -30);
	_r_listview_setcolumn (hwnd, IDC_DRIVES, 2, NULL, -20);
	_r_listview_setcolumn (hwnd, IDC_DRIVES, 3, NULL, -20);
	_r_listview_setcolumn (hwnd, IDC_DRIVES, 4, NULL, -20);
}

VOID _app_setstatusparts (
	_In_ HWND hwnd
)
{
	LONG parts[4] = {0};
	LONG width;

	width = _r_ctrl_getwidth (hwnd, 0);

	parts[0] = _r_calc_percentval (25, width);
	parts[1] = _r_calc_percentval (50, width);
	parts[2] = _r_calc_percentval (75, width);
	parts[3] = -1;

	_r_status_setparts (hwnd, IDC_STATUSBAR, parts, RTL_NUMBER_OF (parts));
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
	DRIVE_STATUS status;
	LONG protected_count = 0;
	LONG infected_count = 0;
	LONG locked_count = 0;
	LONG dpi_value;
	LONG image_id;

	dpi_value = _r_dc_getwindowdpi (hwnd);

	_app_setstatusparts (hwnd);

	_r_listview_setcolumn (hwnd, IDC_DRIVES, 0, NULL, _r_dc_getdpi (50, dpi_value));
	_r_listview_setcolumn (hwnd, IDC_DRIVES, 1, NULL, _r_dc_getdpi (90, dpi_value));
	_r_listview_setcolumn (hwnd, IDC_DRIVES, 2, NULL, _r_dc_getdpi (90, dpi_value));
	_r_listview_setcolumn (hwnd, IDC_DRIVES, 3, NULL, _r_dc_getdpi (90, dpi_value));
	_r_listview_setcolumn (hwnd, IDC_DRIVES, 4, NULL, _r_dc_getdpi (85, dpi_value));

	_r_listview_deleteallitems (hwnd, IDC_DRIVES);

	_r_fs_getdisklist (&drives);

	for (LONG i = 0, j = 0; i < PR_DEVICE_COUNT; i++)
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

	_r_status_settextformat (hwnd, IDC_STATUSBAR, 0, L"%s: %d", _r_locale_getstring (IDS_TOTALDRIVES), _r_listview_getitemcount (hwnd, IDC_DRIVES));
	_r_status_settextformat (hwnd, IDC_STATUSBAR, 1, L"%s: %d", _r_locale_getstring (IDS_PROTECTEDDRIVES), protected_count);
	_r_status_settextformat (hwnd, IDC_STATUSBAR, 2, L"%s: %d", _r_locale_getstring (IDS_INFECTEDDRIVES), infected_count);
	_r_status_settextformat (hwnd, IDC_STATUSBAR, 3, L"%s: %d", _r_locale_getstring (IDS_LOCKEDDRIVES), locked_count);

	_app_resizecolumns (hwnd);

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
	PR_STRING autorun_file;
	NTSTATUS status = STATUS_ABANDON_HIBERFILE;

	autorun_file = _r_format_string (L"%C:\\autorun.inf", drive[0]);

	_r_fs_setattributes (NULL, autorun_file->buffer, FILE_ATTRIBUTE_NORMAL);

	if (NT_SUCCESS (status))
	{
		if (_r_fs_isdirectory (&autorun_file->sr))
		{
			status = _r_fs_deletedirectory (autorun_file->buffer, TRUE);
		}
		else
		{
			status = _r_fs_deletefile (autorun_file->buffer, NULL);
		}
	}

	if (status == STATUS_OBJECT_NAME_NOT_FOUND)
		status = STATUS_SUCCESS;

	return status;
}

NTSTATUS _app_lockdrive (
	_In_ LPWSTR drive
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
	_In_ LPWSTR drive
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
		_r_listview_fillitems (hwnd, IDC_PROPERTIES, -1, -1, 1, L"n/a", I_IMAGENONE);

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

	_r_listview_setitem (hwnd, IDC_PROPERTIES, 2, 1, _app_driveislocked (drive->buffer) ? dl[drive_number].label->buffer : _r_obj_getstringordefault (label, L"<empty>"));

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
			_r_listview_setitem (hwnd, IDC_PROPERTIES, 3, 1, L"n/a");
			break;
		}
	}

	_r_listview_setitem (hwnd, IDC_PROPERTIES, 4, 1, _app_driveislocked (drive->buffer) ? dl[drive_number].file_system->buffer : _r_obj_getstringordefault (file_system, L"<unknown>"));
	_r_str_printf (buffer, RTL_NUMBER_OF (buffer), L"%" TEXT (PR_ULONG) " (%04X-%04X)", serial_number, HIWORD (serial_number), LOWORD (serial_number));

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
			_r_wnd_center (hwnd, GetParent (hwnd));

			_r_ctrl_setstring (hwnd, 0, _r_locale_getstring (IDS_PROPERTIES));

			_r_listview_setstyle (hwnd, IDC_PROPERTIES, LVS_EX_FULLROWSELECT | LVS_EX_INFOTIP | LVS_EX_DOUBLEBUFFER, TRUE);

			for (INT i = 0; i < 2; i++)
				_r_listview_addcolumn (hwnd, IDC_PROPERTIES, i, L"", -50, 0);

			_r_listview_addgroup (hwnd, IDC_PROPERTIES, 0, _r_locale_getstring (IDS_GROUP1), 0, LVGS_COLLAPSIBLE, LVGS_COLLAPSIBLE);
			_r_listview_addgroup (hwnd, IDC_PROPERTIES, 1, _r_locale_getstring (IDS_GROUP2), 0, LVGS_COLLAPSIBLE, LVGS_COLLAPSIBLE);

			_r_listview_additem_ex (hwnd, IDC_PROPERTIES, 0, _r_locale_getstring (IDS_DRIVE), I_IMAGENONE, 0, 0);
			_r_listview_additem_ex (hwnd, IDC_PROPERTIES, 1, _r_locale_getstring (IDS_STATUS), I_IMAGENONE, 0, 0);
			_r_listview_additem_ex (hwnd, IDC_PROPERTIES, 2, _r_locale_getstring (IDS_LABEL), I_IMAGENONE, 0, 0);
			_r_listview_additem_ex (hwnd, IDC_PROPERTIES, 3, _r_locale_getstring (IDS_TYPE), I_IMAGENONE, 0, 0);
			_r_listview_additem_ex (hwnd, IDC_PROPERTIES, 4, _r_locale_getstring (IDS_FILESYSTEM), I_IMAGENONE, 0, 0);
			_r_listview_additem_ex (hwnd, IDC_PROPERTIES, 5, _r_locale_getstring (IDS_SERIAL), I_IMAGENONE, 0, 0);

			_r_listview_additem_ex (hwnd, IDC_PROPERTIES, 6, _r_locale_getstring (IDS_FREESPACE), I_IMAGENONE, 1, 0);
			_r_listview_additem_ex (hwnd, IDC_PROPERTIES, 7, _r_locale_getstring (IDS_USEDSPACE), I_IMAGENONE, 1, 0);
			_r_listview_additem_ex (hwnd, IDC_PROPERTIES, 8, _r_locale_getstring (IDS_TOTALSPACE), I_IMAGENONE, 1, 0);

			drive = (PR_STRING)lparam;

			_app_refreshdriveinfo (hwnd, drive);

			_r_theme_initialize (hwnd, _r_theme_isenabled ());

			break;
		}

		case WM_CLOSE:
		{
			if (drive)
				_r_obj_clearreference (&drive);

			EndDialog (hwnd, 0);

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

			if (hsubmenu)
			{
				_r_menu_setitemtext (hsubmenu, IDM_COPY, FALSE, _r_locale_getstring (IDS_COPY));
				_r_menu_setitemtext (hsubmenu, IDM_REFRESH, FALSE, _r_locale_getstring (IDS_REFRESH));

				if (!_r_listview_getselectedcount (hwnd, IDC_PROPERTIES))
					_r_menu_enableitem (hsubmenu, IDM_REFRESH, MF_BYCOMMAND, FALSE);

				_r_menu_popup (hsubmenu, hwnd, NULL, TRUE);
			}

			DestroyMenu (hmenu);

			break;
		}

		case WM_PAINT:
		{
			PAINTSTRUCT ps = {0};
			HDC hdc;

			hdc = BeginPaint (hwnd, &ps);

			_r_dc_drawwindow (hdc, hwnd, TRUE);

			EndPaint (hwnd, &ps);

			return 0;
		}

		case WM_COMMAND:
		{
			INT ctrl_id = LOWORD (wparam);

			switch (ctrl_id)
			{
				case IDCANCEL: // process Esc key
				case IDC_OK:
				{
					EndDialog (hwnd, 0);
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

					if (!_r_listview_getselectedcount (hwnd, IDC_PROPERTIES))
						break;

					_r_obj_initializestringbuilder (&sb, 256);

					while ((item_id = _r_listview_getnextselected (hwnd, IDC_PROPERTIES, item_id)) != -1)
					{
						string = _r_listview_getitemtext (hwnd, IDC_PROPERTIES, item_id, 1);

						if (string)
						{
							_r_obj_appendstringbuilder2 (&sb, &string->sr);
							_r_obj_appendstringbuilder (&sb, L"\r\n");

							_r_obj_dereference (string);
						}
					}

					string = _r_obj_finalstringbuilder (&sb);

					_r_str_trimstring2 (&string->sr, L"\r\n", 0);

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

			_r_listview_addcolumn (hwnd, IDC_DRIVES, 0, L"", -10, 0);
			_r_listview_addcolumn (hwnd, IDC_DRIVES, 1, L"", -40, 0);
			_r_listview_addcolumn (hwnd, IDC_DRIVES, 2, L"", -40, 0);
			_r_listview_addcolumn (hwnd, IDC_DRIVES, 3, L"", -40, 0);
			_r_listview_addcolumn (hwnd, IDC_DRIVES, 4, L"", -40, 0);

			width = _r_dc_getsystemmetrics (SM_CXSMICON, dpi_value);

			himglist = ImageList_Create (width, width, ILC_COLOR32, 0, 5);

			for (INT i = IDI_NORMAL, j = 0; i < (IDI_LOCKED + 1); i++, j++)
			{
				_r_sys_loadicon (_r_sys_getimagebase (), MAKEINTRESOURCEW (i), width, &hicon);

				ImageList_ReplaceIcon (himglist, -1, hicon);

				_r_status_seticon (hwnd, IDC_STATUSBAR, j, ImageList_GetIcon (himglist, j, ILD_NORMAL));

				DestroyIcon (hicon);
			}

			hmenu = GetMenu (hwnd);

			if (hmenu)
			{
				_r_menu_checkitem (hmenu, IDM_ALWAYSONTOP_CHK, 0, MF_BYCOMMAND, _r_config_getboolean (L"AlwaysOnTop", FALSE));
				_r_menu_checkitem (hmenu, IDM_DARKMODE_CHK, 0, MF_BYCOMMAND, _r_theme_isenabled ());
				_r_menu_checkitem (hmenu, IDM_CHECKUPDATES_CHK, 0, MF_BYCOMMAND, _r_update_isenabled (FALSE));
			}

			_r_wnd_sendmessage (hwnd, IDC_DRIVES, LVM_SETIMAGELIST, LVSIL_SMALL, (LPARAM)himglist);

			_app_refreshdrives (hwnd);

			_r_layout_initializemanager (&layout_manager, hwnd);

			break;
		}

		case RM_LOCALIZE:
		{
			HMENU hmenu;

			// localize
			hmenu = GetMenu (hwnd);

			if (hmenu)
			{
				_r_menu_setitemtext (hmenu, 0, TRUE, _r_locale_getstring (IDS_FILE));
				_r_menu_setitemtext (hmenu, 1, TRUE, _r_locale_getstring (IDS_SETTINGS));
				_r_menu_setitemtext (hmenu, 2, TRUE, _r_locale_getstring (IDS_HELP));

				_r_menu_setitemtextformat (hmenu, IDM_SETTINGS, FALSE, L"%s...\tF2", _r_locale_getstring (IDS_SETTINGS));
				_r_menu_setitemtextformat (hmenu, IDM_EXIT, FALSE, L"%s\tEsc", _r_locale_getstring (IDS_EXIT));
				_r_menu_setitemtext (hmenu, IDM_ALWAYSONTOP_CHK, FALSE, _r_locale_getstring (IDS_ALWAYSONTOP_CHK));
				_r_menu_setitemtext (hmenu, IDM_DARKMODE_CHK, FALSE, _r_locale_getstring (IDS_DARKMODE_CHK));
				_r_menu_setitemtext (hmenu, IDM_CHECKUPDATES_CHK, FALSE, _r_locale_getstring (IDS_CHECKUPDATES_CHK));
				_r_menu_setitemtextformat (GetSubMenu (hmenu, 1), LANG_MENU, TRUE, L"%s (Language)", _r_locale_getstring (IDS_LANGUAGE));
				_r_menu_setitemtext (hmenu, IDM_WEBSITE, FALSE, _r_locale_getstring (IDS_WEBSITE));
				_r_menu_setitemtext (hmenu, IDM_CHECKUPDATES, FALSE, _r_locale_getstring (IDS_CHECKUPDATES));
				_r_menu_setitemtextformat (hmenu, IDM_ABOUT, FALSE, L"%s\tF1", _r_locale_getstring (IDS_ABOUT));

				_r_locale_enum (GetSubMenu (hmenu, 1), LANG_MENU, IDX_LANGUAGE); // enum localizations
			}

			_r_listview_setcolumn (hwnd, IDC_DRIVES, 0, _r_locale_getstring (IDS_DRIVE), 0);
			_r_listview_setcolumn (hwnd, IDC_DRIVES, 1, _r_locale_getstring (IDS_LABEL), 0);
			_r_listview_setcolumn (hwnd, IDC_DRIVES, 2, _r_locale_getstring (IDS_TYPE), 0);
			_r_listview_setcolumn (hwnd, IDC_DRIVES, 3, _r_locale_getstring (IDS_FILESYSTEM), 0);
			_r_listview_setcolumn (hwnd, IDC_DRIVES, 4, _r_locale_getstring (IDS_STATUS), 0);

			_app_refreshdrives (hwnd);

			break;
		}

		case WM_SIZE:
		{
			_r_layout_resize (&layout_manager, wparam);

			_app_resizecolumns (hwnd);

			_app_setstatusparts (hwnd);

			break;
		}

		case WM_GETMINMAXINFO:
		{
			_r_layout_resizeminimumsize (&layout_manager, lparam);
			break;
		}

		case WM_DEVICECHANGE:
		{
			_app_refreshdrives (hwnd);
			break;
		}

		case WM_DESTROY:
		{
			_app_unlockalldrives (hwnd);

			PostQuitMessage (0);

			break;
		}

		case WM_CLOSE:
		{

			DestroyWindow (hwnd);
			break;
		}

		case WM_NOTIFY:
		{
			LPNMHDR nmlp;

			nmlp = (LPNMHDR)lparam;

			switch (nmlp->code)
			{
				case NM_RCLICK:
				{
					LPNMITEMACTIVATE lpnmlv;
					HMENU hsubmenu;
					HMENU hmenu;

					lpnmlv = (LPNMITEMACTIVATE)lparam;

					if (!nmlp->idFrom || lpnmlv->iItem == -1 || nmlp->idFrom != IDC_DRIVES)
						break;

					// localize
					hmenu = LoadMenuW (NULL, MAKEINTRESOURCEW (IDM_DRIVES));

					if (!hmenu)
						break;

					hsubmenu = GetSubMenu (hmenu, 0);

					if (hsubmenu)
					{
						_r_menu_setitemtext (hsubmenu, IDM_OPEN, FALSE, _r_locale_getstring (IDS_OPEN));
						_r_menu_setitemtext (hsubmenu, IDM_PROTECT, FALSE, _r_locale_getstring (IDS_PROTECT));
						_r_menu_setitemtext (hsubmenu, IDM_UNPROTECT, FALSE, _r_locale_getstring (IDS_UNPROTECT));
						_r_menu_setitemtextformat (hsubmenu, IDM_REFRESH, FALSE, L"%s\tF5", _r_locale_getstring (IDS_REFRESH));
						_r_menu_setitemtextformat (hsubmenu, IDM_COPY, FALSE, L"%s\tCtrl+C", _r_locale_getstring (IDS_COPY));
						_r_menu_setitemtext (hsubmenu, 7, TRUE, _r_locale_getstring (IDS_SERVICE));
						_r_menu_setitemtext (hsubmenu, IDM_LOCK, FALSE, _r_locale_getstring (IDS_LOCK));
						_r_menu_setitemtext (hsubmenu, IDM_UNLOCK, FALSE, _r_locale_getstring (IDS_UNLOCK));
						_r_menu_setitemtext (hsubmenu, IDM_UNLOCK_ALL, FALSE, _r_locale_getstring (IDS_UNLOCK_ALL));
						_r_menu_setitemtext (hsubmenu, IDM_EJECT, FALSE, _r_locale_getstring (IDS_EJECT));
						_r_menu_setitemtext (hsubmenu, IDM_FORMAT, FALSE, _r_locale_getstring (IDS_FORMAT));
						_r_menu_setitemtext (hsubmenu, IDM_PROPERTY, FALSE, _r_locale_getstring (IDS_PROPERTIES));

						_r_menu_popup (hsubmenu, hwnd, NULL, TRUE);
					}

					DestroyMenu (hmenu);

					break;
				}

				case NM_DBLCLK:
				{
					LPNMITEMACTIVATE lpnmia;

					if (nmlp->idFrom != IDC_DRIVES)
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
			INT notify_code = HIWORD (wparam);

			if (notify_code == 0 && ctrl_id >= IDX_LANGUAGE && ctrl_id <= IDX_LANGUAGE + (INT)(_r_locale_getcount () + 1))
			{
				HMENU hsubmenu;
				HMENU hmenu;

				hmenu = GetMenu (hwnd);

				if (hmenu)
				{
					hsubmenu = GetSubMenu (hmenu, 1);

					if (hsubmenu)
					{
						hsubmenu = GetSubMenu (hsubmenu, LANG_MENU);

						if (hsubmenu)
							_r_locale_apply (hsubmenu, ctrl_id, IDX_LANGUAGE);
					}
				}

				return FALSE;
			}

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

				case IDM_DARKMODE_CHK:
				{
					BOOLEAN new_val;

					new_val = !_r_theme_isenabled ();

					_r_menu_checkitem (GetMenu (hwnd), ctrl_id, 0, MF_BYCOMMAND, new_val);
					_r_theme_enable (hwnd, new_val);


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

				case IDM_CHECKUPDATES:
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
							_r_shell_showfile (&string->sr);

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

					break;
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
