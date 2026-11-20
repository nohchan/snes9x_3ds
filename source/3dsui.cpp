//=============================================================================
// Basic user interface framework for low-level drawing operations to
// the bottom screen.
//=============================================================================

#include <cstdio>
#include <cstring>
extern "C" {
#include <3ds/font.h>
}

#include "snes9x.h"

#include "3dssettings.h"
#include "3dslog.h"
#include "3dsfiles.h"
#include "3dsfont.h"
#include "3dsui.h"

#define MAX_ALPHA 8
#define FONT_CELL_WIDTH 16
#define GETFONTBITMAP(c, x, y) fontBitmap[c * 256 + x + (y) * FONT_CELL_WIDTH]

typedef struct
{
    int red[MAX_ALPHA + 1][32];
    int green[MAX_ALPHA + 1][32];
    int blue[MAX_ALPHA + 1][32];
} SAlpha;

static u8 *fontWidthArray[] = { fontTempestaWidth, fontRondaWidth };
static u8 *fontBitmapArray[] = { fontTempestaBitmap, fontRondaBitmap };
static const int fontCount = sizeof(fontBitmapArray) / sizeof(fontBitmapArray[0]);

static u8 *fontBitmap;
static u8 *fontWidth;
static const int fontHeight = FONT_HEIGHT;
static CFNT_s *systemFont;

static bool ui3dsDecodeUtf8At(const char *text, int length, int *offset, u32 *codePoint)
{
    int i = *offset;
    u8 lead = (u8)text[i];
    int extra = (lead >= 0xC2 && lead <= 0xDF) ? 1
              : (lead >= 0xE0 && lead <= 0xEF) ? 2
              : (lead >= 0xF0 && lead <= 0xF4) ? 3 : 0;
    if (extra == 0 || i + extra >= length) {
        *codePoint = lead;
        *offset = i + 1;
        return false;
    }
    u32 cp = lead & (extra == 1 ? 0x1F : extra == 2 ? 0x0F : 0x07);
    for (int j = 1; j <= extra; j++) {
        u8 tail = (u8)text[i + j];
        if ((tail & 0xC0) != 0x80) {
            *codePoint = lead;
            *offset = i + 1;
            return false;
        }
        cp = (cp << 6) | (tail & 0x3F);
    }
    *codePoint = cp;
    *offset = i + extra + 1;
    return true;
}

static int translateX = 0;
static int translateY = 0;
static int viewportX1, viewportY1, viewportX2, viewportY2;

static int viewportStackCount = 0;
static int viewportStack[20][4];

static SAlpha alphas;
static u16 alphas4Bit[MAX_ALPHA + 1];

// Text-colour alpha, rebuilt only when the text colour changes.
static u16 fgAlpha565[MAX_ALPHA + 1];
static int fgAlpha565Color = -1;

// shared between 3dsui_img and 3dsui_notif — never use concurrently
u8* g_texUploadBuffer;

void ui3dsPrepare()
{
    fontEnsureMapped();
    systemFont = fontGetSystemFont();
    for (int a = 0; a <= MAX_ALPHA; a++) {
        alphas4Bit[a] = (a * 15 + 4) >> 3;
    }

    for (int i = 0; i < 32; i++)
    {
        for (int a = 0; a <= MAX_ALPHA; a++)
        {
            int f = i * a / MAX_ALPHA;
            alphas.red[a][i] = f << 11;
            alphas.green[a][i] = f << 6;
            alphas.blue[a][i] = f;
        }
    }

    for (int f = 0; f < fontCount; f++)
    {
        fontBitmap = fontBitmapArray[f];
        fontWidth = fontWidthArray[f];
        for (int i = 0; i < 65536; i++)
        {
            u8 c = fontBitmap[i];
            if (c == ' ')
                fontBitmap[i] = 0;
            else
                fontBitmap[i] = c - '0';
        }
    }

    ui3dsSetFont();
    ui3dsSetScreenLayout();
}
    
void ui3dsSetScreenLayout() {
    if (settings3DS.GameScreen == GFX_TOP) {
	    settings3DS.GameScreenWidth = SCREEN_TOP_WIDTH;
	    settings3DS.SecondScreen = GFX_BOTTOM;
        settings3DS.SecondScreenWidth = SCREEN_BOTTOM_WIDTH;
    } else {
	    settings3DS.GameScreenWidth = SCREEN_BOTTOM_WIDTH;
	    settings3DS.SecondScreen = GFX_TOP;
        settings3DS.SecondScreenWidth = SCREEN_TOP_WIDTH;
    }
    
    viewportX1 = 0;
    viewportY1 = 0;
    viewportX2 = settings3DS.SecondScreenWidth;
    viewportY2 = SCREEN_HEIGHT;
    viewportStackCount = 0;
}

