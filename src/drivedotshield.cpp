/************************************
*  	Drive Dot Shield
*	Copyright © 2012 Henry++
*
*	GNU General Public License v2
*	http://www.gnu.org/licenses/
*
*	http://www.henrypp.org/
*************************************/

// Include
#include <windows.h>
#include <commctrl.h>
#include <wininet.h>
#include <shlobj.h>
#include <atlstr.h> // cstring
#include <process.h> // _beginthreadex

#include "drivedotshield.h"
#include "routine.h"
#include "resource.h"
#include "ini.h"

INI ini;
CONFIG cfg = {0};
DRIVE_LOCK dl[26] = {0};
CONST UINT WM_MUTEX = RegisterWindowMessage(APP_NAME_SHORT);

// Check Updates
UINT WINAPI CheckUpdates(LPVOID lpParam)
{
	BOOL bStatus = 0;
	HINTERNET hInternet = 0, hConnect = 0;

	// Disable Menu
	EnableMenuItem(GetMenu(cfg.hWnd), IDM_CHECK_UPDATES, MF_BYCOMMAND | MF_DISABLED);

	// Connect
	if((hInternet = InternetOpen(APP_NAME L"/" APP_VERSION L" (+" APP_WEBSITE L")", INTERNET_OPEN_TYPE_DIRECT, NULL, NULL, 0)) && (hConnect = InternetOpenUrl(hInternet, APP_WEBSITE L"/update.php?product=" APP_NAME_SHORT, NULL, 0, INTERNET_FLAG_RELOAD | INTERNET_FLAG_NO_CACHE_WRITE, 0)))
	{
		// Get Status
		DWORD dwStatus = 0, dwStatusSize = sizeof(dwStatus);
		HttpQueryInfo(hConnect, HTTP_QUERY_FLAG_NUMBER | HTTP_QUERY_STATUS_CODE, &dwStatus, &dwStatusSize, NULL);

		// Check Errors
		if(dwStatus == HTTP_STATUS_OK)
		{
			// Reading
			ULONG ulReaded = 0;
			CHAR szBufferA[MAX_PATH] = {0};

			if(InternetReadFile(hConnect, szBufferA, MAX_PATH, &ulReaded) && ulReaded)
			{
				// Convert to Unicode
				CA2W newver(szBufferA, CP_UTF8);

				// If NEWVER == CURVER
				if(lstrcmpi(newver, APP_VERSION) == 0)
				{
					if(!lpParam)
						MessageBox(cfg.hWnd, MB_OK | MB_ICONINFORMATION, APP_NAME, L"Вы используете последнюю версию программы");
				}
				else
				{
					if(MessageBox(cfg.hWnd, MB_YESNO | MB_ICONQUESTION, APP_NAME, L"Доступна новая версия программы: %s\r\nВы хотите открыть страницу загрузки новой версии?", newver) == IDYES)
						ShellExecute(cfg.hWnd, 0, APP_WEBSITE L"/?product=" APP_NAME_SHORT, NULL, NULL, SW_SHOWDEFAULT);
				}

				// Switch Result
				bStatus = 1;
			}
		}
	}

	if(!bStatus && !lpParam)
		MessageBox(cfg.hWnd, MB_OK | MB_ICONSTOP, APP_NAME, L"Ошибка при подключении к серверу обновления");

	// Restore Menu
	EnableMenuItem(GetMenu(cfg.hWnd), IDM_CHECK_UPDATES, MF_BYCOMMAND | MF_ENABLED);

	// Clear Memory
	InternetCloseHandle(hConnect);
	InternetCloseHandle(hInternet);

	return bStatus;
}

CString Lv_GetItemText(HWND hWnd, int idCtrl, int iItem, int iSubItem)
{
	LVITEM lvi = {0};
	CString buffer;

	if(iItem < 0)
		iItem = SendDlgItemMessage(hWnd, idCtrl, LVM_GETSELECTIONMARK, 0, 0);

	lvi.iSubItem = iSubItem;
	lvi.pszText = buffer.GetBuffer(512);
	lvi.cchTextMax = 512;

	SendDlgItemMessage(hWnd, idCtrl, LVM_GETITEMTEXT, iItem, (LPARAM)&lvi);
	buffer.ReleaseBuffer();

	return buffer;
}

BOOL ExecuteCommand(LPWSTR lpszCommand)
{
	STARTUPINFO si = {0};
	PROCESS_INFORMATION pi = {0};

	return CreateProcess(NULL, lpszCommand, NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi);
}

INT GetDriveNumber(LPCWSTR lpcszDrive)
{
	return INT(lpcszDrive[0] - 65);
}

BOOL DriveIsLocked(LPCWSTR lpcszDrive)
{
	return dl[GetDriveNumber(lpcszDrive)].hDrive ? 1 : 0;
}

BOOL DriveIsReady(LPCWSTR lpcszDrive)
{
	if(DriveIsLocked(lpcszDrive))
		return 1;

	WCHAR szBuffer[20] = {0};
	StringCchPrintf(szBuffer, 20, L"%c:\\", lpcszDrive[0]);

	UINT uOldErr = SetErrorMode(SEM_FAILCRITICALERRORS);
	BOOL bRes = GetVolumeInformation(szBuffer, NULL, 0, NULL, 0, 0, NULL, 0);
	SetErrorMode(uOldErr);

	return (bRes) ? 1 : 0;
}

DRIVE_STATUS GetDriveStatus(LPCWSTR lpcszDrive)
{		
	if(DriveIsLocked(lpcszDrive))
		return DS_LOCKED;

	WCHAR szBuffer[MAX_PATH] = {0};
	StringCchPrintf(szBuffer, MAX_PATH, L"%c:\\autorun.inf", lpcszDrive[0]);

	if(FileExists(szBuffer))
		return (GetFileAttributes(szBuffer) & FILE_ATTRIBUTE_DIRECTORY) ? DS_PROTECTED : DS_INFECTED;

	return DS_NORMAL;
}

BOOL ProtectDrive(LPCWSTR lpcszDrive)
{		
	CString buffer;

	buffer.Format(L"\\\\?\\%c:\\autorun.inf", lpcszDrive[0]);

	CreateDirectory(buffer, NULL);
	SetFileAttributes(buffer, FILE_ATTRIBUTE_HIDDEN | FILE_ATTRIBUTE_READONLY | FILE_ATTRIBUTE_SYSTEM);

	buffer.Append(L"\\" APP_NAME_SHORT L".");
	HANDLE hFile = CreateFile(buffer, GENERIC_WRITE | GENERIC_READ, FILE_SHARE_WRITE | FILE_SHARE_READ, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_HIDDEN | FILE_ATTRIBUTE_READONLY | FILE_ATTRIBUTE_SYSTEM, NULL);
	CloseHandle(hFile);

	return 1;
}

BOOL UnprotectDrive(LPCWSTR lpcszDrive)
{		
	CString buffer;

	buffer.Format(L"\\\\?\\%c:\\autorun.inf", lpcszDrive[0]);

	SetFileAttributes(buffer + L"\\" APP_NAME_SHORT L".", 0);
	DeleteFile(buffer + L"\\" APP_NAME_SHORT L".");

	SetFileAttributes(buffer, 0);
	RemoveDirectory(buffer);

	return 1;
}

