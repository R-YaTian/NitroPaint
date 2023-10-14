#pragma once

#include "nclr.h"

#define NCGR_TYPE_INVALID	0
#define NCGR_TYPE_NCGR		1
#define NCGR_TYPE_HUDSON	2
#define NCGR_TYPE_HUDSON2	3
#define NCGR_TYPE_NCBR      4
#define NCGR_TYPE_BIN       5
#define NCGR_TYPE_NC        6
#define NCGR_TYPE_AC        7
#define NCGR_TYPE_COMBO     8

#define GX_OBJVRAMMODE_CHAR_2D        0x000000
#define GX_OBJVRAMMODE_CHAR_1D_32K    0x000010
#define GX_OBJVRAMMODE_CHAR_1D_64K    0x100010
#define GX_OBJVRAMMODE_CHAR_1D_128K   0x200010
#define GX_OBJVRAMMODE_CHAR_1D_256K   0x300010

#define NCGR_2D(m)              ((m)==GX_OBJVRAMMODE_CHAR_2D)
#define NCGR_1D(m)              (!NCGR_2D(m))
#define NCGR_BYTE_BOUNDARY(m)   (1<<((((m)>>20)&0x7)+5))
#define NCGR_BOUNDARY(n,x)      (NCGR_BYTE_BOUNDARY((n)->mappingMode)*(x)/(((n)->nBits)<<3))

extern LPCWSTR characterFormatNames[];

typedef struct NCGR_{
	OBJECT_HEADER header;
	int nTiles;
	int tilesX;
	int tilesY;
	int mappingMode;
	int nBits;
	int tileWidth;
	char *comment;		//null terminated
	char *link;			//linked NCL file, null terminated
	unsigned char *attr; //unused by most things
	int attrWidth;		//width of ATTR
	int attrHeight;		//height of ATTR
	BYTE **tiles;
	struct COMBO2D_ *combo2d; //for combination files
} NCGR;

#include "combo2d.h"

//
// Calculates a sensible width given a character count.
//
int ChrGuessWidth(int nTiles);

//
// Initialize an NCGR structure with sensible values.
//
void ChrInit(NCGR *ncgr, int format);

//
// Determines if a byte array represents a valid Hudson character graphics file
//
int ChrIsValidHudson(unsigned char *buffer, unsigned int size);

//
// Determines if a byte array represents a valid raw character graphics file.
//
int ChrIsValidBin(unsigned char *buffer, unsigned int size);

//
// Determines if a byte array represents a valid IS-AGB-CHARACTER graphics file
//
int ChrIsValidAcg(unsigned char *buffer, unsigned int size);

//
// Get a 32-bit color render of graphics data
//
int ChrRenderCharacter(NCGR *ncgr, NCLR *nclr, int chNo, COLOR32 *out, int previewPalette, int transparent);

//
// Update the width of graphics data. Useful for bitmapped graphics.
//
void ChrSetWidth(NCGR *ncgr, int width);

//
// Read character data from binary data and store in an NCGR.
//
void ChrReadChars(NCGR *ncgr, unsigned char *buffer);

//
// Rad bitmap data from binary data and store in an NCGR.
//
void ChrReadBitmap(NCGR *ncgr, unsigned char *buffer);

//
// Read character graphics from a byte array.
//
int ChrRead(NCGR *ncgr, unsigned char *buffer, unsigned int size);

//
// Read character graphics from a file.
//
int ChrReadFile(NCGR *ncgr, LPCWSTR path);

//
// Write character data to stream.
//
void ChrWriteChars(NCGR *ncgr, BSTREAM *stream);

//
// Write character graphics to a stream.
//
int ChrWrite(NCGR *ncgr, BSTREAM *stream);

//
// Write character graphics to a file.
//
int ChrWriteFile(NCGR *ncgr, LPCWSTR name);