void ui3dsSetFont()
{
    int fontIndex = static_cast<int>(settings3DS.Font);

    if (fontIndex >= 0 && fontIndex < fontCount)
    {
        fontBitmap = fontBitmapArray[fontIndex];
        fontWidth = fontWidthArray[fontIndex];
    }
}

//---------------------------------------------------------------
// Sets the global viewport for all drawing
//---------------------------------------------------------------
void ui3dsSetViewport(int x1, int y1, int x2, int y2)
{
    viewportX1 = x1;
    viewportX2 = x2;
    viewportY1 = y1;
    viewportY2 = y2;

    if (viewportX1 < 0) viewportX1 = 0;
    if (viewportX2 > settings3DS.SecondScreenWidth) viewportX2 = settings3DS.SecondScreenWidth;
    if (viewportY1 < 0) viewportY1 = 0;
    if (viewportY2 > SCREEN_HEIGHT) viewportY2 = SCREEN_HEIGHT;
}

//---------------------------------------------------------------
// Push the global viewport for all drawing
//---------------------------------------------------------------
void ui3dsPushViewport(int x1, int y1, int x2, int y2)
{
    if (viewportStackCount < 10)
    {
        if (x1 < viewportX1) x1 = viewportX1;
        if (x2 > viewportX2) x2 = viewportX2;
        if (y1 < viewportY1) y1 = viewportY1;
        if (y2 > viewportY2) y2 = viewportY2;

        viewportStack[viewportStackCount][0] = viewportX1;
        viewportStack[viewportStackCount][1] = viewportX2;
        viewportStack[viewportStackCount][2] = viewportY1;
        viewportStack[viewportStackCount][3] = viewportY2;
        viewportStackCount++;

        ui3dsSetViewport(x1, y1, x2, y2);
    }
}

//---------------------------------------------------------------
// Pop the global viewport 
//---------------------------------------------------------------
void ui3dsPopViewport()
{
    if (viewportStackCount > 0)
    {
        viewportStackCount--;
        viewportX1 = viewportStack[viewportStackCount][0];
        viewportX2 = viewportStack[viewportStackCount][1];
        viewportY1 = viewportStack[viewportStackCount][2];
        viewportY2 = viewportStack[viewportStackCount][3];
    }
}


//---------------------------------------------------------------
// Applies alpha to a given colour.
// NOTE: Alpha is a value from 0 to 10. (0 = transparent, 10 = opaque)
//---------------------------------------------------------------
inline u16 __attribute__((always_inline)) ui3dsApplyAlphaToColour565(int color565, int alpha)
{
    int red = (color565 >> 11) & 0x1f;
    int green = (color565 >> 6) & 0x1f; // drop the LSB of the green colour
    int blue = (color565) & 0x1f;

    return alphas.red[alpha][red] | alphas.blue[alpha][blue] | alphas.green[alpha][green];
}


inline u16 __attribute__((always_inline)) ui3dsBlendPixel565(u16 fg, u16 bg, int alpha)
{
    return ui3dsApplyAlphaToColour565(fg, alpha) + ui3dsApplyAlphaToColour565(bg, MAX_ALPHA - alpha);
}

//---------------------------------------------------------------
// Sets the global translate for all drawing
//---------------------------------------------------------------
void ui3dsSetTranslate(int tx, int ty)
{
    translateX = tx;
    translateY = ty;
}


//---------------------------------------------------------------
// Computes the frame buffer offset given the x, y
// coordinates.
//---------------------------------------------------------------
inline int __attribute__((always_inline)) ui3dsComputeFrameBufferOffset(int x, int y)
{
    return ((x) * SCREEN_HEIGHT + (239 - y));
}


//---------------------------------------------------------------
// Gets a pixel colour.
//---------------------------------------------------------------
inline u16 __attribute__((always_inline)) ui3dsGetPixelInline(u16 *frameBuffer, int x, int y)
{
    return frameBuffer[ui3dsComputeFrameBufferOffset((x), (y))];  
}


//---------------------------------------------------------------
// Sets a pixel colour.
//---------------------------------------------------------------
inline void __attribute__((always_inline)) ui3dsSetPixelInline(u16 *frameBuffer, int x, int y, int color)
{
    if (color < 0) return;
    if ((x) >= viewportX1 && (x) < viewportX2 && 
        (y) >= viewportY1 && (y) < viewportY2) 
    { 
        frameBuffer[ui3dsComputeFrameBufferOffset((x), (y))] = color;  
    }
}