BOOL LockDrive(LPCWSTR lpcszDrive)
{		
	DWORD dwRet = 0;
	BOOL bRet = 0;

	CString buffer;

	buffer.Format(L"\\\\.\\%c:", lpcszDrive[0]);

	HANDLE hFile = CreateFile(buffer, GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, NULL);

	if(hFile)
	{
		INT iDrive = GetDriveNumber(lpcszDrive);

		buffer = lpcszDrive[0];
		buffer.Append(L":\\\0");
	
		GetVolumeInformation(buffer, dl[iDrive].szLabel, MAX_PATH, &dl[iDrive].dwSerial, NULL, NULL, dl[iDrive].szFileSystem, MAX_PATH);
		GetDiskFreeSpaceEx(buffer, 0, &dl[iDrive].uiTotal, &dl[iDrive].uiFree);

		if(DeviceIoControl(hFile, FSCTL_LOCK_VOLUME, NULL, 0, NULL, 0, &dwRet, NULL))
		{
			bRet = 1;
			dl[iDrive].hDrive = hFile;
		}
		else
		{
			CloseHandle(hFile);
		}
	}

	return bRet;
}

BOOL UnlockDrive(LPCWSTR lpcszDrive)
{		
	CString buffer;

	INT iDrive = GetDriveNumber(lpcszDrive);

	if(!dl[iDrive].hDrive)
		return 0;

	DWORD dwRet = 0;
	DeviceIoControl(dl[iDrive].hDrive, FSCTL_UNLOCK_VOLUME, NULL, NULL, NULL, NULL, &dwRet, NULL);
	CloseHandle(dl[iDrive].hDrive);

	dl[iDrive].hDrive = 0;

	return 1;
}

BOOL EjectDrive(LPCWSTR lpcszDrive)
{		
	CString buffer;

	DWORD dwRet = 0;
	SECURITY_ATTRIBUTES sa = {0};
	HANDLE hFile = NULL;
	BOOL bRet = 0;

	buffer.Format(L"\\\\.\\%c:", lpcszDrive[0]);

	sa.nLength = sizeof(sa);
	sa.lpSecurityDescriptor = NULL;
	sa.bInheritHandle = TRUE;
			
	if((hFile = CreateFile(buffer, GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, &sa, OPEN_EXISTING, FILE_FLAG_WRITE_THROUGH, NULL)))
	{
		FlushFileBuffers(hFile);

		if(DeviceIoControl(hFile, IOCTL_DISK_EJECT_MEDIA, NULL, 0, NULL, 0, &dwRet, NULL))
			bRet = 1;

		CloseHandle(hFile);
	}

	return bRet;
}

void RefreshDrivesList(HWND hWnd)
{
	DWORD dwDrives = GetLogicalDrives();
	WCHAR szDrive[4] = {0}, szLabel[MAX_PATH] = {0}, szFileSystem[MAX_PATH] = {0};
	INT iInfectedCount = 0, iProtectedCount = 0, iLockedCount = 0;

	// Clear Previous Result
	SendDlgItemMessage(hWnd, IDC_DRIVES, LVM_DELETEALLITEMS, 0, 0);

	for(int i = 0, j = 0; i < 26; i++)
	{
		if(((dwDrives >> i) & 0x00000001))
		{
			szDrive[0] = (65 + i);
			szDrive[1] = L':';
			szDrive[2] = L'\0';

			if(DriveIsReady(szDrive))
			{
				// Drive Letter
				Lv_InsertItem(hWnd, IDC_DRIVES, szDrive, j, 0, GetDriveStatus(szDrive));

				szDrive[2] = L'\\';
				szDrive[3] = L'\0';

				// General Info
				if(!DriveIsLocked(szDrive))
					GetVolumeInformation(szDrive, szLabel, MAX_PATH, NULL, NULL, NULL, szFileSystem, MAX_PATH);

				Lv_InsertItem(hWnd, IDC_DRIVES, DriveIsLocked(szDrive) ? dl[GetDriveNumber(szDrive)].szLabel : szLabel, j, 1);
				Lv_InsertItem(hWnd, IDC_DRIVES, DriveIsLocked(szDrive) ? dl[GetDriveNumber(szDrive)].szFileSystem : szFileSystem, j, 3);

				// Drive Type
				switch(GetDriveType(szDrive))
				{
					case DRIVE_REMOVABLE:
						Lv_InsertItem(hWnd, IDC_DRIVES, L"Съёмный диск", j, 2);
						break;

					case DRIVE_FIXED:
						Lv_InsertItem(hWnd, IDC_DRIVES, L"Локальный диск", j, 2);
						break;

					case DRIVE_REMOTE:
						Lv_InsertItem(hWnd, IDC_DRIVES, L"Сетевой диск", j, 2);
						break;

					case DRIVE_CDROM:
						Lv_InsertItem(hWnd, IDC_DRIVES, L"CD-ROM", j, 2);
						break;

					default:
						Lv_InsertItem(hWnd, IDC_DRIVES, L"Неизвестно", j, 2);
						break;
				}

				switch(GetDriveStatus(szDrive))
				{
					case DS_PROTECTED:
						Lv_InsertItem(hWnd, IDC_DRIVES, L"Защищён", j, 4);
						iProtectedCount++;

						break;

					case DS_INFECTED:
						Lv_InsertItem(hWnd, IDC_DRIVES, L"Заражён", j, 4);
						iInfectedCount++;

						break;
					
					case DS_LOCKED:
						Lv_InsertItem(hWnd, IDC_DRIVES, L"Заблокирован", j, 4);
						iLockedCount++;

						break;
					
					default:
						Lv_InsertItem(hWnd, IDC_DRIVES, L"Не заражён", j, 4);
						break;
				}

				j++;
			}
		}
	}

	// Update StatusBar Text
	WCHAR szBuffer[MAX_PATH] = {0};

	StringCchPrintf(szBuffer, MAX_PATH, L"Всего дисков: %i\0", SendDlgItemMessage(hWnd, IDC_DRIVES, LVM_GETITEMCOUNT, 0, 0));
	SendDlgItemMessage(hWnd, IDC_STATUSBAR, SB_SETTEXT, MAKEWPARAM(0, 0), (LPARAM)szBuffer);

	StringCchPrintf(szBuffer, MAX_PATH, L"Защищено: %i\0", iProtectedCount);
	SendDlgItemMessage(hWnd, IDC_STATUSBAR, SB_SETTEXT, MAKEWPARAM(1, 0), (LPARAM)szBuffer);
	
	StringCchPrintf(szBuffer, MAX_PATH, L"Заражено: %i\0", iInfectedCount);
	SendDlgItemMessage(hWnd, IDC_STATUSBAR, SB_SETTEXT, MAKEWPARAM(2, 0), (LPARAM)szBuffer);

	StringCchPrintf(szBuffer, MAX_PATH, L"Заблокировано: %i\0", iLockedCount);
	SendDlgItemMessage(hWnd, IDC_STATUSBAR, SB_SETTEXT, MAKEWPARAM(3, 0), (LPARAM)szBuffer);
}

