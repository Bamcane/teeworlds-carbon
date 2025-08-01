/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include <base/detect.h>
#include <SDL3/SDL.h>
#include <SDL3/SDL_opengl.h>

#include <base/tl/threading.h>

#include "backend_sdl.h"
#include "graphics_threaded.h"

#if defined(CONF_FAMILY_WINDOWS)
PFNGLTEXIMAGE3DPROC glTexImage3DInternal;

#if defined(_MSC_VER)
GLAPI void GLAPIENTRY glTexImage3D(GLenum target, GLint level, GLint internalFormat, GLsizei width, GLsizei height, GLsizei depth, GLint border, GLenum format, GLenum type, const GLvoid *pixels)
#else
void glTexImage3D(GLenum target, GLint level, GLint internalFormat, GLsizei width, GLsizei height, GLsizei depth, GLint border, GLenum format, GLenum type, const GLvoid *pixels)
#endif
{
	glTexImage3DInternal(target, level, internalFormat, width, height, depth, border, format, type, pixels);
}
#endif

// ------------ CGraphicsBackend_Threaded

void CGraphicsBackend_Threaded::ThreadFunc(void *pUser)
{
	CGraphicsBackend_Threaded *pThis = (CGraphicsBackend_Threaded *) pUser;

	while(!pThis->m_Shutdown)
	{
		pThis->m_Activity.wait();
		if(pThis->m_pBuffer)
		{
#ifdef CONF_PLATFORM_MACOS
			CAutoreleasePool AutoreleasePool;
#endif
			pThis->m_pProcessor->RunBuffer(pThis->m_pBuffer);
			sync_barrier();
			pThis->m_pBuffer = 0x0;
			pThis->m_BufferDone.signal();
		}
	}
}

CGraphicsBackend_Threaded::CGraphicsBackend_Threaded()
{
	m_pBuffer = 0x0;
	m_pProcessor = 0x0;
	m_pThread = 0x0;
}

void CGraphicsBackend_Threaded::StartProcessor(ICommandProcessor *pProcessor)
{
	m_Shutdown = false;
	m_pProcessor = pProcessor;
	m_pThread = thread_init(ThreadFunc, this);
	m_BufferDone.signal();
}

void CGraphicsBackend_Threaded::StopProcessor()
{
	m_Shutdown = true;
	m_Activity.signal();
	thread_wait(m_pThread);
	thread_destroy(m_pThread);
}

void CGraphicsBackend_Threaded::RunBuffer(CCommandBuffer *pBuffer)
{
	WaitForIdle();
	m_pBuffer = pBuffer;
	m_Activity.signal();
}

bool CGraphicsBackend_Threaded::IsIdle() const
{
	return m_pBuffer == 0x0;
}

void CGraphicsBackend_Threaded::WaitForIdle()
{
	while(m_pBuffer != 0x0)
		m_BufferDone.wait();
}

// ------------ CCommandProcessorFragment_General

void CCommandProcessorFragment_General::Cmd_Signal(const CCommandBuffer::CSignalCommand *pCommand)
{
	pCommand->m_pSemaphore->signal();
}

bool CCommandProcessorFragment_General::RunCommand(const CCommandBuffer::CCommand *pBaseCommand)
{
	switch(pBaseCommand->m_Cmd)
	{
	case CCommandBuffer::CMD_NOP: break;
	case CCommandBuffer::CMD_SIGNAL: Cmd_Signal(static_cast<const CCommandBuffer::CSignalCommand *>(pBaseCommand)); break;
	default: return false;
	}

	return true;
}

// ------------ CCommandProcessorFragment_OpenGL

int CCommandProcessorFragment_OpenGL::TexFormatToOpenGLFormat(int TexFormat)
{
	if(TexFormat == CCommandBuffer::TEXFORMAT_RGB)
		return GL_RGB;
	if(TexFormat == CCommandBuffer::TEXFORMAT_ALPHA)
		return GL_ALPHA;
	if(TexFormat == CCommandBuffer::TEXFORMAT_RGBA)
		return GL_RGBA;
	return GL_RGBA;
}

unsigned char CCommandProcessorFragment_OpenGL::Sample(int w, int h, const unsigned char *pData, int u, int v, int Offset, int ScaleW, int ScaleH, int Bpp)
{
	int Sum = 0;
	for(int x = 0; x < ScaleW; x++)
		for(int y = 0; y < ScaleH; y++)
			Sum += pData[((v + y) * w + (u + x)) * Bpp + Offset];
	return Sum / (ScaleW * ScaleH);
}

