/*----------------------------------------------------------
    Charu3
    アプリケーション用クラスの機能定義	'2002.11.15 (c)C+ Factory
----------------------------------------------------------*/
#include "stdafx.h"
#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

#if false
// TODO: Did not handle this well with Visual Studio 2019.
#include <MULTIMON.H>
#endif
#include <dwmapi.h>
#include <ctype.h>
#include <list>
#include <vector>

#include "Charu3.h"
#include "Charu3Tree.h"
#include "InitialDialog.h"
#include "EditDialog.h"
#include "OptMainDialog.h"
#include "AboutDialog.h"
#include "commonDialog.h"
#include "window.h"
#include "hotkey.h"
#include "key.h"
#include "text.h"
#include "log.h"

#define KEY_DOWN			0x01
#define KEY_UP				0x02

#define MACRO_START			_T("<charuMACRO>")
#define MACRO_END			_T("</charuMACRO>")

#define MACRO_START_KEY		_T("<charuMACRO_KEY>")
#define MACRO_END_KEY		_T("</charuMACRO_KEY>")

#define EXMACRO_DIRECT_COPY "directcopykey"
#define EXMACRO_HOT_KEY		"hotkey"

//---------------------------------------------------
//関数名	MonitorEnumFunc
//機能		モニター列挙コールバック
//---------------------------------------------------
BOOL CALLBACK MonitorEnumFunc(HMONITOR hMonitor, HDC hdc, LPRECT rect, LPARAM lParam)
{
    MONITORINFOEX MonitorInfoEx = {};
    static int count = 0;

    CArray<RECT, RECT>* pArray;
    pArray = (CArray<RECT, RECT>*)lParam;
    count++;
    MonitorInfoEx.cbSize = sizeof(MonitorInfoEx);
    if (!GetMonitorInfo(hMonitor, &MonitorInfoEx)) {
        return FALSE;
    }
    pArray->Add(*rect);
    return TRUE;
}

//---------------------------------------------------
// CCharu3App
//---------------------------------------------------
BEGIN_MESSAGE_MAP(CCharu3App, CWinApp)
    //{{AFX_MSG_MAP(CCharu3App)
    //ON_COMMAND(IDM_EXIT, OnExit)
    ON_COMMAND(IDM_ABOUT, OnAbout)
    ON_COMMAND(IDM_ISSUES, OnIssues)
    ON_COMMAND(IDM_DISCUSSIONS, OnDiscussions)
    ON_COMMAND(IDM_WIKI, OnWiki)
    ON_COMMAND(IDM_OPTION, OnOption)
    ON_COMMAND(IDM_STOCK_STOP, OnStockStop)
    ON_COMMAND(IDM_ADD_DATA, OnAddData)
    ON_COMMAND(IDM_CHANG_DATA, OnChangData)
    ON_COMMAND(IDM_RESET_TREE, OnResetTree)
    ON_COMMAND(IDM_DATA_SAVE, OnExport)
    //}}AFX_MSG_MAP
END_MESSAGE_MAP()

//---------------------------------------------------
//関数名	CCharu3App()
//機能		CCharu3Appクラスのコンストラクタ
//---------------------------------------------------
CCharu3App::CCharu3App()// : m_treeDlg(&m_tree)
    : m_isCloseKey(false)
    , m_isStockMode(false)
    , m_nPhase(PHASE_START)
    , m_dwDoubleKeyPopTime(0)
    , m_dwDoubleKeyFifoTime(0)
    , m_hSelectItemBkup(NULL)
{
    m_focusInfo.m_hActiveWnd = NULL;
    m_focusInfo.m_hFocusWnd = NULL;

    int i, nCount = 0, nSize;

    int stKeycode[] = { 0x08,0x09,0x0c,0x0d,0x10,0x11,0x12,0x13,
                        0x14,0x1B,0x20,0x21,0x22,0x23,0x24,0x25,
                        0x26,0x27,0x28,0x29,0x2b,0x2c,0x2d,0x2e,
                        0x2f,0x6A,0x6B,0x6C,0x6D,0x6E,0x6F,0x90,
                        0x91,0x92,0xBB,0xBC, };

    const char* stKeyName[] = { "BS","TAB","CLEAR","ENTER","SHIFT","CTRL","ALT","PAUSE",
                        "CAPSLOCK","ESC","SPACE","PAGEUP","PAGEDOWN","END","HOME","LEFT",
                        "UP","RIGHT","DOWN","SELECT","EXECUTE","PRINTSCREEN","INSERT","DEL",
                        "HELP","NUM*","NUM+","NUMSEP","NUM-","NUM.","NUM/","NUMLOCK",
                        "SCROLLLOCK","NUM=","SEMICOLON","COMMA", };

    //テンキーの数字
    for (i = 0x60; i <= 0x69; i++, nCount++) {
        m_keyStruct[nCount].nKeyCode = i;
        m_keyStruct[nCount].strName.Format(_T("NUM%d"), i - 0x60);
    }
    //ファンクションキー
    for (i = 0x70; i <= 0x87; i++, nCount++) {
        m_keyStruct[nCount].nKeyCode = i;
        m_keyStruct[nCount].strName.Format(_T("F%d"), i - 0x6f);
    }

    nSize = _countof(stKeycode);
    //拡張キー
    for (i = 0; i < nSize; i++, nCount++) {
        m_keyStruct[nCount].nKeyCode = stKeycode[i];
        m_keyStruct[nCount].strName = stKeyName[i];
    }
    m_keyStruct[nCount].nKeyCode = -1;

    m_pTreeDlg = new CMyTreeDialog;
    m_pTree = new CCharu3Tree;
}
//---------------------------------------------------
//関数名	~CCharu3App()
//機能		CCharu3Appクラスのデストラクタ
//---------------------------------------------------
CCharu3App::~CCharu3App()
{
    if (m_pTreeDlg)	delete m_pTreeDlg;
    if (m_pTree)		delete m_pTree;
    if (m_hMutex)	CloseHandle(m_hMutex);
}

//---------------------------------------------------
// 唯一の CCharu3App オブジェクト
//---------------------------------------------------
CCharu3App theApp;

//---------------------------------------------------
//関数名	InitInstance()
//機能		CCharu3App クラスの初期化
//---------------------------------------------------
BOOL CCharu3App::InitInstance()
{
    //重複起動阻止
    LPCTSTR pszMutexObjectName = _T("Charu3");			//重複起動防止名
    m_hMutex = OpenMutex(MUTEX_ALL_ACCESS, FALSE, pszMutexObjectName);
    //	HWND hActiveWnd = ::GetForegroundWindow();
    Window::GetFocusInfo(&m_focusInfo);

    if (m_hMutex) {
        CloseHandle(m_hMutex);
        m_hMutex = NULL;
        return FALSE;
    }
    m_hMutex = CreateMutex(FALSE, 0, pszMutexObjectName);

#if false
    // no longer needed (obsolete)
#ifdef _AFXDLL
    Enable3dControls();		// 共有 DLL の中で MFC を使用する場合にはここを呼び出してください。
#else
    Enable3dControlsStatic();	// MFC と静的にリンクしている場合にはここを呼び出してください。
#endif
#endif

    //言語リソースを設定
    m_hLangDll = LoadLibrary(_T("c3language.dll"));
    if (m_hLangDll) {
        AfxSetResourceHandle(m_hLangDll);
    }

    //メインフォームの作成
    CMainFrame* pFrame = new CMainFrame;
    m_pMainWnd = pFrame;
    // フレームをリソースからロードして作成します
    pFrame->LoadFrame(IDR_MAINFRAME, WS_OVERLAPPEDWINDOW | FWS_ADDTOTITLE, NULL, NULL);
    pFrame->ShowWindow(SW_HIDE);
    pFrame->UpdateWindow();
    m_pMainFrame = pFrame;

    HWND hWndTop;
    m_hSelfWnd = CWnd::GetSafeOwner_(NULL, &hWndTop);

    m_pTreeDlg->SetTree(m_pTree);
    m_pTreeDlg->Create(IDD_DATA_TREE_VIEW, this->m_pMainWnd);
    //	::SetForegroundWindow(hActiveWnd);

    if (init()) {
        Window::SetFocusInfo(&m_focusInfo);
        //	setAppendKeyInit(m_focusInfo.m_hActiveWnd,&m_keySet);//キー設定を変更

        m_nPhase = PHASE_IDOL;
        return TRUE;
    }
    else {
        if (m_hLangDll) {
            FreeLibrary(m_hLangDll);
        }
        return FALSE;
    }
}

int CCharu3App::ExitInstance()
{
    if (m_hLangDll) {
        FreeLibrary(m_hLangDll);
    }
    return CWinApp::ExitInstance();
}

//---------------------------------------------------
//関数名	init()
//機能		初期化
//---------------------------------------------------
bool CCharu3App::init()
{
    m_ini.initialize();
    m_ini.setHookKey(m_hSelfWnd);
    {
        TCHAR strKeyLayoutName[256];
        GetKeyboardLayoutName(strKeyLayoutName);
        m_ini.m_keyLayout = LoadKeyboardLayout(strKeyLayoutName, KLF_REPLACELANG);
        if (m_ini.m_bDebug) {
            LOG(_T("KeyLayoutName \"%s\""), strKeyLayoutName);
        }
    }

    m_pTree->setImageList(theApp.m_ini.m_IconSize, theApp.m_ini.m_strResourceName, m_ini.m_strAppPath);
    m_pTree->setInitInfo(&m_ini.m_nTreeID, &m_ini.m_nSelectID, &m_ini.m_nRecNumber);//ID初期値を設定

    if (m_ini.m_bDebug) {
        LOG(_T("Start"));
    }

    m_pTree->setPlugin(m_ini.m_strRwPluginFolder);

    {
        bool ok = m_pTree->loadDataFileDef(m_ini.m_strDataPath, m_ini.m_strDataFormat);
        while (!ok) {
            CInitialDialog dlg;
            int ret = dlg.DoModal();
            switch (ret) {
            case IDC_NEW:
            {
                CString file = CommonDialog::NewFilePath(_T(DAT_EXT));
                if (file != _T("")) {
                    m_ini.m_strDataPath = file;
                    m_ini.m_strDataFormat = DAT_FORMAT;
                    m_ini.writeEnvInitData();
                    CString text, caption;
                    (void)text.LoadString(APP_MES_NEWFILE_READY);
                    (void)caption.LoadStringW(AFX_IDS_APP_TITLE);
                    MessageBox(m_hSelfWnd, text, caption, MB_OK | MB_ICONINFORMATION);
                    ok = true;
                }
                break;
            }
            case IDC_OPEN:
                if (SelectFile()) {
                    CString text, caption;
                    (void)text.LoadString(APP_MES_DATA_READY);
                    (void)caption.LoadStringW(AFX_IDS_APP_TITLE);
                    MessageBox(m_hSelfWnd, text, caption, MB_OK | MB_ICONINFORMATION);
                    ok = true;
                }
                break;
            case IDC_QUIT:
            case IDCANCEL:
            default:
                return false;
            }
        }
    }

    if (m_ini.m_bDebug) {
        LOG(_T("read data file\"%s\" %s"), m_ini.m_strDataPath.GetString(), m_ini.m_strDataFormat.GetString());
    }

    //クリップボードクラスの初期化 変更検知を設定(メインフレームでメッセージ処理をしてます)
    GetClipboardText(m_strSavedClipboard);
    m_clipboard.SetParent(this->m_pMainWnd->m_hWnd);
    m_keySet = m_ini.m_defKeySet;

    //ホットキーを設定
    RegisterHotkeys();
    RegisterAdditionalHotkeys();

    return true;
}

void CCharu3App::BeForeground()
{
    Window::SetAbsoluteForegroundWindow(m_pMainFrame->m_hWnd);
}

