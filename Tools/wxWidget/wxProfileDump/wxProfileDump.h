/** @file
File:       wxProfileDump.h

Contains:   Header file for main application that displays profile contents
            using wxWidgets

Version:    V1

Copyright:  (c) see Software License
*/

/*
* Copyright (c) International Color Consortium.
*
* Redistribution and use in source and binary forms, with or without
* modification, are permitted provided that the following conditions
* are met:
*
* 1. Redistributions of source code must retain the above copyright
*    notice, this list of conditions and the following disclaimer. 
*
* 2. Redistributions in binary form must reproduce the above copyright
*    notice, this list of conditions and the following disclaimer in
*    the documentation and/or other materials provided with the
*    distribution.
*
* 3. In the absence of prior written permission, the names "ICC" and "The
*    International Color Consortium" must not be used to imply that the
*    ICC organization endorses or promotes products derived from this
*    software.
*
*
* THIS SOFTWARE IS PROVIDED ``AS IS'' AND ANY EXPRESSED OR IMPLIED
* WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
* OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
* DISCLAIMED.  IN NO EVENT SHALL THE INTERNATIONAL COLOR CONSORTIUM OR
* ITS CONTRIBUTING MEMBERS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
* SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
* LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF
* USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
* ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
* OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT
* OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
* SUCH DAMAGE.
* ====================================================================
*
* This software consists of voluntary contributions made by many
* individuals on behalf of the The International Color Consortium. 
*
*
* Membership in the ICC is encouraged when this software is used for
* commercial purposes. 
*
*  
* For more information on The International Color Consortium, please
* see <http://www.color.org/>.
*  
* 
*/

#include <wx/toolbar.h>
#include <wx/listctrl.h>
#include <wx/docview.h>
#include <wx/dnd.h>

// Define a new application
class MyApp : public wxApp
{
public:
    bool OnInit();
    int OnExit();

    wxFileHistory m_history;
};

class MyCanvas : public wxScrolledWindow
{
public:
    MyCanvas(wxWindow *parent, const wxPoint& pos, const wxSize& size);
    virtual void OnDraw(wxDC& dc);

    bool IsDirty() const { return m_dirty; }

    void OnEvent(wxMouseEvent& event);

private:
    bool m_dirty;

    DECLARE_EVENT_TABLE()
};

// Define a new frame
class MyFrame : public wxMDIParentFrame
{
public:
    MyFrame(wxWindow *parent, const wxWindowID id, const wxString& title,
            const wxPoint& pos, const wxSize& size, const long style);

    void InitToolBar(wxToolBar* toolBar);

    void OnAbout(wxCommandEvent& event);
    void OnOpenProfile(wxCommandEvent& event);
    void OnQuit(wxCommandEvent& event);
    void OnClose(wxCloseEvent& event);

    void OpenFile(wxString path);

    DECLARE_EVENT_TABLE()
};

class MyDnDFile : public wxFileDropTarget
{
public:
  MyDnDFile(MyFrame *owner) { m_owner = owner; }

  virtual bool OnDropFiles(wxCoord x, wxCoord y,
                           const wxArrayString& filenames)
  {
     size_t nFiles = filenames.GetCount();
     wxString str;
     for (size_t n=0; n<nFiles; n++) {
       m_owner->OpenFile(filenames[n]);
     }
     return true;
  }
private:
  MyFrame *m_owner;
};

class MyChild: public wxMDIChildFrame
{
public:
    MyChild(wxMDIParentFrame *parent, const wxString& title, CIccProfile *pIcc, const wxString &profilePath);
    ~MyChild();

    void OnActivate(wxActivateEvent& event);

    void OnRefresh(wxCommandEvent& event);
    void OnUpdateRefresh(wxUpdateUIEvent& event);
    void OnQuit(wxCommandEvent& event);
    void OnClose(wxCloseEvent& event);
	void OnValidate(wxCommandEvent& event);
    void OnRoundTrip(wxCommandEvent& event);
	void OnTagClicked(wxListEvent& event);

    void SetFileMenu(wxMenu *menu);

protected:
    wxMenu *m_fileMenu;

	CIccProfile *m_pIcc;
	wxString m_profilePath;

	wxPanel *m_panel;
	wxListCtrl *m_tagsCtrl;

	wxStaticText *m_textAttribute;
    wxStaticText *m_textBiSpectralWavelengths;
	wxStaticText *m_textCMM;
	wxStaticText *m_textColorSpace;
	wxStaticText *m_textCreationDate;
	wxStaticText *m_textCreator;
	wxStaticText *m_textFlags;
	wxStaticText *m_textIlluminant;
	wxStaticText *m_textPCS;
	wxStaticText *m_textPlatform;
    wxStaticText *m_textDeviceManufacturer;
	wxStaticText *m_textProfileID;
	wxStaticText *m_textRenderingIntent;
    wxStaticText *m_textSpectralPCS;
    wxStaticText *m_textSpectralWavelengths;
    wxStaticText *m_textMaterialColorSpace;
	wxStaticText *m_textSize;
	wxStaticText *m_textClass;
    wxStaticText *m_textSubClass;
	wxStaticText *m_textVersion;
    wxStaticText *m_textSubClassVersion;

	wxSizer *CreateSizerWithText(const wxString &labelText, wxStaticText **ppText);

    DECLARE_EVENT_TABLE()
};

class MyDialog : public wxDialog
{
public:
	MyDialog(wxWindow *pParent, const wxString& title, wxString &profilePath);

	wxString m_profilePath;
};

class MyRoundTripDialog : public wxDialog
{
public:
  MyRoundTripDialog(wxWindow *pParent, const wxString& title, wxString &profilePath, CIccProfile *pIcc);

  wxString m_profilePath;
};

class MyTagDialog : public wxDialog
{
public:
	MyTagDialog(wxWindow *pParent, CIccProfile *pIcc /*=NULL*/, icTagSignature sig/*=icMaxEnumTag*/, CIccTag *pTag /*=NULL*/);

	CIccProfile *m_pIcc;
	icTagSignature m_sigTag;
	CIccTag *m_pTag;

	wxFont m_FixedFont;
};

// menu items ids
enum
{
    MDI_QUIT = wxID_EXIT,
    MDI_OPEN_PROFILE = 101,
    MDI_CHILD_QUIT,
    MDI_ABOUT = wxID_ABOUT
};