void *CCommandProcessorFragment_OpenGL::Rescale(int Width, int Height, int NewWidth, int NewHeight, int Format, const unsigned char *pData)
{
	int ScaleW = Width / NewWidth;
	int ScaleH = Height / NewHeight;

	int Bpp = 3;
	if(Format == CCommandBuffer::TEXFORMAT_RGBA)
		Bpp = 4;

	unsigned char *pTmpData = (unsigned char *) mem_alloc(NewWidth * NewHeight * Bpp);

	for(int y = 0; y < NewHeight; y++)
		for(int x = 0; x < NewWidth; x++)
			for(int b = 0; b < Bpp; b++)
				pTmpData[(NewWidth * y + x) * Bpp + b] = Sample(Width, Height, pData, x * ScaleW, y * ScaleH, b, ScaleW, ScaleH, Bpp);

	return pTmpData;
}

void CCommandProcessorFragment_OpenGL::SetState(const CCommandBuffer::CState &State)
{
	// clip
	if(State.m_ClipEnable)
	{
		glScissor(State.m_ClipX, State.m_ClipY, State.m_ClipW, State.m_ClipH);
		glEnable(GL_SCISSOR_TEST);
	}
	else
		glDisable(GL_SCISSOR_TEST);

	// texture
	int SrcBlendMode = GL_ONE;
	glDisable(GL_TEXTURE_2D);
	glDisable(GL_TEXTURE_3D);
	if(State.m_Texture >= 0 && State.m_Texture < CCommandBuffer::MAX_TEXTURES)
	{
		if(State.m_Dimension == 2 && (m_aTextures[State.m_Texture].m_State & CTexture::STATE_TEX2D))
		{
			glEnable(GL_TEXTURE_2D);
			glBindTexture(GL_TEXTURE_2D, m_aTextures[State.m_Texture].m_Tex2D);
		}
		else if(State.m_Dimension == 3 && (m_aTextures[State.m_Texture].m_State & CTexture::STATE_TEX3D))
		{
			glEnable(GL_TEXTURE_3D);
			glBindTexture(GL_TEXTURE_3D, m_aTextures[State.m_Texture].m_Tex3D[State.m_TextureArrayIndex]);
		}
		else
			dbg_msg("render", "invalid texture %d %d %d", State.m_Texture, State.m_Dimension, m_aTextures[State.m_Texture].m_State);

		if(m_aTextures[State.m_Texture].m_Format == CCommandBuffer::TEXFORMAT_RGBA)
			SrcBlendMode = GL_ONE;
		else
			SrcBlendMode = GL_SRC_ALPHA;
	}

	// blend
	switch(State.m_BlendMode)
	{
	case CCommandBuffer::BLEND_NONE:
		glDisable(GL_BLEND);
		break;
	case CCommandBuffer::BLEND_ALPHA:
		glEnable(GL_BLEND);
		glBlendFunc(SrcBlendMode, GL_ONE_MINUS_SRC_ALPHA);
		break;
	case CCommandBuffer::BLEND_ADDITIVE:
		glEnable(GL_BLEND);
		glBlendFunc(SrcBlendMode, GL_ONE);
		break;
	default:
		dbg_msg("render", "unknown blendmode %d", State.m_BlendMode);
	};

	// wrap mode
	switch(State.m_WrapModeU)
	{
	case IGraphics::WRAP_REPEAT:
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
		break;
	case IGraphics::WRAP_CLAMP:
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		break;
	default:
		dbg_msg("render", "unknown wrapmode u %d", State.m_WrapModeU);
	};

	switch(State.m_WrapModeV)
	{
	case IGraphics::WRAP_REPEAT:
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
		break;
	case IGraphics::WRAP_CLAMP:
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
		break;
	default:
		dbg_msg("render", "unknown wrapmode v %d", State.m_WrapModeV);
	};

	if(State.m_Texture >= 0 && State.m_Texture < CCommandBuffer::MAX_TEXTURES && State.m_Dimension == 3)
	{
		glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_R, GL_REPEAT);
	}

	// screen mapping
	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	glOrtho(State.m_ScreenTL.x, State.m_ScreenBR.x, State.m_ScreenBR.y, State.m_ScreenTL.y, -1.0f, 1.0f);
}

void CCommandProcessorFragment_OpenGL::Cmd_Init(const CInitCommand *pCommand)
{
	// set some default settings
	glEnable(GL_BLEND);
	glDisable(GL_CULL_FACE);
	glDisable(GL_DEPTH_TEST);
	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();

	glAlphaFunc(GL_GREATER, 0);
	glEnable(GL_ALPHA_TEST);
	glDepthMask(0);

	m_pTextureMemoryUsage = pCommand->m_pTextureMemoryUsage;
	*m_pTextureMemoryUsage = 0;
	glGetIntegerv(GL_MAX_TEXTURE_SIZE, &m_MaxTexSize);
	glGetIntegerv(GL_MAX_3D_TEXTURE_SIZE, &m_Max3DTexSize);
	dbg_msg("render", "opengl max texture sizes: %d, %d(3D)", m_MaxTexSize, m_Max3DTexSize);
	if(m_Max3DTexSize < IGraphics::NUMTILES_DIMENSION * IGraphics::NUMTILES_DIMENSION)
		dbg_msg("render", "*** warning *** max 3D texture size is too low - using the fallback system");
	m_TextureArraySize = IGraphics::NUMTILES_DIMENSION * IGraphics::NUMTILES_DIMENSION / minimum(m_Max3DTexSize, IGraphics::NUMTILES_DIMENSION * IGraphics::NUMTILES_DIMENSION);
	*pCommand->m_pTextureArraySize = m_TextureArraySize;

	glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
}

