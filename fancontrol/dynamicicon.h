#pragma once

#include <windows.h>
#include <tchar.h>

class CDynamicIcon {
public:

	CDynamicIcon(const char line1[3], const char line2[3], const int iFarbeIconA, const int iFontIconA, float scale = 1.0f);
	~CDynamicIcon();

	HICON GetHIcon();
private:

	HDC      memDC1_;
	HDC      memDC2_;
	HBITMAP  oldBmp_1;
	HBITMAP  oldBmp_2;
	HBITMAP  iconBmp_;
	HBITMAP  iconMaskBmp_;
	HBRUSH   hOldBrush;
	HRGN     rgn;
	HICON    icon_;
	int      iconWidth_;
	int      iconHeight_;

private:
	HFONT CreateFont(const HDC hDC);
	//default und copy verbergen
	__inline CDynamicIcon() {};
	__inline CDynamicIcon(const CDynamicIcon&) {};
};
