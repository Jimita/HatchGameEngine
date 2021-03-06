#if INTERFACE
#include <Engine/Includes/Standard.h>
#include <Engine/Includes/StandardSDL2.h>
#include <Engine/ResourceTypes/ISprite.h>
#include <Engine/ResourceTypes/IModel.h>
#include <Engine/Math/Matrix4x4.h>
#include <Engine/Rendering/GL/GLShader.h>
#include <Engine/Rendering/Texture.h>
#include <Engine/Includes/HashMap.h>

class SoftwareRenderer {
public:
    static GraphicsFunctions  BackendFunctions;
};
#endif

#include <Engine/Rendering/Software/SoftwareRenderer.h>

#include <Engine/Diagnostics/Log.h>
#include <Engine/Diagnostics/Memory.h>
#include <Engine/IO/ResourceStream.h>

GraphicsFunctions  SoftwareRenderer::BackendFunctions;
// // Framebuffer-related variables
// Uint32*       SoftwareRenderer::FrameBuffer = NULL;
// int           SoftwareRenderer::FrameBufferSize = 0;
// int           SoftwareRenderer::FrameBufferStride = 0;
// SDL_Surface*  SoftwareRenderer::FrameBufferSurface = NULL;
// void*         SoftwareRenderer::FrameBufferSurfaceOriginalPixels = NULL;
// // Pixel coloring variables
// uint8_t       SoftwareRenderer::Fade = 0;
// bool          SoftwareRenderer::FadeToWhite = false;
// int           SoftwareRenderer::WaterPaletteStartLine = 0xFFFF;
// int           SoftwareRenderer::DivideBy3Table[0x100];
// // Pixel positioning variables
// int           SoftwareRenderer::Clip[4];
// bool          SoftwareRenderer::ClipSet = false;
// Sint8*        SoftwareRenderer::Deform = NULL;
// bool          SoftwareRenderer::DoDeform = false;
// // Draw mode variables
// int           SoftwareRenderer::DrawAlpha = 0xFF;
// int           SoftwareRenderer::DrawModeOverlay = false;
// int           SoftwareRenderer::DrawPixelFilter = 0;
// void          (*SoftwareRenderer::PixelFunction)(int, int, Uint32) = NULL;
// Uint32        (*SoftwareRenderer::FilterFunction[4])(Uint32);
// // Other variables
// Uint8         SoftwareRenderer::BitmapFont[128][8];

int DivideBy3Table[0x100];

struct SWTextureData {
	Uint32*           Palette = NULL;
	int               PaletteSize = 0;
	int               PaletteCount = 0;
	int               TransparentColorIndex = 0;
};

inline Uint32 ColorAdd(Uint32 color1, Uint32 color2, int percent) {
	Uint32 r = (color1 & 0xFF0000U) + (((color2 & 0xFF0000U) * percent) >> 8);
	Uint32 g = (color1 & 0xFF00U) + (((color2 & 0xFF00U) * percent) >> 8);
	Uint32 b = (color1 & 0xFFU) + (((color2 & 0xFFU) * percent) >> 8);
    if (r > 0xFF0000U) r = 0xFF0000U;
	if (g > 0xFF00U) g = 0xFF00U;
	if (b > 0xFFU) b = 0xFFU;
	return r | g | b;
}
inline Uint32 ColorBlend(Uint32 color1, Uint32 color2, int percent) {
	Uint32 rb = color1 & 0xFF00FFU;
	Uint32 g  = color1 & 0x00FF00U;
	rb += (((color2 & 0xFF00FFU) - rb) * percent) >> 8;
	g  += (((color2 & 0x00FF00U) - g) * percent) >> 8;
	return (rb & 0xFF00FFU) | (g & 0x00FF00U);
}
inline Uint32 ColorSubtract(Uint32 color1, Uint32 color2, int percent) {
    Sint32 r = (color1 & 0xFF0000U) - (((color2 & 0xFF0000U) * percent) >> 8);
	Sint32 g = (color1 & 0xFF00U) - (((color2 & 0xFF00U) * percent) >> 8);
	Sint32 b = (color1 & 0xFFU) - (((color2 & 0xFFU) * percent) >> 8);
    if (r < 0) r = 0;
	if (g < 0) g = 0;
	if (b < 0) b = 0;
	return r | g | b;
}

inline Uint32 FilterNone(Uint32 pixel) {
	return pixel;
}
inline Uint32 FilterGrayscale(Uint32 pixel) {
	pixel = (
		DivideBy3Table[((pixel >> 16) & 0xFF)] +
		DivideBy3Table[((pixel >> 8) & 0xFF)] +
		DivideBy3Table[(pixel & 0xFF)]) & 0xFF;
	pixel = pixel << 16 | pixel << 8 | pixel;
	return pixel;
}
inline Uint32 FilterInversionRadius(Uint32 pixel) {
	// if ((x - 200) * (x - 200) + (y - Application::HEIGHT / 2) * (y - Application::HEIGHT / 2) < invertRadius * invertRadius)
	//     pixel = pixel ^ 0xFFFFFF;
	return pixel;
}

inline void   SetPixelNormal(int x, int y, Uint32 pixel) {
	// *(SoftwareRenderer::FrameBuffer + y * SoftwareRenderer::FrameBufferStride + x) = pixel;
}
inline void   SetPixelAlpha(int x, int y, Uint32 pixel) {
	// *(SoftwareRenderer::FrameBuffer + y * SoftwareRenderer::FrameBufferStride + x) = ColorBlend(*(SoftwareRenderer::FrameBuffer + y * SoftwareRenderer::FrameBufferStride + x), pixel, SoftwareRenderer::DrawAlpha);
}
inline void   SetPixelAdditive(int x, int y, Uint32 pixel) {
	// *(SoftwareRenderer::FrameBuffer + y * SoftwareRenderer::FrameBufferStride + x) = ColorAdd(*(SoftwareRenderer::FrameBuffer + y * SoftwareRenderer::FrameBufferStride + x), pixel, SoftwareRenderer::DrawAlpha);
}

inline void SetPixel(int x, int y, Uint32 color) {
	// if (DoDeform) x += Deform[y];

	// if (x <  Clip[0]) return;
	// if (y <  Clip[1]) return;
	// if (x >= Clip[2]) return;
	// if (y >= Clip[3]) return;

	// SetPixelFunction(x, y, GetPixelOutputColor(color));
	// SetPixelFunction(x, y, color);
}

inline Uint32 GetPixelOutputColor(Uint32 color) {
	// color = (this->*SetFilterFunction[0])(color);
	// color = (this->*SetFilterFunction[1])(color);
	// color = (this->*SetFilterFunction[2])(color);
	// color = (this->*SetFilterFunction[3])(color);

	return color;
}

bool SaveScreenshot(const char* filename) {
    /*
	Uint32* palette = (Uint32*)Memory::Calloc(256, sizeof(Uint32));
	Uint32* pixelChecked = (Uint32*)Memory::Calloc(FrameBufferSize, sizeof(Uint32));
	Uint32* pixelIndexes = (Uint32*)Memory::Calloc(FrameBufferSize, sizeof(Uint32));
	Uint32  paletteCount = 0;
	for (int checking = 0; checking < FrameBufferSize; checking++) {
		if (!pixelChecked[checking]) {
			Uint32 color = FrameBuffer[checking] & 0xFFFFFF;
			for (int comparing = checking; comparing < FrameBufferSize; comparing++) {
				if (color == (FrameBuffer[comparing] & 0xFFFFFF)) {
					pixelIndexes[comparing] = paletteCount;
					pixelChecked[comparing] = 1;
				}
			}
			if (paletteCount == 255) {
				Memory::Free(palette);
				Memory::Free(pixelChecked);
				Memory::Free(pixelIndexes);
				Log::Print(Log::LOG_ERROR, "Too many colors!");
				return false;
			}
			palette[paletteCount++] = color;
		}
	}

	char filenameStr[100];
	time_t now; time(&now);
	struct tm* local = localtime(&now);
	sprintf(filenameStr, "ImpostorEngine3_%02d-%02d-%02d_%02d-%02d-%02d.gif", local->tm_mday, local->tm_mon + 1, local->tm_year - 100, local->tm_hour, local->tm_min, local->tm_sec);

	GIF* gif = new GIF;
	gif->Colors = palette;
    gif->Data = pixelIndexes;
    gif->Width = Application::WIDTH;
    gif->Height = Application::HEIGHT;
    gif->TransparentColorIndex = 0xFF;
	if (!gif->Save(filenameStr)) {
		gif->Data = NULL;
		delete gif;
		Memory::Free(palette);
		Memory::Free(pixelChecked);
		Memory::Free(pixelIndexes);
		return false;
	}
	gif->Data = NULL;
	delete gif;
	Memory::Free(palette);
	Memory::Free(pixelChecked);
	Memory::Free(pixelIndexes);

    */

	return true;
}

PUBLIC STATIC inline Uint32 SoftwareRenderer::GetPixelIndex(ISprite* sprite, int x, int y) {
	// return sprite->Data[x + y * sprite->Width];
	return 0;
}