void CCharu3App::CheckFocusInfo(HWND hActiveWnd)
{
    if (hActiveWnd != m_focusInfo.m_hActiveWnd) {
        Window::GetFocusInfo(&m_focusInfo);
    }
}

void CCharu3App::OnClick(HWND hActiveWnd)
{
    if (m_ini.m_nIconClick == 0) {
        if (GetPhase() == PHASE_IDOL) {
            // Pop-up Data Tree View Window at the mouse pointer.
            POINT pos;
            GetCursorPos(&pos);
            pos.x -= m_ini.m_treeviewSize.x;
            pos.y -= m_ini.m_treeviewSize.y;
            adjustLocation(&pos);
            // Window::GetFocusInfo(&theApp.m_focusInfo,hForeground);
            popupTreeWindow(pos, m_ini.m_bKeepSelection);
        }
    }
    else {
        // Toggle Stock Mode
        ToggleStockMode();
    }
}

void CCharu3App::RedrawDataTreeView()
{
    if (m_pTreeDlg->IsWindowVisible()) {
        m_pTreeDlg->RedrawWindow(NULL, NULL, RDW_FRAME | RDW_INVALIDATE);
    }
}

//---------------------------------------------------
//関数名	popupTreeWindow
//機能		ポップアップを表示
//---------------------------------------------------
void CCharu3App::popupTreeWindow(POINT pos, bool keepSelection, HTREEITEM hOpenItem)
{
    if (m_nPhase != PHASE_IDOL) return;
    m_nPhase = PHASE_POPUP;
    m_ini.unHookKey();
    UnregisterAdditionalHotkeys();//追加ホットキーを停止

    // Window::GetFocusInfo(&m_focusInfo);

    if (m_focusInfo.m_hActiveWnd == this->m_pMainFrame->m_hWnd) {
        m_nPhase = PHASE_IDOL;
        return;
    }

    if (m_isStockMode)	KillTimer(m_pMainWnd->m_hWnd, TIMER_ACTIVE);//監視タイマー停止

    if (m_ini.m_bDebug) {
        LOG(_T("popupTreeWindow x=%d y=%d keep=%s"), pos.x, pos.y, keepSelection ? _T("true") : _T("false"));
    }

    Window::SetAbsoluteForegroundWindow(m_pMainWnd->m_hWnd);//自分をアクティブに設定
    m_pTreeDlg->ShowWindowPos(pos, m_ini.m_treeviewSize, SW_SHOW, keepSelection, hOpenItem);
}

//---------------------------------------------------
//関数名	adjustLocation(POINT pos)
//機能		ポップアップ位置を必ずデスクトップ内にする
//---------------------------------------------------
void CCharu3App::adjustLocation(POINT* pos)
{
    RECT DeskTopSize;
    int nMonCount = GetSystemMetrics(SM_CMONITORS);

    if (nMonCount <= 1) {
        //デスクトップのサイズを取得
        int nDektopWidth = GetSystemMetrics(SM_CXSCREEN);
        int nDesktopHeight = GetSystemMetrics(SM_CYFULLSCREEN);
        //解像度の取得
        int nScreenWidth = GetSystemMetrics(SM_CXSCREEN);
        int nScreenHeight = GetSystemMetrics(SM_CYSCREEN);

        if (m_ini.m_bDebug) {
            LOG(_T("reviseWindowPos %d %d %d %d"), nDektopWidth, nDesktopHeight, nScreenWidth, nScreenHeight);
        }

        HWND hDeskTop = GetDesktopWindow();
        if (hDeskTop) ::GetWindowRect(hDeskTop, &DeskTopSize);
        else return;
    }
    else {
        CArray<RECT, RECT> arrayRect;
        arrayRect.RemoveAll();
        EnumDisplayMonitors(NULL, NULL, (MONITORENUMPROC)MonitorEnumFunc, (LPARAM)&arrayRect);

        int nSize = arrayRect.GetSize(), nCurrentMon = 0, nWidth, nHeight;
        CString strBuff;
        double dDistance, dMinDistance = -1.0f;
        for (int i = 0; i < nSize; i++) {
            strBuff.Format(_T("left:%d top:%d right:%d bottom:%d"), arrayRect[i].left, arrayRect[i].top, arrayRect[i].right, arrayRect[i].bottom);
            nWidth = abs((arrayRect[i].left + arrayRect[i].right) / 2 - pos->x);
            nHeight = abs((arrayRect[i].top + arrayRect[i].bottom) / 2 - pos->y);

            dDistance = sqrt((nWidth * nWidth) + (nHeight * nHeight));
            if (dDistance < dMinDistance || i == 0) {
                dMinDistance = dDistance;
                nCurrentMon = i;
            }
        }
        DeskTopSize = arrayRect[nCurrentMon];
    }
    //ウィンドウ位置を補正
    if (pos->y + m_ini.m_treeviewSize.y > DeskTopSize.bottom)
        pos->y -= (pos->y + m_ini.m_treeviewSize.y) - DeskTopSize.bottom;
    if (pos->x + m_ini.m_treeviewSize.x > DeskTopSize.right)
        pos->x -= (pos->x + m_ini.m_treeviewSize.x) - DeskTopSize.right;

    if (pos->y < DeskTopSize.top)
        pos->y = DeskTopSize.top;
    if (pos->x < DeskTopSize.left)
        pos->x = DeskTopSize.left;
}

//---------------------------------------------------
//関数名	closeTreeWindow(int nRet)
//機能		ポップアップ隠蔽処理
//---------------------------------------------------
void CCharu3App::closeTreeWindow(int nRet)
{
    m_pTreeDlg->ShowWindow(SW_HIDE);
    bool isPaste = true;
    if (::GetAsyncKeyState(VK_SHIFT) < 0) isPaste = false;

    if (m_ini.m_bDebug) {
        LOG(_T("closeTreeWindow %d"), nRet);
    }

    //アクティブウィンドウを復帰
    if (nRet == IDOK) {
        CString strClip, strSelect;
        GetClipboardText(strClip);

        setAppendKeyInit(m_focusInfo.m_hActiveWnd, &m_keySet);//キー設定を変更
        //キーが離されるのを待つ
        while (::GetAsyncKeyState(VK_MENU) < 0 || ::GetAsyncKeyState(VK_CONTROL) < 0 ||
            ::GetAsyncKeyState(VK_SHIFT) < 0 || ::GetAsyncKeyState(VK_RETURN) < 0) Sleep(50);
        Window::SetFocusInfo(&m_focusInfo);//ターゲットをフォーカス

        if (m_ini.m_bDebug) {
            LOG(_T("setFocusInfo active:%p focus:%p"), m_focusInfo.m_hActiveWnd, m_focusInfo.m_hFocusWnd);
        }

        //貼り付け処理
        if (m_pTree->m_ltCheckItems.size() > 0) {  // Selected multiple items.
            strSelect = GetSelectedText();  // TODO: Poor performance because it is always done without considering the need.

            if (m_ini.m_bDebug) {
                LOG(_T("closeTreeWindow sel:%s clip:%s"), strSelect.GetString(), strClip.GetString());
            }

            std::list<HTREEITEM>::iterator it;
            for (it = m_pTree->m_ltCheckItems.begin(); it != m_pTree->m_ltCheckItems.end(); it++) {
                if (m_pTree->GetItemState(*it, TVIF_HANDLE)) {
                    const STRING_DATA* dataPtr = m_pTree->getDataPtr(*it);
                    if (dataPtr->m_cKind & KIND_DATA_ALL) {
                        if (m_ini.m_bDebug) {
                            LOG(_T("closeTreeWindow check paste %s"), dataPtr->m_strTitle.GetString());
                        }
                        playData(dataPtr, strClip, strSelect, isPaste, false);
                    }
                }
            }
            //一時項目は消す
            for (it = m_pTree->m_ltCheckItems.begin(); it != m_pTree->m_ltCheckItems.end(); it++) {
                if (m_pTree->GetItemState(*it, TVIF_HANDLE)) {
                    HTREEITEM hItem = *it;
                    *it = NULL;
                    if (m_pTree->getDataPtr(hItem)->m_cKind & KIND_ONETIME) {
                        //m_hSelectItemBkup = NULL;
                    }
                }
            }
        }
        else {
            const STRING_DATA* dataPtr = m_pTreeDlg->GetSelectedDataPtr();
            if (nullptr != dataPtr) {
                bool requiresSelectionText = (dataPtr->m_strData.Find(_T("$SEL")) != -1);  // TODO: This test is true even if $SEL is outside <charu3MACRO>
                strSelect = requiresSelectionText ? GetSelectedText() : _T("");

                if (m_ini.m_bDebug) {
                    LOG(_T("closeTreeWindow sel:%s clip:%s title:%s"), strSelect.GetString(), strClip.GetString(), dataPtr->m_strTitle.GetString());
                }
                playData(dataPtr, strClip, strSelect, isPaste);
            }
        }
    }
    else if (::GetForegroundWindow() == m_focusInfo.m_hActiveWnd) {
        Window::SetFocusInfo(&m_focusInfo);
    }
    if (m_hSelectItemBkup) {
        m_pTree->SelectItem(m_hSelectItemBkup);
        m_hSelectItemBkup = NULL;
    }

    //データを保存
    SaveData();
    m_ini.writeEnvInitData();//環境設定を保存

    //監視タイマーセット
    if (m_isStockMode && m_ini.m_nWindowCheckInterval > 0) {
        SetTimer(m_pMainWnd->m_hWnd, TIMER_ACTIVE, m_ini.m_nWindowCheckInterval, NULL);
    }

    RegisterAdditionalHotkeys();//追加ホットキーを設定
    m_ini.setHookKey(m_hSelfWnd);

    if (m_pTree->GetStyle() & TVS_CHECKBOXES) {
        resetTreeDialog();
        Window::SetFocusInfo(&m_focusInfo);
    }
    ASSERT(m_nPhase == PHASE_POPUP);
    m_nPhase = PHASE_IDOL;
}

bool CCharu3App::SelectFile()
{
    CString strDisplay, strPattern;
    (void)strDisplay.LoadString(APP_INF_FILE_FILTER_C3D_DISPLAY);
    (void)strPattern.LoadString(APP_INF_FILE_FILTER_C3D_PATTERN);
    CString strFilter = strDisplay + _T('\0') + strPattern + _T('\0');
    for (std::vector<RW_PLUGIN>::iterator it = m_pTree->m_rwPlugin.begin(); it != m_pTree->m_rwPlugin.end(); it++) {
        CString strFormat, strDisplay, strPattern;
        (void)strFormat.LoadString(APP_INF_FILE_FILTER_FMT_DISPLAY);
        strDisplay.Format(strFormat, it->m_strFormatName, it->m_strExtension);
        (void)strFormat.LoadString(APP_INF_FILE_FILTER_FMT_PATTERN);
        strPattern.Format(strFormat, it->m_strExtension);
        strFilter = strFilter + strDisplay + _T('\0') + strPattern + _T('\0'); // NOTE: Don't use operator +=
    }
    strFilter = strFilter + _T('\0') + _T('\0'); // NOTE: Don't use operator +=

    OPENFILENAME param;
    ZeroMemory(&param, sizeof param);
    TCHAR tcPath[MAX_PATH] = _T("");
    param.lStructSize = sizeof param;
    param.hwndOwner = m_hSelfWnd;
    param.lpstrFilter = strFilter.GetBuffer();
    param.lpstrCustomFilter = NULL;
    param.nFilterIndex = 1;
    param.lpstrFile = tcPath;
    param.nMaxFile = MAX_PATH;
    param.lpstrFileTitle = NULL;
    param.lpstrInitialDir = NULL;
    param.lpstrTitle = NULL;
    param.Flags = OFN_FILEMUSTEXIST;
    param.nFileOffset = 0;
    param.nFileExtension = 0;
    param.lpstrDefExt = NULL;
    if (GetOpenFileName(&param)) {
        if (m_ini.m_bDebug) {
            LOG(_T("SelectFile \"%s\""), tcPath);
        }

        CString path = CString(tcPath);
        CString format = param.nFilterIndex < 2 ? DAT_FORMAT : m_pTree->m_rwPlugin[param.nFilterIndex - 2].m_strFormatName;

        if (m_pTree->loadDataFileDef(path, format)) {
            m_ini.m_strDataPath = path;
            m_ini.m_strDataFormat = format;
            m_ini.m_bReadOnly = ((param.Flags & OFN_READONLY) != 0);
            m_ini.writeEnvInitData();
            return true;
        }
        else {
            CString strRes;
            (void)strRes.LoadString(APP_MES_UNKNOWN_FORMAT);
            AfxMessageBox(strRes, MB_ICONEXCLAMATION, 0);
            return false;
        }
    }
    else {
        if (m_ini.m_bDebug) {
            LOG(_T("SelectFile failed"));
        }
        return false;
    }
}

