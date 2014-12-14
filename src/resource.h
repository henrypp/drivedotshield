#ifndef __RESOURCE_H__
#define __RESOURCE_H__

#ifndef IDC_STATIC
#define IDC_STATIC (-1)
#endif

// Dialogs
#define IDD_MAIN	                            100
#define IDD_SETTINGS	                        101
#define IDD_PROPERTIES                          102

// Menus
#define IDM_MAIN		                        100
#define IDM_DRIVES_MENU							101
#define IDM_PROPERTIES_MENU	                    102

// Main Dlg
#define IDC_DRIVES		                        100
#define IDC_STATUSBAR                           101

// Properties Dlg
#define IDC_PROPERTIES							100

// Common Controls
#define IDC_OK									200
#define IDC_CANCEL								201

#define IDC_LABEL_1								203
#define IDC_LABEL_2								204
#define IDC_LABEL_3								205
#define IDC_LABEL_4								206

#define IDC_CHECK_UPDATE_AT_STARTUP_CHK			100
#define IDC_ALWAYS_ON_TOP_CHK					101
#define IDC_ENABLE_LOWLEVEL_FAT_PROTECTION_CHK	102
#define IDC_NODRIVETYPEAUTORUN_CHK				103
#define IDC_INIFILEMAPPING_CHK					104
#define IDC_LANGUAGE_CB							105
#define IDC_LANGUAGE_INFO						106

// Main Menu
#define IDM_SETTINGS                            40000
#define IDM_EXIT                                40001
#define IDM_WEBSITE                             40003
#define IDM_CHECK_UPDATES		                40004
#define IDM_ABOUT                               40005

// Drives Menu
#define IDM_OPEN                                1000
#define IDM_PROTECT                             1001
#define IDM_UNPROTECT                          	1002
#define IDM_REFRESH                             1003
#define IDM_LOCK                           		1004
#define IDM_UNLOCK                            	1005
#define IDM_UNLOCK_ALL                         	1006
#define IDM_CHKDSK                              1007
#define IDM_CHKDSK_REPAIR                       1008
#define IDM_EJECT                               1009
#define IDM_FORMAT                              1010
#define IDM_PROPERTIES                          1011

// Properties Menu
#define IDM_PROPERTIES_COPY						1000
#define IDM_PROPERTIES_REFRESH					1001

// Strings
#define IDS_TRANSLATION_INFO					1000

// Icons
#define IDI_MAIN	                            100
#define IDI_NORMAL								101
#define IDI_PROTECTED							102
#define IDI_INFECTED							103
#define IDI_LOCKED								104

#endif
