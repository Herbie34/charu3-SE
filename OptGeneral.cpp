/*----------------------------------------------------------
    一般設定クラス
                                    2002/11/16 (c)Keizi
----------------------------------------------------------*/

#include "stdafx.h"
#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

#include "OptGeneral.h"
#include "Charu3.h" // for theApp.m_ini
#include "key.h"
#include "util.h"
#include "resource.h"

//---------------------------------------------------
//関数名	COptGereral
//機能		コンストラクタ
//---------------------------------------------------
COptGeneral::COptGeneral(CWnd* pParent /*=NULL*/) : CDialog(COptGeneral::IDD, pParent)
{
    //{{AFX_DATA_INIT(COptGeneral)
    //}}AFX_DATA_INIT
    m_nPutBackClipboard = Util::BoolToInt(theApp.m_ini.m_bPutBackClipboard);
    m_nShowClipboardInNotifyIconTooltip = Util::BoolToInt(theApp.m_ini.m_bShowClipboardInTooltipOfNofifyIcon);
}

//---------------------------------------------------
//関数名	DoDataExchange(CDataExchange* pDX)
//機能		データエクスチェンジ
//---------------------------------------------------
void COptGeneral::DoDataExchange(CDataExchange* pDX)
{
    CDialog::DoDataExchange(pDX);

    //{{AFX_DATA_MAP(COptGeneral)

    if (GetDlgItem(IDC_OPT_KEYRADIO_POP))
        DDX_Radio(pDX, IDC_OPT_KEYRADIO_POP, theApp.m_ini.m_nDoubleKeyPOP);
    if (GetDlgItem(IDC_OPT_POPUPKEY))
        DDX_Control(pDX, IDC_OPT_POPUPKEY, m_ctrlPopupKey);

    if (GetDlgItem(IDC_OPT_KEYRADIO_FIFO))
        DDX_Radio(pDX, IDC_OPT_KEYRADIO_FIFO, theApp.m_ini.m_nDoubleKeyFIFO);
    if (GetDlgItem(IDC_OPT_FIFO_KEY))
        DDX_Control(pDX, IDC_OPT_FIFO_KEY, m_ctrlFifoKey);

    if (GetDlgItem(IDC_OPT_ICON_POP))
        DDX_Radio(pDX, IDC_OPT_ICON_POP, theApp.m_ini.m_nIconClick);
    if (GetDlgItem(IDC_OPT_TOCLIP))
        DDX_Check(pDX, IDC_OPT_TOCLIP, m_nPutBackClipboard);
    if (GetDlgItem(IDC_OPT_TOOLTIP_TITLE))
        DDX_Check(pDX, IDC_OPT_TOOLTIP_TITLE, m_nShowClipboardInNotifyIconTooltip);
    //}}AFX_DATA_MAP
}

BEGIN_MESSAGE_MAP(COptGeneral, CDialog)
    //{{AFX_MSG_MAP(COptGeneral)
    ON_BN_CLICKED(IDC_OPT_KEYRADIO_POP, HotkeyEnablePOP)
    ON_BN_CLICKED(IDC_OPT_KEYRADIO_POP2, HotkeyDisablePOP)
    ON_BN_CLICKED(IDC_OPT_KEYRADIO_POP3, HotkeyDisablePOP)
    ON_BN_CLICKED(IDC_OPT_KEYRADIO_POP4, HotkeyDisablePOP)
    ON_BN_CLICKED(IDC_OPT_KEYRADIO_FIFO, HotkeyEnableFIFO)
    ON_BN_CLICKED(IDC_OPT_KEYRADIO_FIFO2, HotkeyDisableFIFO)
    ON_BN_CLICKED(IDC_OPT_KEYRADIO_FIFO3, HotkeyDisableFIFO)
    ON_BN_CLICKED(IDC_OPT_KEYRADIO_FIFO4, HotkeyDisableFIFO)
    ON_WM_SHOWWINDOW()
    //}}AFX_MSG_MAP
END_MESSAGE_MAP()

//---------------------------------------------------
// COptGeneral メッセージ ハンドラ
//---------------------------------------------------

