#include "Common.h"

#include <string.h>

#define BCDEC_IMPLEMENTATION
#include "bcdec.h"
#define ETCDEC_IMPLEMENTATION
#include "etcdec.h"

// ASTC 디코더는 basisu_transcoder.cpp가 구현부를 컴파일하고
// basisu_transcoder_init()이 dequant 테이블까지 초기화한다.
// basisu_astc_helpers.h는 단독 헤더가 아니라 basisu의 컨테이너와 assert를 요구한다.
#include <assert.h>
#include "basisu_transcoder.h"
#include "basisu_astc_helpers.h"

// TexFormat::ASTC_4x4 ~ ASTC_12x12 순서와 일치 (KTX/Vulkan의 ASTC 열거 순서)
static const uint8_t g_astcBlockDims[14][2] =
{
	{ 4, 4 }, { 5, 4 }, { 5, 5 }, { 6, 5 }, { 6, 6 },
	{ 8, 5 }, { 8, 6 }, { 8, 8 }, { 10, 5 }, { 10, 6 },
	{ 10, 8 }, { 10, 10 }, { 12, 10 }, { 12, 12 },
};

static bool IsAstc(TexFormat f)
{
	return f >= TexFormat::ASTC_4x4 && f <= TexFormat::ASTC_12x12;
}

TexFormat TexFormatAstcFromIndex(uint32_t index)
{
	if (index >= 14)
		return TexFormat::Invalid;
	return (TexFormat)((int)TexFormat::ASTC_4x4 + (int)index);
}

void TexFormatBlockDims(TexFormat f, uint32_t *blockW, uint32_t *blockH)
{
	if (IsAstc(f))
	{
		uint32_t i = (uint32_t)((int)f - (int)TexFormat::ASTC_4x4);
		*blockW = g_astcBlockDims[i][0];
		*blockH = g_astcBlockDims[i][1];
	}
	else if (TexFormatIsBlock(f))
	{
		*blockW = *blockH = 4;
	}
	else
	{
		*blockW = *blockH = 1;
	}
}

bool TexFormatIsBlock(TexFormat f)
{
	switch (f)
	{
	case TexFormat::R8: case TexFormat::RG8: case TexFormat::L8: case TexFormat::LA8:
	case TexFormat::RGB8: case TexFormat::BGR8: case TexFormat::RGBA8: case TexFormat::BGRA8:
		return false;
	default:
		return true;
	}
}

bool TexFormatHasAlpha(TexFormat f)
{
	switch (f)
	{
	case TexFormat::LA8: case TexFormat::RGBA8: case TexFormat::BGRA8:
	case TexFormat::BC1A: case TexFormat::BC2: case TexFormat::BC3: case TexFormat::BC7:
	case TexFormat::ETC2_RGB_A1: case TexFormat::ETC2_RGBA:
		return true;
	default:
		// ASTC는 포맷만으로 알파 유무를 알 수 없다(블록마다 다름). 불투명 블록은
		// A=255로 디코딩되므로 알파를 켜두는 쪽이 안전하다.
		return IsAstc(f);
	}
}

static uint32_t PixelBytes(TexFormat f)
{
	switch (f)
	{
	case TexFormat::R8: case TexFormat::L8: return 1;
	case TexFormat::RG8: case TexFormat::LA8: return 2;
	case TexFormat::RGB8: case TexFormat::BGR8: return 3;
	case TexFormat::RGBA8: case TexFormat::BGRA8: return 4;
	default: return 0;
	}
}

static uint32_t BlockBytes(TexFormat f)
{
	switch (f)
	{
	case TexFormat::BC1: case TexFormat::BC1A: case TexFormat::BC4:
	case TexFormat::ETC1: case TexFormat::ETC2_RGB: case TexFormat::ETC2_RGB_A1:
	case TexFormat::EAC_R11:
		return 8;
	case TexFormat::BC2: case TexFormat::BC3: case TexFormat::BC5:
	case TexFormat::BC6H_UF: case TexFormat::BC6H_SF: case TexFormat::BC7:
	case TexFormat::ETC2_RGBA: case TexFormat::EAC_RG11:
		return 16;
	default:
		return IsAstc(f) ? 16 : 0; // ASTC는 블록 크기와 무관하게 항상 128비트
	}
}