inline u16 color32toRGBA4(u32 color, u8 alphaIndex)
{
    u32 r = (color >> 24) & 0xFF; // Top byte
    u32 g = (color >> 16) & 0xFF; 
    u32 b = (color >> 8)  & 0xFF; 

    u16 r4 = r >> 4;
    u16 g4 = g >> 4;
    u16 b4 = b >> 4;
    
    // We ignore the 32-bit alpha (color & 0xFF) here
    // instead we use one of those MAX_ALPHA + 1 alpha values from alphas4Bit
    if (alphaIndex > MAX_ALPHA) alphaIndex = MAX_ALPHA;
    u16 a4 = alphas4Bit[alphaIndex];
    
    return (r4 << 12) | (g4 << 8) | (b4 << 4) | a4;
}

//---------------------------------------------------------------
// Draws a single character to the screen
//---------------------------------------------------------------
void ui3dsDrawRGB565_CharToFramebuffer(u16 *frameBuffer, int x, int y, int color565, u8 c)
{
    // Draws a character to the screen at (x,y) 
    // (0,0) is at the top left of the screen.
    //
    if ((y) >= viewportY1 && (y) < viewportY2)
    {
        int wid = fontWidth[c];
        for (int x1 = 0; x1 < wid; x1++)
        {
            int cx = x + x1;
            if (cx < viewportX1 || cx >= viewportX2)
                continue;

            for (int h = 0; h < fontHeight; h++)
            {
                int cy = y + h;
                if (cy < viewportY1 || cy >= viewportY2)
                    continue;

                u8 alpha = GETFONTBITMAP(c, x1, h);
                ui3dsSetPixelInline(frameBuffer, cx, cy,
                    alpha == MAX_ALPHA ? color565 :
                    alpha == 0x0 ? -1 :
                        fgAlpha565[alpha] + ui3dsApplyAlphaToColour565(ui3dsGetPixelInline(frameBuffer, cx, cy), MAX_ALPHA - alpha));
            }
        }
    }
}

static inline u8 ui3dsReadSystemGlyphPixel(const u8 *sheet, int sheetWidth, int x, int y)
{
    int bitIndex = (x & 1) + ((x >> 1 & 1) * 4) + ((x >> 2 & 1) * 16) + ((x >> 3) * 64)
                 + ((y & 1) * 2) + ((y >> 1 & 1) * 8) + ((y >> 2 & 1) * 32)
                 + ((y >> 3) * sheetWidth * 8);
    return (sheet[bitIndex >> 1] >> ((bitIndex & 1) * 4)) & 0x0F;
}

// バイリニア補間で滑らかにサンプリングする（0.0〜15.0）
static float ui3dsSampleSystemGlyphBilinear(const u8 *sheet, int sheetWidth,
                                            int srcX, int srcY, int glyphW, int cellH,
                                            float fx, float fy)
{
    int x0 = (int)fx;
    int y0 = (int)fy;
    float wx = fx - x0;
    float wy = fy - y0;

    int x1 = x0 + 1 < glyphW ? x0 + 1 : x0;
    int y1 = y0 + 1 < cellH  ? y0 + 1 : y0;

    float p00 = (x0 >= 0 && x0 < glyphW && y0 >= 0 && y0 < cellH) ? ui3dsReadSystemGlyphPixel(sheet, sheetWidth, srcX + x0, srcY + y0) : 0.0f;
    float p10 = (x1 >= 0 && x1 < glyphW && y0 >= 0 && y0 < cellH) ? ui3dsReadSystemGlyphPixel(sheet, sheetWidth, srcX + x1, srcY + y0) : 0.0f;
    float p01 = (x0 >= 0 && x0 < glyphW && y1 >= 0 && y1 < cellH) ? ui3dsReadSystemGlyphPixel(sheet, sheetWidth, srcX + x0, srcY + y1) : 0.0f;
    float p11 = (x1 >= 0 && x1 < glyphW && y1 >= 0 && y1 < cellH) ? ui3dsReadSystemGlyphPixel(sheet, sheetWidth, srcX + x1, srcY + y1) : 0.0f;

    float top = p00 * (1.0f - wx) + p10 * wx;
    float bot = p01 * (1.0f - wx) + p11 * wx;
    return top * (1.0f - wy) + bot * wy;
}

// システムフォント本来の文字送り幅（プロポーショナル幅）を返す
static int ui3dsSystemGlyphWidth(u32 codePoint)
{
    if (!systemFont || codePoint < 0x80) return fontWidth[(u8)codePoint];
    int glyph = fontGlyphIndexFromCodePoint(systemFont, codePoint);
    if (glyph < 0) return fontWidth[(u8)'?'];

    TGLP_s *tglp = fontGetGlyphInfo(systemFont);
    charWidthInfo_s *info = fontGetCharWidthInfo(systemFont, glyph);
    if (!tglp || !info || !tglp->cellHeight) return 14;

    // mGBAと同じくらいの存在感になるよう14.5px基準でスケーリング
    const float scale = 14.5f / (float)tglp->cellHeight;
    int w = (int)(info->charWidth * scale + 0.5f);
    return w > 0 ? w : 14;
}

