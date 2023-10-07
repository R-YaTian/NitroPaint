#include <Windows.h>
#include "color.h"
#include "combo2d.h"
#include "g2dfile.h"

#include "nclr.h"
#include "ncgr.h"
#include "nscr.h"

typedef struct BANNER_INFO_ {
	int version;
	WCHAR titleJp[128];
	WCHAR titleEn[128];
	WCHAR titleFr[128];
	WCHAR titleGe[128];
	WCHAR titleIt[128];
	WCHAR titleSp[128];
	WCHAR titleCn[128];
	WCHAR titleHn[128];
} BANNER_INFO;

void combo2dInit(COMBO2D *combo, int format) {
	fileInitCommon(&combo->header, FILE_TYPE_COMBO2D, format);
}

int combo2dCount(COMBO2D *combo, int type) {
	int count = 0;
	for (int i = 0; i < combo->nLinks; i++) {
		OBJECT_HEADER *header = combo->links[i];
		if (header->type == type) count++;
	}
	return count;
}

OBJECT_HEADER *combo2dGet(COMBO2D *combo, int type, int index) {
	//keep track of number of objects of this type we've counted
	int nCounted = 0;
	for (int i = 0; i < combo->nLinks; i++) {
		OBJECT_HEADER *object = combo->links[i];
		if (object->type != type) continue;

		if (nCounted == index) return object;
		nCounted++;
	}
	return NULL;
}

void combo2dLink(COMBO2D *combo, OBJECT_HEADER *object) {
	combo->nLinks++;
	combo->links = (OBJECT_HEADER **) realloc(combo->links, combo->nLinks * sizeof(OBJECT_HEADER *));
	combo->links[combo->nLinks - 1] = object;
}

void combo2dUnlink(COMBO2D *combo, OBJECT_HEADER *object) {
	for (int i = 0; i < combo->nLinks; i++) {
		if (combo->links[i] != object) continue;

		//remove
		memmove(combo->links + i, combo->links + i + 1, (combo->nLinks - i - 1) * sizeof(OBJECT_HEADER *));
		combo->nLinks--;
		combo->links = (OBJECT_HEADER **) realloc(combo->links, combo->nLinks * sizeof(OBJECT_HEADER *));
		return;
	}
}

int combo2dFormatHasPalette(int format) {
	return format == COMBO2D_TYPE_BANNER
		|| format == COMBO2D_TYPE_TIMEACE
		|| format == COMBO2D_TYPE_5BG
		|| format == COMBO2D_TYPE_MBB;
}

int combo2dFormatHasCharacter(int format) {
	return format == COMBO2D_TYPE_BANNER
		|| format == COMBO2D_TYPE_TIMEACE
		|| format == COMBO2D_TYPE_5BG
		|| format == COMBO2D_TYPE_MBB;
}

int combo2dFormatHasScreen(int format) {
	return format == COMBO2D_TYPE_TIMEACE
		|| format == COMBO2D_TYPE_5BG;
}

int combo2dCanSave(COMBO2D *combo) {
	int nPalettes = combo2dCount(combo, FILE_TYPE_PALETTE);
	int nCharacters = combo2dCount(combo, FILE_TYPE_CHARACTER);
	int nScreens = combo2dCount(combo, FILE_TYPE_SCREEN);
	if (combo2dFormatHasPalette(combo->header.format) && nPalettes == 0) return 0;
	if (combo2dFormatHasCharacter(combo->header.format) && nCharacters == 0) return 0;
	if (combo2dFormatHasScreen(combo->header.format) && nScreens == 0) return 0;
	return 1;
}

void combo2dFree(COMBO2D *combo) {
	//free all links
	for (int i = 0; i < combo->nLinks; i++) {
		OBJECT_HEADER *object = combo->links[i];
		fileFree(object);
		free(object);
	}
	if (combo->links != NULL) {
		free(combo->links);
	}
	combo->links = NULL;
	combo->nLinks = 0;

	if (combo->extraData != NULL) {
		if (combo->header.format == COMBO2D_TYPE_DATAFILE) {
			DATAFILECOMBO *dfc = (DATAFILECOMBO *) combo->extraData;
			if (dfc->data != NULL) free(dfc->data);
			dfc->data = NULL;
		}
		free(combo->extraData);
		combo->extraData = NULL;
	}
}