// Rendering functions
PUBLIC STATIC void    SoftwareRenderer::SetDrawAlpha(int a) {
	// DrawAlpha = a;
    //
	// if (SetPixelFunction != &SoftwareRenderer::SetPixelNormal && SetPixelFunction != &SoftwareRenderer::SetPixelAlpha) return;
    //
	// if (DrawAlpha == 0xFF)
	// 	SetPixelFunction = &SoftwareRenderer::SetPixelNormal;
	// else
	// 	SetPixelFunction = &SoftwareRenderer::SetPixelAlpha;
}
PUBLIC STATIC void    SoftwareRenderer::SetDrawFunc(int a) {
	// switch (a) {
	// 	case 1:
	// 		SetPixelFunction = &SoftwareRenderer::SetPixelAdditive;
	// 		break;
	// 	default:
	// 		SetPixelFunction = &SoftwareRenderer::SetPixelNormal;
	// 		SetDrawAlpha(DrawAlpha);
	// 		break;
	// }
}

PUBLIC STATIC void    SoftwareRenderer::SetFade(int fade) {
	// if (fade < 0)
	// 	Fade = 0x00;
	// else if (fade > 0xFF)
	// 	Fade = 0xFF;
	// else
	// 	Fade = fade;
}
PUBLIC STATIC void    SoftwareRenderer::SetFilter(int filter) {
	// DrawPixelFilter = filter;
    //
	// SetFilterFunction[0] = &SoftwareRenderer::FilterNone;
	// // SetFilterFunction[1] = &SoftwareRenderer::FilterNone;
	// // SetFilterFunction[2] = &SoftwareRenderer::FilterNone;
	// SetFilterFunction[3] = &SoftwareRenderer::FilterNone;
	// if ((DrawPixelFilter & 0x1) == 0x1)
	// 	SetFilterFunction[0] = &SoftwareRenderer::FilterGrayscale;
	// if ((DrawPixelFilter & 0x4) == 0x4)
	// 	SetFilterFunction[3] = &SoftwareRenderer::FilterFadeToBlack;
}
PUBLIC STATIC int     SoftwareRenderer::GetFilter() {
	// return DrawPixelFilter;
    return 0;
}

// Utility functions
PUBLIC STATIC bool    SoftwareRenderer::IsOnScreen(int x, int y, int w, int h) {
	// return (
	// 	x + w >= Clip[0] &&
	// 	y + h >= Clip[1] &&
	// 	x     <  Clip[2] &&
	// 	y     <  Clip[3]);
    return false;
}

// Drawing Shapes
long ContourX[800][2];
PUBLIC STATIC void    SoftwareRenderer::DrawPoint(int x, int y, Uint32 color) {
	SetPixel(x, y, color);
}
PUBLIC STATIC void    SoftwareRenderer::DrawLine(int x0, int y0, int x1, int y1, Uint32 color) {
	int dx = Math::Abs(x1 - x0), sx = x0 < x1 ? 1 : -1;
	int dy = Math::Abs(y1 - y0), sy = y0 < y1 ? 1 : -1;
	int err = (dx > dy ? dx : -dy) / 2, e2;

	while (true) {
		SetPixel(x0, y0, color);
		if (x0 == x1 && y0 == y1) break;
		e2 = err;

		if (e2 > -dx) { err -= dy; x0 += sx; }
		if (e2 <  dy) { err += dx; y0 += sy; }
	}
}
PUBLIC STATIC void    SoftwareRenderer::DrawCircle(int x, int y, int radius, Uint32 color) {

}
PUBLIC STATIC void    SoftwareRenderer::DrawCircleStroke(int x, int y, int radius, Uint32 color) {

}
PUBLIC STATIC void    SoftwareRenderer::DrawEllipse(int x, int y, int width, int height, Uint32 color) {

}
PUBLIC STATIC void    SoftwareRenderer::DrawEllipseStroke(int x, int y, int width, int height, int radius, Uint32 color) {

}
PUBLIC STATIC void    SoftwareRenderer::DrawTriangle(int p0_x, int p0_y, int p1_x, int p1_y, int p2_x, int p2_y, Uint32 color) {
	// // Bresenham Scanline method:
	// int y, maxX, maxY;
	// for (y = 0; y < Application::HEIGHT; y++) {
	// 	ContourX[y][0] = LONG_MAX; // min X
	// 	ContourX[y][1] = LONG_MIN; // max X
	// }
    //
	// ScanLine(p0_x, p0_y, p1_x, p1_y);
	// ScanLine(p1_x, p1_y, p2_x, p2_y);
	// ScanLine(p2_x, p2_y, p0_x, p0_y);
    //
	// maxX = Math::Max(p0_x, Math::Max(p1_x, p2_x));
	// maxY = Math::Max(p0_y, Math::Max(p1_y, p2_y));
    //
	// for (y = 0; y < Application::HEIGHT; y++) {
	// 	if (ContourX[y][1] >= ContourX[y][0]) {
	// 		long x = ContourX[y][0];
	// 		long len = ContourX[y][1] - ContourX[y][0];
    //
	// 		// Can draw a horizontal line instead of individual pixels here
	// 		while (len--) {
	// 			if (x < maxX && y < maxY)
	// 				SetPixel(x++, y, color);
	// 		}
	// 	}
	// }
}
PUBLIC STATIC void    SoftwareRenderer::DrawTriangleStroke(int p0_x, int p0_y, int p1_x, int p1_y, int p2_x, int p2_y, Uint32 color) {
	DrawLine(p0_x, p0_y, p1_x, p1_y, color);
	DrawLine(p1_x, p1_y, p2_x, p2_y, color);
	DrawLine(p2_x, p2_y, p0_x, p0_y, color);
}
PUBLIC STATIC void    SoftwareRenderer::DrawRectangle(int x, int y, int width, int height, Uint32 color) {
	// NOTE: Faster
	/*
	int Amin, Amax, Bmin, Bmax;
	Amin = x;
    Amax = Amin + width;
    Bmin = Clip[0];
    Bmax = Clip[2];
    if (Bmin > Amin)
        Amin = Bmin;
    x = Amin;
    if (Bmax < Amax)
        Amax = Bmax;
    width = Amax - Amin;

    Amin = y;
    Amax = Amin + height;
    Bmin = Clip[1];
    Bmax = Clip[3];
    if (Bmin > Amin)
        Amin = Bmin;
    y = Amin;
    if (Bmax < Amax)
        Amax = Bmax;
    height = Amax - Amin;

	if ((width <= 0) || (height <= 0)) return;

	color = GetPixelOutputColor(color);

	if (SetPixelFunction == &SoftwareRenderer::SetPixelNormal) {
		int pitch = FrameBufferStride << 2;
		Uint8* pixels = (Uint8*)(FrameBuffer + x + y * FrameBufferStride);
		while (height--) {
	        Memory::Memset4(pixels, color, width);
	        pixels += pitch;
	    }
	}
	else {
		int bx = x + width;
		for (int b = 0; b < width * height; b++) {
			SetPixelFunction(x, y, color);

			x++;
			if (x >= bx) {
				x -= width;
				y++;
			}
		}
	}
	//*/
}
PUBLIC STATIC void    SoftwareRenderer::DrawRectangleStroke(int x, int y, int width, int height, Uint32 color) {
	int x1, x2, y1, y2;

	x1 = x;
	x2 = x + width - 1;
	if (width < 0)
		x1 += width, x2 -= width;

	y1 = y;
	y2 = y + height - 1;
	if (height < 0)
		y1 += height, y2 -= height;

	for (int b = x1; b <= x2; b++) {
		SetPixel(b, y1, color);
		SetPixel(b, y2, color);
	}
	for (int b = y1 + 1; b < y2; b++) {
		SetPixel(x1, b, color);
		SetPixel(x2, b, color);
	}
}
PUBLIC STATIC void    SoftwareRenderer::DrawRectangleSkewedH(int x, int y, int width, int height, int sk, Uint32 color) {
	DrawTriangle(
		x, y,
		x + width, y,
		x + width + sk, y + height, color);

	DrawTriangle(
		x, y,
		x + sk, y + height,
		x + width + sk, y + height, color);
}