bool TexFormatImageSize(TexFormat f, uint32_t w, uint32_t h, uint32_t rowAlign, uint64_t *outSize)
{
	if (!w || !h)
		return false;

	if (TexFormatIsBlock(f))
	{
		uint32_t blockBytes = BlockBytes(f);
		if (!blockBytes)
			return false;
		uint32_t blockW, blockH;
		TexFormatBlockDims(f, &blockW, &blockH);
		uint64_t bw = (w + blockW - 1) / blockW, bh = (h + blockH - 1) / blockH;
		*outSize = bw * bh * blockBytes;
	}
	else
	{
		uint32_t bpp = PixelBytes(f);
		if (!bpp || !rowAlign)
			return false;
		uint64_t rowBytes = ((uint64_t)w * bpp + rowAlign - 1) / rowAlign * rowAlign;
		*outSize = rowBytes * h;
	}
	return *outSize <= 0x40000000ull; // 1GB 초과는 손상 파일로 간주
}

uint8_t HalfToU8(uint16_t h)
{
	uint32_t sign = (h >> 15) & 1;
	uint32_t exp = (h >> 10) & 0x1F;
	uint32_t man = h & 0x3FF;
	float value;

	if (exp == 0)
		value = (float)man * (1.0f / 16777216.0f); // subnormal: man * 2^-24
	else if (exp == 31)
		value = man ? 0.0f : 3.4e38f; // NaN → 0, Inf → 큰 값(클램프됨)
	else
	{
		value = (float)(man + 1024);
		int e = (int)exp - 25; // value * 2^(exp-15-10)
		while (e > 0) { value *= 2.0f; e--; }
		while (e < 0) { value *= 0.5f; e++; }
	}
	if (sign)
		value = 0.0f;
	if (value >= 1.0f)
		return 255;
	return (uint8_t)(value * 255.0f + 0.5f);
}

uint8_t *AllocRgba(const IMAGINEPLUGININTERFACE *iface, uint32_t w, uint32_t h)
{
	uint64_t size = (uint64_t)w * h * 4;
	if (!size || size > 0x40000000ull)
		return NULL;
	return (uint8_t *)iface->lpVtbl->Alloc((int)size);
}

// 블록 하나를 디코딩한 tmpRgba(blockW×blockH, 행 피치 blockW*4)를 이미지에 복사.
// 이미지 경계를 넘는 블록은 잘라서 복사한다.
static void ExpandBlock(uint8_t *dst, uint32_t w, uint32_t h, uint32_t px, uint32_t py,
                        const uint8_t *tmpRgba, uint32_t blockW, uint32_t blockH)
{
	uint32_t cw = (w - px < blockW) ? (w - px) : blockW;
	uint32_t ch = (h - py < blockH) ? (h - py) : blockH;
	for (uint32_t r = 0; r < ch; r++)
		memcpy(dst + ((uint64_t)(py + r) * w + px) * 4, tmpRgba + (uint64_t)r * blockW * 4, cw * 4);
}