static int ui3dsDrawSystemGlyph(u16 *frameBuffer, int x, int y, int color565, u32 codePoint)
{
    if (!systemFont || codePoint < 0x80) return 0;
    int glyph = fontGlyphIndexFromCodePoint(systemFont, codePoint);
    TGLP_s *tglp = fontGetGlyphInfo(systemFont);
    if (glyph < 0 || !tglp || tglp->sheetFmt != GPU_A4 || !tglp->sheetData
        || !tglp->nRows || !tglp->nLines || !tglp->cellWidth || !tglp->cellHeight)
        return 0;

    charWidthInfo_s *info = fontGetCharWidthInfo(systemFont, glyph);
    if (!info) return 0;

    const float scale = 14.5f / (float)tglp->cellHeight;
    const int advanceWidth = (int)(info->charWidth * scale + 0.5f);
    if (info->glyphWidth == 0) return advanceWidth > 0 ? advanceWidth : 7;

    int perSheet = tglp->nRows * tglp->nLines;
    int sheetIndex = glyph / perSheet;
    if (sheetIndex >= tglp->nSheets) return 0;

    int glyphInSheet = glyph % perSheet;
    int srcX = (glyphInSheet % tglp->nRows) * (tglp->cellWidth + 1) + 1;
    int srcY = (glyphInSheet / tglp->nRows) * (tglp->cellHeight + 1) + 1;
    const u8 *sheet = (const u8 *)fontGetGlyphSheetTex(systemFont, sheetIndex);

    int dstWidth  = (int)(info->glyphWidth * scale + 0.5f);
    int dstHeight = 15;
    if (dstWidth < 1) dstWidth = 1;

    int offsetX = (int)(info->left * scale + 0.5f);
    int offsetY = (fontHeight - dstHeight) / 2;

    for (int dy = 0; dy < dstHeight; dy++) {
        float fy = ((dy + 0.5f) / dstHeight) * tglp->cellHeight - 0.5f;
        for (int dx = 0; dx < dstWidth; dx++) {
            int px = x + offsetX + dx;
            int py = y + offsetY + dy;
            if (px < viewportX1 || px >= viewportX2 || py < viewportY1 || py >= viewportY2)
                continue;

            float fx = ((dx + 0.5f) / dstWidth) * info->glyphWidth - 0.5f;
            float val = ui3dsSampleSystemGlyphBilinear(sheet, tglp->sheetWidth, srcX, srcY, info->glyphWidth, tglp->cellHeight, fx, fy);

            // 輪郭の半透明部分を強く引き上げてmGBAのような太い文字にする
            float norm = val / 15.0f;
            if (norm <= 0.05f) continue;
            float boldAlpha = norm * 1.85f;
            if (boldAlpha > 1.0f) boldAlpha = 1.0f;

            int alpha = (int)(boldAlpha * MAX_ALPHA + 0.5f);
            if (alpha <= 0) continue;

            u16 outColor = (alpha >= MAX_ALPHA)
                ? (u16)color565
                : ui3dsBlendPixel565(color565, ui3dsGetPixelInline(frameBuffer, px, py), alpha);
            ui3dsSetPixelInline(frameBuffer, px, py, outColor);
        }
    }
    return advanceWidth > 0 ? advanceWidth : dstWidth;
}

// Upscaled texture text blends two source pixels per axis. 0 matches bilinear blur;
// 1 narrows the filter for crisper enlarged glyphs.
static const float UI_TEXT_UPSCALE_SHARPNESS = 0.5f;

static const int UI_TEXT_MAX_FONT_HEIGHT = 32;
static const int UI_TEXT_MAX_SAMPLE_COUNT = FONT_CELL_WIDTH * UI_TEXT_MAX_FONT_HEIGHT / FONT_HEIGHT + 1;

// Do not downscale texture text; measurement and rasterization must clamp identically.
static inline int ui3dsClampDestHeight(int destHeight)
{
    return destHeight < fontHeight ? fontHeight : destHeight;
}

static inline int ui3dsCharAdvance(u8 c, float scale)
{
    return (int)(fontWidth[c] * scale + 0.5f);
}

static inline float ui3dsUpscaleFilterWeight(float d, float half)
{
    return d >= half ? 0.0f : 1.0f - d / half;
}

typedef struct {
    s16 firstSample;
    float firstWeight;
} UITextSamplePair;

