#include "Plugin.h"

#include <mutex>

#include "basisu_transcoder.h"

// 'Installed plugins' 목록에는 이 한 줄만 보이므로 지원 코덱을 간략히 덧붙인다.
#define PLUGIN_DESCRIPTION "KTX / KTX2 / Basis Universal Loader Plugin (ETC, BCn, ASTC)"
#define VERSION_NUMBER MAKE_IMAGINEPLUGININTERFACE_VERSION(1, 0, 0, 0)

#define FILETYPE_KTX "KTX Texture"
#define EXTENSION_KTX "KTX\0"
#define FILETYPE_KTX2 "KTX2 Texture"
#define EXTENSION_KTX2 "KTX2\0"
#define FILETYPE_BASIS "Basis Universal Texture"
#define EXTENSION_BASIS "BASIS\0"

// 정보 대화상자 본문. A/W가 갈라지지 않도록 한 곳에만 정의한다.
#define PLUGIN_INFO_TEXT \
	"KTX / KTX2 / Basis Universal texture loader.\n" \
	"\n" \
	"KTX1 (.ktx)\n" \
	"    Uncompressed (RGBA8, RGB8, BGRA, BGR, R8, RG8, L8, LA8)\n" \
	"    ETC1, ETC2 (RGB / RGB-A1 / RGBA), EAC (R11, RG11)\n" \
	"    BC1-BC7 (DXT1/3/5, RGTC, BPTC), BC6H HDR\n" \
	"    ASTC LDR, all block sizes from 4x4 to 12x12\n" \
	"\n" \
	"KTX2 (.ktx2)\n" \
	"    Basis ETC1S (BasisLZ) and UASTC LDR\n" \
	"    ASTC LDR and ASTC HDR (SFLOAT), all block sizes\n" \
	"    Raw VkFormat: uncompressed, BCn, ETC2, EAC\n" \
	"    Zstandard supercompression\n" \
	"\n" \
	"Basis Universal (.basis)\n" \
	"    ETC1S and UASTC, including HDR\n" \
	"\n" \
	"ASTC block sizes: 4x4 5x4 5x5 6x5 6x6 8x5 8x6 8x8\n" \
	"                  10x5 10x6 10x8 10x10 12x10 12x12\n" \
	"\n" \
	"HDR content is tone-clamped to 8 bits for display.\n" \
	"Shows the base mip level and the first face / array layer.\n" \
	"Saving is not supported."

void EnsureBasisTranscoderInit()
{
	static std::once_flag s_once;
	std::call_once(s_once, [] { basist::basisu_transcoder_init(); });
}

BOOL IMAGINEAPI saveStub(IMAGINEPLUGINFILEINFOTABLE *fileInfoTable, LPIMAGINEBITMAP bitmap, IMAGINESAVEPARAM *saveParam, int flags)
{
	(void)fileInfoTable; (void)bitmap; (void)flags;
	saveParam->errorCode = IMAGINEERROR_UNSUPPORTEDTYPE;
	return FALSE;
}

// ---- 파일 타입 테이블 (A/W는 문자열만 다름) ----

static const IMAGINEFILEINFOITEM fileInfoKtxA = { checkKtx1, loadKtx1, saveStub, FILETYPE_KTX, EXTENSION_KTX, NULL, NULL };
static const IMAGINEFILEINFOITEM fileInfoKtx2A = { checkKtx2, loadKtx2, saveStub, FILETYPE_KTX2, EXTENSION_KTX2, NULL, NULL };
static const IMAGINEFILEINFOITEM fileInfoBasisA = { checkBasis, loadBasis, saveStub, FILETYPE_BASIS, EXTENSION_BASIS, NULL, NULL };

static const IMAGINEFILEINFOITEM fileInfoKtxW = { checkKtx1, loadKtx1, saveStub,
	(LPCTSTR)UNICODE_TEXT(FILETYPE_KTX), (LPCTSTR)UNICODE_TEXT(EXTENSION_KTX), NULL, NULL };
