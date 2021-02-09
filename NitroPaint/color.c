#include "color.h"

COLOR ColorConvertToDS(unsigned long c) {
	int r = ((c & 0xFF) + 4) * 31 / 255;
	int g = (((c >> 8) & 0xFF) + 4) * 31 / 255;
	int b = (((c >> 16) & 0xFF) + 4) * 31 / 255;
	return r | (g << 5) | (b << 10);
}

unsigned long ColorConvertFromDS(COLOR c) {
	int r = GetR(c) * 255 / 31;
	int g = GetG(c) * 255 / 31;
	int b = GetB(c) * 255 / 31;
	return r | (g << 8) | (b << 16);
}

COLOR ColorInterpolate(COLOR c1, COLOR c2, float amt) {
	int r = (int) (GetR(c1) * (1.0f - amt) + GetR(c2) * amt);
	int g = (int) (GetG(c1) * (1.0f - amt) + GetG(c2) * amt);
	int b = (int) (GetB(c1) * (1.0f - amt) + GetB(c2) * amt);
	return r | (g << 5) | (b << 10);
}