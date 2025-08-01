/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */

#include <base/color.h>
#include <base/system.h>

#include <engine/client.h>
#include <engine/console.h>
#include <engine/graphics.h>
#include <engine/input.h>
#include <engine/keys.h>
#include <engine/shared/config.h>
#include <engine/shared/datafile.h>
#include <engine/storage.h>
#include <engine/textrender.h>

#include <game/client/components/menus.h>
#include <game/client/lineinput.h>
#include <game/client/localization.h>
#include <game/client/render.h>
#include <game/client/ui.h>
#include <game/gamecore.h>
#include <generated/client_data.h>

#include "auto_map.h"
#include "editor.h"

const void *CEditor::ms_pUiGotContext;

enum
{
	BUTTON_CONTEXT = 1,
};

CEditorImage::~CEditorImage()
{
	m_pEditor->Graphics()->UnloadTexture(&m_Texture);
	if(m_pData)
	{
		mem_free(m_pData);
		m_pData = 0;
	}
	if(m_pAutoMapper)
	{
		delete m_pAutoMapper;
		m_pAutoMapper = 0;
	}
}

CLayerGroup::CLayerGroup()
{
	m_aName[0] = 0;
	m_Visible = true;
	m_SaveToMap = true;
	m_Collapse = false;
	m_GameGroup = false;
	m_OffsetX = 0;
	m_OffsetY = 0;
	m_ParallaxX = 100;
	m_ParallaxY = 100;

	m_UseClipping = 0;
	m_ClipX = 0;
	m_ClipY = 0;
	m_ClipW = 0;
	m_ClipH = 0;
}

CLayerGroup::~CLayerGroup()
{
	Clear();
}

void CLayerGroup::Convert(CUIRect *pRect)
{
	pRect->x += m_OffsetX;
	pRect->y += m_OffsetY;
}

void CLayerGroup::Mapping(float *pPoints)
{
	m_pMap->m_pEditor->RenderTools()->MapScreenToWorld(
		m_pMap->m_pEditor->m_WorldOffsetX, m_pMap->m_pEditor->m_WorldOffsetY,
		m_ParallaxX / 100.0f, m_ParallaxY / 100.0f,
		m_OffsetX, m_OffsetY,
		m_pMap->m_pEditor->Graphics()->ScreenAspect(), m_pMap->m_pEditor->m_WorldZoom, pPoints);

	pPoints[0] += m_pMap->m_pEditor->m_EditorOffsetX;
	pPoints[1] += m_pMap->m_pEditor->m_EditorOffsetY;
	pPoints[2] += m_pMap->m_pEditor->m_EditorOffsetX;
	pPoints[3] += m_pMap->m_pEditor->m_EditorOffsetY;
}

void CLayerGroup::MapScreen()
{
	float aPoints[4];
	Mapping(aPoints);
	m_pMap->m_pEditor->Graphics()->MapScreen(aPoints[0], aPoints[1], aPoints[2], aPoints[3]);
}

void CLayerGroup::Render()
{
	MapScreen();
	IGraphics *pGraphics = m_pMap->m_pEditor->Graphics();

	if(m_UseClipping)
	{
		float aPoints[4];
		m_pMap->m_pGameGroup->Mapping(aPoints);
		float x0 = (m_ClipX - aPoints[0]) / (aPoints[2] - aPoints[0]);
		float y0 = (m_ClipY - aPoints[1]) / (aPoints[3] - aPoints[1]);
		float x1 = ((m_ClipX + m_ClipW) - aPoints[0]) / (aPoints[2] - aPoints[0]);
		float y1 = ((m_ClipY + m_ClipH) - aPoints[1]) / (aPoints[3] - aPoints[1]);

		pGraphics->ClipEnable((int) (x0 * pGraphics->ScreenWidth()), (int) (y0 * pGraphics->ScreenHeight()),
			(int) ((x1 - x0) * pGraphics->ScreenWidth()), (int) ((y1 - y0) * pGraphics->ScreenHeight()));
	}

	for(int i = 0; i < m_lLayers.size(); i++)
	{
		if(m_lLayers[i]->m_Visible && m_lLayers[i] != m_pMap->m_pGameLayer)
		{
			if(m_pMap->m_pEditor->m_ShowDetail || !(m_lLayers[i]->m_Flags & LAYERFLAG_DETAIL))
				m_lLayers[i]->Render();
		}
	}

	if(m_UseClipping)
		pGraphics->ClipDisable();
}

void CLayerGroup::AddLayer(CLayer *l)
{
	m_pMap->m_Modified = true;
	m_lLayers.add(l);
}

void CLayerGroup::DeleteLayer(int Index)
{
	if(Index < 0 || Index >= m_lLayers.size())
		return;
	delete m_lLayers[Index];
	m_lLayers.remove_index(Index);
	m_pMap->m_Modified = true;
}

void CLayerGroup::GetSize(float *w, float *h) const
{
	*w = 0;
	*h = 0;
	for(int i = 0; i < m_lLayers.size(); i++)
	{
		float lw, lh;
		m_lLayers[i]->GetSize(&lw, &lh);
		*w = maximum(*w, lw);
		*h = maximum(*h, lh);
	}
}

int CLayerGroup::SwapLayers(int Index0, int Index1)
{
	if(Index0 < 0 || Index0 >= m_lLayers.size())
		return Index0;
	if(Index1 < 0 || Index1 >= m_lLayers.size())
		return Index0;
	if(Index0 == Index1)
		return Index0;
	m_pMap->m_Modified = true;
	std::swap(m_lLayers[Index0], m_lLayers[Index1]);
	return Index1;
}

void CEditorImage::AnalyzeTileFlags()
{
	mem_zero(m_aTileFlags, sizeof(m_aTileFlags));

	int tw = m_Width / 16; // tilesizes
	int th = m_Height / 16;
	if(tw == th)
	{
		unsigned char *pPixelData = (unsigned char *) m_pData;

		int TileID = 0;
		for(int ty = 0; ty < 16; ty++)
			for(int tx = 0; tx < 16; tx++, TileID++)
			{
				bool Opaque = true;
				for(int x = 0; x < tw; x++)
					for(int y = 0; y < th; y++)
					{
						int p = (ty * tw + y) * m_Width + tx * tw + x;
						if(pPixelData[p * 4 + 3] < 250)
						{
							Opaque = false;
							break;
						}
					}

				if(Opaque)
					m_aTileFlags[TileID] |= TILEFLAG_OPAQUE;
			}
	}
}

void CEditorImage::LoadAutoMapper()
{
	if(m_pAutoMapper)
		return;

	char aBuf[IO_MAX_PATH_LENGTH];
	str_format(aBuf, sizeof(aBuf), "editor/automap/%s.json", m_aName);
	CJsonParser JsonParser;
	const json_value *pJsonData = JsonParser.ParseFile(aBuf, m_pEditor->Storage());
	if(pJsonData == 0)
	{
		m_pEditor->Console()->Print(IConsole::OUTPUT_LEVEL_ADDINFO, "editor", JsonParser.Error());
		return;
	}

	// generate configurations
	const json_value &rTileset = (*pJsonData)[(const char *) IAutoMapper::GetTypeName(IAutoMapper::TYPE_TILESET)];
	if(rTileset.type == json_array)
	{
		m_pAutoMapper = new CTilesetMapper(m_pEditor);
		m_pAutoMapper->Load(rTileset);
	}
	else
	{
		const json_value &rDoodads = (*pJsonData)[(const char *) IAutoMapper::GetTypeName(IAutoMapper::TYPE_DOODADS)];
		if(rDoodads.type == json_array)
		{
			m_pAutoMapper = new CDoodadsMapper(m_pEditor);
			m_pAutoMapper->Load(rDoodads);
		}
	}

	if(m_pAutoMapper && m_pEditor->Config()->m_Debug)
	{
		str_format(aBuf, sizeof(aBuf), "loaded %s.json (%s)", m_aName, IAutoMapper::GetTypeName(m_pAutoMapper->GetType()));
		m_pEditor->Console()->Print(IConsole::OUTPUT_LEVEL_DEBUG, "editor", aBuf);
	}
}

void CEditor::EnvelopeEval(float TimeOffset, int Env, float *pChannels, void *pUser)
{
	CEditor *pThis = (CEditor *) pUser;
	if(Env < 0 || Env >= pThis->m_Map.m_lEnvelopes.size())
	{
		pChannels[0] = 0;
		pChannels[1] = 0;
		pChannels[2] = 0;
		pChannels[3] = 0;
		return;
	}

	CEnvelope *e = pThis->m_Map.m_lEnvelopes[Env];
	float t = pThis->m_AnimateTime;
	t *= pThis->m_AnimateSpeed;
	t += TimeOffset;
	e->Eval(t, pChannels);
}

/********************************************************
 OTHER
*********************************************************/

vec4 CEditor::GetButtonColor(const void *pID, int Checked)
{
	if(Checked < 0)
		return vec4(0, 0, 0, 0.5f);

	if(Checked > 0)
	{
		if(UI()->HotItem() == pID)
			return vec4(1, 0, 0, 0.75f);
		return vec4(1, 0, 0, 0.5f);
	}

	if(UI()->HotItem() == pID)
		return vec4(1, 1, 1, 0.75f);
	return vec4(1, 1, 1, 0.5f);
}

int CEditor::DoButton_Editor_Common(const void *pID, const char *pText, int Checked, const CUIRect *pRect, int Flags, const char *pToolTip)
{
	if(UI()->MouseInside(pRect))
	{
		if(Flags & BUTTON_CONTEXT)
			ms_pUiGotContext = pID;
		if(m_pTooltip)
			m_pTooltip = pToolTip;
	}

	if(UI()->HotItem() == pID && pToolTip)
		m_pTooltip = (const char *) pToolTip;

	if(UI()->DoButtonLogic(pID, pRect, 0))
		return 1;
	if(UI()->DoButtonLogic(pID, pRect, 1))
		return 2;
	return 0;
}

int CEditor::DoButton_Editor(const void *pID, const char *pText, int Checked, const CUIRect *pRect, int Flags, const char *pToolTip)
{
	pRect->Draw(GetButtonColor(pID, Checked), 3.0f);
	UI()->DoLabel(pRect, pText, 10.0f, TEXTALIGN_MC);
	return DoButton_Editor_Common(pID, pText, Checked, pRect, Flags, pToolTip);
}

int CEditor::DoButton_Image(const void *pID, const char *pText, int Checked, const CUIRect *pRect, int Flags, const char *pToolTip, bool Used)
{
	// darken the button if not used
	vec4 ButtonColor = GetButtonColor(pID, Checked);
	if(!Used)
		ButtonColor *= vec4(0.5f, 0.5f, 0.5f, 1.0f);

	const float FontSize = clamp(8.0f * pRect->w / TextRender()->TextWidth(10.0f, pText, -1), 6.0f, 10.0f);
	pRect->Draw(ButtonColor, 3.0f);
	UI()->DoLabel(pRect, pText, FontSize, TEXTALIGN_MC);
	return DoButton_Editor_Common(pID, pText, Checked, pRect, Flags, pToolTip);
}

int CEditor::DoButton_Menu(const void *pID, const char *pText, int Checked, const CUIRect *pRect, int Flags, const char *pToolTip)
{
	pRect->Draw(vec4(0.5f, 0.5f, 0.5f, 1.0f), 3.0f, CUIRect::CORNER_T);

	CUIRect Label = *pRect;
	Label.VMargin(5.0f, &Label);
	UI()->DoLabel(&Label, pText, 10.0f, TEXTALIGN_ML);
	return DoButton_Editor_Common(pID, pText, Checked, pRect, Flags, pToolTip);
}

int CEditor::DoButton_MenuItem(const void *pID, const char *pText, int Checked, const CUIRect *pRect, int Flags, const char *pToolTip)
{
	if(UI()->HotItem() == pID || Checked)
		pRect->Draw(GetButtonColor(pID, Checked), 3.0f);

	CUIRect Label = *pRect;
	Label.VMargin(5.0f, &Label);
	UI()->DoLabel(&Label, pText, 10.0f, TEXTALIGN_ML);
	return DoButton_Editor_Common(pID, pText, Checked, pRect, Flags, pToolTip);
}

int CEditor::DoButton_Tab(const void *pID, const char *pText, int Checked, const CUIRect *pRect, int Flags, const char *pToolTip)
{
	pRect->Draw(GetButtonColor(pID, Checked), 5.0f, CUIRect::CORNER_T);
	UI()->DoLabel(pRect, pText, 10.0f, TEXTALIGN_MC);
	return DoButton_Editor_Common(pID, pText, Checked, pRect, Flags, pToolTip);
}

int CEditor::DoButton_Ex(const void *pID, const char *pText, int Checked, const CUIRect *pRect, int Flags, const char *pToolTip, int Corners, float FontSize)
{
	pRect->Draw(GetButtonColor(pID, Checked), 3.0f, Corners);
	UI()->DoLabel(pRect, pText, FontSize, TEXTALIGN_MC);
	return DoButton_Editor_Common(pID, pText, Checked, pRect, Flags, pToolTip);
}

int CEditor::DoButton_ButtonInc(const void *pID, const char *pText, int Checked, const CUIRect *pRect, int Flags, const char *pToolTip)
{
	pRect->Draw(GetButtonColor(pID, Checked), 3.0f, CUIRect::CORNER_R);
	UI()->DoLabel(pRect, pText ? pText : "+", 10.0f, TEXTALIGN_MC);
	return DoButton_Editor_Common(pID, pText, Checked, pRect, Flags, pToolTip);
}

int CEditor::DoButton_ButtonDec(const void *pID, const char *pText, int Checked, const CUIRect *pRect, int Flags, const char *pToolTip)
{
	pRect->Draw(GetButtonColor(pID, Checked), 3.0f, CUIRect::CORNER_L);
	UI()->DoLabel(pRect, pText ? pText : "-", 10.0f, TEXTALIGN_MC);
	return DoButton_Editor_Common(pID, pText, Checked, pRect, Flags, pToolTip);
}

void CEditor::RenderGrid(CLayerGroup *pGroup)
{
	if(!m_GridActive)
		return;

	vec4 GridColor;
	float aGroupPoints[4];
	pGroup->Mapping(aGroupPoints);

	float w = UI()->Screen()->w;
	float h = UI()->Screen()->h;

	int LineDistance = GetLineDistance();

	int XOffset = aGroupPoints[0] / LineDistance;
	int YOffset = aGroupPoints[1] / LineDistance;
	int XGridOffset = XOffset % m_GridFactor;
	int YGridOffset = YOffset % m_GridFactor;

	Graphics()->TextureClear();
	Graphics()->LinesBegin();

	for(int i = 0; i < (int) w; i++)
	{
		if((i + YGridOffset) % m_GridFactor == 0)
			GridColor = HexToRgba(Config()->m_EdColorGridOuter);
		else
			GridColor = HexToRgba(Config()->m_EdColorGridInner);

		Graphics()->SetColor(GridColor.r, GridColor.g, GridColor.b, GridColor.a);
		IGraphics::CLineItem Line = IGraphics::CLineItem(LineDistance * XOffset, LineDistance * i + LineDistance * YOffset, w + aGroupPoints[2], LineDistance * i + LineDistance * YOffset);
		Graphics()->LinesDraw(&Line, 1);

		if((i + XGridOffset) % m_GridFactor == 0)
			GridColor = HexToRgba(Config()->m_EdColorGridOuter);
		else
			GridColor = HexToRgba(Config()->m_EdColorGridInner);

		Graphics()->SetColor(GridColor.r, GridColor.g, GridColor.b, GridColor.a);
		Line = IGraphics::CLineItem(LineDistance * i + LineDistance * XOffset, LineDistance * YOffset, LineDistance * i + LineDistance * XOffset, h + aGroupPoints[3]);
		Graphics()->LinesDraw(&Line, 1);
	}
	Graphics()->SetColor(1.0f, 1.0f, 1.0f, 1.0f);
	Graphics()->LinesEnd();
}

void CEditor::RenderBackground(CUIRect View, IGraphics::CTextureHandle Texture, float Size, float Brightness)
{
	Graphics()->TextureSet(Texture);
	Graphics()->BlendNormal();
	Graphics()->QuadsBegin();
	Graphics()->SetColor(Brightness, Brightness, Brightness, 1.0f);
	Graphics()->QuadsSetSubset(0, 0, View.w / Size, View.h / Size);
	IGraphics::CQuadItem QuadItem(View.x, View.y, View.w, View.h);
	Graphics()->QuadsDrawTL(&QuadItem, 1);
	Graphics()->QuadsEnd();
}

int CEditor::UiDoValueSelector(void *pID, CUIRect *pRect, const char *pLabel, int Current, int Min, int Max, int Step, float Scale, const char *pToolTip)
{
	// logic
	static float s_Value;
	bool Inside = UI()->MouseInside(pRect);

	if(UI()->CheckActiveItem(pID))
	{
		if(!UI()->MouseButton(0) || Input()->KeyPress(KEY_ESCAPE))
		{
			m_LockMouse = false;
			UI()->SetActiveItem(0);
		}
		else
		{
			if(Input()->KeyIsPressed(KEY_LSHIFT) || Input()->KeyIsPressed(KEY_RSHIFT))
				s_Value += m_MouseDeltaX * 0.05f;
			else
				s_Value += m_MouseDeltaX;

			if(absolute(s_Value) > Scale)
			{
				int Count = (int) (s_Value / Scale);
				s_Value = fmod(s_Value, Scale);
				Current += Step * Count;
				if(Current < Min)
					Current = Min;
				if(Current > Max)
					Current = Max;
			}
		}
		if(pToolTip)
			m_pTooltip = pToolTip;
	}
	else if(UI()->HotItem() == pID)
	{
		if(UI()->MouseButton(0))
		{
			m_LockMouse = true;
			s_Value = 0;
			UI()->SetActiveItem(pID);
		}
		if(pToolTip)
			m_pTooltip = pToolTip;
	}

	if(Inside)
		UI()->SetHotItem(pID);

	// render
	char aBuf[128];
	str_format(aBuf, sizeof(aBuf), "%s %d", pLabel, Current);
	pRect->Draw(GetButtonColor(pID, 0));
	pRect->y += pRect->h / 2.0f - 7.0f;
	UI()->DoLabel(pRect, aBuf, 10, TEXTALIGN_CENTER);

	return Current;
}

CLayerGroup *CEditor::GetSelectedGroup()
{
	if(m_SelectedGroup >= 0 && m_SelectedGroup < m_Map.m_lGroups.size())
		return m_Map.m_lGroups[m_SelectedGroup];
	return 0x0;
}

CLayer *CEditor::GetSelectedLayer(int Index)
{
	CLayerGroup *pGroup = GetSelectedGroup();
	if(!pGroup)
		return 0x0;

	if(m_SelectedLayer >= 0 && m_SelectedLayer < m_Map.m_lGroups[m_SelectedGroup]->m_lLayers.size())
		return pGroup->m_lLayers[m_SelectedLayer];
	return 0x0;
}

CLayer *CEditor::GetSelectedLayerType(int Index, int Type)
{
	CLayer *p = GetSelectedLayer(Index);
	if(p && p->m_Type == Type)
		return p;
	return 0x0;
}

CQuad *CEditor::GetSelectedQuad()
{
	CLayerQuads *ql = (CLayerQuads *) GetSelectedLayerType(0, LAYERTYPE_QUADS);
	if(!ql)
		return 0;
	if(m_SelectedQuad >= 0 && m_SelectedQuad < ql->m_lQuads.size())
		return &ql->m_lQuads[m_SelectedQuad];
	return 0;
}

void CEditor::CallbackOpenMap(const char *pFileName, int StorageType, void *pUser)
{
	CEditor *pEditor = (CEditor *) pUser;
	if(pEditor->Load(pFileName, StorageType))
	{
		str_copy(pEditor->m_aFileName, pFileName, 512);
		pEditor->m_ValidSaveFilename = StorageType == IStorage::TYPE_SAVE && pEditor->m_pFileDialogPath == pEditor->m_aFileDialogCurrentFolder;
		pEditor->SortImages();
		pEditor->m_Dialog = DIALOG_NONE;
		pEditor->m_Map.m_Modified = false;
	}
	else
	{
		pEditor->Reset();
		pEditor->m_aFileName[0] = 0;
	}
}
void CEditor::CallbackAppendMap(const char *pFileName, int StorageType, void *pUser)
{
	CEditor *pEditor = (CEditor *) pUser;
	if(pEditor->Append(pFileName, StorageType))
		pEditor->m_aFileName[0] = 0;
	else
		pEditor->SortImages();

	pEditor->m_Dialog = DIALOG_NONE;
}
void CEditor::CallbackSaveMap(const char *pFileName, int StorageType, void *pUser)
{
	CEditor *pEditor = static_cast<CEditor *>(pUser);
	char aBuf[1024];
	// add map extension
	if(!str_endswith(pFileName, ".map"))
	{
		str_format(aBuf, sizeof(aBuf), "%s.map", pFileName);
		pFileName = aBuf;
	}

	if(pEditor->Save(pFileName))
	{
		str_copy(pEditor->m_aFileName, pFileName, sizeof(pEditor->m_aFileName));
		pEditor->m_ValidSaveFilename = StorageType == IStorage::TYPE_SAVE && pEditor->m_pFileDialogPath == pEditor->m_aFileDialogCurrentFolder;
		pEditor->m_Map.m_Modified = false;
	}

	pEditor->m_Dialog = DIALOG_NONE;
}