INT_PTR CALLBACK SettingsDlgProc(HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	CString buffer;
	INT iBuffer = 0;
	RECT rc = {0};

	switch(uMsg)
	{
		case WM_INITDIALOG:
		{
			// Centering By Parent
			CenterDialog(hwndDlg);

			// Use Bold Font for Label
			SendDlgItemMessage(hwndDlg, IDC_LABEL_1, WM_SETFONT, (WPARAM)cfg.hBold, 0);
			SendDlgItemMessage(hwndDlg, IDC_LABEL_2, WM_SETFONT, (WPARAM)cfg.hBold, 0);
			SendDlgItemMessage(hwndDlg, IDC_LABEL_3, WM_SETFONT, (WPARAM)cfg.hBold, 0);
			SendDlgItemMessage(hwndDlg, IDC_LABEL_4, WM_SETFONT, (WPARAM)cfg.hBold, 0);

			// Main (Section)
			CheckDlgButton(hwndDlg, IDC_CHECK_UPDATE_AT_STARTUP_CHK, ini.read(APP_NAME_SHORT, L"CheckUpdateAtStartup", 1) ? BST_CHECKED : BST_UNCHECKED);
			CheckDlgButton(hwndDlg, IDC_ALWAYS_ON_TOP_CHK, ini.read(APP_NAME_SHORT, L"AlwaysOnTop", 0) ? BST_CHECKED : BST_UNCHECKED);

			// Protection (Section)
			CheckDlgButton(hwndDlg, IDC_ENABLE_LOWLEVEL_FAT_PROTECTION_CHK, ini.read(APP_NAME_SHORT, L"EnableLowLevelFatProtection", 0) ? BST_CHECKED : BST_UNCHECKED);

			// System Settings (Section)
			HKEY hKey = NULL, hKeyRoots[] = {HKEY_LOCAL_MACHINE, HKEY_CURRENT_USER};
			DWORD dwType = 0, dwValue = 0, dwSize = sizeof(dwValue);

			for(int i = 0; i < (sizeof(hKeyRoots) / sizeof(hKeyRoots[0])); i++)
			{
				if(RegOpenKeyEx(hKeyRoots[i], L"Software\\Microsoft\\Windows\\CurrentVersion\\Policies\\Explorer", 0, KEY_ALL_ACCESS, &hKey) == ERROR_SUCCESS)
				{
					RegQueryValueEx(hKey, L"NoDriveTypeAutoRun", 0, &dwType, (LPBYTE)&dwValue, &dwSize);
					RegCloseKey(hKey);

					if(dwValue != 255)
					{
						iBuffer = 1;
						break;
					}
				}
			}

			CheckDlgButton(hwndDlg, IDC_NODRIVETYPEAUTORUN_CHK, !iBuffer ? BST_CHECKED : BST_UNCHECKED);

			if(RegOpenKeyEx(HKEY_LOCAL_MACHINE, L"SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion\\IniFileMapping\\Autorun.inf", 0, KEY_ALL_ACCESS, &hKey) == ERROR_SUCCESS)
			{
				dwSize = MAX_PATH;

				RegQueryValueEx(hKey, NULL, 0, &dwType, (LPBYTE)buffer.GetBuffer(MAX_PATH), &dwSize);
				buffer.ReleaseBuffer();

				RegCloseKey(hKey);

				if(lstrcmpi(buffer, L"@SYS:DoesNotExist") == 0)
					CheckDlgButton(hwndDlg, IDC_INIFILEMAPPING_CHK, BST_CHECKED);
			}

			// Language (Section)
			SendDlgItemMessage(hwndDlg, IDC_LANGUAGE_CB, CB_ADDSTRING, 0, (LPARAM)L"English (default)");
			buffer.Format(L"%s\\Languages\\*.dll", cfg.szCurrentDir);

			WIN32_FIND_DATA wfd = {0};
			HANDLE hFind = FindFirstFile(buffer, &wfd);

			if(hFind != INVALID_HANDLE_VALUE)
			{
				do {
					if(!(wfd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY))
					{
						PathRemoveExtension(wfd.cFileName);
						SendDlgItemMessage(hwndDlg, IDC_LANGUAGE_CB, CB_ADDSTRING, 0, (LPARAM)wfd.cFileName);
					}
				} while(FindNextFile(hFind, &wfd));

				FindClose(hFind);
			}
			else
			{
				EnableWindow(GetDlgItem(hwndDlg, IDC_LANGUAGE_CB), 0);
			}

			if(SendDlgItemMessage(hwndDlg, IDC_LANGUAGE_CB, CB_SELECTSTRING, 1, (LPARAM)ini.read(APP_NAME_SHORT, L"Language", MAX_PATH, 0)) == CB_ERR)
				SendDlgItemMessage(hwndDlg, IDC_LANGUAGE_CB, CB_SETCURSEL, 0, 0);

			SendMessage(hwndDlg, WM_COMMAND, MAKELPARAM(IDC_LANGUAGE_CB, CBN_SELENDOK), 0);

			EnableWindow(GetDlgItem(hwndDlg, IDC_OK), 0);

			break;
		}

		case WM_CLOSE:
		{
			EndDialog(hwndDlg, 0);
			break;
		}

		case WM_PAINT:
		{
			PAINTSTRUCT ps = {0};
			HDC hDC = BeginPaint(hwndDlg, &ps);

			GetClientRect(hwndDlg, &rc);
			rc.top = rc.bottom - 43;

			// Instead FillRect
			COLORREF clrOld = SetBkColor(hDC, GetSysColor(COLOR_BTNFACE));
			ExtTextOut(hDC, 0, 0, ETO_OPAQUE, &rc, NULL, 0, NULL);
			SetBkColor(hDC, clrOld);

			// Draw Line
			for(int i = 0; i < rc.right; i++)
				SetPixel(hDC, i, rc.top, GetSysColor(COLOR_BTNSHADOW));

			EndPaint(hwndDlg, &ps);

			return 0;
		}

		case WM_CTLCOLORSTATIC:
		case WM_CTLCOLORDLG:
		{
			return (INT_PTR)GetSysColorBrush(COLOR_WINDOW);
		}

		case WM_COMMAND:
		{
			if(lParam && (HIWORD(wParam) == BN_CLICKED || HIWORD(wParam) == EN_CHANGE || HIWORD(wParam) == CBN_SELENDOK))
				EnableWindow(GetDlgItem(hwndDlg, IDC_OK), 1);

			if(HIWORD(wParam) == CBN_SELENDOK && LOWORD(wParam) == IDC_LANGUAGE_CB)
			{
				HINSTANCE hModule = NULL;
				WCHAR szBuffer[MAX_PATH] = {0};

				iBuffer = (SendDlgItemMessage(hwndDlg, IDC_LANGUAGE_CB, CB_GETCURSEL, 0, 0) > 0);

				if(iBuffer)
				{
					GetDlgItemText(hwndDlg, IDC_LANGUAGE_CB, szBuffer, MAX_PATH);
					buffer.Format(L"%s\\Languages\\%s.dll", cfg.szCurrentDir, szBuffer);

					hModule = LoadLibraryEx(buffer, 0, LOAD_LIBRARY_AS_DATAFILE);
				}

				SetDlgItemText(hwndDlg, IDC_LANGUAGE_INFO, (iBuffer && !hModule) ? L"unknown" : ls(hModule, IDS_TRANSLATION_INFO));

				if(hModule)
					FreeLibrary(hModule);
			}

			switch(LOWORD(wParam))
			{
				case IDC_OK:
				{
					// Main (Section)
					ini.write(APP_NAME_SHORT, L"CheckUpdateAtStartup", (IsDlgButtonChecked(hwndDlg, IDC_CHECK_UPDATE_AT_STARTUP_CHK) == BST_CHECKED));
					ini.write(APP_NAME_SHORT, L"AlwaysOnTop", (IsDlgButtonChecked(hwndDlg, IDC_ALWAYS_ON_TOP_CHK) == BST_CHECKED));
					
					// Protection (Section)
					ini.write(APP_NAME_SHORT, L"EnableLowLevelFatProtection", (IsDlgButtonChecked(hwndDlg, IDC_ENABLE_LOWLEVEL_FAT_PROTECTION_CHK) == BST_CHECKED));

					// System Config (Section)

					// Language (Section)
					iBuffer = SendDlgItemMessage(hwndDlg, IDC_LANGUAGE_CB, CB_GETCURSEL, 0, 0);
						
					if(cfg.hLocale)
						FreeLibrary(cfg.hLocale);

					if(iBuffer <= 0)
					{
						ini.write(APP_NAME_SHORT, L"Language", 0);
						cfg.hLocale = 0;
					}
					else
					{
						wchar_t szBuffer[MAX_PATH] = {0};

						GetDlgItemText(hwndDlg, IDC_LANGUAGE_CB, szBuffer, MAX_PATH);
						ini.write(APP_NAME_SHORT, L"Language", szBuffer);

						buffer.Format(L"%s\\Languages\\%s.dll", cfg.szCurrentDir, szBuffer);
						cfg.hLocale = LoadLibraryEx(buffer, 0, LOAD_LIBRARY_AS_DATAFILE);
					}


					EnableWindow(GetDlgItem(hwndDlg, IDC_OK), 0);

					break;
				}

				case IDCANCEL: // process Esc key
				case IDC_CANCEL:
				{
					SendMessage(hwndDlg, WM_CLOSE, 0, 0);
					break;
				}
			}

			break;
		}
	}

	return 0;
}

