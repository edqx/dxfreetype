#pragma once

#include <d3d9.h>
#include <d3dx9.h>
#include <map>
#include <string>
#include <ft2build.h>
#include FT_FREETYPE_H
#include FT_STROKER_H
#pragma comment(lib,"freetype.lib")

#include "CSVFont.h"
#include "DXTexture.h"

class DXFontTexture {
public:
	int x, y;
	int width;
	int height;
	bool unusable;
};

#define TEXTURE_HEIGHT 1024
#define TEXTURE_WIDTH 1024

struct CachedTexture
{
	int adv;
	DXFontTexture* texture;
};

enum TextAlign
{
	TextAlignLeft,
	TextAlignCenter,
	TextAlignRight
};

class DXFont {
private:
	// drawing
	IDirect3DDevice9* device;

	// for caching
	std::map<int, CachedTexture> texture_cache;

	// freetype
	FT_Library ftLib;
	FT_Face ftFace;
	FT_Stroker ftStroker;
	DXTexture fontTexture;
	bool fontTextureLoaded;

	D3DCOLOR getColor(int x, int y, int a);

	BOOL getAvailableGlyphSlot(DXFontTexture *data);
	int cx, cy;

	// refer
	// http://www.devpia.com/MAEUL/Contents/Detail.aspx?BoardID=51&MAEULNO=20&no=8373
public:
	CSVFont *fontData;

	BOOL InitDXFont(IDirect3DDevice9* device);
	BOOL Release();
	DXFontTexture* GetFontTexture(TCHAR chr, int* wid = nullptr, int* hei = nullptr, int* adv = nullptr);

	BOOL Draw(IDirect3DDevice9* device, LPD3DXSPRITE sprite, std::string text, float size, TextAlign align, int x, int y, uint8_t r, uint8_t g, uint8_t b, uint8_t a);
	BOOL RenderChar(TCHAR chr, bool render = true, int* width = nullptr, int* height = nullptr, int* adv = nullptr);
	BOOL DrawChar(TCHAR chr, D3DCOLOR *pixels, int textureWidth, int x=0, int y=0);
	
	LPDIRECT3DTEXTURE9 glyphTexture;

	~DXFont();
};