/************************************
*  	Drive Dot Shield
*	Copyright © 2012 Henry++
*
*	GNU General Public License v2
*	http://www.gnu.org/licenses/
*
*	http://www.henrypp.org/
*************************************/

#ifndef __DRIVEDOTSHIELD_H__
#define __DRIVEDOTSHIELD_H__

// Define
#define APP_NAME L"Drive Dot Shield"
#define APP_NAME_SHORT L"drivedotshield"
#define APP_VERSION L"2.0"
#define APP_VERSION_RES 2,0
#define APP_HOST L"www.henrypp.org"
#define APP_WEBSITE L"http://" APP_HOST

// Settings Structure
struct CONFIG
{
	HWND hWnd; // main window handle

	HINSTANCE hLocale; // language module handle
	HFONT hBold; // bold font for titles

	WCHAR cDrive; // current drive
	WCHAR szCurrentDir[MAX_PATH]; // current directory
};

// Locked Drive Info Structure
struct DRIVE_LOCK
{
	HANDLE hDrive;

	WCHAR szLabel[MAX_PATH];
	WCHAR szFileSystem[MAX_PATH];

	DWORD dwSerial;

	ULARGE_INTEGER uiTotal, uiFree;
};

// Drive Status Enumeration
enum DRIVE_STATUS
{
	DS_NORMAL,
	DS_PROTECTED,
	DS_INFECTED, // maybe
	DS_LOCKED
};

//
// IOCTL to obtain general information about
// the specified volume.
//

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


/*
//void NtCreateDirectory(PHANDLE hHandle, PIO_STATUS_BLOCK iosb, POBJECT_ATTRIBUTES objatr, PUNICODE_STRING usString)
//{
//	InitializeObjectAttributes(objatr, usString, OBJ_CASE_INSENSITIVE, NULL, NULL);

//	NtCreateFile(hHandle, FILE_LIST_DIRECTORY | SYNCHRONIZE | FILE_OPEN_FOR_BACKUP_INTENT, objatr, iosb, NULL, FILE_ATTRIBUTE_SYSTEM | FILE_ATTRIBUTE_NORMAL, FILE_SHARE_READ, FILE_CREATE, FILE_DIRECTORY_FILE | FILE_SYNCHRONOUS_IO_NONALERT, NULL, 0);
//	NtClose(hHandle);
//}

bool NtCreateDirectory(LPWSTR lpDirPathName)
{
/*	if(Lib_IsFileExists(lpDirPathName))
		return 0;

	UNICODE_STRING usString;
	HANDLE hHandle;
	IO_STATUS_BLOCK iosb = {0};
	OBJECT_ATTRIBUTES objatr = {0};
	RtlInitUnicodeString(&usString, lpDirPathName);
	RtlDosPathNameToNtPathName_U(lpDirPathName, &usString, NULL, NULL);

	NtCreateDirectory(&hHandle, &iosb, &objatr, &usString);

	wcscat(usString.Buffer, L"\\..\\");
	usString.Length += 4 * sizeof(WCHAR);

	NtCreateDirectory(&hHandle, &iosb, &objatr, &usString);


	return 1;
}

// Class
class DDS
{
	public:
		// Protect Drive
		bool Drive_Protect(wchar_t* szDrive)
		{
			HANDLE hFile1 = NULL, hFile2 = NULL;
			BOOL bRet = 1;
			wchar_t szDirectory[MAX_PATH] = L"\0", szFile1[MAX_PATH] = L"\0", szFile2[MAX_PATH] = L"\0", szBuffer[MAX_PATH] = L"\0";

			// First - Unprotect
			Drive_UnProtect(szDrive);

			// Generate Random Name
			wsprintf(szDirectory, L"\\\\?\\%s\\autorun.inf", szDrive);

			srand(time(0));

			Lib_GenRandomName(Lib_Random(5, 10), szBuffer);
			wsprintf(szFile1, L"\\\\?\\%s\\autorun.inf\\%s.", szDrive, szBuffer); // Dot Method

			Lib_GenRandomName(Lib_Random(5, 10), szBuffer);
			wsprintf(szFile2, L"\\\\?\\%s\\autorun.inf\\%s :stream", szDrive, szBuffer); // Space Method

			if(NtCreateDirectory(szDirectory))
				bRet = 0;

			SetFileAttributes(szDirectory, FILE_ATTRIBUTE_HIDDEN | FILE_ATTRIBUTE_READONLY | FILE_ATTRIBUTE_SYSTEM);
					
			hFile1 = CreateFile(szFile1, GENERIC_WRITE | GENERIC_READ, FILE_SHARE_WRITE, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_HIDDEN | FILE_ATTRIBUTE_READONLY | FILE_ATTRIBUTE_SYSTEM, NULL);
			hFile2 = CreateFile(szFile2, GENERIC_WRITE | GENERIC_READ, FILE_SHARE_WRITE, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_HIDDEN | FILE_ATTRIBUTE_READONLY | FILE_ATTRIBUTE_SYSTEM, NULL);

			if(!hFile1 || !hFile2)
				bRet = 0;

			if(hFile1)
				CloseHandle(hFile1);

			if(hFile2)
				CloseHandle(hFile2);

			return bRet;
		}

		// Unprotect Drive
		bool Drive_UnProtect(wchar_t* szDrive)
		{
			HANDLE hFind = NULL;
			WIN32_FIND_DATA FindData = {0};

			wchar_t szFile[MAX_PATH] = L"\0", szSearch[MAX_PATH] = L"\0";

			wsprintf(szFile, L"\\\\?\\%s\\autorun.inf", szDrive);
			wsprintf(szSearch, L"%s\\*.*", szFile);
			
			if(Lib_IsFileExists(szFile))
			{
				SetFileAttributes(szFile, 0);

				if(Lib_FileIsDirectory(szFile))
				{
					hFind = FindFirstFile(szSearch, &FindData);

					if(hFind != INVALID_HANDLE_VALUE)
					{
						do
						{
							if(!(FindData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY))
							{
								wsprintf(szSearch, L"%s\\%s", szFile, FindData.cFileName);

								SetFileAttributes(szSearch, 0);
								DeleteFile(szSearch);
							} 
						}
						while(FindNextFile(hFind, &FindData));
					}

					FindClose(hFind);

					return RemoveDirectory(szFile);
				}
				else
				{
					return DeleteFile(szFile);
				}
			}

			return 1;
		}
*/

#endif // __DRIVEDOTSHIELD_H__
