#include "stdafx.h"

CLIST_INTERFACE *pcli;

//PLUGINLINK *pluginLink=NULL;
HANDLE hOptInit = nullptr;
static HWND hTimerWnd = (HWND)nullptr;
static UINT TID = (UINT)12021;
//HANDLE hHookModulesLoaded=NULL;
HANDLE hCheckEvent = nullptr;
HANDLE hCheckHook = nullptr;
HANDLE hHookModulesLoaded = nullptr;
HANDLE hHookPreShutdown = nullptr;
HANDLE hConnectionCheckThread = nullptr;
HANDLE hFilterOptionsThread = nullptr;
HANDLE killCheckThreadEvent = nullptr;
HANDLE hExceptionsMutex = nullptr;
//HANDLE hCurrentEditMutex=NULL;
int hLangpack = 0;

DWORD FilterOptionsThreadId;
DWORD ConnectionCheckThreadId;
BYTE settingSetColours = 0;
COLORREF settingBgColor;
COLORREF settingFgColor;
int settingInterval = 0;
int settingInterval1 = 0;
BYTE settingResolveIp = 0;
BOOL settingStatus[STATUS_COUNT];
int settingFiltersCount = 0;
BOOL settingDefaultAction = TRUE;
WORD settingStatusMask = 0;

struct CONNECTION *first = nullptr;
struct CONNECTION *connExceptions = nullptr;
struct CONNECTION *connCurrentEdit;
struct CONNECTION *connExceptionsTmp = nullptr;
struct CONNECTION *connCurrentEditModal = nullptr;
int currentStatus = ID_STATUS_OFFLINE, diffstat = 0;
BOOL bOptionsOpen = FALSE;
wchar_t *tcpStates[] = { L"CLOSED", L"LISTEN", L"SYN_SENT", L"SYN_RCVD", L"ESTAB", L"FIN_WAIT1", L"FIN_WAIT2", L"CLOSE_WAIT", L"CLOSING", L"LAST_ACK", L"TIME_WAIT", L"DELETE_TCB" };

PLUGININFOEX pluginInfo = {
	sizeof(PLUGININFOEX),
	PLUGINNAME,
	__VERSION_DWORD,
	__DESCRIPTION,
	__AUTHOR,
	__COPYRIGHT,
	__AUTHORWEB,
	UNICODE_AWARE,		//not transient
	//	4BB5B4AA-C364-4F23-9746-D5B708A286A5
	{ 0x4bb5b4aa, 0xc364, 0x4f23, { 0x97, 0x46, 0xd5, 0xb7, 0x8, 0xa2, 0x86, 0xa5 } }
};

extern "C" __declspec(dllexport) const PLUGININFOEX* MirandaPluginInfoEx(DWORD)
{
	return &pluginInfo;
}

/////////////////////////////////////////////////////////////////////////////////////////

CMPlugin	g_plugin;

/////////////////////////////////////////////////////////////////////////////////////////

extern "C" __declspec(dllexport) const MUUID MirandaInterfaces[] = { MIID_PROTOCOL, MIID_LAST };

/////////////////////////////////////////////////////////////////////////////////////////
// authentication callback futnction from extension manager

BOOL strrep(wchar_t *src, wchar_t *needle, wchar_t *newstring)
{
	wchar_t *found, begining[MAX_SETTING_STR], tail[MAX_SETTING_STR];
	size_t pos = 0;

	//strset(begining, ' ');
	//strset(tail, ' ');
	if (!(found = wcsstr(src, needle)))
		return FALSE;

	pos = (found - src);
	wcsncpy_s(begining, src, pos);
	begining[pos] = 0;

	pos = pos + mir_wstrlen(needle);
	wcsncpy_s(tail, src + pos, _TRUNCATE);
	begining[pos] = 0;

	pos = mir_snwprintf(src, mir_wstrlen(src), L"%s%s%s", begining, newstring, tail);
	return TRUE;
}

void saveSettingsConnections(struct CONNECTION *connHead)
{
	char buff[128];
	int i = 0;
	struct CONNECTION *tmp = connHead;
	while (tmp != nullptr) {

		mir_snprintf(buff, "%dFilterIntIp", i);
		db_set_ws(NULL, PLUGINNAME, buff, tmp->strIntIp);
		mir_snprintf(buff, "%dFilterExtIp", i);
		db_set_ws(NULL, PLUGINNAME, buff, tmp->strExtIp);
		mir_snprintf(buff, "%dFilterPName", i);
		db_set_ws(NULL, PLUGINNAME, buff, tmp->PName);
		mir_snprintf(buff, "%dFilterIntPort", i);
		db_set_dw(NULL, PLUGINNAME, buff, tmp->intIntPort);
		mir_snprintf(buff, "%dFilterExtPort", i);
		db_set_dw(NULL, PLUGINNAME, buff, tmp->intExtPort);
		mir_snprintf(buff, "%dFilterAction", i);
		db_set_dw(NULL, PLUGINNAME, buff, tmp->Pid);
		i++;
		tmp = tmp->next;
	}
	settingFiltersCount = i;
	db_set_dw(NULL, PLUGINNAME, "FiltersCount", settingFiltersCount);

}

