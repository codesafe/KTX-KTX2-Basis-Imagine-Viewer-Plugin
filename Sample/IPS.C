#include "Main.H"

static void importPalette(PALETTEENTRY palettes[],const IPSPALETTEENTRY entries[],int numEntries)
{
	int i=0;

	for (i=0;i<numEntries;i++)
	{
		PALETTEENTRY *palette=&palettes[i];
		const IPSPALETTEENTRY *entry=&entries[i];

		palette->peRed=entry->red;
		palette->peGreen=entry->green;
		palette->peBlue=entry->blue;
		palette->peFlags=0;
	}
}

static void exportPalette(const PALETTEENTRY palettes[],IPSPALETTEENTRY entries[],int numEntries)
{
	int i=0;

	for (i=0;i<numEntries;i++)
	{
		const PALETTEENTRY *palette=&palettes[i];
		IPSPALETTEENTRY *entry=&entries[i];

		entry->red=palette->peRed;
		entry->green=palette->peGreen;
		entry->blue=palette->peBlue;
		entry->reserved=0;
	}
}

static BOOL IMAGINEAPI checkFile(IMAGINEPLUGINFILEINFOTABLE *fileInfoTable,IMAGINELOADPARAM *loadParam,int flags)
{
	BOOL isValid=FALSE;
	LPIPSHEADER header=(LPIPSHEADER)loadParam->buffer;

	if (header->signature==IPS_SIGNATURE)
		isValid=TRUE;

	return isValid;
}

static LPIMAGINEBITMAP IMAGINEAPI loadFile(IMAGINEPLUGINFILEINFOTABLE *fileInfoTable,IMAGINELOADPARAM *loadParam,int flags)
{
	LPIMAGINEBITMAP bitmap=NULL;
	const IMAGINEPLUGININTERFACE *iface=fileInfoTable->iface;
	if (iface)
	{
		LPIPSHEADER header=(LPIPSHEADER)loadParam->buffer;
		LONG width=(LONG)header->width;
		LONG height=(LONG)header->height;
		LONG bitCount=(LONG)header->bitCount;
		bitmap=iface->lpVtbl->Create(width,height,bitCount,flags);
		if (bitmap)
		{
			if (!(flags&IMAGINELOADPARAM_GETINFO))
			{
				LPBYTE buffer=(LPBYTE)(header+1);
				LONG pureWidthBytes=(width*bitCount)/8;
				BOOL keepGoing=TRUE;
				IMAGINECALLBACKPARAM callback={0};
				LONG y=0;

				if (bitCount<=8)
				{
					LPIPSPALETTEENTRY entry=(LPIPSPALETTEENTRY)buffer;
					PALETTEENTRY palette[256];

					importPalette(palette,entry,1<<bitCount);
					iface->lpVtbl->SetPalette(bitmap,palette);
					buffer=(LPBYTE)(entry+(1<<bitCount));
				}

				callback.dib=bitmap;
				callback.param=loadParam->callback.param;
				callback.current=0;
				callback.overall=height-1;

				for (y=0;(y<height)&&(keepGoing);y++)
				{
					LPBYTE scanline=iface->lpVtbl->GetLineBits(bitmap,y);

					memcpy(scanline,buffer,pureWidthBytes);

					if ((flags&IMAGINELOADPARAM_CALLBACK)&&(!loadParam->callback.proc(&callback)))
					{
						loadParam->errorCode=IMAGINEERROR_ABORTED;
						keepGoing=FALSE;
					}
					else
					{
						buffer+=pureWidthBytes;
						callback.current++;
					}
				}
			}
		}
		else
		{
			loadParam->errorCode=IMAGINEERROR_OUTOFMEMORY;
		}
	}

	return bitmap;
}