void CCommandProcessorFragment_OpenGL::Cmd_Texture_Update(const CCommandBuffer::CTextureUpdateCommand *pCommand)
{
	if(m_aTextures[pCommand->m_Slot].m_State & CTexture::STATE_TEX2D)
	{
		glBindTexture(GL_TEXTURE_2D, m_aTextures[pCommand->m_Slot].m_Tex2D);
		glTexSubImage2D(GL_TEXTURE_2D, 0, pCommand->m_X, pCommand->m_Y, pCommand->m_Width, pCommand->m_Height,
			TexFormatToOpenGLFormat(pCommand->m_Format), GL_UNSIGNED_BYTE, pCommand->m_pData);
	}
	mem_free(pCommand->m_pData);
}

void CCommandProcessorFragment_OpenGL::Cmd_Texture_Destroy(const CCommandBuffer::CTextureDestroyCommand *pCommand)
{
	if(m_aTextures[pCommand->m_Slot].m_State & CTexture::STATE_TEX2D)
		glDeleteTextures(1, &m_aTextures[pCommand->m_Slot].m_Tex2D);
	if(m_aTextures[pCommand->m_Slot].m_State & CTexture::STATE_TEX3D)
		glDeleteTextures(m_TextureArraySize, m_aTextures[pCommand->m_Slot].m_Tex3D);
	*m_pTextureMemoryUsage -= m_aTextures[pCommand->m_Slot].m_MemSize;
	m_aTextures[pCommand->m_Slot].m_State = CTexture::STATE_EMPTY;
	m_aTextures[pCommand->m_Slot].m_MemSize = 0;
}