//load filters from db
struct CONNECTION* LoadSettingsConnections()
{
	struct CONNECTION *connHead = nullptr;
	DBVARIANT dbv;
	char buff[128];
	int i = 0;
	for (i = settingFiltersCount - 1; i >= 0; i--) {
		struct CONNECTION *conn = (struct CONNECTION*)mir_alloc(sizeof(struct CONNECTION));
		mir_snprintf(buff, "%dFilterIntIp", i);
		if (!db_get_ws(NULL, PLUGINNAME, buff, &dbv))
			wcsncpy(conn->strIntIp, dbv.ptszVal, _countof(conn->strIntIp));
		db_free(&dbv);
		mir_snprintf(buff, "%dFilterExtIp", i);
		if (!db_get_ws(NULL, PLUGINNAME, buff, &dbv))
			wcsncpy(conn->strExtIp, dbv.ptszVal, _countof(conn->strExtIp));
		db_free(&dbv);
		mir_snprintf(buff, "%dFilterPName", i);
		if (!db_get_ws(NULL, PLUGINNAME, buff, &dbv))
			wcsncpy(conn->PName, dbv.ptszVal, _countof(conn->PName));
		db_free(&dbv);

		mir_snprintf(buff, "%dFilterIntPort", i);
		conn->intIntPort = db_get_dw(0, PLUGINNAME, buff, -1);

		mir_snprintf(buff, "%dFilterExtPort", i);
		conn->intExtPort = db_get_dw(0, PLUGINNAME, buff, -1);

		mir_snprintf(buff, "%dFilterAction", i);
		conn->Pid = db_get_dw(0, PLUGINNAME, buff, 0);

		conn->next = connHead;
		connHead = conn;
	}
	return connHead;
}
//called to load settings from database
void LoadSettings()
{
	settingInterval = (INT)db_get_dw(NULL, PLUGINNAME, "Interval", 500);
	settingInterval1 = (INT)db_get_dw(NULL, PLUGINNAME, "PopupInterval", 0);
	settingResolveIp = db_get_b(NULL, PLUGINNAME, "ResolveIp", TRUE);
	settingDefaultAction = db_get_b(NULL, PLUGINNAME, "FilterDefaultAction", TRUE);

	settingSetColours = db_get_b(NULL, PLUGINNAME, "PopupSetColours", 0);
	settingBgColor = (COLORREF)db_get_dw(NULL, PLUGINNAME, "PopupBgColor", (DWORD)0xFFFFFF);
	settingFgColor = (COLORREF)db_get_dw(NULL, PLUGINNAME, "PopupFgColor", (DWORD)0x000000);
	settingFiltersCount = (INT)db_get_dw(NULL, PLUGINNAME, "FiltersCount", 0);
	settingStatusMask = (WORD)db_get_w(NULL, PLUGINNAME, "StatusMask", 16);
	for (int i = 0; i < STATUS_COUNT; i++) {
		char buff[128];
		mir_snprintf(buff, "Status%d", i);
		settingStatus[i] = (db_get_b(0, PLUGINNAME, buff, 0) == 1);
	}
	//lookupLotusDefaultSettings();
}