int combo2dIsValidTimeAce(BYTE *file, int size) {
	//file must be big enough for 12 bytes plus palette (512 bytes) and screen (2048 bytes).
	if (size < 0xA0C) return 0;

	//validate fields
	int bitness = *(int *) file;
	if (bitness != 0 && bitness != 1) return 0;
	int nChars = *(int *) (file + 0xA08);
	int charSize = bitness ? 0x40 : 0x20;
	if (0xA0C + nChars * charSize != size) return 0;

	for (int i = 0; i < 256; i++) {
		COLOR c = ((COLOR *) (file + 4))[i];
		if (c & 0x8000) return 0;
	}
	return 1;
}

int combo2dIsValidMbb(BYTE *file, unsigned int size) {
	if (size < 0x74) return 0;

	//check that offsets aren't out of bounds
	uint32_t palofs = *(uint32_t *) (file + 0x00);
	uint32_t chrofs = *(uint32_t *) (file + 0x04);
	uint32_t *scrofs = (uint32_t *) (file + 0x08);
	if (palofs < 0x70 || palofs >= size) return 0;
	if (chrofs < 0x70 || chrofs >= size) return 0;

	//check palette size
	if (palofs + 0x200 > size) return 0; //palette always 256 colors

	//check that at least one screen is valid
	int scrBitmap = 0;
	for (int i = 0; i < 4; i++) {
		uint32_t scrnofs = scrofs[i];
		if (scrnofs >= size) return 0;

		if (scrnofs == 0) continue;
		scrBitmap |= 1 << i;
	}
	if (scrBitmap == 0) return 0;

	//validate screen datas
	for (int i = 0; i < 4; i++) {
		if (!(scrBitmap & (1 << i))) continue;

		uint16_t scrWidth = *(uint16_t *) (file + 0x18 + i * 0x10 + 0x8);
		uint16_t scrHeight = *(uint16_t *) (file + 0x18 + i * 0x10 + 0xA);
		if ((scrWidth & 7) || (scrHeight & 7) || (scrWidth == 0) || (scrHeight == 0)) return 0;

		int screenSize = (scrWidth / 8) * (scrHeight / 8) * 2;
		if (scrofs[i] + screenSize > size) return 0;
	}

	return 1;
}

int combo2dIsValidBanner(BYTE *file, int size) {
	if (size < 0x840) return 0;

	int version = *(unsigned short *) file;
	int crcA = *(unsigned short *) (file + 2);
	int crcB = *(unsigned short *) (file + 4);
	int crcC = *(unsigned short *) (file + 6);
	int crcD = *(unsigned short *) (file + 8);
	if (version != 1 && version != 2 && version != 3 && version != 0x0103) return 0;
	if (crcA != computeCrc16(file + 0x20, 0x820, 0xFFFF)) return 0;

	//at 0xA, 0x16 bytes should be 0.
	for (int i = 0; i < 0x16; i++) if (file[i + 0xA] != 0) return 0;

	COLOR *palette = (COLOR *) (file + 0x220);
	for (int i = 0; i < 16; i++) if (palette[i] & 0x8000) return 0;
	
	if (version == 1 && (size != 0x840 && size != 0xA00)) return 0;
	if (version == 2 && (size != 0x940 && size != 0xA00)) return 0;
	if (version == 3 && (size != 0xA40 && size != 0xC00)) return 0;

	return 1;
}

int combo2dIsValid5bg(BYTE *file, int size) {
	//must be a valid G2D structured file
	if (!g2dIsValid(file, size)) return 0;

	//must have PALT section
	char *palt = g2dGetSectionByMagic(file, size, 'PALT');
	if (palt == NULL) palt = g2dGetSectionByMagic(file, size, 'TLAP');
	if (palt == NULL) return 0;

	//must have BGDT section
	char *bgdt = g2dGetSectionByMagic(file, size, 'BGDT');
	if (bgdt == NULL) bgdt = g2dGetSectionByMagic(file, size, 'TDGB');
	if (bgdt == NULL) return 0;

	//may have DFPL section
	return 1;
}

int combo2dIsValid(BYTE *file, int size) {
	if (combo2dIsValid5bg(file, size)) return COMBO2D_TYPE_5BG;
	if (combo2dIsValidTimeAce(file, size)) return COMBO2D_TYPE_TIMEACE;
	if (combo2dIsValidBanner(file, size)) return COMBO2D_TYPE_BANNER;
	if (combo2dIsValidMbb(file, size)) return COMBO2D_TYPE_MBB;
	return 0;
}