void CEditor::DoToolbar(CUIRect ToolBar)
{
	CUIRect TB_Top, TB_Bottom;
	CUIRect Button;

	ToolBar.HSplitTop(ToolBar.h / 2.0f, &TB_Top, &TB_Bottom);

	TB_Top.HSplitBottom(2.5f, &TB_Top, 0);
	TB_Bottom.HSplitTop(2.5f, 0, &TB_Bottom);

	// ctrl+o to open
	if(Input()->KeyPress(KEY_O) && (Input()->KeyIsPressed(KEY_LCTRL) || Input()->KeyIsPressed(KEY_RCTRL)) && m_Dialog == DIALOG_NONE)
	{
		if(HasUnsavedData())
		{
			if(!m_PopupEventWasActivated)
			{
				m_PopupEventType = POPEVENT_LOAD;
				m_PopupEventActivated = true;
			}
		}
		else
			InvokeFileDialog(IStorage::TYPE_ALL, FILETYPE_MAP, "Load map", "Load", "maps", "", CallbackOpenMap, this);
	}

	// ctrl+s to save
	if(Input()->KeyPress(KEY_S) && (Input()->KeyIsPressed(KEY_LCTRL) || Input()->KeyIsPressed(KEY_RCTRL)) && m_Dialog == DIALOG_NONE)
	{
		if(m_aFileName[0] && m_ValidSaveFilename)
		{
			if(!m_PopupEventWasActivated)
			{
				str_copy(m_aFileSaveName, m_aFileName, sizeof(m_aFileSaveName));
				m_PopupEventType = POPEVENT_SAVE;
				m_PopupEventActivated = true;
			}
		}
		else
			InvokeFileDialog(IStorage::TYPE_SAVE, FILETYPE_MAP, "Save map", "Save", "maps", "", CallbackSaveMap, this);
	}

	// detail button
	TB_Top.VSplitLeft(30.0f, &Button, &TB_Top);
	static int s_HqButton = 0;
	if(DoButton_Editor(&s_HqButton, "HD", m_ShowDetail, &Button, 0, "[ctrl+h] Toggle High Detail") ||
		(Input()->KeyPress(KEY_H) && (Input()->KeyIsPressed(KEY_LCTRL) || Input()->KeyIsPressed(KEY_RCTRL))))
	{
		m_ShowDetail = !m_ShowDetail;
	}

	TB_Top.VSplitLeft(5.0f, 0, &TB_Top);

	// animation button
	TB_Top.VSplitLeft(40.0f, &Button, &TB_Top);
	static int s_AnimateButton = 0;
	if(DoButton_Editor(&s_AnimateButton, "Anim", m_Animate, &Button, 0, "[ctrl+m] Toggle animation") ||
		(Input()->KeyPress(KEY_M) && (Input()->KeyIsPressed(KEY_LCTRL) || Input()->KeyIsPressed(KEY_RCTRL))))
	{
		m_AnimateStart = time_get();
		m_Animate = !m_Animate;
	}

	TB_Top.VSplitLeft(5.0f, 0, &TB_Top);

	// proof button
	TB_Top.VSplitLeft(40.0f, &Button, &TB_Top);
	static int s_ProofButton = 0;
	if(DoButton_Editor(&s_ProofButton, "Proof", m_ProofBorders, &Button, 0, "[ctrl+p] Toggle proof borders. These borders represent the maximum range players are able to see in-game.") ||
		(Input()->KeyPress(KEY_P) && (Input()->KeyIsPressed(KEY_LCTRL) || Input()->KeyIsPressed(KEY_RCTRL))))
	{
		m_ProofBorders = !m_ProofBorders;
	}

	TB_Top.VSplitLeft(5.0f, 0, &TB_Top);

	// tile info button
	TB_Top.VSplitLeft(40.0f, &Button, &TB_Top);
	static int s_TileInfoButton = 0;
	if(DoButton_Editor(&s_TileInfoButton, "Info", m_ShowTileInfo, &Button, 0, "[ctrl+i] Show tile information") ||
		(Input()->KeyPress(KEY_I) && (Input()->KeyIsPressed(KEY_LCTRL) || Input()->KeyIsPressed(KEY_RCTRL))))
	{
		m_ShowTileInfo = !m_ShowTileInfo;
		m_ShowEnvelopePreview = SHOWENV_NONE;
	}

	TB_Top.VSplitLeft(15.0f, 0, &TB_Top);

	// zoom group
	TB_Top.VSplitLeft(30.0f, &Button, &TB_Top);
	static int s_ZoomOutButton = 0;
	if(DoButton_Ex(&s_ZoomOutButton, "ZO", 0, &Button, 0, "[NumPad-] Zoom out", CUIRect::CORNER_L))
		m_ZoomLevel += 50;

	TB_Top.VSplitLeft(30.0f, &Button, &TB_Top);
	static int s_ZoomNormalButton = 0;
	if(DoButton_Ex(&s_ZoomNormalButton, "1:1", 0, &Button, 0, "[NumPad*] Zoom to normal and remove editor offset", 0))
	{
		m_EditorOffsetX = 0;
		m_EditorOffsetY = 0;
		m_ZoomLevel = 100;
	}

	TB_Top.VSplitLeft(30.0f, &Button, &TB_Top);
	static int s_ZoomInButton = 0;
	if(DoButton_Ex(&s_ZoomInButton, "ZI", 0, &Button, 0, "[NumPad+] Zoom in", CUIRect::CORNER_R))
		m_ZoomLevel -= 50;

	TB_Top.VSplitLeft(10.0f, 0, &TB_Top);

	// animation speed
	TB_Top.VSplitLeft(30.0f, &Button, &TB_Top);
	static int s_AnimFasterButton = 0;
	if(DoButton_Ex(&s_AnimFasterButton, "A+", 0, &Button, 0, "Increase animation speed", CUIRect::CORNER_L))
		m_AnimateSpeed += 0.5f;

	TB_Top.VSplitLeft(30.0f, &Button, &TB_Top);
	static int s_AnimNormalButton = 0;
	if(DoButton_Ex(&s_AnimNormalButton, "1", 0, &Button, 0, "Normal animation speed", 0))
		m_AnimateSpeed = 1.0f;

	TB_Top.VSplitLeft(30.0f, &Button, &TB_Top);
	static int s_AnimSlowerButton = 0;
	if(DoButton_Ex(&s_AnimSlowerButton, "A-", 0, &Button, 0, "Decrease animation speed", CUIRect::CORNER_R))
	{
		if(m_AnimateSpeed > 0.5f)
			m_AnimateSpeed -= 0.5f;
	}

	TB_Top.VSplitLeft(10.0f, &Button, &TB_Top);

	// brush manipulation
	{
		int Enabled = m_Brush.IsEmpty() ? -1 : 0;

		// flip buttons
		TB_Top.VSplitLeft(30.0f, &Button, &TB_Top);
		static int s_FlipXButton = 0;
		if(DoButton_Ex(&s_FlipXButton, "X/X", Enabled, &Button, 0, "[N] Flip brush horizontal", CUIRect::CORNER_L) || Input()->KeyPress(KEY_N))
		{
			for(int i = 0; i < m_Brush.m_lLayers.size(); i++)
				m_Brush.m_lLayers[i]->BrushFlipX();
		}

		TB_Top.VSplitLeft(30.0f, &Button, &TB_Top);
		static int s_FlipyButton = 0;
		if(DoButton_Ex(&s_FlipyButton, "Y/Y", Enabled, &Button, 0, "[M] Flip brush vertical", CUIRect::CORNER_R) || Input()->KeyPress(KEY_M))
		{
			for(int i = 0; i < m_Brush.m_lLayers.size(); i++)
				m_Brush.m_lLayers[i]->BrushFlipY();
		}

		// rotate buttons
		TB_Top.VSplitLeft(15.0f, &Button, &TB_Top);

		TB_Top.VSplitLeft(30.0f, &Button, &TB_Top);
		static int s_RotationAmount = 90;
		bool TileLayer = false;
		// check for tile layers in brush selection
		for(int i = 0; i < m_Brush.m_lLayers.size(); i++)
			if(m_Brush.m_lLayers[i]->m_Type == LAYERTYPE_TILES)
			{
				TileLayer = true;
				s_RotationAmount = maximum(90, (s_RotationAmount / 90) * 90);
				break;
			}
		s_RotationAmount = UiDoValueSelector(&s_RotationAmount, &Button, "", s_RotationAmount, TileLayer ? 90 : 1, 359, TileLayer ? 90 : 1, TileLayer ? 10.0f : 2.0f, "Rotation of the brush in degrees. Hold down the left mouse button to change the value. Hold Shift for more precision.");

		TB_Top.VSplitLeft(5.0f, &Button, &TB_Top);
		TB_Top.VSplitLeft(30.0f, &Button, &TB_Top);
		static int s_CcwButton = 0;
		if(DoButton_Ex(&s_CcwButton, "CCW", Enabled, &Button, 0, "[R] Rotates the brush counter-clockwise", CUIRect::CORNER_L) || Input()->KeyPress(KEY_R))
		{
			for(int i = 0; i < m_Brush.m_lLayers.size(); i++)
				m_Brush.m_lLayers[i]->BrushRotate(-s_RotationAmount / 360.0f * pi * 2);
		}

		TB_Top.VSplitLeft(30.0f, &Button, &TB_Top);
		static int s_CwButton = 0;
		if(DoButton_Ex(&s_CwButton, "CW", Enabled, &Button, 0, "[T] Rotates the brush clockwise", CUIRect::CORNER_R) || Input()->KeyPress(KEY_T))
		{
			for(int i = 0; i < m_Brush.m_lLayers.size(); i++)
				m_Brush.m_lLayers[i]->BrushRotate(s_RotationAmount / 360.0f * pi * 2);
		}
	}

	// quad manipulation
	{
		// do add button
		TB_Top.VSplitLeft(10.0f, &Button, &TB_Top);
		TB_Top.VSplitLeft(60.0f, &Button, &TB_Top);
		static int s_NewButton = 0;

		CLayerQuads *pQLayer = (CLayerQuads *) GetSelectedLayerType(0, LAYERTYPE_QUADS);
		// CLayerTiles *tlayer = (CLayerTiles *)get_selected_layer_type(0, LAYERTYPE_TILES);
		if(DoButton_Editor(&s_NewButton, "Add Quad", pQLayer ? 0 : -1, &Button, 0, "Adds a new quad"))
		{
			if(pQLayer)
			{
				float Mapping[4];
				CLayerGroup *g = GetSelectedGroup();
				g->Mapping(Mapping);
				int AddX = f2fx(Mapping[0] + (Mapping[2] - Mapping[0]) / 2);
				int AddY = f2fx(Mapping[1] + (Mapping[3] - Mapping[1]) / 2);

				CQuad *q = pQLayer->NewQuad();
				for(int i = 0; i < 5; i++)
				{
					q->m_aPoints[i].x += AddX;
					q->m_aPoints[i].y += AddY;
				}
			}
		}
	}

	// tile manipulation
	{
		TB_Bottom.VSplitLeft(40.0f, &Button, &TB_Bottom);
		static int s_BorderBut = 0;
		CLayerTiles *pT = (CLayerTiles *) GetSelectedLayerType(0, LAYERTYPE_TILES);

		if(DoButton_Editor(&s_BorderBut, "Border", pT ? 0 : -1, &Button, 0, "Adds border tiles"))
		{
			if(pT)
				DoMapBorder();
		}
	}

	TB_Bottom.VSplitLeft(5.0f, 0, &TB_Bottom);

	// refocus button
	TB_Bottom.VSplitLeft(50.0f, &Button, &TB_Bottom);
	static int s_RefocusButton = 0;
	if(DoButton_Editor(&s_RefocusButton, "Refocus", m_WorldOffsetX && m_WorldOffsetY ? 0 : -1, &Button, 0, "[HOME] Restore map focus") || (m_EditBoxActive == 0 && Input()->KeyPress(KEY_HOME)))
	{
		m_WorldOffsetX = 0;
		m_WorldOffsetY = 0;
	}

	TB_Bottom.VSplitLeft(5.0f, 0, &TB_Bottom);

	// grid button
	TB_Bottom.VSplitLeft(50.0f, &Button, &TB_Bottom);
	static int s_GridButton = 0;
	if(DoButton_Editor(&s_GridButton, "Grid", m_GridActive, &Button, 0, "Toggle grid"))
	{
		m_GridActive = !m_GridActive;
	}

	TB_Bottom.VSplitLeft(30.0f, 0, &TB_Bottom);

	// grid zoom
	TB_Bottom.VSplitLeft(30.0f, &Button, &TB_Bottom);
	static int s_GridIncreaseButton = 0;
	if(DoButton_Ex(&s_GridIncreaseButton, "G-", 0, &Button, 0, "Decrease grid", CUIRect::CORNER_L))
	{
		if(m_GridFactor > 1)
			m_GridFactor--;
	}

	TB_Bottom.VSplitLeft(30.0f, &Button, &TB_Bottom);
	static int s_GridNormalButton = 0;
	if(DoButton_Ex(&s_GridNormalButton, "1", 0, &Button, 0, "Normal grid", 0))
		m_GridFactor = 1;

	TB_Bottom.VSplitLeft(30.0f, &Button, &TB_Bottom);

	static int s_GridDecreaseButton = 0;
	if(DoButton_Ex(&s_GridDecreaseButton, "G+", 0, &Button, 0, "Increase grid", CUIRect::CORNER_R))
	{
		if(m_GridFactor < 15)
			m_GridFactor++;
	}

	TB_Bottom.VSplitLeft(10.0f, 0, &TB_Bottom);

	// pipette / color picking
	TB_Bottom.VSplitLeft(50.0f, &Button, &TB_Bottom);
	static int s_ColorPickingButton = 0;
	if(DoButton_Editor(&s_ColorPickingButton, "Pipette", m_MouseEdMode == MOUSE_PIPETTE, &Button, 0, "Pick color from view"))
	{
		// toggle mouse mode
		if(m_MouseEdMode == MOUSE_PIPETTE)
			m_MouseEdMode = MOUSE_EDIT;
		else
			m_MouseEdMode = MOUSE_PIPETTE;
	}

	// display selected color
	if(m_SelectedColor.a > 0.0f)
	{
		TB_Bottom.VSplitLeft(4.0f, 0, &TB_Bottom);

		TB_Bottom.VSplitLeft(24.0f, &Button, &TB_Bottom);
		Button.Draw(m_SelectedColor, 0.0f, CUIRect::CORNER_NONE);
	}
}

static void Rotate(const CPoint *pCenter, CPoint *pPoint, float Rotation)
{
	int x = pPoint->x - pCenter->x;
	int y = pPoint->y - pCenter->y;
	pPoint->x = (int) (x * cosf(Rotation) - y * sinf(Rotation) + pCenter->x);
	pPoint->y = (int) (x * sinf(Rotation) + y * cosf(Rotation) + pCenter->y);
}

void CEditor::DoQuad(CQuad *q, int Index)
{
	enum
	{
		OP_NONE = 0,
		OP_MOVE_ALL,
		OP_MOVE_PIVOT,
		OP_ROTATE,
		OP_CONTEXT_MENU,
	};

	// some basic values
	void *pID = &q->m_aPoints[4]; // use pivot addr as id
	static CPoint s_RotatePoints[4];
	static float s_LastWx;
	static float s_LastWy;
	static int s_Operation = OP_NONE;
	static float s_RotateAngle = 0;
	float wx = UI()->MouseWorldX();
	float wy = UI()->MouseWorldY();

	// get pivot
	float CenterX = fx2f(q->m_aPoints[4].x);
	float CenterY = fx2f(q->m_aPoints[4].y);

	float dx = (CenterX - wx) / m_WorldZoom;
	float dy = (CenterY - wy) / m_WorldZoom;
	if(dx * dx + dy * dy < 50)
		UI()->SetHotItem(pID);

	bool IgnoreGrid;
	if(Input()->KeyIsPressed(KEY_LALT) || Input()->KeyIsPressed(KEY_RALT))
		IgnoreGrid = true;
	else
		IgnoreGrid = false;

	// draw selection background
	if(m_SelectedQuad == Index)
	{
		Graphics()->SetColor(0, 0, 0, 1);
		IGraphics::CQuadItem QuadItem(CenterX, CenterY, 7.0f * m_WorldZoom, 7.0f * m_WorldZoom);
		Graphics()->QuadsDraw(&QuadItem, 1);
	}

	vec4 PivotColor;

	if(UI()->CheckActiveItem(pID))
	{
		if(m_MouseDeltaWx * m_MouseDeltaWx + m_MouseDeltaWy * m_MouseDeltaWy > 0.5f)
		{
			// check if we only should move pivot
			if(s_Operation == OP_MOVE_PIVOT)
			{
				if(m_GridActive && !IgnoreGrid)
				{
					int LineDistance = GetLineDistance();

					float x = 0.0f;
					float y = 0.0f;
					if(wx >= 0)
						x = (int) ((wx + (LineDistance / 2) * m_GridFactor) / (LineDistance * m_GridFactor)) * (LineDistance * m_GridFactor);
					else
						x = (int) ((wx - (LineDistance / 2) * m_GridFactor) / (LineDistance * m_GridFactor)) * (LineDistance * m_GridFactor);
					if(wy >= 0)
						y = (int) ((wy + (LineDistance / 2) * m_GridFactor) / (LineDistance * m_GridFactor)) * (LineDistance * m_GridFactor);
					else
						y = (int) ((wy - (LineDistance / 2) * m_GridFactor) / (LineDistance * m_GridFactor)) * (LineDistance * m_GridFactor);

					q->m_aPoints[4].x = f2fx(x);
					q->m_aPoints[4].y = f2fx(y);
				}
				else
				{
					q->m_aPoints[4].x += f2fx(wx - s_LastWx);
					q->m_aPoints[4].y += f2fx(wy - s_LastWy);
				}
			}
			else if(s_Operation == OP_MOVE_ALL)
			{
				// move all points including pivot
				if(m_GridActive && !IgnoreGrid)
				{
					int LineDistance = GetLineDistance();

					float x = 0.0f;
					float y = 0.0f;
					if(wx >= 0)
						x = (int) ((wx + (LineDistance / 2) * m_GridFactor) / (LineDistance * m_GridFactor)) * (LineDistance * m_GridFactor);
					else
						x = (int) ((wx - (LineDistance / 2) * m_GridFactor) / (LineDistance * m_GridFactor)) * (LineDistance * m_GridFactor);
					if(wy >= 0)
						y = (int) ((wy + (LineDistance / 2) * m_GridFactor) / (LineDistance * m_GridFactor)) * (LineDistance * m_GridFactor);
					else
						y = (int) ((wy - (LineDistance / 2) * m_GridFactor) / (LineDistance * m_GridFactor)) * (LineDistance * m_GridFactor);

					int OldX = q->m_aPoints[4].x;
					int OldY = q->m_aPoints[4].y;
					q->m_aPoints[4].x = f2fx(x);
					q->m_aPoints[4].y = f2fx(y);
					int DiffX = q->m_aPoints[4].x - OldX;
					int DiffY = q->m_aPoints[4].y - OldY;

					for(int v = 0; v < 4; v++)
					{
						q->m_aPoints[v].x += DiffX;
						q->m_aPoints[v].y += DiffY;
					}
				}
				else
				{
					for(int v = 0; v < 5; v++)
					{
						q->m_aPoints[v].x += f2fx(wx - s_LastWx);
						q->m_aPoints[v].y += f2fx(wy - s_LastWy);
					}
				}
			}
			else if(s_Operation == OP_ROTATE)
			{
				for(int v = 0; v < 4; v++)
				{
					q->m_aPoints[v] = s_RotatePoints[v];
					Rotate(&q->m_aPoints[4], &q->m_aPoints[v], s_RotateAngle);
				}
			}
		}

		s_RotateAngle += (m_MouseDeltaX) * 0.002f;
		s_LastWx = wx;
		s_LastWy = wy;

		if(s_Operation == OP_CONTEXT_MENU)
		{
			if(!UI()->MouseButton(1))
			{
				UI()->DoPopupMenu(UI()->MouseX(), UI()->MouseY(), 120, 180, this, PopupQuad);
				m_LockMouse = false;
				s_Operation = OP_NONE;
				UI()->SetActiveItem(0);
			}
		}
		else
		{
			if(!UI()->MouseButton(0))
			{
				m_LockMouse = false;
				s_Operation = OP_NONE;
				UI()->SetActiveItem(0);
			}
		}

		PivotColor = HexToRgba(Config()->m_EdColorQuadPivotActive);
	}
	else if(UI()->HotItem() == pID)
	{
		ms_pUiGotContext = pID;

		PivotColor = HexToRgba(Config()->m_EdColorQuadPivotHover);
		m_pTooltip = "Left mouse button to move. Hold Shift to move pivot. Hold Ctrl to rotate. Hold Alt to ignore grid.";

		if(UI()->MouseButton(0))
		{
			if(Input()->KeyIsPressed(KEY_LSHIFT) || Input()->KeyIsPressed(KEY_RSHIFT))
				s_Operation = OP_MOVE_PIVOT;
			else if(Input()->KeyIsPressed(KEY_LCTRL) || Input()->KeyIsPressed(KEY_RCTRL))
			{
				m_LockMouse = true;
				s_Operation = OP_ROTATE;
				s_RotateAngle = 0;
				s_RotatePoints[0] = q->m_aPoints[0];
				s_RotatePoints[1] = q->m_aPoints[1];
				s_RotatePoints[2] = q->m_aPoints[2];
				s_RotatePoints[3] = q->m_aPoints[3];
			}
			else
				s_Operation = OP_MOVE_ALL;

			UI()->SetActiveItem(pID);
			if(m_SelectedQuad != Index)
				m_SelectedPoints = 0;
			m_SelectedQuad = Index;
			s_LastWx = wx;
			s_LastWy = wy;
		}

		if(UI()->MouseButton(1))
		{
			if(m_SelectedQuad != Index)
				m_SelectedPoints = 0;
			m_SelectedQuad = Index;
			s_Operation = OP_CONTEXT_MENU;
			UI()->SetActiveItem(pID);
		}
	}
	else
		PivotColor = HexToRgba(Config()->m_EdColorQuadPivot);

	Graphics()->SetColor(PivotColor.r, PivotColor.g, PivotColor.b, PivotColor.a);
	IGraphics::CQuadItem QuadItem(CenterX, CenterY, 5.0f * m_WorldZoom, 5.0f * m_WorldZoom);
	Graphics()->QuadsDraw(&QuadItem, 1);
}

