#include "Common.h"

#include <string.h>

// KTX1: https://registry.khronos.org/KTX/specs/1.0/ktxspec.v1.html

namespace
{

const uint8_t KTX1_IDENTIFIER[12] = { 0xAB, 0x4B, 0x54, 0x58, 0x20, 0x31, 0x31, 0xBB, 0x0D, 0x0A, 0x1A, 0x0A };

#pragma pack(push, 1)
struct Ktx1Header
{
	uint8_t identifier[12];
	uint32_t endianness;
	uint32_t glType;
	uint32_t glTypeSize;
	uint32_t glFormat;
	uint32_t glInternalFormat;
	uint32_t glBaseInternalFormat;
	uint32_t pixelWidth;
	uint32_t pixelHeight;
	uint32_t pixelDepth;
	uint32_t numberOfArrayElements;
	uint32_t numberOfFaces;
	uint32_t numberOfMipmapLevels;
	uint32_t bytesOfKeyValueData;
};
#pragma pack(pop)

const uint32_t KTX1_ENDIAN_NATIVE = 0x04030201;
const uint32_t KTX1_ENDIAN_SWAPPED = 0x01020304;

// GL 상수
const uint32_t GL_UNSIGNED_BYTE = 0x1401;
const uint32_t GL_RED = 0x1903;
const uint32_t GL_RGB = 0x1907;
const uint32_t GL_RGBA = 0x1908;
const uint32_t GL_LUMINANCE = 0x1909;
const uint32_t GL_LUMINANCE_ALPHA = 0x190A;
const uint32_t GL_BGR = 0x80E0;
const uint32_t GL_BGRA = 0x80E1;
const uint32_t GL_RG = 0x8227;

uint32_t Swap32(uint32_t v)
{
	return (v >> 24) | ((v >> 8) & 0xFF00u) | ((v << 8) & 0xFF0000u) | (v << 24);
}

// glInternalFormat(압축) → TexFormat. sRGB 변종이면 *outSrgb를 설정한다.
TexFormat MapCompressedFormat(uint32_t internalFormat, bool *outSrgb)
{
	// ASTC LDR: 0x93B0~0x93BD가 UNORM, 0x93D0~0x93DD가 sRGB이며 둘 다 블록 크기 순서가 같다
	if (internalFormat >= 0x93B0 && internalFormat <= 0x93BD)
		return TexFormatAstcFromIndex(internalFormat - 0x93B0);
	if (internalFormat >= 0x93D0 && internalFormat <= 0x93DD)
	{
		*outSrgb = true;
		return TexFormatAstcFromIndex(internalFormat - 0x93D0);
	}

	switch (internalFormat)
	{
	case 0x8D64: return TexFormat::ETC1;                        // ETC1_RGB8_OES
	case 0x9274: case 0x9275: return TexFormat::ETC2_RGB;      // COMPRESSED_(S)RGB8_ETC2
	case 0x9276: case 0x9277: return TexFormat::ETC2_RGB_A1;   // punchthrough alpha1
	case 0x9278: case 0x9279: return TexFormat::ETC2_RGBA;     // (S)RGBA8_ETC2_EAC
	case 0x9270: return TexFormat::EAC_R11;                     // R11_EAC (unsigned)
	case 0x9272: return TexFormat::EAC_RG11;                    // RG11_EAC (unsigned)
	case 0x9271: case 0x9273: return TexFormat::Unsupported;    // signed EAC

	case 0x83F0: return TexFormat::BC1;                         // DXT1 RGB
	case 0x83F1: return TexFormat::BC1A;                        // DXT1 RGBA
	case 0x83F2: return TexFormat::BC2;                         // DXT3
	case 0x83F3: return TexFormat::BC3;                         // DXT5
	case 0x8C4C: return TexFormat::BC1;                         // SRGB_S3TC_DXT1
	case 0x8C4D: return TexFormat::BC1A;                        // SRGB_ALPHA_S3TC_DXT1
	case 0x8C4E: return TexFormat::BC2;                         // SRGB_ALPHA_S3TC_DXT3
	case 0x8C4F: return TexFormat::BC3;                         // SRGB_ALPHA_S3TC_DXT5
	case 0x8DBB: return TexFormat::BC4;                         // RED_RGTC1
	case 0x8DBD: return TexFormat::BC5;                         // RG_RGTC2
	case 0x8DBC: case 0x8DBE: return TexFormat::Unsupported;    // signed RGTC
	case 0x8E8C: case 0x8E8D: return TexFormat::BC7;            // BPTC_UNORM / SRGB
	case 0x8E8F: return TexFormat::BC6H_UF;                     // BPTC_UNSIGNED_FLOAT
	case 0x8E8E: return TexFormat::BC6H_SF;                     // BPTC_SIGNED_FLOAT
	}
	return TexFormat::Invalid;
}

// glFormat(비압축, glType=UNSIGNED_BYTE) → TexFormat
TexFormat MapUncompressedFormat(uint32_t glFormat)
{
	switch (glFormat)
	{
	case GL_RGBA: return TexFormat::RGBA8;
	case GL_RGB: return TexFormat::RGB8;
	case GL_BGRA: return TexFormat::BGRA8;
	case GL_BGR: return TexFormat::BGR8;
	case GL_RED: return TexFormat::R8;
	case GL_RG: return TexFormat::RG8;
	case GL_LUMINANCE: return TexFormat::L8;
	case GL_LUMINANCE_ALPHA: return TexFormat::LA8;
	}
	return TexFormat::Invalid;
}

bool ParseHeader(const IMAGINELOADPARAM *loadParam, Ktx1Header *outHeader)
{
	if (loadParam->length < sizeof(Ktx1Header))
		return false;

	memcpy(outHeader, loadParam->buffer, sizeof(Ktx1Header));
	if (memcmp(outHeader->identifier, KTX1_IDENTIFIER, 12) != 0)
		return false;

	if (outHeader->endianness == KTX1_ENDIAN_SWAPPED)
	{
		uint32_t *fields = &outHeader->glType;
		for (int i = 0; i < 13; i++)
			fields[i] = Swap32(fields[i]);
	}
	else if (outHeader->endianness != KTX1_ENDIAN_NATIVE)
		return false;

	return true;
}

} // namespace