int combo2dReadTimeAce(COMBO2D *combo, char *buffer, int size) {
	//add palette
	NCLR *nclr = (NCLR *) calloc(1, sizeof(NCLR));
	nclrInit(nclr, NCLR_TYPE_COMBO);
	nclr->nColors = 256;
	nclr->extPalette = 0;
	nclr->idxTable = NULL;
	nclr->nBits = 4;
	nclr->nPalettes = 0;
	nclr->totalSize = 256 * sizeof(COLOR);
	nclr->colors = (COLOR *) calloc(256, sizeof(COLOR));
	memcpy(nclr->colors, buffer + 4, 512);
	combo2dLink(combo, &nclr->header);

	//add character
	NCGR *ncgr = (NCGR *) calloc(1, sizeof(NCGR));
	ncgrInit(ncgr, NCGR_TYPE_COMBO);
	ncgr->nTiles = *(int *) (buffer + 0xA08);
	ncgr->mappingMode = GX_OBJVRAMMODE_CHAR_2D;
	ncgr->nBits = *(int *) buffer == 0 ? 4 : 8;
	ncgr->tilesX = calculateWidth(ncgr->nTiles);
	ncgr->tilesY = ncgr->nTiles / ncgr->tilesX;
	ncgrReadChars(ncgr, buffer + 0xA0C);
	combo2dLink(combo, &ncgr->header);

	//add screen
	NSCR *nscr = (NSCR *) calloc(1, sizeof(NSCR));
	nscrInit(nscr, NSCR_TYPE_COMBO);
	nscr->nWidth = 256;
	nscr->nHeight = 256;
	nscr->dataSize = (nscr->nWidth / 8) * (nscr->nHeight / 8) * 2;
	nscr->nHighestIndex = 0;
	nscr->data = (uint16_t *) calloc(nscr->dataSize, 1);
	memcpy(nscr->data, buffer + 0x208, nscr->dataSize);
	nscrGetHighestCharacter(nscr);
	combo2dLink(combo, &nscr->header);

	return 0;
}

int combo2dRead5bg(COMBO2D *combo, char *buffer, int size) {
	char *palt = g2dGetSectionByMagic(buffer, size, 'PALT');
	if (palt == NULL) palt = g2dGetSectionByMagic(buffer, size, 'TLAP');
	char *bgdt = g2dGetSectionByMagic(buffer, size, 'BGDT');
	if (bgdt == NULL) bgdt = g2dGetSectionByMagic(buffer, size, 'TDGB');
	char *dfpl = g2dGetSectionByMagic(buffer, size, 'DFPL');
	if (dfpl == NULL) dfpl = g2dGetSectionByMagic(buffer, size, 'LPFD');

	int nColors = *(uint32_t *) (palt + 0x08);

	int chrWidth = *(uint16_t *) (bgdt + 0x14);
	int chrHeight = *(uint16_t *) (bgdt + 0x16);
	int scrSize = *(uint32_t *) (bgdt + 0x0C);
	int mapping = *(uint32_t *) (bgdt + 0x08);
	int charSize = *(uint32_t *) (bgdt + 0x18);
	int charOffset = 0x1C + scrSize;
	int nBits = dfpl == NULL ? 8 : 4; //8-bit if no DFPL present

	int scrX = *(uint16_t *) (bgdt + 0x10);
	int scrY = *(uint16_t *) (bgdt + 0x12);
	int scrDataSize = scrX * scrY * 2;
	int scrWidth = scrX * 8;
	int scrHeight = scrY * 8;

	//addpalette
	NCLR *nclr = (NCLR *) calloc(1, sizeof(NCLR));
	nclrInit(nclr, NCLR_TYPE_COMBO);
	nclr->nColors = nColors;
	nclr->extPalette = 0;
	nclr->idxTable = NULL;
	nclr->nBits = 4;
	nclr->nPalettes = 0;
	nclr->totalSize = nColors * sizeof(COLOR);
	nclr->colors = (COLOR *) calloc(nclr->nColors, sizeof(COLOR));
	memcpy(nclr->colors, palt + 0xC, nColors * sizeof(COLOR));
	combo2dLink(combo, &nclr->header);

	//add character
	NCGR *ncgr = (NCGR *) calloc(1, sizeof(NCGR));
	ncgrInit(ncgr, NCGR_TYPE_COMBO);
	ncgr->nTiles = chrWidth * chrHeight;
	ncgr->tilesX = chrWidth;
	ncgr->tilesY = chrHeight;
	ncgr->nBits = nBits;
	ncgr->mappingMode = mapping;
	ncgrReadChars(ncgr, bgdt + charOffset);
	combo2dLink(combo, &ncgr->header);

	//add screen
	NSCR *nscr = (NSCR *) calloc(1, sizeof(NSCR));
	nscrInit(nscr, NSCR_TYPE_COMBO);
	nscr->nWidth = scrWidth;
	nscr->nHeight = scrHeight;
	nscr->dataSize = scrDataSize;
	nscr->nHighestIndex = 0;
	nscr->data = (uint16_t *) calloc(scrDataSize, 1);
	memcpy(nscr->data, bgdt + 0x1C, scrDataSize);
	nscrGetHighestCharacter(nscr);
	combo2dLink(combo, &nscr->header);

	return 0;
}