// Drawing Sprites
PUBLIC STATIC void    SoftwareRenderer::DrawSpriteNormal(ISprite* sprite, int SrcX, int SrcY, int Width, int Height, int CenterX, int CenterY, bool FlipX, bool FlipY, int RealCenterX, int RealCenterY) {
	// Uint32 PC;
	// bool AltPal = false;
	// Uint32* Pal = sprite->Palette;
	// int DX = 0, DY = 0, PX, PY, size = Width * Height;
	// // Uint32 TrPal = sprite->Palette[sprite->TransparentColorIndex];
	// Width--;
	// Height--;
	//
	// if (!AltPal && CenterY + RealCenterY >= WaterPaletteStartLine && sprite->PaletteCount == 2) {
	// 	AltPal = true;
	// 	Pal = sprite->PaletteAlt;
	// 	// TrPal = sprite->PaletteAlt[sprite->TransparentColorIndex];
	// }
	// for (int D = 0; D < size; D++) {
	// 	PX = DX;
	// 	PY = DY;
	// 	if (FlipX)
	// 		PX = Width - PX;
	// 	if (FlipY)
	// 		PY = Height - PY;
	//
	// 	PC = GetPixelIndex(sprite, SrcX + PX, SrcY + PY);
	// 	if (sprite->PaletteCount == 0) {
	// 		SetPixel(CenterX + DX + RealCenterX, CenterY + DY + RealCenterY, PC);
	// 	}
	// 	else if (PC) {
	// 		SetPixel(CenterX + DX + RealCenterX, CenterY + DY + RealCenterY, Pal[PC]);
	// 	}
	//
	// 	DX++;
	// 	if (DX > Width) {
	// 		DX = 0;
	// 		DY++;
	//
	// 		if (!AltPal && CenterY + DY + RealCenterY >= WaterPaletteStartLine && sprite->PaletteCount == 2) {
	// 			AltPal = true;
	// 			Pal = sprite->PaletteAlt;
	// 			// TrPal = sprite->PaletteAlt[sprite->TransparentColorIndex];
	// 		}
	// 	}
	// }
}
PUBLIC STATIC void    SoftwareRenderer::DrawSpriteTransformed(ISprite* sprite, int animation, int frame, int x, int y, bool flipX, bool flipY, int scaleW, int scaleH, int angle) {
	if (Graphics::SpriteRangeCheck(sprite, animation, frame)) return;

	// AnimFrame animframe = sprite->Animations[animation].Frames[frame];

	// TODO: Add shortcut here if angle == 0 && scaleW == 0x100 && scaleH = 0x100
	// DrawSpriteNormal();
	// return;

	/*

	int srcx = 0, srcy = 0, srcw = animframe.W, srch = animframe.H;
	if (srcw >= animframe.Width- srcx)
		srcw  = animframe.Width- srcx;
	if (srch >= animframe.H - srcy)
		srch  = animframe.H - srcy;

	bool AltPal = false;
	Uint32 PixelIndex, *Pal = sprite->Palette;
	int DX = 0, DY = 0, PX, PY, size;
	int CenterX = x, CenterY = y, RealCenterX = animframe.OffX, RealCenterY = animframe.OffY;
	int SrcX = animframe.X + srcx, SrcY = animframe.Y + srcy, Width = srcw - 1, Height = srch - 1;
	if (flipX)
		RealCenterX = -RealCenterX - Width - 1;
	if (flipY)
		RealCenterY = -RealCenterY - Height - 1;

	int minX = 0, maxX = 0, minY = 0, maxY = 0,
		c0x, c0y,
		mcos = Math::Cos(-angle), msin = Math::Sin(-angle),
		left = RealCenterX, top = RealCenterY;
	c0x = ((left) * mcos - (top) * msin); minX = Math::Min(minX, c0x); maxX = Math::Max(maxX, c0x);
	c0y = ((left) * msin + (top) * mcos); minY = Math::Min(minY, c0y); maxY = Math::Max(maxY, c0y);
	left += Width;
	c0x = ((left) * mcos - (top) * msin); minX = Math::Min(minX, c0x); maxX = Math::Max(maxX, c0x);
	c0y = ((left) * msin + (top) * mcos); minY = Math::Min(minY, c0y); maxY = Math::Max(maxY, c0y);
	top += Height;
	c0x = ((left) * mcos - (top) * msin); minX = Math::Min(minX, c0x); maxX = Math::Max(maxX, c0x);
	c0y = ((left) * msin + (top) * mcos); minY = Math::Min(minY, c0y); maxY = Math::Max(maxY, c0y);
	left = RealCenterX;
	c0x = ((left) * mcos - (top) * msin); minX = Math::Min(minX, c0x); maxX = Math::Max(maxX, c0x);
	c0y = ((left) * msin + (top) * mcos); minY = Math::Min(minY, c0y); maxY = Math::Max(maxY, c0y);

	minX >>= 16; maxX >>= 16;
	minY >>= 16; maxY >>= 16;

	size = (maxX - minX + 1) * (maxY - minY + 1);

	minX *= scaleW; maxX *= scaleW;
	minY *= scaleH; maxY *= scaleH;
	size *= scaleW * scaleH;

	minX >>= 8; maxX >>= 8;
	minY >>= 8; maxY >>= 8;
	size >>= 16;

	// DrawRectangle(minX + CenterX, minY + CenterY, maxX - minX + 1, maxY - minY + 1, 0xFF00FF);


	mcos = Math::Cos(angle);
	msin = Math::Sin(angle);

	if (!AltPal && CenterY + DY + minY >= WaterPaletteStartLine)
		AltPal = true, Pal = sprite->PaletteAlt;

	int NPX, NPY, X, Y, FX, FY;
	for (int D = 0; D < size; D++) {
		PX = DX + minX;
		PY = DY + minY;

		NPX = PX;
		NPY = PY;
		NPX = (NPX << 8) / scaleW;
		NPY = (NPY << 8) / scaleH;
		X = (NPX * mcos - NPY * msin) >> 16;
		Y = (NPX * msin + NPY * mcos) >> 16;

		FX = CenterX + PX;
		FY = CenterY + PY;
		if (FX <  Clip[0]) goto CONTINUE;
		if (FY <  Clip[1]) goto CONTINUE;
		if (FX >= Clip[2]) goto CONTINUE;
		if (FY >= Clip[3]) return;

		if (X >= RealCenterX && Y >= RealCenterY && X <= RealCenterX + Width && Y <= RealCenterY + Height) {
			int SX = X - RealCenterX;
			int SY = Y - RealCenterY;
			if (flipX)
				SX = Width - SX;
			if (flipY)
				SY = Height - SY;

			PixelIndex = GetPixelIndex(sprite, SrcX + SX, SrcY + SY);
			if (sprite->PaletteCount) {
				if (PixelIndex != sprite->TransparentColorIndex)
					SetPixelFunction(FX, FY, GetPixelOutputColor(Pal[PixelIndex]));
					// SetPixelFunction(FX, FY, Pal[PixelIndex]);
			}
			else if (PixelIndex & 0xFF000000U) {
				SetPixelFunction(FX, FY, GetPixelOutputColor(PixelIndex));
				// SetPixelFunction(FX, FY, PixelIndex);
			}
		}

		CONTINUE:

		DX++;
		if (DX >= maxX - minX + 1) {
			DX = 0, DY++;

			if (!AltPal && CenterY + PY >= WaterPaletteStartLine)
				AltPal = true, Pal = sprite->PaletteAlt;
		}
	}
	//*/
}

PUBLIC STATIC void    SoftwareRenderer::DrawSpriteSizedTransformed(ISprite* sprite, int animation, int frame, int x, int y, bool flipX, bool flipY, int width, int height, int angle) {
	if (Graphics::SpriteRangeCheck(sprite, animation, frame)) return;

	// ignores X/Y offsets of sprite
}
PUBLIC STATIC void    SoftwareRenderer::DrawSpriteAtlas(ISprite* sprite, int x, int y, int pivotX, int pivotY, bool flipX, bool flipY) {
	if (!sprite) return;

	// bool AltPal = false;
	// Uint32 PixelIndex, *Pal = sprite->Palette;
	//
	// int DX = 0, DY = 0, PX, PY, size = sprite->Width * sprite->Height;
	// int CenterX = x, CenterY = y, RealCenterX = -pivotX, RealCenterY = -pivotY;
	// int SrcX = 0, SrcY = 0, Width = sprite->Width - 1, Height = sprite->Height - 1;
	//
	// if (!AltPal && CenterY + RealCenterY >= WaterPaletteStartLine)
	// 	AltPal = true, Pal = sprite->PaletteAlt;
	//
	// for (int D = 0; D < size; D++) {
	// 	PX = DX;
	// 	PY = DY;
	// 	if (flipX)
	// 		PX = Width - PX;
	// 	if (flipY)
	// 		PY = Height - PY;
	//
	// 	PixelIndex = GetPixelIndex(sprite, SrcX + PX, SrcY + PY);
	// 	if (sprite->PaletteCount) {
	// 		if (PixelIndex != sprite->TransparentColorIndex)
	// 			SetPixel(CenterX + DX + RealCenterX, CenterY + DY + RealCenterY, Pal[PixelIndex]);
	// 	}
	// 	else if (PixelIndex & 0xFF000000U) {
	// 		SetPixel(CenterX + DX + RealCenterX, CenterY + DY + RealCenterY, PixelIndex);
	// 	}
	//
	// 	DX++;
	// 	if (DX > Width) {
	// 		DX = 0, DY++;
	//
	// 		if (!AltPal && CenterY + DY + RealCenterY >= WaterPaletteStartLine)
	// 			AltPal = true, Pal = sprite->PaletteAlt;
	// 	}
	// }
}