void CEditor::DoQuadPoint(CQuad *pQuad, int QuadIndex, int V)
{
	void *pID = &pQuad->m_aPoints[V];

	float wx = UI()->MouseWorldX();
	float wy = UI()->MouseWorldY();

	float px = fx2f(pQuad->m_aPoints[V].x);
	float py = fx2f(pQuad->m_aPoints[V].y);

	float dx = (px - wx) / m_WorldZoom;
	float dy = (py - wy) / m_WorldZoom;
	if(dx * dx + dy * dy < 50)
		UI()->SetHotItem(pID);

	// draw selection background
	if(m_SelectedQuad == QuadIndex && m_SelectedPoints & (1 << V))
	{
		Graphics()->SetColor(0, 0, 0, 1);
		IGraphics::CQuadItem QuadItem(px, py, 7.0f * m_WorldZoom, 7.0f * m_WorldZoom);
		Graphics()->QuadsDraw(&QuadItem, 1);
	}

	enum
	{
		OP_NONE = 0,
		OP_MOVEPOINT,
		OP_MOVEUV,
		OP_CONTEXT_MENU
	};

	static bool s_Moved;
	static int s_Operation = OP_NONE;

	bool IgnoreGrid;
	if(Input()->KeyIsPressed(KEY_LALT) || Input()->KeyIsPressed(KEY_RALT))
		IgnoreGrid = true;
	else
		IgnoreGrid = false;

	vec4 pointColor;

	if(UI()->CheckActiveItem(pID))
	{
		float dx = m_MouseDeltaWx;
		float dy = m_MouseDeltaWy;
		if(!s_Moved)
		{
			if(dx * dx + dy * dy > 0.5f)
				s_Moved = true;
		}

		if(s_Moved)
		{
			if(s_Operation == OP_MOVEPOINT)
			{
				if(m_GridActive && !IgnoreGrid)
				{
					for(int m = 0; m < 4; m++)
						if(m_SelectedPoints & (1 << m))
						{
							int LineDistance = GetLineDistance();

							float x = 0.0f;
							float y = 0.0f;
							if(wx >= 0)
								x = (int) ((wx + (LineDistance / 2) * m_GridFactor) / (LineDistance * m_GridFactor)) * (LineDistance * m_GridFactor);
							else
								x = (int) ((wx - (LineDistance / 2) * m_GridFactor) / (LineDistance * m_GridFactor)) * (LineDistance * m_GridFactor);
							if(wy >= 0)
								y = (int) ((wy + (LineDistance / 2) * m_GridFactor) / (LineDistance * m_GridFactor)) * (LineDistance * m_GridFactor);
							else
								y = (int) ((wy - (LineDistance / 2) * m_GridFactor) / (LineDistance * m_GridFactor)) * (LineDistance * m_GridFactor);

							pQuad->m_aPoints[m].x = f2fx(x);
							pQuad->m_aPoints[m].y = f2fx(y);
						}
				}
				else
				{
					for(int m = 0; m < 4; m++)
						if(m_SelectedPoints & (1 << m))
						{
							pQuad->m_aPoints[m].x += f2fx(dx);
							pQuad->m_aPoints[m].y += f2fx(dy);
						}
				}
			}
			else if(s_Operation == OP_MOVEUV)
			{
				for(int m = 0; m < 4; m++)
					if(m_SelectedPoints & (1 << m))
					{
						// 0,2;1,3 - line x
						// 0,1;2,3 - line y

						pQuad->m_aTexcoords[m].x += f2fx(dx * 0.001f);
						pQuad->m_aTexcoords[(m + 2) % 4].x += f2fx(dx * 0.001f);

						pQuad->m_aTexcoords[m].y += f2fx(dy * 0.001f);
						pQuad->m_aTexcoords[m ^ 1].y += f2fx(dy * 0.001f);
					}
			}
		}

		if(s_Operation == OP_CONTEXT_MENU)
		{
			if(!UI()->MouseButton(1))
			{
				UI()->DoPopupMenu(UI()->MouseX(), UI()->MouseY(), 120, 150, this, PopupPoint);
				UI()->SetActiveItem(0);
			}
		}
		else
		{
			if(!UI()->MouseButton(0))
			{
				if(!s_Moved)
				{
					if(Input()->KeyIsPressed(KEY_LSHIFT) || Input()->KeyIsPressed(KEY_RSHIFT))
						m_SelectedPoints ^= 1 << V;
					else
						m_SelectedPoints = 1 << V;
				}
				m_LockMouse = false;
				UI()->SetActiveItem(0);
			}
		}

		pointColor = HexToRgba(Config()->m_EdColorQuadPointActive);
	}
	else if(UI()->HotItem() == pID)
	{
		ms_pUiGotContext = pID;

		pointColor = HexToRgba(Config()->m_EdColorQuadPointHover);
		m_pTooltip = "Left mouse button to move. Hold Shift to move the texture. Hold Alt to ignore grid.";

		if(UI()->MouseButton(0))
		{
			UI()->SetActiveItem(pID);
			s_Moved = false;
			if(Input()->KeyIsPressed(KEY_LSHIFT) || Input()->KeyIsPressed(KEY_RSHIFT))
			{
				s_Operation = OP_MOVEUV;
				m_LockMouse = true;
			}
			else
				s_Operation = OP_MOVEPOINT;

			if(!(m_SelectedPoints & (1 << V)))
			{
				if(Input()->KeyIsPressed(KEY_LSHIFT) || Input()->KeyIsPressed(KEY_RSHIFT))
					m_SelectedPoints |= 1 << V;
				else
					m_SelectedPoints = 1 << V;
				s_Moved = true;
			}

			m_SelectedQuad = QuadIndex;
		}
		else if(UI()->MouseButton(1))
		{
			s_Operation = OP_CONTEXT_MENU;
			m_SelectedQuad = QuadIndex;
			UI()->SetActiveItem(pID);
			if(!(m_SelectedPoints & (1 << V)))
			{
				if(Input()->KeyIsPressed(KEY_LSHIFT) || Input()->KeyIsPressed(KEY_RSHIFT))
					m_SelectedPoints |= 1 << V;
				else
					m_SelectedPoints = 1 << V;
				s_Moved = true;
			}
		}
	}
	else
		pointColor = HexToRgba(Config()->m_EdColorQuadPoint);

	Graphics()->SetColor(pointColor.r, pointColor.g, pointColor.b, pointColor.a);
	IGraphics::CQuadItem QuadItem(px, py, 5.0f * m_WorldZoom, 5.0f * m_WorldZoom);
	Graphics()->QuadsDraw(&QuadItem, 1);
}

void CEditor::DoQuadEnvelopes(const array<CQuad> &lQuads, IGraphics::CTextureHandle Texture)
{
	int Num = lQuads.size();
	CEnvelope **apEnvelope = new CEnvelope *[Num];
	for(int i = 0; i < Num; i++)
	{
		apEnvelope[i] = 0;
		if((m_ShowEnvelopePreview == SHOWENV_SELECTED && lQuads[i].m_PosEnv == m_SelectedEnvelope) || m_ShowEnvelopePreview == SHOWENV_ALL)
			if(lQuads[i].m_PosEnv >= 0 && lQuads[i].m_PosEnv < m_Map.m_lEnvelopes.size())
				apEnvelope[i] = m_Map.m_lEnvelopes[lQuads[i].m_PosEnv];
	}

	// Draw Lines
	Graphics()->TextureClear();
	Graphics()->LinesBegin();
	Graphics()->SetColor(80.0f / 255, 150.0f / 255, 230.f / 255, 0.5f);
	for(int j = 0; j < Num; j++)
	{
		if(!apEnvelope[j])
			continue;

		// QuadParams
		const CPoint *pPoints = lQuads[j].m_aPoints;
		for(int i = 0; i < apEnvelope[j]->m_lPoints.size() - 1; i++)
		{
			float aResults[4];
			vec2 Pos0 = vec2(0, 0);

			apEnvelope[j]->Eval(apEnvelope[j]->m_lPoints[i].m_Time / 1000.0f + 0.000001f, aResults);
			Pos0 = vec2(fx2f(pPoints[4].x) + aResults[0], fx2f(pPoints[4].y) + aResults[1]);

			const int Steps = 15;
			for(int n = 1; n <= Steps; n++)
			{
				float a = n / (float) Steps;

				// little offset to prevent looping due to fmod
				float time = mix(apEnvelope[j]->m_lPoints[i].m_Time, apEnvelope[j]->m_lPoints[i + 1].m_Time, a);
				apEnvelope[j]->Eval(time / 1000.0f - 0.000001f, aResults);

				vec2 Pos1 = vec2(fx2f(pPoints[4].x) + aResults[0], fx2f(pPoints[4].y) + aResults[1]);

				IGraphics::CLineItem Line = IGraphics::CLineItem(Pos0.x, Pos0.y, Pos1.x, Pos1.y);
				Graphics()->LinesDraw(&Line, 1);

				Pos0 = Pos1;
			}
		}
	}
	Graphics()->SetColor(1.0f, 1.0f, 1.0f, 1.0f);
	Graphics()->LinesEnd();

	// Draw Quads
	Graphics()->TextureSet(Texture);
	Graphics()->QuadsBegin();

	for(int j = 0; j < Num; j++)
	{
		if(!apEnvelope[j])
			continue;

		// QuadParams
		const CPoint *pPoints = lQuads[j].m_aPoints;

		for(int i = 0; i < apEnvelope[j]->m_lPoints.size(); i++)
		{
			// Calc Env Position
			float OffsetX = fx2f(apEnvelope[j]->m_lPoints[i].m_aValues[0]);
			float OffsetY = fx2f(apEnvelope[j]->m_lPoints[i].m_aValues[1]);
			float Rot = fx2f(apEnvelope[j]->m_lPoints[i].m_aValues[2]) / 360.0f * pi * 2;

			// Set Colors
			float Alpha = (m_SelectedQuadEnvelope == lQuads[j].m_PosEnv && m_SelectedEnvelopePoint == i) ? 0.65f : 0.35f;
			IGraphics::CColorVertex aArray[4] = {
				IGraphics::CColorVertex(0, lQuads[j].m_aColors[0].r, lQuads[j].m_aColors[0].g, lQuads[j].m_aColors[0].b, Alpha),
				IGraphics::CColorVertex(1, lQuads[j].m_aColors[1].r, lQuads[j].m_aColors[1].g, lQuads[j].m_aColors[1].b, Alpha),
				IGraphics::CColorVertex(2, lQuads[j].m_aColors[2].r, lQuads[j].m_aColors[2].g, lQuads[j].m_aColors[2].b, Alpha),
				IGraphics::CColorVertex(3, lQuads[j].m_aColors[3].r, lQuads[j].m_aColors[3].g, lQuads[j].m_aColors[3].b, Alpha)};
			Graphics()->SetColorVertex(aArray, 4);

			// Rotation
			if(Rot != 0)
			{
				static CPoint aRotated[4];
				aRotated[0] = lQuads[j].m_aPoints[0];
				aRotated[1] = lQuads[j].m_aPoints[1];
				aRotated[2] = lQuads[j].m_aPoints[2];
				aRotated[3] = lQuads[j].m_aPoints[3];
				pPoints = aRotated;

				Rotate(&lQuads[j].m_aPoints[4], &aRotated[0], Rot);
				Rotate(&lQuads[j].m_aPoints[4], &aRotated[1], Rot);
				Rotate(&lQuads[j].m_aPoints[4], &aRotated[2], Rot);
				Rotate(&lQuads[j].m_aPoints[4], &aRotated[3], Rot);
			}

			// Set Texture Coords
			Graphics()->QuadsSetSubsetFree(
				fx2f(lQuads[j].m_aTexcoords[0].x), fx2f(lQuads[j].m_aTexcoords[0].y),
				fx2f(lQuads[j].m_aTexcoords[1].x), fx2f(lQuads[j].m_aTexcoords[1].y),
				fx2f(lQuads[j].m_aTexcoords[2].x), fx2f(lQuads[j].m_aTexcoords[2].y),
				fx2f(lQuads[j].m_aTexcoords[3].x), fx2f(lQuads[j].m_aTexcoords[3].y));

			// Set Quad Coords & Draw
			IGraphics::CFreeformItem Freeform(
				fx2f(pPoints[0].x) + OffsetX, fx2f(pPoints[0].y) + OffsetY,
				fx2f(pPoints[1].x) + OffsetX, fx2f(pPoints[1].y) + OffsetY,
				fx2f(pPoints[2].x) + OffsetX, fx2f(pPoints[2].y) + OffsetY,
				fx2f(pPoints[3].x) + OffsetX, fx2f(pPoints[3].y) + OffsetY);
			Graphics()->QuadsDrawFreeform(&Freeform, 1);
		}
	}
	Graphics()->QuadsEnd();
	Graphics()->TextureClear();
	Graphics()->QuadsBegin();

	// Draw QuadPoints
	for(int j = 0; j < Num; j++)
	{
		if(!apEnvelope[j])
			continue;

		for(int i = 0; i < apEnvelope[j]->m_lPoints.size(); i++)
			DoQuadEnvPoint(&lQuads[j], j, i);
	}
	Graphics()->QuadsEnd();
	delete[] apEnvelope;
}

void CEditor::DoQuadEnvPoint(const CQuad *pQuad, int QIndex, int PIndex)
{
	enum
	{
		OP_NONE = 0,
		OP_MOVE,
		OP_ROTATE,
	};

	// some basic values
	static float s_LastWx;
	static float s_LastWy;
	static int s_Operation = OP_NONE;
	float wx = UI()->MouseWorldX();
	float wy = UI()->MouseWorldY();
	CEnvelope *pEnvelope = m_Map.m_lEnvelopes[pQuad->m_PosEnv];
	void *pID = &pEnvelope->m_lPoints[PIndex];
	static int s_ActQIndex = -1;

	// get pivot
	float CenterX = fx2f(pQuad->m_aPoints[4].x) + fx2f(pEnvelope->m_lPoints[PIndex].m_aValues[0]);
	float CenterY = fx2f(pQuad->m_aPoints[4].y) + fx2f(pEnvelope->m_lPoints[PIndex].m_aValues[1]);

	float dx = (CenterX - wx) / m_WorldZoom;
	float dy = (CenterY - wy) / m_WorldZoom;
	if(dx * dx + dy * dy < 50.0f && UI()->CheckActiveItem(0))
	{
		UI()->SetHotItem(pID);
		s_ActQIndex = QIndex;
	}

	bool IgnoreGrid;
	if(Input()->KeyIsPressed(KEY_LALT) || Input()->KeyIsPressed(KEY_RALT))
		IgnoreGrid = true;
	else
		IgnoreGrid = false;

	if(UI()->CheckActiveItem(pID) && s_ActQIndex == QIndex)
	{
		if(s_Operation == OP_MOVE)
		{
			if(m_GridActive && !IgnoreGrid)
			{
				int LineDistance = GetLineDistance();

				float x = 0.0f;
				float y = 0.0f;
				if(wx >= 0)
					x = (int) ((wx + (LineDistance / 2) * m_GridFactor) / (LineDistance * m_GridFactor)) * (LineDistance * m_GridFactor);
				else
					x = (int) ((wx - (LineDistance / 2) * m_GridFactor) / (LineDistance * m_GridFactor)) * (LineDistance * m_GridFactor);
				if(wy >= 0)
					y = (int) ((wy + (LineDistance / 2) * m_GridFactor) / (LineDistance * m_GridFactor)) * (LineDistance * m_GridFactor);
				else
					y = (int) ((wy - (LineDistance / 2) * m_GridFactor) / (LineDistance * m_GridFactor)) * (LineDistance * m_GridFactor);

				pEnvelope->m_lPoints[PIndex].m_aValues[0] = f2fx(x);
				pEnvelope->m_lPoints[PIndex].m_aValues[1] = f2fx(y);
			}
			else
			{
				pEnvelope->m_lPoints[PIndex].m_aValues[0] += f2fx(wx - s_LastWx);
				pEnvelope->m_lPoints[PIndex].m_aValues[1] += f2fx(wy - s_LastWy);
			}
		}
		else if(s_Operation == OP_ROTATE)
			pEnvelope->m_lPoints[PIndex].m_aValues[2] += 10 * m_MouseDeltaX;

		s_LastWx = wx;
		s_LastWy = wy;

		if(!UI()->MouseButton(0))
		{
			m_LockMouse = false;
			s_Operation = OP_NONE;
			UI()->SetActiveItem(0);
		}

		Graphics()->SetColor(1.0f, 1.0f, 1.0f, 1.0f);
	}
	else if(UI()->HotItem() == pID && s_ActQIndex == QIndex)
	{
		ms_pUiGotContext = pID;

		Graphics()->SetColor(1.0f, 1.0f, 1.0f, 1.0f);
		m_pTooltip = "Left mouse button to move. Hold Ctrl to rotate. Hold Alt to ignore grid.";

		if(UI()->MouseButton(0))
		{
			if(Input()->KeyIsPressed(KEY_LCTRL) || Input()->KeyIsPressed(KEY_RCTRL))
			{
				m_LockMouse = true;
				s_Operation = OP_ROTATE;
			}
			else
				s_Operation = OP_MOVE;

			m_SelectedEnvelopePoint = PIndex;
			m_SelectedQuadEnvelope = pQuad->m_PosEnv;

			UI()->SetActiveItem(pID);
			if(m_SelectedQuad != QIndex)
				m_SelectedPoints = 0;
			m_SelectedQuad = QIndex;
			s_LastWx = wx;
			s_LastWy = wy;
		}
		else
		{
			m_SelectedEnvelopePoint = -1;
			m_SelectedQuadEnvelope = -1;
		}
	}
	else
		Graphics()->SetColor(0.0f, 1.0f, 0.0f, 1.0f);

	IGraphics::CQuadItem QuadItem(CenterX, CenterY, 5.0f * m_WorldZoom, 5.0f * m_WorldZoom);
	Graphics()->QuadsDraw(&QuadItem, 1);
}