INT_PTR CALLBACK PropertiesDlgProc(HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	CString buffer;
	RECT rc = {0};

	switch(uMsg)
	{
		case WM_INITDIALOG:
		{
			// Centering By Parent
			CenterDialog(hwndDlg);

			// Configure Listview
			SetWindowTheme(GetDlgItem(hwndDlg, IDC_PROPERTIES), L"Explorer", 0);
			SendDlgItemMessage(hwndDlg, IDC_PROPERTIES, LVM_SETEXTENDEDLISTVIEWSTYLE, 0, LVS_EX_FULLROWSELECT | LVS_EX_INFOTIP | LVS_EX_DOUBLEBUFFER);
			SendDlgItemMessage(hwndDlg, IDC_PROPERTIES, LVM_ENABLEGROUPVIEW, TRUE, 0);

			// Insert Columns
			GetClientRect(GetDlgItem(hwndDlg, IDC_PROPERTIES), &rc);

			for(int i = 0; i < 2; i++)
				Lv_InsertColumn(hwndDlg, IDC_PROPERTIES, L"", (rc.right / 2), i, 0);

			// Groups
			Lv_InsertGroup(hwndDlg, IDC_PROPERTIES, L"Общие", 0);
			Lv_InsertGroup(hwndDlg, IDC_PROPERTIES, L"Ёмкость", 1);

			// Insert Items
			Lv_InsertItem(hwndDlg, IDC_PROPERTIES, L"Диск", 0, 0, -1, 0);
			Lv_InsertItem(hwndDlg, IDC_PROPERTIES, L"Статус", 1, 0, -1, 0);
			Lv_InsertItem(hwndDlg, IDC_PROPERTIES, L"Метка тома", 2, 0, -1, 0);
			Lv_InsertItem(hwndDlg, IDC_PROPERTIES, L"Тип", 3, 0, -1, 0);
			Lv_InsertItem(hwndDlg, IDC_PROPERTIES, L"Файловая система", 4, 0, -1, 0);
			Lv_InsertItem(hwndDlg, IDC_PROPERTIES, L"Серийный номер", 5, 0, -1, 0);

			Lv_InsertItem(hwndDlg, IDC_PROPERTIES, L"Объём", 6, 0, -1, 1);
			Lv_InsertItem(hwndDlg, IDC_PROPERTIES, L"Занято", 7, 0, -1, 1);
			Lv_InsertItem(hwndDlg, IDC_PROPERTIES, L"Свободно", 8, 0, -1, 1);

			SendMessage(hwndDlg, WM_COMMAND, MAKELPARAM(IDM_PROPERTIES_REFRESH, 0), 0);

			break;
		}

		case WM_CLOSE:
		{
			EndDialog(hwndDlg, 0);
			break;
		}

		case WM_CONTEXTMENU:
		{
			if(GetDlgCtrlID((HWND)wParam) == IDC_DRIVES)
			{
				// Load Menu
				HMENU hMenu = LoadMenu(NULL, MAKEINTRESOURCE(IDM_PROPERTIES_MENU)), hSubMenu = GetSubMenu(hMenu, 0);

				if(!SendDlgItemMessage(hwndDlg, IDC_PROPERTIES, LVM_GETSELECTEDCOUNT, 0, 0))
					EnableMenuItem(hSubMenu, IDM_PROPERTIES_COPY, MF_BYCOMMAND | MF_DISABLED);

				// Show Menu
				TrackPopupMenuEx(hSubMenu, TPM_LEFTALIGN | TPM_RIGHTBUTTON | TPM_LEFTBUTTON | TPM_NOANIMATION, LOWORD(lParam), HIWORD(lParam), hwndDlg, NULL);

				// Destroy Menu
				DestroyMenu(hMenu);
				DestroyMenu(hSubMenu);
			}

			break;
		}

		case WM_PAINT:
		{
			PAINTSTRUCT ps = {0};
			HDC hDC = BeginPaint(hwndDlg, &ps);

			GetClientRect(hwndDlg, &rc);
			rc.top = rc.bottom - 43;

			// Instead FillRect
			COLORREF clrOld = SetBkColor(hDC, GetSysColor(COLOR_BTNFACE));
			ExtTextOut(hDC, 0, 0, ETO_OPAQUE, &rc, NULL, 0, NULL);
			SetBkColor(hDC, clrOld);

			// Draw Line
			for(int i = 0; i < rc.right; i++)
				SetPixel(hDC, i, rc.top, GetSysColor(COLOR_BTNSHADOW));

			EndPaint(hwndDlg, &ps);

			return 0;
		}

		case WM_CTLCOLORSTATIC:
		case WM_CTLCOLORDLG:
		{
			return (INT_PTR)GetSysColorBrush(COLOR_WINDOW);
		}

		case WM_NOTIFY:
		{
			LPNMHDR lpnmhdr = (LPNMHDR)lParam;

			switch(lpnmhdr->code)
			{
				case NM_CUSTOMDRAW:
				{
					LONG lResult = CDRF_DODEFAULT;
					LPNMLVCUSTOMDRAW lplvcd = (LPNMLVCUSTOMDRAW)lParam;

					switch(lplvcd->nmcd.dwDrawStage)
					{
						case CDDS_PREPAINT:
						{
							lResult = CDRF_NOTIFYITEMDRAW;
							break;
						}

						case CDDS_ITEMPREPAINT:
						{
							lResult = CDRF_NOTIFYSUBITEMDRAW;
							break;
						}

						case CDDS_SUBITEM | CDDS_ITEMPREPAINT:
						{
							if(lplvcd->iSubItem == 1)
							{
								lplvcd->clrText = RGB(90, 90, 90);
								lResult = CDRF_NEWFONT;
							}

							break;
						}
					}

					SetWindowLong(hwndDlg, DWL_MSGRESULT, lResult);
					return 1;
				}
			}

			break;
		}

		case WM_COMMAND:
		{
			switch(LOWORD(wParam))
			{
				case IDM_PROPERTIES_REFRESH:
				{
					DWORD dwSerial = 0;
					ULARGE_INTEGER uiTotal = {0}, uiFree = {0};
					WCHAR szLabel[MAX_PATH] = {0}, szFileSystem[MAX_PATH] = {0};

					buffer = cfg.cDrive;
					buffer.Append(L":\\\0");

					if(!DriveIsReady(buffer))
					{
						for(int i = 0; i < SendDlgItemMessage(hwndDlg, IDC_PROPERTIES, LVM_GETITEMCOUNT, 0, 0); i++)
							Lv_InsertItem(hwndDlg, IDC_PROPERTIES, i ? L"n/a" : buffer.Mid(0, 2), i, 1);

						return 0;
					}

					Lv_InsertItem(hwndDlg, IDC_PROPERTIES, buffer.Mid(0, 2), 0, 1);

					switch(GetDriveStatus(buffer))
					{
						case DS_PROTECTED:
							Lv_InsertItem(hwndDlg, IDC_PROPERTIES, L"Защищён", 1, 1);
							break;

						case DS_INFECTED:
							Lv_InsertItem(hwndDlg, IDC_PROPERTIES, L"Заражён", 1, 1);
							break;
							
						case DS_LOCKED:
							Lv_InsertItem(hwndDlg, IDC_PROPERTIES, L"Заблокирован", 1, 1);
							break;

						default:
							Lv_InsertItem(hwndDlg, IDC_PROPERTIES, L"Не заражён", 1, 1);
							break;
					}

					if(!DriveIsLocked(buffer))
					{
						GetVolumeInformation(buffer, szLabel, MAX_PATH, &dwSerial, NULL, NULL, szFileSystem, MAX_PATH);
						GetDiskFreeSpaceEx(buffer, 0, &uiTotal, &uiFree);
					}
					else
					{
						dwSerial = dl[GetDriveNumber(buffer)].dwSerial;

						uiTotal = dl[GetDriveNumber(buffer)].uiTotal;
						uiFree = dl[GetDriveNumber(buffer)].uiFree;
					}

					Lv_InsertItem(hwndDlg, IDC_PROPERTIES, DriveIsLocked(buffer) ? dl[GetDriveNumber(buffer)].szLabel : szLabel, 2, 1);

					switch(GetDriveType(buffer))
					{
						case DRIVE_REMOVABLE:
							Lv_InsertItem(hwndDlg, IDC_PROPERTIES,  L"Съёмный диск", 3, 1);
							break;

						case DRIVE_FIXED:
							Lv_InsertItem(hwndDlg, IDC_PROPERTIES,  L"Локальный диск", 3, 1);
							break;

						case DRIVE_REMOTE:
							Lv_InsertItem(hwndDlg, IDC_PROPERTIES,  L"Сетевой диск", 3, 1);
							break;

						case DRIVE_CDROM:
							Lv_InsertItem(hwndDlg, IDC_PROPERTIES,  L"CD-ROM", 3, 1);
							break;

						default:
							Lv_InsertItem(hwndDlg, IDC_PROPERTIES,  L"Неизвестно", 3, 1);
							break;
					}

					Lv_InsertItem(hwndDlg, IDC_PROPERTIES, DriveIsLocked(buffer) ? dl[GetDriveNumber(buffer)].szFileSystem : szFileSystem, 4, 1);

					buffer.Format(L"%04X-%04X", dwSerial >> 16, dwSerial & 0xFFFF);
					Lv_InsertItem(hwndDlg, IDC_PROPERTIES, buffer, 5, 1);
					
					Lv_InsertItem(hwndDlg, IDC_PROPERTIES, number_format(uiTotal.QuadPart / 1048576, L" mb"), 6, 1);
					Lv_InsertItem(hwndDlg, IDC_PROPERTIES, number_format((uiTotal.QuadPart - uiFree.QuadPart) / 1048576, L" mb"), 7, 1);
					Lv_InsertItem(hwndDlg, IDC_PROPERTIES, number_format(uiFree.QuadPart / 1048576, L" mb"), 8, 1);

					break;
				}
				
				case IDM_PROPERTIES_COPY:
				{
					ClipboardPut(Lv_GetItemText(hwndDlg, IDC_PROPERTIES, -1, 1));
					break;
				}

				case IDCANCEL: // process Esc key
				case IDC_OK:
				{
					SendMessage(hwndDlg, WM_CLOSE, 0, 0);
					break;
				}
			}

			break;
		}
	}

	return 0;
}

