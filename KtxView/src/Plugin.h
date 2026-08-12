#pragma once

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include "ImagPlug.H"

#define __UNICODE_TEXT(x) L##x
#define UNICODE_TEXT(x) __UNICODE_TEXT(x)

// KTX1 (.ktx)
BOOL IMAGINEAPI checkKtx1(IMAGINEPLUGINFILEINFOTABLE *fileInfoTable, IMAGINELOADPARAM *loadParam, int flags);
LPIMAGINEBITMAP IMAGINEAPI loadKtx1(IMAGINEPLUGINFILEINFOTABLE *fileInfoTable, IMAGINELOADPARAM *loadParam, int flags);

// KTX2 (.ktx2)
BOOL IMAGINEAPI checkKtx2(IMAGINEPLUGINFILEINFOTABLE *fileInfoTable, IMAGINELOADPARAM *loadParam, int flags);
LPIMAGINEBITMAP IMAGINEAPI loadKtx2(IMAGINEPLUGINFILEINFOTABLE *fileInfoTable, IMAGINELOADPARAM *loadParam, int flags);

// Basis Universal (.basis)
BOOL IMAGINEAPI checkBasis(IMAGINEPLUGINFILEINFOTABLE *fileInfoTable, IMAGINELOADPARAM *loadParam, int flags);
LPIMAGINEBITMAP IMAGINEAPI loadBasis(IMAGINEPLUGINFILEINFOTABLE *fileInfoTable, IMAGINELOADPARAM *loadParam, int flags);

// 저장은 지원하지 않음 — 공용 스텁
BOOL IMAGINEAPI saveStub(IMAGINEPLUGINFILEINFOTABLE *fileInfoTable, LPIMAGINEBITMAP bitmap, IMAGINESAVEPARAM *saveParam, int flags);

// basisu_transcoder_init()을 1회만 호출 (DllMain에서는 금지 — 로더 락)
void EnsureBasisTranscoderInit();