void CEditor::DoMapEditor(CUIRect View)
{
	// render all good stuff
	if(!m_ShowTilePicker)
	{
		for(int g = 0; g < m_Map.m_lGroups.size(); g++)
		{
			if(m_Map.m_lGroups[g]->m_Visible)
				m_Map.m_lGroups[g]->Render();
		}

		// render the game above everything else
		if(m_Map.m_pGameGroup->m_Visible && m_Map.m_pGameLayer->m_Visible)
		{
			m_Map.m_pGameGroup->MapScreen();
			m_Map.m_pGameLayer->Render();
		}

		CLayerTiles *pT = static_cast<CLayerTiles *>(GetSelectedLayerType(0, LAYERTYPE_TILES));
		if(m_ShowTileInfo && pT && pT->m_Visible && m_ZoomLevel <= 300)
		{
			GetSelectedGroup()->MapScreen();
			pT->ShowInfo();
		}
	}
	else
	{
		// fix aspect ratio of the image in the picker
		float Max = minimum(View.w, View.h);
		View.w = View.h = Max;
	}

	static void *s_pEditorID = (void *) &s_pEditorID;
	bool Inside = UI()->MouseInside(&View);

	// fetch mouse position
	float wx = UI()->MouseWorldX();
	float wy = UI()->MouseWorldY();
	float mx = UI()->MouseX();
	float my = UI()->MouseY();

	static float s_StartWx = 0;
	static float s_StartWy = 0;

	enum
	{
		OP_NONE = 0,
		OP_BRUSH_GRAB,
		OP_BRUSH_DRAW,
		OP_BRUSH_PAINT,
		OP_PAN_WORLD,
		OP_PAN_EDITOR,
		OP_PIPETTE,
	};

	static int s_Operation = OP_NONE;

	if(m_ShowTilePicker)
	{
		// remap the screen so it can display the whole tileset
		CUIRect Screen = *UI()->Screen();
		float Size = 32.0 * 16.0f;
		float w = Size * (Screen.w / View.w);
		float h = Size * (Screen.h / View.h);
		float x = -(View.x / Screen.w) * w;
		float y = -(View.y / Screen.h) * h;
		wx = x + w * mx / Screen.w;
		wy = y + h * my / Screen.h;
		CLayerTiles *t = (CLayerTiles *) GetSelectedLayerType(0, LAYERTYPE_TILES);
		if(t)
		{
			Graphics()->MapScreen(x, y, x + w, y + h);
			m_TilesetPicker.m_Image = t->m_Image;
			m_TilesetPicker.m_Texture = t->m_Texture;
			m_TilesetPicker.m_Game = t->m_Game;
			m_TilesetPicker.Render();
			if(m_ShowTileInfo)
				m_TilesetPicker.ShowInfo();
		}
		else
		{
			CLayerQuads *t = (CLayerQuads *) GetSelectedLayerType(0, LAYERTYPE_QUADS);
			if(t)
			{
				m_QuadsetPicker.m_Image = t->m_Image;
				m_QuadsetPicker.m_lQuads[0].m_aPoints[0].x = (int) View.x << 10;
				m_QuadsetPicker.m_lQuads[0].m_aPoints[0].y = (int) View.y << 10;
				m_QuadsetPicker.m_lQuads[0].m_aPoints[1].x = (int) (View.x + View.w) << 10;
				m_QuadsetPicker.m_lQuads[0].m_aPoints[1].y = (int) View.y << 10;
				m_QuadsetPicker.m_lQuads[0].m_aPoints[2].x = (int) View.x << 10;
				m_QuadsetPicker.m_lQuads[0].m_aPoints[2].y = (int) (View.y + View.h) << 10;
				m_QuadsetPicker.m_lQuads[0].m_aPoints[3].x = (int) (View.x + View.w) << 10;
				m_QuadsetPicker.m_lQuads[0].m_aPoints[3].y = (int) (View.y + View.h) << 10;
				m_QuadsetPicker.m_lQuads[0].m_aPoints[4].x = (int) (View.x + View.w / 2) << 10;
				m_QuadsetPicker.m_lQuads[0].m_aPoints[4].y = (int) (View.y + View.h / 2) << 10;
				m_QuadsetPicker.Render();
			}
		}
	}

	// draw layer borders
	CLayer *pEditLayers[16];
	int NumEditLayers = 0;

	if(m_ShowTilePicker && GetSelectedLayer(0) && GetSelectedLayer(0)->m_Type == LAYERTYPE_TILES)
	{
		pEditLayers[0] = &m_TilesetPicker;
		NumEditLayers++;
	}
	else if(m_ShowTilePicker)
	{
		pEditLayers[0] = &m_QuadsetPicker;
		NumEditLayers++;
	}
	else
	{
		pEditLayers[0] = GetSelectedLayer(0);
		if(pEditLayers[0])
			NumEditLayers++;

		CLayerGroup *g = GetSelectedGroup();
		if(g)
		{
			g->MapScreen();

			RenderGrid(g);

			for(int i = 0; i < NumEditLayers; i++)
			{
				if(pEditLayers[i]->m_Type != LAYERTYPE_TILES)
					continue;

				float w, h;
				pEditLayers[i]->GetSize(&w, &h);

				IGraphics::CLineItem Array[4] = {
					IGraphics::CLineItem(0, 0, w, 0),
					IGraphics::CLineItem(w, 0, w, h),
					IGraphics::CLineItem(w, h, 0, h),
					IGraphics::CLineItem(0, h, 0, 0)};
				Graphics()->TextureClear();
				Graphics()->LinesBegin();
				Graphics()->LinesDraw(Array, 4);
				Graphics()->LinesEnd();
			}
		}
	}

	if(Inside)
	{
		UI()->SetHotItem(s_pEditorID);

		// do global operations like pan and zoom
		if(UI()->CheckActiveItem(0) && (UI()->MouseButton(0) || UI()->MouseButton(2)))
		{
			s_StartWx = wx;
			s_StartWy = wy;

			if(Input()->KeyIsPressed(KEY_LCTRL) || Input()->KeyIsPressed(KEY_RCTRL) || UI()->MouseButton(2))
			{
				if(Input()->KeyIsPressed(KEY_LSHIFT))
					s_Operation = OP_PAN_EDITOR;
				else
					s_Operation = OP_PAN_WORLD;
				UI()->SetActiveItem(s_pEditorID);
			}
		}

		switch(m_MouseEdMode)
		{
		case MOUSE_EDIT:
		{
			if(UI()->HotItem() == s_pEditorID)
			{
				// brush editing
				if(m_Brush.IsEmpty())
					m_pTooltip = "Use left mouse button to drag and create a brush.";
				else
					m_pTooltip = "Use left mouse button to paint with the brush. Right button clears the brush.";

				if(UI()->CheckActiveItem(s_pEditorID))
				{
					CUIRect r;
					r.x = s_StartWx;
					r.y = s_StartWy;
					r.w = wx - s_StartWx;
					r.h = wy - s_StartWy;
					if(r.w < 0)
					{
						r.x += r.w;
						r.w = -r.w;
					}

					if(r.h < 0)
					{
						r.y += r.h;
						r.h = -r.h;
					}

					if(s_Operation == OP_BRUSH_DRAW)
					{
						if(!m_Brush.IsEmpty())
						{
							// draw with brush
							for(int k = 0; k < NumEditLayers; k++)
							{
								if(pEditLayers[k]->m_Type == m_Brush.m_lLayers[0]->m_Type)
									pEditLayers[k]->BrushDraw(m_Brush.m_lLayers[0], wx, wy);
							}
						}
					}
					else if(s_Operation == OP_BRUSH_GRAB)
					{
						if(!UI()->MouseButton(0))
						{
							// grab brush
							char aBuf[256];
							str_format(aBuf, sizeof(aBuf), "grabbing %f %f %f %f", r.x, r.y, r.w, r.h);
							Console()->Print(IConsole::OUTPUT_LEVEL_DEBUG, "editor", aBuf);

							// TODO: do all layers
							int Grabs = 0;
							for(int k = 0; k < NumEditLayers; k++)
								Grabs += pEditLayers[k]->BrushGrab(&m_Brush, r);
							if(Grabs == 0)
								m_Brush.Clear();
						}
						else
						{
							for(int k = 0; k < NumEditLayers; k++)
								pEditLayers[k]->BrushSelecting(r);
							UI()->MapScreen();
						}
					}
					else if(s_Operation == OP_BRUSH_PAINT)
					{
						if(!UI()->MouseButton(0))
						{
							for(int k = 0; k < NumEditLayers; k++)
								pEditLayers[k]->FillSelection(m_Brush.IsEmpty(), m_Brush.m_lLayers[0], r);
						}
						else
						{
							for(int k = 0; k < NumEditLayers; k++)
								pEditLayers[k]->BrushSelecting(r);
							UI()->MapScreen();
						}
					}
				}
				else
				{
					if(UI()->MouseButton(1))
						m_Brush.Clear();

					if(UI()->MouseButton(0) && s_Operation == OP_NONE)
					{
						UI()->SetActiveItem(s_pEditorID);

						if(m_Brush.IsEmpty())
							s_Operation = OP_BRUSH_GRAB;
						else
						{
							s_Operation = OP_BRUSH_DRAW;
							for(int k = 0; k < NumEditLayers; k++)
							{
								if(pEditLayers[k]->m_Type == m_Brush.m_lLayers[0]->m_Type)
									pEditLayers[k]->BrushPlace(m_Brush.m_lLayers[0], wx, wy);
							}
						}

						CLayerTiles *pLayer = (CLayerTiles *) GetSelectedLayerType(0, LAYERTYPE_TILES);
						if((Input()->KeyIsPressed(KEY_LSHIFT) || Input()->KeyIsPressed(KEY_RSHIFT)) && pLayer)
							s_Operation = OP_BRUSH_PAINT;
					}

					if(!m_Brush.IsEmpty())
					{
						m_Brush.m_OffsetX = -(int) wx;
						m_Brush.m_OffsetY = -(int) wy;
						for(int i = 0; i < m_Brush.m_lLayers.size(); i++)
						{
							if(m_Brush.m_lLayers[i]->m_Type == LAYERTYPE_TILES)
							{
								m_Brush.m_OffsetX = -(int) (wx / 32.0f) * 32;
								m_Brush.m_OffsetY = -(int) (wy / 32.0f) * 32;
								break;
							}
						}

						CLayerGroup *pGroup = GetSelectedGroup();
						if(pGroup && !m_ShowTilePicker)
						{
							m_Brush.m_OffsetX += pGroup->m_OffsetX;
							m_Brush.m_OffsetY += pGroup->m_OffsetY;
							m_Brush.m_ParallaxX = pGroup->m_ParallaxX;
							m_Brush.m_ParallaxY = pGroup->m_ParallaxY;
							m_Brush.Render();
							float w, h;
							m_Brush.GetSize(&w, &h);

							IGraphics::CLineItem Array[4] = {
								IGraphics::CLineItem(0, 0, w, 0),
								IGraphics::CLineItem(w, 0, w, h),
								IGraphics::CLineItem(w, h, 0, h),
								IGraphics::CLineItem(0, h, 0, 0)};
							Graphics()->TextureClear();
							Graphics()->LinesBegin();
							Graphics()->LinesDraw(Array, 4);
							Graphics()->LinesEnd();
						}
					}
				}
			}

			// quad editing
			{
				if(!m_ShowTilePicker && m_Brush.IsEmpty())
				{
					// fetch layers
					CLayerGroup *g = GetSelectedGroup();
					if(g)
						g->MapScreen();

					// adjust z-index
					{
						CLayerQuads *pQuadLayer = (CLayerQuads *) GetSelectedLayerType(0, LAYERTYPE_QUADS);
						if(pQuadLayer && (m_SelectedQuad >= 0 && m_SelectedQuad < pQuadLayer->m_lQuads.size()))
						{
							if(Input()->KeyPress(KEY_PAGEUP))
							{
								// move up
								if(m_SelectedQuad < pQuadLayer->m_lQuads.size() - 1)
								{
									std::swap(pQuadLayer->m_lQuads[m_SelectedQuad], pQuadLayer->m_lQuads[m_SelectedQuad + 1]);
									m_SelectedQuad++;
								}
							}
							else if(Input()->KeyPress(KEY_PAGEDOWN))
							{
								// move down
								if(m_SelectedQuad > 0)
								{
									std::swap(pQuadLayer->m_lQuads[m_SelectedQuad], pQuadLayer->m_lQuads[m_SelectedQuad - 1]);
									m_SelectedQuad--;
								}
							}
							else if(Input()->KeyPress(KEY_HOME))
							{
								// move to front
								int NumQuads = pQuadLayer->m_lQuads.size();
								while(m_SelectedQuad < NumQuads - 1)
								{
									std::swap(pQuadLayer->m_lQuads[m_SelectedQuad], pQuadLayer->m_lQuads[m_SelectedQuad + 1]);
									m_SelectedQuad++;
								}
							}
							else if(Input()->KeyPress(KEY_END))
							{
								// move to back
								while(m_SelectedQuad > 0)
								{
									std::swap(pQuadLayer->m_lQuads[m_SelectedQuad], pQuadLayer->m_lQuads[m_SelectedQuad - 1]);
									m_SelectedQuad--;
								}
							}
						}
					}

					for(int k = 0; k < NumEditLayers; k++)
					{
						if(pEditLayers[k]->m_Type == LAYERTYPE_QUADS)
						{
							CLayerQuads *pLayer = (CLayerQuads *) pEditLayers[k];

							if(m_ShowEnvelopePreview == SHOWENV_NONE)
								m_ShowEnvelopePreview = SHOWENV_ALL;

							Graphics()->TextureClear();
							Graphics()->QuadsBegin();
							for(int i = 0; i < pLayer->m_lQuads.size(); i++)
							{
								for(int v = 0; v < 4; v++)
									DoQuadPoint(&pLayer->m_lQuads[i], i, v);

								DoQuad(&pLayer->m_lQuads[i], i);
							}
							Graphics()->QuadsEnd();
						}
					}

					UI()->MapScreen();
				}
			}
		}
		break;

		case MOUSE_PIPETTE:
		{
			if(UI()->HotItem() == s_pEditorID)
			{
				m_pTooltip = "Use left mouse button to pick a color from screen.";

				if(UI()->CheckActiveItem(s_pEditorID))
				{
					if(s_Operation == OP_PIPETTE)
					{
						const int Width = Graphics()->ScreenWidth();
						const int Height = Graphics()->ScreenHeight();

						const int px = clamp(0, int(mx * Width / UI()->Screen()->w), Width);
						const int py = clamp(0, int(my * Height / UI()->Screen()->h), Height);

						unsigned char *pPixelData = 0x0;
						Graphics()->ReadBackbuffer(&pPixelData, px, py, 1, 1); // get pixel at location (px, py)

						m_SelectedColor = vec4(pPixelData[0] / 255.0f, pPixelData[1] / 255.0f, pPixelData[2] / 255.0f, 1.0f);

						mem_free(pPixelData);
					}
				}
				else
				{
					if(UI()->MouseButton(0))
					{
						UI()->SetActiveItem(s_pEditorID);
						s_Operation = OP_PIPETTE;
					}
				}
			}

			// leave pipette mode on right-click
			if(UI()->MouseButton(1))
				m_MouseEdMode = MOUSE_EDIT;
		}
		break;
		}

		// do panning
		if(UI()->CheckActiveItem(s_pEditorID))
		{
			if(s_Operation == OP_PAN_WORLD)
			{
				m_WorldOffsetX -= m_MouseDeltaX * m_WorldZoom;
				m_WorldOffsetY -= m_MouseDeltaY * m_WorldZoom;
			}
			else if(s_Operation == OP_PAN_EDITOR)
			{
				m_EditorOffsetX -= m_MouseDeltaX * m_WorldZoom;
				m_EditorOffsetY -= m_MouseDeltaY * m_WorldZoom;
			}

			// release mouse
			if(!UI()->MouseButton(0))
			{
				s_Operation = OP_NONE;
				UI()->SetActiveItem(0);
			}
		}
	}
	else if(UI()->CheckActiveItem(s_pEditorID))
	{
		// release mouse
		if(!UI()->MouseButton(0))
		{
			s_Operation = OP_NONE;
			UI()->SetActiveItem(0);
		}
	}

	if(!m_ShowTilePicker && GetSelectedGroup() && GetSelectedGroup()->m_UseClipping)
	{
		CLayerGroup *g = m_Map.m_pGameGroup;
		g->MapScreen();

		Graphics()->TextureClear();
		Graphics()->LinesBegin();

		CUIRect r;
		r.x = GetSelectedGroup()->m_ClipX;
		r.y = GetSelectedGroup()->m_ClipY;
		r.w = GetSelectedGroup()->m_ClipW;
		r.h = GetSelectedGroup()->m_ClipH;

		IGraphics::CLineItem Array[4] = {
			IGraphics::CLineItem(r.x, r.y, r.x + r.w, r.y),
			IGraphics::CLineItem(r.x + r.w, r.y, r.x + r.w, r.y + r.h),
			IGraphics::CLineItem(r.x + r.w, r.y + r.h, r.x, r.y + r.h),
			IGraphics::CLineItem(r.x, r.y + r.h, r.x, r.y)};
		Graphics()->SetColor(1, 0, 0, 1);
		Graphics()->LinesDraw(Array, 4);

		Graphics()->LinesEnd();
	}

	// render screen sizes
	if(!m_ShowTilePicker && m_ProofBorders)
	{
		CLayerGroup *g = m_Map.m_pGameGroup;
		g->MapScreen();

		Graphics()->TextureClear();
		Graphics()->LinesBegin();

		float aLastPoints[4];
		float Start = 1.0f; // 9.0f/16.0f;
		float End = 16.0f / 9.0f;
		const int NumSteps = 20;
		for(int i = 0; i <= NumSteps; i++)
		{
			float aPoints[4];
			float Aspect = Start + (End - Start) * (i / (float) NumSteps);

			RenderTools()->MapScreenToWorld(
				m_WorldOffsetX, m_WorldOffsetY,
				1.0f, 1.0f, 0.0f, 0.0f, Aspect, 1.0f, aPoints);

			if(i == 0)
			{
				IGraphics::CLineItem Array[2] = {
					IGraphics::CLineItem(aPoints[0], aPoints[1], aPoints[2], aPoints[1]),
					IGraphics::CLineItem(aPoints[0], aPoints[3], aPoints[2], aPoints[3])};
				Graphics()->LinesDraw(Array, 2);
			}

			if(i != 0)
			{
				IGraphics::CLineItem Array[4] = {
					IGraphics::CLineItem(aPoints[0], aPoints[1], aLastPoints[0], aLastPoints[1]),
					IGraphics::CLineItem(aPoints[2], aPoints[1], aLastPoints[2], aLastPoints[1]),
					IGraphics::CLineItem(aPoints[0], aPoints[3], aLastPoints[0], aLastPoints[3]),
					IGraphics::CLineItem(aPoints[2], aPoints[3], aLastPoints[2], aLastPoints[3])};
				Graphics()->LinesDraw(Array, 4);
			}

			if(i == NumSteps)
			{
				IGraphics::CLineItem Array[2] = {
					IGraphics::CLineItem(aPoints[0], aPoints[1], aPoints[0], aPoints[3]),
					IGraphics::CLineItem(aPoints[2], aPoints[1], aPoints[2], aPoints[3])};
				Graphics()->LinesDraw(Array, 2);
			}

			mem_copy(aLastPoints, aPoints, sizeof(aPoints));
		}

		if(1)
		{
			Graphics()->SetColor(1, 0, 0, 1);
			for(int i = 0; i < 2; i++)
			{
				float aPoints[4];
				float aAspects[] = {4.0f / 3.0f, 16.0f / 10.0f, 5.0f / 4.0f, 16.0f / 9.0f};
				float Aspect = aAspects[i];

				RenderTools()->MapScreenToWorld(
					m_WorldOffsetX, m_WorldOffsetY,
					1.0f, 1.0f, 0.0f, 0.0f, Aspect, 1.0f, aPoints);

				CUIRect r;
				r.x = aPoints[0];
				r.y = aPoints[1];
				r.w = aPoints[2] - aPoints[0];
				r.h = aPoints[3] - aPoints[1];

				IGraphics::CLineItem Array[4] = {
					IGraphics::CLineItem(r.x, r.y, r.x + r.w, r.y),
					IGraphics::CLineItem(r.x + r.w, r.y, r.x + r.w, r.y + r.h),
					IGraphics::CLineItem(r.x + r.w, r.y + r.h, r.x, r.y + r.h),
					IGraphics::CLineItem(r.x, r.y + r.h, r.x, r.y)};
				Graphics()->LinesDraw(Array, 4);
				Graphics()->SetColor(0, 1, 0, 1);
			}
		}

		Graphics()->LinesEnd();
	}

	if(!m_ShowTilePicker && m_ShowTileInfo && m_ShowEnvelopePreview != SHOWENV_NONE && GetSelectedLayer(0) && GetSelectedLayer(0)->m_Type == LAYERTYPE_QUADS)
	{
		GetSelectedGroup()->MapScreen();

		CLayerQuads *pLayer = (CLayerQuads *) GetSelectedLayer(0);
		IGraphics::CTextureHandle Texture;
		if(pLayer->m_Image >= 0 && pLayer->m_Image < m_Map.m_lImages.size())
			Texture = m_Map.m_lImages[pLayer->m_Image]->m_Texture;

		DoQuadEnvelopes(pLayer->m_lQuads, Texture);
		m_ShowEnvelopePreview = SHOWENV_NONE;
	}

	UI()->MapScreen();
}

int CEditor::DoProperties(CUIRect *pToolBox, CProperty *pProps, int *pIDs, int *pNewVal)
{
	int Change = -1;

	for(int i = 0; pProps[i].m_pName; i++)
	{
		CUIRect Slot;
		pToolBox->HSplitTop(13.0f, &Slot, pToolBox);
		CUIRect Label, Shifter;
		Slot.VSplitMid(&Label, &Shifter);
		Shifter.HMargin(1.0f, &Shifter);
		UI()->DoLabel(&Label, pProps[i].m_pName, 10.0f, TEXTALIGN_LEFT);

		if(pProps[i].m_Type == PROPTYPE_INT_STEP)
		{
			CUIRect Inc, Dec;
			char aBuf[64];

			Shifter.VSplitRight(10.0f, &Shifter, &Inc);
			Shifter.VSplitLeft(10.0f, &Dec, &Shifter);
			str_format(aBuf, sizeof(aBuf), "%d", pProps[i].m_Value);
			Shifter.Draw(vec4(1, 1, 1, 0.5f), 0.0f, CUIRect::CORNER_NONE);
			UI()->DoLabel(&Shifter, aBuf, 10.0f, TEXTALIGN_CENTER);

			if(DoButton_ButtonDec(&pIDs[i], 0, 0, &Dec, 0, "Decrease"))
			{
				*pNewVal = pProps[i].m_Value - 1;
				Change = i;
			}
			if(DoButton_ButtonInc(((char *) &pIDs[i]) + 1, 0, 0, &Inc, 0, "Increase"))
			{
				*pNewVal = pProps[i].m_Value + 1;
				Change = i;
			}
		}
		else if(pProps[i].m_Type == PROPTYPE_BOOL)
		{
			CUIRect No, Yes;
			Shifter.VSplitMid(&No, &Yes);
			if(DoButton_ButtonDec(&pIDs[i], "No", !pProps[i].m_Value, &No, 0, ""))
			{
				*pNewVal = 0;
				Change = i;
			}
			if(DoButton_ButtonInc(((char *) &pIDs[i]) + 1, "Yes", pProps[i].m_Value, &Yes, 0, ""))
			{
				*pNewVal = 1;
				Change = i;
			}
		}
		else if(pProps[i].m_Type == PROPTYPE_INT_SCROLL)
		{
			int NewValue = UiDoValueSelector(&pIDs[i], &Shifter, "", pProps[i].m_Value, pProps[i].m_Min, pProps[i].m_Max, 1, 1.0f, "Use left mouse button to drag and change the value. Hold Shift for more precision.");
			if(NewValue != pProps[i].m_Value)
			{
				*pNewVal = NewValue;
				Change = i;
			}
		}
		else if(pProps[i].m_Type == PROPTYPE_COLOR)
		{
			static const char *s_paTexts[4] = {"R", "G", "B", "A"};
			static int s_aShift[] = {24, 16, 8, 0};
			int NewColor = 0;

			// extra space
			CUIRect ColorBox, ColorSlots;

			pToolBox->HSplitTop(3.0f * 13.0f, &Slot, pToolBox);
			Slot.VSplitMid(&ColorBox, &ColorSlots);
			ColorBox.HMargin(1.0f, &ColorBox);
			ColorBox.VMargin(6.0f, &ColorBox);

			for(int c = 0; c < 4; c++)
			{
				int v = (pProps[i].m_Value >> s_aShift[c]) & 0xff;
				NewColor |= UiDoValueSelector(((char *) &pIDs[i]) + c, &Shifter, s_paTexts[c], v, 0, 255, 1, 1.0f, "Use left mouse button to drag and change the color value. Hold Shift for more precision.") << s_aShift[c];

				if(c != 3)
				{
					ColorSlots.HSplitTop(13.0f, &Shifter, &ColorSlots);
					Shifter.HMargin(1.0f, &Shifter);
				}
			}

			// color picker
			vec4 Color = vec4(
				((pProps[i].m_Value >> s_aShift[0]) & 0xff) / 255.0f,
				((pProps[i].m_Value >> s_aShift[1]) & 0xff) / 255.0f,
				((pProps[i].m_Value >> s_aShift[2]) & 0xff) / 255.0f,
				1.0f);

			ColorBox.Draw(Color, 0.0f, CUIRect::CORNER_NONE);
			static int s_ColorPicker;
			if(DoButton_Editor_Common(&s_ColorPicker, 0x0, 0, &ColorBox, 0, 0x0))
			{
				m_InitialPickerColor = RgbToHsv(vec3(Color.r, Color.g, Color.b));
				m_SelectedPickerColor = m_InitialPickerColor;
				UI()->DoPopupMenu(UI()->MouseX(), UI()->MouseY(), 180, 180, this, PopupColorPicker);
			}

			if(m_InitialPickerColor != m_SelectedPickerColor)
			{
				m_InitialPickerColor = m_SelectedPickerColor;
				vec3 c = HsvToRgb(m_SelectedPickerColor);
				NewColor = ((int) (c.r * 255.0f) & 0xff) << 24 | ((int) (c.g * 255.0f) & 0xff) << 16 | ((int) (c.b * 255.0f) & 0xff) << 8 | (pProps[i].m_Value & 0xff);
			}

			if(NewColor != pProps[i].m_Value)
			{
				*pNewVal = NewColor;
				Change = i;
			}
		}
		else if(pProps[i].m_Type == PROPTYPE_IMAGE)
		{
			char aBuf[64];
			if(pProps[i].m_Value < 0)
				str_copy(aBuf, "None", sizeof(aBuf));
			else
				str_format(aBuf, sizeof(aBuf), "%s", m_Map.m_lImages[pProps[i].m_Value]->m_aName);

			if(DoButton_Editor(&pIDs[i], aBuf, 0, &Shifter, 0, 0))
				PopupSelectImageInvoke(pProps[i].m_Value, UI()->MouseX(), UI()->MouseY());

			int r = PopupSelectImageResult();
			if(r >= -1)
			{
				*pNewVal = r;
				Change = i;
			}
		}
		else if(pProps[i].m_Type == PROPTYPE_SHIFT)
		{
			CUIRect Left, Right, Up, Down;
			Shifter.VSplitMid(&Left, &Up, 2.0f);
			Left.VSplitLeft(10.0f, &Left, &Shifter);
			Shifter.VSplitRight(10.0f, &Shifter, &Right);
			Shifter.Draw(vec4(1, 1, 1, 0.5f), 0.0f, CUIRect::CORNER_NONE);
			UI()->DoLabel(&Shifter, "X", 10.0f, TEXTALIGN_CENTER);
			Up.VSplitLeft(10.0f, &Up, &Shifter);
			Shifter.VSplitRight(10.0f, &Shifter, &Down);
			Shifter.Draw(vec4(1, 1, 1, 0.5f), 0.0f, CUIRect::CORNER_NONE);
			UI()->DoLabel(&Shifter, "Y", 10.0f, TEXTALIGN_CENTER);
			if(DoButton_ButtonDec(&pIDs[i], "-", 0, &Left, 0, "Left"))
			{
				*pNewVal = 1;
				Change = i;
			}
			if(DoButton_ButtonInc(((char *) &pIDs[i]) + 3, "+", 0, &Right, 0, "Right"))
			{
				*pNewVal = 2;
				Change = i;
			}
			if(DoButton_ButtonDec(((char *) &pIDs[i]) + 1, "-", 0, &Up, 0, "Up"))
			{
				*pNewVal = 4;
				Change = i;
			}
			if(DoButton_ButtonInc(((char *) &pIDs[i]) + 2, "+", 0, &Down, 0, "Down"))
			{
				*pNewVal = 8;
				Change = i;
			}
		}
	}

	return Change;
}