PUBLIC STATIC void    SoftwareRenderer::DrawSpritePart(ISprite* sprite, int animation, int frame, int srcx, int srcy, int srcw, int srch, int x, int y, bool flipX, bool flipY) {
	if (Graphics::SpriteRangeCheck(sprite, animation, frame)) return;

	/*

	bool AltPal = false;
	Uint32 PixelIndex, *Pal = sprite->Palette;
	AnimFrame animframe = sprite->Animations[animation].Frames[frame];

	srcw = Math::Min(srcw, animframe.Width- srcx);
	srch = Math::Min(srch, animframe.H - srcy);

	int DX = 0, DY = 0, PX, PY, size = srcw * srch;
	int CenterX = x, CenterY = y, RealCenterX = animframe.OffX, RealCenterY = animframe.OffY;
	int SrcX = animframe.X + srcx, SrcY = animframe.Y + srcy, Width = srcw - 1, Height = srch - 1;

	if (flipX)
		RealCenterX = -RealCenterX - Width - 1;
	if (flipY)
		RealCenterY = -RealCenterY - Height - 1;

	if (!AltPal && CenterY + RealCenterY >= WaterPaletteStartLine)
		AltPal = true, Pal = sprite->PaletteAlt;

	if (CenterX + Width + RealCenterX < Clip[0]) return;
	if (CenterY + Height + RealCenterY < Clip[1]) return;
	if (CenterX + RealCenterX >= Clip[2]) return;
	if (CenterY + RealCenterY >= Clip[3]) return;

	int FX, FY;
	for (int D = 0; D < size; D++) {
		PX = DX;
		PY = DY;
		if (flipX)
			PX = Width - PX;
		if (flipY)
			PY = Height - PY;

		FX = CenterX + DX + RealCenterX;
		FY = CenterY + DY + RealCenterY;
		if (FX <  Clip[0]) goto CONTINUE;
		if (FY <  Clip[1]) goto CONTINUE;
		if (FX >= Clip[2]) goto CONTINUE;
		if (FY >= Clip[3]) return;

		PixelIndex = GetPixelIndex(sprite, SrcX + PX, SrcY + PY);
		if (sprite->PaletteCount) {
			if (PixelIndex != sprite->TransparentColorIndex)
				SetPixelFunction(FX, FY, GetPixelOutputColor(Pal[PixelIndex]));
				// SetPixelFunction(FX, FY, Pal[PixelIndex]);
		}
		else if (PixelIndex & 0xFF000000U) {
			SetPixelFunction(FX, FY, GetPixelOutputColor(PixelIndex));
			// SetPixelFunction(FX, FY, PixelIndex);
		}

		CONTINUE:

		DX++;
		if (DX > Width) {
			DX = 0, DY++;

			if (!AltPal && CenterY + DY + RealCenterY >= WaterPaletteStartLine)
				AltPal = true, Pal = sprite->PaletteAlt;
		}
	}
	//*/
}

// Drawing Text
PUBLIC STATIC void    SoftwareRenderer::DrawText(int x, int y, const char* string, unsigned int pixel) {
	pixel = GetPixelOutputColor(pixel);
	// for (size_t i = 0; i < strlen(string); i++) {
	// 	for (int by = 0; by < 8; by++) {
	// 		for (int bx = 0; bx < 8; bx++) {
	// 			if (Font8x8_basic[(int)string[i]][by] & 1 << bx) {
	// 				SetPixel(x + bx + i * 8, y + by, pixel);
	// 			}
	// 		}
	// 	}
	// }
}
PUBLIC STATIC void    SoftwareRenderer::DrawTextShadow(int x, int y, const char* string, unsigned int pixel) {
	DrawText(x + 1, y + 1, string, 0x000000);
	DrawText(x, y, string, pixel);
}

PUBLIC STATIC void    SoftwareRenderer::DrawTextSprite(ISprite* sprite, int animation, char first, int x, int y, const char* string) {
	AnimFrame animframe;
	for (int i = 0; i < (int)strlen(string); i++) {
		animframe = sprite->Animations[animation].Frames[string[i] - first];
		// DrawSpriteNormal(sprite, animation, string[i] - first, x - animframe.OffsetX, y, false, false);

		x += animframe.Width;
	}
}
PUBLIC STATIC int     SoftwareRenderer::MeasureTextSprite(ISprite* sprite, int animation, char first, const char* string) {
	int x = 0;
	AnimFrame animframe;
	for (int i = 0; i < (int)strlen(string); i++) {
		animframe = sprite->Animations[animation].Frames[string[i] - first];
		x += animframe.Width;
	}
	return x;
}