void CCharu3App::SaveData()
{
    if (m_ini.m_bReadOnly) {
        return;
    }
    if (m_pTree->saveDataToFile(m_ini.m_strDataPath, m_ini.m_strDataFormat, NULL)) {
        return;
    }

    CString strRes;
    (void)strRes.LoadString(APP_MES_SAVE_FAILURE);
    AfxMessageBox(strRes, MB_ICONEXCLAMATION, 0);
}

//---------------------------------------------------
//関数名	registerHotkeys()
//機能		ホットキーを設定する
//---------------------------------------------------
void CCharu3App::RegisterHotkeys()
{
    if (m_ini.m_bDebug) {
        LOG(_T("registerHotkeys"));
    }

    UINT uMod = 0, uVK = 0;
    switch (m_ini.m_nDoubleKeyPOP) {
    case 1:	uMod = MOD_SHIFT; break;
    case 2:	uMod = MOD_CONTROL; break;
    case 3:	uMod = MOD_ALT; break;
    default:
    case 0:
        uMod = m_ini.m_uMod_Pouup;
        uVK = m_ini.m_uVK_Pouup;
        break;
    }
    if (!RegisterHotKey(NULL, HOTKEY_POPUP, uMod, uVK)) {//ポップアップキー
        if (m_ini.m_bDebug) {
            LOG(_T("Failed to register popup hotkey"));
        }
    }

    uMod = 0, uVK = 0;
    switch (m_ini.m_nDoubleKeyFIFO) {
    case 1:	uMod = MOD_SHIFT; break;
    case 2:	uMod = MOD_CONTROL; break;
    case 3:	uMod = MOD_ALT; break;
    default:
    case 0:
        uMod = m_ini.m_uMod_Fifo;
        uVK = m_ini.m_uVK_Fifo;
        break;
    }
    if (!RegisterHotKey(NULL, HOTKEY_FIFO, uMod, uVK)) {//履歴FIFOキー
        if (m_ini.m_bDebug) {
            LOG(_T("Failed to register stock mode hotkey"));
        }
    }
}

//---------------------------------------------------
//関数名	unregisterHotkeys()
//機能		ホットキーを止める
//---------------------------------------------------
void CCharu3App::UnregisterHotkeys()
{
    if (m_ini.m_bDebug) {
        LOG(_T("unregisterHotkeys"));
    }

    UnregisterHotKey(NULL, HOTKEY_POPUP);
    UnregisterHotKey(NULL, HOTKEY_FIFO);
}

//---------------------------------------------------
//関数名	registerdditionalHotkeys()
//機能		追加ホットキーを設定
//---------------------------------------------------
void CCharu3App::RegisterAdditionalHotkeys()
{
    HOT_KEY_CODE keyData;
    std::list<STRING_DATA>::iterator it;

    m_hotkeyVector.clear();

    CString strKey;
    STRING_DATA* data;
    HTREEITEM hRet = NULL;

    if (m_isStockMode)
        RegisterHotKey(NULL, HOTKEY_PASTE, m_keySet.m_uMod_Paste, m_keySet.m_uVK_Paste);//ペーストキー

    int nSize = m_pTree->m_MyStringList.size();
    HTREEITEM hTreeItem = m_pTree->GetRootItem();
    for (int i = 0; i < nSize && hTreeItem; hTreeItem = m_pTree->GetTrueNextItem(hTreeItem), i++) {
        if (hTreeItem) {
            data = m_pTree->getDataPtr(hTreeItem);
            strKey = m_pTree->getDataOptionStr(data->m_strMacro, EXMACRO_HOT_KEY);
            if (strKey != "") {
                ParseHotkeyDescriptor(strKey, &keyData.m_uModKey, &keyData.m_uVkCode, &keyData.m_bDoubleStroke);
                keyData.m_strMacroName = EXMACRO_HOT_KEY;
                keyData.m_nDataID = data->m_nMyID;
                keyData.m_hItem = hTreeItem;
                keyData.m_dwDoubleKeyTime = 0;
                m_hotkeyVector.insert(m_hotkeyVector.end(), keyData);//設定アレイに追加
                int nret = RegisterHotKey(NULL, HOT_ITEM_KEY + m_hotkeyVector.size() - 1, keyData.m_uModKey, keyData.m_uVkCode);

                if (m_ini.m_bDebug) {
                    LOG(_T("registerAdditionalHotkeys hotkey \"%s\" %d"), strKey.GetString(), nret);
                }
            }
            strKey = m_pTree->getDataOptionStr(data->m_strMacro, EXMACRO_DIRECT_COPY);
            if (strKey != "") {
                ParseHotkeyDescriptor(strKey, &keyData.m_uModKey, &keyData.m_uVkCode, &keyData.m_bDoubleStroke);
                keyData.m_strMacroName = EXMACRO_DIRECT_COPY;
                keyData.m_nDataID = data->m_nMyID;
                keyData.m_hItem = hTreeItem;
                keyData.m_dwDoubleKeyTime = 0;
                m_hotkeyVector.insert(m_hotkeyVector.end(), keyData);//設定アレイに追加
                int nret = RegisterHotKey(NULL, HOT_ITEM_KEY + m_hotkeyVector.size() - 1, keyData.m_uModKey, keyData.m_uVkCode);

                if (m_ini.m_bDebug) {
                    LOG(_T("registerAdditionalHotkeys directcopy \"%s\" %d"), strKey.GetString(), nret);
                }
            }
        }
    }
}

//---------------------------------------------------
//関数名	unregisterAdditionalHotkeys()
//機能		追加ホットキーをすべて止める
//---------------------------------------------------
void CCharu3App::UnregisterAdditionalHotkeys()
{
    UnregisterHotKey(NULL, HOTKEY_PASTE);
    int nSize = m_hotkeyVector.size();
    for (int i = 0; i < nSize; i++) {
        UnregisterHotKey(NULL, HOT_ITEM_KEY + i);
    }

    if (m_ini.m_bDebug) {
        LOG(_T("stopAppendHotKey"));
    }
}

//---------------------------------------------------
//関数名	ParseHotkeyDescriptor()
//機能		文字列からホットキーの設定を解読
//---------------------------------------------------
void CCharu3App::ParseHotkeyDescriptor(CString strKey, UINT* pMod, UINT* pVK, bool* pDoubleStroke)
{
    CString strVK, strMod;
    strKey.MakeUpper();
    *pMod = 0;
    *pDoubleStroke = false;
    int indexPlus = strKey.ReverseFind('+');
    if (indexPlus < 0) { // strkey does not have a '+'.
        strVK = strKey.Trim();
        if (strVK == "SHIFT*2") {
            *pMod = MOD_SHIFT;
        }
        else if (strVK == "CTRL*2") {
            *pMod = MOD_CONTROL;
        }
        else if (strVK == "ALT*2") {
            *pMod = MOD_ALT;
        }
        if (*pMod) {
            *pDoubleStroke = true;
            *pVK = 0;
            return;
        }
    }
    else { // strKey has '+'.
        strVK = strKey.Mid(indexPlus + 1).Trim();
        if (strKey.Find(_T("SHIFT")) >= 0) {
            *pMod = *pMod | MOD_SHIFT;
        }
        if (strKey.Find(_T("CTRL")) >= 0) {
            *pMod = *pMod | MOD_CONTROL;
        }
        if (strKey.Find(_T("ALT")) >= 0) {
            *pMod = *pMod | MOD_ALT;
        }
    }
    *pVK = getKeycode(strVK.GetBuffer(strVK.GetLength()), false);
}
//---------------------------------------------------
//関数名	getKeycode(char *szKeyName)
//機能		キー名からコードを得る
//---------------------------------------------------
int CCharu3App::getKeycode(TCHAR* szKeyName, bool scanLayout)
{
    CString strTmp;
    strTmp = szKeyName;
    strTmp.MakeUpper();

    int i, nRet = 0;

    if (strTmp.Find(_T("0X")) >= 0)
        _stscanf_s(szKeyName, _T("0x%2x"), &nRet);

    if (!nRet) {
        if (_tcsclen(szKeyName) == 1) {
            if (scanLayout || *szKeyName > 0x7f) {
                SHORT code = VkKeyScanEx(*szKeyName, m_ini.m_keyLayout);
                nRet = code & 0xff;
            }
            else {
                nRet = toupper(*szKeyName);
            }
        }
        else {
            for (i = 0; m_keyStruct[i].nKeyCode > 0 && i < 256; i++) {
                if (m_keyStruct[i].strName == strTmp) {
                    nRet = m_keyStruct[i].nKeyCode;
                    break;
                }
            }
        }
    }
    return nRet;
}

//---------------------------------------------------
//関数名	setAppendKeyInit(HWND hTopWindow)
//機能		追加キー設定
//---------------------------------------------------
bool CCharu3App::setAppendKeyInit(HWND hTopWindow, COPYPASTE_KEY* keySet)
{
    TCHAR strWindowName[1024] = {};
    *strWindowName = (TCHAR)NULL;
    CString strWinName;
    bool isRet = false;

    if (!hTopWindow || m_hActiveKeyWnd == hTopWindow) return isRet;//前と同じハンドルなら帰る
    if (!GetWindowText(hTopWindow, strWindowName, _countof(strWindowName))) return isRet;//キャプションを取得

    strWinName = strWindowName;
    *keySet = m_ini.getAppendKeyInit(strWinName);
    if (keySet->m_nMessage <= -1)	*keySet = m_ini.m_defKeySet;
    m_hActiveKeyWnd = hTopWindow;

    if (m_ini.m_bDebug) {
        LOG(_T("setAppendKeyInit %s %d %d %d %d %d"), strWinName.GetString(), keySet->m_uMod_Copy, keySet->m_uVK_Copy, keySet->m_uMod_Paste, keySet->m_uVK_Paste, keySet->m_nMessage);
    }

    isRet = true;
    return isRet;
}

