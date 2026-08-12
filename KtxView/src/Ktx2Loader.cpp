#include "Common.h"

#include <string.h>

#include "basisu_transcoder.h"
#include "zstd.h"

// KTX2: https://registry.khronos.org/KTX/specs/2.0/ktxspec.v2.html
// vkFormat==0 (Basis ETC1S/UASTC) → basist::ktx2_transcoder
// vkFormat!=0 (raw VkFormat, 선택적 zstd supercompression) → 자체 파싱 + 공통 디코더

namespace
{

const uint8_t KTX2_IDENTIFIER[12] = { 0xAB, 0x4B, 0x54, 0x58, 0x20, 0x32, 0x30, 0xBB, 0x0D, 0x0A, 0x1A, 0x0A };

const uint32_t KTX2_SUPERCOMPRESSION_NONE = 0;
const uint32_t KTX2_SUPERCOMPRESSION_BASISLZ = 1;
const uint32_t KTX2_SUPERCOMPRESSION_ZSTD = 2;

// VK_FORMAT_ASTC_4x4_SFLOAT_BLOCK. 이후 13개가 블록 크기 순으로 이어진다.
const uint32_t ASTC_SFLOAT_FIRST = 1000066000;

// VkFormat → TexFormat. sRGB 변종이면 *outSrgb를 설정한다.
TexFormat MapVkFormat(uint32_t vkFormat, bool *outSrgb)
{
	// ASTC LDR 157~184: 블록 크기별로 (UNORM, SRGB) 쌍이 KTX1과 같은 순서로 이어진다
	if (vkFormat >= 157 && vkFormat <= 184)
	{
		uint32_t i = vkFormat - 157;
		*outSrgb = (i & 1) != 0;
		return TexFormatAstcFromIndex(i / 2);
	}
	// ASTC HDR(SFLOAT) 1000066000~1000066013: 블록 크기당 하나씩, 같은 순서
	if (vkFormat >= ASTC_SFLOAT_FIRST && vkFormat <= ASTC_SFLOAT_FIRST + 13)
		return TexFormatAstcFromIndex(vkFormat - ASTC_SFLOAT_FIRST);

	switch (vkFormat)
	{
	case 9: case 15: return TexFormat::R8;        // R8_UNORM / R8_SRGB
	case 16: case 22: return TexFormat::RG8;      // R8G8_UNORM / SRGB
	case 23: case 29: return TexFormat::RGB8;     // R8G8B8_UNORM / SRGB
	case 30: case 36: return TexFormat::BGR8;     // B8G8R8_UNORM / SRGB
	case 37: case 43: return TexFormat::RGBA8;    // R8G8B8A8_UNORM / SRGB
	case 44: case 50: return TexFormat::BGRA8;    // B8G8R8A8_UNORM / SRGB

	case 131: case 132: return TexFormat::BC1;    // BC1_RGB_UNORM / SRGB
	case 133: case 134: return TexFormat::BC1A;   // BC1_RGBA_UNORM / SRGB
	case 135: case 136: return TexFormat::BC2;
	case 137: case 138: return TexFormat::BC3;
	case 139: return TexFormat::BC4;              // BC4_UNORM
	case 141: return TexFormat::BC5;              // BC5_UNORM
	case 140: case 142: return TexFormat::Unsupported; // BC4/BC5 SNORM
	case 143: return TexFormat::BC6H_UF;
	case 144: return TexFormat::BC6H_SF;
	case 145: case 146: return TexFormat::BC7;

	case 147: case 148: return TexFormat::ETC2_RGB;    // ETC2_R8G8B8_UNORM / SRGB
	case 149: case 150: return TexFormat::ETC2_RGB_A1;
	case 151: case 152: return TexFormat::ETC2_RGBA;
	case 153: return TexFormat::EAC_R11;               // EAC_R11_UNORM
	case 155: return TexFormat::EAC_RG11;              // EAC_R11G11_UNORM
	case 154: case 156: return TexFormat::Unsupported; // EAC SNORM
	}
	return TexFormat::Invalid;
}

void FailBitmap(const IMAGINEPLUGININTERFACE *iface, LPIMAGINEBITMAP bitmap)
{
	if (bitmap)
		iface->lpVtbl->Destroy(bitmap);
}

// Basis(ETC1S/UASTC) 경로
LPIMAGINEBITMAP LoadBasisKtx2(const IMAGINEPLUGININTERFACE *iface, IMAGINELOADPARAM *loadParam, int flags)
{
	EnsureBasisTranscoderInit();

	basist::ktx2_transcoder transcoder;
	if (!transcoder.init(loadParam->buffer, (uint32_t)loadParam->length))
	{
		loadParam->errorCode = IMAGINEERROR_INVALIDDATA;
		return NULL;
	}

	uint32_t width = transcoder.get_width();
	uint32_t height = transcoder.get_height();
	if (!width || !height || width > 65536 || height > 65536)
	{
		loadParam->errorCode = IMAGINEERROR_INVALIDDATA;
		return NULL;
	}

	LPIMAGINEBITMAP bitmap = iface->lpVtbl->Create((LONG)width, (LONG)height, 32, flags);
	if (!bitmap)
	{
		loadParam->errorCode = IMAGINEERROR_OUTOFMEMORY;
		return NULL;
	}
	if (flags & IMAGINELOADPARAM_GETINFO)
		return bitmap;

	if (!transcoder.start_transcoding())
	{
		FailBitmap(iface, bitmap);
		loadParam->errorCode = IMAGINEERROR_INVALIDDATA;
		return NULL;
	}

	BOOL hasAlpha = transcoder.get_has_alpha() ? TRUE : FALSE;
	uint8_t *rgba = AllocRgba(iface, width, height);
	if (!rgba)
	{
		FailBitmap(iface, bitmap);
		loadParam->errorCode = IMAGINEERROR_OUTOFMEMORY;
		return NULL;
	}

	BOOL ok = FALSE;
	if (transcoder.is_hdr())
	{
		// HDR → half RGBA로 트랜스코딩 후 클램프해 8비트로
		uint16_t *half = (uint16_t *)iface->lpVtbl->Alloc((int)((uint64_t)width * height * 8));
		if (half)
		{
			if (transcoder.transcode_image_level(0, 0, 0, half, width * height,
			                                     basist::transcoder_texture_format::cTFRGBA_HALF))
			{
				for (uint64_t i = 0; i < (uint64_t)width * height; i++)
				{
					rgba[i * 4 + 0] = HalfToU8(half[i * 4 + 0]);
					rgba[i * 4 + 1] = HalfToU8(half[i * 4 + 1]);
					rgba[i * 4 + 2] = HalfToU8(half[i * 4 + 2]);
					rgba[i * 4 + 3] = HalfToU8(half[i * 4 + 3]);
				}
				ok = TRUE;
			}
			iface->lpVtbl->Free(half);
		}
		hasAlpha = FALSE; // HDR은 알파 없음 (A=1.0)
	}
	else
	{
		ok = transcoder.transcode_image_level(0, 0, 0, rgba, width * height,
		                                      basist::transcoder_texture_format::cTFRGBA32) ? TRUE : FALSE;
	}

	if (ok)
		ok = CopyRgbaToBitmap(iface, loadParam, flags, bitmap, rgba, (LONG)width, (LONG)height, hasAlpha);

	iface->lpVtbl->Free(rgba);

	if (!ok)
	{
		if (loadParam->errorCode == IMAGINEERROR_NOERROR)
			loadParam->errorCode = IMAGINEERROR_INVALIDDATA;
		FailBitmap(iface, bitmap);
		return NULL;
	}
	return bitmap;
}

// raw VkFormat 경로
LPIMAGINEBITMAP LoadRawKtx2(const IMAGINEPLUGININTERFACE *iface, IMAGINELOADPARAM *loadParam, int flags,
                            const basist::ktx2_header &header)
{
	uint32_t width = header.m_pixel_width;
	uint32_t height = header.m_pixel_height ? (uint32_t)header.m_pixel_height : 1;
	if (!width || width > 65536 || height > 65536)
	{
		loadParam->errorCode = IMAGINEERROR_INVALIDDATA;
		return NULL;
	}

	bool srgb = false;
	TexFormat fmt = MapVkFormat(header.m_vk_format, &srgb);
	if (fmt == TexFormat::Invalid)
	{
		loadParam->errorCode = IMAGINEERROR_INVALIDDATA;
		return NULL;
	}
	if (fmt == TexFormat::Unsupported)
	{
		loadParam->errorCode = IMAGINEERROR_UNSUPPORTEDTYPE;
		return NULL;
	}

	uint32_t scheme = header.m_supercompression_scheme;
	if (scheme != KTX2_SUPERCOMPRESSION_NONE && scheme != KTX2_SUPERCOMPRESSION_ZSTD)
	{
		// BasisLZ는 vkFormat==0에서만 유효, zlib 등은 미지원
		loadParam->errorCode = IMAGINEERROR_UNSUPPORTEDTYPE;
		return NULL;
	}

	// 레벨 인덱스: 헤더 바로 뒤, 레벨당 24바이트
	uint32_t levelCount = header.m_level_count ? (uint32_t)header.m_level_count : 1;
	uint64_t indexEnd = sizeof(basist::ktx2_header) + (uint64_t)levelCount * sizeof(basist::ktx2_level_index);
	if (levelCount > 32 || indexEnd > loadParam->length)
	{
		loadParam->errorCode = IMAGINEERROR_INVALIDDATA;
		return NULL;
	}
	basist::ktx2_level_index level0;
	memcpy(&level0, (const uint8_t *)loadParam->buffer + sizeof(basist::ktx2_header), sizeof(level0));

	uint64_t byteOffset = level0.m_byte_offset.get_uint64();
	uint64_t byteLength = level0.m_byte_length.get_uint64();
	if (!byteLength || byteOffset > loadParam->length || byteLength > loadParam->length - byteOffset)
	{
		loadParam->errorCode = IMAGINEERROR_INVALIDDATA;
		return NULL;
	}

	uint64_t faceSize = 0;
	if (!TexFormatImageSize(fmt, width, height, 1, &faceSize)) // KTX2는 행 패딩 없음
	{
		loadParam->errorCode = IMAGINEERROR_INVALIDDATA;
		return NULL;
	}

	LPIMAGINEBITMAP bitmap = iface->lpVtbl->Create((LONG)width, (LONG)height, 32, flags);
	if (!bitmap)
	{
		loadParam->errorCode = IMAGINEERROR_OUTOFMEMORY;
		return NULL;
	}
	if (flags & IMAGINELOADPARAM_GETINFO)
		return bitmap;

	// 레벨 0 데이터 확보 (zstd면 해제)
	const uint8_t *levelData = (const uint8_t *)loadParam->buffer + byteOffset;
	uint64_t levelSize = byteLength;
	uint8_t *decompressed = NULL;

	if (scheme == KTX2_SUPERCOMPRESSION_ZSTD)
	{
		uint64_t rawSize = level0.m_uncompressed_byte_length.get_uint64();
		if (!rawSize || rawSize > 0x40000000ull)
		{
			FailBitmap(iface, bitmap);
			loadParam->errorCode = IMAGINEERROR_INVALIDDATA;
			return NULL;
		}
		decompressed = (uint8_t *)iface->lpVtbl->Alloc((int)rawSize);
		if (!decompressed)
		{
			FailBitmap(iface, bitmap);
			loadParam->errorCode = IMAGINEERROR_OUTOFMEMORY;
			return NULL;
		}
		size_t result = ZSTD_decompress(decompressed, (size_t)rawSize, levelData, (size_t)byteLength);
		if (ZSTD_isError(result) || result != rawSize)
		{
			iface->lpVtbl->Free(decompressed);
			FailBitmap(iface, bitmap);
			loadParam->errorCode = IMAGINEERROR_INVALIDDATA;
			return NULL;
		}
		levelData = decompressed;
		levelSize = rawSize;
	}

	BOOL ok = FALSE;
	uint8_t *rgba = NULL;
	if (faceSize <= levelSize) // 첫 layer/face 이미지만 사용
	{
		rgba = AllocRgba(iface, width, height);
		if (rgba)
			ok = DecodeImage(fmt, levelData, width, height, 1, rgba, srgb) &&
			     CopyRgbaToBitmap(iface, loadParam, flags, bitmap, rgba, (LONG)width, (LONG)height, TexFormatHasAlpha(fmt));
		else
			loadParam->errorCode = IMAGINEERROR_OUTOFMEMORY;
	}

	if (rgba)
		iface->lpVtbl->Free(rgba);
	if (decompressed)
		iface->lpVtbl->Free(decompressed);

	if (!ok)
	{
		if (loadParam->errorCode == IMAGINEERROR_NOERROR)
			loadParam->errorCode = IMAGINEERROR_INVALIDDATA;
		FailBitmap(iface, bitmap);
		return NULL;
	}
	return bitmap;
}

} // namespace