// Drawing 3D
PUBLIC STATIC void    SoftwareRenderer::DrawModelOn2D(IModel* model, int x, int y, double scale, int rx, int ry, int rz, Uint32 color, bool wireframe) {
	// rx &= 0xFF;
	// IMatrix3 rotateX = IMatrix3(
	// 	1, 0, 0,
	// 	0, Math::Cos(rx), Math::Sin(rx),
	// 	0, -Math::Sin(rx), Math::Cos(rx));
    //
	// ry &= 0xFF;
	// IMatrix3 rotateY = IMatrix3(
	// 	Math::Cos(ry), 0, -Math::Sin(ry),
	// 	0, 1, 0,
	// 	Math::Sin(ry), 0, Math::Cos(ry));
    //
	// rz &= 0xFF;
	// IMatrix3 rotateZ = IMatrix3(
	// 	Math::Cos(rz), -Math::Sin(rz), 0,
	// 	Math::Sin(rz), Math::Cos(rz), 0,
	// 	0, 0, 1);
    //
	// IMatrix3 transform = rotateX.Multiply(rotateY).Multiply(rotateZ);
    //
	// int size = FrameBufferSize;
	// double* zBuffer = (double*)malloc(size * sizeof(double));
	// for (int q = 0; q < size; q++) {
	// 	zBuffer[q] = -9999999999.9f;
	// }
    //
	// int wHalf = Application::WIDTH / 2;
	// int hHalf = Application::HEIGHT / 2;
    //
	// int frame = 0;
    //
	// for (int i = 0; i < (int)model->Faces.size(); i++) {
	// 	IFace t = model->Faces[i];
    //
	// 	if (t.Quad && !wireframe) {
	// 		IVertex v1 = transform.Transform(model->Vertices[frame][t.v1]).Multiply(scale, scale, scale);
	// 		IVertex v2 = transform.Transform(model->Vertices[frame][t.v3]).Multiply(scale, scale, scale);
	// 		IVertex v3 = transform.Transform(model->Vertices[frame][t.v4]).Multiply(scale, scale, scale);
    //
	// 		// Offset model
	// 		v1.x += x;
	// 		v1.y += y;
	// 		v2.x += x;
	// 		v2.y += y;
	// 		v3.x += x;
	// 		v3.y += y;
    //
	// 		IVertex n1 = transform.Transform(model->Normals[frame][t.v1].Normalize());
	// 		IVertex n2 = transform.Transform(model->Normals[frame][t.v3].Normalize());
	// 		IVertex n3 = transform.Transform(model->Normals[frame][t.v4].Normalize());
    //
	// 		IVertex varying_intensity;
	// 		IVertex lightdir = IVertex(0.0, -1.0, 0.0);
    //
	// 		varying_intensity.x = fmax(0.0, lightdir.DotProduct(n1));
	// 		varying_intensity.y = fmax(0.0, lightdir.DotProduct(n2));
	// 		varying_intensity.z = fmax(0.0, lightdir.DotProduct(n3));
    //
	// 		double intensity = varying_intensity.Distance();
    //
	// 		int minX = (int)fmax(0.0, ceil(fmin(v1.x, fmin(v2.x, v3.x))));
	// 		int maxX = (int)fmin(wHalf * 2 - 1.0, floor(fmax(v1.x, fmax(v2.x, v3.x))));
	// 		int minY = (int)fmax(0.0, ceil(fmin(v1.y, fmin(v2.y, v3.y))));
	// 		int maxY = (int)fmin(hHalf * 2 - 1.0, floor(fmax(v1.y, fmax(v2.y, v3.y))));
    //
	// 		double triangleArea = (v1.y - v3.y) * (v2.x - v3.x) + (v2.y - v3.y) * (v3.x - v1.x);
    //
	// 		for (int y = minY; y <= maxY; y++) {
	// 			for (int x = minX; x <= maxX; x++) {
	// 				double b1 = ((y - v3.y) * (v2.x - v3.x) + (v2.y - v3.y) * (v3.x - x)) / triangleArea;
	// 				double b2 = ((y - v1.y) * (v3.x - v1.x) + (v3.y - v1.y) * (v1.x - x)) / triangleArea;
	// 				double b3 = ((y - v2.y) * (v1.x - v2.x) + (v1.y - v2.y) * (v2.x - x)) / triangleArea;
	// 				if (b1 >= 0 && b1 <= 1 && b2 >= 0 && b2 <= 1 && b3 >= 0 && b3 <= 1) {
	// 					double depth = b1 * v1.z + b2 * v2.z + b3 * v3.z;
    //
	// 					intensity = (double)varying_intensity.DotProduct(b1, b2, b3);
    //
	// 					int zIndex = y * wHalf * 2 + x;
	// 					if (zBuffer[zIndex] < depth) {
	// 						if (intensity > 0.5)
	// 							SetPixel(x, y, ColorBlend(color, 0xFFFFFF, intensity * 2.0 - 1.0));
	// 						else
	// 							SetPixel(x, y, ColorBlend(color, 0x000000, (0.5 - intensity)));
	// 						zBuffer[zIndex] = depth;
	// 					}
	// 				}
	// 			}
	// 		}
	// 	}
    //
	// 	IVertex v1 = transform.Transform(model->Vertices[frame][t.v1]).Multiply((float)scale, (float)scale, (float)scale);
	// 	IVertex v2 = transform.Transform(model->Vertices[frame][t.v2]).Multiply((float)scale, (float)scale, (float)scale);
	// 	IVertex v3 = transform.Transform(model->Vertices[frame][t.v3]).Multiply((float)scale, (float)scale, (float)scale);
    //
	// 	// Offset model
	// 	v1.x += x;
	// 	v1.y += y;
	// 	v2.x += x;
	// 	v2.y += y;
	// 	v3.x += x;
	// 	v3.y += y;
    //
	// 	IVertex n1 = transform.Transform(model->Normals[frame][t.v1].Normalize());
	// 	IVertex n2 = transform.Transform(model->Normals[frame][t.v2].Normalize());
	// 	IVertex n3 = transform.Transform(model->Normals[frame][t.v3].Normalize());
    //
	// 	IVertex varying_intensity;
	// 	IVertex lightdir = IVertex(0.0, -1.0, 0.0);
    //
	// 	varying_intensity.x = fmax(0.0f, lightdir.DotProduct(n1));
	// 	varying_intensity.y = fmax(0.0f, lightdir.DotProduct(n2));
	// 	varying_intensity.z = fmax(0.0f, lightdir.DotProduct(n3));
    //
	// 	double intensity;// = varying_intensity.Distance();
    //
	// 	int minX = (int)fmax(0.0, ceil(fmin(v1.x, fmin(v2.x, v3.x))));
	// 	int maxX = (int)fmin(wHalf * 2 - 1.0, floor(fmax(v1.x, fmax(v2.x, v3.x))));
	// 	int minY = (int)fmax(0.0, ceil(fmin(v1.y, fmin(v2.y, v3.y))));
	// 	int maxY = (int)fmin(hHalf * 2 - 1.0, floor(fmax(v1.y, fmax(v2.y, v3.y))));
    //
	// 	if (wireframe) {
	// 		intensity = fmin(1.0, fmax(0.0, varying_intensity.Distance()));
    //
	// 		if (intensity > 0.5) {
	// 			DrawLine((int)v1.x, (int)v1.y, (int)v2.x, (int)v2.y, ColorBlend(color, 0xFFFFFF, intensity * 2.0 - 1.0));
	// 			DrawLine((int)v3.x, (int)v3.y, (int)v2.x, (int)v2.y, ColorBlend(color, 0xFFFFFF, intensity * 2.0 - 1.0));
	// 		}
	// 		else {
	// 			DrawLine((int)v1.x, (int)v1.y, (int)v2.x, (int)v2.y, ColorBlend(color, 0x000000, (0.5 - intensity)));
	// 			DrawLine((int)v3.x, (int)v3.y, (int)v2.x, (int)v2.y, ColorBlend(color, 0x000000, (0.5 - intensity)));
	// 		}
	// 	}
	// 	else {
	// 		double triangleArea = (v1.y - v3.y) * (v2.x - v3.x) + (v2.y - v3.y) * (v3.x - v1.x);
    //
	// 		for (int y = minY; y <= maxY; y++) {
	// 			for (int x = minX; x <= maxX; x++) {
	// 				double b1 = ((y - v3.y) * (v2.x - v3.x) + (v2.y - v3.y) * (v3.x - x)) / triangleArea;
	// 				double b2 = ((y - v1.y) * (v3.x - v1.x) + (v3.y - v1.y) * (v1.x - x)) / triangleArea;
	// 				double b3 = ((y - v2.y) * (v1.x - v2.x) + (v1.y - v2.y) * (v2.x - x)) / triangleArea;
	// 				if (b1 >= 0 && b1 <= 1 && b2 >= 0 && b2 <= 1 && b3 >= 0 && b3 <= 1) {
	// 					double depth = b1 * v1.z + b2 * v2.z + b3 * v3.z;
	// 					// b1, b2, b3 make up "bar"; a Vector3
    //
	// 					// fragment
	// 					intensity = varying_intensity.DotProduct((float)b1, (float)b2, (float)b3);
    //
	// 					int zIndex = y * wHalf * 2 + x;
	// 					if (zBuffer[zIndex] < depth) {
	// 						if (intensity > 0.5)
	// 							SetPixel(x, y, ColorBlend(color, 0xFFFFFF, intensity * 2.0 - 1.0));
	// 						else
	// 							SetPixel(x, y, ColorBlend(color, 0x000000, (0.5 - intensity)));
	// 						zBuffer[zIndex] = depth;
	// 					}
	// 				}
	// 			}
	// 		}
	// 	}
	// }
    //
	// free(zBuffer);
}
PUBLIC STATIC void    SoftwareRenderer::DrawSpriteIn3D(ISprite* sprite, int animation, int frame, int x, int y, int z, double scale, int rx, int ry, int rz) {
	// rx &= 0xFF;
	// IMatrix3 rotateX = IMatrix3(
	// 	1, 0, 0,
	// 	0, Math::Cos(rx), Math::Sin(rx),
	// 	0, -Math::Sin(rx), Math::Cos(rx));
    //
	// ry &= 0xFF;
	// IMatrix3 rotateY = IMatrix3(
	// 	Math::Cos(ry), 0, -Math::Sin(ry),
	// 	0, 1, 0,
	// 	Math::Sin(ry), 0, Math::Cos(ry));
    //
	// rz &= 0xFF;
	// IMatrix3 rotateZ = IMatrix3(
	// 	Math::Cos(rz), -Math::Sin(rz), 0,
	// 	Math::Sin(rz), Math::Cos(rz), 0,
	// 	0, 0, 1);
    //
	// IMatrix3 transform = rotateX.Multiply(rotateY).Multiply(rotateZ);
    //
	// int size = FrameBufferSize;
	// double* zBuffer = (double*)malloc(size * sizeof(double));
	// for (int q = 0; q < size; q++) {
	// 	zBuffer[q] = -9999999999.9f;
	// }
    //
	// int wHalf = Application::WIDTH / 2;
	// int hHalf = Application::HEIGHT / 2;
    //
	// IFace t(1, 0, 2, 3);
	// vector<IVertex> Vertices;
	// vector<IVertex2> UVs;
    //
	// IMatrix2x3 varying_uv;
    //
	// Vertices.push_back(IVertex(0.0, 0.0, -0.5));
	// Vertices.push_back(IVertex(1.0, 0.0, -0.5));
	// Vertices.push_back(IVertex(0.0, 1.0, -0.5));
	// Vertices.push_back(IVertex(1.0, 1.0, -0.5));
    //
	// UVs.push_back(IVertex2(0.0, 0.0));
	// UVs.push_back(IVertex2(1.0, 0.0));
	// UVs.push_back(IVertex2(0.0, 1.0));
	// UVs.push_back(IVertex2(1.0, 1.0));
    //
	// /*
	// Sprite.Animations[CurrentAnimation].Frames[Frame >> 8].X,
	// Sprite.Animations[CurrentAnimation].Frames[Frame >> 8].Y,
	// Sprite.Animations[CurrentAnimation].Frames[Frame >> 8].W,
	// Sprite.Animations[CurrentAnimation].Frames[Frame >> 8].H, X - CamX, Y - CamY, 0, IE_NOFLIP,
	// Sprite.Animations[CurrentAnimation].Frames[Frame >> 8].OffX,
	// Sprite.Animations[CurrentAnimation].Frames[Frame >> 8].OffY
	// */
    //
	// AnimFrame currentFrame = sprite->Animations[animation].Frames[frame];
    //
	// x += currentFrame.OffX;
	// y += currentFrame.OffY;
    //
	// if (t.Quad) {
	// 	IVertex v1 = transform.Transform(Vertices[t.v1]).Multiply((float)scale, (float)scale, (float)scale);
	// 	IVertex v2 = transform.Transform(Vertices[t.v3]).Multiply((float)scale, (float)scale, (float)scale);
	// 	IVertex v3 = transform.Transform(Vertices[t.v4]).Multiply((float)scale, (float)scale, (float)scale);
    //
	// 	varying_uv.SetColumn(0, UVs[t.v1]);
	// 	varying_uv.SetColumn(1, UVs[t.v3]);
	// 	varying_uv.SetColumn(2, UVs[t.v4]);
    //
	// 	v1 = v1.Multiply((float)currentFrame.W, (float)currentFrame.H, 1.0);
	// 	v2 = v2.Multiply((float)currentFrame.W, (float)currentFrame.H, 1.0);
	// 	v3 = v3.Multiply((float)currentFrame.W, (float)currentFrame.H, 1.0);
    //
	// 	// Offset model
	// 	v1.x += x;
	// 	v1.y += y;
	// 	v2.x += x;
	// 	v2.y += y;
	// 	v3.x += x;
	// 	v3.y += y;
    //
	// 	int minX = (int)fmax(0.0, ceil(fmin(v1.x, fmin(v2.x, v3.x))));
	// 	int maxX = (int)fmin(wHalf * 2 - 1.0, floor(fmax(v1.x, fmax(v2.x, v3.x))));
	// 	int minY = (int)fmax(0.0, ceil(fmin(v1.y, fmin(v2.y, v3.y))));
	// 	int maxY = (int)fmin(hHalf * 2 - 1.0, floor(fmax(v1.y, fmax(v2.y, v3.y))));
    //
	// 	double triangleArea = (v1.y - v3.y) * (v2.x - v3.x) + (v2.y - v3.y) * (v3.x - v1.x);
    //
	// 	for (int y = minY; y <= maxY; y++) {
	// 		for (int x = minX; x <= maxX; x++) {
	// 			double b1 = ((y - v3.y) * (v2.x - v3.x) + (v2.y - v3.y) * (v3.x - x)) / triangleArea;
	// 			double b2 = ((y - v1.y) * (v3.x - v1.x) + (v3.y - v1.y) * (v1.x - x)) / triangleArea;
	// 			double b3 = ((y - v2.y) * (v1.x - v2.x) + (v1.y - v2.y) * (v2.x - x)) / triangleArea;
	// 			if (b1 >= 0 && b1 <= 1 && b2 >= 0 && b2 <= 1 && b3 >= 0 && b3 <= 1) {
	// 				double depth = b1 * v1.z + b2 * v2.z + b3 * v3.z;
    //
	// 				IVertex2 uv = varying_uv.Transform(IVertex((float)b1, (float)b2, (float)b3));
	// 				Uint32 color = GetPixelIndex(sprite,
	// 					currentFrame.X + (int)(uv.x * (currentFrame.W - 1)),
	// 					currentFrame.Y + (int)(uv.y * (currentFrame.H - 1)));
	// 				// color = sprite->Palette[color];
    //
	// 				int zIndex = y * wHalf * 2 + x;
	// 				if (zBuffer[zIndex] < depth) {
	// 					SetPixel(x, y, color);
	// 					zBuffer[zIndex] = depth;
	// 				}
	// 			}
	// 		}
	// 	}
	// }
    //
	// IVertex v1 = transform.Transform(Vertices[t.v1]).Multiply((float)scale, (float)scale, (float)scale);
	// IVertex v2 = transform.Transform(Vertices[t.v2]).Multiply((float)scale, (float)scale, (float)scale);
	// IVertex v3 = transform.Transform(Vertices[t.v3]).Multiply((float)scale, (float)scale, (float)scale);
    //
	// varying_uv.SetColumn(0, UVs[t.v1]);
	// varying_uv.SetColumn(1, UVs[t.v2]);
	// varying_uv.SetColumn(2, UVs[t.v3]);
    //
	// v1 = v1.Multiply((float)currentFrame.Width, (float)currentFrame.Height, 1.0);
	// v2 = v2.Multiply((float)currentFrame.Width, (float)currentFrame.Height, 1.0);
	// v3 = v3.Multiply((float)currentFrame.Width, (float)currentFrame.Height, 1.0);
	// // v1 = v1.Multiply(currentFrame.W - 1, currentFrame.H - 1, 1.0);
	// // v2 = v2.Multiply(currentFrame.W - 1, currentFrame.H - 1, 1.0);
	// // v3 = v3.Multiply(currentFrame.W - 1, currentFrame.H - 1, 1.0);
    //
	// // Offset model
	// v1.x += x;
	// v1.y += y;
	// v2.x += x;
	// v2.y += y;
	// v3.x += x;
	// v3.y += y;
    //
	// int minX = (int)fmax(0.0, ceil(fmin(v1.x, fmin(v2.x, v3.x))));
	// int maxX = (int)fmin(wHalf * 2 - 1.0, floor(fmax(v1.x, fmax(v2.x, v3.x))));
	// int minY = (int)fmax(0.0, ceil(fmin(v1.y, fmin(v2.y, v3.y))));
	// int maxY = (int)fmin(hHalf * 2 - 1.0, floor(fmax(v1.y, fmax(v2.y, v3.y))));
    //
	// double triangleArea = (v1.y - v3.y) * (v2.x - v3.x) + (v2.y - v3.y) * (v3.x - v1.x);
    //
	// for (int y = minY; y <= maxY; y++) {
	// 	for (int x = minX; x <= maxX; x++) {
	// 		double b1 = ((y - v3.y) * (v2.x - v3.x) + (v2.y - v3.y) * (v3.x - x)) / triangleArea;
	// 		double b2 = ((y - v1.y) * (v3.x - v1.x) + (v3.y - v1.y) * (v1.x - x)) / triangleArea;
	// 		double b3 = ((y - v2.y) * (v1.x - v2.x) + (v1.y - v2.y) * (v2.x - x)) / triangleArea;
	// 		if (b1 >= 0 && b1 <= 1 && b2 >= 0 && b2 <= 1 && b3 >= 0 && b3 <= 1) {
	// 			double depth = b1 * v1.z + b2 * v2.z + b3 * v3.z;
    //
	// 			IVertex2 uv = varying_uv.Transform(IVertex(b1, b2, b3));
	// 			Uint32 color = GetPixelIndex(sprite,
	// 				currentFrame.X + (int)(uv.x * (currentFrame.W - 1)),
	// 				currentFrame.Y + (int)(uv.y * (currentFrame.H - 1)));
	// 			// color = sprite->Palette[color];
    //
	// 			int zIndex = y * wHalf * 2 + x;
	// 			if (zBuffer[zIndex] < depth) {
	// 				SetPixel(x, y, color);
	// 				zBuffer[zIndex] = depth;
	// 			}
	// 		}
	// 	}
	// }
    //
	// free(zBuffer);
}