int combo2dReadBanner(COMBO2D *combo, char *buffer, int size) {
	BANNER_INFO *info = (BANNER_INFO *) calloc(1, sizeof(BANNER_INFO));
	combo->extraData = (void *) info;
	info->version = *(unsigned short *) buffer;
	memcpy(info->titleJp, buffer + 0x240, 0x600);
	if (info->version >= 2) memcpy(info->titleCn, buffer + 0x840, 0x100);
	if (info->version >= 3) memcpy(info->titleHn, buffer + 0x940, 0x100);

	//add palette
	NCLR *nclr = (NCLR *) calloc(1, sizeof(NCLR));
	nclrInit(nclr, NCLR_TYPE_COMBO);
	nclr->nColors = 16;
	nclr->extPalette = 0;
	nclr->idxTable = NULL;
	nclr->nBits = 4;
	nclr->nPalettes = 0;
	nclr->totalSize = 16 * sizeof(COLOR);
	nclr->colors = (COLOR *) calloc(16, sizeof(COLOR));
	memcpy(nclr->colors, buffer + 0x220, 32);
	combo2dLink(combo, &nclr->header);

	//add character
	NCGR *ncgr = (NCGR *) calloc(1, sizeof(NCGR));
	ncgrInit(ncgr, FILE_TYPE_CHARACTER);
	ncgr->nTiles = 16;
	ncgr->tileWidth = 8;
	ncgr->tilesX = 4;
	ncgr->tilesY = 4;
	ncgr->nBits = 4;
	ncgr->mappingMode = GX_OBJVRAMMODE_CHAR_1D_32K;
	ncgrReadChars(ncgr, buffer + 0x20);
	combo2dLink(combo, &ncgr->header);

	return 0;
}

int combo2dReadMbb(COMBO2D *combo, char *buffer, int size) {
	MBBCOMBO *mbbInfo = (MBBCOMBO *) calloc(1, sizeof(MBBCOMBO));
	combo->extraData = mbbInfo;
	mbbInfo->screenBitmap = 0;

	uint32_t palofs = *(uint32_t *) (buffer + 0x00);
	uint32_t chrofs = *(uint32_t *) (buffer + 0x04);
	uint32_t charSize = *(uint16_t *) (buffer + 0x60);
	uint32_t *scrofs = (uint32_t *) (buffer + 0x08);

	int nBits = (((unsigned char) buffer[0x59]) == 0x80) ? 8 : 4;
	int nChars = (charSize * 0x20) / (8 * nBits);

	//add palette
	NCLR *nclr = (NCLR *) calloc(1, sizeof(NCLR));
	nclrInit(nclr, NCLR_TYPE_COMBO);
	nclr->nColors = 256;
	nclr->extPalette = 0;
	nclr->idxTable = NULL;
	nclr->nBits = nBits;
	nclr->nPalettes = 0;
	nclr->totalSize = nclr->nColors * sizeof(COLOR);
	nclr->colors = (COLOR *) calloc(nclr->nColors, sizeof(COLOR));
	memcpy(nclr->colors, buffer + palofs, nclr->nColors * sizeof(COLOR));
	combo2dLink(combo, &nclr->header);

	//add character
	NCGR *ncgr = (NCGR *) calloc(1, sizeof(NCGR));
	ncgrInit(ncgr, NCGR_TYPE_COMBO);
	ncgr->nBits = nBits;
	ncgr->nTiles = nChars;
	ncgr->tilesX = calculateWidth(ncgr->nTiles);
	ncgr->tilesY = ncgr->nTiles / ncgr->tilesX;
	ncgr->tileWidth = 8;
	ncgr->mappingMode = GX_OBJVRAMMODE_CHAR_1D_32K;
	ncgrReadChars(ncgr, buffer + chrofs);
	combo2dLink(combo, &ncgr->header);

	//add screen
	for (int i = 0; i < 4; i++) {
		uint32_t scrnofs = scrofs[i];
		if (scrnofs == 0) continue;

		int scrWidth = *(uint16_t *) (buffer + 0x18 + i * 0x10 + 0x8);
		int scrHeight = *(uint16_t *) (buffer + 0x18 + i * 0x10 + 0xA);
		int scrDataSize = (scrWidth / 8) * (scrHeight / 8) * 2;

		//add screen
		NSCR *nscr = (NSCR *) calloc(1, sizeof(NSCR));
		nscrInit(nscr, NSCR_TYPE_COMBO);
		nscr->nWidth = scrWidth;
		nscr->nHeight = scrHeight;
		nscr->dataSize = scrDataSize;
		nscr->nHighestIndex = 0;
		nscr->data = (uint16_t *) calloc(scrDataSize, 1);
		memcpy(nscr->data, buffer + scrnofs, scrDataSize);
		nscrGetHighestCharacter(nscr);
		combo2dLink(combo, &nscr->header);

		mbbInfo->screenBitmap |= (1 << i);
	}

	return 0;
}

