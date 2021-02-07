#include "DXFont.h"
#include <vector>
#include <string>

	
BOOL DXFont::InitDXFont(IDirect3DDevice9* device) {
	// set device
	this->device = device;

	// init freetype library
	if (FT_Init_FreeType(&ftLib)) {
		return FALSE;
	}

	// make font (fallback)
	std::wstring path_ws = fontData->fontPath;
	int err = FT_New_Face(ftLib, std::string(path_ws.begin(), path_ws.end()).c_str(), 0, &ftFace);
	if (err)
		return FALSE;

	if (FT_Stroker_New(ftLib, &ftStroker))
		return FALSE;
	
	// TODO: too small size can cause error
	//if (fontData->fontHeight < 20)
	//	fontData->fontHeight = 20;

	// set size and stroke
	FT_Set_Char_Size(ftFace, fontData->fontHeight * 64, 0,
		96, 96);
	FT_Stroker_Set(ftStroker, fontData->borderWidth * 64, FT_STROKER_LINECAP_ROUND, FT_STROKER_LINEJOIN_ROUND,
		0);
	
	// fill glyphHeight with simple index
	fontData->glyphHeight = ::FT_MulFix(ftFace->bbox.yMax - ftFace->bbox.yMin, ftFace->size->metrics.y_scale)/64;

	/* Old method
	FT_Get_Char_Index(ftFace, L'��');
	FT_Get_Char_Index(ftFace, L'^');
	FT_Load_Glyph(ftFace,ind,FT_LOAD_DEFAULT);
	FT_Render_Glyph(ftFace->glyph, FT_RENDER_MODE_NORMAL);
//	fontData->glyphHeight = ftFace->glyph->metrics.vertAdvance/64;*/

	// if there's texture for DXFont, load it.
	fontTextureLoaded = false;
	if (wcslen(fontData->fontTexturePath) > 0 && device) {
		if (fontTexture.LoadTexture(fontData->fontTexturePath, device)) {
			fontTextureLoaded = true;
		}
	}

	int errco = D3DXCreateTexture(device, TEXTURE_WIDTH, TEXTURE_HEIGHT, 0, D3DUSAGE_DYNAMIC, D3DFMT_A8R8G8B8, D3DPOOL_DEFAULT, &glyphTexture);
	// TODO: create texture
	if (FAILED(errco))
		return FALSE;
	// glyphTexture

	// clear texture cache
	texture_cache.clear();
	cx = 0;
	cy = 0;
	return TRUE;
}

BOOL DXFont::getAvailableGlyphSlot(DXFontTexture *data) {
	int h = fontData->glyphHeight + fontData->borderWidth*2;
	if (cx+data->width > TEXTURE_WIDTH) {
		cx = 0;
		cy += h;
	}
	if (cy+h > TEXTURE_HEIGHT) {
		// no place left for new chr
		data->unusable = true;
		return FALSE;
	}
	
	data->x = cx;
	data->y = cy+fontData->borderWidth*2;
	cx += data->width;
}

BOOL DXFont::Release() {
	if (ftLib) {
		FT_Done_FreeType(ftLib);
	
		glyphTexture->Release();

		// TODO is releasing routing OKAY?
		std::map<int, CachedTexture>::iterator it;
		for (it=texture_cache.begin(); it != texture_cache.end(); it++) {
			delete it->second.texture;
		}
		texture_cache.clear();

		ftLib = 0;
		return TRUE;
	}
}

BOOL DXFont::Draw(IDirect3DDevice9* device, LPD3DXSPRITE sprite, std::string text, float size, TextAlign align, int x, int y, uint8_t r, uint8_t g, uint8_t b, uint8_t a)
{
	int totalWidth = 0;
	for (int i = 0; i < text.length(); i++)
	{
		int charAdv;
		if (GetFontTexture(text[i], nullptr, nullptr, &charAdv))
			totalWidth += charAdv;
	}

	double multiply = (double)size / fontData->fontHeight;
	double widthMul = 1;
	double totalWidthMul = widthMul * multiply;
	int currentWidth = totalWidth;

	int sx = x;
	switch (align)
	{
	case TextAlignCenter:
		sx -= totalWidth / 2;
		break;
	case TextAlignRight:
		sx -= totalWidth;
		break;
	}

	D3DXMATRIX mat, orgmat;
	if (sprite) sprite->GetTransform(&orgmat);

	for (int i = 0; i < text.length(); i++)
	{
		int charAdv;
		char ch = text[i];
		DXFontTexture* dxfnt;
		dxfnt = GetFontTexture(text[i], nullptr, nullptr, &charAdv);
		if (dxfnt)
		{
			D3DXVECTOR2 vec1 = D3DXVECTOR2(x, y);
			D3DXVECTOR2 vec2 = D3DXVECTOR2(totalWidthMul, multiply);
			D3DXVECTOR2 vec3 = D3DXVECTOR2(0, 0);
			D3DXVECTOR2 vec4 = D3DXVECTOR2(sx * totalWidthMul, y * multiply);
			D3DXMatrixTransformation2D(&mat, &vec1, 0,
				&vec2, &vec3, 0,
				&vec4);
			sprite->SetTransform(&mat);

			RECT rec;
			SetRect(&rec, dxfnt->x, dxfnt->y, dxfnt->width + dxfnt->x, dxfnt->height + dxfnt->y);
			sprite->Draw(glyphTexture, &rec, 0, 0, D3DCOLOR_ARGB(a, r, g, b));
		}

		sx += charAdv;
	}

	sprite->SetTransform(&orgmat);
	return TRUE;
}