BOOL IMAGINEAPI checkKtx1(IMAGINEPLUGINFILEINFOTABLE *fileInfoTable, IMAGINELOADPARAM *loadParam, int flags)
{
	(void)fileInfoTable; (void)flags;
	return (loadParam->length >= sizeof(Ktx1Header)) &&
	       (memcmp(loadParam->buffer, KTX1_IDENTIFIER, 12) == 0);
}

LPIMAGINEBITMAP IMAGINEAPI loadKtx1(IMAGINEPLUGINFILEINFOTABLE *fileInfoTable, IMAGINELOADPARAM *loadParam, int flags)
{
	const IMAGINEPLUGININTERFACE *iface = fileInfoTable->iface;
	if (!iface)
		return NULL;

	Ktx1Header header;
	if (!ParseHeader(loadParam, &header))
	{
		loadParam->errorCode = IMAGINEERROR_INVALIDDATA;
		return NULL;
	}

	uint32_t width = header.pixelWidth;
	uint32_t height = header.pixelHeight ? header.pixelHeight : 1; // 1D 텍스처
	if (!width || width > 65536 || height > 65536)
	{
		loadParam->errorCode = IMAGINEERROR_INVALIDDATA;
		return NULL;
	}

	// 포맷 판정
	TexFormat fmt;
	bool srgb = false;
	if (header.glType == 0)
	{
		fmt = MapCompressedFormat(header.glInternalFormat, &srgb);
	}
	else if (header.glType == GL_UNSIGNED_BYTE)
	{
		fmt = MapUncompressedFormat(header.glFormat);
	}
	else
	{
		fmt = TexFormat::Unsupported; // 16/32비트 채널 등
	}

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

	LPIMAGINEBITMAP bitmap = iface->lpVtbl->Create((LONG)width, (LONG)height, 32, flags);
	if (!bitmap)
	{
		loadParam->errorCode = IMAGINEERROR_OUTOFMEMORY;
		return NULL;
	}

	if (flags & IMAGINELOADPARAM_GETINFO)
		return bitmap;

	// base level(mip 0), face 0 데이터 위치 계산
	uint64_t offset = sizeof(Ktx1Header);
	if ((uint64_t)header.bytesOfKeyValueData > loadParam->length - offset)
	{
		iface->lpVtbl->Destroy(bitmap);
		loadParam->errorCode = IMAGINEERROR_INVALIDDATA;
		return NULL;
	}
	offset += header.bytesOfKeyValueData;
	if (offset + 4 > loadParam->length)
	{
		iface->lpVtbl->Destroy(bitmap);
		loadParam->errorCode = IMAGINEERROR_INVALIDDATA;
		return NULL;
	}
	// mip 0의 imageSize 필드는 건너뛰고(신뢰하지 않음) 필요 크기를 직접 계산
	offset += 4;

	uint64_t needed = 0;
	if (!TexFormatImageSize(fmt, width, height, 4, &needed) ||
	    needed > loadParam->length - offset)
	{
		iface->lpVtbl->Destroy(bitmap);
		loadParam->errorCode = IMAGINEERROR_INVALIDDATA;
		return NULL;
	}

	uint8_t *rgba = AllocRgba(iface, width, height);
	if (!rgba)
	{
		iface->lpVtbl->Destroy(bitmap);
		loadParam->errorCode = IMAGINEERROR_OUTOFMEMORY;
		return NULL;
	}

	const uint8_t *src = (const uint8_t *)loadParam->buffer + offset;
	BOOL ok = DecodeImage(fmt, src, width, height, 4, rgba, srgb) &&
	          CopyRgbaToBitmap(iface, loadParam, flags, bitmap, rgba, (LONG)width, (LONG)height, TexFormatHasAlpha(fmt));

	iface->lpVtbl->Free(rgba);

	if (!ok)
	{
		if (loadParam->errorCode == IMAGINEERROR_NOERROR)
			loadParam->errorCode = IMAGINEERROR_INVALIDDATA;
		iface->lpVtbl->Destroy(bitmap);
		return NULL;
	}
	return bitmap;
}