static BOOL IMAGINEAPI saveFile(IMAGINEPLUGINFILEINFOTABLE *fileInfoTable,LPIMAGINEBITMAP bitmap,IMAGINESAVEPARAM *saveParam,int flags)
{
	BOOL result=FALSE;
	const IMAGINEPLUGININTERFACE *iface=fileInfoTable->iface;
	if (iface)
	{
		LONG bitCount=iface->lpVtbl->GetBitCount(bitmap);
		switch (bitCount)
		{
		case 8:
		case 24:
			// Assume that only 8bit and 24bit colors are supported.
			break;

		default:
			saveParam->errorCode=IMAGINEERROR_COLORNOTSUPPORTED;
			break;
		}

		if (saveParam->errorCode!=IMAGINEERROR_COLORNOTSUPPORTED)
		{
			// The rest can be saved with no problems.
			if (flags&IMAGINESAVEPARAM_TEST)
			{
				result=TRUE;
			}
			else
			{
				LONG length=iface->lpVtbl->GetLength(bitmap);
				if (length)
				{
					// First argument: The size of initial allocation
					// Second argument: The size of allcation unit (When the initial allocated memory is not enough)
					// They can be any values, but they affect to performance. (Reallocation)
					saveParam->sb=iface->lpVtbl->sbAlloc(length,32768);
					if (saveParam->sb)
					{
						LONG width=iface->lpVtbl->GetWidth(bitmap);
						LONG height=iface->lpVtbl->GetHeight(bitmap);
						IPSHEADER header={0};
						BOOL keepGoing=TRUE;

						header.signature=IPS_SIGNATURE;
						header.width=(DWORD)width;
						header.height=(DWORD)height;
						header.bitCount=(DWORD)bitCount;

						if (iface->lpVtbl->sbWrite(saveParam->sb,&header,sizeof(header)))
						{
							if (bitCount<=8)
							{
								PALETTEENTRY palette[256];
								IPSPALETTEENTRY entry[256];

								iface->lpVtbl->GetPalette(bitmap,palette);
								exportPalette(palette,entry,1<<bitCount);

								if (!iface->lpVtbl->sbWrite(saveParam->sb,entry,sizeof(*entry)*(1<<bitCount)))
								{
									saveParam->errorCode=IMAGINEERROR_WRITEERROR;
									keepGoing=FALSE;
								}
							}

							if (keepGoing)
							{
								LONG pureWidthBytes=iface->lpVtbl->GetPureWidthBytes(bitmap);
								if (pureWidthBytes>0)
								{
									IMAGINECALLBACKPARAM callback={0};
									LONG y=0;

									callback.dib=bitmap;
									callback.param=saveParam->callback.param;
									callback.current=0;
									callback.overall=height-1;

									for (y=0;(y<height)&&(keepGoing);y++)
									{
										LPBYTE scanline=iface->lpVtbl->GetLineBits(bitmap,y);

										if (!iface->lpVtbl->sbWrite(saveParam->sb,scanline,pureWidthBytes))
										{
											saveParam->errorCode=IMAGINEERROR_WRITEERROR;
											keepGoing=FALSE;
										}

										if ((flags&IMAGINESAVEPARAM_CALLBACK)&&(!saveParam->callback.proc(&callback)))
										{
											saveParam->errorCode=IMAGINEERROR_ABORTED;
											keepGoing=FALSE;
										}

										callback.current++;
									}

									if (saveParam->errorCode==IMAGINEERROR_NOERROR)
										result=TRUE;
								}
								else
								{
									saveParam->errorCode=IMAGINEERROR_UNKNOWNERROR;
								}
							}
						}
						else
						{
							saveParam->errorCode=IMAGINEERROR_WRITEERROR;
						}
					}
					else
					{
						saveParam->errorCode=IMAGINEERROR_OUTOFMEMORY;
					}
				}
			}
		}
	}

	return result;
}

// file information (ANSI version)
static const IMAGINEFILEINFOITEM fileInfoItemA=
{
	checkFile,
	loadFile,
	saveFile,
	FILETYPE_IPS,
	EXTENSION_IPS,
};

// file information (UNICODE version)
static const IMAGINEFILEINFOITEM fileInfoItemW=
{
	checkFile,
	loadFile,
	saveFile,
	(LPCTSTR)UNICODE_TEXT(FILETYPE_IPS),
	(LPCTSTR)UNICODE_TEXT(EXTENSION_IPS),
};

const IMAGINEFILEINFOITEM *IMAGINEAPI IPSGetFileInfoA(void)
{
	return &fileInfoItemA;
}

const IMAGINEFILEINFOITEM *IMAGINEAPI IPSGetFileInfoW(void)
{
	return &fileInfoItemW;
}