BOOL DXFont::RenderChar(TCHAR chr, bool render, int *width, int *height, int* adv) {
	// get glyph index
	int index = FT_Get_Char_Index(ftFace, chr);
	if (index == 0) {
		return FALSE;
	}

	// load glyph
	FT_Load_Glyph(ftFace, index, FT_LOAD_MONOCHROME);
 
	// do rendering
	if (render) {
		FT_Render_Glyph(ftFace->glyph, FT_RENDER_MODE_NORMAL);

		if (width)
			*width = ftFace->glyph->metrics.width / 64;
		if (height)
			*height = ftFace->glyph->metrics.height / 64;
		if (adv)
			*adv = ftFace->glyph->metrics.horiAdvance / 64;
	}

	return TRUE;
}

D3DCOLOR DXFont::getColor(int x, int y, int a) {
	// if texture exists then get from it
	if (y < 0) y+=fontTexture.height;
	if (fontTextureLoaded) {
		int clr;
		D3DLOCKED_RECT lRect;
		fontTexture.GetTexture()->LockRect(0, &lRect, NULL, 0);
		D3DCOLOR* pClr = (D3DCOLOR*)lRect.pBits;
		clr = pClr[x%fontTexture.width + fontTexture.width*(y%fontTexture.height)];
		clr = a<<24 | clr&0x00FFFFFF;
		fontTexture.GetTexture()->UnlockRect(0);
		return clr;
	} else {
		// set argb
		return D3DCOLOR_ARGB(a, fontData->fontR, fontData->fontG, fontData->fontB);
	}
}

BOOL DXFont::DrawChar(TCHAR chr, D3DCOLOR* pixels, int textureWidth, int x, int y) {
	RenderChar(chr);

	int width = ftFace->glyph->bitmap.width;
	int height = ftFace->glyph->bitmap.rows;
	int left = ftFace->glyph->bitmap_left;
	int top = ftFace->glyph->bitmap_top;
	int border = fontData->borderWidth;

	if (pixels) {
		for (int i=0; i<width; i++) {
			for (int j=0; j<height; j++) {
				int Color = ftFace->glyph->bitmap.buffer[j*width+i];
				if (Color > 0) {
					int px = x+i+border;
					int py = fontData->glyphHeight - ftFace->glyph->bitmap_top +y+j;
					pixels[px + py*textureWidth] = getColor(px, fontData->glyphHeight-j, Color);
				}
			}
		}
	}

	// if stroke exists?
	// then draw it
	if (border > 0) {
		RenderChar(chr, false);

		FT_Glyph glyph;
		FT_Get_Glyph(ftFace->glyph, &glyph);
		
		FT_Outline *o =
			&reinterpret_cast<FT_OutlineGlyph>(glyph)->outline;
		
		FT_Glyph_StrokeBorder(&glyph, ftStroker, 0, 1);
		FT_Vector vec1 = FT_Vector();
		FT_Glyph_To_Bitmap(&glyph, FT_RENDER_MODE_NORMAL, &vec1, 1);


		FT_BitmapGlyph b = (FT_BitmapGlyph)glyph;
		width = b->bitmap.width;
		height = b->bitmap.rows;
		left -= fontData->borderWidth;
		top += fontData->borderWidth;

		if (pixels)
		{
			for (int i = 0; i < width; i++)
			{
				for (int j = 0; j < height; j++)
				{
					int Color = b->bitmap.buffer[j * width + i];
					if (Color > 0)
					{
						int px = x + i;
						int py = fontData->glyphHeight - top + y + j;
						int ind = px + py * textureWidth;
						if (pixels[ind] == 0)
						{
							// set argb
							pixels[ind] = D3DCOLOR_ARGB(255, fontData->borderR, fontData->borderG, fontData->borderB);
						}
					}
				}
			}
		}
		
		FT_Done_Glyph(glyph);
	}

	return TRUE;
}

DXFontTexture* DXFont::GetFontTexture(TCHAR chr, int *width, int *height, int* adv) {
	int chrIndex = (int)chr;
	std::map<int, CachedTexture>::iterator it = texture_cache.find(chrIndex);
	if (it == texture_cache.end()) {
		int index = FT_Get_Char_Index(ftFace, chr);
		if (!index) {
			chr = L'?';	// FALLBACK
			index = FT_Get_Char_Index(ftFace, chr);
		}

		// render character
		int wid, hei, ad;
		if (RenderChar(chr, true, &wid, &hei, &ad)) {
			// add border (necessary)
			int border = fontData->borderWidth;
			wid += border*2;
			hei = fontData->glyphHeight*1.1f + border*2;	// IMPORTANT! glyph height

			// get new position for draw
			DXFontTexture *t = new DXFontTexture();
			t->width = wid;
			t->height = hei;
			if (!getAvailableGlyphSlot(t))
				return 0;

			D3DCOLOR *pixels;
			D3DLOCKED_RECT lRect;
			if (FAILED(glyphTexture->LockRect(0, &lRect, NULL, 0)))
				return 0;
			pixels = (D3DCOLOR*)lRect.pBits;

			// draw text
			DrawChar(chr, pixels, TEXTURE_WIDTH, border+t->x, border+t->y); 
			glyphTexture->UnlockRect(0);

			// add to array
			texture_cache.insert(std::make_pair(chrIndex, CachedTexture{ ad, t }));

			if (width)
				*width = wid;
			if (height)
				*height = hei;
			if (adv)
				*adv = ad;

			return t;
		} else {
			return 0;
		}
	} else {
		CachedTexture& t = it->second;
		if (width)
			*width = t.texture->width;
		if (height)
			*height = t.texture->height;
		if (adv)
			*adv = t.adv;
		return t.texture;
	}
}

DXFont::~DXFont() {
	Release();
}