void CEditor::RenderLayers(CUIRect LayersBox)
{
	const float RowHeight = 12.0f;
	char aBuf[64];

	static CScrollRegion s_ScrollRegion;
	vec2 ScrollOffset(0.0f, 0.0f);
	CScrollRegionParams ScrollParams;
	ScrollParams.m_ClipBgColor = vec4(0.0f, 0.0f, 0.0f, 0.0f);
	ScrollParams.m_ScrollbarBgColor = vec4(0.0f, 0.0f, 0.0f, 0.0f);
	ScrollParams.m_ScrollbarWidth = 10.0f;
	ScrollParams.m_ScrollbarMargin = 3.0f;
	ScrollParams.m_ScrollUnit = RowHeight * 5;
	s_ScrollRegion.Begin(&LayersBox, &ScrollOffset, &ScrollParams);
	LayersBox.y += ScrollOffset.y;

	// render layers
	for(int g = 0; g < m_Map.m_lGroups.size(); g++)
	{
		CUIRect Slot, VisibleToggle, SaveCheck;
		LayersBox.HSplitTop(RowHeight, &Slot, &LayersBox);
		s_ScrollRegion.AddRect(Slot);

		if(!s_ScrollRegion.IsRectClipped(Slot))
		{
			Slot.VSplitLeft(12.0f, &VisibleToggle, &Slot);
			if(DoButton_Ex(&m_Map.m_lGroups[g]->m_Visible, m_Map.m_lGroups[g]->m_Visible ? "V" : "H", m_Map.m_lGroups[g]->m_Collapse ? 1 : 0, &VisibleToggle, 0, "Toggle group visibility", CUIRect::CORNER_L))
				m_Map.m_lGroups[g]->m_Visible = !m_Map.m_lGroups[g]->m_Visible;

			Slot.VSplitRight(12.0f, &Slot, &SaveCheck);
			if(DoButton_Ex(&m_Map.m_lGroups[g]->m_SaveToMap, "S", m_Map.m_lGroups[g]->m_SaveToMap, &SaveCheck, 0, "Enable/disable group for saving", CUIRect::CORNER_R))
				if(!m_Map.m_lGroups[g]->m_GameGroup)
					m_Map.m_lGroups[g]->m_SaveToMap = !m_Map.m_lGroups[g]->m_SaveToMap;

			str_format(aBuf, sizeof(aBuf), "#%d %s", g, m_Map.m_lGroups[g]->m_aName);
			const float GroupFontSize = clamp(10.0f * Slot.w / TextRender()->TextWidth(10.0f, aBuf, -1), 6.0f, 10.0f);
			const char *pGroupTooltip = m_Map.m_lGroups[g]->m_Collapse ? "Select group. Double click to expand." : "Select group. Double click to collapse.";
			if(int Result = DoButton_Ex(&m_Map.m_lGroups[g], aBuf, g == m_SelectedGroup, &Slot, BUTTON_CONTEXT, pGroupTooltip, 0, GroupFontSize))
			{
				m_SelectedGroup = g;
				m_SelectedLayer = 0;

				if(Result == 2)
					UI()->DoPopupMenu(UI()->MouseX(), UI()->MouseY(), 145, 220, this, PopupGroup);

				if(m_Map.m_lGroups[g]->m_lLayers.size() && Input()->MouseDoubleClick())
					m_Map.m_lGroups[g]->m_Collapse ^= 1;
			}
		}

		LayersBox.HSplitTop(2.0f, &Slot, &LayersBox);
		s_ScrollRegion.AddRect(Slot);

		for(int l = 0; l < m_Map.m_lGroups[g]->m_lLayers.size(); l++)
		{
			if(m_Map.m_lGroups[g]->m_Collapse)
				continue;

			LayersBox.HSplitTop(RowHeight + 2.0f, &Slot, &LayersBox);
			s_ScrollRegion.AddRect(Slot);
			if(s_ScrollRegion.IsRectClipped(Slot))
				continue;
			Slot.HSplitTop(RowHeight, &Slot, 0);

			Slot.VSplitLeft(12.0f, 0, &Slot);
			Slot.VSplitLeft(12.0f, &VisibleToggle, &Slot);

			if(DoButton_Ex(&m_Map.m_lGroups[g]->m_lLayers[l]->m_Visible, m_Map.m_lGroups[g]->m_lLayers[l]->m_Visible ? "V" : "H", 0, &VisibleToggle, 0, "Toggle layer visibility", CUIRect::CORNER_L))
				m_Map.m_lGroups[g]->m_lLayers[l]->m_Visible = !m_Map.m_lGroups[g]->m_lLayers[l]->m_Visible;

			Slot.VSplitRight(12.0f, &Slot, &SaveCheck);
			if(DoButton_Ex(&m_Map.m_lGroups[g]->m_lLayers[l]->m_SaveToMap, "S", m_Map.m_lGroups[g]->m_lLayers[l]->m_SaveToMap, &SaveCheck, 0, "Enable/disable layer for saving", CUIRect::CORNER_R))
				if(m_Map.m_lGroups[g]->m_lLayers[l] != m_Map.m_pGameLayer)
					m_Map.m_lGroups[g]->m_lLayers[l]->m_SaveToMap = !m_Map.m_lGroups[g]->m_lLayers[l]->m_SaveToMap;

			const char *pGroupLabel;
			if(m_Map.m_lGroups[g]->m_lLayers[l]->m_aName[0])
				pGroupLabel = m_Map.m_lGroups[g]->m_lLayers[l]->m_aName;
			else if(m_Map.m_lGroups[g]->m_lLayers[l]->m_Type == LAYERTYPE_TILES)
				pGroupLabel = "Tiles";
			else
				pGroupLabel = "Quads";

			const float LayerFontSize = clamp(10.0f * Slot.w / TextRender()->TextWidth(10.0f, pGroupLabel, -1), 6.0f, 10.0f);
			if(int Result = DoButton_Ex(m_Map.m_lGroups[g]->m_lLayers[l], pGroupLabel, g == m_SelectedGroup && l == m_SelectedLayer, &Slot, BUTTON_CONTEXT, "Select layer.", 0, LayerFontSize))
			{
				m_SelectedLayer = l;
				m_SelectedGroup = g;
				if(Result == 2)
					UI()->DoPopupMenu(UI()->MouseX(), UI()->MouseY(), 120, 245, this, PopupLayer);
			}
		}

		LayersBox.HSplitTop(5.0f, &Slot, &LayersBox);
		s_ScrollRegion.AddRect(Slot);
	}

	CUIRect AddGroupButton;
	LayersBox.HSplitTop(RowHeight + 1.0f, &AddGroupButton, &LayersBox);
	s_ScrollRegion.AddRect(AddGroupButton);
	if(!s_ScrollRegion.IsRectClipped(AddGroupButton))
	{
		AddGroupButton.HSplitTop(RowHeight, &AddGroupButton, 0);
		static int s_AddGroupButton = 0;
		if(DoButton_Editor(&s_AddGroupButton, "Add group", 0, &AddGroupButton, 0, "Adds a new group"))
		{
			m_Map.NewGroup();
			m_SelectedGroup = m_Map.m_lGroups.size() - 1;
		}
	}

	s_ScrollRegion.End();
}

void CEditor::ReplaceImage(const char *pFileName, int StorageType, void *pUser)
{
	CEditor *pEditor = (CEditor *) pUser;
	CEditorImage ImgInfo(pEditor);
	if(!pEditor->Graphics()->LoadPNG(&ImgInfo, pFileName, StorageType))
		return;

	CEditorImage *pImg = pEditor->m_Map.m_lImages[pEditor->m_SelectedImage];
	int External = pImg->m_External;
	pEditor->Graphics()->UnloadTexture(&(pImg->m_Texture));
	if(pImg->m_pData)
	{
		mem_free(pImg->m_pData);
		pImg->m_pData = 0;
	}
	if(pImg->m_pAutoMapper)
	{
		delete pImg->m_pAutoMapper;
		pImg->m_pAutoMapper = 0;
		for(int g = 0; g < pEditor->m_Map.m_lGroups.size(); g++)
		{
			CLayerGroup *pGroup = pEditor->m_Map.m_lGroups[g];
			for(int l = 0; l < pGroup->m_lLayers.size(); l++)
			{
				if(pGroup->m_lLayers[l]->m_Type == LAYERTYPE_TILES)
				{
					CLayerTiles *pLayer = static_cast<CLayerTiles *>(pGroup->m_lLayers[l]);
					// resets live auto map of affected layers
					if(pLayer->m_Image == pEditor->m_SelectedImage)
					{
						pLayer->m_SelectedRuleSet = 0;
						pLayer->m_LiveAutoMap = false;
					}
				}
			}
		}
	}
	*pImg = ImgInfo;
	pImg->m_External = External;
	pEditor->ExtractName(pFileName, pImg->m_aName, sizeof(pImg->m_aName));
	pImg->LoadAutoMapper();
	pImg->m_Texture = pEditor->Graphics()->LoadTextureRaw(ImgInfo.m_Width, ImgInfo.m_Height, ImgInfo.m_Format, ImgInfo.m_pData, CImageInfo::FORMAT_AUTO, IGraphics::TEXLOAD_MULTI_DIMENSION);
	ImgInfo.m_pData = 0;
	pEditor->SortImages();
	for(int i = 0; i < pEditor->m_Map.m_lImages.size(); ++i)
	{
		if(!str_comp(pEditor->m_Map.m_lImages[i]->m_aName, pImg->m_aName))
			pEditor->m_SelectedImage = i;
	}
	pEditor->m_Dialog = DIALOG_NONE;
}

void CEditor::AddImage(const char *pFileName, int StorageType, void *pUser)
{
	CEditor *pEditor = (CEditor *) pUser;
	CEditorImage ImgInfo(pEditor);
	if(!pEditor->Graphics()->LoadPNG(&ImgInfo, pFileName, StorageType))
		return;

	// check if we have that image already
	char aBuf[128];
	ExtractName(pFileName, aBuf, sizeof(aBuf));
	for(int i = 0; i < pEditor->m_Map.m_lImages.size(); ++i)
	{
		if(!str_comp(pEditor->m_Map.m_lImages[i]->m_aName, aBuf))
			return;
	}

	CEditorImage *pImg = new CEditorImage(pEditor);
	*pImg = ImgInfo;
	pImg->m_Texture = pEditor->Graphics()->LoadTextureRaw(ImgInfo.m_Width, ImgInfo.m_Height, ImgInfo.m_Format, ImgInfo.m_pData, CImageInfo::FORMAT_AUTO, IGraphics::TEXLOAD_MULTI_DIMENSION);
	ImgInfo.m_pData = 0;
	pImg->m_External = 1; // external by default
	str_copy(pImg->m_aName, aBuf, sizeof(pImg->m_aName));
	pImg->LoadAutoMapper();
	pEditor->m_Map.m_lImages.add(pImg);
	pEditor->SortImages();
	if(pEditor->m_SelectedImage > -1 && pEditor->m_SelectedImage < pEditor->m_Map.m_lImages.size())
	{
		for(int i = 0; i <= pEditor->m_SelectedImage; ++i)
			if(!str_comp(pEditor->m_Map.m_lImages[i]->m_aName, aBuf))
			{
				pEditor->m_SelectedImage++;
				break;
			}
	}
	pEditor->m_Dialog = DIALOG_NONE;
}

static int *gs_pSortedIndex = 0;
static void ModifySortedIndex(int *pIndex)
{
	if(*pIndex > -1)
		*pIndex = gs_pSortedIndex[*pIndex];
}

static int CompareImage(const CEditorImage *pImage1, const CEditorImage *pImage2)
{
	return *pImage1 < *pImage2;
}

void CEditor::SortImages()
{
	bool Sorted = true;
	for(int i = 1; i < m_Map.m_lImages.size(); i++)
		if(*m_Map.m_lImages[i] < *m_Map.m_lImages[i - 1])
		{
			Sorted = false;
			break;
		}

	if(!Sorted)
	{
		array<CEditorImage *> lTemp = array<CEditorImage *>(m_Map.m_lImages);
		gs_pSortedIndex = new int[lTemp.size()];

		std::stable_sort(&m_Map.m_lImages[0], &m_Map.m_lImages[m_Map.m_lImages.size()], CompareImage);

		for(int OldIndex = 0; OldIndex < lTemp.size(); OldIndex++)
			for(int NewIndex = 0; NewIndex < m_Map.m_lImages.size(); NewIndex++)
				if(lTemp[OldIndex] == m_Map.m_lImages[NewIndex])
					gs_pSortedIndex[OldIndex] = NewIndex;

		m_Map.ModifyImageIndex(ModifySortedIndex);

		delete[] gs_pSortedIndex;
		gs_pSortedIndex = 0;
	}
}

void CEditor::RenderImagesList(CUIRect ToolBox)
{
	const float RowHeight = 12.0f;

	static CScrollRegion s_ScrollRegion;
	vec2 ScrollOffset(0.0f, 0.0f);
	CScrollRegionParams ScrollParams;
	ScrollParams.m_ClipBgColor = vec4(0.0f, 0.0f, 0.0f, 0.0f);
	ScrollParams.m_ScrollbarBgColor = vec4(0.0f, 0.0f, 0.0f, 0.0f);
	ScrollParams.m_ScrollbarWidth = 10.0f;
	ScrollParams.m_ScrollbarMargin = 3.0f;
	ScrollParams.m_ScrollUnit = RowHeight * 5;
	s_ScrollRegion.Begin(&ToolBox, &ScrollOffset, &ScrollParams);
	ToolBox.y += ScrollOffset.y;

	for(int e = 0; e < 2; e++) // two passes, first embedded, then external
	{
		CUIRect Slot;
		ToolBox.HSplitTop(RowHeight + 3.0f, &Slot, &ToolBox);
		s_ScrollRegion.AddRect(Slot);
		if(!s_ScrollRegion.IsRectClipped(Slot))
			UI()->DoLabel(&Slot, e == 0 ? "Embedded" : "External", 12.0f, TEXTALIGN_CENTER);

		for(int i = 0; i < m_Map.m_lImages.size(); i++)
		{
			if((e && !m_Map.m_lImages[i]->m_External) || (!e && m_Map.m_lImages[i]->m_External))
				continue;

			ToolBox.HSplitTop(RowHeight + 2.0f, &Slot, &ToolBox);
			s_ScrollRegion.AddRect(Slot);
			if(s_ScrollRegion.IsRectClipped(Slot))
				continue;
			Slot.HSplitTop(RowHeight, &Slot, 0);

			// check if image is used
			bool Used = false;
			for(int g = 0; !Used && (g < m_Map.m_lGroups.size()); g++)
			{
				CLayerGroup *pGroup = m_Map.m_lGroups[g];
				for(int l = 0; !Used && l < pGroup->m_lLayers.size(); l++)
				{
					if(pGroup->m_lLayers[l]->m_Type == LAYERTYPE_TILES)
					{
						if(static_cast<CLayerTiles *>(pGroup->m_lLayers[l])->m_Image == i)
							Used = true;
					}
					else if(pGroup->m_lLayers[l]->m_Type == LAYERTYPE_QUADS)
					{
						if(static_cast<CLayerQuads *>(pGroup->m_lLayers[l])->m_Image == i)
							Used = true;
					}
				}
			}

			if(int Result = DoButton_Image(&m_Map.m_lImages[i], m_Map.m_lImages[i]->m_aName, m_SelectedImage == i, &Slot, BUTTON_CONTEXT, "Select image", Used))
			{
				m_SelectedImage = i;

				if(Result == 2)
					UI()->DoPopupMenu(UI()->MouseX(), UI()->MouseY(), 120, 80, this, PopupImage);
			}
		}

		// separator
		ToolBox.HSplitTop(5.0f, &Slot, &ToolBox);
		s_ScrollRegion.AddRect(Slot);
		if(!s_ScrollRegion.IsRectClipped(Slot))
		{
			IGraphics::CLineItem LineItem(Slot.x, Slot.y + Slot.h / 2, Slot.x + Slot.w, Slot.y + Slot.h / 2);
			Graphics()->TextureClear();
			Graphics()->LinesBegin();
			Graphics()->LinesDraw(&LineItem, 1);
			Graphics()->LinesEnd();
		}
	}

	// new image
	static int s_AddImageButton = 0;
	CUIRect AddImageButton;
	ToolBox.HSplitTop(5.0f + RowHeight + 1.0f, &AddImageButton, &ToolBox);
	s_ScrollRegion.AddRect(AddImageButton);
	if(!s_ScrollRegion.IsRectClipped(AddImageButton))
	{
		AddImageButton.HSplitTop(5.0f, 0, &AddImageButton);
		AddImageButton.HSplitTop(RowHeight, &AddImageButton, 0);
		if(DoButton_Editor(&s_AddImageButton, "Add", 0, &AddImageButton, 0, "Load a new image to use in the map"))
			InvokeFileDialog(IStorage::TYPE_ALL, FILETYPE_IMG, "Add Image", "Add", "mapres", "", AddImage, this);
	}
	s_ScrollRegion.End();
}

void CEditor::RenderSelectedImage(CUIRect View)
{
	if(m_SelectedImage < 0 || m_SelectedImage >= m_Map.m_lImages.size())
		return;

	View.Margin(10.0f, &View);
	if(View.h < View.w)
		View.w = View.h;
	else
		View.h = View.w;
	float Max = (float) (maximum(m_Map.m_lImages[m_SelectedImage]->m_Width, m_Map.m_lImages[m_SelectedImage]->m_Height));
	View.w *= m_Map.m_lImages[m_SelectedImage]->m_Width / Max;
	View.h *= m_Map.m_lImages[m_SelectedImage]->m_Height / Max;
	Graphics()->TextureSet(m_Map.m_lImages[m_SelectedImage]->m_Texture);
	Graphics()->BlendNormal();
	Graphics()->WrapClamp();
	Graphics()->QuadsBegin();
	IGraphics::CQuadItem QuadItem(View.x, View.y, View.w, View.h);
	Graphics()->QuadsDrawTL(&QuadItem, 1);
	Graphics()->QuadsEnd();
	Graphics()->WrapNormal();
}

static int EditorListdirCallback(const char *pName, int IsDir, int StorageType, void *pUser)
{
	CEditor *pEditor = (CEditor *) pUser;
	const char *pExt = 0;
	switch(pEditor->m_FileDialogFileType)
	{
	case CEditor::FILETYPE_MAP: pExt = ".map"; break;
	case CEditor::FILETYPE_IMG: pExt = ".png"; break;
	}
	if(str_comp(pName, ".") == 0 || (str_comp(pName, "..") == 0 && (str_comp(pEditor->m_pFileDialogPath, "maps") == 0 || str_comp(pEditor->m_pFileDialogPath, "mapres") == 0)) || (pExt && !IsDir && !str_endswith(pName, pExt)))
	{
		return 0;
	}

	CEditor::CFilelistItem Item;
	str_copy(Item.m_aFilename, pName, sizeof(Item.m_aFilename));
	if(IsDir)
		str_format(Item.m_aName, sizeof(Item.m_aName), "%s/", pName);
	else
		str_truncate(Item.m_aName, sizeof(Item.m_aName), pName, str_length(pName) - 4);
	Item.m_IsDir = IsDir != 0;
	Item.m_IsLink = false;
	Item.m_StorageType = StorageType;
	pEditor->m_CompleteFileList.add(Item);

	return 0;
}