// Utility Functions
PUBLIC STATIC Uint32  SoftwareRenderer::ColorAdd(Uint32 color1, Uint32 color2, int percent) {
	Uint32 r = color1 >> 16 & 0xFF;
	Uint32 g = color1 >> 8 & 0xFF;
	Uint32 b = color1 & 0xFF;

	r += (color2 >> 16 & 0xFF) * percent >> 8;
	g += (color2 >> 8 & 0xFF) * percent >> 8;
	b += (color2 & 0xFF) * percent >> 8;

	if (r > 0xFF) r = 0xFF;
	if (g > 0xFF) g = 0xFF;
	if (b > 0xFF) b = 0xFF;

	return r << 16 | g << 8 | b;
}
PUBLIC STATIC Uint32  SoftwareRenderer::ColorBlend(Uint32 color1, Uint32 color2, int percent) {
	Uint32 rb = color1 & 0xff00ff;
	Uint32 g = color1 & 0x00ff00;
	rb += ((color2 & 0xff00ff) - rb) * percent >> 8;
	g += ((color2 & 0x00ff00) - g) * percent >> 8;
	return (rb & 0xff00ff) | (g & 0xff00);
}

PUBLIC STATIC void    SoftwareRenderer::ScanLine(long x1, long y1, long x2, long y2) {
	long sx, sy, dx1, dy1, dx2, dy2, x, y, m, n, k, cnt;

	sx = x2 - x1;
	sy = y2 - y1;

	if (sx > 0) dx1 = 1;
	else if (sx < 0) dx1 = -1;
	else dx1 = 0;

	if (sy > 0) dy1 = 1;
	else if (sy < 0) dy1 = -1;
	else dy1 = 0;

	m = labs(sx);
	n = labs(sy);
	dx2 = dx1;
	dy2 = 0;

	if (m < n) {
		m = labs(sy);
		n = labs(sx);
		dx2 = 0;
		dy2 = dy1;
	}

	x = x1;
	y = y1;
	cnt = m;
	k = n / 2;

	// while (cnt--) {
	// 	if ((y >= 0) && (y < Application::HEIGHT)) {
	// 		if (x < ContourX[y][0]) ContourX[y][0] = x;
	// 		if (x > ContourX[y][1]) ContourX[y][1] = x;
	// 	}
    //
	// 	k += n;
	// 	if (k < m) {
	// 		x += dx2;
	// 		y += dy2;
	// 	}
	// 	else {
	// 		k -= m;
	// 		x += dx1;
	// 		y += dy1;
	// 	}
	// }
}