// Build the two source samples used for each output pixel along one axis.
static void ui3dsBuildUpscaleSamples(UITextSamplePair *samples, int count, float scale)
{
    float filterRadius = 1.0f - UI_TEXT_UPSCALE_SHARPNESS
                       + UI_TEXT_UPSCALE_SHARPNESS / scale;

    for (int d = 0; d < count; d++)
    {
        float source = (d + 0.5f) / scale;
        int firstSample = (int)(source + 0.5f) - 1; // floor(source - 0.5); source >= 0
        float d0 = source - (firstSample + 0.5f);

        float w0 = ui3dsUpscaleFilterWeight(d0, filterRadius);
        float w1 = ui3dsUpscaleFilterWeight(1.0f - d0, filterRadius);

        // filterRadius > 0.5 keeps at least one sample weighted.
        samples[d].firstSample = (s16)firstSample;
        samples[d].firstWeight = w0 / (w0 + w1);
    }
}

// Outside-glyph samples read as transparent, preserving soft edges.
static inline float ui3dsSampleGlyph(u8 c, int x, int y, int charWidth)
{
    if (x < 0 || x >= charWidth || y < 0 || y >= fontHeight) return 0.0f;
    return GETFONTBITMAP(c, x, y);
}

static int ui3dsDrawUpscaledCharToTexture(u16 *buffer, u8 c, int xStart, int yStart, int xMax, int yMax,
                                          u16 color, int destHeight, float scale, const UITextSamplePair *samples)
{
    if (c == 0) return 0;

    int destWidth = ui3dsCharAdvance(c, scale);

    if (c == ' ') return destWidth;
    if ((xStart + destWidth > xMax) || (yStart + destHeight > yMax)) return 0;

    int charWidth = fontWidth[c];

    for (int dy = 0; dy < destHeight; dy++)
    {
        int y0 = samples[dy].firstSample;
        float wy0 = samples[dy].firstWeight;

        for (int dx = 0; dx < destWidth; dx++)
        {
            int x0 = samples[dx].firstSample;
            float wx0 = samples[dx].firstWeight;

            float row0 = wx0 * ui3dsSampleGlyph(c, x0, y0,     charWidth)
                       + (1.0f - wx0) * ui3dsSampleGlyph(c, x0 + 1, y0,     charWidth);
            float row1 = wx0 * ui3dsSampleGlyph(c, x0, y0 + 1, charWidth)
                       + (1.0f - wx0) * ui3dsSampleGlyph(c, x0 + 1, y0 + 1, charWidth);

            float acc = wy0 * row0 + (1.0f - wy0) * row1;

            // Collapse filtered coverage to RGBA4 alpha.
            int a4 = (int)(acc * (15.0f / MAX_ALPHA) + 0.5f);
            if (a4 > 15) a4 = 15;
            if (a4 > 0)
                buffer[(yStart + dy) * xMax + (xStart + dx)] = color | (u16)a4;
        }
    }

    return destWidth;
}

int ui3dsDrawRGBA4_CharToTexture(u16 *buffer, u8 c, int xStart, int yStart, int xMax, int yMax,  u16 color)
{
    if (c == 0) return 0;
    if (c == ' ') return fontWidth[' '];

    int charWidth = fontWidth[c];

    if ((xStart + charWidth > xMax) || (yStart + fontHeight > yMax)) return 0;

    u16* dstPtr = &buffer[(yStart * xMax) + xStart];
    int dstStride = xMax - charWidth;

    for (int y = 0; y < FONT_HEIGHT; y++)
    {
        for (int x = 0; x < charWidth; x++)
        {
            u8 alpha = GETFONTBITMAP(c, x, y);

            if (alpha > 0) {
                u16 a4 = alphas4Bit[alpha];
                *dstPtr = color | a4;
            }
            
            dstPtr++;
        }

        dstPtr += dstStride;
    }

    return charWidth;
}

// single-line (!) width helper.
int ui3dsGetStringWidth(const char *s, int startPos, int endPos, int destHeight)
{
    float scale = (float)ui3dsClampDestHeight(destHeight) / fontHeight;
    int totalWidth = 0;
    int length = strlen(s);
    for (int i = startPos; i <= endPos && i < length; )
    {
        u32 cp;
        int next = i;
        bool validUtf8 = ui3dsDecodeUtf8At(s, length, &next, &cp);
        if (!validUtf8) {
            u8 c = (u8)cp;
            if (c == '\n') c = ' ';
            totalWidth += ui3dsCharAdvance(c, scale);
        } else if (cp == '\n') {
            totalWidth += ui3dsCharAdvance(' ', scale);
        } else {
            totalWidth += (int)(ui3dsSystemGlyphWidth(cp) * scale + 0.5f);
        }
        if (next - 1 > endPos) break;
        i = next;
    }
    return totalWidth;
}