void CEditor::RenderFileDialog()
{
	// GUI coordsys
	CUIRect View = *UI()->Screen();
	Graphics()->MapScreen(View.x, View.y, View.w, View.h);
	CUIRect Preview = {0, 0, 0, 0};
	float Width = View.w, Height = View.h;

	View.Draw(vec4(0, 0, 0, 0.25f), 0.0f, CUIRect::CORNER_NONE);
	View.VMargin(150.0f, &View);
	View.HMargin(50.0f, &View);
	View.Draw(vec4(0, 0, 0, 0.75f));
	View.Margin(10.0f, &View);

	CUIRect Title, FileBox, FileBoxLabel, ButtonBar, Scroll, PathBox;
	View.HSplitTop(18.0f, &Title, &View);
	View.HSplitTop(5.0f, 0, &View); // some spacing
	View.HSplitBottom(14.0f, &View, &ButtonBar);
	View.HSplitBottom(10.0f, &View, 0); // some spacing
	View.HSplitBottom(14.0f, &View, &PathBox);
	View.HSplitBottom(5.0f, &View, 0); // some spacing
	View.HSplitBottom(14.0f, &View, &FileBox);
	FileBox.VSplitLeft(55.0f, &FileBoxLabel, &FileBox);
	View.HSplitBottom(10.0f, &View, 0); // some spacing
	if(m_FileDialogFileType == CEditor::FILETYPE_IMG)
		View.VSplitMid(&View, &Preview);
	View.VSplitRight(15.0f, &View, &Scroll);

	// title
	Title.Draw(vec4(1, 1, 1, 0.25f), 4.0f);
	Title.VMargin(10.0f, &Title);
	UI()->DoLabel(&Title, m_pFileDialogTitle, 12.0f, TEXTALIGN_LEFT);

	// pathbox
	char aPath[128], aBuf[128];
	if(m_FilesSelectedIndex != -1)
		Storage()->GetCompletePath(m_FilteredFileList[m_FilesSelectedIndex]->m_StorageType, m_pFileDialogPath, aPath, sizeof(aPath));
	else
		aPath[0] = 0;
	str_format(aBuf, sizeof(aBuf), "Current path: %s", aPath);
	UI()->DoLabel(&PathBox, aBuf, 10.0f, TEXTALIGN_LEFT);

	// filebox
	static CListBox s_ListBox;

	if(m_FileDialogStorageType == IStorage::TYPE_SAVE)
	{
		UI()->DoLabel(&FileBoxLabel, "Filename:", 10.0f, TEXTALIGN_LEFT);
		if(DoEditBox(&m_FileDialogFileNameInput, &FileBox, 10.0f))
		{
			// remove '/' and '\'
			for(int i = 0; m_FileDialogFileNameInput.GetString()[i]; ++i)
			{
				if(m_FileDialogFileNameInput.GetString()[i] == '/' || m_FileDialogFileNameInput.GetString()[i] == '\\')
				{
					m_FileDialogFileNameInput.SetRange(m_FileDialogFileNameInput.GetString() + i + 1, i, m_FileDialogFileNameInput.GetLength());
					--i;
				}
			}
			m_FilesSelectedIndex = -1;
			m_aFilesSelectedName[0] = '\0';
			// find first valid entry, if it exists
			for(int i = 0; i < m_FilteredFileList.size(); i++)
			{
				if(str_comp_nocase(m_FilteredFileList[i]->m_aName, m_FileDialogFileNameInput.GetString()) == 0)
				{
					m_FilesSelectedIndex = i;
					str_copy(m_aFilesSelectedName, m_FilteredFileList[i]->m_aName, sizeof(m_aFilesSelectedName));
					break;
				}
			}
			if(m_FilesSelectedIndex >= 0)
				s_ListBox.ScrollToSelected();
		}
	}
	else
	{
		// render search bar
		UI()->DoLabel(&FileBoxLabel, "Search:", 10.0f, TEXTALIGN_LEFT);
		if(DoEditBox(&m_FileDialogFilterInput, &FileBox, 10.0f))
		{
			RefreshFilteredFileList();
			if(m_FilteredFileList.size() == 0)
			{
				m_FilesSelectedIndex = -1;
			}
			else if(m_FilesSelectedIndex == -1 || (m_FileDialogFilterInput.GetLength() && !str_find_nocase(m_FilteredFileList[m_FilesSelectedIndex]->m_aName, m_FileDialogFilterInput.GetString())))
			{
				// we need to refresh selection
				m_FilesSelectedIndex = -1;
				for(int i = 0; i < m_FilteredFileList.size(); i++)
				{
					if(str_find_nocase(m_FilteredFileList[i]->m_aName, m_FileDialogFilterInput.GetString()))
					{
						m_FilesSelectedIndex = i;
						break;
					}
				}
				if(m_FilesSelectedIndex == -1)
				{
					// select first item
					m_FilesSelectedIndex = 0;
				}
			}
			if(m_FilesSelectedIndex >= 0)
				str_copy(m_aFilesSelectedName, m_FilteredFileList[m_FilesSelectedIndex]->m_aName, sizeof(m_aFilesSelectedName));
			else
				m_aFilesSelectedName[0] = '\0';
			if(m_FilesSelectedIndex >= 0 && !m_FilteredFileList[m_FilesSelectedIndex]->m_IsDir)
				m_FileDialogFileNameInput.Set(m_FilteredFileList[m_FilesSelectedIndex]->m_aFilename);
			else
				m_FileDialogFileNameInput.Clear();
			s_ListBox.ScrollToSelected();
		}
	}

	if(m_FilesSelectedIndex >= 0 && m_FilesSelectedIndex < m_FilteredFileList.size() && m_FileDialogFileType == CEditor::FILETYPE_IMG)
	{
		if(!m_PreviewImageIsLoaded)
		{
			int Length = str_length(m_FilteredFileList[m_FilesSelectedIndex]->m_aFilename);
			if(Length >= str_length(".png") && str_endswith_nocase(m_FilteredFileList[m_FilesSelectedIndex]->m_aFilename, ".png"))
			{
				char aBuffer[IO_MAX_PATH_LENGTH];
				str_format(aBuffer, sizeof(aBuffer), "%s/%s", m_pFileDialogPath, m_FilteredFileList[m_FilesSelectedIndex]->m_aFilename);
				if(Graphics()->LoadPNG(&m_FilePreviewImageInfo, aBuffer, m_FilteredFileList[m_FilesSelectedIndex]->m_StorageType))
				{
					Graphics()->UnloadTexture(&m_FilePreviewImage);
					m_FilePreviewImage = Graphics()->LoadTextureRaw(m_FilePreviewImageInfo.m_Width, m_FilePreviewImageInfo.m_Height, m_FilePreviewImageInfo.m_Format, m_FilePreviewImageInfo.m_pData, m_FilePreviewImageInfo.m_Format, IGraphics::TEXLOAD_NORESAMPLE);
					mem_free(m_FilePreviewImageInfo.m_pData);
					m_PreviewImageIsLoaded = true;
				}
			}
		}
		if(m_PreviewImageIsLoaded)
		{
			int w = m_FilePreviewImageInfo.m_Width;
			int h = m_FilePreviewImageInfo.m_Height;
			if(m_FilePreviewImageInfo.m_Width > Preview.w)
			{
				h = m_FilePreviewImageInfo.m_Height * Preview.w / m_FilePreviewImageInfo.m_Width;
				w = Preview.w;
			}
			if(h > Preview.h)
			{
				w = w * Preview.h / h,
				h = Preview.h;
			}

			Graphics()->TextureSet(m_FilePreviewImage);
			Graphics()->BlendNormal();
			Graphics()->QuadsBegin();
			IGraphics::CQuadItem QuadItem(Preview.x, Preview.y, w, h);
			Graphics()->QuadsDrawTL(&QuadItem, 1);
			Graphics()->QuadsEnd();
		}
	}

	s_ListBox.DoStart(15.0f, m_FilteredFileList.size(), 1, 5, m_FilesSelectedIndex, &View);

	for(int i = 0; i < m_FilteredFileList.size(); i++)
	{
		CListboxItem Item = s_ListBox.DoNextItem(&m_FilteredFileList[i], m_FilesSelectedIndex == i);
		if(!Item.m_Visible)
			continue;

		CUIRect Label, FileIcon;
		Item.m_Rect.VSplitLeft(Item.m_Rect.h, &FileIcon, &Label);
		FileIcon.Margin(2.0f, &FileIcon);
		Label.VSplitLeft(5.0f, 0, &Label);

		Graphics()->TextureSet(g_pData->m_aImages[IMAGE_FILEICONS].m_Id);
		Graphics()->QuadsBegin();
		RenderTools()->SelectSprite(m_FilteredFileList[i]->m_IsDir ? SPRITE_FILE_FOLDER : SPRITE_FILE_MAP2);
		IGraphics::CQuadItem QuadItem(FileIcon.x, FileIcon.y, FileIcon.w, FileIcon.h);
		Graphics()->QuadsDrawTL(&QuadItem, 1);
		Graphics()->QuadsEnd();

		UI()->DoLabelSelected(&Label, m_FilteredFileList[i]->m_aName, Item.m_Selected, 10.0f, TEXTALIGN_ML);
	}

	int NewSelection = s_ListBox.DoEnd();
	if(NewSelection != m_FilesSelectedIndex)
	{
		m_FilesSelectedIndex = NewSelection;
		str_copy(m_aFilesSelectedName, m_FilteredFileList[m_FilesSelectedIndex]->m_aName, sizeof(m_aFilesSelectedName));
		if(!m_FilteredFileList[m_FilesSelectedIndex]->m_IsDir)
			m_FileDialogFileNameInput.Set(m_FilteredFileList[m_FilesSelectedIndex]->m_aFilename);
		else
			m_FileDialogFileNameInput.Clear();
		m_PreviewImageIsLoaded = false;
	}

	// the buttons
	CUIRect Button;
	ButtonBar.VSplitRight(50.0f, &ButtonBar, &Button);
	bool IsDir = m_FilesSelectedIndex >= 0 && m_FilteredFileList[m_FilesSelectedIndex]->m_IsDir;
	static int s_OkButton = 0;
	if(DoButton_Editor(&s_OkButton, IsDir ? "Open" : m_pFileDialogButtonText, 0, &Button, 0, 0) || s_ListBox.WasItemActivated())
	{
		if(IsDir) // folder
		{
			if(str_comp(m_FilteredFileList[m_FilesSelectedIndex]->m_aFilename, "..") == 0) // parent folder
			{
				if(fs_parent_dir(m_pFileDialogPath))
					m_pFileDialogPath = m_aFileDialogCurrentFolder; // leave the link
			}
			else // sub folder
			{
				if(m_FilteredFileList[m_FilesSelectedIndex]->m_IsLink)
				{
					m_pFileDialogPath = m_aFileDialogCurrentLink; // follow the link
					str_copy(m_aFileDialogCurrentLink, m_FilteredFileList[m_FilesSelectedIndex]->m_aFilename, sizeof(m_aFileDialogCurrentLink));
				}
				else
				{
					char aTemp[IO_MAX_PATH_LENGTH];
					str_copy(aTemp, m_pFileDialogPath, sizeof(aTemp));
					str_format(m_pFileDialogPath, IO_MAX_PATH_LENGTH, "%s/%s", aTemp, m_FilteredFileList[m_FilesSelectedIndex]->m_aFilename);
				}
			}
			FilelistPopulate(!str_comp(m_pFileDialogPath, "maps") || !str_comp(m_pFileDialogPath, "mapres") ? m_FileDialogStorageType :
															  m_FilteredFileList[m_FilesSelectedIndex]->m_StorageType);
			if(m_FilesSelectedIndex >= 0 && !m_FilteredFileList[m_FilesSelectedIndex]->m_IsDir)
				m_FileDialogFileNameInput.Set(m_FilteredFileList[m_FilesSelectedIndex]->m_aFilename);
			else
				m_FileDialogFileNameInput.Clear();
		}
		else // file
		{
			str_format(m_aFileSaveName, sizeof(m_aFileSaveName), "%s/%s", m_pFileDialogPath, m_FileDialogFileNameInput.GetString());
			if(!str_comp(m_pFileDialogButtonText, "Save"))
			{
				IOHANDLE File = Storage()->OpenFile(m_aFileSaveName, IOFLAG_READ, IStorage::TYPE_SAVE);
				if(File)
				{
					io_close(File);
					m_PopupEventType = POPEVENT_SAVE;
					m_PopupEventActivated = true;
				}
				else if(m_pfnFileDialogFunc)
					m_pfnFileDialogFunc(m_aFileSaveName, m_FilesSelectedIndex >= 0 ? m_FilteredFileList[m_FilesSelectedIndex]->m_StorageType : m_FileDialogStorageType, m_pFileDialogUser);
			}
			else if(m_pfnFileDialogFunc)
				m_pfnFileDialogFunc(m_aFileSaveName, m_FilesSelectedIndex >= 0 ? m_FilteredFileList[m_FilesSelectedIndex]->m_StorageType : m_FileDialogStorageType, m_pFileDialogUser);
		}
		s_ListBox.ScrollToSelected();
	}

	ButtonBar.VSplitRight(40.0f, &ButtonBar, &Button);
	ButtonBar.VSplitRight(50.0f, &ButtonBar, &Button);
	static int s_CancelButton = 0;
	if(DoButton_Editor(&s_CancelButton, "Cancel", 0, &Button, 0, 0) || UI()->ConsumeHotkey(CUI::HOTKEY_ESCAPE))
		m_Dialog = DIALOG_NONE;

	if(m_FileDialogStorageType == IStorage::TYPE_SAVE)
	{
		ButtonBar.VSplitLeft(40.0f, 0, &ButtonBar);
		ButtonBar.VSplitLeft(70.0f, &Button, &ButtonBar);
		static int s_NewFolderButton = 0;
		if(DoButton_Editor(&s_NewFolderButton, "New folder", 0, &Button, 0, 0))
		{
			m_FileDialogNewFolderNameInput.Clear();
			m_aFileDialogErrString[0] = 0;
			UI()->DoPopupMenu(Width / 2.0f - 200.0f, Height / 2.0f - 100.0f, 400.0f, 200.0f, this, PopupNewFolder);
			UI()->SetActiveItem(&m_FileDialogNewFolderNameInput);
		}

		ButtonBar.VSplitLeft(40.0f, 0, &ButtonBar);
		ButtonBar.VSplitLeft(70.0f, &Button, &ButtonBar);
		static int s_MapInfoButton = 0;
		if(DoButton_Editor(&s_MapInfoButton, "Map details", 0, &Button, 0, 0))
		{
			str_copy(m_Map.m_MapInfoTmp.m_aAuthor, m_Map.m_MapInfo.m_aAuthor, sizeof(m_Map.m_MapInfoTmp.m_aAuthor));
			str_copy(m_Map.m_MapInfoTmp.m_aVersion, m_Map.m_MapInfo.m_aVersion, sizeof(m_Map.m_MapInfoTmp.m_aVersion));
			str_copy(m_Map.m_MapInfoTmp.m_aCredits, m_Map.m_MapInfo.m_aCredits, sizeof(m_Map.m_MapInfoTmp.m_aCredits));
			str_copy(m_Map.m_MapInfoTmp.m_aLicense, m_Map.m_MapInfo.m_aLicense, sizeof(m_Map.m_MapInfoTmp.m_aLicense));
			UI()->DoPopupMenu(Width / 2.0f - 200.0f, Height / 2.0f - 100.0f, 400.0f, 200.0f, this, PopupMapInfo);
			UI()->SetActiveItem(0);
		}
	}
}

void CEditor::RefreshFilteredFileList()
{
	m_FilteredFileList.clear();
	for(int i = 0; i < m_CompleteFileList.size(); i++)
	{
		if(!m_FileDialogFilterInput.GetLength() || str_find_nocase(m_CompleteFileList[i].m_aName, m_FileDialogFilterInput.GetString()))
		{
			m_FilteredFileList.add(&m_CompleteFileList[i]);
		}
	}
	if(m_FilteredFileList.size() > 0)
	{
		if(m_aFilesSelectedName[0])
		{
			for(int i = 0; i < m_FilteredFileList.size(); i++)
			{
				if(m_aFilesSelectedName[0] && str_comp(m_FilteredFileList[i]->m_aName, m_aFilesSelectedName) == 0)
				{
					m_FilesSelectedIndex = i;
					break;
				}
			}
		}
		m_FilesSelectedIndex = clamp(m_FilesSelectedIndex, 0, m_FilteredFileList.size() - 1);
		str_copy(m_aFilesSelectedName, m_FilteredFileList[m_FilesSelectedIndex]->m_aName, sizeof(m_aFilesSelectedName));
	}
	else
	{
		m_FilesSelectedIndex = -1;
		m_aFilesSelectedName[0] = '\0';
	}
}

void CEditor::FilelistPopulate(int StorageType)
{
	m_CompleteFileList.clear();
	if(m_FileDialogStorageType != IStorage::TYPE_SAVE && !str_comp(m_pFileDialogPath, "maps"))
	{
		CFilelistItem Item;
		str_copy(Item.m_aFilename, "downloadedmaps", sizeof(Item.m_aFilename));
		str_copy(Item.m_aName, "downloadedmaps/", sizeof(Item.m_aName));
		Item.m_IsDir = true;
		Item.m_IsLink = true;
		Item.m_StorageType = IStorage::TYPE_SAVE;
		m_CompleteFileList.add(Item);
	}
	Storage()->ListDirectory(StorageType, m_pFileDialogPath, EditorListdirCallback, this);
	RefreshFilteredFileList();
	m_FilesSelectedIndex = m_FilteredFileList.size() ? 0 : -1;
	if(m_FilesSelectedIndex >= 0)
		str_copy(m_aFilesSelectedName, m_FilteredFileList[m_FilesSelectedIndex]->m_aName, sizeof(m_aFilesSelectedName));
	else
		m_aFilesSelectedName[0] = '\0';
	m_PreviewImageIsLoaded = false;
}

void CEditor::InvokeFileDialog(int StorageType, int FileType, const char *pTitle, const char *pButtonText,
	const char *pBasePath, const char *pDefaultName,
	void (*pfnFunc)(const char *pFileName, int StorageType, void *pUser), void *pUser)
{
	m_FileDialogStorageType = StorageType;
	m_pFileDialogTitle = pTitle;
	m_pFileDialogButtonText = pButtonText;
	m_pfnFileDialogFunc = pfnFunc;
	m_pFileDialogUser = pUser;
	m_FileDialogFileNameInput.Clear();
	m_aFileDialogCurrentFolder[0] = 0;
	m_aFileDialogCurrentLink[0] = 0;
	m_FileDialogFilterInput.Clear();
	m_pFileDialogPath = m_aFileDialogCurrentFolder;
	m_FileDialogFileType = FileType;
	UI()->SetActiveItem(&m_FileDialogFilterInput);
	m_PreviewImageIsLoaded = false;

	if(pDefaultName)
		m_FileDialogFileNameInput.Set(pDefaultName);
	if(pBasePath)
		str_copy(m_aFileDialogCurrentFolder, pBasePath, sizeof(m_aFileDialogCurrentFolder));

	FilelistPopulate(m_FileDialogStorageType);

	m_Dialog = DIALOG_FILE;
}

void CEditor::RenderModebar(CUIRect View)
{
	CUIRect Button;

	// mode buttons
	{
		View.VSplitLeft(65.0f, &Button, &View);
		Button.HSplitTop(30.0f, 0, &Button);
		static int s_Button = 0;
		const char *pButName = m_Mode == MODE_LAYERS ? "Layers" : "Images";
		if(DoButton_Tab(&s_Button, pButName, 0, &Button, 0, "Switch between image and layer management."))
		{
			if(m_Mode == MODE_LAYERS)
				m_Mode = MODE_IMAGES;
			else
				m_Mode = MODE_LAYERS;
		}
	}

	View.VSplitLeft(5.0f, 0, &View);
}

void CEditor::RenderStatusbar(CUIRect View)
{
	CUIRect Button;
	View.VSplitRight(60.0f, &View, &Button);
	static int s_EnvelopeButton = 0;
	if(DoButton_Editor(&s_EnvelopeButton, "Envelopes", m_ShowEnvelopeEditor, &Button, 0, "Toggles the envelope editor."))
		m_ShowEnvelopeEditor = (m_ShowEnvelopeEditor + 1) % 4;

	if(m_pTooltip)
	{
		if(ms_pUiGotContext && ms_pUiGotContext == UI()->HotItem())
		{
			char aBuf[512];
			str_format(aBuf, sizeof(aBuf), "%s Right click for context menu.", m_pTooltip);
			UI()->DoLabel(&View, aBuf, 10.0f, TEXTALIGN_LEFT);
		}
		else
			UI()->DoLabel(&View, m_pTooltip, 10.0f, TEXTALIGN_LEFT);
	}
}