BOOL IMAGINEAPI checkKtx2(IMAGINEPLUGINFILEINFOTABLE *fileInfoTable, IMAGINELOADPARAM *loadParam, int flags)
{
	(void)fileInfoTable; (void)flags;
	return (loadParam->length >= sizeof(basist::ktx2_header)) &&
	       (memcmp(loadParam->buffer, KTX2_IDENTIFIER, 12) == 0);
}

LPIMAGINEBITMAP IMAGINEAPI loadKtx2(IMAGINEPLUGINFILEINFOTABLE *fileInfoTable, IMAGINELOADPARAM *loadParam, int flags)
{
	const IMAGINEPLUGININTERFACE *iface = fileInfoTable->iface;
	if (!iface)
		return NULL;

	if (loadParam->length < sizeof(basist::ktx2_header) ||
	    memcmp(loadParam->buffer, KTX2_IDENTIFIER, 12) != 0)
	{
		loadParam->errorCode = IMAGINEERROR_INVALIDDATA;
		return NULL;
	}

	basist::ktx2_header header;
	memcpy(&header, loadParam->buffer, sizeof(header));

	// vkFormat==0(Basis ETC1S/UASTC)만 ktx2_transcoder로 보낸다.
	// raw ASTC는 LDR/HDR 모두 자체 디코더로 처리한다 — ktx2_transcoder는 ASTC SFLOAT 중
	// 4x4와 6x6만 받아들여서 8x8 SFLOAT 같은 파일을 놓친다.
	if (header.m_vk_format == basist::KTX2_VK_FORMAT_UNDEFINED)
		return LoadBasisKtx2(iface, loadParam, flags);
	return LoadRawKtx2(iface, loadParam, flags, header);
}