void fillExceptionsListView(HWND hwndDlg)
{
	LVITEM lvI = { 0 };

	int i = 0;
	struct CONNECTION *tmp = connExceptionsTmp;
	HWND hwndList = GetDlgItem(hwndDlg, IDC_LIST_EXCEPTIONS);
	ListView_DeleteAllItems(hwndList);

	// Some code to create the list-view control.
	// Initialize LVITEM members that are common to all
	// items. 
	lvI.mask = LVIF_TEXT;
	while (tmp) {
		wchar_t tmpAddress[25];
		lvI.iItem = i++;
		lvI.iSubItem = 0;
		lvI.pszText = tmp->PName;
		ListView_InsertItem(hwndList, &lvI);
		lvI.iSubItem = 1;
		if (tmp->intIntPort == -1)
			mir_snwprintf(tmpAddress, L"%s:*", tmp->strIntIp);
		else
			mir_snwprintf(tmpAddress, L"%s:%d", tmp->strIntIp, tmp->intIntPort);
		lvI.pszText = tmpAddress;
		ListView_SetItem(hwndList, &lvI);
		lvI.iSubItem = 2;
		if (tmp->intExtPort == -1)
			mir_snwprintf(tmpAddress, L"%s:*", tmp->strExtIp);
		else
			mir_snwprintf(tmpAddress, L"%s:%d", tmp->strExtIp, tmp->intExtPort);
		lvI.pszText = tmpAddress;
		ListView_SetItem(hwndList, &lvI);
		lvI.iSubItem = 3;
		lvI.pszText = tmp->Pid ? LPGENW("Show") : LPGENW("Hide");
		ListView_SetItem(hwndList, &lvI);

		tmp = tmp->next;
	}

}
//filter editor dialog box procedure opened modally from options dialog
static INT_PTR CALLBACK FilterEditProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	switch (message) {
	case WM_INITDIALOG:
		{
			struct CONNECTION *conn = (struct CONNECTION*)lParam;
			TranslateDialogDefault(hWnd);
			connCurrentEditModal = conn;
			SetDlgItemText(hWnd, ID_TEXT_NAME, conn->PName);
			SetDlgItemText(hWnd, ID_TXT_LOCAL_IP, conn->strIntIp);
			SetDlgItemText(hWnd, ID_TXT_REMOTE_IP, conn->strExtIp);

			if (conn->intIntPort == -1)
				SetDlgItemText(hWnd, ID_TXT_LOCAL_PORT, L"*");
			else
				SetDlgItemInt(hWnd, ID_TXT_LOCAL_PORT, conn->intIntPort, FALSE);

			if (conn->intExtPort == -1)
				SetDlgItemText(hWnd, ID_TXT_REMOTE_PORT, L"*");
			else
				SetDlgItemInt(hWnd, ID_TXT_REMOTE_PORT, conn->intExtPort, FALSE);

			SendDlgItemMessage(hWnd, ID_CBO_ACTION, CB_ADDSTRING, 0, (LPARAM)TranslateT("Always show popup"));
			SendDlgItemMessage(hWnd, ID_CBO_ACTION, CB_ADDSTRING, 0, (LPARAM)TranslateT("Never show popup"));
			SendDlgItemMessage(hWnd, ID_CBO_ACTION, CB_SETCURSEL, conn->Pid == 0 ? 1 : 0, 0);
			return TRUE;
		}
	case WM_COMMAND:
		switch (LOWORD(wParam)) {
		case ID_OK:
			{
				wchar_t tmpPort[6];
				GetDlgItemText(hWnd, ID_TXT_LOCAL_PORT, tmpPort, _countof(tmpPort));
				if (tmpPort[0] == '*')
					connCurrentEditModal->intIntPort = -1;
				else
					connCurrentEditModal->intIntPort = GetDlgItemInt(hWnd, ID_TXT_LOCAL_PORT, nullptr, FALSE);
				GetDlgItemText(hWnd, ID_TXT_REMOTE_PORT, tmpPort, _countof(tmpPort));
				if (tmpPort[0] == '*')
					connCurrentEditModal->intExtPort = -1;
				else
					connCurrentEditModal->intExtPort = GetDlgItemInt(hWnd, ID_TXT_REMOTE_PORT, nullptr, FALSE);

				GetDlgItemText(hWnd, ID_TXT_LOCAL_IP, connCurrentEditModal->strIntIp, _countof(connCurrentEditModal->strIntIp));
				GetDlgItemText(hWnd, ID_TXT_REMOTE_IP, connCurrentEditModal->strExtIp, _countof(connCurrentEditModal->strExtIp));
				GetDlgItemText(hWnd, ID_TEXT_NAME, connCurrentEditModal->PName, _countof(connCurrentEditModal->PName));

				connCurrentEditModal->Pid = !(BOOL)SendDlgItemMessage(hWnd, ID_CBO_ACTION, CB_GETCURSEL, 0, 0);

				connCurrentEditModal = nullptr;
				EndDialog(hWnd, IDOK);
				return TRUE;
			}
		case ID_CANCEL:
			connCurrentEditModal = nullptr;
			EndDialog(hWnd, IDCANCEL);
			return TRUE;
		}
		return FALSE;
		break;
	case WM_CLOSE:
		{
			connCurrentEditModal = nullptr;
			EndDialog(hWnd, IDCANCEL);
			break;
		}
	}
	return FALSE;
}