static bool DecodeBlockImage(TexFormat f, const uint8_t *src, uint32_t w, uint32_t h, uint8_t *dst, bool srgb)
{
	uint32_t blockW, blockH;
	TexFormatBlockDims(f, &blockW, &blockH);
	uint32_t bw = (w + blockW - 1) / blockW, bh = (h + blockH - 1) / blockH;
	uint32_t blockBytes = BlockBytes(f);
	bool isSignedBc6h = (f == TexFormat::BC6H_SF);

	if (IsAstc(f))
		EnsureBasisTranscoderInit(); // astc_helpers의 dequant 테이블 초기화

	for (uint32_t by = 0; by < bh; by++)
	{
		for (uint32_t bx = 0; bx < bw; bx++)
		{
			const uint8_t *blk = src + ((uint64_t)by * bw + bx) * blockBytes;
			uint8_t tmp[12 * 12 * 4]; // 최대 블록(12x12) 기준

			if (IsAstc(f))
			{
				astc_helpers::log_astc_block logBlk;
				if (!astc_helpers::unpack_block(blk, logBlk, blockW, blockH))
					return false;

				// m_num_partitions/m_color_endpoint_modes는 단색·에러 블록에서 유효하지 않으므로
				// 그 두 경우를 먼저 걸러낸 뒤에 CEM을 본다.
				bool isHdr = logBlk.m_solid_color_flag_hdr ||
				             (!logBlk.m_solid_color_flag_ldr && !logBlk.m_error_flag &&
				              astc_helpers::block_has_any_hdr_cems(logBlk));

				if (isHdr)
				{
					// HDR CEM은 8비트 경로로 디코딩할 수 없다. half로 받아 클램프한다.
					uint16_t half[12 * 12 * 4];
					astc_helpers::decode_block(logBlk, half, blockW, blockH, astc_helpers::cDecodeModeHDR16);
					for (uint32_t i = 0; i < blockW * blockH * 4; i++)
						tmp[i] = HalfToU8(half[i]);
				}
				else
				{
					// 디코드에 실패하면 decode_block이 에러 블록(마젠타)을 써 두므로,
					// 해당 블록만 눈에 띄게 남기고 이미지 전체는 계속 진행한다.
					astc_helpers::decode_block(logBlk, tmp, blockW, blockH,
					                           srgb ? astc_helpers::cDecodeModeSRGB8
					                                : astc_helpers::cDecodeModeLDR8);
				}

				ExpandBlock(dst, w, h, bx * blockW, by * blockH, tmp, blockW, blockH);
				continue;
			}

			switch (f)
			{
			case TexFormat::BC1:
			case TexFormat::BC1A:
				bcdec_bc1(blk, tmp, 16);
				break;
			case TexFormat::BC2:
				bcdec_bc2(blk, tmp, 16);
				break;
			case TexFormat::BC3:
				bcdec_bc3(blk, tmp, 16);
				break;
			case TexFormat::BC7:
				bcdec_bc7(blk, tmp, 16);
				break;
			case TexFormat::BC4:
			{
				uint8_t t[16];
				bcdec_bc4(blk, t, 4);
				for (int i = 0; i < 16; i++)
				{
					tmp[i * 4 + 0] = tmp[i * 4 + 1] = tmp[i * 4 + 2] = t[i];
					tmp[i * 4 + 3] = 0xFF;
				}
				break;
			}
			case TexFormat::BC5:
			{
				uint8_t t[32];
				bcdec_bc5(blk, t, 8);
				for (int i = 0; i < 16; i++)
				{
					tmp[i * 4 + 0] = t[i * 2 + 0];
					tmp[i * 4 + 1] = t[i * 2 + 1];
					tmp[i * 4 + 2] = 0;
					tmp[i * 4 + 3] = 0xFF;
				}
				break;
			}
			case TexFormat::BC6H_UF:
			case TexFormat::BC6H_SF:
			{
				unsigned short t[16 * 3];
				bcdec_bc6h_half(blk, t, 4 * 3, isSignedBc6h ? 1 : 0);
				for (int i = 0; i < 16; i++)
				{
					tmp[i * 4 + 0] = HalfToU8(t[i * 3 + 0]);
					tmp[i * 4 + 1] = HalfToU8(t[i * 3 + 1]);
					tmp[i * 4 + 2] = HalfToU8(t[i * 3 + 2]);
					tmp[i * 4 + 3] = 0xFF;
				}
				break;
			}
			case TexFormat::ETC1:
			case TexFormat::ETC2_RGB:
				etcdec_etc_rgb(blk, tmp, 16);
				break;
			case TexFormat::ETC2_RGB_A1:
				etcdec_etc_rgb_a1(blk, tmp, 16);
				break;
			case TexFormat::ETC2_RGBA:
				etcdec_eac_rgba(blk, tmp, 16);
				break;
			case TexFormat::EAC_R11:
			{
				uint16_t t[16];
				etcdec_eac_r11_u16(blk, t, 4 * 2);
				for (int i = 0; i < 16; i++)
				{
					uint8_t v = (uint8_t)(t[i] >> 8);
					tmp[i * 4 + 0] = tmp[i * 4 + 1] = tmp[i * 4 + 2] = v;
					tmp[i * 4 + 3] = 0xFF;
				}
				break;
			}
			case TexFormat::EAC_RG11:
			{
				uint16_t t[16 * 2];
				etcdec_eac_rg11_u16(blk, t, 4 * 4);
				for (int i = 0; i < 16; i++)
				{
					tmp[i * 4 + 0] = (uint8_t)(t[i * 2 + 0] >> 8);
					tmp[i * 4 + 1] = (uint8_t)(t[i * 2 + 1] >> 8);
					tmp[i * 4 + 2] = 0;
					tmp[i * 4 + 3] = 0xFF;
				}
				break;
			}
			default:
				return false;
			}

			ExpandBlock(dst, w, h, bx * blockW, by * blockH, tmp, blockW, blockH);
		}
	}
	return true;
}