void CCommandProcessorFragment_OpenGL::Cmd_Texture_Create(const CCommandBuffer::CTextureCreateCommand *pCommand)
{
	int Width = pCommand->m_Width;
	int Height = pCommand->m_Height;
	void *pTexData = pCommand->m_pData;

	// resample if needed
	if(pCommand->m_Format == CCommandBuffer::TEXFORMAT_RGBA || pCommand->m_Format == CCommandBuffer::TEXFORMAT_RGB)
	{
		int MaxTexSize = m_MaxTexSize;
		if((pCommand->m_Flags & CCommandBuffer::TEXFLAG_TEXTURE3D) && m_Max3DTexSize >= CTexture::MIN_GL_MAX_3D_TEXTURE_SIZE)
		{
			if(pCommand->m_Flags & CCommandBuffer::TEXFLAG_TEXTURE2D)
				MaxTexSize = minimum(MaxTexSize, m_Max3DTexSize * IGraphics::NUMTILES_DIMENSION);
			else
				MaxTexSize = m_Max3DTexSize * IGraphics::NUMTILES_DIMENSION;
		}
		if(Width > MaxTexSize || Height > MaxTexSize)
		{
			do
			{
				Width >>= 1;
				Height >>= 1;
			} while(Width > MaxTexSize || Height > MaxTexSize);

			void *pTmpData = Rescale(pCommand->m_Width, pCommand->m_Height, Width, Height, pCommand->m_Format, static_cast<const unsigned char *>(pCommand->m_pData));
			mem_free(pTexData);
			pTexData = pTmpData;
		}
		else if(Width > IGraphics::NUMTILES_DIMENSION && Height > IGraphics::NUMTILES_DIMENSION && (pCommand->m_Flags & CCommandBuffer::TEXFLAG_QUALITY) == 0)
		{
			Width >>= 1;
			Height >>= 1;

			void *pTmpData = Rescale(pCommand->m_Width, pCommand->m_Height, Width, Height, pCommand->m_Format, static_cast<const unsigned char *>(pCommand->m_pData));
			mem_free(pTexData);
			pTexData = pTmpData;
		}
	}

	// use premultiplied alpha for rgba textures
	if(pCommand->m_Format == CCommandBuffer::TEXFORMAT_RGBA)
	{
		unsigned char *pTexels = (unsigned char *) pTexData;
		for(int i = 0; i < Width * Height; ++i)
		{
			const float a = (pTexels[i * 4 + 3] / 255.0f);
			pTexels[i * 4 + 0] = (unsigned char) (pTexels[i * 4 + 0] * a);
			pTexels[i * 4 + 1] = (unsigned char) (pTexels[i * 4 + 1] * a);
			pTexels[i * 4 + 2] = (unsigned char) (pTexels[i * 4 + 2] * a);
		}
	}
	m_aTextures[pCommand->m_Slot].m_Format = pCommand->m_Format;

	//
	int Oglformat = TexFormatToOpenGLFormat(pCommand->m_Format);
	int StoreOglformat = TexFormatToOpenGLFormat(pCommand->m_StoreFormat);

	if(pCommand->m_Flags & CCommandBuffer::TEXFLAG_COMPRESSED)
	{
		switch(StoreOglformat)
		{
		case GL_RGB: StoreOglformat = GL_COMPRESSED_RGB_ARB; break;
		case GL_ALPHA: StoreOglformat = GL_COMPRESSED_ALPHA_ARB; break;
		case GL_RGBA: StoreOglformat = GL_COMPRESSED_RGBA_ARB; break;
		default: StoreOglformat = GL_COMPRESSED_RGBA_ARB;
		}
	}

	// 2D texture
	if(pCommand->m_Flags & CCommandBuffer::TEXFLAG_TEXTURE2D)
	{
		bool Mipmaps = !(pCommand->m_Flags & CCommandBuffer::TEXFLAG_NOMIPMAPS);
		glGenTextures(1, &m_aTextures[pCommand->m_Slot].m_Tex2D);
		m_aTextures[pCommand->m_Slot].m_State |= CTexture::STATE_TEX2D;
		glBindTexture(GL_TEXTURE_2D, m_aTextures[pCommand->m_Slot].m_Tex2D);
		if(!Mipmaps)
		{
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
			glTexImage2D(GL_TEXTURE_2D, 0, StoreOglformat, Width, Height, 0, Oglformat, GL_UNSIGNED_BYTE, pTexData);
		}
		else
		{
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
			if(pCommand->m_Flags & CCommandBuffer::TEXTFLAG_LINEARMIPMAPS)
				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
			else
				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_NEAREST);
			glTexParameteri(GL_TEXTURE_2D, GL_GENERATE_MIPMAP, GL_TRUE);
			glTexImage2D(GL_TEXTURE_2D, 0, StoreOglformat, Width, Height, 0, Oglformat, GL_UNSIGNED_BYTE, pTexData);
		}

		// calculate memory usage
		m_aTextures[pCommand->m_Slot].m_MemSize = Width * Height * pCommand->m_PixelSize;
		if(Mipmaps)
		{
			int TexWidth = Width;
			int TexHeight = Height;
			while(TexWidth > 2 && TexHeight > 2)
			{
				TexWidth >>= 1;
				TexHeight >>= 1;
				m_aTextures[pCommand->m_Slot].m_MemSize += TexWidth * TexHeight * pCommand->m_PixelSize;
			}
		}
	}

	// 3D texture
	if((pCommand->m_Flags & CCommandBuffer::TEXFLAG_TEXTURE3D) && m_Max3DTexSize >= CTexture::MIN_GL_MAX_3D_TEXTURE_SIZE)
	{
		Width /= IGraphics::NUMTILES_DIMENSION;
		Height /= IGraphics::NUMTILES_DIMENSION;
		int Depth = minimum(m_Max3DTexSize, IGraphics::NUMTILES_DIMENSION * IGraphics::NUMTILES_DIMENSION);

		// copy and reorder texture data
		int MemSize = Width * Height * IGraphics::NUMTILES_DIMENSION * IGraphics::NUMTILES_DIMENSION * pCommand->m_PixelSize;
		char *pTmpData = (char *) mem_alloc(MemSize);

		const int TileSize = (Height * Width) * pCommand->m_PixelSize;
		const int TileRowSize = Width * pCommand->m_PixelSize;
		const int ImagePitch = Width * IGraphics::NUMTILES_DIMENSION * pCommand->m_PixelSize;
		mem_zero(pTmpData, MemSize);
		for(int i = 0; i < IGraphics::NUMTILES_DIMENSION * IGraphics::NUMTILES_DIMENSION; i++)
		{
			const int px = (i % IGraphics::NUMTILES_DIMENSION) * Width;
			const int py = (i / IGraphics::NUMTILES_DIMENSION) * Height;
			const char *pTileData = (const char *) pTexData + (py * Width * IGraphics::NUMTILES_DIMENSION + px) * pCommand->m_PixelSize;
			for(int y = 0; y < Height; y++)
				mem_copy(pTmpData + i * TileSize + y * TileRowSize, pTileData + y * ImagePitch, TileRowSize);
		}

		mem_free(pTexData);

		//
		glGenTextures(m_TextureArraySize, m_aTextures[pCommand->m_Slot].m_Tex3D);
		m_aTextures[pCommand->m_Slot].m_State |= CTexture::STATE_TEX3D;
		for(int i = 0; i < m_TextureArraySize; ++i)
		{
			glBindTexture(GL_TEXTURE_3D, m_aTextures[pCommand->m_Slot].m_Tex3D[i]);
			glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
			glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
			pTexData = pTmpData + i * (Width * Height * Depth * pCommand->m_PixelSize);
			glTexImage3D(GL_TEXTURE_3D, 0, StoreOglformat, Width, Height, Depth, 0, Oglformat, GL_UNSIGNED_BYTE, pTexData);

			m_aTextures[pCommand->m_Slot].m_MemSize += Width * Height * pCommand->m_PixelSize;
		}
		pTexData = pTmpData;
	}

	*m_pTextureMemoryUsage += m_aTextures[pCommand->m_Slot].m_MemSize;

	mem_free(pTexData);
}