//options page on miranda called
INT_PTR CALLBACK DlgProcConnectionNotifyOpts(HWND hwndDlg, UINT msg, WPARAM wParam, LPARAM lParam)
{
	HWND hwndList;
	switch (msg) {
	case WM_INITDIALOG://initialize dialog, so set properties from db.
		{
			LVCOLUMN lvc = { 0 };
			LVITEM lvI = { 0 };
			wchar_t buff[256];
			bOptionsOpen = TRUE;
			TranslateDialogDefault(hwndDlg);//translate miranda function
			#ifdef _WIN64
			mir_snwprintf(buff, L"%d.%d.%d.%d/64", HIBYTE(HIWORD(pluginInfo.version)), LOBYTE(HIWORD(pluginInfo.version)), HIBYTE(LOWORD(pluginInfo.version)), LOBYTE(LOWORD(pluginInfo.version)));
			#else
			mir_snwprintf(buff, L"%d.%d.%d.%d/32", HIBYTE(HIWORD(pluginInfo.version)), LOBYTE(HIWORD(pluginInfo.version)), HIBYTE(LOWORD(pluginInfo.version)), LOBYTE(LOWORD(pluginInfo.version)));
			#endif
			SetDlgItemText(hwndDlg, IDC_VERSION, buff);
			LoadSettings();
			//connExceptionsTmp=LoadSettingsConnections();
			SetDlgItemInt(hwndDlg, IDC_INTERVAL, settingInterval, FALSE);
			SetDlgItemInt(hwndDlg, IDC_INTERVAL1, settingInterval1, TRUE);
			CheckDlgButton(hwndDlg, IDC_SETCOLOURS, settingSetColours ? BST_CHECKED : BST_UNCHECKED);
			CheckDlgButton(hwndDlg, IDC_RESOLVEIP, settingResolveIp ? BST_CHECKED : BST_UNCHECKED);
			CheckDlgButton(hwndDlg, ID_CHK_DEFAULTACTION, settingDefaultAction ? BST_CHECKED : BST_UNCHECKED);

			SendDlgItemMessage(hwndDlg, IDC_BGCOLOR, CPM_SETCOLOUR, 0, (LPARAM)settingBgColor);
			SendDlgItemMessage(hwndDlg, IDC_FGCOLOR, CPM_SETCOLOUR, 0, (LPARAM)settingFgColor);
			if (!settingSetColours) {
				HWND hwnd = GetDlgItem(hwndDlg, IDC_BGCOLOR);
				CheckDlgButton(hwndDlg, IDC_SETCOLOURS, BST_UNCHECKED);
				EnableWindow(hwnd, FALSE);
				hwnd = GetDlgItem(hwndDlg, IDC_FGCOLOR);
				EnableWindow(hwnd, FALSE);
			}
			SendDlgItemMessage(hwndDlg, ID_ADD, BM_SETIMAGE, IMAGE_ICON, (LPARAM)LoadImage(g_plugin.getInst(), MAKEINTRESOURCE(IDI_ICON6), IMAGE_ICON, 16, 16, 0));
			SendDlgItemMessage(hwndDlg, ID_DELETE, BM_SETIMAGE, IMAGE_ICON, (LPARAM)LoadImage(g_plugin.getInst(), MAKEINTRESOURCE(IDI_ICON3), IMAGE_ICON, 16, 16, 0));
			SendDlgItemMessage(hwndDlg, ID_DOWN, BM_SETIMAGE, IMAGE_ICON, (LPARAM)LoadImage(g_plugin.getInst(), MAKEINTRESOURCE(IDI_ICON4), IMAGE_ICON, 16, 16, 0));
			SendDlgItemMessage(hwndDlg, ID_UP, BM_SETIMAGE, IMAGE_ICON, (LPARAM)LoadImage(g_plugin.getInst(), MAKEINTRESOURCE(IDI_ICON5), IMAGE_ICON, 16, 16, 0));
			// initialise and fill listbox
			hwndList = GetDlgItem(hwndDlg, IDC_STATUS);
			ListView_DeleteAllItems(hwndList);
			SendMessage(hwndList, LVM_SETEXTENDEDLISTVIEWSTYLE, 0, LVS_EX_FULLROWSELECT | LVS_EX_CHECKBOXES);
			// Initialize the LVCOLUMN structure.
			// The mask specifies that the format, width, text, and
			// subitem members of the structure are valid. 
			lvc.mask = LVCF_FMT | LVCF_WIDTH | LVCF_TEXT | LVCF_SUBITEM;
			lvc.fmt = LVCFMT_LEFT;
			lvc.iSubItem = 0;
			lvc.pszText = TranslateT("Status");
			lvc.cx = 120;     // width of column in pixels
			ListView_InsertColumn(hwndList, 0, &lvc);
			// Some code to create the list-view control.
			// Initialize LVITEM members that are common to all
			// items. 
			lvI.mask = LVIF_TEXT;
			for (int i = 0; i < STATUS_COUNT; i++) {
				lvI.pszText = Clist_GetStatusModeDescription(ID_STATUS_ONLINE + i, 0);
				lvI.iItem = i;
				ListView_InsertItem(hwndList, &lvI);
				ListView_SetCheckState(hwndList, i, settingStatus[i]);
			}

			connExceptionsTmp = LoadSettingsConnections();
			hwndList = GetDlgItem(hwndDlg, IDC_LIST_EXCEPTIONS);
			SendMessage(hwndList, LVM_SETEXTENDEDLISTVIEWSTYLE, 0, LVS_EX_FULLROWSELECT);

			lvc.mask = LVCF_FMT | LVCF_WIDTH | LVCF_TEXT | LVCF_SUBITEM;
			lvc.fmt = LVCFMT_LEFT;
			lvc.iSubItem = 0;
			lvc.cx = 120;     // width of column in pixels
			lvc.pszText = TranslateT("Application");
			ListView_InsertColumn(hwndList, 1, &lvc);
			lvc.pszText = TranslateT("Internal socket");
			ListView_InsertColumn(hwndList, 2, &lvc);
			lvc.pszText = TranslateT("External socket");
			ListView_InsertColumn(hwndList, 3, &lvc);
			lvc.pszText = TranslateT("Action");
			lvc.cx = 50;
			ListView_InsertColumn(hwndList, 4, &lvc);

			//fill exceptions list
			fillExceptionsListView(hwndDlg);
		}
		break;

	case WM_COMMAND://user changed something, so get changes to variables
		PostMessage(GetParent(hwndDlg), PSM_CHANGED, 0, 0);
		switch (LOWORD(wParam)) {
		case IDC_INTERVAL: settingInterval = GetDlgItemInt(hwndDlg, IDC_INTERVAL, nullptr, FALSE); break;
		case IDC_INTERVAL1: settingInterval1 = GetDlgItemInt(hwndDlg, IDC_INTERVAL1, nullptr, TRUE); break;
		case IDC_RESOLVEIP: settingResolveIp = (BYTE)IsDlgButtonChecked(hwndDlg, IDC_RESOLVEIP); break;
		case ID_CHK_DEFAULTACTION: settingDefaultAction = (BYTE)IsDlgButtonChecked(hwndDlg, ID_CHK_DEFAULTACTION); break;
		case ID_ADD:
			{
				struct CONNECTION *cur = (struct CONNECTION *)mir_alloc(sizeof(struct CONNECTION));
				memset(cur, 0, sizeof(struct CONNECTION));
				cur->intExtPort = -1;
				cur->intIntPort = -1;
				cur->Pid = 0;
				cur->PName[0] = '*';
				cur->strExtIp[0] = '*';
				cur->strIntIp[0] = '*';

				if (DialogBoxParam(g_plugin.getInst(), MAKEINTRESOURCE(IDD_FILTER_DIALOG), hwndDlg, FilterEditProc, (LPARAM)cur) == IDCANCEL) {
					mir_free(cur);
					cur = nullptr;
				}
				else {
					cur->next = connExceptionsTmp;
					connExceptionsTmp = cur;
				}

				fillExceptionsListView(hwndDlg);
				ListView_SetItemState(GetDlgItem(hwndDlg, IDC_LIST_EXCEPTIONS), 0, LVNI_FOCUSED | LVIS_SELECTED, LVNI_FOCUSED | LVIS_SELECTED);
				SetFocus(GetDlgItem(hwndDlg, IDC_LIST_EXCEPTIONS));
			}
			break;

		case ID_DELETE:
			{
				int pos, pos1;
				struct CONNECTION *cur = connExceptionsTmp, *pre = nullptr;

				pos = (int)ListView_GetNextItem(GetDlgItem(hwndDlg, IDC_LIST_EXCEPTIONS), -1, LVNI_FOCUSED);
				if (pos == -1)break;
				pos1 = pos;
				while (pos--) {
					pre = cur;
					cur = cur->next;
				}
				if (pre == nullptr)
					connExceptionsTmp = connExceptionsTmp->next;
				else
					(pre)->next = cur->next;
				mir_free(cur);
				fillExceptionsListView(hwndDlg);
				ListView_SetItemState(GetDlgItem(hwndDlg, IDC_LIST_EXCEPTIONS), pos1, LVNI_FOCUSED | LVIS_SELECTED, LVNI_FOCUSED | LVIS_SELECTED);
				SetFocus(GetDlgItem(hwndDlg, IDC_LIST_EXCEPTIONS));
				break;
			}
		case ID_UP:
			{
				int pos, pos1;
				struct CONNECTION *cur = nullptr, *pre = nullptr, *prepre = nullptr;

				cur = connExceptionsTmp;

				pos = (int)ListView_GetNextItem(GetDlgItem(hwndDlg, IDC_LIST_EXCEPTIONS), -1, LVNI_FOCUSED);
				if (pos == -1)break;
				pos1 = pos;
				while (pos--) {
					prepre = pre;
					pre = cur;
					cur = cur->next;
				}
				if (prepre != nullptr) {
					pre->next = cur->next;
					cur->next = pre;
					prepre->next = cur;
				}
				else if (pre != nullptr) {
					pre->next = cur->next;
					cur->next = pre;
					connExceptionsTmp = cur;
				}
				fillExceptionsListView(hwndDlg);
				ListView_SetItemState(GetDlgItem(hwndDlg, IDC_LIST_EXCEPTIONS), pos1 - 1, LVNI_FOCUSED | LVIS_SELECTED, LVNI_FOCUSED | LVIS_SELECTED);
				SetFocus(GetDlgItem(hwndDlg, IDC_LIST_EXCEPTIONS));
				break;
			}
		case ID_DOWN:
			{
				int pos, pos1;
				struct CONNECTION *cur = nullptr, *pre = nullptr;

				cur = connExceptionsTmp;

				pos = (int)ListView_GetNextItem(GetDlgItem(hwndDlg, IDC_LIST_EXCEPTIONS), -1, LVNI_FOCUSED);
				if (pos == -1)break;
				pos1 = pos;
				while (pos--) {
					pre = cur;
					cur = cur->next;
				}
				if (cur == connExceptionsTmp&&cur->next != nullptr) {
					connExceptionsTmp = cur->next;
					cur->next = cur->next->next;
					connExceptionsTmp->next = cur;
				}
				else if (cur->next != nullptr) {
					struct CONNECTION *tmp = cur->next->next;
					pre->next = cur->next;
					cur->next->next = cur;
					cur->next = tmp;
				}
				fillExceptionsListView(hwndDlg);
				ListView_SetItemState(GetDlgItem(hwndDlg, IDC_LIST_EXCEPTIONS), pos1 + 1, LVNI_FOCUSED | LVIS_SELECTED, LVNI_FOCUSED | LVIS_SELECTED);
				SetFocus(GetDlgItem(hwndDlg, IDC_LIST_EXCEPTIONS));
				break;
			}
		case IDC_SETCOLOURS:
			{
				HWND hwnd = GetDlgItem(hwndDlg, IDC_BGCOLOR);
				settingSetColours = IsDlgButtonChecked(hwndDlg, IDC_SETCOLOURS);
				EnableWindow(hwnd, settingSetColours);
				hwnd = GetDlgItem(hwndDlg, IDC_FGCOLOR);
				EnableWindow(hwnd, settingSetColours);
				break;
			}
		case IDC_BGCOLOR: settingBgColor = (COLORREF)SendDlgItemMessage(hwndDlg, IDC_BGCOLOR, CPM_GETCOLOUR, 0, 0); break;
		case IDC_FGCOLOR: settingFgColor = (COLORREF)SendDlgItemMessage(hwndDlg, IDC_FGCOLOR, CPM_GETCOLOUR, 0, 0); break;

		}
		break;

	case WM_NOTIFY://apply changes so write it to db
		switch (((LPNMHDR)lParam)->idFrom) {
		case 0:
			switch (((LPNMHDR)lParam)->code) {
			case PSN_RESET:
				LoadSettings();
				deleteConnectionsTable(connExceptionsTmp);
				connExceptionsTmp = LoadSettingsConnections();
				return TRUE;

			case PSN_APPLY:
				db_set_dw(NULL, PLUGINNAME, "Interval", settingInterval);
				db_set_dw(NULL, PLUGINNAME, "PopupInterval", settingInterval1);
				db_set_b(NULL, PLUGINNAME, "PopupSetColours", settingSetColours);
				db_set_dw(NULL, PLUGINNAME, "PopupBgColor", (DWORD)settingBgColor);
				db_set_dw(NULL, PLUGINNAME, "PopupFgColor", (DWORD)settingFgColor);
				db_set_b(NULL, PLUGINNAME, "ResolveIp", settingResolveIp);
				db_set_b(NULL, PLUGINNAME, "FilterDefaultAction", settingDefaultAction);

				for (int i = 0; i < STATUS_COUNT; i++) {
					char buff[128];
					mir_snprintf(buff, "Status%d", i);
					settingStatus[i] = (ListView_GetCheckState(GetDlgItem(hwndDlg, IDC_STATUS), i) ? TRUE : FALSE);
					db_set_b(0, PLUGINNAME, buff, settingStatus[i] ? 1 : 0);
				}
				if (WAIT_OBJECT_0 == WaitForSingleObject(hExceptionsMutex, 100)) {
					deleteConnectionsTable(connExceptions);
					saveSettingsConnections(connExceptionsTmp);
					connExceptions = connExceptionsTmp;
					connExceptionsTmp = LoadSettingsConnections();
					ReleaseMutex(hExceptionsMutex);
				}
				return TRUE;
			}
			break;
		}

		if (GetDlgItem(hwndDlg, IDC_LIST_EXCEPTIONS) == ((LPNMHDR)lParam)->hwndFrom) {
			switch (((LPNMHDR)lParam)->code) {
			case NM_DBLCLK:
				{
					int pos, pos1;
					struct CONNECTION *cur = nullptr;

					cur = connExceptionsTmp;

					pos = (int)ListView_GetNextItem(GetDlgItem(hwndDlg, IDC_LIST_EXCEPTIONS), -1, LVNI_FOCUSED);
					if (pos == -1)break;
					pos1 = pos;
					while (pos--) {
						cur = cur->next;
					}
					DialogBoxParam(g_plugin.getInst(), MAKEINTRESOURCE(IDD_FILTER_DIALOG), hwndDlg, FilterEditProc, (LPARAM)cur);
					fillExceptionsListView(hwndDlg);
					ListView_SetItemState(GetDlgItem(hwndDlg, IDC_LIST_EXCEPTIONS), pos1, LVNI_FOCUSED | LVIS_SELECTED, LVNI_FOCUSED | LVIS_SELECTED);
					SetFocus(GetDlgItem(hwndDlg, IDC_LIST_EXCEPTIONS));
					break;
				}
			}
		}

		if (GetDlgItem(hwndDlg, IDC_STATUS) == ((LPNMHDR)lParam)->hwndFrom) {
			switch (((LPNMHDR)lParam)->code) {
			case LVN_ITEMCHANGED:
				NMLISTVIEW *nmlv = (NMLISTVIEW *)lParam;
				if ((nmlv->uNewState ^ nmlv->uOldState) & LVIS_STATEIMAGEMASK)
					SendMessage(GetParent(hwndDlg), PSM_CHANGED, 0, 0);
				break;
			}
		}
		break;

	case WM_DESTROY:
		bOptionsOpen = FALSE;
		deleteConnectionsTable(connExceptionsTmp);
		connExceptionsTmp = nullptr;
		return TRUE;
	}
	return 0;
}