/*
// PUBLIC STATIC void   SoftwareRenderer::DrawSpritePart(ISprite* sprite, int animation, int frame, int srcx, int srcy, int srcw, int srch, int x, int y, bool flipX, bool flipY) {
// 	if (Graphics::SpriteRangeCheck(sprite, animation, frame)) return;
//
// 	bool AltPal = false;
// 	Uint32 PixelIndex, *Pal = sprite->Palette;
// 	AnimFrame animframe = sprite->Animations[animation].Frames[frame];
//
// 	srcw = Math::Min(srcw, animframe.Width- srcx);
// 	srch = Math::Min(srch, animframe.H - srcy);
//
// 	int DX = 0, DY = 0, PX, PY, size = srcw * srch;
// 	int CenterX = x, CenterY = y, RealCenterX = animframe.OffX, RealCenterY = animframe.OffY;
// 	int SrcX = animframe.X + srcx, SrcY = animframe.Y + srcy, Width = srcw - 1, Height = srch - 1;
//
// 	if (flipX)
// 		RealCenterX = -RealCenterX - Width - 1;
// 	if (flipY)
// 		RealCenterY = -RealCenterY - Height - 1;
//
// 	if (!AltPal && CenterY + RealCenterY >= WaterPaletteStartLine)
// 		AltPal = true, Pal = sprite->PaletteAlt;
//
// 	if (CenterX + Width + RealCenterX < Clip[0]) return;
// 	if (CenterY + Height + RealCenterY < Clip[1]) return;
// 	if (CenterX + RealCenterX >= Clip[2]) return;
// 	if (CenterY + RealCenterY >= Clip[3]) return;
//
// 	int FX, FY;
// 	for (int D = 0; D < size; D++) {
// 		PX = DX;
// 		PY = DY;
// 		if (flipX)
// 			PX = Width - PX;
// 		if (flipY)
// 			PY = Height - PY;
//
// 		FX = CenterX + DX + RealCenterX;
// 		FY = CenterY + DY + RealCenterY;
// 		if (FX <  Clip[0]) goto CONTINUE;
// 		if (FY <  Clip[1]) goto CONTINUE;
// 		if (FX >= Clip[2]) goto CONTINUE;
// 		if (FY >= Clip[3]) return;
//
// 		PixelIndex = GetPixelIndex(sprite, SrcX + PX, SrcY + PY);
// 		if (sprite->PaletteCount) {
// 			if (PixelIndex != sprite->TransparentColorIndex)
// 				SetPixelFunction(FX, FY, GetPixelOutputColor(Pal[PixelIndex]));
// 				// SetPixelFunction(FX, FY, Pal[PixelIndex]);
// 		}
// 		else if (PixelIndex & 0xFF000000U) {
// 			SetPixelFunction(FX, FY, GetPixelOutputColor(PixelIndex));
// 			// SetPixelFunction(FX, FY, PixelIndex);
// 		}
//
// 		CONTINUE:
//
// 		DX++;
// 		if (DX > Width) {
// 			DX = 0, DY++;
//
// 			if (!AltPal && CenterY + DY + RealCenterY >= WaterPaletteStartLine)
// 				AltPal = true, Pal = sprite->PaletteAlt;
// 		}
// 	}
// }
// PUBLIC STATIC void   SoftwareRenderer::DrawSpriteTransformed(ISprite* sprite, int animation, int frame, int x, int y, bool flipX, bool flipY, int scaleW, int scaleH, int angle) {
// 	if (Graphics::SpriteRangeCheck(sprite, animation, frame)) return;
//
// 	AnimFrame animframe = sprite->Animations[animation].Frames[frame];
//
// 	// TODO: Add shortcut here if angle == 0 && scaleW == 0x100 && scaleH = 0x100
// 	// DrawSpriteNormal();
// 	// return;
//
// 	int srcx = 0, srcy = 0, srcw = animframe.W, srch = animframe.H;
// 	if (srcw >= animframe.Width- srcx)
// 		srcw  = animframe.Width- srcx;
// 	if (srch >= animframe.H - srcy)
// 		srch  = animframe.H - srcy;
//
// 	bool AltPal = false;
// 	Uint32 PixelIndex, *Pal = sprite->Palette;
// 	int DX = 0, DY = 0, PX, PY, size;
// 	int CenterX = x, CenterY = y, RealCenterX = animframe.OffX, RealCenterY = animframe.OffY;
// 	int SrcX = animframe.X + srcx, SrcY = animframe.Y + srcy, Width = srcw - 1, Height = srch - 1;
// 	if (flipX)
// 		RealCenterX = -RealCenterX - Width - 1;
// 	if (flipY)
// 		RealCenterY = -RealCenterY - Height - 1;
//
// 	int minX = 0, maxX = 0, minY = 0, maxY = 0,
// 		c0x, c0y,
// 		mcos = Math::Cos(-angle), msin = Math::Sin(-angle),
// 		left = RealCenterX, top = RealCenterY;
// 	c0x = ((left) * mcos - (top) * msin); minX = Math::Min(minX, c0x); maxX = Math::Max(maxX, c0x);
// 	c0y = ((left) * msin + (top) * mcos); minY = Math::Min(minY, c0y); maxY = Math::Max(maxY, c0y);
// 	left += Width;
// 	c0x = ((left) * mcos - (top) * msin); minX = Math::Min(minX, c0x); maxX = Math::Max(maxX, c0x);
// 	c0y = ((left) * msin + (top) * mcos); minY = Math::Min(minY, c0y); maxY = Math::Max(maxY, c0y);
// 	top += Height;
// 	c0x = ((left) * mcos - (top) * msin); minX = Math::Min(minX, c0x); maxX = Math::Max(maxX, c0x);
// 	c0y = ((left) * msin + (top) * mcos); minY = Math::Min(minY, c0y); maxY = Math::Max(maxY, c0y);
// 	left = RealCenterX;
// 	c0x = ((left) * mcos - (top) * msin); minX = Math::Min(minX, c0x); maxX = Math::Max(maxX, c0x);
// 	c0y = ((left) * msin + (top) * mcos); minY = Math::Min(minY, c0y); maxY = Math::Max(maxY, c0y);
//
// 	minX >>= 16; maxX >>= 16;
// 	minY >>= 16; maxY >>= 16;
//
// 	size = (maxX - minX + 1) * (maxY - minY + 1);
//
// 	minX *= scaleW; maxX *= scaleW;
// 	minY *= scaleH; maxY *= scaleH;
// 	size *= scaleW * scaleH;
//
// 	minX >>= 8; maxX >>= 8;
// 	minY >>= 8; maxY >>= 8;
// 	size >>= 16;
//
// 	// DrawRectangle(minX + CenterX, minY + CenterY, maxX - minX + 1, maxY - minY + 1, 0xFF00FF);
//
//
// 	mcos = Math::Cos(angle);
// 	msin = Math::Sin(angle);
//
// 	if (!AltPal && CenterY + DY + minY >= WaterPaletteStartLine)
// 		AltPal = true, Pal = sprite->PaletteAlt;
//
// 	int NPX, NPY, X, Y, FX, FY;
// 	for (int D = 0; D < size; D++) {
// 		PX = DX + minX;
// 		PY = DY + minY;
//
// 		NPX = PX;
// 		NPY = PY;
// 		NPX = (NPX << 8) / scaleW;
// 		NPY = (NPY << 8) / scaleH;
// 		X = (NPX * mcos - NPY * msin) >> 16;
// 		Y = (NPX * msin + NPY * mcos) >> 16;
//
// 		FX = CenterX + PX;
// 		FY = CenterY + PY;
// 		if (FX <  Clip[0]) goto CONTINUE;
// 		if (FY <  Clip[1]) goto CONTINUE;
// 		if (FX >= Clip[2]) goto CONTINUE;
// 		if (FY >= Clip[3]) return;
//
// 		if (X >= RealCenterX && Y >= RealCenterY && X <= RealCenterX + Width && Y <= RealCenterY + Height) {
// 			int SX = X - RealCenterX;
// 			int SY = Y - RealCenterY;
// 			if (flipX)
// 				SX = Width - SX;
// 			if (flipY)
// 				SY = Height - SY;
//
// 			PixelIndex = GetPixelIndex(sprite, SrcX + SX, SrcY + SY);
// 			if (sprite->PaletteCount) {
// 				if (PixelIndex != sprite->TransparentColorIndex)
// 					SetPixelFunction(FX, FY, GetPixelOutputColor(Pal[PixelIndex]));
// 					// SetPixelFunction(FX, FY, Pal[PixelIndex]);
// 			}
// 			else if (PixelIndex & 0xFF000000U) {
// 				SetPixelFunction(FX, FY, GetPixelOutputColor(PixelIndex));
// 				// SetPixelFunction(FX, FY, PixelIndex);
// 			}
// 		}
//
// 		CONTINUE:
//
// 		DX++;
// 		if (DX >= maxX - minX + 1) {
// 			DX = 0, DY++;
//
// 			if (!AltPal && CenterY + PY >= WaterPaletteStartLine)
// 				AltPal = true, Pal = sprite->PaletteAlt;
// 		}
// 	}
// }
*/