void CCommandProcessorFragment_OpenGL::Cmd_Clear(const CCommandBuffer::CClearCommand *pCommand)
{
	glClearColor(pCommand->m_Color.r, pCommand->m_Color.g, pCommand->m_Color.b, 0.0f);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
}

void CCommandProcessorFragment_OpenGL::Cmd_Render(const CCommandBuffer::CRenderCommand *pCommand)
{
	SetState(pCommand->m_State);

	glVertexPointer(2, GL_FLOAT, sizeof(CCommandBuffer::CVertex), (char *) pCommand->m_pVertices);
	glTexCoordPointer(3, GL_FLOAT, sizeof(CCommandBuffer::CVertex), (char *) pCommand->m_pVertices + sizeof(float) * 2);
	glColorPointer(4, GL_FLOAT, sizeof(CCommandBuffer::CVertex), (char *) pCommand->m_pVertices + sizeof(float) * 5);
	glEnableClientState(GL_VERTEX_ARRAY);
	glEnableClientState(GL_TEXTURE_COORD_ARRAY);
	glEnableClientState(GL_COLOR_ARRAY);

	switch(pCommand->m_PrimType)
	{
	case CCommandBuffer::PRIMTYPE_QUADS:
		glDrawArrays(GL_QUADS, 0, pCommand->m_PrimCount * 4);
		break;
	case CCommandBuffer::PRIMTYPE_LINES:
		glDrawArrays(GL_LINES, 0, pCommand->m_PrimCount * 2);
		break;
	default:
		dbg_msg("render", "unknown primtype %d", pCommand->m_PrimType);
	};
}

void CCommandProcessorFragment_OpenGL::Cmd_Screenshot(const CCommandBuffer::CScreenshotCommand *pCommand)
{
	// fetch image data
	GLint aViewport[4] = {0, 0, 0, 0};
	glGetIntegerv(GL_VIEWPORT, aViewport);

	int w = pCommand->m_W == -1 ? aViewport[2] : pCommand->m_W;
	int h = pCommand->m_H == -1 ? aViewport[3] : pCommand->m_H;
	int x = pCommand->m_X;
	int y = aViewport[3] - pCommand->m_Y - 1 - (h - 1);

	// we allocate one more row to use when we are flipping the texture
	unsigned char *pPixelData = (unsigned char *) mem_alloc(w * (h + 1) * 3);
	unsigned char *pTempRow = pPixelData + w * h * 3;

	// fetch the pixels
	GLint Alignment;
	glGetIntegerv(GL_PACK_ALIGNMENT, &Alignment);
	glPixelStorei(GL_PACK_ALIGNMENT, 1);
	glReadPixels(x, y, w, h, GL_RGB, GL_UNSIGNED_BYTE, pPixelData);
	glPixelStorei(GL_PACK_ALIGNMENT, Alignment);

	// flip the pixel because opengl works from bottom left corner
	for(int ty = 0; ty < h / 2; ty++)
	{
		mem_copy(pTempRow, pPixelData + ty * w * 3, w * 3);
		mem_copy(pPixelData + ty * w * 3, pPixelData + (h - ty - 1) * w * 3, w * 3);
		mem_copy(pPixelData + (h - ty - 1) * w * 3, pTempRow, w * 3);
	}

	// fill in the information
	pCommand->m_pImage->m_Width = w;
	pCommand->m_pImage->m_Height = h;
	pCommand->m_pImage->m_Format = CImageInfo::FORMAT_RGB;
	pCommand->m_pImage->m_pData = pPixelData;
}

CCommandProcessorFragment_OpenGL::CCommandProcessorFragment_OpenGL()
{
	mem_zero(m_aTextures, sizeof(m_aTextures));
	m_pTextureMemoryUsage = 0;
}

