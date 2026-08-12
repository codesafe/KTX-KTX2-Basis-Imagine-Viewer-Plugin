#include "Common.h"

#include "basisu_transcoder.h"

// .basis (Basis Universal 원본 컨테이너)

namespace
{

bool IsHdrFormat(basist::basis_tex_format fmt)
{
	return basist::basis_tex_format_is_hdr(fmt);
}

} // namespace

BOOL IMAGINEAPI checkBasis(IMAGINEPLUGINFILEINFOTABLE *fileInfoTable, IMAGINELOADPARAM *loadParam, int flags)
{
	(void)fileInfoTable; (void)flags;

	if (loadParam->length < sizeof(basist::basis_file_header))
		return FALSE;

	EnsureBasisTranscoderInit();
	basist::basisu_transcoder transcoder;
	return transcoder.validate_header(loadParam->buffer, (uint32_t)loadParam->length) ? TRUE : FALSE;
}

LPIMAGINEBITMAP IMAGINEAPI loadBasis(IMAGINEPLUGINFILEINFOTABLE *fileInfoTable, IMAGINELOADPARAM *loadParam, int flags)
{
	const IMAGINEPLUGININTERFACE *iface = fileInfoTable->iface;
	if (!iface)
		return NULL;

	EnsureBasisTranscoderInit();

	basist::basisu_transcoder transcoder;
	if (!transcoder.validate_header(loadParam->buffer, (uint32_t)loadParam->length))
	{
		loadParam->errorCode = IMAGINEERROR_INVALIDDATA;
		return NULL;
	}

	basist::basisu_image_info imageInfo;
	if (!transcoder.get_image_info(loadParam->buffer, (uint32_t)loadParam->length, imageInfo, 0))
	{
		loadParam->errorCode = IMAGINEERROR_INVALIDDATA;
		return NULL;
	}

	uint32_t width = imageInfo.m_orig_width;
	uint32_t height = imageInfo.m_orig_height;
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

	if (!transcoder.start_transcoding(loadParam->buffer, (uint32_t)loadParam->length))
	{
		iface->lpVtbl->Destroy(bitmap);
		loadParam->errorCode = IMAGINEERROR_INVALIDDATA;
		return NULL;
	}

	BOOL hasAlpha = imageInfo.m_alpha_flag ? TRUE : FALSE;
	bool isHdr = IsHdrFormat(transcoder.get_basis_tex_format(loadParam->buffer, (uint32_t)loadParam->length));

	uint8_t *rgba = AllocRgba(iface, width, height);
	if (!rgba)
	{
		iface->lpVtbl->Destroy(bitmap);
		loadParam->errorCode = IMAGINEERROR_OUTOFMEMORY;
		return NULL;
	}

	BOOL ok = FALSE;
	if (isHdr)
	{
		uint16_t *half = (uint16_t *)iface->lpVtbl->Alloc((int)((uint64_t)width * height * 8));
		if (half)
		{
			if (transcoder.transcode_image_level(loadParam->buffer, (uint32_t)loadParam->length, 0, 0,
			                                     half, width * height,
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
		hasAlpha = FALSE;
	}
	else
	{
		ok = transcoder.transcode_image_level(loadParam->buffer, (uint32_t)loadParam->length, 0, 0,
		                                      rgba, width * height,
		                                      basist::transcoder_texture_format::cTFRGBA32) ? TRUE : FALSE;
	}

	if (ok)
		ok = CopyRgbaToBitmap(iface, loadParam, flags, bitmap, rgba, (LONG)width, (LONG)height, hasAlpha);

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
