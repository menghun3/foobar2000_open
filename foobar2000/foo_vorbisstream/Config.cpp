// Config.cpp : implementation file
//

#include "stdafx.h"
#include "resource.h"
#include "Config.h"
//#include "MainWindow.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif


CConfig::CConfig(dsp_preset_edit_callback & p_callback,CWnd* pParent /*=NULL*/)
	: CDialog(CConfig::IDD, pParent), m_callback(p_callback)
{
	//{{AFX_DATA_INIT(CConfig)
		// NOTE: the ClassWizard will add member initialization here
	//}}AFX_DATA_INIT

    basicSettings = new CBasicSettings();
    ypSettings = new CYPSettings();
    currentEnc = 0;
}

/////////////////////////////////////////////////////////////////////////////
// CConfig dialog
/*CConfig::CConfig(CWnd* pParent)
	: CDialog(CConfig::IDD, pParent)
{
	//{{AFX_DATA_INIT(CConfig)
		// NOTE: the ClassWizard will add member initialization here
	//}}AFX_DATA_INIT

    basicSettings = new CBasicSettings();
    ypSettings = new CYPSettings();
    currentEnc = 0;
}*/

CConfig::~CConfig()
{
    if (basicSettings) {
        delete basicSettings;
    }
    if (ypSettings) {
        delete ypSettings;
    }
}
void CConfig::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	//{{AFX_DATA_MAP(CConfig)
	DDX_Control(pDX, IDC_TAB1, m_TabCtrl);
	//}}AFX_DATA_MAP
}


BEGIN_MESSAGE_MAP(CConfig, CDialog)
	//{{AFX_MSG_MAP(CConfig)
	ON_NOTIFY(TCN_SELCHANGE, IDC_TAB1, OnSelchangeTab1)
	ON_WM_CLOSE()
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CConfig message handlers

BOOL CConfig::OnInitDialog() 
{
	CDialog::OnInitDialog();
	
	// TODO: Add extra initialization here
	m_TabCtrl.InsertItem(0, _T("Basic Settings"));
	m_TabCtrl.InsertItem(1, _T("YP Settings"));

    basicSettings->Create((UINT)IDD_PROPPAGE_LARGE, this);    
    ypSettings->Create((UINT)IDD_PROPPAGE_LARGE1, this);    

    basicSettings->ShowWindow(SW_SHOW);
    ypSettings->ShowWindow(SW_HIDE);

	GlobalsToDialog(globals);

	return TRUE;  
}

void CConfig::OnSelchangeTab1(NMHDR* pNMHDR, LRESULT* pResult) 
{
	int selected = m_TabCtrl.GetCurSel();
    if (selected == 0) {
        basicSettings->ShowWindow(SW_SHOW);
        ypSettings->ShowWindow(SW_HIDE);
    }
    if (selected == 1) {
        ypSettings->ShowWindow(SW_SHOW);
        basicSettings->ShowWindow(SW_HIDE);
    }

	*pResult = 0;
}
void CConfig::GlobalsToDialog(edcastGlobals *g) {
    char    buf[255] = "";

    currentEnc = g->encoderNumber;
	sprintf(buf, "%d", getCurrentBitrate(g));
    basicSettings->m_Bitrate = buf;
    sprintf(buf, "%d", getCurrentChannels(g));
    basicSettings->m_Channels = buf;
    sprintf(buf, "%d", getCurrentSamplerate(g));
    basicSettings->m_Samplerate = buf;
	
	basicSettings->m_UseBitrate=(g->gOggBitQualFlag!=0);
	basicSettings->m_EncoderType = "OggVorbis";
	basicSettings->m_Quality = g->gOggQuality;

	//    basicSettings->m_EncoderTypeCtrl.SelectString(0, basicSettings->m_EncoderType);
    basicSettings->m_Mountpoint = g->gMountpoint;
    basicSettings->m_Password = g->gPassword;
    basicSettings->m_ServerIP = g->gServer;
    basicSettings->m_ServerPort = g->gPort;
	basicSettings->m_ServerType = (g->gShoutcastFlag)?"Shoutcast":"Icecast2";
	basicSettings->m_ServerTypeCtrl.SelectString(0, basicSettings->m_ServerType);
    basicSettings->UpdateData(FALSE);
    basicSettings->UpdateFields();

	ypSettings->m_Public=g->gPubServ;
	ypSettings->m_StreamDesc = g->gServDesc;
    ypSettings->m_StreamName = g->gServName;
    ypSettings->m_StreamGenre = g->gServGenre;
    ypSettings->m_StreamURL = g->gServURL;
    ypSettings->m_StreamAIM = g->gServAIM;
    ypSettings->m_StreamICQ = g->gServICQ;
    ypSettings->m_StreamIRC = g->gServIRC;
    ypSettings->UpdateData(FALSE);
    ypSettings->EnableDisable();
}