//options page on miranda called
int ConnectionNotifyOptInit(WPARAM wParam, LPARAM)
{
	OPTIONSDIALOGPAGE odp = {};
	odp.hInstance = g_plugin.getInst();
	odp.pszTemplate = MAKEINTRESOURCEA(IDD_OPT_DIALOG);
	odp.szTitle.w = _A2W(PLUGINNAME);
	odp.szGroup.w = LPGENW("Plugins");
	odp.flags = ODPF_BOLDGROUPS | ODPF_UNICODE;
	odp.pfnDlgProc = DlgProcConnectionNotifyOpts;//callback function name
	Options_AddPage(wParam, &odp);
	return 0;
}

//gives protocol avainable statuses
INT_PTR GetCaps(WPARAM wParam, LPARAM)
{
	if (wParam == PFLAGNUM_1)
		return 0;
	if (wParam == PFLAGNUM_2)
		return PF2_ONLINE; // add the possible statuses here.
	if (wParam == PFLAGNUM_3)
		return 0;
	return 0;
}

//gives  name to protocol module
INT_PTR GetName(WPARAM wParam, LPARAM lParam)
{
	mir_strncpy((char*)lParam, PLUGINNAME, wParam);
	return 0;
}

//gives icon for proto module
INT_PTR TMLoadIcon(WPARAM wParam, LPARAM)
{
	UINT id;

	switch (wParam & 0xFFFF) {
	case PLI_PROTOCOL:
		id = IDI_ICON1;
		break; // IDI_TM is the main icon for the protocol
	default:
		return 0;
	}
	return (INT_PTR)LoadImage(g_plugin.getInst(), MAKEINTRESOURCE(id), IMAGE_ICON, GetSystemMetrics(wParam&PLIF_SMALL ? SM_CXSMICON : SM_CXICON), GetSystemMetrics(wParam&PLIF_SMALL ? SM_CYSMICON : SM_CYICON), 0);
}
//=======================================================
//SetStatus
//=======================================================
INT_PTR SetStatus(WPARAM wParam, LPARAM lParam)
{
	if (wParam == ID_STATUS_OFFLINE) {
		diffstat = 0;
		//PostThreadMessage(ConnectionCheckThreadId,WM_QUIT ,0, 0);
		SetEvent(killCheckThreadEvent);

	}
	else if (wParam == ID_STATUS_ONLINE) {
		diffstat = 0;
		ResetEvent(killCheckThreadEvent);
		if (!hConnectionCheckThread)
			hConnectionCheckThread = (HANDLE)mir_forkthreadex(checkthread, nullptr, (unsigned int*)&ConnectionCheckThreadId);
	}
	else {
		int retv = 0;

		if (settingStatus[wParam - ID_STATUS_ONLINE])
			retv = SetStatus(ID_STATUS_OFFLINE, lParam);
		else
			retv = SetStatus(ID_STATUS_ONLINE, lParam);
		//LNEnableMenuItem(hMenuHandle ,TRUE);
		diffstat = wParam;
		return retv;

		// the status has been changed to unknown  (maybe run some more code)
	}
	//broadcast the message

	//oldStatus = currentStatus;
	if (currentStatus != (int)wParam)
		ProtoBroadcastAck(PLUGINNAME, NULL, ACKTYPE_STATUS, ACKRESULT_SUCCESS, (HANDLE)currentStatus, wParam);
	currentStatus = wParam;
	return 0;

}
//=======================================================
//GetStatus
//=======================================================
INT_PTR GetStatus(WPARAM, LPARAM)
{
	return currentStatus;
}

