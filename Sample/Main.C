#include "Main.H"

#define VERSION_NUMBER1 1
#define VERSION_NUMBER2 0
#define VERSION_NUMBER3 0
#define VERSION_NUMBER4 0
#define VERSION_NUMBER (VERSION_NUMBER1<<24)|(VERSION_NUMBER2<<16)|(VERSION_NUMBER3<<8)|(VERSION_NUMBER4<<0)

static BOOL IMAGINEAPI registerProcA(const IMAGINEPLUGININTERFACE *iface)
{
	return (BOOL)iface->lpVtbl->RegisterFileType(IPSGetFileInfoA());
}

static BOOL IMAGINEAPI registerProcW(const IMAGINEPLUGININTERFACE *iface)
{
	return (BOOL)iface->lpVtbl->RegisterFileType(IPSGetFileInfoW());
}

static BOOL IMAGINEAPI infoProcA(const IMAGINEPLUGININTERFACE *iface,HINSTANCE hInstance,HWND parent)
{
	BOOL result=TRUE;
	LPCTSTR text="Imagine Plugin Sample Format Plugin\n\nPresented by nyam";
	LPCTSTR caption="IPS";

	iface->lpVtbl->MessageBox(parent,text,caption,MB_OK|MB_ICONINFORMATION);

	return result;
}

static BOOL IMAGINEAPI infoProcW(const IMAGINEPLUGININTERFACE *iface,HINSTANCE hInstance,HWND parent)
{
	BOOL result=TRUE;
	LPCTSTR text=(LPCTSTR)UNICODE_TEXT("Imagine Plugin Sample Format Plugin\n\nPresented by nyam");
	LPCTSTR caption=(LPCTSTR)UNICODE_TEXT("IPS");

	iface->lpVtbl->MessageBox(parent,text,caption,MB_OK|MB_ICONINFORMATION);

	return result;
}

// Plugin info (ANSI)
static const IMAGINEPLUGININFOA pluginInfoA=
{
	sizeof(pluginInfoA),
	registerProcA,
	VERSION_NUMBER,
	"Imagine Plugin Sample Format Plugin",
	MAKE_IMAGINEPLUGININTERFACE_VERSION(1,5,3,0),
	NULL,
	infoProcA,
};

// Plugin info (Unicode)
static const IMAGINEPLUGININFOW pluginInfoW=
{
	sizeof(pluginInfoW),
	registerProcW,
	VERSION_NUMBER,
	L"Imagine Plugin Sample Format Plugin",
	MAKE_IMAGINEPLUGININTERFACE_VERSION(1,5,3,0),
	NULL,
	infoProcW,
};

BOOL CALLBACK DllMain(HINSTANCE hInstance,DWORD dwReason,LPVOID lpvReserved)
{
	return TRUE;
}

BOOL IMAGINEAPI ImaginePluginGetInfoA(IMAGINEPLUGININFOA *dest)
{
	*dest=pluginInfoA;

	return TRUE;
}

BOOL IMAGINEAPI ImaginePluginGetInfoW(IMAGINEPLUGININFOW *dest)
{
	*dest=pluginInfoW;

	return TRUE;
}