void CEditor::RenderEnvelopeEditor(CUIRect View)
{
	if(m_SelectedEnvelope < 0)
		m_SelectedEnvelope = 0;
	if(m_SelectedEnvelope >= m_Map.m_lEnvelopes.size())
		m_SelectedEnvelope = m_Map.m_lEnvelopes.size() - 1;

	CEnvelope *pEnvelope = 0;
	if(m_SelectedEnvelope >= 0 && m_SelectedEnvelope < m_Map.m_lEnvelopes.size())
		pEnvelope = m_Map.m_lEnvelopes[m_SelectedEnvelope];

	CUIRect ToolBar, CurveBar, ColorBar;
	View.HSplitTop(15.0f, &ToolBar, &View);
	View.HSplitTop(15.0f, &CurveBar, &View);
	ToolBar.Margin(2.0f, &ToolBar);
	CurveBar.Margin(2.0f, &CurveBar);

	// do the toolbar
	{
		CUIRect Button;
		CEnvelope *pNewEnv = 0;

		ToolBar.VSplitRight(50.0f, &ToolBar, &Button);
		static int s_New4dButton = 0;
		if(DoButton_Editor(&s_New4dButton, "Color+", 0, &Button, 0, "Creates a new color envelope"))
		{
			m_Map.m_Modified = true;
			pNewEnv = m_Map.NewEnvelope(4);
		}

		ToolBar.VSplitRight(5.0f, &ToolBar, &Button);
		ToolBar.VSplitRight(50.0f, &ToolBar, &Button);
		static int s_New2dButton = 0;
		if(DoButton_Editor(&s_New2dButton, "Pos.+", 0, &Button, 0, "Creates a new pos envelope"))
		{
			m_Map.m_Modified = true;
			pNewEnv = m_Map.NewEnvelope(3);
		}

		// Delete button
		if(m_SelectedEnvelope >= 0)
		{
			ToolBar.VSplitRight(10.0f, &ToolBar, &Button);
			ToolBar.VSplitRight(50.0f, &ToolBar, &Button);
			static int s_DelButton = 0;
			if(DoButton_Editor(&s_DelButton, "Delete", 0, &Button, 0, "Delete this envelope"))
			{
				m_Map.m_Modified = true;
				m_Map.DeleteEnvelope(m_SelectedEnvelope);
				if(m_SelectedEnvelope >= m_Map.m_lEnvelopes.size())
					m_SelectedEnvelope = m_Map.m_lEnvelopes.size() - 1;
				pEnvelope = m_SelectedEnvelope >= 0 ? m_Map.m_lEnvelopes[m_SelectedEnvelope] : 0;
			}
		}

		if(pNewEnv) // add the default points
		{
			if(pNewEnv->m_Channels == 4)
			{
				pNewEnv->AddPoint(0, f2fx(1.0f), f2fx(1.0f), f2fx(1.0f), f2fx(1.0f));
				pNewEnv->AddPoint(1000, f2fx(1.0f), f2fx(1.0f), f2fx(1.0f), f2fx(1.0f));
			}
			else
			{
				pNewEnv->AddPoint(0, 0);
				pNewEnv->AddPoint(1000, 0);
			}
		}

		CUIRect Shifter, Inc, Dec;
		ToolBar.VSplitLeft(60.0f, &Shifter, &ToolBar);
		Shifter.VSplitRight(15.0f, &Shifter, &Inc);
		Shifter.VSplitLeft(15.0f, &Dec, &Shifter);
		char aBuf[IO_MAX_PATH_LENGTH];
		str_format(aBuf, sizeof(aBuf), "%d/%d", m_SelectedEnvelope + 1, m_Map.m_lEnvelopes.size());
		Shifter.Draw(vec4(1, 1, 1, 0.5f), 0.0f, CUIRect::CORNER_NONE);
		UI()->DoLabel(&Shifter, aBuf, 10.0f, TEXTALIGN_CENTER);

		static int s_PrevButton = 0;
		if(DoButton_ButtonDec(&s_PrevButton, 0, 0, &Dec, 0, "Previous Envelope"))
			m_SelectedEnvelope--;

		static int s_NextButton = 0;
		if(DoButton_ButtonInc(&s_NextButton, 0, 0, &Inc, 0, "Next Envelope"))
			m_SelectedEnvelope++;

		if(pEnvelope)
		{
			ToolBar.VSplitLeft(15.0f, &Button, &ToolBar);
			ToolBar.VSplitLeft(35.0f, &Button, &ToolBar);
			UI()->DoLabel(&Button, "Name:", 10.0f, TEXTALIGN_LEFT);

			ToolBar.VSplitLeft(80.0f, &Button, &ToolBar);

			static CLineInput s_NameInput;
			s_NameInput.SetBuffer(pEnvelope->m_aName, sizeof(pEnvelope->m_aName));
			if(DoEditBox(&s_NameInput, &Button, 10.0f))
				m_Map.m_Modified = true;
		}
	}

	bool ShowColorBar = false;
	if(pEnvelope && pEnvelope->m_Channels == 4)
	{
		ShowColorBar = true;
		View.HSplitTop(20.0f, &ColorBar, &View);
		ColorBar.Margin(2.0f, &ColorBar);
		RenderBackground(ColorBar, m_CheckerTexture, 16.0f, 1.0f);
	}

	RenderBackground(View, m_CheckerTexture, 32.0f, 0.1f);

	if(pEnvelope)
	{
		static array<int> Selection;
		static int sEnvelopeEditorID = 0;
		static int s_ActiveChannels = 0xf;

		{
			CUIRect Button;

			ToolBar.VSplitLeft(15.0f, &Button, &ToolBar);

			static const char *s_paNames[2][4] = {
				{"X", "Y", "R", ""},
				{"R", "G", "B", "A"},
			};

			const char *paDescriptions[2][4] = {
				{"X-axis of the envelope", "Y-axis of the envelope", "Rotation of the envelope", ""},
				{"Red value of the envelope", "Green value of the envelope", "Blue value of the envelope", "Alpha value of the envelope"},
			};

			static int s_aChannelButtons[4] = {0};
			int Bit = 1;

			for(int i = 0; i < pEnvelope->m_Channels; i++, Bit <<= 1)
			{
				ToolBar.VSplitLeft(15.0f, &Button, &ToolBar);

				if(DoButton_Editor(&s_aChannelButtons[i], s_paNames[pEnvelope->m_Channels - 3][i], s_ActiveChannels & Bit, &Button, 0, paDescriptions[pEnvelope->m_Channels - 3][i]))
					s_ActiveChannels ^= Bit;
			}

			// sync checkbox
			ToolBar.VSplitLeft(15.0f, &Button, &ToolBar);
			ToolBar.VSplitLeft(12.0f, &Button, &ToolBar);
			static int s_SyncButton;
			if(DoButton_Editor(&s_SyncButton, pEnvelope->m_Synchronized ? "X" : "", 0, &Button, 0, "Enable envelope synchronization between clients"))
				pEnvelope->m_Synchronized = !pEnvelope->m_Synchronized;

			ToolBar.VSplitLeft(4.0f, &Button, &ToolBar);
			ToolBar.VSplitLeft(80.0f, &Button, &ToolBar);
			UI()->DoLabel(&Button, "Synchronized", 10.0f, TEXTALIGN_LEFT);
		}

		float EndTime = pEnvelope->EndTime();
		if(EndTime < 1)
			EndTime = 1;

		pEnvelope->FindTopBottom(s_ActiveChannels);
		float Top = pEnvelope->m_Top;
		float Bottom = pEnvelope->m_Bottom;

		if(Top < 1)
			Top = 1;
		if(Bottom >= 0)
			Bottom = 0;

		float TimeScale = EndTime / View.w;
		float ValueScale = (Top - Bottom) / View.h;

		if(UI()->MouseInside(&View))
			UI()->SetHotItem(&sEnvelopeEditorID);

		if(UI()->HotItem() == &sEnvelopeEditorID)
		{
			// do stuff
			if(UI()->MouseButtonClicked(1))
			{
				// add point
				int Time = (int) (((UI()->MouseX() - View.x) * TimeScale) * 1000.0f);
				float aChannels[4];
				pEnvelope->Eval(Time / 1000.0f, aChannels);
				pEnvelope->AddPoint(Time,
					f2fx(aChannels[0]), f2fx(aChannels[1]),
					f2fx(aChannels[2]), f2fx(aChannels[3]));
				m_Map.m_Modified = true;
			}

			m_ShowEnvelopePreview = SHOWENV_SELECTED;
			m_pTooltip = "Press right mouse button to create a new point";
		}

		vec3 aColors[] = {vec3(1, 0.2f, 0.2f), vec3(0.2f, 1, 0.2f), vec3(0.2f, 0.2f, 1), vec3(1, 1, 0.2f)};

		// render tangents for bezier curves
		{
			UI()->ClipEnable(&View);
			Graphics()->TextureClear();
			Graphics()->LinesBegin();
			for(int c = 0; c < pEnvelope->m_Channels; c++)
			{
				if(!(s_ActiveChannels & (1 << c)))
					continue;

				for(int i = 0; i < pEnvelope->m_lPoints.size(); i++)
				{
					float px = pEnvelope->m_lPoints[i].m_Time / 1000.0f / EndTime;
					float py = (fx2f(pEnvelope->m_lPoints[i].m_aValues[c]) - Bottom) / (Top - Bottom);

					if(pEnvelope->m_lPoints[i].m_Curvetype == CURVETYPE_BEZIER)
					{
						// Out-Tangent
						float x_out = px + (pEnvelope->m_lPoints[i].m_aOutTangentdx[c] / 1000.0f / EndTime);
						float y_out = py + fx2f(pEnvelope->m_lPoints[i].m_aOutTangentdy[c]) / (Top - Bottom);

						if(m_SelectedQuadEnvelope == m_SelectedEnvelope && m_SelectedEnvelopePoint == i)
							Graphics()->SetColor(1.0f, 1.0f, 1.0f, 0.4f);
						else
							Graphics()->SetColor(aColors[c].r, aColors[c].g, aColors[c].b, 0.4f);

						IGraphics::CLineItem LineItem(View.x + px * View.w, View.y + View.h - py * View.h, View.x + x_out * View.w, View.y + View.h - y_out * View.h);
						Graphics()->LinesDraw(&LineItem, 1);
					}

					if(i > 0 && pEnvelope->m_lPoints[i - 1].m_Curvetype == CURVETYPE_BEZIER)
					{
						// In-Tangent
						float x_in = px + (pEnvelope->m_lPoints[i].m_aInTangentdx[c] / 1000.0f / EndTime);
						float y_in = py + fx2f(pEnvelope->m_lPoints[i].m_aInTangentdy[c]) / (Top - Bottom);

						if(m_SelectedQuadEnvelope == m_SelectedEnvelope && m_SelectedEnvelopePoint == i)
							Graphics()->SetColor(1.0f, 1.0f, 1.0f, 0.4f);
						else
							Graphics()->SetColor(aColors[c].r, aColors[c].g, aColors[c].b, 0.4f);

						IGraphics::CLineItem LineItem(View.x + px * View.w, View.y + View.h - py * View.h, View.x + x_in * View.w, View.y + View.h - y_in * View.h);
						Graphics()->LinesDraw(&LineItem, 1);
					}
				}
			}
			Graphics()->LinesEnd();
			UI()->ClipDisable();
		}

		// render lines
		{
			UI()->ClipEnable(&View);
			Graphics()->TextureClear();
			Graphics()->LinesBegin();
			for(int c = 0; c < pEnvelope->m_Channels; c++)
			{
				if(s_ActiveChannels & (1 << c))
					Graphics()->SetColor(aColors[c].r, aColors[c].g, aColors[c].b, 1);
				else
					Graphics()->SetColor(aColors[c].r * 0.5f, aColors[c].g * 0.5f, aColors[c].b * 0.5f, 1);

				float PrevX = 0;
				float aResults[4];
				pEnvelope->Eval(0.000001f, aResults);
				float PrevValue = aResults[c];

				int Steps = (int) ((View.w / UI()->Screen()->w) * Graphics()->ScreenWidth());
				for(int i = 1; i <= Steps; i++)
				{
					float a = i / (float) Steps;
					pEnvelope->Eval(a * EndTime, aResults);
					float v = aResults[c];
					v = (v - Bottom) / (Top - Bottom);

					IGraphics::CLineItem LineItem(View.x + PrevX * View.w, View.y + View.h - PrevValue * View.h, View.x + a * View.w, View.y + View.h - v * View.h);
					Graphics()->LinesDraw(&LineItem, 1);
					PrevX = a;
					PrevValue = v;
				}
			}
			Graphics()->LinesEnd();
			UI()->ClipDisable();
		}

		// render curve options
		{
			for(int i = 0; i < pEnvelope->m_lPoints.size() - 1; i++)
			{
				float t0 = pEnvelope->m_lPoints[i].m_Time / 1000.0f / EndTime;
				float t1 = pEnvelope->m_lPoints[i + 1].m_Time / 1000.0f / EndTime;

				CUIRect v;
				v.x = CurveBar.x + (t0 + (t1 - t0) * 0.5f) * CurveBar.w;
				v.y = CurveBar.y;
				v.h = CurveBar.h;
				v.w = CurveBar.h;
				v.x -= v.w / 2;
				void *pID = &pEnvelope->m_lPoints[i].m_Curvetype;
				const char *paTypeName[] = {
					"N", "L", "S", "F", "M", "B"};
				const char *pTypeName = "!?";
				if(0 <= pEnvelope->m_lPoints[i].m_Curvetype && pEnvelope->m_lPoints[i].m_Curvetype < (int) (sizeof(paTypeName) / sizeof(const char *)))
					pTypeName = paTypeName[pEnvelope->m_lPoints[i].m_Curvetype];
				if(DoButton_Editor(pID, pTypeName, 0, &v, 0, "Switch curve type"))
					pEnvelope->m_lPoints[i].m_Curvetype = (pEnvelope->m_lPoints[i].m_Curvetype + 1) % NUM_CURVETYPES;
			}
		}

		// render colorbar
		if(ShowColorBar)
		{
			Graphics()->TextureClear();
			Graphics()->QuadsBegin();
			for(int i = 0; i < pEnvelope->m_lPoints.size() - 1; i++)
			{
				float r0 = fx2f(pEnvelope->m_lPoints[i].m_aValues[0]);
				float g0 = fx2f(pEnvelope->m_lPoints[i].m_aValues[1]);
				float b0 = fx2f(pEnvelope->m_lPoints[i].m_aValues[2]);
				float a0 = fx2f(pEnvelope->m_lPoints[i].m_aValues[3]);
				float r1 = fx2f(pEnvelope->m_lPoints[i + 1].m_aValues[0]);
				float g1 = fx2f(pEnvelope->m_lPoints[i + 1].m_aValues[1]);
				float b1 = fx2f(pEnvelope->m_lPoints[i + 1].m_aValues[2]);
				float a1 = fx2f(pEnvelope->m_lPoints[i + 1].m_aValues[3]);

				IGraphics::CColorVertex Array[4] = {IGraphics::CColorVertex(0, r0, g0, b0, a0),
					IGraphics::CColorVertex(1, r1, g1, b1, a1),
					IGraphics::CColorVertex(2, r1, g1, b1, a1),
					IGraphics::CColorVertex(3, r0, g0, b0, a0)};
				Graphics()->SetColorVertex(Array, 4);

				float x0 = pEnvelope->m_lPoints[i].m_Time / 1000.0f / EndTime;
				float x1 = pEnvelope->m_lPoints[i + 1].m_Time / 1000.0f / EndTime;
				CUIRect v;
				v.x = ColorBar.x + x0 * ColorBar.w;
				v.y = ColorBar.y;
				v.w = (x1 - x0) * ColorBar.w;
				v.h = ColorBar.h;

				IGraphics::CQuadItem QuadItem(v.x, v.y, v.w, v.h);
				Graphics()->QuadsDrawTL(&QuadItem, 1);
			}
			Graphics()->QuadsEnd();
		}

		// render handles
		{
			int CurrentValue = 0, CurrentTime = 0;

			Graphics()->TextureClear();
			Graphics()->QuadsBegin();
			for(int c = 0; c < pEnvelope->m_Channels; c++)
			{
				if(!(s_ActiveChannels & (1 << c)))
					continue;

				for(int i = 0; i < pEnvelope->m_lPoints.size(); i++)
				{
					{
						// point handle
						float x0 = pEnvelope->m_lPoints[i].m_Time / 1000.0f / EndTime;
						float y0 = (fx2f(pEnvelope->m_lPoints[i].m_aValues[c]) - Bottom) / (Top - Bottom);
						CUIRect Final;
						Final.x = View.x + x0 * View.w;
						Final.y = View.y + View.h - y0 * View.h;
						Final.x -= 2.0f;
						Final.y -= 2.0f;
						Final.w = 4.0f;
						Final.h = 4.0f;

						void *pID = &pEnvelope->m_lPoints[i].m_aValues[c];

						if(UI()->MouseInside(&Final))
							UI()->SetHotItem(pID);

						float ColorMod = 1.0f;

						if(UI()->CheckActiveItem(pID))
						{
							if(!UI()->MouseButton(0))
							{
								m_SelectedQuadEnvelope = -1;
								m_SelectedEnvelopePoint = -1;

								UI()->SetActiveItem(0);
							}
							else
							{
								if(Input()->KeyIsPressed(KEY_LSHIFT) || Input()->KeyIsPressed(KEY_RSHIFT))
								{
									if(i != 0)
									{
										if((Input()->KeyIsPressed(KEY_LCTRL) || Input()->KeyIsPressed(KEY_RCTRL)))
											pEnvelope->m_lPoints[i].m_Time += (int) ((m_MouseDeltaX));
										else
											pEnvelope->m_lPoints[i].m_Time += (int) ((m_MouseDeltaX * TimeScale) * 1000.0f);
										if(pEnvelope->m_lPoints[i].m_Time < pEnvelope->m_lPoints[i - 1].m_Time)
											pEnvelope->m_lPoints[i].m_Time = pEnvelope->m_lPoints[i - 1].m_Time + 1;
										if(i + 1 != pEnvelope->m_lPoints.size() && pEnvelope->m_lPoints[i].m_Time > pEnvelope->m_lPoints[i + 1].m_Time)
											pEnvelope->m_lPoints[i].m_Time = pEnvelope->m_lPoints[i + 1].m_Time - 1;

										// clamp tangents
										pEnvelope->m_lPoints[i].m_aInTangentdx[c] = clamp(pEnvelope->m_lPoints[i].m_aInTangentdx[c], -pEnvelope->m_lPoints[i].m_Time, 0);
										pEnvelope->m_lPoints[i].m_aOutTangentdx[c] = clamp(pEnvelope->m_lPoints[i].m_aOutTangentdx[c], 0,
											(int) (EndTime * 1000.f - pEnvelope->m_lPoints[i].m_Time));
									}
								}
								else
								{
									if((Input()->KeyIsPressed(KEY_LCTRL) || Input()->KeyIsPressed(KEY_RCTRL)))
										pEnvelope->m_lPoints[i].m_aValues[c] -= f2fx(m_MouseDeltaY * 0.001f);
									else
										pEnvelope->m_lPoints[i].m_aValues[c] -= f2fx(m_MouseDeltaY * ValueScale);
								}

								m_SelectedQuadEnvelope = m_SelectedEnvelope;
								m_ShowEnvelopePreview = SHOWENV_SELECTED;
								m_SelectedEnvelopePoint = i;
								m_Map.m_Modified = true;
							}

							ColorMod = 100.0f;
							Graphics()->SetColor(1, 1, 1, 1);
						}
						else if(UI()->HotItem() == pID)
						{
							if(UI()->MouseButton(0))
							{
								Selection.clear();
								Selection.add(i);
								UI()->SetActiveItem(pID);
							}

							// remove point
							if(UI()->MouseButtonClicked(1))
							{
								pEnvelope->m_lPoints.remove_index(i);
								m_Map.m_Modified = true;
							}

							m_ShowEnvelopePreview = SHOWENV_SELECTED;
							ColorMod = 100.0f;
							Graphics()->SetColor(1, 0.75f, 0.75f, 1);
							m_pTooltip = "Left mouse to drag. Hold Ctrl for more precision. Hold Shift to alter time point aswell. Right click to delete.";
						}

						if(UI()->CheckActiveItem(pID) || UI()->HotItem() == pID)
						{
							CurrentTime = pEnvelope->m_lPoints[i].m_Time;
							CurrentValue = pEnvelope->m_lPoints[i].m_aValues[c];
						}

						if(m_SelectedQuadEnvelope == m_SelectedEnvelope && m_SelectedEnvelopePoint == i)
							Graphics()->SetColor(1.0f, 1.0f, 1.0f, 1.0f);
						else
							Graphics()->SetColor(aColors[c].r * ColorMod, aColors[c].g * ColorMod, aColors[c].b * ColorMod, 1.0f);

						IGraphics::CQuadItem QuadItem(Final.x, Final.y, Final.w, Final.h);
						Graphics()->QuadsDrawTL(&QuadItem, 1);
					}

					{
						float px = pEnvelope->m_lPoints[i].m_Time / 1000.0f / EndTime;
						float py = (fx2f(pEnvelope->m_lPoints[i].m_aValues[c]) - Bottom) / (Top - Bottom);

						// tangent handles for bezier curves
						if(pEnvelope->m_lPoints[i].m_Curvetype == CURVETYPE_BEZIER)
						{
							// Out-Tangent handle
							float x_out = px + (pEnvelope->m_lPoints[i].m_aOutTangentdx[c] / 1000.0f / EndTime);
							float y_out = py + fx2f(pEnvelope->m_lPoints[i].m_aOutTangentdy[c]) / (Top - Bottom);

							CUIRect Final;
							Final.x = View.x + x_out * View.w;
							Final.y = View.y + View.h - y_out * View.h;
							Final.x -= 2.0f;
							Final.y -= 2.0f;
							Final.w = 4.0f;
							Final.h = 4.0f;

							// handle logic
							void *pID = &pEnvelope->m_lPoints[i].m_aOutTangentdx[c];

							float ColorMod = 1.0f;
							if(UI()->MouseInside(&Final))
								UI()->SetHotItem(pID);

							if(UI()->CheckActiveItem(pID))
							{
								if(!UI()->MouseButton(0))
								{
									m_SelectedQuadEnvelope = -1;
									m_SelectedEnvelopePoint = -1;

									UI()->SetActiveItem(0);
								}
								else
								{
									if((Input()->KeyIsPressed(KEY_LCTRL) || Input()->KeyIsPressed(KEY_RCTRL)))
									{
										pEnvelope->m_lPoints[i].m_aOutTangentdx[c] += (int) ((m_MouseDeltaX));
										pEnvelope->m_lPoints[i].m_aOutTangentdy[c] -= f2fx(m_MouseDeltaY * 0.001f);
									}
									else
									{
										pEnvelope->m_lPoints[i].m_aOutTangentdx[c] += (int) ((m_MouseDeltaX * TimeScale) * 1000.0f);
										pEnvelope->m_lPoints[i].m_aOutTangentdy[c] -= f2fx(m_MouseDeltaY * ValueScale);
									}

									// clamp time value
									pEnvelope->m_lPoints[i].m_aOutTangentdx[c] = clamp(pEnvelope->m_lPoints[i].m_aOutTangentdx[c], 0,
										(int) (EndTime * 1000.f - pEnvelope->m_lPoints[i].m_Time));

									m_SelectedQuadEnvelope = m_SelectedEnvelope;
									m_ShowEnvelopePreview = SHOWENV_SELECTED;
									m_SelectedEnvelopePoint = i;
									m_Map.m_Modified = true;
								}
								ColorMod = 100.0f;
							}
							else if(UI()->HotItem() == pID)
							{
								if(UI()->MouseButton(0))
								{
									Selection.clear();
									Selection.add(i);
									UI()->SetActiveItem(pID);
								}

								m_ShowEnvelopePreview = SHOWENV_SELECTED;
								ColorMod = 100.0f;
								m_pTooltip = "Left mouse to drag. Hold Ctrl for more precision.";
							}

							if(UI()->CheckActiveItem(pID) || UI()->HotItem() == pID)
							{
								CurrentTime = pEnvelope->m_lPoints[i].m_Time + pEnvelope->m_lPoints[i].m_aOutTangentdx[c];
								CurrentValue = pEnvelope->m_lPoints[i].m_aValues[c] + pEnvelope->m_lPoints[i].m_aOutTangentdy[c];
							}

							if(m_SelectedQuadEnvelope == m_SelectedEnvelope && m_SelectedEnvelopePoint == i)
								Graphics()->SetColor(1.0f, 1.0f, 1.0f, 0.5f);
							else
								Graphics()->SetColor(aColors[c].r * ColorMod, aColors[c].g * ColorMod, aColors[c].b * ColorMod, 0.5f);

							IGraphics::CFreeformItem FreeformItem(Final.x + Final.w / 2, Final.y, Final.x + Final.w / 2, Final.y,
								Final.x + Final.w, Final.y + Final.h, Final.x, Final.y + Final.h);
							Graphics()->QuadsDrawFreeform(&FreeformItem, 1);
						}

						if(i > 0 && pEnvelope->m_lPoints[i - 1].m_Curvetype == CURVETYPE_BEZIER)
						{
							// In-Tangent handle
							float x_in = px + (pEnvelope->m_lPoints[i].m_aInTangentdx[c] / 1000.0f / EndTime);
							float y_in = py + fx2f(pEnvelope->m_lPoints[i].m_aInTangentdy[c]) / (Top - Bottom);

							CUIRect Final;
							Final.x = View.x + x_in * View.w;
							Final.y = View.y + View.h - y_in * View.h;
							Final.x -= 2.0f;
							Final.y -= 2.0f;
							Final.w = 4.0f;
							Final.h = 4.0f;

							// handle logic
							void *pID = &pEnvelope->m_lPoints[i].m_aInTangentdx[c];

							float ColorMod = 1.0f;
							if(UI()->MouseInside(&Final))
								UI()->SetHotItem(pID);

							if(UI()->CheckActiveItem(pID))
							{
								if(!UI()->MouseButton(0))
								{
									m_SelectedQuadEnvelope = -1;
									m_SelectedEnvelopePoint = -1;

									UI()->SetActiveItem(0);
								}
								else
								{
									if((Input()->KeyIsPressed(KEY_LCTRL) || Input()->KeyIsPressed(KEY_RCTRL)))
									{
										pEnvelope->m_lPoints[i].m_aInTangentdx[c] += (int) ((m_MouseDeltaX));
										pEnvelope->m_lPoints[i].m_aInTangentdy[c] -= f2fx(m_MouseDeltaY * 0.001f);
									}
									else
									{
										pEnvelope->m_lPoints[i].m_aInTangentdx[c] += (int) ((m_MouseDeltaX * TimeScale) * 1000.0f);
										pEnvelope->m_lPoints[i].m_aInTangentdy[c] -= f2fx(m_MouseDeltaY * ValueScale);
									}

									// clamp time value
									pEnvelope->m_lPoints[i].m_aInTangentdx[c] = clamp(pEnvelope->m_lPoints[i].m_aInTangentdx[c], -pEnvelope->m_lPoints[i].m_Time, 0);

									m_SelectedQuadEnvelope = m_SelectedEnvelope;
									m_ShowEnvelopePreview = SHOWENV_SELECTED;
									m_SelectedEnvelopePoint = i;
									m_Map.m_Modified = true;
								}
								ColorMod = 100.0f;
							}
							else if(UI()->HotItem() == pID)
							{
								if(UI()->MouseButton(0))
								{
									Selection.clear();
									Selection.add(i);
									UI()->SetActiveItem(pID);
								}

								m_ShowEnvelopePreview = SHOWENV_SELECTED;
								ColorMod = 100.0f;
								m_pTooltip = "Left mouse to drag. Hold Ctrl for more precision.";
							}

							if(UI()->CheckActiveItem(pID) || UI()->HotItem() == pID)
							{
								CurrentTime = pEnvelope->m_lPoints[i].m_Time + pEnvelope->m_lPoints[i].m_aInTangentdx[c];
								CurrentValue = pEnvelope->m_lPoints[i].m_aValues[c] + pEnvelope->m_lPoints[i].m_aInTangentdy[c];
							}

							if(m_SelectedQuadEnvelope == m_SelectedEnvelope && m_SelectedEnvelopePoint == i)
								Graphics()->SetColor(1.0f, 1.0f, 1.0f, 0.5f);
							else
								Graphics()->SetColor(aColors[c].r * ColorMod, aColors[c].g * ColorMod, aColors[c].b * ColorMod, 0.5f);

							// draw triangle
							IGraphics::CFreeformItem FreeformItem(Final.x + Final.w / 2, Final.y, Final.x + Final.w / 2, Final.y,
								Final.x + Final.w, Final.y + Final.h, Final.x, Final.y + Final.h);
							Graphics()->QuadsDrawFreeform(&FreeformItem, 1);
						}
					}
				}
			}
			Graphics()->QuadsEnd();

			char aBuf[512];
			str_format(aBuf, sizeof(aBuf), "%.3f %.3f", CurrentTime / 1000.0f, fx2f(CurrentValue));
			UI()->DoLabel(&ToolBar, aBuf, 10.0f, TEXTALIGN_CENTER);
		}
	}
}

void CEditor::RenderMenubar(CUIRect MenuBar)
{
	CUIRect FileButton;
	static int s_FileButton;
	MenuBar.VSplitLeft(60.0f, &FileButton, &MenuBar);
	if(DoButton_Menu(&s_FileButton, "File", 0, &FileButton, 0, 0))
		UI()->DoPopupMenu(FileButton.x, FileButton.y + FileButton.h - 1.0f, 120, 150, this, PopupMenuFile, CUIRect::CORNER_R | CUIRect::CORNER_B);

	CUIRect Info, ExitButton;
	MenuBar.VSplitRight(20.f, &MenuBar, &ExitButton);
	MenuBar.VSplitLeft(40.0f, 0, &MenuBar);
	MenuBar.VSplitLeft(MenuBar.w * 0.75f, &MenuBar, &Info);

	char aBuf[128];
	str_format(aBuf, sizeof(aBuf), "File: %s", m_aFileName);
	UI()->DoLabel(&MenuBar, aBuf, 10.0f, TEXTALIGN_LEFT);

	str_format(aBuf, sizeof(aBuf), "Z: %i, A: %.1f, G: %i", m_ZoomLevel, m_AnimateSpeed, m_GridFactor);
	UI()->DoLabel(&Info, aBuf, 10.0f, TEXTALIGN_RIGHT);

	// Exit editor button
	static int s_ExitButton;
	ExitButton.VSplitRight(13.f, 0, &ExitButton);
	if(DoButton_Editor(&s_ExitButton, "\xE2\x9C\x95", 1, &ExitButton, 0, "[ctrl+shift+e] Exit"))
	{
		Config()->m_ClEditor ^= 1;
		Input()->MouseModeRelative();
	}
}