// Initialization and disposal functions
PUBLIC STATIC void     SoftwareRenderer::Init() {
    SoftwareRenderer::BackendFunctions.Init();

    // PixelFunction = SetPixelNormal;
	// FilterFunction[0] = FilterNone;
	// FilterFunction[1] = FilterNone;
	// FilterFunction[2] = FilterNone;
	// FilterFunction[3] = FilterNone;
    //
	// Deform = (Sint8*)Memory::TrackedCalloc("SoftwareRenderer::DeformMap", Application::HEIGHT, 1);
    //
	// Clip[0] = 0;
	// Clip[1] = 0;
	// Clip[2] = Application::WIDTH;
	// Clip[3] = Application::HEIGHT;
    // ClipSet = false;
    //
	// for (int i = 0; i < 0x100; i++)
	// 	DivideBy3Table[i] = i / 3;
    //
	// FrameBufferSize = Application::WIDTH * Application::HEIGHT;
	// FrameBuffer = (Uint32*)Memory::TrackedCalloc("SoftwareRenderer::FrameBuffer", FrameBufferSize, sizeof(Uint32));
	// FrameBufferStride = Application::WIDTH;
    //
	// ResourceStream* res;
    // if ((res = ResourceStream::New("Sprites/UI/DevFont.bin"))) {
    // 	res->ReadBytes(BitmapFont, 0x400);
    //     res->Close();
    // }
}
PUBLIC STATIC Uint32   SoftwareRenderer::GetWindowFlags() {
    return SoftwareRenderer::BackendFunctions.GetWindowFlags();
}
PUBLIC STATIC void     SoftwareRenderer::SetGraphicsFunctions(void* baseGFXfunctions) {
    ((void(*)())baseGFXfunctions)();
    SoftwareRenderer::BackendFunctions = Graphics::Internal; // memcpy(&SoftwareRenderer::BackendFunctions, Graphics::Internal, sizeof(Graphics::Internal));

    return;

    // Graphics::Internal.Init = SoftwareRenderer::Init;
    // Graphics::Internal.GetWindowFlags = SoftwareRenderer::GetWindowFlags;
    // Graphics::Internal.Dispose = SoftwareRenderer::Dispose;
    //
    // // Texture management functions
    // Graphics::Internal.CreateTexture = SoftwareRenderer::CreateTexture;
    // Graphics::Internal.LockTexture = SoftwareRenderer::LockTexture;
    // Graphics::Internal.UpdateTexture = SoftwareRenderer::UpdateTexture;
    // Graphics::Internal.UpdateYUVTexture = SoftwareRenderer::UpdateTextureYUV;
    // Graphics::Internal.UnlockTexture = SoftwareRenderer::UnlockTexture;
    // Graphics::Internal.DisposeTexture = SoftwareRenderer::DisposeTexture;
    //
    // // Viewport and view-related functions
    // Graphics::Internal.SetRenderTarget = SoftwareRenderer::SetRenderTarget;
    // Graphics::Internal.UpdateWindowSize = SoftwareRenderer::UpdateWindowSize;
    // Graphics::Internal.UpdateViewport = SoftwareRenderer::UpdateViewport;
    // Graphics::Internal.UpdateClipRect = SoftwareRenderer::UpdateClipRect;
    // Graphics::Internal.UpdateOrtho = SoftwareRenderer::UpdateOrtho;
    // Graphics::Internal.UpdatePerspective = SoftwareRenderer::UpdatePerspective;
    // Graphics::Internal.UpdateProjectionMatrix = SoftwareRenderer::UpdateProjectionMatrix;
    //
    // // Shader-related functions
    // Graphics::Internal.UseShader = SoftwareRenderer::UseShader;
    // Graphics::Internal.SetUniformF = SoftwareRenderer::SetUniformF;
    // Graphics::Internal.SetUniformI = SoftwareRenderer::SetUniformI;
    // Graphics::Internal.SetUniformTexture = SoftwareRenderer::SetUniformTexture;
    //
    // // These guys
    // Graphics::Internal.Clear = SoftwareRenderer::Clear;
    // Graphics::Internal.Present = SoftwareRenderer::Present;
    //
    // // Draw mode setting functions
    // Graphics::Internal.SetBlendColor = SoftwareRenderer::SetBlendColor;
    // Graphics::Internal.SetBlendMode = SoftwareRenderer::SetBlendMode;
    // Graphics::Internal.SetLineWidth = SoftwareRenderer::SetLineWidth;
    //
    // // Primitive drawing functions
    // Graphics::Internal.StrokeLine = SoftwareRenderer::StrokeLine;
    // Graphics::Internal.StrokeCircle = SoftwareRenderer::StrokeCircle;
    // Graphics::Internal.StrokeEllipse = SoftwareRenderer::StrokeEllipse;
    // Graphics::Internal.StrokeRectangle = SoftwareRenderer::StrokeRectangle;
    // Graphics::Internal.FillCircle = SoftwareRenderer::FillCircle;
    // Graphics::Internal.FillEllipse = SoftwareRenderer::FillEllipse;
    // Graphics::Internal.FillTriangle = SoftwareRenderer::FillTriangle;
    // Graphics::Internal.FillRectangle = SoftwareRenderer::FillRectangle;
    //
    // // Texture drawing functions
    // Graphics::Internal.DrawTexture = SoftwareRenderer::DrawTexture;
    // Graphics::Internal.DrawSprite = SoftwareRenderer::DrawSprite;
    // Graphics::Internal.DrawSpritePart = SoftwareRenderer::DrawSpritePart;
    // Graphics::Internal.MakeFrameBufferID = SoftwareRenderer::MakeFrameBufferID;
}
PUBLIC STATIC void     SoftwareRenderer::Dispose() {
	// Memory::Free(Deform);
	// Memory::Free(FrameBuffer);
	// SDL_FreeSurface(WindowScreen);
	// SDL_DestroyTexture(ScreenTexture);
	// SDL_DestroyWindow(Window);
    //
	// Screen->pixels = ScreenOriginalPixels;
	// SDL_FreeSurface(Screen);
    SoftwareRenderer::BackendFunctions.Dispose();
}

// Texture management functions
PUBLIC STATIC Texture* SoftwareRenderer::CreateTexture(Uint32 format, Uint32 access, Uint32 width, Uint32 height) {
    Texture* texture = Texture::New(format, access, width, height);

    return NULL;
}
PUBLIC STATIC int      SoftwareRenderer::LockTexture(Texture* texture, void** pixels, int* pitch) {
    return 0;
}
PUBLIC STATIC int      SoftwareRenderer::UpdateTexture(Texture* texture, SDL_Rect* src, void* pixels, int pitch) {
    return 0;
}
PUBLIC STATIC int      SoftwareRenderer::UpdateYUVTexture(Texture* texture, SDL_Rect* src, Uint8* pixelsY, int pitchY, Uint8* pixelsU, int pitchU, Uint8* pixelsV, int pitchV) {
    return 0;
}
PUBLIC STATIC void     SoftwareRenderer::UnlockTexture(Texture* texture) {

}
PUBLIC STATIC void     SoftwareRenderer::DisposeTexture(Texture* texture) {

}

// Viewport and view-related functions
PUBLIC STATIC void     SoftwareRenderer::SetRenderTarget(Texture* texture) {

}

// Shader-related functions
PUBLIC STATIC void     SoftwareRenderer::UseShader(void* shader) {
    // NOTE: THESE WILL JUST BE A POINTER TO AN ARRAY of 0x10000 Uint16s

    // Override shader
    // if (Graphics::CurrentShader)
    //     shader = Graphics::CurrentShader;
    //
    // if (GLRenderer::CurrentShader != (GLShader*)shader) {
    //     GLRenderer::CurrentShader = (GLShader*)shader;
    //     GLRenderer::CurrentShader->Use();
    //     // glEnableVertexAttribArray(CurrentShader->LocTexCoord);
    //
    //     glActiveTexture(GL_TEXTURE0); CHECK_GL();
    //     glUniform1i(GLRenderer::CurrentShader->LocTexture, 0); CHECK_GL();
    // }
}
PUBLIC STATIC void     SoftwareRenderer::SetUniformF(int location, int count, float* values) {

}
PUBLIC STATIC void     SoftwareRenderer::SetUniformI(int location, int count, int* values) {

}
PUBLIC STATIC void     SoftwareRenderer::SetUniformTexture(Texture* texture, int uniform_index, int slot) {

}

// These guys
PUBLIC STATIC void     SoftwareRenderer::Clear() {

}
PUBLIC STATIC void     SoftwareRenderer::Present() {

}

// Draw mode setting functions
PUBLIC STATIC void     SoftwareRenderer::SetBlendColor(float r, float g, float b, float a) {

}
PUBLIC STATIC void     SoftwareRenderer::SetBlendMode(int srcC, int dstC, int srcA, int dstA) {

}

PUBLIC STATIC void     SoftwareRenderer::Resize(int width, int height) {
    // Deform = (Sint8*)Memory::Realloc(Deform, height);
    // // memset(Deform + last_height, 0, height - last_height);
    //
    // if (!ClipSet) {
    //     Clip[0] = 0;
    // 	Clip[1] = 0;
    // 	Clip[2] = width;
    // 	Clip[3] = height;
    // }
    //
	// FrameBuffer = (Uint32*)Memory::Realloc(FrameBuffer, width * height * sizeof(Uint32));
    // if (width * height - FrameBufferSize > 0)
    //     memset(FrameBuffer + FrameBufferSize, 0, width * height - FrameBufferSize);
    // FrameBufferSize = width * height;
	// FrameBufferStride = width;
}

PUBLIC STATIC void     SoftwareRenderer::SetClip(float x, float y, float width, float height) {
    // ClipSet = true;
    //
    // Clip[0] = (int)x;
	// Clip[1] = (int)y;
	// Clip[2] = (int)x + (int)w;
	// Clip[3] = (int)y + (int)h;
    //
	// if (Clip[0] < 0)
	// 	Clip[0] = 0;
	// if (Clip[1] < 0)
	// 	Clip[1] = 0;
	// if (Clip[2] > Application::WIDTH)
	// 	Clip[2] = Application::WIDTH;
	// if (Clip[3] > Application::HEIGHT)
	// 	Clip[3] = Application::HEIGHT;
}
PUBLIC STATIC void     SoftwareRenderer::ClearClip() {
    // ClipSet = false;
    //
    // Clip[0] = 0;
	// Clip[1] = 0;
	// Clip[2] = Application::WIDTH;
	// Clip[3] = Application::HEIGHT;
}

PUBLIC STATIC void     SoftwareRenderer::Save() {

}
PUBLIC STATIC void     SoftwareRenderer::Translate(float x, float y, float z) {

}
PUBLIC STATIC void     SoftwareRenderer::Rotate(float x, float y, float z) {

}
PUBLIC STATIC void     SoftwareRenderer::Scale(float x, float y, float z) {

}
PUBLIC STATIC void     SoftwareRenderer::Restore() {

}

PUBLIC STATIC void     SoftwareRenderer::SetLineWidth(float n) {

}
PUBLIC STATIC void     SoftwareRenderer::StrokeLine(float x1, float y1, float x2, float y2) {

}
PUBLIC STATIC void     SoftwareRenderer::StrokeCircle(float x, float y, float rad) {

}
PUBLIC STATIC void     SoftwareRenderer::StrokeEllipse(float x, float y, float w, float h) {

}
PUBLIC STATIC void     SoftwareRenderer::StrokeRectangle(float x, float y, float w, float h) {

}

PUBLIC STATIC void     SoftwareRenderer::FillCircle(float x, float y, float rad) {

}
PUBLIC STATIC void     SoftwareRenderer::FillEllipse(float x, float y, float w, float h) {

}
PUBLIC STATIC void     SoftwareRenderer::FillRectangle(float x, float y, float w, float h) {

}

PUBLIC STATIC void     SoftwareRenderer::DrawTexture(Texture* texture, float sx, float sy, float sw, float sh, float x, float y, float w, float h) {

}
PUBLIC STATIC void     SoftwareRenderer::DrawTexture(Texture* texture, float x, float y, float w, float h) {

}
PUBLIC STATIC void     SoftwareRenderer::DrawTexture(Texture* texture, float x, float y) {

}