//---------------------------------------------------
//関数名	playData(STRING_DATA data,CString strClip)
//機能		データの展開処理をする
//---------------------------------------------------
void CCharu3App::playData(const STRING_DATA* dataPtr, CString strClip, CString strSelect, bool isPaste, bool isChange)
{
    HTREEITEM hSelectItem = m_pTree->GetSelectedItem();
    CString strPaste;

    //マクロ処理
    {
        HTREEITEM hMacroItem = NULL;
        if (hSelectItem) {
            hMacroItem = m_pTree->searchParentOption(hSelectItem, _T("macro"));//一番近い親か自分のマクロを調べる
        }
        CString strMacro = m_ini.m_strDataFormat;
        if (hMacroItem) {
            STRING_DATA* macroDataPtr = m_pTree->getDataPtr(hMacroItem);
            strMacro = m_pTree->getDataOptionStr(macroDataPtr->m_strMacro, _T("macro"));
        }
        strPaste = convertMacro(dataPtr, strSelect, strClip, strMacro);
    }

    //テキストを貼り付け処理
    if (isPaste) {
        if (m_ini.m_bDebug) {
            LOG(_T("playData active:%p focus:%p"), m_focusInfo.m_hActiveWnd, m_focusInfo.m_hFocusWnd);
        }

        execData(strPaste, m_keySet, hSelectItem, m_focusInfo.m_hFocusWnd);

        //クリップボード復帰
        if (m_ini.m_bPutBackClipboard && strClip != "") {
            m_strSavedClipboard = strClip;
            SetClipboardText(strClip.GetString());
        }
        if (isChange) {
            //貼り付けデータをフォルダの先頭に移動
            HTREEITEM hMoveItem = NULL;
            if (hSelectItem) {
                hMoveItem = m_pTree->searchParentOption(hSelectItem, _T("move"));
            }
            if (hMoveItem) {
                STRING_DATA* macroDataPtr = m_pTree->getDataPtr(hMoveItem);
                int nIsMove = m_pTree->getDataOption(macroDataPtr->m_strMacro, _T("move"));
                if (nIsMove) {
                    hSelectItem = m_pTree->moveFolderTop(hSelectItem);
                    if (hSelectItem) m_pTree->SelectItem(hSelectItem);
                }
            }
            //一時項目は消す
            if (m_pTree->getDatakind(hSelectItem) & KIND_ONETIME) {
                m_pTree->DeleteData(hSelectItem);
                m_hSelectItemBkup = NULL;
            }
        }
    }
    else {
        SetClipboardText(strPaste);
    }
}

//---------------------------------------------------
//関数名	playHotItem(nTarget)
//機能		ホットアイテム処理
//---------------------------------------------------
void CCharu3App::playHotItem(int nTarget)
{
    if (nTarget >= 0 && static_cast<size_t>(nTarget) < m_hotkeyVector.size()) {
        HOT_KEY_CODE keyData;
        keyData = m_hotkeyVector.at(nTarget);
        if (!keyData.m_uVkCode) {
            DWORD dwTime = timeGetTime();
            if (static_cast<int>(dwTime - keyData.m_dwDoubleKeyTime) > m_ini.m_nDCKeyPopTime) {
                m_hotkeyVector[nTarget].m_dwDoubleKeyTime = dwTime;
                return;
            }
        }
        STRING_DATA data;//テキストデータ

        if (keyData.m_hItem) {
            data = m_pTree->getData(keyData.m_hItem);
            CString strMacroData;
            CString strSelect, strMacro;
            //フォルダの場合
            if (data.m_cKind & KIND_FOLDER_ALL) {
                POINT pos;
                //ダイレクトコピー
                if (keyData.m_strMacroName == EXMACRO_DIRECT_COPY) {
                    m_nPhase = PHASE_LOCK;
                    UnregisterAdditionalHotkeys();//追加ホットキーを停止
                    STRING_DATA dataChild;
                    Window::GetCaretPos(&pos, &m_focusInfo);//キャレット位置を取得(ハンドルを取るため)

                    //キーが離されるのを待つ
                    while (::GetAsyncKeyState(VK_MENU) < 0 || ::GetAsyncKeyState(VK_CONTROL) < 0 ||
                        ::GetAsyncKeyState(VK_SHIFT) < 0 || ::GetAsyncKeyState(keyData.m_uVkCode) < 0) Sleep(50);
                    keyUpDown(keyData.m_uModKey, keyData.m_uVkCode, KEY_UP);//キーを離す処理（これが無いと選択テキスト取得で失敗）

                    setAppendKeyInit(m_focusInfo.m_hActiveWnd, &m_keySet);//キー設定を変更
                    strSelect = GetSelectedText();
                    if (strSelect != "") {
                        dataChild.m_cKind = KIND_LOCK;
                        dataChild.m_nParentID = data.m_nMyID;
                        dataChild.m_strData = strSelect;
                        int nTitleLength = m_pTree->getDataOption(data.m_strMacro, _T("titlelen"));//タイトルの文字数
                        if (nTitleLength < 1 || nTitleLength > 256) nTitleLength = 32;
                        dataChild.m_strTitle = m_pTree->makeTitle(strSelect, nTitleLength);

                        if (m_ini.m_bDebug) {
                            LOG(_T("Direct copy folder %s"), dataChild.m_strTitle.GetString());
                        }

                        m_pTree->AddData(keyData.m_hItem, dataChild, true);
                    }
                    RegisterAdditionalHotkeys();//追加ホットキーを設定
                    m_nPhase = PHASE_IDOL;
                }
                //ホットキー
                else if (keyData.m_strMacroName == EXMACRO_HOT_KEY) {
                    m_hSelectItemBkup = m_pTree->GetSelectedItem();

                    m_pTree->SelectItem(keyData.m_hItem);
                    getPopupPos(&pos, m_ini.m_nPopupPos);//ポップアップ位置を取得
                    adjustLocation(&pos);
                    popupTreeWindow(pos, true, keyData.m_hItem);
                }
            }
            else {//定形文は一発貼り付け
                m_nPhase = PHASE_LOCK;
                UnregisterAdditionalHotkeys();//追加ホットキーを停止

                CString strClip, strPaste;
                GetClipboardText(strClip);
                strPaste = data.m_strData;

                POINT pos;

                //キーが離されるのを待つ
                while (::GetAsyncKeyState(VK_MENU) < 0 || ::GetAsyncKeyState(VK_CONTROL) < 0 ||
                    ::GetAsyncKeyState(VK_SHIFT) < 0 || ::GetAsyncKeyState(keyData.m_uVkCode) < 0) Sleep(50);
                keyUpDown(keyData.m_uModKey, keyData.m_uVkCode, KEY_UP);//キーを離す処理（これが無いと選択テキスト取得で失敗）

                int nIsMove;

                //ダイレクトコピー
                if (keyData.m_strMacroName == EXMACRO_DIRECT_COPY) {
                    Window::GetCaretPos(&pos, &m_focusInfo);//キャレット位置を取得
                    setAppendKeyInit(m_focusInfo.m_hActiveWnd, &m_keySet);//キー設定を変更
                    CString selectedText = GetSelectedText();
                    m_pTree->SetText(keyData.m_hItem, selectedText);

                    if (m_ini.m_bDebug) {
                        LOG(_T("Direct copy data %s"), data.m_strTitle.GetString());
                    }
                }
                //ホットキー
                else if (keyData.m_strMacroName == EXMACRO_HOT_KEY) {
                    //マクロ処理
                    {
                        HTREEITEM hMacroItem = m_pTree->searchParentOption(keyData.m_hItem, _T("macro"));//一番近い親か自分のマクロを調べる
                        strMacro = m_ini.m_strDataFormat;
                        if (hMacroItem) {
                            STRING_DATA* macroDataPtr = m_pTree->getDataPtr(hMacroItem);
                            strMacro = m_pTree->getDataOptionStr(macroDataPtr->m_strMacro, ("macro"));
                        }
                    }

                    Window::GetCaretPos(&pos, &m_focusInfo);//キャレット位置を取得
                    setAppendKeyInit(m_focusInfo.m_hActiveWnd, &m_keySet);//キー設定を変更

                    strSelect = GetSelectedText();
                    strPaste = convertMacro(&data, strSelect, strClip, strMacro);//マクロ変換

                    if (m_ini.m_bDebug) {
                        LOG(_T("Direct paste data %s active:%p focus:%p"), strPaste.GetString(), m_focusInfo.m_hActiveWnd, m_focusInfo.m_hFocusWnd);
                    }

                    //テキストを貼り付け処理
                    execData(strPaste, m_keySet, keyData.m_hItem, m_focusInfo.m_hFocusWnd);

                    //貼り付けデータをフォルダの先頭に移動
                    {
                        HTREEITEM hMoveItem = m_pTree->searchParentOption(keyData.m_hItem, _T("move"));
                        if (hMoveItem) {
                            STRING_DATA* macroDataPtr = m_pTree->getDataPtr(hMoveItem);
                            nIsMove = m_pTree->getDataOption(macroDataPtr->m_strMacro, _T("move"));
                            if (nIsMove) {
                                keyData.m_hItem = m_pTree->moveFolderTop(keyData.m_hItem);
                            }
                        }
                    }
                    //クリップボード復帰
                    if (m_ini.m_bPutBackClipboard && strClip != "") {
                        SetClipboardText(strClip.GetString());
                    }
                    //一時項目は消す
                    if (m_pTree->getDatakind(keyData.m_hItem) & KIND_ONETIME) {
                        m_pTree->DeleteData(keyData.m_hItem);
                    }
                }
                RegisterAdditionalHotkeys();//追加ホットキーを設定
                m_nPhase = PHASE_IDOL;
            }
        }
    }
    return;
}

CString CCharu3App::GetSelectedText()
{
    SetClipboardText(CString());

    // Get the active window name.
    TCHAR windowName[1024] = _T("");
    GetWindowText(m_focusInfo.m_hActiveWnd, windowName, _countof(windowName));
    CString csWindowName(windowName);

    // Instructs the active window to copy the selected text to the clipboard.
    COPYPASTE_KEY keySet;
    keySet.m_nMessage = 0;
    for (int i = 0; keySet.m_nMessage > -1; i++) { 
        keySet = m_ini.getAppendKeyInit(csWindowName, i);
        if (keySet.m_nMessage <= -1 && i == 0) {
            keySet = m_keySet;
        }
        if (keySet.m_uVK_Copy) {
            if (keySet.m_nMessage > -1) {
                if (keySet.m_nMessage == 0) {  //イベント方式
                    keyUpDown(keySet.m_uMod_Copy, keySet.m_uVK_Copy, KEY_DOWN);
                    keyUpDown(keySet.m_uMod_Copy, keySet.m_uVK_Copy, KEY_UP);
                    Sleep(keySet.m_nCopyWait);
                    keyUpDownMessage(keySet.m_uMod_Copy, keySet.m_uVK_Copy, KEY_UP, m_focusInfo.m_hActiveWnd);
                }
                else if (keySet.m_nMessage == 1) {  //Charu2Pro方式
                    keyUpDownC2(keySet.m_uMod_Copy, keySet.m_uVK_Copy, KEY_DOWN);
                    keyUpDownC2(keySet.m_uMod_Copy, keySet.m_uVK_Copy, KEY_UP);
                    Sleep(keySet.m_nCopyWait);
                    keyUpDownMessage(keySet.m_uMod_Copy, keySet.m_uVK_Copy, KEY_DOWN, m_focusInfo.m_hActiveWnd);
                    Sleep(keySet.m_nCopyWait / 4);
                    keyUpDownMessage(keySet.m_uMod_Copy, keySet.m_uVK_Copy, KEY_DOWN, m_focusInfo.m_hActiveWnd);
                    keyUpDownMessage(keySet.m_uMod_Copy, keySet.m_uVK_Copy, KEY_UP, m_focusInfo.m_hActiveWnd);
                    Sleep(keySet.m_nCopyWait);
                }
                else if (keySet.m_nMessage == 2) {  //メッセージ方式
                    ::PostMessage(m_focusInfo.m_hFocusWnd, m_keySet.m_copyMessage.Msg, m_keySet.m_copyMessage.wParam, m_keySet.m_copyMessage.lParam);
                    // ::SendMessage(m_focusInfo.m_hFocusWnd,WM_COPY,NULL,NULL);
                }
            }
        }
    }

    // Get from the clipboard.
    CString strSelect;
    if (GetClipboardText(strSelect)) {
        if (m_ini.m_bDebug) {
            LOG(_T("GetSelectedText \"%s\""), strSelect.GetString());
        }
        return strSelect;
    }

    if (m_ini.m_bDebug) {
        LOG(_T("GetSelectedText failed"));
    }
    return CString();
}

