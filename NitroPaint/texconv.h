#pragma once
#include <Windows.h>
#include "texture.h"

//
// Structure used by texture conversion functions.
//
typedef struct {
	COLOR32 *px;
	int width;
	int height;
	int fmt;
	int dither;
	float diffuseAmount;
	int ditherAlpha;
	int colorEntries;
	int useFixedPalette;
	COLOR *fixedPalette;
	int threshold;
	int balance;
	int colorBalance;
	int enhanceColors;
	TEXTURE *dest;
	void (*callback) (void *);
	void *callbackParam;
	char pnam[17];
} CREATEPARAMS;

//
// Counts the number of colors in an image (transparent counts as a color)
//
int ImgCountColors(COLOR32 *px, int nPx);

//
// Convert an image to a direct mode texture
//
int TxConvertDirect(CREATEPARAMS *params);

//
// Convert an image to a paletted texture
//
int TxConvertIndexedOpaque(CREATEPARAMS *params);

//
// Convert an image to a translucent (a3i5 or a5i3) texture
//
int TxConvertIndexedTranslucent(CREATEPARAMS *params);

//progress markers for TxConvert4x4.
extern volatile g_texCompressionProgress;
extern volatile g_texCompressionProgressMax;
extern volatile g_texCompressionFinished;

//
// Convert an image to a 4x4 compressed texture
//
int TxConvert4x4(CREATEPARAMS *params);

//
// Convert a texture given some input parameters.
//
int TxConvert(CREATEPARAMS *params);

//
// Begin a texture conversion in a new thread, returning a handle to the thread.
//
HANDLE textureConvertThreaded(COLOR32 *px, int width, int height, int fmt, int dither, float diffuse, int ditherAlpha, int colorEntries, int useFixedPalette, COLOR *fixedPalette, int threshold, int balance, int colorBalance, int enhanceColors, char *pnam, TEXTURE *dest, void (*callback) (void *), void *callbackParam);