int combo2dRead(COMBO2D *combo, char *buffer, int size) {
	int format = combo2dIsValid(buffer, size);
	if (format == COMBO2D_TYPE_INVALID) return 1;

	combo2dInit(combo, format);
	switch (format) {
		case COMBO2D_TYPE_TIMEACE:
			return combo2dReadTimeAce(combo, buffer, size);
		case COMBO2D_TYPE_5BG:
			return combo2dRead5bg(combo, buffer, size);
		case COMBO2D_TYPE_BANNER:
			return combo2dReadBanner(combo, buffer, size);
		case COMBO2D_TYPE_MBB:
			return combo2dReadMbb(combo, buffer, size);
	}
	return 1;
}

int combo2dWrite(COMBO2D *combo, BSTREAM *stream) {
	if(!combo2dCanSave(combo)) return 1;

	//write out 
	if (combo->header.format == COMBO2D_TYPE_TIMEACE) {
		NCLR *nclr = (NCLR *) combo2dGet(combo, FILE_TYPE_PALETTE, 0);
		NCGR *ncgr = (NCGR *) combo2dGet(combo, FILE_TYPE_CHARACTER, 0);
		NSCR *nscr = (NSCR *) combo2dGet(combo, FILE_TYPE_SCREEN, 0);
		BOOL is8bpp = ncgr->nBits == 8;
		int dummy = 0;

		bstreamWrite(stream, &is8bpp, sizeof(is8bpp));
		bstreamWrite(stream, nclr->colors, 2 * nclr->nColors);
		bstreamWrite(stream, &dummy, sizeof(dummy));
		bstreamWrite(stream, nscr->data, nscr->dataSize);
		bstreamWrite(stream, &ncgr->nTiles, 4);

		ncgrWriteChars(ncgr, stream);
	} else if (combo->header.format == COMBO2D_TYPE_BANNER) {
		NCLR *nclr = (NCLR *) combo2dGet(combo, FILE_TYPE_PALETTE, 0);
		NCGR *ncgr = (NCGR *) combo2dGet(combo, FILE_TYPE_CHARACTER, 0);

		BANNER_INFO *info = (BANNER_INFO *) combo->extraData;
		unsigned short header[16] = { 0 };
		bstreamWrite(stream, header, sizeof(header));
		ncgrWriteChars(ncgr, stream);

		//write palette
		bstreamWrite(stream, nclr->colors, 32);

		//write titles
		bstreamWrite(stream, info->titleJp, 0x600);
		if (info->version >= 2) bstreamWrite(stream, info->titleCn, 0x100);
		if (info->version >= 3) bstreamWrite(stream, info->titleHn, 0x100);

		//go back and write the CRCs
		bstreamSeek(stream, 0, 0);
		header[0] = info->version;
		header[1] = computeCrc16(stream->buffer + 0x20, 0x820, 0xFFFF);
		if (info->version >= 2) header[2] = computeCrc16(stream->buffer + 0x20, 0x920, 0xFFFF);
		if (info->version >= 3) header[3] = computeCrc16(stream->buffer + 0x20, 0xA20, 0xFFFF);
		bstreamWrite(stream, header, sizeof(header));
	} else if (combo->header.format == COMBO2D_TYPE_DATAFILE) {
		//the original data is in combo->extraData->data, but has key replacements
		DATAFILECOMBO *dfc = (DATAFILECOMBO *) combo->extraData;
		char *copy = (char *) malloc(dfc->size);
		memcpy(copy, dfc->data, dfc->size);

		//process all contained objects
		for (int i = 0; i < combo->nLinks; i++) {
			OBJECT_HEADER *object = combo->links[i];
			int type = object->type;

			//write object
			switch (type) {
				case FILE_TYPE_PALETTE:
				{
					NCLR *nclr = (NCLR *) object;

					int palDataSize = nclr->nColors * 2;
					if (palDataSize > dfc->pltSize) palDataSize = dfc->pltSize;
					memcpy(copy + dfc->pltOffset, nclr->colors, 2 * nclr->nColors);
					break;
				}
				case FILE_TYPE_CHARACTER:
				{
					NCGR *ncgr = (NCGR *) object;

					BSTREAM chrStream;
					bstreamCreate(&chrStream, NULL, 0);
					ncgrWriteChars(ncgr, &chrStream);

					if (chrStream.size > dfc->chrSize) chrStream.size = dfc->chrSize;
					memcpy(copy + dfc->chrOffset, chrStream.buffer, chrStream.size);
					bstreamFree(&chrStream);
					break;
				}
				case FILE_TYPE_SCREEN:
				{
					NSCR *nscr = (NSCR *) object;

					int scrDataSize = nscr->dataSize;
					if (scrDataSize > dfc->scrSize) scrDataSize = dfc->scrSize;
					memcpy(copy + dfc->scrOffset, nscr->data, scrDataSize);
					break;
				}
			}
		}

		bstreamWrite(stream, copy, dfc->size);
		free(copy);
	} else if (combo->header.format == COMBO2D_TYPE_5BG) {
		unsigned char header[] = { 'N', 'T', 'B', 'G', 0xFF, 0xFE, 0, 1, 0, 0, 0, 0, 0x10, 0, 0, 0 };

		NCLR *nclr = (NCLR *) combo2dGet(combo, FILE_TYPE_PALETTE, 0);
		NCGR *ncgr = (NCGR *) combo2dGet(combo, FILE_TYPE_CHARACTER, 0);
		NSCR *nscr = (NSCR *) combo2dGet(combo, FILE_TYPE_SCREEN, 0);

		//how many characters do we write?
		int nCharsWrite = nscrGetHighestCharacter(nscr) + 1;

		int nSections = ncgr->nBits == 4 ? 3 : 2; //no flags for 8-bit images
		int paltSize = 0xC + nclr->nColors * 2;
		int bgdtSize = 0x1C + nCharsWrite * (8 * ncgr->nBits) + (nscr->nWidth / 8 * nscr->nHeight / 8) * 2;
		int dfplSize = nSections == 2 ? 0 : (0xC + ncgr->nTiles);
		*(uint32_t *) (header + 0x08) = sizeof(header) + paltSize + bgdtSize + dfplSize;
		*(uint16_t *) (header + 0x0E) = nSections;

		//write header
		bstreamWrite(stream, header, sizeof(header));

		//write PALT
		unsigned char paltHeader[] = { 'P', 'A', 'L', 'T', 0, 0, 0, 0, 0, 0, 0, 0 };
		*(uint32_t *) (paltHeader + 0x04) = paltSize;
		*(uint32_t *) (paltHeader + 0x08) = nclr->nColors;
		bstreamWrite(stream, paltHeader, sizeof(paltHeader));
		bstreamWrite(stream, nclr->colors, nclr->nColors * sizeof(COLOR));

		//write BGDT
		unsigned char bgdtHeader[0x1C] = { 'B', 'G', 'D', 'T' };
		*(uint32_t *) (bgdtHeader + 0x04) = bgdtSize;
		*(uint32_t *) (bgdtHeader + 0x08) = ncgr->mappingMode;
		*(uint32_t *) (bgdtHeader + 0x0C) = nscr->dataSize;
		*(uint16_t *) (bgdtHeader + 0x10) = nscr->nWidth / 8;
		*(uint16_t *) (bgdtHeader + 0x12) = nscr->nHeight / 8;
		*(uint16_t *) (bgdtHeader + 0x14) = ncgr->tilesX;
		*(uint16_t *) (bgdtHeader + 0x16) = ncgr->tilesY;
		*(uint32_t *) (bgdtHeader + 0x18) = nCharsWrite * (8 * ncgr->nBits);
		bstreamWrite(stream, bgdtHeader, sizeof(bgdtHeader));
		bstreamWrite(stream, nscr->data, nscr->dataSize);
		
		//quick n' dirty write graphics data
		int nTilesOld = ncgr->nTiles;
		ncgr->nTiles = nCharsWrite;
		ncgr->header.format = NCGR_TYPE_BIN;
		ncgrWrite(ncgr, stream);
		ncgr->header.type = NCGR_TYPE_COMBO;
		ncgr->nTiles = nTilesOld;

		//write DFPL
		if (nSections > 2) {
			unsigned char dfplHeader[] = { 'D', 'F', 'P', 'L', 0, 0, 0, 0, 0, 0, 0, 0 };
			*(uint32_t *) (dfplHeader + 0x04) = dfplSize;
			*(uint32_t *) (dfplHeader + 0x08) = ncgr->nTiles;

			//iterate the screen and set attributes
			uint8_t *attr = (uint8_t *) calloc(ncgr->nTiles, 1);
			for (unsigned int i = 0; i < nscr->dataSize / 2; i++) {
				uint16_t tile = nscr->data[i];
				int charNum = tile & 0x3FF;
				int palNum = tile >> 12;
				attr[charNum] = palNum;
			}
			bstreamWrite(stream, dfplHeader, sizeof(dfplHeader));
			bstreamWrite(stream, attr, ncgr->nTiles);
			free(attr);
		}

	} else if (combo->header.format == COMBO2D_TYPE_MBB) {
		MBBCOMBO *mbbInfo = (MBBCOMBO *) combo->extraData;
		NCLR *nclr = (NCLR *) combo2dGet(combo, FILE_TYPE_PALETTE, 0);
		NCGR *ncgr = (NCGR *) combo2dGet(combo, FILE_TYPE_CHARACTER, 0);

		unsigned char header[0x74] = { 0 };
		*(uint32_t *) (header + 0x00) = sizeof(header);
		*(uint32_t *) (header + 0x04) = sizeof(header) + 0x200;
		*(uint16_t *) (header + 0x60) = ncgr->nTiles * (8 * ncgr->nBits) / 0x20;
		header[0x59] = (ncgr->nBits == 8) ? 0x80 : 0;

		//write screen offset infos
		int nScreensWritten = 0;
		uint32_t currentScreenOffset = sizeof(header) + 0x200 + ncgr->nTiles * (8 * ncgr->nBits);
		for (int i = 0; i < 4; i++) {
			if (!(mbbInfo->screenBitmap & (1 << i))) continue;

			NSCR *nscr = (NSCR *) combo2dGet(combo, FILE_TYPE_SCREEN, nScreensWritten);
			*(uint32_t *) (header + 0x08 + i * 4) = currentScreenOffset;
			*(uint16_t *) (header + 0x18 + i * 0x10 + 0x8) = nscr->nWidth;
			*(uint16_t *) (header + 0x18 + i * 0x10 + 0xA) = nscr->nHeight;
			currentScreenOffset += nscr->dataSize;
		}

		bstreamWrite(stream, header, sizeof(header));
		bstreamWrite(stream, nclr->colors, nclr->nColors * sizeof(COLOR));
		ncgrWriteChars(ncgr, stream);

		//write screens
		nScreensWritten = 0;
		for (int i = 0; i < 4; i++) {
			if (!(mbbInfo->screenBitmap & (1 << i))) continue;
			NSCR *nscr = (NSCR *) combo2dGet(combo, FILE_TYPE_SCREEN, nScreensWritten);

			bstreamWrite(stream, nscr->data, nscr->dataSize);

			nScreensWritten++;
		}
	}

	return 0;
}

int combo2dWriteFile(COMBO2D *combo, LPWSTR path) {
	return fileWrite(path, (OBJECT_HEADER *) combo, (OBJECT_WRITER) combo2dWrite);
}