void ui3dsEllipsize(const char *src, char *dst, size_t dstSize, int maxWidth, int destHeight)
{
    if (dstSize == 0) return;

    const char *ellipsis = "\205";
    int ellipsisWidth = ui3dsGetStringWidth(ellipsis, 0, 0xffff, destHeight);
    float scale = (float)ui3dsClampDestHeight(destHeight) / fontHeight;

    // Find the ellipsis cutoff without truncating text that still fits.
    int width = 0;
    int keep = -1;
    for (int i = 0; src[i]; i++) {
        u8 c = src[i] == '\n' ? ' ' : (u8)src[i];
        width += ui3dsCharAdvance(c, scale);
        if (keep < 0 && width + ellipsisWidth > maxWidth)
            keep = i;
        if (width > maxWidth)
            break;
    }

    if (width <= maxWidth) {
        snprintf(dst, dstSize, "%s", src);
        return;
    }

    if (keep + 1 >= static_cast<int>(dstSize))
        keep = static_cast<int>(dstSize) - 2;
    if (keep < 0)
        keep = 0;

    memcpy(dst, src, keep);
    snprintf(dst + keep, dstSize - keep, "%s", ellipsis);
}

#define CONVERT_TO_565(x)    (((x & 0xf8) >> 3) | (((x >> 8) & 0xf8) << 3) | (((x >> 16) & 0xf8) << 8))

//---------------------------------------------------------------
// Draws a rectangle with the colour (in RGB888 format).
// 
// Note: x0,y0 are inclusive. x1,y1 are exclusive.
//---------------------------------------------------------------
void ui3dsDrawRect(int x0, int y0, int x1, int y1, int color, float alpha)
{
    if (color < 0)
        return;

    x0 += translateX;
    x1 += translateX;
    y0 += translateY;
    y1 += translateY;

    if (x0 < viewportX1) x0 = viewportX1;
    if (x1 > viewportX2) x1 = viewportX2;
    if (y0 < viewportY1) y0 = viewportY1;
    if (y1 > viewportY2) y1 = viewportY2;
    
    if (alpha <= 0) return;
    
    if (alpha > 1.0f) alpha = 1.0f;
        
    u16* fb = (u16 *) gfxGetFramebuffer(settings3DS.SecondScreen, GFX_LEFT, NULL, NULL);

    color = CONVERT_TO_565(color);
   
    if (alpha == 1.0f)
    {
        for (int x = x0; x < x1; x++)
        {
            int fbofs = (x) * SCREEN_HEIGHT + (239 - y0);
            for (int y = y0; y < y1; y++)
                fb[fbofs--] = color;
        }
    }
    else
    {
        int iAlpha = alpha * MAX_ALPHA;
        for (int x = x0; x < x1; x++)
        {
            int fbofs = (x) * SCREEN_HEIGHT + (239 - y0);
            for (int y = y0; y < y1; y++)
            {
                fb[fbofs] = ui3dsBlendPixel565(color, fb[fbofs], iAlpha);
                fbofs--;
            }
        }
    }
}

void ui3dsDrawCheckerboard(int x0, int y0, int x1, int y1, int color1, int color2)
{
    x0 += translateX;
    x1 += translateX;
    y0 += translateY;
    y1 += translateY;

    if (x0 < viewportX1) x0 = viewportX1;
    if (x1 > viewportX2) x1 = viewportX2;
    if (y0 < viewportY1) y0 = viewportY1;
    if (y1 > viewportY2) y1 = viewportY2;

    if (x0 >= x1 || y0 >= y1)
        return;

    u16* fb = (u16 *) gfxGetFramebuffer(settings3DS.SecondScreen, GFX_LEFT, NULL, NULL);
    const u16 color1_565 = CONVERT_TO_565(color1);
    const u16 color2_565 = CONVERT_TO_565(color2);

    for (int x = x0; x < x1; x++)
    {
        int fbofs = x * SCREEN_HEIGHT + (239 - y0);
        const int xParity = (x >> 1) & 1;
        int y = y0;

        while (y < y1)
        {
            const int yParity = (y >> 1) & 1;
            const u16 tileColor = (xParity ^ yParity) ? color2_565 : color1_565;
            int run = 2 - (y & 1);
            const int remaining = y1 - y;
            if (run > remaining) run = remaining;

            if (run == 2) {
                fb[fbofs--] = tileColor;
                fb[fbofs--] = tileColor;
            } else {
                fb[fbofs--] = tileColor;
            }

            y += run;
        }
    }
}