//thread function with connections check loop
static unsigned __stdcall checkthread(void *)
{

	#ifdef _DEBUG
	_OutputDebugString(L"check thread started");
	#endif
	while (1) {
		struct CONNECTION* conn = nullptr, *connOld = first, *cur = nullptr;
		#ifdef _DEBUG
		_OutputDebugString(L"checking connections table...");
		#endif
		if (WAIT_OBJECT_0 == WaitForSingleObject(killCheckThreadEvent, 100)) {
			hConnectionCheckThread = nullptr;
			return 0;
		}

		conn = GetConnectionsTable();
		cur = conn;
		while (cur != nullptr) {
			if (searchConnection(first, cur->strIntIp, cur->strExtIp, cur->intIntPort, cur->intExtPort, cur->state) == nullptr && (settingStatusMask & (1 << (cur->state - 1)))) {

				#ifdef _DEBUG
				wchar_t msg[1024];
				mir_snwprintf(msg, L"%s:%d\n%s:%d", cur->strIntIp, cur->intIntPort, cur->strExtIp, cur->intExtPort);
				_OutputDebugString(L"New connection: %s", msg);
				#endif
				pid2name(cur->Pid, cur->PName, _countof(cur->PName));
				if (WAIT_OBJECT_0 == WaitForSingleObject(hExceptionsMutex, 100)) {
					if (checkFilter(connExceptions, cur)) {
						showMsg(cur->PName, cur->Pid, cur->strIntIp, cur->strExtIp, cur->intIntPort, cur->intExtPort, cur->state);
						Skin_PlaySound(PLUGINNAME_NEWSOUND);
					}
					ReleaseMutex(hExceptionsMutex);
				}
			}
			cur = cur->next;
		}

		first = conn;
		deleteConnectionsTable(connOld);
		Sleep(settingInterval);
	}
	hConnectionCheckThread = nullptr;
	return 1;
}