void CEditor::Render()
{
	// basic start
	Graphics()->Clear(1.0f, 0.0f, 1.0f);
	CUIRect View = *UI()->Screen();
	Graphics()->MapScreen(View.x, View.y, View.w, View.h);

	float Width = View.w;
	float Height = View.h;

	// reset tip
	m_pTooltip = 0;

	if(UI()->IsInputActive())
		m_EditBoxActive = 2;
	else if(m_EditBoxActive)
		--m_EditBoxActive;

	// render checker
	RenderBackground(View, m_CheckerTexture, 32.0f, 1.0f);

	CUIRect MenuBar, CModeBar, ToolBar, StatusBar, EnvelopeEditor, ToolBox;
	m_ShowTilePicker = Input()->KeyIsPressed(KEY_SPACE) != 0 && m_Dialog == DIALOG_NONE;

	if(m_GuiActive)
	{
		View.HSplitTop(16.0f, &MenuBar, &View);
		View.HSplitTop(53.0f, &ToolBar, &View);
		View.VSplitLeft(100.0f, &ToolBox, &View);
		View.HSplitBottom(16.0f, &View, &StatusBar);

		if(m_ShowEnvelopeEditor && !m_ShowTilePicker)
		{
			float size = 125.0f;
			if(m_ShowEnvelopeEditor == 2)
				size *= 2.0f;
			else if(m_ShowEnvelopeEditor == 3)
				size *= 3.0f;
			View.HSplitBottom(size, &View, &EnvelopeEditor);
		}
	}

	//	a little hack for now
	if(m_Mode == MODE_LAYERS)
		DoMapEditor(View);

	// do zooming
	if(Input()->KeyPress(KEY_KP_MINUS))
		m_ZoomLevel += 50;
	if(Input()->KeyPress(KEY_KP_PLUS))
		m_ZoomLevel -= 50;
	if(Input()->KeyPress(KEY_KP_MULTIPLY))
	{
		m_EditorOffsetX = 0;
		m_EditorOffsetY = 0;
		m_ZoomLevel = 100;
	}
	if(m_Dialog == DIALOG_NONE && !UI()->IsPopupActive() && UI()->MouseInside(&View))
	{
		// Determines in which direction to zoom.
		int Zoom = 0;
		if(Input()->KeyPress(KEY_MOUSE_WHEEL_UP))
			Zoom--;
		if(Input()->KeyPress(KEY_MOUSE_WHEEL_DOWN))
			Zoom++;

		if(Zoom != 0)
		{
			float OldLevel = m_ZoomLevel;
			m_ZoomLevel = clamp(m_ZoomLevel + Zoom * 20, 50, 2000);
			if(Config()->m_EdZoomTarget)
				ZoomMouseTarget((float) m_ZoomLevel / OldLevel);
		}
	}
	m_ZoomLevel = clamp(m_ZoomLevel, 50, 2000);
	m_WorldZoom = m_ZoomLevel / 100.0f;

	if(m_GuiActive)
	{
		float Brightness = 0.25f;
		RenderBackground(MenuBar, m_BackgroundTexture, 128.0f, Brightness * 0);
		MenuBar.Margin(2.0f, &MenuBar);

		RenderBackground(ToolBox, m_BackgroundTexture, 128.0f, Brightness);
		ToolBox.Margin(2.0f, &ToolBox);

		RenderBackground(ToolBar, m_BackgroundTexture, 128.0f, Brightness);
		ToolBar.Margin(2.0f, &ToolBar);
		ToolBar.VSplitLeft(100.0f, &CModeBar, &ToolBar);

		RenderBackground(StatusBar, m_BackgroundTexture, 128.0f, Brightness);
		StatusBar.Margin(2.0f, &StatusBar);

		// do the toolbar
		if(m_Mode == MODE_LAYERS)
			DoToolbar(ToolBar);

		if(m_ShowEnvelopeEditor)
		{
			RenderBackground(EnvelopeEditor, m_BackgroundTexture, 128.0f, Brightness);
			EnvelopeEditor.Margin(2.0f, &EnvelopeEditor);
		}
	}

	if(m_Mode == MODE_LAYERS && m_GuiActive)
		RenderLayers(ToolBox);
	else if(m_Mode == MODE_IMAGES)
	{
		if(m_GuiActive)
			RenderImagesList(ToolBox);
		RenderSelectedImage(View);
	}

	UI()->MapScreen();

	if(m_GuiActive)
	{
		RenderMenubar(MenuBar);

		RenderModebar(CModeBar);
		if(m_ShowEnvelopeEditor)
			RenderEnvelopeEditor(EnvelopeEditor);
	}

	if(m_Dialog == DIALOG_FILE)
	{
		static int s_NullUiTarget = 0;
		UI()->SetHotItem(&s_NullUiTarget);
		RenderFileDialog();
	}

	if(m_PopupEventActivated)
	{
		UI()->DoPopupMenu(Width / 2.0f - 200.0f, Height / 2.0f - 100.0f, 400.0f, 200.0f, this, PopupEvent);
		m_PopupEventActivated = false;
		m_PopupEventWasActivated = true;
	}

	UI()->RenderPopupMenus();

	if(m_GuiActive)
		RenderStatusbar(StatusBar);

	// todo: fix this
	if(Config()->m_EdShowkeys)
	{
		UI()->MapScreen();
		static CTextCursor s_Cursor(24.0f);
		s_Cursor.Reset();
		s_Cursor.MoveTo(View.x + 10, View.y + View.h - 24 - 10);

		int NKeys = 0;
		for(int i = 0; i < KEY_LAST; i++)
		{
			if(Input()->KeyIsPressed(i))
			{
				if(NKeys)
					TextRender()->TextDeferred(&s_Cursor, " + ", -1);
				TextRender()->TextDeferred(&s_Cursor, Input()->KeyName(i), -1);
				NKeys++;
			}
		}

		TextRender()->DrawTextOutlined(&s_Cursor);
	}

	if(m_ShowMousePointer)
	{
		float mx = UI()->MouseX();
		float my = UI()->MouseY();

		{
			// render butt ugly mouse cursor
			Graphics()->TextureSet(m_CursorTexture);
			Graphics()->WrapClamp();
			Graphics()->QuadsBegin();
			if(ms_pUiGotContext == UI()->HotItem())
				Graphics()->SetColor(1, 0, 0, 1);
			IGraphics::CQuadItem QuadItem(mx, my, 16.0f, 16.0f);
			Graphics()->QuadsDrawTL(&QuadItem, 1);
			Graphics()->QuadsEnd();
			Graphics()->WrapNormal();
		}

		if(m_MouseEdMode == MOUSE_PIPETTE)
		{
			// display selected color (pipette)
			Graphics()->TextureClear();
			Graphics()->QuadsBegin();
			Graphics()->SetColor(m_SelectedColor.r, m_SelectedColor.g, m_SelectedColor.b, m_SelectedColor.a);
			IGraphics::CQuadItem QuadItem(mx + 1.0f, my - 20.0f, 16.0f, 16.0f);
			Graphics()->QuadsDrawTL(&QuadItem, 1);
			Graphics()->QuadsEnd();
		}
	}
}

void CEditor::Reset(bool CreateDefault)
{
	m_Map.Clean();

	// create default layers
	if(CreateDefault)
		m_Map.CreateDefault();

	m_SelectedLayer = 0;
	m_SelectedGroup = 0;
	m_SelectedQuad = -1;
	m_SelectedPoints = 0;
	m_SelectedEnvelope = 0;
	m_SelectedImage = 0;

	m_WorldOffsetX = 0;
	m_WorldOffsetY = 0;
	m_EditorOffsetX = 0.0f;
	m_EditorOffsetY = 0.0f;

	m_WorldZoom = 1.0f;
	m_ZoomLevel = 200;

	m_MouseDeltaX = 0;
	m_MouseDeltaY = 0;
	m_MouseDeltaWx = 0;
	m_MouseDeltaWy = 0;

	m_Map.m_Modified = false;

	m_ShowEnvelopePreview = SHOWENV_NONE;
}

int CEditor::GetLineDistance() const
{
	int LineDistance = 512;

	if(m_ZoomLevel <= 100)
		LineDistance = 16;
	else if(m_ZoomLevel <= 250)
		LineDistance = 32;
	else if(m_ZoomLevel <= 450)
		LineDistance = 64;
	else if(m_ZoomLevel <= 850)
		LineDistance = 128;
	else if(m_ZoomLevel <= 1550)
		LineDistance = 256;

	return LineDistance;
}

void CEditor::ZoomMouseTarget(float ZoomFactor)
{
	// zoom to the current mouse position
	// get absolute mouse position
	float aPoints[4];
	RenderTools()->MapScreenToWorld(
		m_WorldOffsetX, m_WorldOffsetY,
		1.0f, 1.0f, 0.0f, 0.0f, Graphics()->ScreenAspect(), m_WorldZoom, aPoints);

	float WorldWidth = aPoints[2] - aPoints[0];
	float WorldHeight = aPoints[3] - aPoints[1];

	float Mwx = aPoints[0] + WorldWidth * (UI()->MouseX() / UI()->Screen()->w);
	float Mwy = aPoints[1] + WorldHeight * (UI()->MouseY() / UI()->Screen()->h);

	// adjust camera
	m_WorldOffsetX += (Mwx - m_WorldOffsetX) * (1 - ZoomFactor);
	m_WorldOffsetY += (Mwy - m_WorldOffsetY) * (1 - ZoomFactor);
}

void CEditorMap::DeleteEnvelope(int Index)
{
	if(Index < 0 || Index >= m_lEnvelopes.size())
		return;

	m_Modified = true;

	// fix links between envelopes and quads
	for(int i = 0; i < m_lGroups.size(); ++i)
		for(int j = 0; j < m_lGroups[i]->m_lLayers.size(); ++j)
			if(m_lGroups[i]->m_lLayers[j]->m_Type == LAYERTYPE_QUADS)
			{
				CLayerQuads *pLayer = static_cast<CLayerQuads *>(m_lGroups[i]->m_lLayers[j]);
				for(int k = 0; k < pLayer->m_lQuads.size(); ++k)
				{
					if(pLayer->m_lQuads[k].m_PosEnv == Index)
						pLayer->m_lQuads[k].m_PosEnv = -1;
					else if(pLayer->m_lQuads[k].m_PosEnv > Index)
						pLayer->m_lQuads[k].m_PosEnv--;
					if(pLayer->m_lQuads[k].m_ColorEnv == Index)
						pLayer->m_lQuads[k].m_ColorEnv = -1;
					else if(pLayer->m_lQuads[k].m_ColorEnv > Index)
						pLayer->m_lQuads[k].m_ColorEnv--;
				}
			}
			else if(m_lGroups[i]->m_lLayers[j]->m_Type == LAYERTYPE_TILES)
			{
				CLayerTiles *pLayer = static_cast<CLayerTiles *>(m_lGroups[i]->m_lLayers[j]);
				if(pLayer->m_ColorEnv == Index)
					pLayer->m_ColorEnv = -1;
				if(pLayer->m_ColorEnv > Index)
					pLayer->m_ColorEnv--;
			}

	m_lEnvelopes.remove_index(Index);
}

void CEditorMap::MakeGameLayer(CLayer *pLayer)
{
	m_pGameLayer = (CLayerGame *) pLayer;
	m_pGameLayer->m_pEditor = m_pEditor;
	m_pGameLayer->m_Texture = m_pEditor->m_EntitiesTexture;
}

void CEditorMap::MakeGameGroup(CLayerGroup *pGroup)
{
	m_pGameGroup = pGroup;
	m_pGameGroup->m_GameGroup = true;
	str_copy(m_pGameGroup->m_aName, "Game", sizeof(m_pGameGroup->m_aName));
}

void CEditorMap::Clean()
{
	m_lGroups.delete_all();
	m_lEnvelopes.delete_all();
	m_lImages.delete_all();

	m_MapInfo.Reset();

	m_pGameLayer = 0x0;
	m_pGameGroup = 0x0;

	m_Modified = false;
}

void CEditorMap::CreateDefault()
{
	// add background
	CLayerGroup *pGroup = NewGroup();
	pGroup->m_ParallaxX = 0;
	pGroup->m_ParallaxY = 0;
	CLayerQuads *pLayer = new CLayerQuads;
	pLayer->m_pEditor = m_pEditor;
	CQuad *pQuad = pLayer->NewQuad();
	const int Width = i2fx(800);
	const int Height = i2fx(600);
	pQuad->m_aPoints[0].x = pQuad->m_aPoints[2].x = -Width;
	pQuad->m_aPoints[1].x = pQuad->m_aPoints[3].x = Width;
	pQuad->m_aPoints[0].y = pQuad->m_aPoints[1].y = -Height;
	pQuad->m_aPoints[2].y = pQuad->m_aPoints[3].y = Height;
	pQuad->m_aPoints[4].x = pQuad->m_aPoints[4].y = 0;
	pQuad->m_aColors[0].r = pQuad->m_aColors[1].r = 94;
	pQuad->m_aColors[0].g = pQuad->m_aColors[1].g = 132;
	pQuad->m_aColors[0].b = pQuad->m_aColors[1].b = 174;
	pQuad->m_aColors[2].r = pQuad->m_aColors[3].r = 204;
	pQuad->m_aColors[2].g = pQuad->m_aColors[3].g = 232;
	pQuad->m_aColors[2].b = pQuad->m_aColors[3].b = 255;
	pGroup->AddLayer(pLayer);

	// add game layer
	MakeGameGroup(NewGroup());
	MakeGameLayer(new CLayerGame(50, 50));
	m_pGameGroup->AddLayer(m_pGameLayer);
}

void CEditor::Init()
{
	m_pInput = Kernel()->RequestInterface<IInput>();
	m_pClient = Kernel()->RequestInterface<IClient>();
	m_pConfig = Kernel()->RequestInterface<IConfigManager>()->Values();
	m_pConsole = Kernel()->RequestInterface<IConsole>();
	m_pGraphics = Kernel()->RequestInterface<IGraphics>();
	m_pTextRender = Kernel()->RequestInterface<ITextRender>();
	m_pStorage = Kernel()->RequestInterface<IStorage>();
	m_UI.Init(Kernel());
	m_RenderTools.Init(m_pConfig, m_pGraphics);
	m_Map.m_pEditor = this;

	m_CheckerTexture = Graphics()->LoadTexture("editor/checker.png", IStorage::TYPE_ALL, CImageInfo::FORMAT_AUTO, 0);
	m_BackgroundTexture = Graphics()->LoadTexture("editor/background.png", IStorage::TYPE_ALL, CImageInfo::FORMAT_AUTO, 0);
	m_CursorTexture = Graphics()->LoadTexture("editor/cursor.png", IStorage::TYPE_ALL, CImageInfo::FORMAT_AUTO, 0);
	m_EntitiesTexture = Graphics()->LoadTexture("editor/entities.png", IStorage::TYPE_ALL, CImageInfo::FORMAT_AUTO, IGraphics::TEXLOAD_MULTI_DIMENSION);

	m_TilesetPicker.m_pEditor = this;
	m_TilesetPicker.MakePalette();
	m_TilesetPicker.m_Readonly = true;

	m_QuadsetPicker.m_pEditor = this;
	m_QuadsetPicker.NewQuad();
	m_QuadsetPicker.m_Readonly = true;

	m_Brush.m_pMap = &m_Map;

	Reset();
	m_Map.m_Modified = false;

#ifdef CONF_DEBUG
	m_pConsole->Register("map_magic", "i", CFGFLAG_CLIENT, ConMapMagic, this, "1-grass_doodads, 2-winter_main, 3-both");
#endif
}

static int s_GrassDoodadsIndicesOld[] = {42, 43, 44, 58, 59, 60, 74, 75, 76, 132, 133, 148, 149, 163, 164, 165, 169, 170, 185, 186};
static int s_GrassDoodadsIndicesNew[] = {217, 218, 219, 233, 234, 235, 249, 250, 251, 182, 183, 198, 199, 213, 214, 215, 184, 185, 200, 201};
static int s_WinterMainIndicesOld[] = {166, 167, 168, 169, 170, 171, 182, 183, 184, 185, 186, 187, 198, 199, 200, 201, 202, 203, 174, 177, 178, 179, 180, 193, 194, 195, 196, 197, 209, 210, 211, 212, 215, 216, 231, 232, 248, 249, 250, 251, 252, 253, 254, 255, 229, 230, 224, 225, 226, 227, 228};
static int s_WinterMainIndicesNew[] = {218, 219, 220, 221, 222, 223, 234, 235, 236, 237, 238, 239, 250, 251, 252, 253, 254, 255, 95, 160, 161, 162, 163, 192, 193, 194, 195, 196, 176, 177, 178, 179, 232, 233, 248, 249, 240, 241, 242, 243, 244, 245, 246, 247, 224, 225, 226, 227, 228, 229, 230};

void CEditor::ConMapMagic(IConsole::IResult *pResult, void *pUserData)
{
}

void CEditor::DoMapMagic(int ImageID, int SrcIndex)
{
	for(int g = 0; g < m_Map.m_lGroups.size(); g++)
	{
		for(int i = 0; i < m_Map.m_lGroups[g]->m_lLayers.size(); i++)
		{
			if(m_Map.m_lGroups[g]->m_lLayers[i]->m_Type == LAYERTYPE_TILES)
			{
				CLayerTiles *pLayer = static_cast<CLayerTiles *>(m_Map.m_lGroups[g]->m_lLayers[i]);
				if(pLayer->m_Image == ImageID)
				{
					for(int Count = 0; Count < pLayer->m_Height * pLayer->m_Width; ++Count)
					{
						if(SrcIndex == 0) // grass_doodads
						{
							for(unsigned TileIndex = 0; TileIndex < sizeof(s_GrassDoodadsIndicesOld) / sizeof(s_GrassDoodadsIndicesOld[0]); ++TileIndex)
							{
								if(pLayer->m_pTiles[Count].m_Index == s_GrassDoodadsIndicesOld[TileIndex])
								{
									pLayer->m_pTiles[Count].m_Index = s_GrassDoodadsIndicesNew[TileIndex];
									break;
								}
							}
						}
						else if(SrcIndex == 1) // winter_main
						{
							for(unsigned TileIndex = 0; TileIndex < sizeof(s_WinterMainIndicesOld) / sizeof(s_WinterMainIndicesOld[0]); ++TileIndex)
							{
								if(pLayer->m_pTiles[Count].m_Index == s_WinterMainIndicesOld[TileIndex])
								{
									pLayer->m_pTiles[Count].m_Index = s_WinterMainIndicesNew[TileIndex];
									break;
								}
							}
						}
					}
				}
			}
		}
	}
}

void CEditor::DoMapBorder()
{
	CLayerTiles *pT = (CLayerTiles *) GetSelectedLayerType(0, LAYERTYPE_TILES);

	for(int i = 0; i < pT->m_Width * 2; ++i)
		pT->m_pTiles[i].m_Index = 1;

	for(int i = 0; i < pT->m_Width * pT->m_Height; ++i)
	{
		if(i % pT->m_Width < 2 || i % pT->m_Width > pT->m_Width - 3)
			pT->m_pTiles[i].m_Index = 1;
	}

	for(int i = (pT->m_Width * (pT->m_Height - 2)); i < pT->m_Width * pT->m_Height; ++i)
		pT->m_pTiles[i].m_Index = 1;
}

void CEditor::OnUpdate()
{
	CUIElementBase::Init(UI()); // update static pointer because game and editor use separate UI

	for(int i = 0; i < Input()->NumEvents(); i++)
		UI()->OnInput(Input()->GetEvent(i));

	// handle cursor movement
	{
		static float s_MouseX = 0.0f;
		static float s_MouseY = 0.0f;
		static float s_MouseDeltaX = 0.0f;
		static float s_MouseDeltaY = 0.0f;

		float MouseRelX = 0.0f, MouseRelY = 0.0f;
		int CursorType = Input()->CursorRelative(&MouseRelX, &MouseRelY);
		if(CursorType != IInput::CURSOR_NONE)
			UI()->ConvertCursorMove(&MouseRelX, &MouseRelY, CursorType);

		m_MouseDeltaX = MouseRelX;
		m_MouseDeltaY = MouseRelY;

		if(!m_LockMouse)
		{
			s_MouseX += MouseRelX;
			s_MouseY += MouseRelY;
		}
		s_MouseX = clamp<float>(s_MouseX, 0.0f, Graphics()->ScreenWidth());
		s_MouseY = clamp<float>(s_MouseY, 0.0f, Graphics()->ScreenHeight());

		// update positions for ui, but only update ui when rendering
		m_MouseX = UI()->Screen()->w * (s_MouseX / (float) Graphics()->ScreenWidth());
		m_MouseY = UI()->Screen()->h * (s_MouseY / (float) Graphics()->ScreenHeight());
		s_MouseDeltaX = UI()->Screen()->w * (m_MouseDeltaX / (float) Graphics()->ScreenWidth());
		s_MouseDeltaY = UI()->Screen()->h * (m_MouseDeltaY / (float) Graphics()->ScreenHeight());

		// fix correct world x and y
		CLayerGroup *pSelectedGroup = GetSelectedGroup();
		if(pSelectedGroup)
		{
			float aPoints[4];
			pSelectedGroup->Mapping(aPoints);

			float WorldWidth = aPoints[2] - aPoints[0];
			float WorldHeight = aPoints[3] - aPoints[1];

			m_MouseWorldX = aPoints[0] + WorldWidth * (m_MouseX / UI()->Screen()->w);
			m_MouseWorldY = aPoints[1] + WorldHeight * (m_MouseY / UI()->Screen()->h);
			m_MouseDeltaWx = s_MouseDeltaX * (WorldWidth / UI()->Screen()->w);
			m_MouseDeltaWy = s_MouseDeltaY * (WorldHeight / UI()->Screen()->h);
		}
		else
		{
			m_MouseWorldX = 0.0f;
			m_MouseWorldY = 0.0f;
		}
	}
}

void CEditor::OnRender()
{
	// toggle gui
	if(Input()->KeyPress(KEY_TAB))
		m_GuiActive = !m_GuiActive;

	if(Input()->KeyPress(KEY_F10))
		m_ShowMousePointer = false;

	if(m_Animate)
		m_AnimateTime = (time_get() - m_AnimateStart) / (float) time_freq();
	else
		m_AnimateTime = 0;

	ms_pUiGotContext = 0;
	UI()->StartCheck();

	UI()->Update(m_MouseX, m_MouseY, m_MouseWorldX, m_MouseWorldY);

	Render();

	m_MouseDeltaX = 0.0f;
	m_MouseDeltaY = 0.0f;
	m_MouseDeltaWx = 0.0f;
	m_MouseDeltaWy = 0.0f;

	if(Input()->KeyPress(KEY_F10))
	{
		Graphics()->TakeScreenshot(0);
		m_ShowMousePointer = true;
	}

	UI()->FinishCheck();
	UI()->ClearHotkeys();
	Input()->Clear();

	CLineInput::RenderCandidates();
}

IEditor *CreateEditor() { return new CEditor; }