static const IMAGINEFILEINFOITEM fileInfoKtx2W = { checkKtx2, loadKtx2, saveStub,
	(LPCTSTR)UNICODE_TEXT(FILETYPE_KTX2), (LPCTSTR)UNICODE_TEXT(EXTENSION_KTX2), NULL, NULL };
static const IMAGINEFILEINFOITEM fileInfoBasisW = { checkBasis, loadBasis, saveStub,
	(LPCTSTR)UNICODE_TEXT(FILETYPE_BASIS), (LPCTSTR)UNICODE_TEXT(EXTENSION_BASIS), NULL, NULL };

// ---- 등록 / 정보 ----

static BOOL IMAGINEAPI registerProcA(const IMAGINEPLUGININTERFACE *iface)
{
	EnsureBasisTranscoderInit();
	BOOL result = TRUE;
	result = iface->lpVtbl->RegisterFileType(&fileInfoKtxA) ? result : FALSE;
	result = iface->lpVtbl->RegisterFileType(&fileInfoKtx2A) ? result : FALSE;
	result = iface->lpVtbl->RegisterFileType(&fileInfoBasisA) ? result : FALSE;
	return result;
}

static BOOL IMAGINEAPI registerProcW(const IMAGINEPLUGININTERFACE *iface)
{
	EnsureBasisTranscoderInit();
	BOOL result = TRUE;
	result = iface->lpVtbl->RegisterFileType(&fileInfoKtxW) ? result : FALSE;
	result = iface->lpVtbl->RegisterFileType(&fileInfoKtx2W) ? result : FALSE;
	result = iface->lpVtbl->RegisterFileType(&fileInfoBasisW) ? result : FALSE;
	return result;
}

static BOOL IMAGINEAPI infoProcA(const IMAGINEPLUGININTERFACE *iface, HINSTANCE hInstance, HWND parent)
{
	(void)hInstance;
	iface->lpVtbl->MessageBox(parent, (LPCTSTR)PLUGIN_INFO_TEXT, (LPCTSTR)PLUGIN_DESCRIPTION, MB_OK);
	return TRUE;
}

static BOOL IMAGINEAPI infoProcW(const IMAGINEPLUGININTERFACE *iface, HINSTANCE hInstance, HWND parent)
{
	(void)hInstance;
	iface->lpVtbl->MessageBox(parent, (LPCTSTR)UNICODE_TEXT(PLUGIN_INFO_TEXT),
		(LPCTSTR)UNICODE_TEXT(PLUGIN_DESCRIPTION), MB_OK);
	return TRUE;
}

// ---- 플러그인 정보 ----

static const IMAGINEPLUGININFOA pluginInfoA =
{
	sizeof(IMAGINEPLUGININFOA),
	registerProcA,
	VERSION_NUMBER,
	PLUGIN_DESCRIPTION,
	MAKE_IMAGINEPLUGININTERFACE_VERSION(1, 5, 3, 0),
	NULL,
	infoProcA,
};

static const IMAGINEPLUGININFOW pluginInfoW =
{
	sizeof(IMAGINEPLUGININFOW),
	registerProcW,
	VERSION_NUMBER,
	UNICODE_TEXT(PLUGIN_DESCRIPTION),
	MAKE_IMAGINEPLUGININTERFACE_VERSION(1, 5, 3, 0),
	NULL,
	infoProcW,
};

BOOL WINAPI DllMain(HINSTANCE hInstance, DWORD dwReason, LPVOID lpvReserved)
{
	(void)hInstance; (void)dwReason; (void)lpvReserved;
	return TRUE;
}

extern "C" BOOL IMAGINEAPI ImaginePluginGetInfoA(IMAGINEPLUGININFOA *dest)
{
	*dest = pluginInfoA;
	return TRUE;
}

extern "C" BOOL IMAGINEAPI ImaginePluginGetInfoW(IMAGINEPLUGININFOW *dest)
{
	*dest = pluginInfoW;
	return TRUE;
}