/*
	iMode:
		0 - check "NoDriveTypeAutoRun" values
		1 - check "IniFileMapping" values

	bSwitch:
		0 - check "NoDriveTypeAutoRun" values
		1 - check "IniFileMapping" values

	return values:
		0 - autorun is enabled
		1 - autorun is disabled (system protected)
*/
BOOL AutorunRegistryCheck(INT iMode, BOOL bProtect, BOOL bSwitch)
{
	HKEY hKey = NULL;
	DWORD dwType = 0, dwValue = 0, dwSize = sizeof(dwValue);
	WCHAR szBuffer[MAX_PATH] = {0};

	if(iMode == 0)
	{
		HKEY hKeyRoots[] = {HKEY_LOCAL_MACHINE, HKEY_CURRENT_USER};

		for(int i = 0; i < (sizeof(hKeyRoots) / sizeof(hKeyRoots[0])); i++)
		{
			if(RegOpenKeyEx(hKeyRoots[i], L"Software\\Microsoft\\Windows\\CurrentVersion\\Policies\\Explorer", 0, KEY_ALL_ACCESS, &hKey) == ERROR_SUCCESS)
			{
				if(bProtect)
				{
					dwValue = bSwitch ? 145 : 255;
					dwSize = sizeof(dwValue);

					RegSetValueEx(hKey, L"NoDriveTypeAutoRun", 0, REG_DWORD, (LPBYTE)&dwValue, dwSize);

					RegCloseKey(hKey);
				}
				else
				{
					RegQueryValueEx(hKey, L"NoDriveTypeAutoRun", 0, &dwType, (LPBYTE)&dwValue, &dwSize);
					RegCloseKey(hKey);

					if(dwValue != 255)
						return 0;
				}
			}
		}
	}
	else if(iMode == 1)
	{
		if(RegOpenKeyEx(HKEY_LOCAL_MACHINE, L"SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion\\IniFileMapping\\Autorun.inf", 0, KEY_ALL_ACCESS, &hKey) == ERROR_SUCCESS)
		{
			if(bProtect)
			{
				if(bSwitch)
				{
					StringCchCopy(szBuffer, MAX_PATH, L"@SYS:DoesNotExist");
					RegSetValueEx(hKey, NULL, 0, REG_DWORD, (LPBYTE)szBuffer, MAX_PATH);
				}
				else
				{
					RegDeleteValue(hKey, NULL);
				}

				RegCloseKey(hKey);
			}
			else
			{
				dwSize = MAX_PATH + sizeof(szBuffer);

				RegQueryValueEx(hKey, NULL, 0, &dwType, (LPBYTE)&szBuffer, &dwSize);
				RegCloseKey(hKey);

				if(lstrcmpi(szBuffer, L"@SYS:DoesNotExist"))
					return 0;
			}
		}
	}

	return 1;
}

