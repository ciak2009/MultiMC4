#include "multimc.h"

#include <wx/utils.h>
#include <wx/mstream.h>
#include <wx/cmdline.h>
#include <wx/process.h>
#include <wx/stdpaths.h>
#include <wx/fs_arc.h>
#include <wx/socket.h>
#include <wx/app.h>
#include <wx/sysopt.h>
#include <wx/progdlg.h>

#include "mainwindow.h"

#include "apputils.h"
#include "osutils.h"

#include "resources/windowicon.h"

IMPLEMENT_APP(MultiMC)

// App
bool MultiMC::OnInit()
{
#if __WXGTK__ || defined MSVC
	// Only works with Linux GCC or MSVC
	wxHandleFatalExceptions();
#endif
	updateOnExit = false;
	startMode = START_NORMAL;
	
	// This is necessary for the update system since it calls OnInitCmdLine
	// to set up the command line arguments that the update system uses.
	if (!wxApp::OnInit())
		return false;

	// On OS X set the working directory to $HOME/MultiMC
	if (IS_MAC())
	{
		wxFileName mmcDir = wxFileName::DirName(wxStandardPaths::Get().GetResourcesDir());
		mmcDir.Normalize();

		if (!mmcDir.DirExists())
			mmcDir.Mkdir(0777, wxPATH_MKDIR_FULL);

		wxSetWorkingDirectory(mmcDir.GetFullPath());
	}
	else
	{
		wxFileName mmcDir = wxStandardPaths::Get().GetExecutablePath();
		wxSetWorkingDirectory(mmcDir.GetPath());
	}

	SetAppName(_("MultiMC"));
	
	wxInitAllImageHandlers();
	wxSocketBase::Initialize();
	
	wxMemoryInputStream iconInput16(multimc16, sizeof(multimc16));
	wxMemoryInputStream iconInput32(multimc32, sizeof(multimc32));
	wxMemoryInputStream iconInput64(multimc64, sizeof(multimc64));
	wxMemoryInputStream iconInput128(multimc128, sizeof(multimc128));
	wxIcon icon16,icon32,icon64,icon128;
	icon16.CopyFromBitmap(wxBitmap(wxImage(iconInput16)));
	icon32.CopyFromBitmap(wxBitmap(wxImage(iconInput32)));
	icon64.CopyFromBitmap(wxBitmap(wxImage(iconInput64)));
	icon128.CopyFromBitmap(wxBitmap(wxImage(iconInput128)));
	AppIcons.AddIcon(icon16);
	AppIcons.AddIcon(icon32);
	AppIcons.AddIcon(icon64);
	AppIcons.AddIcon(icon128);
	
	wxFileSystem::AddHandler(new wxArchiveFSHandler);
	// 	wxFileSystem::AddHandler(new wxMemoryFSHandler);
	
	if (!InitAppSettings())
	{
		wxLogError(_("Failed to initialize settings->"));
		return false;
	}
	
	if (!settings->GetInstDir().DirExists())
		settings->GetInstDir().Mkdir();
	if (!settings->GetModsDir().DirExists())
		settings->GetModsDir().Mkdir();
	
	switch (startMode)
	{
	case START_NORMAL:
		{
			MainWindow *mainWin = new MainWindow();
			mainWin->Show();
			mainWin->OnStartup();
			return true;
		}

	case START_INSTALL_UPDATE:
		InstallUpdate();
		return false;
	}

	return false;
}

void MultiMC::OnInitCmdLine(wxCmdLineParser &parser)
{
	parser.SetDesc(cmdLineDesc);
	parser.SetSwitchChars(_("-"));
}

bool MultiMC::OnCmdLineParsed(wxCmdLineParser& parser)
{
	wxString fileToUpdate;
	if (parser.Found(_("u"), &fileToUpdate))
	{
		thisFileName = wxStandardPaths::Get().GetExecutablePath();
		updateTarget = fileToUpdate;
		startMode = START_INSTALL_UPDATE;
		return true;
	}
	return true;
}

inline void PulseYieldSleep(int secs, wxProgressDialog *dlg)
{
	int waitLoops = secs * 10;
	for (int i = 0; i < waitLoops; i++)
	{
		wxMilliSleep(100);
		wxYieldIfNeeded();
		dlg->Pulse();
	}
}

const int installUpdateTries = 5;

