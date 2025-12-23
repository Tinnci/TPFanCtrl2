#include "_prec.h"
#include "DynamicIcon.h"
#include <string.h>


CDynamicIcon::CDynamicIcon(const char *line1, const char *line2, const int iFarbeIconA, const int iFontIconA, float scale) {
    iconWidth_ = (int)(16 * scale);
    iconHeight_ = (int)(16 * scale);

    //3 chars per line
    char _line1[4], _line2[4];
    strncpy_s(_line1, sizeof(_line1), line1, 3);
    strncpy_s(_line2, sizeof(_line2), line2, 3);
    _line1[3] = 0;
    _line2[3] = 0;

    HDC hDC = GetDC(0);

    memDC1_ = CreateCompatibleDC(hDC);
    memDC2_ = CreateCompatibleDC(hDC);
    iconBmp_ = CreateCompatibleBitmap(hDC, iconWidth_, iconHeight_);

    iconMaskBmp_ = CreateCompatibleBitmap(hDC, iconWidth_, iconHeight_);
    oldBmp_1 = (HBITMAP) SelectObject(memDC1_, (HBITMAP) iconBmp_);
    oldBmp_2 = (HBITMAP) SelectObject(memDC2_, (HBITMAP) iconMaskBmp_);

    // prepare mask
    HBRUSH hBrush = CreateSolidBrush(RGB(255, 255, 255));
    hOldBrush = (HBRUSH) SelectObject(memDC2_, hBrush);
    PatBlt(memDC2_, 0, 0, iconWidth_, iconHeight_, PATCOPY);
    SelectObject(memDC2_, hOldBrush);
    DeleteObject(hBrush);

    // draw background on both bitmaps
    rgn = CreateRectRgn(0, 0, iconWidth_, iconHeight_);

    switch (iFarbeIconA) {
        case 10:
            hBrush = CreateSolidBrush(RGB(245, 245, 245)); //grau
            break;
        case 11:
            hBrush = CreateSolidBrush(RGB(191, 239, 255)); //blau
            break;
        case 12:
            hBrush = CreateSolidBrush(RGB(255, 255, 0)); //gelb
            break;
        case 13:
            hBrush = CreateSolidBrush(RGB(255, 165, 0)); //orange
            break;
        case 14:
            hBrush = CreateSolidBrush(RGB(255, 69, 0)); //rot
            break;
        case 21:
            hBrush = CreateSolidBrush(RGB(175, 255, 175)); //sehr hell grün
            break;
        case 22:
            hBrush = CreateSolidBrush(RGB(123, 255, 123)); //hell grün
            break;
        case 23:
            hBrush = CreateSolidBrush(RGB(0, 255, 0)); //grün
            break;
        case 24:
            hBrush = CreateSolidBrush(RGB(0, 218, 0)); //dunkel grün
            break;
        case 25:
            hBrush = CreateSolidBrush(RGB(0, 164, 0)); //sehr dunkel grün
            break;
        default:
            hBrush = CreateSolidBrush(RGB(255, 255, 255)); // weiss
    };


    FillRgn(memDC1_, rgn, hBrush);
    DeleteObject(hBrush);

    hBrush = CreateSolidBrush(RGB(0, 0, 0));
    FillRgn(memDC2_, rgn, hBrush);


    HFONT hfnt, hOldFont;

    hfnt = this->CreateFont(memDC1_);

    if (hOldFont = (HFONT) SelectObject(memDC1_, hfnt)) {
        SetBkMode(memDC1_, TRANSPARENT);
        SetTextColor(memDC1_, RGB(0, 0, 0)); // Black text for better contrast on light backgrounds

        RECT r;
        r.top = (int)(-2 * scale);
        r.left = 0;
        r.bottom = (int)(9 * scale);
        r.right = iconWidth_;
        DrawTextEx(memDC1_, (LPSTR) _line1, (int)strlen(_line1), &r, DT_CENTER, NULL);

        r.top = (int)(6 * scale);
        r.left = 0;
        r.bottom = (int)(16 * scale);
        r.right = iconWidth_;
        DrawTextEx(memDC1_, (LPSTR) _line2, (int)strlen(_line2), &r, DT_CENTER, NULL);

        SelectObject(memDC1_, hOldFont);
    }
    DeleteObject(hfnt);

    DeleteObject(hBrush);//


    DeleteObject(hBrush);
    DeleteObject(rgn);

    SelectObject(memDC1_, (HBITMAP) oldBmp_1);
    DeleteDC(memDC1_);
    SelectObject(memDC2_, (HBITMAP) oldBmp_2);
    DeleteDC(memDC2_);
    ReleaseDC(0, hDC);

    ICONINFO ii = {TRUE, 0, 0, iconMaskBmp_, iconBmp_};
    icon_ = CreateIconIndirect(&ii);

}

CDynamicIcon::~CDynamicIcon() {
    DestroyIcon(icon_);
    DeleteObject(iconBmp_);
    DeleteObject(iconMaskBmp_);
}

HICON CDynamicIcon::GetHIcon() {
    return icon_;
}

HFONT CDynamicIcon::CreateFont(const HDC hDC) {
    LOGFONT lf;
    SecureZeroMemory(&lf, sizeof(LOGFONT));
    
    // Scale font height. Original was -9 for 16px icon.
    float scale = (float)iconWidth_ / 16.0f;
    lf.lfHeight = (int)(-10 * scale); 
    lf.lfWeight = FW_BOLD;
    lf.lfOutPrecision = OUT_TT_ONLY_PRECIS;
    lf.lfClipPrecision = CLIP_DEFAULT_PRECIS;
    lf.lfQuality = CLEARTYPE_QUALITY;
    lf.lfPitchAndFamily = DEFAULT_PITCH | FF_SWISS;
    strcpy_s(lf.lfFaceName, sizeof(lf.lfFaceName), "Segoe UI");

    return CreateFontIndirect(&lf);
};