//---------------------------------------------------
//関数名	execKeyMacro(CString strKeyMacro)
//機能		キーボードマクロを実行
//---------------------------------------------------
void CCharu3App::execKeyMacro(CString strKeyMacro)
{
    TCHAR* szKeyMacro, strKeyCode[256], * szSplit;
    int nCount, nSleep, nKey[16] = {}, i, nKeyCount;
    szKeyMacro = strKeyMacro.GetBuffer(strKeyMacro.GetLength());

    //貼り付け後のスリープを取得
    szSplit = Text::Awk(szKeyMacro, strKeyCode, _countof(strKeyCode), 3, _T(';'));
    if (szSplit) {
        nSleep = _ttoi(strKeyCode);
        if (nSleep > 10000) nSleep = 10000;
    }
    else         nSleep = 0;

    //貼り付け回数を取得
    szSplit = Text::Awk(szKeyMacro, strKeyCode, _countof(strKeyCode), 2, _T(';'));
    if (szSplit) {
        nCount = _ttoi(strKeyCode);
        if (nCount > 256) nCount = 256;
        *szSplit = NULL;
    }
    else         nCount = 1;

    //キー配列を取得　最大で16キー
    for (i = 0; i <= 15; i++) {
        Text::Awk(szKeyMacro, strKeyCode, _countof(strKeyCode), i + 1, _T(','));
        if (_tcsclen(strKeyCode) > 0) nKey[i] = getKeycode(strKeyCode, true);
        else break;
    }
    nKeyCount = i - 1;//キーの個数

    //キーを押す
    for (; nCount > 0; nCount--) {
        for (i = 0; i <= nKeyCount; i++) {
            keybd_event(nKey[i], 0, KEYEVENTF_EXTENDEDKEY, 0);//押す
        }
    }
    for (i = 0; i <= nKeyCount; i++) {
        keybd_event(nKey[i], 0, KEYEVENTF_EXTENDEDKEY | KEYEVENTF_KEYUP, 0);//キーを離す
    }
    Sleep(nSleep);
}

//---------------------------------------------------
//関数名	execData()
//機能		キーボードマクロを実行しながらデータを貼り付ける
//---------------------------------------------------
void CCharu3App::execData(CString strPaste, COPYPASTE_KEY key, HTREEITEM hTargetItem, HWND hWnd)
{
    HTREEITEM hMacroItem;
    CString strCut, strKeyMacro;
    int nStart = 0;
    int nFoundStart = -1;
    int nFoundEnd;
    int nMacroLenS;
    int nMacroLenE;

    //ビフォアキー処理
    if (hTargetItem)	hMacroItem = m_pTree->searchParentOption(hTargetItem, _T("beforkey"));
    else			hMacroItem = NULL;
    if (hMacroItem) {
        STRING_DATA* macroDataPtr = m_pTree->getDataPtr(hMacroItem);
        strKeyMacro = m_pTree->getDataOptionStr(macroDataPtr->m_strMacro, _T("beforkey"));
        if (strKeyMacro != "") execKeyMacro(strKeyMacro);
    }

    //マクロを置換
    strPaste.Replace(_T("<charu2MACRO_KEY>"), MACRO_START_KEY);
    strPaste.Replace(_T("</charu2MACRO_KEY>"), MACRO_END_KEY);

    nMacroLenS = _tcsclen(MACRO_START_KEY);
    nMacroLenE = _tcsclen(MACRO_END_KEY);

    do {
        nFoundStart = strPaste.Find(MACRO_START_KEY, nStart);
        strKeyMacro = "";
        //キーマクロを含む
        if (nFoundStart >= 0) {
            nFoundEnd = strPaste.Find(MACRO_END_KEY, nFoundStart);
            strCut = strPaste.Mid(nStart, nFoundStart - nStart);
            if (nFoundEnd > nFoundStart) {
                nStart = nFoundEnd + nMacroLenE;
                strKeyMacro = strPaste.Mid(nFoundStart + nMacroLenS, nFoundEnd - nFoundStart - nMacroLenS);
            }
            else	nStart = nFoundStart + nMacroLenS;
        }
        else 	strCut = strPaste.Mid(nStart, strPaste.GetLength() - nStart);
        //個別に貼り付け処理
        if (strCut != _T("")) {
            //			pasteData(strCut,key,hWnd);//貼り付け
            //			m_strlClipBackup = strCut;

            TCHAR strWindowName[1024] = {};
            *strWindowName = (char)NULL;
            CString strWinName;
            int i;
            COPYPASTE_KEY keySet;

            GetWindowText(m_focusInfo.m_hActiveWnd, strWindowName, _countof(strWindowName));
            strWinName = strWindowName;

            keySet.m_nMessage = 0;
            for (i = 0; keySet.m_nMessage > -1; i++) {
                keySet = m_ini.getAppendKeyInit(strWinName, i);
                if (keySet.m_nMessage <= -1 && i == 0)	keySet = key;
                if (keySet.m_nMessage > -1) {
                    pasteData(strCut, keySet, hWnd);//貼り付け
                }
            }
            m_strSavedClipboard = strCut;
        }
        //キーマクロを実行
        if (strKeyMacro != _T("")) {
            execKeyMacro(strKeyMacro);
        }
    } while (nFoundStart >= 0);

    //アフターキー処理
    if (hTargetItem)	hMacroItem = m_pTree->searchParentOption(hTargetItem, _T("afterkey"));
    else			hMacroItem = NULL;
    if (hMacroItem) {
        STRING_DATA* macroDataPtr = m_pTree->getDataPtr(hMacroItem);
        strKeyMacro = m_pTree->getDataOptionStr(macroDataPtr->m_strMacro, _T("afterkey"));
        if (strKeyMacro != _T("")) execKeyMacro(strKeyMacro);
    }
}

//---------------------------------------------------
//関数名	pasteData()
//機能		データを貼り付ける
//---------------------------------------------------
void CCharu3App::pasteData(CString strPaste, COPYPASTE_KEY key, HWND hWnd)
{
    if (m_isStockMode) {
        m_strSavedClipboard = strPaste;
    }
    SetClipboardText(strPaste);

    if (key.m_nMessage == 0) {//イベント方式
        keyUpDown(key.m_uMod_Paste, key.m_uVK_Paste, KEY_DOWN);
        keyUpDown(key.m_uMod_Paste, key.m_uVK_Paste, KEY_UP);
        Sleep(key.m_nPasteWait);
    }
    else if (key.m_nMessage == 1) {//Charu2Pro方式
        keyUpDownC2(key.m_uMod_Paste, key.m_uVK_Paste, KEY_DOWN);
        keyUpDownC2(key.m_uMod_Paste, key.m_uVK_Paste, KEY_UP);
        Sleep(key.m_nPasteWait);
    }
    else if (key.m_nMessage == 2) {
        ::PostMessage(hWnd, key.m_pasteMessage.Msg, key.m_pasteMessage.wParam, key.m_pasteMessage.lParam);//メッセージ方式
//		::PostMessage(hWnd,WM_PASTE,NULL,NULL);//メッセージ方式
    }

    if (m_ini.m_bDebug) {
        LOG(_T("pasteData %d %s %x %x active:%p focus:%p"),
            key.m_nMessage,
            strPaste.GetString(),
            key.m_uMod_Paste,
            key.m_uVK_Paste,
            m_focusInfo.m_hActiveWnd,
            m_focusInfo.m_hFocusWnd);
    }
}

//---------------------------------------------------
//関数名	keyUpDown()
//機能		仮想キーを押す
//---------------------------------------------------
void CCharu3App::keyUpDown(UINT uMod, UINT uVKCode, int nFlag)
{
    if (nFlag & KEY_DOWN) {
        if (uMod & MOD_ALT)
            keybd_event(VK_MENU, (BYTE)MapVirtualKey(VK_MENU, 0), KEYEVENTF_EXTENDEDKEY, 0);
        if (uMod & MOD_CONTROL)
            keybd_event(VK_CONTROL, (BYTE)MapVirtualKey(VK_CONTROL, 0), KEYEVENTF_EXTENDEDKEY, 0);
        if (uMod & MOD_SHIFT)
            keybd_event(VK_SHIFT, (BYTE)MapVirtualKey(VK_SHIFT, 0), KEYEVENTF_EXTENDEDKEY, 0);

        keybd_event(uVKCode, 0, KEYEVENTF_EXTENDEDKEY, 0);
    }
    if (nFlag & KEY_UP) {
        keybd_event(uVKCode, 0, KEYEVENTF_EXTENDEDKEY | KEYEVENTF_KEYUP, 0);//キーを離す

        if (uMod & MOD_ALT)
            keybd_event(VK_MENU, (BYTE)MapVirtualKey(VK_MENU, 0), KEYEVENTF_EXTENDEDKEY | KEYEVENTF_KEYUP, 0);
        if (uMod & MOD_CONTROL)
            keybd_event(VK_CONTROL, (BYTE)MapVirtualKey(VK_CONTROL, 0), KEYEVENTF_EXTENDEDKEY | KEYEVENTF_KEYUP, 0);
        if (uMod & MOD_SHIFT)
            keybd_event(VK_SHIFT, (BYTE)MapVirtualKey(VK_SHIFT, 0), KEYEVENTF_EXTENDEDKEY | KEYEVENTF_KEYUP, 0);
    }
}

//---------------------------------------------------
//関数名	keyUpDownC2()
//機能		仮想キーを押す
//---------------------------------------------------
void CCharu3App::keyUpDownC2(UINT uMod, UINT uVKCode, int nFlag)
{
    if (nFlag & KEY_DOWN) {
        if (uMod & MOD_ALT)		keybd_event(VK_MENU, 0, NULL, 0);
        if (uMod & MOD_CONTROL)	keybd_event(VK_CONTROL, 0, NULL, 0);
        if (uMod & MOD_SHIFT)	keybd_event(VK_SHIFT, 0, NULL, 0);

        keybd_event(uVKCode, 0, NULL, 0);
    }
    else if (nFlag & KEY_UP) {
        keybd_event(uVKCode, 0, KEYEVENTF_KEYUP, 0);//キーを離す

        if (uMod & MOD_ALT)		keybd_event(VK_MENU, 0, KEYEVENTF_KEYUP, 0);
        if (uMod & MOD_CONTROL)	keybd_event(VK_CONTROL, 0, KEYEVENTF_KEYUP, 0);
        if (uMod & MOD_SHIFT)	keybd_event(VK_SHIFT, 0, KEYEVENTF_KEYUP, 0);
    }
}