//popup reactions
static LRESULT CALLBACK PopupDlgProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	switch (message) {
	case WM_COMMAND:
		if (HIWORD(wParam) == STN_CLICKED)//client clicked on popup with left mouse button
		{
			struct CONNECTION *conn = (struct CONNECTION*)mir_alloc(sizeof(struct CONNECTION));
			struct CONNECTION *mpd = (struct CONNECTION*) PUGetPluginData(hWnd);

			memcpy(conn, mpd, sizeof(struct CONNECTION));
			PUDeletePopup(hWnd);
			PostThreadMessage(FilterOptionsThreadId, WM_ADD_FILTER, 0, (LPARAM)conn);
		}
		break;

	case WM_RBUTTONUP:
		PUDeletePopup(hWnd);
		break;

	case UM_INITPOPUP:
		//struct CONNECTON *conn=NULL;
		//conn = (struct CONNECTION*)CallService(MS_POPUP_GETPLUGINDATA, (WPARAM)hWnd,(LPARAM)conn);
		//MessageBox(NULL,conn->extIp);
		//PUDeletePopUp(hWnd);
		break;

	case UM_FREEPLUGINDATA:
		struct CONNECTION *mpd = (struct CONNECTION*)PUGetPluginData(hWnd);
		if (mpd > 0) mir_free(mpd);
		return TRUE; //TRUE or FALSE is the same, it gets ignored.
	}
	return DefWindowProc(hWnd, message, wParam, lParam);
}