// RGBA4 only
// returns full length of the string
int ui3dsDrawStringToTexture(u16 *textureBuffer, const char *text, int x, int y, int xMax, int yMax, u32 color, int destHeight)
{
    destHeight = ui3dsClampDestHeight(destHeight);

    if (!text || (x > xMax) || (y + destHeight > yMax)) return x;
    if (destHeight > UI_TEXT_MAX_FONT_HEIGHT) return x;

    u16 color_rgba4 = color32toRGBA4(color, 0);
    int i = 0;

    bool upscaled = destHeight > fontHeight;
    float scale = (float)destHeight / fontHeight;
    UITextSamplePair samples[UI_TEXT_MAX_SAMPLE_COUNT];

    if (upscaled)
    {
        int sampleCount = (int)(FONT_CELL_WIDTH * scale + 0.5f);
        if (sampleCount < destHeight) sampleCount = destHeight;

        ui3dsBuildUpscaleSamples(samples, sampleCount, scale);
    }

    while (text[i] != 0)
    {
        int w = upscaled
            ? ui3dsDrawUpscaledCharToTexture(textureBuffer, (u8)text[i], x, y, xMax, yMax,
                                             color_rgba4, destHeight, scale, samples)
            : ui3dsDrawRGBA4_CharToTexture(textureBuffer, (u8)text[i], x, y, xMax, yMax, color_rgba4);

        if (w == 0) break;

        x += w;
        i++;
    }

    return x;
}

//---------------------------------------------------------------
// Draws a string at the given position without translation.
//---------------------------------------------------------------
int ui3dsDrawRGB565_StringToFramebuffer(gfxScreen_t targetScreen, int absoluteX, int absoluteY, int color, const char *buffer, int startPos = 0, int endPos = 0xffff)
{
    int x = absoluteX;
    int y = absoluteY;

    if (color < 0)
        return x;
    if (y >= viewportY1 - 16 && y <= viewportY2)
    {
        u16 color565 = CONVERT_TO_565(color);
        u16 *fb = (u16 *)gfxGetFramebuffer(targetScreen, GFX_LEFT, NULL, NULL);

        if (fgAlpha565Color != color565)
        {
            for (int a = 0; a <= MAX_ALPHA; a++)
                fgAlpha565[a] = ui3dsApplyAlphaToColour565(color565, a);
            fgAlpha565Color = color565;
        }

        int length = strlen(buffer);
        for (int i = startPos; i <= endPos && i < length; )
        {
            u32 cp;
            int next = i;
            bool validUtf8 = ui3dsDecodeUtf8At(buffer, length, &next, &cp);
            if (validUtf8 && cp >= 0x80) {
                int glyphWidth = ui3dsDrawSystemGlyph(fb, x, y, color565, cp);
                if (glyphWidth > 0) {
                    x += glyphWidth;
                    i = next;
                    continue;
                }
                cp = '?';
            }
            u8 c = (u8)cp;

            if (c == '\n') {
                c = ' ';
            }
            
            if (c != ' ')
                ui3dsDrawRGB565_CharToFramebuffer(fb, x, y, color565, c);
            x += fontWidth[c];
            i = next;
        }
    }

    return x;
}


//---------------------------------------------------------------
// Draws a string with the forecolor, with wrapping
//---------------------------------------------------------------
// Stores wrapped line ranges, capped at maxEntries.
int ui3dsWrapLines(const char *buffer, int maxWidth, int *lineStart, int *lineEnd, int maxEntries)
{
    int strLineCount = 0;
    if (buffer == NULL)
        return 0;

    int slen = strlen(buffer);
    int curStartPos = 0;
    int curEndPos = slen - 1;
    int lineWidth = 0;
    for (int i = 0; i < slen; )
    {
        bool forceBreak = buffer[i] == '\n';
        if (forceBreak)
            curEndPos = i - 1;          // empty range at line start = blank line
        else if (i != curStartPos)
        {
            if (buffer[i] == ' ' && i > 0 && buffer[i-1] != ' ')
                curEndPos = i - 1;
            // Break after a dash or slash only if it still fits, else it gets clipped.
            else if ((buffer[i] == '-' || buffer[i] == '/') &&
                     lineWidth + fontWidth[(unsigned char)buffer[i]] <= maxWidth)
                curEndPos = i;
        }
        lineWidth += fontWidth[(unsigned char)buffer[i]];
        if (forceBreak || lineWidth > maxWidth)
        {
            lineStart[strLineCount] = curStartPos;
            lineEnd[strLineCount] = curEndPos;
            strLineCount++;

            if (strLineCount >= maxEntries) break;

            if (forceBreak)
            {
                i = curEndPos + 2;
            }
            else
            {
                i = curEndPos + 1;
                while (buffer[i] == ' ')
                    i++;
                // The wrap already ended this line, so a \n here adds no blank line.
                if (buffer[i] == '\n')
                    i++;
            }
            curStartPos = i;
            curEndPos = slen - 1;
            lineWidth = 0;
        }
        else
            i++;
    }

    curEndPos = slen - 1;
    if (curStartPos <= curEndPos && strLineCount < maxEntries)
    {
        lineStart[strLineCount] = curStartPos;
        lineEnd[strLineCount] = curEndPos;
        strLineCount++;
    }

    return strLineCount;
}