void CConfig::DialogToGlobals(edcastGlobals *g) {
    char    buf[255] = "";

    currentEnc = g->encoderNumber;

    g->currentBitrate = atoi(LPCSTR(basicSettings->m_Bitrate));
    g->currentChannels = atoi(LPCSTR(basicSettings->m_Channels));
    g->currentSamplerate = atoi(LPCSTR(basicSettings->m_Samplerate));

	g->gOggBitQualFlag=(basicSettings->m_UseBitrate)?1:0;

    strcpy(g->gEncodeType, LPCSTR(basicSettings->m_EncoderType));
    strcpy(g->gOggQuality, LPCSTR(basicSettings->m_Quality));
    strcpy(g->gMountpoint, LPCSTR(basicSettings->m_Mountpoint));
    strcpy(g->gPassword, LPCSTR(basicSettings->m_Password));

    g->gReconnectSec = atoi(LPCSTR(basicSettings->m_ReconnectSecs));
    strcpy(g->gServer, LPCSTR(basicSettings->m_ServerIP));
    strcpy(g->gPort, LPCSTR(basicSettings->m_ServerPort));

    if (basicSettings->m_ServerType == "Shoutcast") {
        g->gShoutcastFlag = 1;
        g->gIcecast2Flag = 0;
    }
    if (basicSettings->m_ServerType == "Icecast2") {
        g->gShoutcastFlag = 0;
        g->gIcecast2Flag = 1;
    }
    strcpy(g->gServerType, LPCSTR(basicSettings->m_ServerType));

    ypSettings->UpdateData(TRUE);
	
	g->gPubServ=(ypSettings->m_Public)?1:0;
    strcpy(g->gServDesc, LPCSTR(ypSettings->m_StreamDesc));
    strcpy(g->gServName, LPCSTR(ypSettings->m_StreamName));
    strcpy(g->gServGenre, LPCSTR(ypSettings->m_StreamGenre));
    strcpy(g->gServURL, LPCSTR(ypSettings->m_StreamURL));
    strcpy(g->gServAIM, LPCSTR(ypSettings->m_StreamAIM));
    strcpy(g->gServICQ, LPCSTR(ypSettings->m_StreamICQ));
    strcpy(g->gServIRC, LPCSTR(ypSettings->m_StreamIRC));
}

void CConfig::OnClose() 
{
    CDialog::OnClose();
}

void CConfig::OnOK() 
{
	basicSettings->UpdateData(TRUE);
	ypSettings->UpdateData(TRUE);
//    CMainWindow *pwin = (CMainWindow *)parentDialog;
//    pwin->ProcessConfigDone(currentEnc, this);
	edcastGlobals* out=new edcastGlobals;
	memset(out, '\000', sizeof(edcastGlobals));
	DialogToGlobals(out);
	dsp_preset_impl* preout=new dsp_preset_impl;
	preout->set_owner(data_owner);
	preout->set_data(out,sizeof(edcastGlobals));
	
	m_callback.on_preset_changed(*preout);
    CDialog::OnOK();
}

void CConfig::OnCancel() 
{
//    CMainWindow *pwin = (CMainWindow *)parentDialog;
//    pwin->ProcessConfigDone(-1, this);
    CDialog::OnCancel();
}