//show popup
void showMsg(wchar_t *pName, DWORD pid, wchar_t *intIp, wchar_t *extIp, int intPort, int extPort, int state)
{

	POPUPDATAT ppd;
	//hContact = A_VALID_HANDLE_YOU_GOT_FROM_SOMEWHERE;
	//hIcon = A_VALID_HANDLE_YOU_GOT_SOMEWHERE;
	//char * lpzContactName = (char*)CallService(MS_CLIST_GETCONTACTDISPLAYNAME,(WPARAM)lhContact,0);
	//99% of the times you'll just copy this line.
	//1% of the times you may wish to change the contact's name. I don't know why you should, but you can.
	//char * lpzText;
	//The text for the second line. You could even make something like: char lpzText[128]; mir_wstrcpy(lpzText, "Hello world!"); It's your choice.

	struct CONNECTION *mpd = (struct CONNECTION*)mir_alloc(sizeof(struct CONNECTION));
	//MessageBox(NULL,"aaa","aaa",1);
	memset(&ppd, 0, sizeof(ppd)); //This is always a good thing to do.
	ppd.lchContact = NULL;//(HANDLE)hContact; //Be sure to use a GOOD handle, since this will not be checked.
	ppd.lchIcon = LoadIcon(g_plugin.getInst(), MAKEINTRESOURCE(IDI_ICON1));
	if (settingResolveIp) {
		wchar_t hostName[128];
		getDnsName(extIp, hostName, _countof(hostName));
		mir_snwprintf(ppd.lptzText, L"%s:%d\n%s:%d", hostName, extPort, intIp, intPort);
	}
	else mir_snwprintf(ppd.lptzText, L"%s:%d\n%s:%d", extIp, extPort, intIp, intPort);

	mir_snwprintf(ppd.lptzContactName, L"%s (%s)", pName, tcpStates[state - 1]);

	if (settingSetColours) {
		ppd.colorBack = settingBgColor;
		ppd.colorText = settingFgColor;
	}
	ppd.PluginWindowProc = PopupDlgProc;

	ppd.iSeconds = settingInterval1;
	//Now the "additional" data.
	wcsncpy_s(mpd->strIntIp, intIp, _TRUNCATE);
	wcsncpy_s(mpd->strExtIp, extIp, _TRUNCATE);
	wcsncpy_s(mpd->PName, pName, _TRUNCATE);
	mpd->intIntPort = intPort;
	mpd->intExtPort = extPort;
	mpd->Pid = pid;

	//Now that the plugin data has been filled, we add it to the PopUpData.
	ppd.PluginData = mpd;

	//Now that every field has been filled, we want to see the popup.
	PUAddPopupT(&ppd);
}

//called after all plugins loaded.
//all Connection staff will be called, that will not hang miranda on startup
static int modulesloaded(WPARAM, LPARAM)
{

	#ifdef _DEBUG
	_OutputDebugString(L"Modules loaded, lets start TN...");
	#endif
	//	hConnectionCheckThread = (HANDLE)mir_forkthreadex(checkthread, 0, 0, ConnectionCheckThreadId);

	//#ifdef _DEBUG
	//	_OutputDebugString("started check thread %d",hConnectionCheckThread);
	//#endif
	killCheckThreadEvent = CreateEvent(nullptr, FALSE, FALSE, L"killCheckThreadEvent");
	hFilterOptionsThread = startFilterThread();
	//updaterRegister();

	return 0;
}
//function hooks before unload
static int preshutdown(WPARAM, LPARAM)
{
	deleteConnectionsTable(first);
	deleteConnectionsTable(connExceptions);
	deleteConnectionsTable(connExceptionsTmp);

	PostThreadMessage(ConnectionCheckThreadId, WM_QUIT, 0, 0);
	PostThreadMessage(FilterOptionsThreadId, WM_QUIT, 0, 0);

	return 0;
}

extern "C" int __declspec(dllexport) Load(void)
{
	#ifdef _DEBUG
	_OutputDebugString(L"Entering Load dll");
	#endif

	mir_getLP(&pluginInfo);
	pcli = Clist_GetInterface();

	hExceptionsMutex = CreateMutex(nullptr, FALSE, L"ExceptionsMutex");

	LoadSettings();
	connExceptions = LoadSettingsConnections();

	// set all contacts to offline
	for (auto &hContact : Contacts(PLUGINNAME))
		db_set_w(hContact, PLUGINNAME, "status", ID_STATUS_OFFLINE);

	CreateProtoServiceFunction(PLUGINNAME, PS_GETCAPS, GetCaps);
	CreateProtoServiceFunction(PLUGINNAME, PS_GETNAME, GetName);
	CreateProtoServiceFunction(PLUGINNAME, PS_LOADICON, TMLoadIcon);
	CreateProtoServiceFunction(PLUGINNAME, PS_SETSTATUS, SetStatus);
	CreateProtoServiceFunction(PLUGINNAME, PS_GETSTATUS, GetStatus);

	Skin_AddSound(PLUGINNAME_NEWSOUND, PLUGINNAMEW, LPGENW("New Connection Notification"));
	hOptInit = HookEvent(ME_OPT_INITIALISE, ConnectionNotifyOptInit);//register service to hook option call
	assert(hOptInit);
	hHookModulesLoaded = HookEvent(ME_SYSTEM_MODULESLOADED, modulesloaded);//hook event that all plugins are loaded
	assert(hHookModulesLoaded);
	hHookPreShutdown = HookEvent(ME_SYSTEM_PRESHUTDOWN, preshutdown);
	return 0;
}

/////////////////////////////////////////////////////////////////////////////////////////

extern "C" int __declspec(dllexport) Unload(void)
{
	WaitForSingleObjectEx(hConnectionCheckThread, INFINITE, FALSE);
	if (hConnectionCheckThread)CloseHandle(hConnectionCheckThread);
	if (hCheckEvent)DestroyHookableEvent(hCheckEvent);
	if (hOptInit) UnhookEvent(hOptInit);
	if (hCheckHook)UnhookEvent(hCheckHook);
	if (hHookModulesLoaded)UnhookEvent(hHookModulesLoaded);
	if (hHookPreShutdown)UnhookEvent(hHookPreShutdown);
	if (killCheckThreadEvent)
		CloseHandle(killCheckThreadEvent);
	//if (hCurrentEditMutex) CloseHandle(hCurrentEditMutex);
	if (hExceptionsMutex) CloseHandle(hExceptionsMutex);

	#ifdef _DEBUG
	_OutputDebugString(L"Unloaded");
	#endif
	return 0;
}