//---------------------------------------------------
//関数名	keyUpDownMessage()
//機能		仮想キーを押す
//---------------------------------------------------
void CCharu3App::keyUpDownMessage(UINT uMod, UINT uVKCode, int nFlag, HWND hWnd)
{
    if (nFlag & KEY_DOWN) {
        if (uMod & MOD_ALT)		::SendMessage(hWnd, WM_KEYDOWN, VK_MENU, 1);
        if (uMod & MOD_CONTROL)	::SendMessage(hWnd, WM_KEYDOWN, VK_CONTROL, 1);
        if (uMod & MOD_SHIFT)	::SendMessage(hWnd, WM_KEYDOWN, VK_SHIFT, 1);

        ::SendMessage(hWnd, WM_KEYDOWN, uVKCode, 1);
    }
    if (nFlag & KEY_UP) {
        ::SendMessage(hWnd, WM_KEYUP, uVKCode, 1);

        if (uMod & MOD_ALT)		::SendMessage(hWnd, WM_KEYUP, VK_MENU, 1);
        if (uMod & MOD_CONTROL)	::SendMessage(hWnd, WM_KEYUP, VK_CONTROL, 1);
        if (uMod & MOD_SHIFT)	::SendMessage(hWnd, WM_KEYUP, VK_SHIFT, 1);
    }
}
//---------------------------------------------------
//関数名	convertMacro(CString strSourceData)
//機能		マクロ文字列を置換
//---------------------------------------------------
CString CCharu3App::convertMacro(const STRING_DATA* sourceDataPtr, CString strSelect, CString strClip, CString strSoftName)
{
    SetCurrentDirectory(m_ini.m_strAppPath);

    if (strSoftName == _T("no")) return sourceDataPtr->m_strData;

    CString strSourceData;
    if (strSoftName != DAT_FORMAT) {
        if (m_pTree->convertMacroPlugin(sourceDataPtr, &strSourceData, strSelect, strClip, strSoftName))
            return strSourceData;
    }

    int nStart, nEnd, nMove, nTagStart, nTagEnd;

    CString strTag, strTagEnd, strBuff, strBuff2, strTime;
    CString strShellStart = _T("$SHELL<"), strShellEnd = _T(">$SHELL");
    CString strRelateStart = _T("$RELATE<"), strRelateEnd = _T(">$RELATE");
    CString strPlugStart = _T("$PLUG-IN<"), strPlugEnd = _T(">$PLUG-IN");

    //マクロ誤処理対策
    TCHAR strMark[2] = {};
    strMark[0] = 0x7f;
    strMark[1] = NULL;
    bool isSelect = false, isClip = false;

    CString strRepBuff1, strRepBuff2, strRepBuff3, strRepBuff4, strRepBuff5;
    strRepBuff1 = _T("<charuMACR@c3mO@>");
    strRepBuff2 = _T("</charuMACR@c3mO@>");
    strRepBuff3 = _T("@c3m$@PLUG-IN<");
    strRepBuff4 = _T(">@c3m$@PLUG-IN");
    strRepBuff5 = _T("@c3m$@CLIP");

    if (sourceDataPtr->m_strData.Find(_T("$SEL")) != -1) {
        strSelect.Replace(MACRO_START, strRepBuff1);
        strSelect.Replace(MACRO_END, strRepBuff2);
        strSelect.Replace(strPlugStart, strRepBuff3);
        strSelect.Replace(strPlugEnd, strRepBuff4);
        strSelect.Replace(_T("$CLIP"), strRepBuff5);
        isSelect = true;
    }
    if (sourceDataPtr->m_strData.Find(_T("$CLIP")) != -1) {
        strClip.Replace(MACRO_START, strRepBuff1);
        strClip.Replace(MACRO_END, strRepBuff2);
        strClip.Replace(strPlugStart, strRepBuff3);
        strClip.Replace(strPlugEnd, strRepBuff4);
        isClip = true;
    }

    CTime cTime = CTime::GetCurrentTime();
    strSourceData = sourceDataPtr->m_strData;

    strSourceData.Replace(_T("<charu2MACRO>"), MACRO_START);
    strSourceData.Replace(_T("</charu2MACRO>"), MACRO_END);
    strTag = MACRO_START;
    strTagEnd = MACRO_END;
    nEnd = 0;
    while (1) {
        nStart = strSourceData.Find(strTag, nEnd);//タグの先頭を取得
        if (nStart == -1) break;
        nEnd = strSourceData.Find(strTagEnd, nStart);//タグの終わりを取得
        if (nEnd == -1) nEnd = strSourceData.GetLength();
        if (nStart > nEnd) break;
        nEnd += strTagEnd.GetLength();

        strBuff = strSourceData.Mid(nStart, nEnd - nStart);//タグ部分切り出し
        strBuff2 = strBuff;
        strBuff2.Replace(strTag, _T(""));
        strBuff2.Replace(strTagEnd, _T(""));

        if (strBuff2.Find(_T("$Y")) != -1) {//2000 西暦4桁
            strTime.Format(_T("%04d"), cTime.GetYear());
            strBuff2.Replace(_T("$Y"), strTime);
        }
        if (strBuff2.Find(_T("$y")) != -1) {//00 西暦2桁
            strTime.Format(_T("%02d"), cTime.GetYear() - 2000);
            strBuff2.Replace(_T("$y"), strTime);
        }

        if (strBuff2.Find(_T("$MM")) != -1) {//May 月英語表記
            CString strMonth[] = { _T("Jan"),_T("Feb"),_T("Mar"),_T("Apr"),_T("May"),_T("Jun"),_T("Jul"),_T("Aug"),_T("Sep"),_T("Oct"),_T("Nov"),_T("Dec") };
            strBuff2.Replace(_T("$MM"), strMonth[cTime.GetMonth() - 1]);
        }
        if (strBuff2.Find(_T("$M")) != -1) {//05 月2桁
            strTime.Format(_T("%02d"), cTime.GetMonth());
            strBuff2.Replace(_T("$M"), strTime);
        }

        if (strBuff2.Find(_T("$D")) != -1) {//06 日付数字2桁
            strTime.Format(_T("%02d"), cTime.GetDay());
            strBuff2.Replace(_T("$D"), strTime);
        }
        if (strBuff2.Find(_T("$ddd")) != -1) {//Saturday 曜日英語
            CString strYoubi[] = { _T("Sunday"),_T("Monday"),_T("Tuesday"),_T("Wednesday"),_T("Thursday"),_T("Friday"),_T("Saturday") };
            strBuff2.Replace(_T("$ddd"), strYoubi[cTime.GetDayOfWeek() - 1]);
        }
        if (strBuff2.Find(_T("$dd")) != -1) {//Sat 曜日英語簡易表記
            CString strYoubi[] = { _T("Sun"),_T("Mon"),_T("Tue"),_T("Wed"),_T("Thu"),_T("Fri"),_T("Sat") };
            strBuff2.Replace(_T("$dd"), strYoubi[cTime.GetDayOfWeek() - 1]);
        }
        if (strBuff2.Find(_T("$d")) != -1) {//土曜 曜日
            CString strYoubi[] = { _T("日"),_T("月"),_T("火"),_T("水"),_T("木"),_T("金"),_T("土") };
            strBuff2.Replace(_T("$d"), strYoubi[cTime.GetDayOfWeek() - 1]);
        }

        if (strBuff2.Find(_T("$H")) != -1) {//21 時間24時間表記
            strTime.Format(_T("%02d"), cTime.GetHour());
            strBuff2.Replace(_T("$H"), strTime);
        }
        if (strBuff2.Find(_T("$h")) != -1) {//09 時間
            int nHours = cTime.GetHour();
            if (nHours > 11) nHours -= 12;
            strTime.Format(_T("%02d"), nHours);
            strBuff2.Replace(_T("$h"), strTime);
        }
        if (strBuff2.Find(_T("$m")) != -1) {//46 分
            strTime.Format(_T("%02d"), cTime.GetMinute());
            strBuff2.Replace(_T("$m"), strTime);
        }
        if (strBuff2.Find(_T("$s")) != -1) {//02 秒
            strTime.Format(_T("%02d"), cTime.GetSecond());
            strBuff2.Replace(_T("$s"), strTime);
        }

        if (strBuff2.Find(_T("$ampm")) != -1) {//午後
            int nHours = cTime.GetHour();
            if (nHours > 11) strTime = _T("午後");
            else			strTime = _T("午前");
            strBuff2.Replace(_T("$ampm"), strTime);
        }
        if (strBuff2.Find(_T("$AMPM")) != -1) {//PM
            int nHours = cTime.GetHour();
            if (nHours > 11) strTime = _T("PM");
            else			strTime = _T("AM");
            strBuff2.Replace(_T("$AMPM"), strTime);
        }
        if (strBuff2.Find(_T("$SEL")) != -1) {//選択部分
            strBuff2.Replace(_T("$SEL"), strSelect);
        }
        if (strBuff2.Find(_T("$CLIP")) != -1) {//選択部分
            strBuff2.Replace(_T("$CLIP"), strClip);
        }
        if (strBuff2.Find(strPlugStart) != -1) {//プラグイン
            int nPlugLength = strPlugStart.GetLength();
            int nPlugEndLength = strPlugEnd.GetLength();

            nTagStart = strBuff2.Find(strPlugStart);
            nTagEnd = strBuff2.Find(strPlugEnd);

            if (nTagEnd < nTagStart) {
                strBuff2.Delete(nTagStart, nPlugLength);
            }
            else {

                HMODULE hDLL;
                CharuPlugIn pPlugIn;

                while (strBuff2.Find(strPlugStart) != -1) {
                    int nPlugStart = strBuff2.Find(strPlugStart) + nPlugLength;//開始位置取得
                    int nPlugEnd = strBuff2.Find(strPlugEnd, nPlugStart);//終わり位置取得
                    if (nPlugEnd == -1) nPlugEnd = strBuff2.GetLength();

                    CString strPlug, strDllName, strTmp;
                    TCHAR* strRet;
                    strPlug = strBuff2.Mid(nPlugStart, nPlugEnd - nPlugStart);
                    int nComma = strPlug.Find(_T(","), 0);
                    if (nComma >= 0) {
                        strDllName = strPlug.Left(nComma);
                        strTmp = strPlug.Right(strPlug.GetLength() - nComma - 1);
                    }
                    else {
                        strDllName = _T("");
                        strTmp = strPlug;
                    }

                    //DLLをロード
                    hDLL = LoadLibrary(strDllName);
                    if (hDLL) {
                        pPlugIn = (CharuPlugIn)GetProcAddress(hDLL, "CharuPlugIn");
                        if (pPlugIn) {
                            int nSize = strTmp.GetLength() * 5;
                            if (nSize < 10240) nSize = 10240;
                            strRet = new TCHAR[nSize];
                            if (strRet) {
                                CString strPluginRet;
                                int nTmpLen = strTmp.GetLength() + 1;
                                if (strTmp.Find(_T("@c3m")) >= 0) {
                                    strTmp.Replace(_T("@c3mO@"), _T("O"));
                                    strTmp.Replace(_T("@c3m$@"), _T("$"));
                                }
                                //								int nBuffSize = nSize * sizeof(TCHAR);

                                pPlugIn(strTmp.GetBuffer(strTmp.GetLength() + 1), strRet, nSize, sourceDataPtr, (void*)&m_focusInfo);
                                strPluginRet = strRet;
                                strPluginRet.Replace(MACRO_START, strRepBuff1);
                                strPluginRet.Replace(MACRO_END, strRepBuff2);
                                strPluginRet.Replace(strPlugStart, strRepBuff3);
                                strPluginRet.Replace(strPlugEnd, strRepBuff4);

                                strBuff2.Delete(nPlugStart, strDllName.GetLength() + nTmpLen);
                                strBuff2.Insert(nPlugStart, strPluginRet);//置換
                                delete[]strRet;
                            }
                        }
                        else {
                            CString strRes;
                            (void)strRes.LoadString(APP_MES_CANT_GET_FUNCTION_ADDRESS);
                            AfxMessageBox(strRes, MB_ICONEXCLAMATION | MB_SYSTEMMODAL, 0);
                        }
                        FreeLibrary(hDLL);
                    }
                    else {
                        CString strMessage, strRes;
                        (void)strRes.LoadString(APP_MES_NOT_FOUND);
                        strMessage.Format(strRes, strDllName);
                        AfxMessageBox(strMessage, MB_ICONEXCLAMATION | MB_SYSTEMMODAL, 0);
                    }
                    strBuff2.Delete(strBuff2.Find(strPlugStart), nPlugLength);
                    strBuff2.Delete(strBuff2.Find(strPlugEnd), nPlugEndLength);
                }
            }
        }
        if (strBuff2.Find(strRelateStart) != -1) {//関連付け
            int nRelateLength = strRelateStart.GetLength();
            nTagStart = strBuff2.Find(strRelateStart);
            nTagEnd = strBuff2.Find(strRelateEnd);

            if (nTagEnd < nTagStart) {
                strBuff2.Delete(nTagStart, nRelateLength);
            }
            else {
                while (strBuff2.Find(strRelateStart) != -1) {
                    int nMacroStart = strBuff2.Find(strRelateStart);
                    int nRelateStart = nMacroStart + nRelateLength;//開始位置取得
                    int nRelateEnd = strBuff2.Find(strRelateEnd, nRelateStart);//終わり位置取得
                    if (nRelateEnd == -1) nRelateEnd = strBuff2.GetLength();

                    CString strRelate;
                    strRelate = strBuff2.Mid(nRelateStart, nRelateEnd - nRelateStart);//コマンドライン切り出し

                    ShellExecute(NULL, NULL, strRelate, NULL, NULL, SW_SHOWNORMAL);
                    strBuff2.Delete(nMacroStart, nRelateLength + nRelateEnd - nMacroStart);//タグを削除
                }
            }
        }
        if (strBuff2.Find(strShellStart) != -1) {//シェル
            int nShellLength = strShellStart.GetLength();
            int nRelateLength = strRelateStart.GetLength();

            nTagStart = strBuff2.Find(strShellStart);
            nTagEnd = strBuff2.Find(strShellEnd);

            if (nTagEnd < nTagStart) {
                strBuff2.Delete(nTagStart, nShellLength);
            }
            else {
                while (strBuff2.Find(strShellStart) >= 0) {
                    int macroStartIndex = strBuff2.Find(strShellStart);
                    int commandStartIndex = macroStartIndex + nShellLength;
                    int commandEndIndex = strBuff2.Find(strShellEnd, commandStartIndex);
                    if (commandEndIndex < 0) commandEndIndex = strBuff2.GetLength();
                    CString strCommand = strBuff2.Mid(commandStartIndex, commandEndIndex - commandStartIndex);

                    STARTUPINFO si;
                    PROCESS_INFORMATION pi;
                    ZeroMemory(&si, sizeof(si));
                    si.cb = sizeof(si);
                    ZeroMemory(&pi, sizeof(pi));
                    CreateProcess(NULL, strCommand.GetBuffer(strCommand.GetLength()), NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi);
                    WaitForInputIdle(pi.hProcess, INFINITE);
                    CloseHandle(pi.hProcess);
                    CloseHandle(pi.hThread);

                    strBuff2.Delete(macroStartIndex, nShellLength + commandEndIndex - macroStartIndex);//タグを削除
                }
            }
        }

        nMove = strBuff2.GetLength();// - strBuff.GetLength();
        strSourceData.Delete(nStart, strBuff.GetLength());
        strSourceData.Insert(nStart, strBuff2);
        nEnd = nStart + nMove;
    }
    if (strSourceData.Find(_T("@c3m")) >= 0) {
        strSourceData.Replace(_T("@c3mO@"), _T("O"));
        strSourceData.Replace(_T("@c3m$@"), _T("$"));
    }
    /*	strSourceData.Replace(strRepBuff2,MACRO_END);
        strSourceData.Replace(strRepBuff3,strPlugStart);
        strSourceData.Replace(strRepBuff4,strPlugEnd);*/

    return strSourceData;
}