//---------------------------------------------------
//関数名	PreTranslateMessage(MSG* pMsg)
//機能		メッセージ前処理
//---------------------------------------------------
BOOL COptGeneral::PreTranslateMessage(MSG* pMsg)
{
    if (pMsg->message == WM_KEYDOWN && pMsg->wParam == VK_TAB && ::GetKeyState(VK_CONTROL) < 0) {
        ::PostMessage(::GetParent(this->m_hWnd), pMsg->message, VK_PRIOR, pMsg->lParam);
    }

    return CDialog::PreTranslateMessage(pMsg);
}

//---------------------------------------------------
//関数名	OnInitDialog()
//機能		初期化
//---------------------------------------------------
BOOL COptGeneral::OnInitDialog()
{
    CDialog::OnInitDialog();

    //ホットキー
    if (theApp.m_ini.m_nDoubleKeyPOP != 0)	m_ctrlPopupKey.EnableWindow(false);
    if (theApp.m_ini.m_nDoubleKeyFIFO != 0)	m_ctrlFifoKey.EnableWindow(false);

    UINT uPopKey, uPopMod, uRirekiKey, uRirekiMod;
    theApp.m_ini.getHotKey(&uPopKey, &uPopMod, &uRirekiKey, &uRirekiMod);

    //コントロールにセット
    uPopMod = KeyHelper::ModToHotkey(uPopMod);
    m_ctrlPopupKey.SetHotKey(uPopKey, uPopMod);
    uRirekiMod = KeyHelper::ModToHotkey(uRirekiMod);
    m_ctrlFifoKey.SetHotKey(uRirekiKey, uRirekiMod);

    return TRUE;
}

void COptGeneral::OnShowWindow(BOOL bShow, UINT nStatus)
{
    CDialog::OnShowWindow(bShow, nStatus);
    if (bShow) {
        CWnd* w;
        switch (theApp.m_ini.m_nDoubleKeyPOP) {
        default:
        case 0:
            w = GetDlgItem(IDC_OPT_KEYRADIO_POP);
            break;
        case 1:
            w = GetDlgItem(IDC_OPT_KEYRADIO_POP2);
            break;
        case 2:
            w = GetDlgItem(IDC_OPT_KEYRADIO_POP3);
            break;
        case 3:
            w = GetDlgItem(IDC_OPT_KEYRADIO_POP4);
            break;
        }
        if (w) w->SetFocus();
    }
}

//---------------------------------------------------
//関数名	DestroyWindow()
//機能		設定を反映
//---------------------------------------------------
BOOL COptGeneral::DestroyWindow()
{
    WORD wVkCodeP, wModP;
    WORD wVkCodeF, wModF;

    m_ctrlPopupKey.GetHotKey(wVkCodeP, wModP);
    wModP = KeyHelper::HotkeyToMod(wModP);
    m_ctrlFifoKey.GetHotKey(wVkCodeF, wModF);
    wModF = KeyHelper::HotkeyToMod(wModF);
    theApp.m_ini.setHotkey(wVkCodeP, wModP, wVkCodeF, wModF);
    theApp.m_ini.m_bPutBackClipboard = ((CButton*)GetDlgItem(IDC_OPT_TOCLIP))->GetCheck() != 0;
    theApp.m_ini.m_bShowClipboardInTooltipOfNofifyIcon = ((CButton*)GetDlgItem(IDC_OPT_TOOLTIP_TITLE))->GetCheck() != 0;

    return CDialog::DestroyWindow();
}

void COptGeneral::HotkeyEnablePOP()
{
    m_ctrlPopupKey.EnableWindow(true);
    m_ctrlPopupKey.SetFocus();
}

void COptGeneral::HotkeyDisablePOP()
{
    m_ctrlPopupKey.EnableWindow(false);
}

void COptGeneral::HotkeyEnableFIFO()
{
    m_ctrlFifoKey.EnableWindow(true);
    m_ctrlFifoKey.SetFocus();
}

void COptGeneral::HotkeyDisableFIFO()
{
    m_ctrlFifoKey.EnableWindow(false);
}