bool CCommandProcessorFragment_OpenGL::RunCommand(const CCommandBuffer::CCommand *pBaseCommand)
{
	switch(pBaseCommand->m_Cmd)
	{
	case CMD_INIT: Cmd_Init(static_cast<const CInitCommand *>(pBaseCommand)); break;
	case CCommandBuffer::CMD_TEXTURE_CREATE: Cmd_Texture_Create(static_cast<const CCommandBuffer::CTextureCreateCommand *>(pBaseCommand)); break;
	case CCommandBuffer::CMD_TEXTURE_DESTROY: Cmd_Texture_Destroy(static_cast<const CCommandBuffer::CTextureDestroyCommand *>(pBaseCommand)); break;
	case CCommandBuffer::CMD_TEXTURE_UPDATE: Cmd_Texture_Update(static_cast<const CCommandBuffer::CTextureUpdateCommand *>(pBaseCommand)); break;
	case CCommandBuffer::CMD_CLEAR: Cmd_Clear(static_cast<const CCommandBuffer::CClearCommand *>(pBaseCommand)); break;
	case CCommandBuffer::CMD_RENDER: Cmd_Render(static_cast<const CCommandBuffer::CRenderCommand *>(pBaseCommand)); break;
	case CCommandBuffer::CMD_SCREENSHOT: Cmd_Screenshot(static_cast<const CCommandBuffer::CScreenshotCommand *>(pBaseCommand)); break;
	default: return false;
	}

	return true;
}

// ------------ CCommandProcessorFragment_SDL

void CCommandProcessorFragment_SDL::Cmd_Init(const CInitCommand *pCommand)
{
	m_GLContext = pCommand->m_GLContext;
	m_pWindow = pCommand->m_pWindow;
	SDL_GL_MakeCurrent(m_pWindow, m_GLContext);
}

void CCommandProcessorFragment_SDL::Cmd_Shutdown(const CShutdownCommand *pCommand)
{
	SDL_GL_MakeCurrent(NULL, NULL);
}

void CCommandProcessorFragment_SDL::Cmd_Swap(const CCommandBuffer::CSwapCommand *pCommand)
{
	SDL_GL_SwapWindow(m_pWindow);

	if(pCommand->m_Finish)
		glFinish();
}

void CCommandProcessorFragment_SDL::Cmd_VSync(const CCommandBuffer::CVSyncCommand *pCommand)
{
	*pCommand->m_pRetOk = SDL_GL_SetSwapInterval(pCommand->m_VSync);
}

CCommandProcessorFragment_SDL::CCommandProcessorFragment_SDL()
{
}

bool CCommandProcessorFragment_SDL::RunCommand(const CCommandBuffer::CCommand *pBaseCommand)
{
	switch(pBaseCommand->m_Cmd)
	{
	case CCommandBuffer::CMD_SWAP: Cmd_Swap(static_cast<const CCommandBuffer::CSwapCommand *>(pBaseCommand)); break;
	case CCommandBuffer::CMD_VSYNC: Cmd_VSync(static_cast<const CCommandBuffer::CVSyncCommand *>(pBaseCommand)); break;
	case CMD_INIT: Cmd_Init(static_cast<const CInitCommand *>(pBaseCommand)); break;
	case CMD_SHUTDOWN: Cmd_Shutdown(static_cast<const CShutdownCommand *>(pBaseCommand)); break;
	default: return false;
	}

	return true;
}

// ------------ CCommandProcessor_SDL_OpenGL

void CCommandProcessor_SDL_OpenGL::RunBuffer(CCommandBuffer *pBuffer)
{
	for(CCommandBuffer::CCommand *pCommand = pBuffer->Head(); pCommand; pCommand = pCommand->m_pNext)
	{
		if(m_OpenGL.RunCommand(pCommand))
			continue;

		if(m_SDL.RunCommand(pCommand))
			continue;

		if(m_General.RunCommand(pCommand))
			continue;

		dbg_msg("graphics", "unknown command %d", pCommand->m_Cmd);
	}
}

// ------------ CGraphicsBackend_SDL_OpenGL

// Ninslash
SDL_DisplayID CGraphicsBackend_SDL_OpenGL::DisplayIDFromIndex(int &Index) const
{
	int DisplayNum = 0;
	SDL_DisplayID *pDisplayIds = SDL_GetDisplays(&DisplayNum);
	if(!pDisplayIds || DisplayNum <= 0)
		return 0; // 0 is invalid
	Index = clamp(Index, 0, DisplayNum - 1);
	return pDisplayIds[Index];
}