void CCharu3App::Record(CString text)
{
    if (m_ini.m_bDebug) {
        LOG(_T("Record \"%s\""), text.GetString());
    }

    //連続で空のクリップボード更新イベントが起こるので対策
    //2007/10/27-20:20:19-------------------
    static int nEmptyCnt = 0;
    if (text == "") {
        /*		if(nEmptyCnt > 5) {
                    Sleep(3000);
                    nEmptyCnt = 0;
                    //デバッグログ処理
                    if(m_ini.m_nDebug) {
                        LOG(_T("onClipBoardChanged Sleep \"%s\""), strClipBoard);
                    }
                }
                else {
                    nEmptyCnt++;
                }
        */		return;
    }
    nEmptyCnt = 0;

    // Check size
    int nLimit = m_ini.m_nHistoryLimit;
    const HWND hActiveWindow = ::GetForegroundWindow();
    CString strTitle = Window::GetWindowTitle(hActiveWindow);
    if (strTitle != "") {
        CHANGE_KEY key;
        key = m_ini.getAppendKeyInit2(strTitle);
        if (key.m_nMatch >= 0) nLimit = key.m_nHistoryLimit;
    }
    nLimit *= 1024;
    if (nLimit >= 0 && text.GetLength() > nLimit) {
        return;
    }

    if (m_nPhase == PHASE_IDOL && /*strClipBoard != m_strlClipBackup &&*/ text != "") {
        STRING_DATA data;
        data.m_cKind = KIND_ONETIME;
        data.m_strData = text;
        data.m_cIcon = KIND_DEFAULT;

        m_nPhase = PHASE_LOCK;
        KillTimer(m_pMainWnd->m_hWnd, TIMER_ACTIVE);

        if (m_isStockMode) {
            if (!(m_ini.m_bDontSaveSameDataAsLast && text == m_strPreviousStocked)) {
                if (m_ini.m_strCopySound != _T("")) {
                    PlaySound(m_ini.m_strCopySound, NULL, SND_ASYNC | SND_FILENAME);
                }
                m_pTree->AddData(NULL, data);
                m_strPreviousStocked = text;
            }
        }

        m_pTree->AddDataToHistoryFolders(data);

        if (m_isStockMode && m_ini.m_nWindowCheckInterval > 0) {
            SetTimer(m_pMainWnd->m_hWnd, TIMER_ACTIVE, m_ini.m_nWindowCheckInterval, NULL);
        }

        m_nPhase = PHASE_IDOL;
    }

    m_strSavedClipboard = text;
}

//---------------------------------------------------
//関数名	fifoClipboard()
//機能		ストックモード処理
//---------------------------------------------------
void CCharu3App::PullOneTimeData()
{
    if (!m_isStockMode) {
        return;
    }

    if (m_ini.m_nFifo) {
        CString text(_T(""));
        HTREEITEM hItem = m_pTree->getOneTimeItem(m_ini.m_nFifo);
        if (hItem) {
            text = m_pTree->getDataPtr(hItem)->m_strData;
            m_pTree->DeleteData(hItem);
        }
        if (text != _T("")) {
            if (m_ini.m_bDebug) {
                LOG(_T("fifoClipboard text:%s"), text.GetString());
            }
            if (SetClipboardText(text)) {
                if (m_ini.m_strPasteSound != _T("")) {
                    PlaySound(m_ini.m_strPasteSound, NULL, SND_ASYNC | SND_FILENAME);
                }
                m_strSavedClipboard = text;
            }
        }
    }

    /* It comes in here by pressing the paste key, which is registered as a
       hotkey. So this key combination should be currently held down.
       Thus, if we leave the modifier key as it isand send events that release
       the main keyand press it again, the target window will recognize that
       the paste key is pressed. */
    UnregisterHotKey(NULL, HOTKEY_PASTE);
    keybd_event(m_keySet.m_uVK_Paste, 0, KEYEVENTF_KEYUP, 0);
    keybd_event(m_keySet.m_uVK_Paste, 0, 0, 0);
    RegisterHotKey(NULL, HOTKEY_PASTE, m_keySet.m_uMod_Paste, m_keySet.m_uVK_Paste);

    if (m_ini.m_bAutoOff && !m_pTree->getOneTimeItem(m_ini.m_nFifo)) {
        ToggleStockMode(); // Turn off stock mode due to one-time item is gone
    }
}

//---------------------------------------------------
//関数名	toggleStockMode()
//機能		ストックモード切替
//---------------------------------------------------
void CCharu3App::ToggleStockMode()
{
    if (m_ini.m_bDebug) {
        LOG(_T("toggleStockMode %d"), !m_isStockMode);
    }

    m_isStockMode = !m_isStockMode;
    m_pMainFrame->SwitchNotificationAreaIcon(m_isStockMode);
    if (m_isStockMode) {
        m_strSavedClipboard = _T("");
        setAppendKeyInit(::GetForegroundWindow(), &m_keySet);
        RegisterHotKey(NULL, HOTKEY_PASTE, theApp.m_keySet.m_uMod_Paste, theApp.m_keySet.m_uVK_Paste);//ペーストキー
        if (m_ini.m_nWindowCheckInterval > 0) {
            SetTimer(m_pMainWnd->m_hWnd, TIMER_ACTIVE, m_ini.m_nWindowCheckInterval, NULL);
        }
    }
    else {
        KillTimer(m_pMainWnd->m_hWnd, TIMER_ACTIVE);
        UnregisterHotKey(NULL, HOTKEY_PASTE);
        if (m_ini.m_bCleanupAtTurnOff) {
            m_pTree->cleanupOneTimeItems(m_pTree->GetRootItem());
        }
    }
}

//---------------------------------------------------
//関数名	getPopupPos(POINT *pPos,int nPodType,HWND hTarget)
//機能		設定に応じたポップアップ位置を取得
//---------------------------------------------------
void CCharu3App::getPopupPos(POINT* pPos, int nPosType)
{
    RECT rectDeskTop;

    //ターゲットのハンドルを取るために実行
    Window::GetCaretPos(pPos, &m_focusInfo);//キャレット位置を取得

    //デスクトップのサイズを取得
    if (nPosType > POPUP_POS_MOUSE) {
        HWND hDeskTop = GetDesktopWindow();
        ::GetWindowRect(hDeskTop, &rectDeskTop);
    }
    switch (nPosType) {
    case POPUP_POS_MOUSE:
        GetCursorPos(pPos);
        break;
    case POPUP_POS_LEFT_U:
        pPos->x = 0;
        pPos->y = 0;
        break;
    case POPUP_POS_RIGHT_U:
        pPos->x = rectDeskTop.right - m_ini.m_treeviewSize.x;
        pPos->y = 0;
        break;
    case POPUP_POS_CENTOR:
        pPos->x = rectDeskTop.right / 2 - m_ini.m_treeviewSize.x / 2;
        pPos->y = rectDeskTop.bottom / 2 - m_ini.m_treeviewSize.y / 2;
        break;
    case POPUP_POS_LEFT_D:
        pPos->x = 0;
        pPos->y = rectDeskTop.bottom - m_ini.m_treeviewSize.y;
        break;
    case POPUP_POS_RIGHT_D:
        pPos->x = rectDeskTop.right - m_ini.m_treeviewSize.x;
        pPos->y = rectDeskTop.bottom - m_ini.m_treeviewSize.y;
        break;
    case POPUP_POS_CARET:
        pPos->x += m_ini.m_posCaretHosei.x;
        pPos->y += m_ini.m_posCaretHosei.y;
        break;
    }
}

