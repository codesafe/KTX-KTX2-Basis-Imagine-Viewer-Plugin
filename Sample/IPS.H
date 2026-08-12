#ifndef _IPS_H_
#define _IPS_H_

#define IPS_SIGNATURE 0x1A535049

#define FILETYPE_IPS "Imagine Plugin Sample"
#define EXTENSION_IPS "IPS\0IPSF\0"

#pragma pack(1)

typedef struct IPSHEADER
{
	DWORD signature; // IPS_SIGNATURE
	DWORD width;
	DWORD height;
	DWORD bitCount;
} IPSHEADER,*LPIPSHEADER;

typedef struct IPSPALETTEENTRY
{
	BYTE red;
	BYTE green;
	BYTE blue;
	BYTE reserved;
} IPSPALETTEENTRY,*LPIPSPALETTEENTRY;

#pragma pack()

const IMAGINEFILEINFOITEM *IMAGINEAPI IPSGetFileInfoA(void);
const IMAGINEFILEINFOITEM *IMAGINEAPI IPSGetFileInfoW(void);

#endif