int CGraphicsBackend_SDL_OpenGL::Init(const char *pName, int *pScreen, int *pWindowWidth, int *pWindowHeight, int *pScreenWidth, int *pScreenHeight, int FsaaSamples, int Flags, int *pDesktopWidth, int *pDesktopHeight)
{
	if(!SDL_WasInit(SDL_INIT_VIDEO))
	{
		if(!SDL_InitSubSystem(SDL_INIT_VIDEO))
		{
			dbg_msg("gfx", "unable to init SDL video: %s", SDL_GetError());
			return -1;
		}
	}

	// set screen
	SDL_Rect ScreenPos;
	SDL_GetDisplays(&m_NumScreens);
	if(m_NumScreens > 0)
	{
		*pScreen = DisplayIDFromIndex(*pScreen);
		if(!SDL_GetDisplayBounds(*pScreen, &ScreenPos))
		{
			dbg_msg("gfx", "unable to retrieve screen information: %s", SDL_GetError());
			return -1;
		}
	}
	else
	{
		dbg_msg("gfx", "unable to retrieve number of screens: %s", SDL_GetError());
		return -1;
	}

	// store desktop resolution for settings reset button
	if(!GetDesktopResolution(*pScreen, pDesktopWidth, pDesktopHeight))
	{
		dbg_msg("gfx", "unable to get desktop resolution: %s", SDL_GetError());
		return -1;
	}

	// use desktop resolution as default resolution
	if(*pWindowWidth == 0 || *pWindowHeight == 0)
	{
		*pWindowWidth = *pDesktopWidth;
		*pWindowHeight = *pDesktopHeight;
	}

	// set flags
	int SdlFlags = SDL_WINDOW_OPENGL;
	if(Flags & IGraphicsBackend::INITFLAG_HIGHDPI)
		SdlFlags |= SDL_WINDOW_HIGH_PIXEL_DENSITY;
	if(Flags & IGraphicsBackend::INITFLAG_RESIZABLE)
		SdlFlags |= SDL_WINDOW_RESIZABLE;
	if(Flags & IGraphicsBackend::INITFLAG_BORDERLESS)
		SdlFlags |= SDL_WINDOW_BORDERLESS;
	if(Flags & IGraphicsBackend::INITFLAG_FULLSCREEN)
		SdlFlags |= SDL_WINDOW_FULLSCREEN;

	if(Flags & IGraphicsBackend::INITFLAG_X11XRANDR)
		SDL_SetHint(SDL_HINT_VIDEO_X11_XRANDR, "1");

	// set gl attributes
	SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
	if(FsaaSamples)
	{
		SDL_GL_SetAttribute(SDL_GL_MULTISAMPLEBUFFERS, 1);
		SDL_GL_SetAttribute(SDL_GL_MULTISAMPLESAMPLES, FsaaSamples);
	}
	else
	{
		SDL_GL_SetAttribute(SDL_GL_MULTISAMPLEBUFFERS, 0);
		SDL_GL_SetAttribute(SDL_GL_MULTISAMPLESAMPLES, 0);
	}

	// calculate centered position in windowed mode
	int OffsetX = 0;
	int OffsetY = 0;
	if(!(Flags & IGraphicsBackend::INITFLAG_FULLSCREEN) && *pDesktopWidth > *pWindowWidth && *pDesktopHeight > *pWindowHeight)
	{
		OffsetX = (*pDesktopWidth - *pWindowWidth) / 2;
		OffsetY = (*pDesktopHeight - *pWindowHeight) / 2;
	}

	// create window
	m_pWindow = SDL_CreateWindow(pName, *pWindowWidth, *pWindowHeight, SdlFlags);
	if(m_pWindow == NULL)
	{
		dbg_msg("gfx", "unable to create window: %s", SDL_GetError());
		return -1;
	}
	SDL_SetWindowPosition(m_pWindow, ScreenPos.x + OffsetX, ScreenPos.y + OffsetY);
	SDL_GetWindowSize(m_pWindow, pWindowWidth, pWindowHeight);

	// create gl context
	m_GLContext = SDL_GL_CreateContext(m_pWindow);
	if(m_GLContext == NULL)
	{
		dbg_msg("gfx", "unable to create OpenGL context: %s", SDL_GetError());
		return -1;
	}

	SDL_GetWindowSizeInPixels(m_pWindow, pScreenWidth, pScreenHeight); // drawable size may differ in high dpi mode

#if defined(CONF_FAMILY_WINDOWS)
	glTexImage3DInternal = (PFNGLTEXIMAGE3DPROC) wglGetProcAddress("glTexImage3D");
	if(glTexImage3DInternal == 0)
	{
		dbg_msg("gfx", "glTexImage3D not supported");
		return -1;
	}
#endif

	SDL_GL_SetSwapInterval(Flags & IGraphicsBackend::INITFLAG_VSYNC ? 1 : 0);

	SDL_GL_MakeCurrent(NULL, NULL);

	// print sdl version
	int Version = SDL_GetVersion();
	dbg_msg("sdl", "SDL version %d.%d.%d", SDL_VERSIONNUM_MAJOR(Version), SDL_VERSIONNUM_MINOR(Version), SDL_VERSIONNUM_MICRO(Version));

	// start the command processor
	m_pProcessor = new CCommandProcessor_SDL_OpenGL;
	StartProcessor(m_pProcessor);

	// issue init commands for OpenGL and SDL
	CCommandBuffer CmdBuffer(1024, 512);
	CCommandProcessorFragment_SDL::CInitCommand CmdSDL;
	CmdSDL.m_pWindow = m_pWindow;
	CmdSDL.m_GLContext = m_GLContext;
	CmdBuffer.AddCommand(CmdSDL);
	CCommandProcessorFragment_OpenGL::CInitCommand CmdOpenGL;
	CmdOpenGL.m_pTextureMemoryUsage = &m_TextureMemoryUsage;
	CmdOpenGL.m_pTextureArraySize = &m_TextureArraySize;
	CmdBuffer.AddCommand(CmdOpenGL);
	RunBuffer(&CmdBuffer);
	WaitForIdle();

	return 0;
}