static bool DecodeUncompressedImage(TexFormat f, const uint8_t *src, uint32_t w, uint32_t h, uint32_t rowAlign, uint8_t *dst)
{
	uint32_t bpp = PixelBytes(f);
	uint64_t rowBytes = ((uint64_t)w * bpp + rowAlign - 1) / rowAlign * rowAlign;

	for (uint32_t y = 0; y < h; y++)
	{
		const uint8_t *s = src + y * rowBytes;
		uint8_t *d = dst + (uint64_t)y * w * 4;

		switch (f)
		{
		case TexFormat::RGBA8:
			memcpy(d, s, (size_t)w * 4);
			break;
		case TexFormat::BGRA8:
			for (uint32_t x = 0; x < w; x++, s += 4, d += 4)
			{
				d[0] = s[2]; d[1] = s[1]; d[2] = s[0]; d[3] = s[3];
			}
			break;
		case TexFormat::RGB8:
			for (uint32_t x = 0; x < w; x++, s += 3, d += 4)
			{
				d[0] = s[0]; d[1] = s[1]; d[2] = s[2]; d[3] = 0xFF;
			}
			break;
		case TexFormat::BGR8:
			for (uint32_t x = 0; x < w; x++, s += 3, d += 4)
			{
				d[0] = s[2]; d[1] = s[1]; d[2] = s[0]; d[3] = 0xFF;
			}
			break;
		case TexFormat::R8:
			for (uint32_t x = 0; x < w; x++, s += 1, d += 4)
			{
				d[0] = s[0]; d[1] = 0; d[2] = 0; d[3] = 0xFF;
			}
			break;
		case TexFormat::L8:
			for (uint32_t x = 0; x < w; x++, s += 1, d += 4)
			{
				d[0] = d[1] = d[2] = s[0]; d[3] = 0xFF;
			}
			break;
		case TexFormat::RG8:
			for (uint32_t x = 0; x < w; x++, s += 2, d += 4)
			{
				d[0] = s[0]; d[1] = s[1]; d[2] = 0; d[3] = 0xFF;
			}
			break;
		case TexFormat::LA8:
			for (uint32_t x = 0; x < w; x++, s += 2, d += 4)
			{
				d[0] = d[1] = d[2] = s[0]; d[3] = s[1];
			}
			break;
		default:
			return false;
		}
	}
	return true;
}

bool DecodeImage(TexFormat f, const uint8_t *src, uint32_t w, uint32_t h, uint32_t rowAlign, uint8_t *dstRgba,
                 bool srgb)
{
	if (TexFormatIsBlock(f))
		return DecodeBlockImage(f, src, w, h, dstRgba, srgb);
	return DecodeUncompressedImage(f, src, w, h, rowAlign, dstRgba);
}

BOOL CopyRgbaToBitmap(const IMAGINEPLUGININTERFACE *iface, IMAGINELOADPARAM *loadParam, int flags,
                      LPIMAGINEBITMAP bitmap, const uint8_t *rgba, LONG width, LONG height, BOOL hasAlpha)
{
	IMAGINECALLBACKPARAM callback;
	callback.dib = bitmap;
	callback.param = loadParam->callback.param;
	callback.current = 0;
	callback.overall = height - 1;

	for (LONG y = 0; y < height; y++)
	{
		LPBYTE scanline = (LPBYTE)iface->lpVtbl->GetLineBits(bitmap, y);
		const uint8_t *s = rgba + (uint64_t)y * width * 4;

		if (hasAlpha)
		{
			for (LONG x = 0; x < width; x++, s += 4, scanline += 4)
			{
				scanline[0] = s[2]; // B
				scanline[1] = s[1]; // G
				scanline[2] = s[0]; // R
				scanline[3] = s[3]; // A
			}
		}
		else
		{
			for (LONG x = 0; x < width; x++, s += 4, scanline += 4)
			{
				scanline[0] = s[2];
				scanline[1] = s[1];
				scanline[2] = s[0];
				scanline[3] = 0xFF;
			}
		}

		if ((flags & IMAGINELOADPARAM_CALLBACK) && loadParam->callback.proc)
		{
			if (!loadParam->callback.proc(&callback))
			{
				loadParam->errorCode = IMAGINEERROR_ABORTED;
				return FALSE;
			}
			callback.current++;
		}
	}

	if (hasAlpha)
	{
		iface->lpVtbl->SetTransMethod(bitmap, IMAGINETRANSMETHOD_ALPHABLEND);
		loadParam->caps |= IMAGINECAPS_ALPHA;
	}
	return TRUE;
}
