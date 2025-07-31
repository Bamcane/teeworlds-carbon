/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef ENGINE_CLIENT_GRAPHICS_THREADED_NULL_H
#define ENGINE_CLIENT_GRAPHICS_THREADED_NULL_H

#include <engine/graphics.h>

class CGraphics_ThreadedNull : public IEngineGraphics
{
public:
	CGraphics_ThreadedNull()
	{
		m_ScreenWidth = 800;
		m_ScreenHeight = 600;
		m_DesktopScreenWidth = 800;
		m_DesktopScreenHeight = 600;
	};

	void ClipEnable(int x, int y, int w, int h) override {};
	void ClipDisable() override {};

	void BlendNone() override {};
	void BlendNormal() override {};
	void BlendAdditive() override {};

	void WrapNormal() override {};
	void WrapClamp() override {};
	void WrapMode(int WrapU, int WrapV) override {};

	int MemoryUsage() const override { return 0; };

	void MapScreen(float TopLeftX, float TopLeftY, float BottomRightX, float BottomRightY) override {};
	void GetScreen(float *pTopLeftX, float *pTopLeftY, float *pBottomRightX, float *pBottomRightY) override
	{
		*pTopLeftX = 0;
		*pTopLeftY = 0;
		*pBottomRightX = 600;
		*pBottomRightY = 600;
	};

	void LinesBegin() override {};
	void LinesEnd() override {};
	void LinesDraw(const CLineItem *pArray, int Num) override {};

	int UnloadTexture(IGraphics::CTextureHandle *Index) override { return 0; };
	IGraphics::CTextureHandle LoadTextureRaw(int Width, int Height, int Format, const void *pData, int StoreFormat, int Flags) override { return CreateTextureHandle(0); };
	int LoadTextureRawSub(IGraphics::CTextureHandle TextureID, int x, int y, int Width, int Height, int Format, const void *pData) override { return 0; };

	// simple uncompressed RGBA loaders
	IGraphics::CTextureHandle LoadTexture(const char *pFilename, int StorageType, int StoreFormat, int Flags) override { return CreateTextureHandle(0); };
	int LoadPNG(CImageInfo *pImg, const char *pFilename, int StorageType) override { return 0; };

	void TextureSet(CTextureHandle TextureID) override {};

	void Clear(float r, float g, float b) override {};

	void QuadsBegin() override {};
	void QuadsEnd() override {};
	void QuadsSetRotation(float Angle) override {};

	void SetColorVertex(const CColorVertex *pArray, int Num) override {};
	void SetColor(float r, float g, float b, float a) override {};
	void SetColor4(const vec4 &TopLeft, const vec4 &TopRight, const vec4 &BottomLeft, const vec4 &BottomRight) override {};

	void QuadsSetSubset(float TlU, float TlV, float BrU, float BrV, int TextureIndex = -1) override {};
	void QuadsSetSubsetFree (
		float x0, float y0, float x1, float y1,
		float x2, float y2, float x3, float y3, int TextureIndex = -1) override {};

	void QuadsDraw(CQuadItem *pArray, int Num) override {};
	void QuadsDrawTL(const CQuadItem *pArray, int Num) override {};
	void QuadsDrawFreeform(const CFreeformItem *pArray, int Num) override {};
	void QuadsText(float x, float y, float Size, const char *pText) override {};

	int GetNumScreens() const override { return 0; };
	void Minimize() override {};
	void Maximize() override {};
	bool Fullscreen(bool State) override { return false; };
	void SetWindowBordered(bool State) override {};
	bool SetWindowScreen(int Index) override { return false; };
	int GetWindowScreen() override { return 0; };

	int WindowActive() override { return 0; };
	int WindowOpen() override { return 0; };

	int Init() override { return 0; };
	void Shutdown() override {};

	void ReadBackbuffer(unsigned char **ppPixels, int x, int y, int w, int h) override {};
	void TakeScreenshot(const char *pFilename) override {};
	void Swap() override {};
	bool SetVSync(bool State) override { return false; };

	int GetVideoModes(CVideoMode *pModes, int MaxModes, int Screen) override { return 0; };

	// syncronization
	void InsertSignal(semaphore *pSemaphore) override {}
	bool IsIdle() const override { return false; }
	void WaitForIdle() override {};

	void *GetWindowHandle() { return nullptr; }
};

#endif // ENGINE_CLIENT_GRAPHICS_THREADED_NULL_H