int ui3dsCountWrappedLines(const char *buffer, int maxWidth)
{
    int lineStart[UI_MAX_WRAPPED_LINES], lineEnd[UI_MAX_WRAPPED_LINES];
    return ui3dsWrapLines(buffer, maxWidth, lineStart, lineEnd, UI_MAX_WRAPPED_LINES);
}

void ui3dsDrawStringWithWrapping(gfxScreen_t targetScreen, int x0, int y0, int x1, int y1, int color, int horizontalAlignment, const char *buffer, int maxLines)
{
    int strLineStart[UI_MAX_WRAPPED_LINES];
    int strLineEnd[UI_MAX_WRAPPED_LINES];

    x0 += translateX;
    x1 += translateX;
    y0 += translateY;
    y1 += translateY;

    ui3dsPushViewport(x0, y0, x1, y1);

    if (buffer != NULL)
    {
        int maxWidth = x1 - x0;
        int strLineCount = ui3dsWrapLines(buffer, maxWidth, strLineStart, strLineEnd, UI_MAX_WRAPPED_LINES);

        // Clamp to maxLines and mark the last visible line with a trailing "...".
        char truncBuf[128];
        const char *truncLine = NULL;
        if (maxLines > 0 && strLineCount > maxLines)
        {
            strLineCount = maxLines;
            int ls = strLineStart[strLineCount - 1];
            int le = strLineEnd[strLineCount - 1];
            char lineBuf[128];
            // Append "..." then ellipsize so the marker is forced onto the line
            // and trimmed to fit maxWidth (the line itself already fits).
            snprintf(lineBuf, sizeof(lineBuf), "%.*s...", le - ls + 1, buffer + ls);
            ui3dsEllipsize(lineBuf, truncBuf, sizeof(truncBuf), maxWidth);
            truncLine = truncBuf;
        }

        for (int i = 0; i < strLineCount; i++)
        {
            const char *lineText = buffer;
            int ls = strLineStart[i];
            int le = strLineEnd[i];
            if (truncLine && i == strLineCount - 1)
            {
                lineText = truncLine;
                ls = 0;
                le = (int)strlen(truncLine) - 1;
            }

            int x = x0;
            if (horizontalAlignment >= 0)
            {
                int sWidth = ui3dsGetStringWidth(lineText, ls, le);

                if (horizontalAlignment == 0)   // center aligned
                    x = (maxWidth - sWidth) / 2 + x0;
                else                            // right aligned
                    x = maxWidth - sWidth + x0;
            }

            ui3dsDrawRGB565_StringToFramebuffer(targetScreen, x, y0, color, lineText, ls, le);
            y0 += FONT_LINE_HEIGHT;
        }
    }

    ui3dsPopViewport();
}


//---------------------------------------------------------------
// Draws a string with the forecolor, with no wrapping
//---------------------------------------------------------------
int ui3dsDrawStringWithNoWrapping(gfxScreen_t targetScreen, int x0, int y0, int x1, int y1, int color, int horizontalAlignment, const char *buffer)
{
    x0 += translateX;
    x1 += translateX;
    y0 += translateY;
    y1 += translateY;

    int xEndPosition = 0;
    
    ui3dsPushViewport(x0, y0, x1, y1);
   
    if (buffer != NULL)
    {
        int maxWidth = x1 - x0;
        int x = x0;
        if (horizontalAlignment >= HALIGN_CENTER)
        {
            int sWidth = ui3dsGetStringWidth(buffer);

            if (horizontalAlignment == HALIGN_CENTER)   // center aligned
                x = (maxWidth - sWidth) / 2 + x0;
            else                                        // right aligned
                x = maxWidth - sWidth + x0;
        }
        xEndPosition = ui3dsDrawRGB565_StringToFramebuffer(targetScreen, x, y0, color, buffer);
    }

    ui3dsPopViewport();

    return xEndPosition;
}

bool ui3dsInitialize()
{
    log3dsWrite("allocate tex upload buffer (%.2fkb)", float(MAX_IO_BUFFER_SIZE) / 1024);
    g_texUploadBuffer = (u8*)linearAlloc(MAX_IO_BUFFER_SIZE);

    if (!g_texUploadBuffer) {
        return false;
    }

    memset(g_texUploadBuffer, 0, MAX_IO_BUFFER_SIZE);
    
    return true;
}

void ui3dsFinalize() {
    log3dsWrite("dealloc tex upload buffer");
    linearFree(g_texUploadBuffer);
}
