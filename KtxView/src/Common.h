#pragma once

#include "Plugin.h"
#include <stdint.h>

// 모든 로더(KTX1 / raw KTX2)가 공유하는 소스 텍스처 포맷
enum class TexFormat
{
	// 비압축 (glType != 0 / VkFormat 비압축)
	R8, RG8, L8, LA8, RGB8, BGR8, RGBA8, BGRA8,
	// 블록 압축
	BC1,       // 알파 없음(RGB) — punchthrough 무시하고 불투명 처리
	BC1A,      // 1비트 punchthrough 알파
	BC2, BC3, BC4, BC5, BC6H_UF, BC6H_SF, BC7,
	ETC1, ETC2_RGB, ETC2_RGB_A1, ETC2_RGBA, EAC_R11, EAC_RG11,
	// ASTC LDR — 아래 14개는 g_astcBlockDims 순서와 일치해야 함
	ASTC_4x4, ASTC_5x4, ASTC_5x5, ASTC_6x5, ASTC_6x6,
	ASTC_8x5, ASTC_8x6, ASTC_8x8, ASTC_10x5, ASTC_10x6,
	ASTC_10x8, ASTC_10x10, ASTC_12x10, ASTC_12x12,

	Unsupported, // 인식은 되지만 디코딩 미지원 (SNORM 등)
	Invalid,     // 알 수 없는 포맷
};

bool TexFormatIsBlock(TexFormat f);
bool TexFormatHasAlpha(TexFormat f);

// 압축 포맷의 블록 크기(픽셀). 비압축이면 1x1.
void TexFormatBlockDims(TexFormat f, uint32_t *blockW, uint32_t *blockH);

// KTX/VkFormat의 ASTC 블록 크기 인덱스(0~13) → TexFormat. 범위 밖이면 Invalid.
TexFormat TexFormatAstcFromIndex(uint32_t index);

// w×h 이미지 1장이 소스 버퍼에서 차지하는 바이트 수.
// 비압축은 rowAlign(KTX1=4, KTX2=1) 행 정렬 반영, 블록 포맷은 4x4 블록 반올림.
// 오버플로 시 false.
bool TexFormatImageSize(TexFormat f, uint32_t w, uint32_t h, uint32_t rowAlign, uint64_t *outSize);

// src의 이미지 1장을 dstRgba(w*h*4, 메모리상 R,G,B,A 순)로 디코딩.
// srcSize는 TexFormatImageSize 이상이어야 함(호출 전에 검증할 것).
// srgb는 ASTC 디코드 프로파일 선택에만 쓰임(다른 포맷은 저장된 바이트를 그대로 사용).
bool DecodeImage(TexFormat f, const uint8_t *src, uint32_t w, uint32_t h, uint32_t rowAlign, uint8_t *dstRgba,
                 bool srgb = false);

// RGBA8 중간 버퍼 → Imagine 비트맵(32bpp BGRA). 행별 스위즐 + 진행 콜백 + 알파 설정.
// 실패(사용자 취소) 시 loadParam->errorCode 설정 후 FALSE.
BOOL CopyRgbaToBitmap(const IMAGINEPLUGININTERFACE *iface, IMAGINELOADPARAM *loadParam, int flags,
                      LPIMAGINEBITMAP bitmap, const uint8_t *rgba, LONG width, LONG height, BOOL hasAlpha);

// 16비트 half float → [0,1] 클램프 후 8비트 (HDR 텍스처 표시용)
uint8_t HalfToU8(uint16_t h);

// w*h*4 RGBA 중간 버퍼 할당. 총 크기가 1GB를 넘거나(손상/과대 파일) 실패하면 NULL.
uint8_t *AllocRgba(const IMAGINEPLUGININTERFACE *iface, uint32_t w, uint32_t h);