//---------------------------------------------------
//関数名	resetTreeDialog()
//機能		ツリーダイアログを再生成
//---------------------------------------------------
void  CCharu3App::resetTreeDialog()
{
    std::list<STRING_DATA> stringList;//データリスト

    //データの順番を正規化してコピー
    HTREEITEM hStartItem = m_pTree->GetRootItem();
    m_pTree->tree2List(hStartItem, &stringList, true);

    if (m_pTree) {
        delete m_pTree;
    }
    if (m_pTreeDlg) {
        delete m_pTreeDlg;
    }
    m_pTreeDlg = new CMyTreeDialog;
    m_pTree = new CCharu3Tree;
    if (m_pTree) {
        m_pTreeDlg->SetTree(m_pTree);
        m_pTreeDlg->Create(IDD_DATA_TREE_VIEW, this->m_pMainWnd);
        m_pTree->setImageList(m_ini.m_IconSize, theApp.m_ini.m_strResourceName, m_ini.m_strAppPath);
        m_pTree->setInitInfo(&m_ini.m_nTreeID, &m_ini.m_nSelectID, &m_ini.m_nRecNumber);//ID初期値を設定
        m_pTree->setPlugin(m_ini.m_strRwPluginFolder);
        m_pTree->m_MyStringList = stringList;
        m_pTree->CWnd::LockWindowUpdate();
        m_pTree->copyData(dataTree::ROOT, TVI_ROOT, &m_pTree->m_MyStringList);//ツリーにデータをセット
        m_pTree->CWnd::UnlockWindowUpdate();
        RegisterAdditionalHotkeys();
    }
}

//---------------------------------------------------
// CCharu3App メッセージ ハンドラ
//---------------------------------------------------

//---------------------------------------------------
//関数名	PreTranslateMessage(MSG* pMsg)
//機能		メッセージ前処理
//---------------------------------------------------
BOOL CCharu3App::PreTranslateMessage(MSG* pMsg)
{
    //キーフック処理
    if (pMsg->message == WM_KEY_HOOK) {
        if (m_ini.m_bDebug) {
            LOG(_T("WM_KEY_HOOK %d %d"), pMsg->wParam, pMsg->lParam);
        }
        if (m_ini.m_nDoubleKeyPOP) {
            UINT key = 0;
            switch (m_ini.m_nDoubleKeyPOP) {
            case 1: key = VK_SHIFT; break;
            case 2: key = VK_CONTROL; break;
            case 3: key = VK_MENU; break;
            }
            if (key == pMsg->wParam) {
                pMsg->message = WM_HOTKEY;
                pMsg->wParam = HOTKEY_POPUP;
            }
        }
        if (m_ini.m_nDoubleKeyFIFO) {
            UINT key = 0;
            switch (m_ini.m_nDoubleKeyFIFO) {
            case 1: key = VK_SHIFT; break;
            case 2: key = VK_CONTROL; break;
            case 3: key = VK_MENU; break;
            }
            if (key == pMsg->wParam) {
                pMsg->message = WM_HOTKEY;
                pMsg->wParam = HOTKEY_FIFO;
            }
        }
        //フォルダホットキー処理
        int nSize = m_hotkeyVector.size();
        for (int i = 0; i < nSize; i++) {
            if (m_hotkeyVector[i].m_bDoubleStroke) {
                if (KeyHelper::ModToVK(m_hotkeyVector[i].m_uModKey) == pMsg->wParam) {
                    pMsg->message = WM_HOTKEY;
                    pMsg->wParam = HOT_ITEM_KEY + i;
                    break;
                }
            }
        }
    }

    if (m_nPhase == PHASE_IDOL && pMsg->message == WM_TIMER) {
        // 監視タイマー処理（ウィンドウが切り替わっていたらストックモードのペーストキーを切り替える）
        if (pMsg->wParam == TIMER_ACTIVE && m_isStockMode) {
            if (setAppendKeyInit(::GetForegroundWindow(), &m_keySet)) {
                UnregisterHotKey(NULL, HOTKEY_PASTE);
                RegisterHotKey(NULL, HOTKEY_PASTE, theApp.m_keySet.m_uMod_Paste, theApp.m_keySet.m_uVK_Paste);
            }
        }
    }
    //ホットキー処理
    if (pMsg->message == WM_HOTKEY) {
        switch (pMsg->wParam) {
        case HOTKEY_POPUP://ポップアップ
            if (m_ini.m_bDebug) {
                LOG(_T("HOTKEY_POPUP"));
            }
            if (m_ini.m_nDoubleKeyPOP) {//ダブルキークリック
                DWORD dwTime = timeGetTime();
                if (static_cast<int>(dwTime - m_dwDoubleKeyPopTime) > m_ini.m_nDCKeyPopTime) {
                    m_dwDoubleKeyPopTime = dwTime;
                    return FALSE;
                }
            }
            if (m_nPhase == PHASE_IDOL) {
                POINT pos;

                getPopupPos(&pos, m_ini.m_nPopupPos);//ポップアップ位置を取得
                if (m_ini.m_bDebug) {
                    LOG(_T("getPopupPos %d %d"), pos.x, pos.y);
                }

                adjustLocation(&pos);
                if (m_ini.m_bDebug) {
                    LOG(_T("reviseWindowPos %d %d"), pos.x, pos.y);
                }
                popupTreeWindow(pos, m_ini.m_bKeepSelection);
            }
            else	m_isCloseKey = true;//閉じるスイッチを入れておく
            return FALSE;
            break;

        case HOTKEY_FIFO://ストックモード切り替え
            if (m_ini.m_bDebug) {
                LOG(_T("HOTKEY_FIFO"));
            }

            if (m_ini.m_nDoubleKeyFIFO) {//ダブルキークリック
                DWORD dwTime = timeGetTime();
                if (static_cast<int>(dwTime - m_dwDoubleKeyFifoTime) > m_ini.m_nDCKeyFifoTime) {
                    m_dwDoubleKeyFifoTime = dwTime;
                    return FALSE;
                }
            }
            ToggleStockMode(); // Toggle stock mode by hotkey
            return FALSE;
            break;

        case HOTKEY_PASTE://履歴FIFO処理
            if (m_ini.m_bDebug) {
                LOG(_T("HOTKEY_PASTE"));
            }
            PullOneTimeData();
            return FALSE;
            break;

        case 99:
            break;

            //ホットアイテム
        default:
            if (m_nPhase == PHASE_IDOL) {
                int nTarget = pMsg->wParam - HOT_ITEM_KEY;
                playHotItem(nTarget);
                return FALSE;
            }
            break;
        }
    }
    //閉じるスイッチが入ってて、キーアップの時にポップアップを閉じる
    //(ALTの時に対象アプリのメニューが開いちゃうのを防ぐ)
    else if ((pMsg->message == WM_KEYUP || pMsg->message == WM_SYSKEYUP) && m_isCloseKey) {
        m_pTreeDlg->PostMessage(WM_TREE_CLOSE, IDCANCEL, NULL);
        m_isCloseKey = false;
    }
    //ポップアップから閉じる命令が来たので閉じる
    if (pMsg->message == WM_TREE_CLOSE && m_nPhase == PHASE_POPUP) {
        closeTreeWindow(pMsg->wParam);
        return FALSE;
    }
    return CWinApp::PreTranslateMessage(pMsg);
}

//---------------------------------------------------
//関数名	OnExit()
//機能		終了処理
//---------------------------------------------------
void CCharu3App::OnExit()
{
    UnregisterAdditionalHotkeys();
    UnregisterHotkeys();
    KillTimer(m_pMainWnd->m_hWnd, TIMER_ACTIVE);
    KillTimer(m_pMainWnd->m_hWnd, TIMER_MOUSE);
    if (m_hLangDll) {
        FreeLibrary(m_hLangDll);
    }
    m_ini.unHookKey();
    SaveData();
    if (m_ini.m_bDebug) {
        LOG(_T("OnExit"));
    }
}

//---------------------------------------------------
//関数名	OnAbout()
//機能		Aboutダイアログ
//---------------------------------------------------
void CCharu3App::OnAbout()
{
    CAboutDlg aboutDlg;
    aboutDlg.DoModal();
}

//---------------------------------------------------
void CCharu3App::OnIssues()
{
    ShellExecute(NULL, NULL, _T("https://github.com/itagagaki/charu3-SE/issues"), NULL, NULL, SW_SHOWNORMAL);
}

void CCharu3App::OnDiscussions()
{
    ShellExecute(NULL, NULL, _T("https://github.com/itagagaki/charu3-SE/discussions"), NULL, NULL, SW_SHOWNORMAL);
}

void CCharu3App::OnWiki()
{
    ShellExecute(NULL, NULL, _T("https://github.com/itagagaki/charu3-SE/wiki"), NULL, NULL, SW_SHOWNORMAL);
}

//---------------------------------------------------
//関数名	OnOption()
//機能		設定
//---------------------------------------------------
void CCharu3App::OnOption()
{
    if (m_nPhase != PHASE_IDOL && m_nPhase != PHASE_POPUP) return;
    int nPhase = m_nPhase;
    m_nPhase = PHASE_OPTION;

    //タイマー停止
    KillTimer(m_pMainWnd->m_hWnd, TIMER_ACTIVE);
    KillTimer(m_pMainWnd->m_hWnd, TIMER_MOUSE);

    if (nPhase == PHASE_IDOL)	m_ini.unHookKey();
    UnregisterAdditionalHotkeys();
    UnregisterHotkeys();

    if (m_ini.m_bDebug) {
        LOG(_T("OnOption"));
    }
    CRect rect;
    m_pTreeDlg->GetWindowRect(&rect);
    CInit iniBackup = m_ini;
    COptMainDialog dlgOption(NULL, m_ini.m_nOptionPage);
    if (dlgOption.DoModal() == IDOK) {
        m_ini.writeAllInitData();
        if (m_ini.m_strResourceName != iniBackup.m_strResourceName) {
            resetTreeDialog();
        }
        if (nPhase == PHASE_POPUP) {
            m_pTreeDlg->ShowWindow(false);
            m_pTreeDlg->ShowWindowPos(rect.TopLeft(), m_ini.m_treeviewSize, SW_SHOW, true);
        }
    }
    else {
        m_ini = iniBackup;
    }

    RegisterHotkeys();
    RegisterAdditionalHotkeys();
    if (nPhase == PHASE_IDOL)	m_ini.setHookKey(m_hSelfWnd);

    //監視タイマーセット アイドル状態でストックモード中なら
    if (m_nPhase == PHASE_IDOL && m_isStockMode && m_ini.m_nWindowCheckInterval > 0) {
        SetTimer(m_pMainWnd->m_hWnd, TIMER_ACTIVE, m_ini.m_nWindowCheckInterval, NULL);
    }

    m_hActiveKeyWnd = NULL;
    m_nPhase = nPhase;
}

//---------------------------------------------------
//関数名	OnChangData()
//機能		データファイル切り替え
//---------------------------------------------------
void CCharu3App::OnChangData()
{
    SelectFile();
}

//---------------------------------------------------
//関数名	OnExport()
//機能		エクスポート
//---------------------------------------------------
void CCharu3App::OnExport()
{
    CString file = CommonDialog::NewFilePath(_T(DAT_EXT));
    if (file != _T("")) {
        if (!m_pTree->saveDataToFile(file, DAT_FORMAT, NULL)) {
            CString strRes;
            (void)strRes.LoadString(APP_MES_EXPORT_FAILURE);
            AfxMessageBox(strRes, MB_ICONEXCLAMATION, 0);
        }
    }
}

//---------------------------------------------------
//関数名	OnAddData()
//機能		データ追加
//---------------------------------------------------
void CCharu3App::OnAddData()
{
    STRING_DATA data;
    CEditDialog editDialog(NULL, &data, true);
    int nRet = editDialog.DoModal();
    if (IDOK == nRet) {
        theApp.m_pTree->AddData(NULL, data);
    }
}

//---------------------------------------------------
//関数名	OnStockStop()
//機能		ストックモード切替
//---------------------------------------------------
void CCharu3App::OnStockStop()
{
    ToggleStockMode(); // Toggle stock mode by main menu
}

//---------------------------------------------------
//関数名	resetTreeDialog()
//機能		ツリー再構築
//---------------------------------------------------
void CCharu3App::OnResetTree()
{
    resetTreeDialog();//ツリー再構築
}