LRESULT CALLBACK DlgProc(HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	CString buffer, errbuf;
	INT iBuffer = 0;

	if(uMsg == WM_MUTEX)
		return WmMutexWrapper(hwndDlg, wParam, lParam);

	switch(uMsg)
	{
		case WM_INITDIALOG:
		{
			// Check User Admin
			if(!IsAdmin())
			{
				MessageBox(hwndDlg, MB_OK | MB_ICONSTOP | MB_TOPMOST, APP_NAME, L"Для полноценной работы программы необходимо иметь права Администратора");
				SendMessage(hwndDlg, WM_CLOSE, 0, 0);

				return 0;
			}

			// Check Mutex
			CreateMutex(NULL, TRUE, APP_NAME_SHORT);

			if(GetLastError() == ERROR_ALREADY_EXISTS)
			{
				PostMessage(HWND_BROADCAST, WM_MUTEX, GetCurrentProcessId(), 1);
				SendMessage(hwndDlg, WM_CLOSE, 0, 0);

				return 0;
			}

			// Load Settings
			GetModuleFileName(0, buffer.GetBuffer(MAX_PATH), MAX_PATH);
			PathRenameExtension(buffer.GetBuffer(MAX_PATH), L".cfg");
			ini.load(buffer);

			// Current Dir
			PathRemoveFileSpec(buffer.GetBuffer(MAX_PATH));
			StringCchCopy(cfg.szCurrentDir, MAX_PATH, buffer);

			// Set Window Title
			SetWindowText(hwndDlg, APP_NAME L" " APP_VERSION);

			// Set Icons
			SendMessage(hwndDlg, WM_SETICON, ICON_BIG, (LPARAM)LoadImage(GetModuleHandle(0), MAKEINTRESOURCE(IDI_MAIN), IMAGE_ICON, 32, 32, 0));
			SendMessage(hwndDlg, WM_SETICON, ICON_SMALL, (LPARAM)LoadImage(GetModuleHandle(0), MAKEINTRESOURCE(IDI_MAIN), IMAGE_ICON, 16, 16, 0));

			// Modify System Menu
			HMENU hMenu = GetSystemMenu(hwndDlg, 0);
			InsertMenu(hMenu, -1, MF_BYPOSITION | MF_SEPARATOR, 0, NULL);
			InsertMenu(hMenu, -1, MF_BYPOSITION | MF_STRING, IDM_ABOUT, L"О программе");

			// Configure Listview
			SetWindowTheme(GetDlgItem(hwndDlg, IDC_DRIVES), L"Explorer", 0);
			SendDlgItemMessage(hwndDlg, IDC_DRIVES, LVM_SETEXTENDEDLISTVIEWSTYLE, 0, LVS_EX_FULLROWSELECT | LVS_EX_INFOTIP | LVS_EX_DOUBLEBUFFER);

			// Insert Columns
			Lv_InsertColumn(hwndDlg, IDC_DRIVES, L"Диск", 50, 0, 0);
			Lv_InsertColumn(hwndDlg, IDC_DRIVES, L"Метка тома", 90, 1, 0);
			Lv_InsertColumn(hwndDlg, IDC_DRIVES, L"Тип", 105, 2, 0);
			Lv_InsertColumn(hwndDlg, IDC_DRIVES, L"Файловая система", 115, 3, 0);
			Lv_InsertColumn(hwndDlg, IDC_DRIVES, L"Статус", 95, 4, 0);

			// Status Bar
			INT iParts[] = {120, 240, 360, -1};
			SendDlgItemMessage(hwndDlg, IDC_STATUSBAR, SB_SETPARTS, 4, (LPARAM)&iParts);

			// Imagelist
			HIMAGELIST hImgList = ImageList_Create(16, 16, ILC_COLOR32, 0, 5);

			for(int i = IDI_NORMAL, j = 0; i < (IDI_LOCKED + 1); i++, j++)
			{
				HICON hIcon = LoadIcon(GetModuleHandle(NULL), MAKEINTRESOURCE(i));
				ImageList_AddIcon(hImgList, hIcon);
				SendDlgItemMessage(hwndDlg, IDC_STATUSBAR, SB_SETICON, j, (LPARAM)ImageList_GetIcon(hImgList, j, ILD_NORMAL));
				DestroyIcon(hIcon);
			}

			SendDlgItemMessage(hwndDlg, IDC_DRIVES, LVM_SETIMAGELIST, LVSIL_SMALL, (LPARAM)hImgList);

			// Update Drive List
			RefreshDrivesList(hwndDlg);
			
			// Always on Top
			iBuffer = ini.read(APP_NAME_SHORT, L"AlwaysOnTop", 0);
			SetWindowPos(hwndDlg, (iBuffer ? HWND_TOPMOST : HWND_NOTOPMOST), 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);

			// Check Updates
			iBuffer = ini.read(APP_NAME_SHORT, L"CheckUpdateAtStartup", 1);

			if(iBuffer)
				_beginthreadex(NULL, 0, &CheckUpdates, (LPVOID)1, 0, NULL);


			LOGFONT lf = {0};
			GetObject((HFONT)SendMessage(hwndDlg, WM_GETFONT, 0, 0), sizeof(lf), &lf);
			lf.lfWeight = FW_BOLD;

			cfg.hBold = CreateFontIndirect(&lf);


			/*
			FS_FILE_INFO fsfi = {0};
			DWORD dw = 0;
			LARGE_INTEGER sdf = {0};

			//if(FsGetFileInfo(L"f:\\autorun.inf", &fsfi) /*&& FsGetVolumeInfo(L"h:", &fsvi) && FsGetFileExtents(&fsvi, &fsfi, &fsfe)*)
			//{

				BY_HANDLE_FILE_INFORMATION bhi = {0};

				HANDLE hFile = CreateFile(L"h:\\ai.dat",FILE_READ_ATTRIBUTES,FILE_SHARE_READ | FILE_SHARE_WRITE, (LPSECURITY_ATTRIBUTES)NULL, OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,NULL);
				GetFileInformationByHandle(hFile, &bhi);
				CloseHandle(hFile);

				//sdf = fsfi.liFileIndex;
				//FsFreeFileInfo(&fsfi);

				HANDLE hDrive = CreateFile(L"\\\\.\\h:", GENERIC_READ|GENERIC_WRITE, FILE_SHARE_READ|FILE_SHARE_WRITE, 0, OPEN_EXISTING, FILE_FLAG_NO_BUFFERING|FILE_FLAG_WRITE_THROUGH, 0);

				//DeviceIoControl(hDrive, FSCTL_LOCK_VOLUME, NULL, 0, NULL, 0, &dw, NULL);
				DeviceIoControl(hDrive, FSCTL_DISMOUNT_VOLUME, NULL, 0, NULL, 0, &dw, NULL);

				BYTE buff[2048] = {0};


				sdf.LowPart  = bhi.nFileIndexLow;
				sdf.HighPart = bhi.nFileIndexHigh;


				BOOL sfp = SetFilePointerEx(hDrive, sdf, NULL, FILE_BEGIN);
				sfp = GetLastError();

				BOOL rf = ReadFile(hDrive, &buff, 2048, &dw, 0);
				rf = GetLastError();


				
				CloseHandle(hDrive);



				HANDLE hWriteFile = CreateFile(L"file.txt",GENERIC_WRITE,NULL,NULL,CREATE_ALWAYS,FILE_FLAG_NO_BUFFERING|FILE_FLAG_WRITE_THROUGH, NULL);

				DWORD dwBytesWritten;

				WriteFile (hWriteFile, &buff, dw, &dwBytesWritten, NULL);
				CloseHandle(hWriteFile);

				MessageBox(0,0,0, L"%d", sdf);
				//MessageBoxA(0, buff,0,0);
				
				//D viceIoControl(hDrive, FSCTL_UNLOCK_VOLUME, NULL, 0, NULL, 0, &dw, NULL);
			//}
				//MessageBoxA(0, buff,0,0);
			//}

			//FsFreeVolumeInfo(&fsvi);


			ExitProcess(1);*/

			break;
		}

		case WM_DEVICECHANGE:
		{
			RefreshDrivesList(hwndDlg);
			break;
		}

		case WM_CLOSE:
		{
			for(int i = 0; i < 26; i++)
			{
				if(dl[i].hDrive)
				{
					if(MessageBox(hwndDlg, MB_ICONINFORMATION | MB_YESNO, APP_NAME, L"Остались заблокированные диски, при выходе из программы они будут разблокированы.\n\nВыйти из программы?") == IDYES)
					{
						for(; i < 26; i++)
						{
							if(dl[i].hDrive)
							{
								buffer.Format(L"%c:\\\0", i + 65);
								UnlockDrive(buffer);
							}
						}
					}
					else
					{
						return 1;
					}
				}
			}

			if(cfg.hBold)
				DeleteObject(cfg.hBold);

			DestroyWindow(hwndDlg);
			PostQuitMessage(0);

			break;
		}

		case WM_CONTEXTMENU:
		{
			if(GetDlgCtrlID((HWND)wParam) == IDC_DRIVES)
			{
				// Load Menu
				HMENU hMenu = LoadMenu(NULL, MAKEINTRESOURCE(IDM_DRIVES_MENU)), hSubMenu = GetSubMenu(hMenu, 0);

				// Set Default Menu Item
				SetMenuDefaultItem(hSubMenu, IDM_OPEN, 0);

				iBuffer = SendDlgItemMessage(hwndDlg, IDC_DRIVES, LVM_GETSELECTEDCOUNT, 0, 0);

				if(!iBuffer)
				{
					EnableMenuItem(hSubMenu, IDM_OPEN, MF_BYCOMMAND | MF_DISABLED);
					EnableMenuItem(hSubMenu, IDM_PROTECT, MF_BYCOMMAND | MF_DISABLED);
					EnableMenuItem(hSubMenu, IDM_UNPROTECT, MF_BYCOMMAND | MF_DISABLED);
					EnableMenuItem(hSubMenu, IDM_LOCK, MF_BYCOMMAND | MF_DISABLED);
					EnableMenuItem(hSubMenu, IDM_UNLOCK, MF_BYCOMMAND | MF_DISABLED);
					EnableMenuItem(hSubMenu, IDM_CHKDSK, MF_BYCOMMAND | MF_DISABLED);
					EnableMenuItem(hSubMenu, IDM_CHKDSK_REPAIR, MF_BYCOMMAND | MF_DISABLED);
					EnableMenuItem(hSubMenu, IDM_EJECT, MF_BYCOMMAND | MF_DISABLED);
					EnableMenuItem(hSubMenu, IDM_FORMAT, MF_BYCOMMAND | MF_DISABLED);
					EnableMenuItem(hSubMenu, IDM_PROPERTIES, MF_BYCOMMAND | MF_DISABLED);
				}
				else if(iBuffer > 1)
				{
					EnableMenuItem(hSubMenu, IDM_CHKDSK, MF_BYCOMMAND | MF_DISABLED);
					EnableMenuItem(hSubMenu, IDM_CHKDSK_REPAIR, MF_BYCOMMAND | MF_DISABLED);
					EnableMenuItem(hSubMenu, IDM_FORMAT, MF_BYCOMMAND | MF_DISABLED);
					EnableMenuItem(hSubMenu, IDM_PROPERTIES, MF_BYCOMMAND | MF_DISABLED);
				}

				if(iBuffer && dl[GetDriveNumber(Lv_GetItemText(hwndDlg, IDC_DRIVES, -1, 0))].hDrive)
				{
					EnableMenuItem(hSubMenu, IDM_OPEN, MF_BYCOMMAND | MF_DISABLED);
					EnableMenuItem(hSubMenu, IDM_PROTECT, MF_BYCOMMAND | MF_DISABLED);
					EnableMenuItem(hSubMenu, IDM_UNPROTECT, MF_BYCOMMAND | MF_DISABLED);
					EnableMenuItem(hSubMenu, IDM_LOCK, MF_BYCOMMAND | MF_DISABLED);
					EnableMenuItem(hSubMenu, IDM_CHKDSK, MF_BYCOMMAND | MF_DISABLED);
					EnableMenuItem(hSubMenu, IDM_CHKDSK_REPAIR, MF_BYCOMMAND | MF_DISABLED);
					EnableMenuItem(hSubMenu, IDM_EJECT, MF_BYCOMMAND | MF_DISABLED);
					EnableMenuItem(hSubMenu, IDM_FORMAT, MF_BYCOMMAND | MF_DISABLED);
				}

				// Show Menu
				TrackPopupMenuEx(hSubMenu, TPM_LEFTALIGN | TPM_RIGHTBUTTON | TPM_LEFTBUTTON | TPM_NOANIMATION, LOWORD(lParam), HIWORD(lParam), hwndDlg, NULL);

				// Destroy Menu
				DestroyMenu(hMenu);
				DestroyMenu(hSubMenu);
			}

			break;
		}
		
		case WM_NOTIFY:
		{
			LPNMHDR lpnmhdr = (LPNMHDR)lParam;

			switch(lpnmhdr->code)
			{
				case NM_DBLCLK:
				{
					if(lpnmhdr->idFrom == IDC_DRIVES)
					{
						LPNMITEMACTIVATE lpnmia = (LPNMITEMACTIVATE)lParam;

						if(lpnmia->iItem != -1)
							SendMessage(hwndDlg, WM_COMMAND, MAKELPARAM(IDM_OPEN, 0), 0);
					}

					break;
				}
			}

			break;
		}

		/*
		case WM_KEYDOWN:
		{
			MessageBox(0,L"",0,0);

			if(wParam == VK_F1)
				SendMessage(hwndDlg, WM_COMMAND, MAKELPARAM(IDM_HELP, 0), 0);

			break;
		}
		*/
		
		case WM_SYSCOMMAND:
		{
			if(wParam == IDM_ABOUT)
				SendMessage(hwndDlg, WM_COMMAND, MAKELPARAM(IDM_ABOUT, 0), 0);

			break;
		}

		case WM_COMMAND:
		{
			switch(LOWORD(wParam))
			{
				case IDM_SETTINGS:
				{
					DialogBox(NULL, MAKEINTRESOURCE(IDD_SETTINGS), hwndDlg, SettingsDlgProc);
					break;
				}

				case IDCANCEL: // process Esc key
				case IDM_EXIT:
				{
					SendMessage(hwndDlg, WM_CLOSE, 0, 0);
					break;
				}

				case IDM_WEBSITE:
				{
					ShellExecute(hwndDlg, 0, APP_WEBSITE, NULL, NULL, SW_SHOWDEFAULT);
					break;
				}

				case IDM_CHECK_UPDATES:
				{
					_beginthreadex(NULL, 0, &CheckUpdates, 0, 0, NULL);
					break;
				}

				case IDM_ABOUT:
				{
					buffer.Format(L"Программа распространяется под лицензией\r\nGNU General Public License v2.\r\n\r\nДанная программа является полностью бесплатной, спасибо за то что вы её выбрали.\r\n\r\n<a href=\"%s\">%s</a>", APP_WEBSITE, APP_HOST);
					AboutBoxCreate(hwndDlg, MAKEINTRESOURCE(IDI_MAIN), L"О программе", APP_NAME L" " APP_VERSION, L"Copyright © 2012 Henry++\r\nAll Rights Reversed\r\n\r\n" + buffer);

					break;
				}

				case IDM_PROPERTIES:
				{
					cfg.cDrive = Lv_GetItemText(hwndDlg, IDC_DRIVES, -1, 0)[0];
					DialogBox(NULL, MAKEINTRESOURCE(IDD_PROPERTIES), hwndDlg, PropertiesDlgProc);

					break;
				}

				case IDM_OPEN:
				{
					iBuffer = -1;

					if(SendDlgItemMessage(hwndDlg, IDC_DRIVES, LVM_GETSELECTEDCOUNT, 0, 0))
					{
						while((iBuffer = SendDlgItemMessage(hwndDlg, IDC_DRIVES, LVM_GETNEXTITEM, iBuffer, LVNI_SELECTED)) >= 0)
						{
							if(!DriveIsLocked(Lv_GetItemText(hwndDlg, IDC_DRIVES, iBuffer, 0)))
							{
								buffer.Format(L"\"explorer.exe\" \"%s\"", Lv_GetItemText(hwndDlg, IDC_DRIVES, iBuffer, 0));
								ExecuteCommand(buffer.GetBuffer());
							}
						}
					}

					break;
				}

				case IDM_REFRESH:
				{
					RefreshDrivesList(hwndDlg);
					break;
				}
				
				case IDM_PROTECT:
				{
					iBuffer = -1;

					if(SendDlgItemMessage(hwndDlg, IDC_DRIVES, LVM_GETSELECTEDCOUNT, 0, 0))
					{
						while((iBuffer = SendDlgItemMessage(hwndDlg, IDC_DRIVES, LVM_GETNEXTITEM, iBuffer, LVNI_SELECTED)) >= 0)
							ProtectDrive(Lv_GetItemText(hwndDlg, IDC_DRIVES, iBuffer, 0));
	
						RefreshDrivesList(hwndDlg);
					}

					break;
				}

				case IDM_UNPROTECT:
				{
					iBuffer = -1;

					if(SendDlgItemMessage(hwndDlg, IDC_DRIVES, LVM_GETSELECTEDCOUNT, 0, 0))
					{
						while((iBuffer = SendDlgItemMessage(hwndDlg, IDC_DRIVES, LVM_GETNEXTITEM, iBuffer, LVNI_SELECTED)) >= 0)
							UnprotectDrive(Lv_GetItemText(hwndDlg, IDC_DRIVES, iBuffer, 0));

						RefreshDrivesList(hwndDlg);
					}

					break;
				}

				case IDM_LOCK:
				{
					iBuffer = -1;

					if(SendDlgItemMessage(hwndDlg, IDC_DRIVES, LVM_GETSELECTEDCOUNT, 0, 0))
					{
						while((iBuffer = SendDlgItemMessage(hwndDlg, IDC_DRIVES, LVM_GETNEXTITEM, iBuffer, LVNI_SELECTED)) >= 0)
							LockDrive(Lv_GetItemText(hwndDlg, IDC_DRIVES, iBuffer, 0));

						RefreshDrivesList(hwndDlg);
					}

					break;
				}

				case IDM_UNLOCK:
				{
					iBuffer = -1;

					if(SendDlgItemMessage(hwndDlg, IDC_DRIVES, LVM_GETSELECTEDCOUNT, 0, 0))
					{
						while((iBuffer = SendDlgItemMessage(hwndDlg, IDC_DRIVES, LVM_GETNEXTITEM, iBuffer, LVNI_SELECTED)) >= 0)
							UnlockDrive(Lv_GetItemText(hwndDlg, IDC_DRIVES, iBuffer, 0));

						RefreshDrivesList(hwndDlg);
					}

					break;
				}

				case IDM_UNLOCK_ALL:
				{
					for(int i = 0; i < 26; i++)
					{
						if(dl[i].hDrive)
						{
							if(MessageBox(hwndDlg, MB_YESNO | MB_ICONWARNING, APP_NAME, L"Вы действительно хотите разблокировать все диски?") == IDYES)
							{
								for(; i < 26; i++)
								{
									if(dl[i].hDrive)
									{
										buffer.Format(L"%c:\\\0", i + 65);
										UnlockDrive(buffer);
									}
								}

								RefreshDrivesList(hwndDlg);
								return 0;
							}
						}
					}

					MessageBox(hwndDlg, MB_OK | MB_ICONINFORMATION, APP_NAME, L"Ни одного диска не заблокировано");
					break;
				}

				case IDM_CHKDSK:
				{
					if(SendDlgItemMessage(hwndDlg, IDC_DRIVES, LVM_GETSELECTEDCOUNT, 0, 0))
					{
						buffer.Format(L"cmd.exe /k echo %s&echo.&chkdsk %s", APP_NAME L" " APP_VERSION, Lv_GetItemText(hwndDlg, IDC_DRIVES, -1, 0));
						ExecuteCommand(buffer.GetBuffer());
					}

					break;
				}

				case IDM_CHKDSK_REPAIR:
				{
					if(SendDlgItemMessage(hwndDlg, IDC_DRIVES, LVM_GETSELECTEDCOUNT, 0, 0))
					{
						buffer.Format(L"cmd.exe /k echo %s&echo.&chkdsk /f %s", APP_NAME L" " APP_VERSION, Lv_GetItemText(hwndDlg, IDC_DRIVES, -1, 0));
						ExecuteCommand(buffer.GetBuffer());
					}

					break;
				}

				case IDM_EJECT:
				{
					iBuffer = -1;

					if(SendDlgItemMessage(hwndDlg, IDC_DRIVES, LVM_GETSELECTEDCOUNT, 0, 0))
					{
						while((iBuffer = SendDlgItemMessage(hwndDlg, IDC_DRIVES, LVM_GETNEXTITEM, iBuffer, LVNI_SELECTED)) >= 0)
						{
							buffer = Lv_GetItemText(hwndDlg, IDC_DRIVES, iBuffer, 0)[0];
							errbuf.Append(EjectDrive(buffer) ? buffer + L" - диск успешно извлечён\r\n" : buffer + L" - ошибка извлечения, возможно диск используется\r\n");
						}

						if(!errbuf.IsEmpty())
						{
							errbuf.Trim();
							MessageBox(hwndDlg, MB_OK | MB_ICONINFORMATION, APP_NAME, errbuf);
						}

						RefreshDrivesList(hwndDlg);
					}
					
					break;
				}

				case IDM_FORMAT:
				{
					if(SendDlgItemMessage(hwndDlg, IDC_DRIVES, LVM_GETSELECTEDCOUNT, 0, 0))
					{
						SHFormatDrive(hwndDlg, GetDriveNumber(Lv_GetItemText(hwndDlg, IDC_DRIVES, -1, 0)), SHFMT_ID_DEFAULT, 0);
						RefreshDrivesList(hwndDlg);
					}

					break;
				}
			}
		}
	}

	return 0;
}

INT APIENTRY wWinMain(HINSTANCE hInstance, HINSTANCE, LPWSTR lpCmdLine, INT nShowCmd)
{
	MSG msg = {0};
	INITCOMMONCONTROLSEX icex = {0};

	icex.dwSize = sizeof(icex);
	icex.dwICC = ICC_WIN95_CLASSES | ICC_STANDARD_CLASSES;

	if(!InitCommonControlsEx(&icex))
		return 0;

	if(!(cfg.hWnd = CreateDialog(hInstance, MAKEINTRESOURCE(IDD_MAIN), NULL, (DLGPROC)DlgProc)))
		return 0;

	while(GetMessage(&msg, NULL, 0, 0))
	{
		if(!IsDialogMessage(cfg.hWnd, &msg))
		{
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
	}
	
	return msg.wParam;
}