int CGraphicsBackend_SDL_OpenGL::Shutdown()
{
	// issue a shutdown command
	CCommandBuffer CmdBuffer(1024, 512);
	CCommandProcessorFragment_SDL::CShutdownCommand Cmd;
	CmdBuffer.AddCommand(Cmd);
	RunBuffer(&CmdBuffer);
	WaitForIdle();

	// stop and delete the processor
	StopProcessor();
	delete m_pProcessor;
	m_pProcessor = 0;

	SDL_GL_DestroyContext(m_GLContext);
	SDL_DestroyWindow(m_pWindow);
	SDL_QuitSubSystem(SDL_INIT_VIDEO);
	return 0;
}

int CGraphicsBackend_SDL_OpenGL::MemoryUsage() const
{
	return m_TextureMemoryUsage;
}

void CGraphicsBackend_SDL_OpenGL::Minimize()
{
	SDL_MinimizeWindow(m_pWindow);
}

void CGraphicsBackend_SDL_OpenGL::Maximize()
{
	SDL_MaximizeWindow(m_pWindow);
}

bool CGraphicsBackend_SDL_OpenGL::Fullscreen(bool State)
{
	return SDL_SetWindowFullscreen(m_pWindow, State);
}

void CGraphicsBackend_SDL_OpenGL::SetWindowBordered(bool State)
{
	SDL_SetWindowBordered(m_pWindow, State);
}

bool CGraphicsBackend_SDL_OpenGL::SetWindowScreen(int Index)
{
	if(Index >= 0 && Index < m_NumScreens)
	{
		SDL_Rect ScreenPos;
		if(SDL_GetDisplayBounds(Index, &ScreenPos) == 0)
		{
			SDL_SetWindowPosition(m_pWindow, ScreenPos.x, ScreenPos.y);
			return true;
		}
	}

	return false;
}

int CGraphicsBackend_SDL_OpenGL::GetWindowScreen()
{
	return SDL_GetDisplayForWindow(m_pWindow);
}

int CGraphicsBackend_SDL_OpenGL::GetVideoModes(CVideoMode *pModes, int MaxModes, int Screen)
{
	SDL_DisplayMode **ppModes;
	int NumModes;
	ppModes = SDL_GetFullscreenDisplayModes(GetWindowScreen(), &NumModes);
	if(NumModes < 0 || !ppModes)
	{
		dbg_msg("gfx", "unable to get the number of display modes: %s", SDL_GetError());
		return 0;
	}

	int ModesCount = 0;
	for(int i = 0; i < NumModes && ModesCount < MaxModes; i++)
	{
		if(!ppModes[i])
		{
			dbg_msg("gfx", "unable to get display mode: %s", SDL_GetError());
			continue;
		}

		bool AlreadyFound = false;
		for(int j = 0; j < ModesCount; j++)
		{
			if(pModes[j].m_Width == ppModes[i]->w && pModes[j].m_Height == ppModes[i]->h)
			{
				AlreadyFound = true;
				break;
			}
		}
		if(AlreadyFound)
			continue;

		pModes[ModesCount].m_Width = ppModes[i]->w;
		pModes[ModesCount].m_Height = ppModes[i]->h;
		ModesCount++;
	}
	return ModesCount;
}

bool CGraphicsBackend_SDL_OpenGL::GetDesktopResolution(int Index, int *pDesktopWidth, int *pDesktopHeight)
{
	const SDL_DisplayMode *pDisplayMode = SDL_GetDesktopDisplayMode(Index);
	if(!pDisplayMode)
		return false;

	*pDesktopWidth = pDisplayMode->w;
	*pDesktopHeight = pDisplayMode->h;
	return true;
}

int CGraphicsBackend_SDL_OpenGL::WindowActive()
{
	return SDL_GetWindowFlags(m_pWindow) & SDL_WINDOW_INPUT_FOCUS;
}

int CGraphicsBackend_SDL_OpenGL::WindowOpen()
{
	return !(SDL_GetWindowFlags(m_pWindow) & SDL_WINDOW_HIDDEN);
}

void *CGraphicsBackend_SDL_OpenGL::GetWindowHandle()
{
	return m_pWindow;
}

IGraphicsBackend *CreateGraphicsBackend() { return new CGraphicsBackend_SDL_OpenGL; }