void MultiMC::InstallUpdate()
{
	int tryNum = 0;
	wxProgressDialog *progDlg = new wxProgressDialog(_("Installing updates..."),
		_("Waiting for old version to exit..."));

Retry:
	tryNum++;
	progDlg->Pulse(wxString::Format(
		_("Waiting for old version to exit... Try #%i."), tryNum));

	// Let the other process exit.
	PulseYieldSleep(1, progDlg);

	wxFileName targetFile(updateTarget);
	
	if (!wxCopyFile(thisFileName, targetFile.GetFullPath()))
	{
		if (tryNum < installUpdateTries)
			goto Retry;
		else
		{
			wxLogError(_("Failed to install updates. Tried %i times. \n\
This is probably because the file that is supposed to be updated is in use. \n\
Please check to make sure there are no other running MultiMC processes \n\
and that MultiMC's updater has sufficient permissions to replace the file \n\
%s and then try again."), tryNum, targetFile.GetFullPath().c_str());
			progDlg->Destroy();
			return;
		}
	}

	progDlg->Pulse(_("Update installed successfully. Starting MultiMC..."));
	progDlg->Fit();
	progDlg->CenterOnScreen();
	wxYieldIfNeeded();
	
	targetFile.MakeAbsolute();
	wxProcess proc;
	wxExecute(targetFile.GetFullPath(), wxEXEC_ASYNC, &proc);
	proc.Detach();
	progDlg->Destroy();
}

void MultiMC::YieldSleep(int secs)
{
	int waitLoops = secs * 10;
	for (int i = 0; i < waitLoops; i++)
	{
		wxMilliSleep(100);
		Yield(true);
	}
}

int MultiMC::OnExit()
{
#ifdef __WXMSW__
	wxString updaterFileName = _("MultiMCUpdate.exe");
#else
	wxString updaterFileName = _("MultiMCUpdate");
#endif

	if (updateOnExit && wxFileExists(updaterFileName))
	{
		wxFileName updateFile(Path::Combine(wxGetCwd(), updaterFileName));
		if (IS_LINUX() || IS_MAC())
		{
			wxExecute(_("chmod +x ") + updateFile.GetFullPath());
		}

		wxProcess proc;
		
		wxString updateFilePath = updateFile.GetFullPath();
		wxString thisFilePath = wxStandardPaths::Get().GetExecutablePath();

#if defined __WXMSW__
		updateFilePath.Replace(_(" "), _("^ "));
		
		wxString launchCmd = wxString::Format(_("%s -u:^\"%s^\""),
			updateFilePath.c_str(), thisFilePath.c_str());
#else
		updateFilePath.Replace(_(" "), _("\\ "));
		thisFilePath.Replace(_(" "), _("\\ "));

		wxString launchCmd = wxString::Format(_("%s -u:%s"),
			updateFilePath.c_str(), thisFilePath.c_str());
#endif

		if (IS_WINDOWS())
		{
			launchCmd = wxString::Format(_("cmd /C \"%s\""), launchCmd.c_str());
		}

		wxExecute(launchCmd, wxEXEC_ASYNC, &proc);
		proc.Detach();
	}

	delete settings;
	
	return wxApp::OnExit();
}

void MultiMC::OnFatalException()
{
	wxMessageBox(_("A fatal error has occurred and MultiMC has to exit. Sorry for the inconvenience."), 
		_("Oh no!"), wxICON_ERROR | wxCENTER);
}

void MultiMC::OnUnhandledException()
{
	OnFatalException();
}

const wxIconBundle &MultiMC::GetAppIcons() const
{
	return AppIcons;
}

const wxString licenseText = _(
"Licensed under the Apache License, Version 2.0 (the \"License\");\n\
you may not use this file except in compliance with the License.\n\
You may obtain a copy of the License at\n\
\n\
\thttp://www.apache.org/licenses/LICENSE-2.0\n\
\n\
Unless required by applicable law or agreed to in writing, software\n\
distributed under the License is distributed on an \"AS IS\" BASIS,\n\
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.\n\
See the License for the specific language governing permissions and\n\
limitations under the License.\n\
\n\
MultiMC uses bspatch, \n\
Copyright 2003-2005 Colin Percival\n\
All rights reserved\n\
\n\
Redistribution and use in source and binary forms, with or without\n\
modification, are permitted providing that the following conditions\n\
are met: \n\
1. Redistributions of source code must retain the above copyright\n\
   notice, this list of conditions and the following disclaimer.\n\
2. Redistributions in binary form must reproduce the above copyright\n\
   notice, this list of conditions and the following disclaimer in the\n\
   documentation and/or other materials provided with the distribution.\n\
\n\
THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR\n\
IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED\n\
WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE\n\
ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY\n\
DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL\n\
DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS\n\
OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)\n\
HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,\n\
STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING\n\
IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE\n\
POSSIBILITY OF SUCH DAMAGE.");
