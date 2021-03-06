// TortoiseSI - a Windows shell extension for easy version control

// Copyright (C) 2003-2010, 2012 - TortoiseSVN
// Copyright (C) 2008-2012,2014 - TortoiseGit
// Copyright (C) 2015 - TortoiseSI

// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License
// as published by the Free Software Foundation; either version 2
// of the License, or (at your option) any later version.

// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.

// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software Foundation,
// 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
//
#include "stdafx.h"
#include "ShellExt.h"
#include "Guids.h"
#include "ShellExtClassFactory.h"
#include "ShellObjects.h"
#include "EventLog.h"

volatile LONG		g_cRefThisDll = 0;				///< reference count of this DLL.
HINSTANCE			g_hmodThisDll = NULL;			///< handle to this DLL itself.
ShellCache			g_ShellCache;					///< caching of registry entries, ...
DWORD				g_langid;
DWORD				g_langTimeout = 0;
HINSTANCE			g_hResInst = NULL;
bool				g_readonlyoverlay = false;
bool				g_lockedoverlay = false;

bool				g_normalovlloaded = false;
bool				g_modifiedovlloaded = false;
bool				g_conflictedovlloaded = false;
bool				g_readonlyovlloaded = false;
bool				g_deletedovlloaded = false;
bool				g_lockedovlloaded = false;
bool				g_addedovlloaded = false;
bool				g_ignoredovlloaded = false;
bool				g_unversionedovlloaded = false;
CComCriticalSection	g_csGlobalCOMGuard;

LPCTSTR				g_MenuIDString = _T("TortoiseSI");

ShellObjects		g_shellObjects;

#pragma comment(linker, "\"/manifestdependency:type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='*' publicKeyToken='6595b64144ccf1df' language='*'\"")

extern "C" int APIENTRY
DllMain(HINSTANCE hInstance, DWORD dwReason, LPVOID /* lpReserved */)
{
#ifdef _DEBUG
	// if no debugger is present, then don't load the dll.
	// this prevents other apps from loading the dll and locking
	// it.

	if (!::IsDebuggerPresent())
	{
		EventLog::writeInformation(L"In debug load preventer");
		return FALSE;
	}
#endif

	// NOTE: Do *NOT* init the PTC API here in DllMain(),
	// because those functions may call LoadLibrary() 
	// And LoadLibrary() inside DllMain() is not allowed and can lead to unexpected
	// behavior and even may create dependency loops in the dll load order.
	if (dwReason == DLL_PROCESS_ATTACH)
	{
		EventLog::writeInformation(L"TortoiseSI dll Loaded");

		if (g_hmodThisDll == NULL)
		{
			g_csGlobalCOMGuard.Init();
		}

		// Extension DLL one-time initialization
		g_hmodThisDll = hInstance;
	}
	else if (dwReason == DLL_PROCESS_DETACH)
	{
		EventLog::writeInformation(L"TortoiseSI dll unloaded by");

		// do not clean up memory here:
		// if an application doesn't release all COM objects
		// but still unloads the dll, cleaning up ourselves
		// will lead to crashes.
		// better to leak some memory than to crash other apps.
		// sometimes an application doesn't release all COM objects
		// but still unloads the dll.
		// in that case, we do it ourselves
		g_csGlobalCOMGuard.Term();
	}
	return 1;	// ok
}

STDAPI DllCanUnloadNow(void)
{
	EventLog::writeInformation(std::wstring(L"DllCanUnloadNow") + 
		L", g_cRefThisDll = " + std::to_wstring(g_cRefThisDll));

	return (g_cRefThisDll == 0 ? S_OK : S_FALSE);
}

STDAPI DllGetClassObject(REFCLSID rclsid, REFIID riid, LPVOID *ppvOut)
{
	if (ppvOut == 0)
		return E_POINTER;
	*ppvOut = NULL;

	FileState state = FileStateInvalid;
	if (IsEqualIID(rclsid, CLSID_TortoiseSI_UPTODATE))
		state = FileStateVersioned;
	else if (IsEqualIID(rclsid, CLSID_TortoiseSI_MODIFIED))
		state = FileStateModified;
	else if (IsEqualIID(rclsid, CLSID_TortoiseSI_CONFLICTING))
		state = FileStateConflict;
	else if (IsEqualIID(rclsid, CLSID_TortoiseSI_UNCONTROLLED))
		state = FileStateUncontrolled;
	else if (IsEqualIID(rclsid, CLSID_TortoiseSI_DELETED))
		state = FileStateDeleted;
	else if (IsEqualIID(rclsid, CLSID_TortoiseSI_READONLY))
		state = FileStateReadOnly;
	else if (IsEqualIID(rclsid, CLSID_TortoiseSI_LOCKED))
		state = FileStateLockedOverlay;
	else if (IsEqualIID(rclsid, CLSID_TortoiseSI_ADDED))
		state = FileStateAddedOverlay;
	else if (IsEqualIID(rclsid, CLSID_TortoiseSI_IGNORED))
		state = FileStateIgnoredOverlay;
	else if (IsEqualIID(rclsid, CLSID_TortoiseSI_UNVERSIONED))
		state = FileStateUnversionedOverlay;

	if (state != FileStateInvalid)
	{
		CShellExtClassFactory *pcf = new (std::nothrow) CShellExtClassFactory(state);
		if (pcf == NULL)
			return E_OUTOFMEMORY;
		
		const HRESULT hr = pcf->QueryInterface(riid, ppvOut);
		if(FAILED(hr))
			delete pcf;
		return hr;
	}

	return CLASS_E_CLASSNOTAVAILABLE;
}

 wchar_t* fileStateString[] = {
    L"FileStateUncontrolled",
    L"FileStateVersioned",
    L"FileStateModified",
    L"FileStateConflict",
    L"FileStateDeleted",
    L"FileStateReadOnly",
    L"FileStateLockedOverlay",
    L"FileStateAddedOverlay",
    L"FileStateIgnoredOverlay",
    L"FileStateUnversionedOverlay",
    L"FileStateDropHandler",
    L"FileStateInvalid",
    L"FileStateNoState"
};

std::wstring to_wstring(FileState fileState)
{
	return fileStateString[fileState];
}
