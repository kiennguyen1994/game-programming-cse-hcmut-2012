// Copyright (C) 2002-2011 Nikolaus Gebhardt
// This file is part of the "Irrlicht Engine".
// For conditions of distribution and use, see copyright notice in irrlicht.h

#include "CGUIEditBox.h"

#include "IGUISkin.h"
#include "IGUIEnvironment.h"
#include "IGUIFont.h"
#include "IVideoDriver.h"
#include "rect.h"
//#include "os.h"
#include "Keycodes.h"

#include "graphics/irr_driver.hpp"
#include "utils/translation.hpp"
#include "utils/time.hpp"

/*
	todo:
	optional scrollbars
	ctrl+left/right to select word
	double click/ctrl click: word select + drag to select whole words, triple click to select line
	optional? dragging selected text
	numerical
*/


Time::TimeType getTime()
{
    return Time::getTimeSinceEpoch();
}

//! constructor
CGUIEditBox::CGUIEditBox(const wchar_t* text, bool border,
		IGUIEnvironment* environment, IGUIElement* parent, s32 id,
		const core::rect<s32>& rectangle, bool is_rtl)
	: IGUIEditBox(environment, parent, id, rectangle), MouseMarking(false),
	Border(border), OverrideColorEnabled(false), MarkBegin(0), MarkEnd(0),
	OverrideColor(video::SColor(101,255,255,255)), OverrideFont(0), LastBreakFont(0),
	Operator(0), BlinkStartTime(0), CursorPos(0), HScrollPos(0), VScrollPos(0), Max(0),
	WordWrap(false), MultiLine(false), AutoScroll(true), PasswordBox(false),
	PasswordChar(L'*'), HAlign(EGUIA_UPPERLEFT), VAlign(EGUIA_CENTER),
	CurrentTextRect(0,0,1,1), FrameRect(rectangle)
{
    m_rtl = is_rtl;
    
	#ifdef _DEBUG
	setDebugName("CGUIEditBox");
	#endif

	Text = text;

	if (Environment)
		Operator = Environment->getOSOperator();

	if (Operator)
		Operator->grab();

	// this element can be tabbed to
	setTabStop(true);
	setTabOrder(-1);

	IGUISkin *skin = 0;
	if (Environment)
		skin = Environment->getSkin();
	if (Border && skin)
	{
		FrameRect.UpperLeftCorner.X += skin->getSize(EGDS_TEXT_DISTANCE_X)+1;
		FrameRect.UpperLeftCorner.Y += skin->getSize(EGDS_TEXT_DISTANCE_Y)+1;
		FrameRect.LowerRightCorner.X -= skin->getSize(EGDS_TEXT_DISTANCE_X)+1;
		FrameRect.LowerRightCorner.Y -= skin->getSize(EGDS_TEXT_DISTANCE_Y)+1;
	}

	breakText();

	calculateScrollPos();
}


//! destructor
CGUIEditBox::~CGUIEditBox()
{
	if (OverrideFont)
		OverrideFont->drop();

	if (Operator)
		Operator->drop();
}


//! Sets another skin independent font.
void CGUIEditBox::setOverrideFont(IGUIFont* font)
{
	if (OverrideFont == font)
		return;

	if (OverrideFont)
		OverrideFont->drop();

	OverrideFont = font;

	if (OverrideFont)
		OverrideFont->grab();

	breakText();
}


//! Sets another color for the text.
void CGUIEditBox::setOverrideColor(video::SColor color)
{
	OverrideColor = color;
	OverrideColorEnabled = true;
}


video::SColor CGUIEditBox::getOverrideColor() const
{
	return OverrideColor;
}


//! Turns the border on or off
void CGUIEditBox::setDrawBorder(bool border)
{
	Border = border;
}


//! Sets if the text should use the overide color or the color in the gui skin.
void CGUIEditBox::enableOverrideColor(bool enable)
{
	OverrideColorEnabled = enable;
}

bool CGUIEditBox::isOverrideColorEnabled() const
{
	_IRR_IMPLEMENT_MANAGED_MARSHALLING_BUGFIX;
	return OverrideColorEnabled;
}

//! Enables or disables word wrap
void CGUIEditBox::setWordWrap(bool enable)
{
	WordWrap = enable;
	breakText();
}


void CGUIEditBox::updateAbsolutePosition()
{
    core::rect<s32> oldAbsoluteRect(AbsoluteRect);
	IGUIElement::updateAbsolutePosition();
	if ( oldAbsoluteRect != AbsoluteRect )
	{
        breakText();
	}
}


//! Checks if word wrap is enabled
bool CGUIEditBox::isWordWrapEnabled() const
{
	_IRR_IMPLEMENT_MANAGED_MARSHALLING_BUGFIX;
	return WordWrap;
}


//! Enables or disables newlines.
void CGUIEditBox::setMultiLine(bool enable)
{
	MultiLine = enable;
}


//! Checks if multi line editing is enabled
bool CGUIEditBox::isMultiLineEnabled() const
{
	_IRR_IMPLEMENT_MANAGED_MARSHALLING_BUGFIX;
	return MultiLine;
}


void CGUIEditBox::setPasswordBox(bool passwordBox, wchar_t passwordChar)
{
	PasswordBox = passwordBox;
	if (PasswordBox)
	{
		PasswordChar = passwordChar;
		setMultiLine(false);
		setWordWrap(false);
		BrokenText.clear();
	}
}


bool CGUIEditBox::isPasswordBox() const
{
	_IRR_IMPLEMENT_MANAGED_MARSHALLING_BUGFIX;
	return PasswordBox;
}


//! Sets text justification
void CGUIEditBox::setTextAlignment(EGUI_ALIGNMENT horizontal, EGUI_ALIGNMENT vertical)
{
	HAlign = horizontal;
	VAlign = vertical;
}


//! called if an event happened.
bool CGUIEditBox::OnEvent(const SEvent& event)
{
	if (isEnabled())
	{

		switch(event.EventType)
		{
		case EET_GUI_EVENT:
			if (event.GUIEvent.EventType == EGET_ELEMENT_FOCUS_LOST)
			{
				if (event.GUIEvent.Caller == this)
				{
					MouseMarking = false;
					setTextMarkers(0,0);
				}
			}
			break;
		case EET_KEY_INPUT_EVENT:
			if (processKey(event))
				return true;
			break;
		case EET_MOUSE_INPUT_EVENT:
			if (processMouse(event))
				return true;
			break;
		default:
			break;
		}
	}

	return IGUIElement::OnEvent(event);
}


bool CGUIEditBox::processKey(const SEvent& event)
{
	if (!event.KeyInput.PressedDown)
		return false;

	bool textChanged = false;
	s32 newMarkBegin = MarkBegin;
	s32 newMarkEnd = MarkEnd;

	// control shortcut handling

	if (event.KeyInput.Control)
	{
		// german backlash '\' entered with control + '?'
		if ( event.KeyInput.Char == '\\' )
		{
			inputChar(event.KeyInput.Char);
			return true;
		}

		switch(event.KeyInput.Key)
		{
		case KEY_KEY_A:
			// select all
			newMarkBegin = 0;
			newMarkEnd = Text.size();
			break;
		case KEY_KEY_C:
			// copy to clipboard
			if (!PasswordBox && Operator && MarkBegin != MarkEnd)
			{
				const s32 realmbgn = MarkBegin < MarkEnd ? MarkBegin : MarkEnd;
				const s32 realmend = MarkBegin < MarkEnd ? MarkEnd : MarkBegin;

				core::stringc s;
				s = Text.subString(realmbgn, realmend - realmbgn).c_str();
				Operator->copyToClipboard(s.c_str());
			}
			break;
		case KEY_KEY_X:
			// cut to the clipboard
			if (!PasswordBox && Operator && MarkBegin != MarkEnd)
			{
				const s32 realmbgn = MarkBegin < MarkEnd ? MarkBegin : MarkEnd;
				const s32 realmend = MarkBegin < MarkEnd ? MarkEnd : MarkBegin;

				// copy
				core::stringc sc;
				sc = Text.subString(realmbgn, realmend - realmbgn).c_str();
				Operator->copyToClipboard(sc.c_str());

				if (isEnabled())
				{
					// delete
					core::stringw s;
					s = Text.subString(0, realmbgn);
					s.append( Text.subString(realmend, Text.size()-realmend) );
					Text = s;

					CursorPos = realmbgn;
					newMarkBegin = 0;
					newMarkEnd = 0;
					textChanged = true;
				}
			}
			break;
                
		case KEY_KEY_V:
			if ( !isEnabled() )
				break;

			// paste from the clipboard
			if (Operator)
			{
				const s32 realmbgn = MarkBegin < MarkEnd ? MarkBegin : MarkEnd;
				const s32 realmend = MarkBegin < MarkEnd ? MarkEnd : MarkBegin;

				// add new character
				const c8* p = Operator->getTextFromClipboard();
				if (p)
				{
					if (MarkBegin == MarkEnd)
					{
						// insert text
						core::stringw s = Text.subString(0, CursorPos);
						s.append(p);
						s.append( Text.subString(CursorPos, Text.size()-CursorPos) );

						if (!Max || s.size()<=Max) // thx to Fish FH for fix
						{
							Text = s;
							s = p;
							CursorPos += s.size();
						}
					}
					else
					{
						// replace text

						core::stringw s = Text.subString(0, realmbgn);
						s.append(p);
						s.append( Text.subString(realmend, Text.size()-realmend) );

						if (!Max || s.size()<=Max)  // thx to Fish FH for fix
						{
							Text = s;
							s = p;
							CursorPos = realmbgn + s.size();
						}
					}
				}

				newMarkBegin = 0;
				newMarkEnd = 0;
				textChanged = true;
			}
			break;
		case KEY_HOME:
            if (!m_rtl)
            {
                // move/highlight to start of text
                if (event.KeyInput.Shift)
                {
                    newMarkEnd = CursorPos;
                    newMarkBegin = 0;
                    CursorPos = 0;
                }
                else
                {
                    CursorPos = 0;
                    newMarkBegin = 0;
                    newMarkEnd = 0;
                }
            }
			break;
		case KEY_END:
            if (!m_rtl)
            {
                // move/highlight to end of text
                if (event.KeyInput.Shift)
                {
                    newMarkBegin = CursorPos;
                    newMarkEnd = Text.size();
                    CursorPos = 0;
                }
                else
                {
                    CursorPos = Text.size();
                    newMarkBegin = 0;
                    newMarkEnd = 0;
                }
            }
			break;
		default:
			return false;
		}
	}
	// default keyboard handling
	else
	switch(event.KeyInput.Key)
	{
            /*
        case KEY_KEY_Q:
            inputChar(L'\u05DC');
            textChanged = true;
            return true;
        case KEY_KEY_W:
            inputChar(L'\u05DB');
            textChanged = true;
            return true;
        case KEY_KEY_E:
            inputChar(L'\u05DA');
            textChanged = true;
            return true;
        case KEY_KEY_R:
            inputChar(L'\u05D9');
            textChanged = true;
            return true;
        case KEY_KEY_T:
            inputChar(L'\u05D8');
            textChanged = true;
            return true;
        case KEY_KEY_Y:
            inputChar(L'\u05D7');
            textChanged = true;
            return true;
            */
            
	case KEY_END:
		if (!m_rtl)
        {
			s32 p = Text.size();
			if (WordWrap || MultiLine)
			{
				p = getLineFromPos(CursorPos);
				p = BrokenTextPositions[p] + (s32)BrokenText[p].size();
				if (p > 0 && (Text[p-1] == L'\r' || Text[p-1] == L'\n' ))
					p-=1;
			}

			if (event.KeyInput.Shift)
			{
				if (MarkBegin == MarkEnd)
					newMarkBegin = CursorPos;

				newMarkEnd = p;
			}
			else
			{
				newMarkBegin = 0;
				newMarkEnd = 0;
			}
			CursorPos = p;
			BlinkStartTime = getTime();
		}
		break;
	case KEY_HOME:
        if (!m_rtl)
		{

			s32 p = 0;
			if (WordWrap || MultiLine)
			{
				p = getLineFromPos(CursorPos);
				p = BrokenTextPositions[p];
			}

			if (event.KeyInput.Shift)
			{
				if (MarkBegin == MarkEnd)
					newMarkBegin = CursorPos;
				newMarkEnd = p;
			}
			else
			{
				newMarkBegin = 0;
				newMarkEnd = 0;
			}
			CursorPos = p;
			BlinkStartTime = getTime();
		}
		break;
	case KEY_RETURN:
		if (MultiLine)
		{
			inputChar(L'\n');
			return true;
		}
		else
		{
		    sendGuiEvent( EGET_EDITBOX_ENTER );
		}
		break;
	case KEY_LEFT:
        if (!m_rtl)
        {
            if (event.KeyInput.Shift)
            {
                if (CursorPos > 0)
                {
                    if (MarkBegin == MarkEnd)
                        newMarkBegin = CursorPos;

                    newMarkEnd = CursorPos-1;
                }
            }
            else
            {
                newMarkBegin = 0;
                newMarkEnd = 0;
            }

            if (CursorPos > 0) CursorPos--;
            BlinkStartTime = getTime();
        }
		break;

	case KEY_RIGHT:
        if (!m_rtl)
        {
            if (event.KeyInput.Shift)
            {
                if (Text.size() > (u32)CursorPos)
                {
                    if (MarkBegin == MarkEnd)
                        newMarkBegin = CursorPos;

                    newMarkEnd = CursorPos+1;
                }
            }
            else
            {
                newMarkBegin = 0;
                newMarkEnd = 0;
            }

            if (Text.size() > (u32)CursorPos) CursorPos++;
            BlinkStartTime = getTime();
        }
		break;
	case KEY_UP:
		if (MultiLine || (WordWrap && BrokenText.size() > 1) )
		{
			s32 lineNo = getLineFromPos(CursorPos);
			s32 mb = (MarkBegin == MarkEnd) ? CursorPos : (MarkBegin > MarkEnd ? MarkBegin : MarkEnd);
			if (lineNo > 0)
			{
				s32 cp = CursorPos - BrokenTextPositions[lineNo];
				if ((s32)BrokenText[lineNo-1].size() < cp)
					CursorPos = BrokenTextPositions[lineNo-1] + (s32)BrokenText[lineNo-1].size()-1;
				else
					CursorPos = BrokenTextPositions[lineNo-1] + cp;
			}

			if (event.KeyInput.Shift)
			{
				newMarkBegin = mb;
				newMarkEnd = CursorPos;
			}
			else
			{
				newMarkBegin = 0;
				newMarkEnd = 0;
			}

		}
		else
		{
			return false;
		}
		break;
	case KEY_DOWN:
		if (MultiLine || (WordWrap && BrokenText.size() > 1) )
		{
			s32 lineNo = getLineFromPos(CursorPos);
			s32 mb = (MarkBegin == MarkEnd) ? CursorPos : (MarkBegin < MarkEnd ? MarkBegin : MarkEnd);
			if (lineNo < (s32)BrokenText.size()-1)
			{
				s32 cp = CursorPos - BrokenTextPositions[lineNo];
				if ((s32)BrokenText[lineNo+1].size() < cp)
					CursorPos = BrokenTextPositions[lineNo+1] + BrokenText[lineNo+1].size()-1;
				else
					CursorPos = BrokenTextPositions[lineNo+1] + cp;
			}

			if (event.KeyInput.Shift)
			{
				newMarkBegin = mb;
				newMarkEnd = CursorPos;
			}
			else
			{
				newMarkBegin = 0;
				newMarkEnd = 0;
			}

		}
		else
		{
			return false;
		}
		break;

	case KEY_BACK:
		if ( !isEnabled() )
			break;

		if (Text.size())
		{
			core::stringw s;

			if (MarkBegin != MarkEnd)
			{
				// delete marked text
				const s32 realmbgn = MarkBegin < MarkEnd ? MarkBegin : MarkEnd;
				const s32 realmend = MarkBegin < MarkEnd ? MarkEnd : MarkBegin;

				s = Text.subString(0, realmbgn);
				s.append( Text.subString(realmend, Text.size()-realmend) );
				Text = s;

				CursorPos = realmbgn;
			}
			else
			{
				// delete text behind cursor
				if (CursorPos>0)
					s = Text.subString(0, CursorPos-1);
				else
					s = L"";
				s.append( Text.subString(CursorPos, Text.size()-CursorPos) );
				Text = s;
				--CursorPos;
			}

			if (CursorPos < 0)
				CursorPos = 0;
			BlinkStartTime = getTime();
			newMarkBegin = 0;
			newMarkEnd = 0;
			textChanged = true;
		}
		break;
	case KEY_DELETE:
		if ( !isEnabled() )
			break;

		if (Text.size() != 0)
		{
			core::stringw s;

			if (MarkBegin != MarkEnd)
			{
				// delete marked text
				const s32 realmbgn = MarkBegin < MarkEnd ? MarkBegin : MarkEnd;
				const s32 realmend = MarkBegin < MarkEnd ? MarkEnd : MarkBegin;

				s = Text.subString(0, realmbgn);
				s.append( Text.subString(realmend, Text.size()-realmend) );
				Text = s;

				CursorPos = realmbgn;
			}
			else
			{
				// delete text before cursor
				s = Text.subString(0, CursorPos);
				s.append( Text.subString(CursorPos+1, Text.size()-CursorPos-1) );
				Text = s;
			}

			if (CursorPos > (s32)Text.size())
				CursorPos = (s32)Text.size();

			BlinkStartTime = getTime();
			newMarkBegin = 0;
			newMarkEnd = 0;
			textChanged = true;
		}
		break;

	case KEY_ESCAPE:
	case KEY_TAB:
	case KEY_SHIFT:
	case KEY_F1:
	case KEY_F2:
	case KEY_F3:
	case KEY_F4:
	case KEY_F5:
	case KEY_F6:
	case KEY_F7:
	case KEY_F8:
	case KEY_F9:
	case KEY_F10:
	case KEY_F11:
	case KEY_F12:
	case KEY_F13:
	case KEY_F14:
	case KEY_F15:
	case KEY_F16:
	case KEY_F17:
	case KEY_F18:
	case KEY_F19:
	case KEY_F20:
	case KEY_F21:
	case KEY_F22:
	case KEY_F23:
	case KEY_F24:
		// ignore these keys
		return false;

	default:
		inputChar(event.KeyInput.Char);
		return true;
	}

    // Set new text markers
    setTextMarkers( newMarkBegin, newMarkEnd );

	// break the text if it has changed
	if (textChanged)
	{
		breakText();
		sendGuiEvent(EGET_EDITBOX_CHANGED);
	}

	calculateScrollPos();

	return true;
}


//! draws the element and its children
void CGUIEditBox::draw()
{
	if (!IsVisible)
		return;

	const bool focus = Environment->hasFocus(this);

	IGUISkin* skin = Environment->getSkin();
	if (!skin)
		return;

	FrameRect = AbsoluteRect;

	// draw the border

	if (Border)
	{
		EGUI_DEFAULT_COLOR col = EGDC_GRAY_EDITABLE;
		if ( isEnabled() )
			col = focus ? EGDC_FOCUSED_EDITABLE : EGDC_EDITABLE;
		skin->draw3DSunkenPane(this, skin->getColor(col),
			false, true, FrameRect, &AbsoluteClippingRect);

		FrameRect.UpperLeftCorner.X += skin->getSize(EGDS_TEXT_DISTANCE_X)+1;
		FrameRect.UpperLeftCorner.Y += skin->getSize(EGDS_TEXT_DISTANCE_Y)+1;
		FrameRect.LowerRightCorner.X -= skin->getSize(EGDS_TEXT_DISTANCE_X)+1;
		FrameRect.LowerRightCorner.Y -= skin->getSize(EGDS_TEXT_DISTANCE_Y)+1;
	}
	core::rect<s32> localClipRect = FrameRect;
	localClipRect.clipAgainst(AbsoluteClippingRect);

	// draw the text

	IGUIFont* font = OverrideFont;
	if (!OverrideFont)
		font = skin->getFont();

	s32 cursorLine = 0;
	s32 charcursorpos = 0;

	if (font)
	{
		if (LastBreakFont != font)
		{
			breakText();
		}

		// calculate cursor pos

		core::stringw *txtLine = &Text;
		s32 startPos = 0;

		core::stringw s, s2;

		// get mark position
		const bool ml = (!PasswordBox && (WordWrap || MultiLine));
		const s32 realmbgn = MarkBegin < MarkEnd ? MarkBegin : MarkEnd;
		const s32 realmend = MarkBegin < MarkEnd ? MarkEnd : MarkBegin;
		const s32 hlineStart = ml ? getLineFromPos(realmbgn) : 0;
		const s32 hlineCount = ml ? getLineFromPos(realmend) - hlineStart + 1 : 1;
		const s32 lineCount = ml ? BrokenText.size() : 1;

		// Save the override color information.
		// Then, alter it if the edit box is disabled.
		const bool prevOver = OverrideColorEnabled;
		const video::SColor prevColor = OverrideColor;

		if (Text.size())
		{
			if (!isEnabled() && !OverrideColorEnabled)
			{
				OverrideColorEnabled = true;
				OverrideColor = skin->getColor(EGDC_GRAY_TEXT);
			}

			for (s32 i=0; i < lineCount; ++i)
			{
				setTextRect(i);

				// clipping test - don't draw anything outside the visible area
				core::rect<s32> c = localClipRect;
				c.clipAgainst(CurrentTextRect);
				if (!c.isValid())
					continue;

				// get current line
				if (PasswordBox)
				{
					if (BrokenText.size() != 1)
					{
						BrokenText.clear();
						BrokenText.push_back(core::stringw());
					}
					if (BrokenText[0].size() != Text.size())
					{
						BrokenText[0] = Text;
						for (u32 q = 0; q < Text.size(); ++q)
						{
							BrokenText[0] [q] = PasswordChar;
						}
					}
					txtLine = &BrokenText[0];
					startPos = 0;
				}
				else
				{
					txtLine = ml ? &BrokenText[i] : &Text;
					startPos = ml ? BrokenTextPositions[i] : 0;
				}


                if (m_rtl)
                {
                    font->draw(translations->fribidize(txtLine->c_str()), CurrentTextRect,
                               OverrideColorEnabled ? OverrideColor : skin->getColor(EGDC_BUTTON_TEXT),
                               false, true, &localClipRect);
                }
                else
                {
                    // draw normal text
                    font->draw(txtLine->c_str(), CurrentTextRect,
                        OverrideColorEnabled ? OverrideColor : skin->getColor(EGDC_BUTTON_TEXT),
                        false, true, &localClipRect);
                }
                
				// draw mark and marked text
				if (focus && MarkBegin != MarkEnd && i >= hlineStart && i < hlineStart + hlineCount)
				{

					s32 mbegin = 0, mend = 0;
					s32 lineStartPos = 0, lineEndPos = txtLine->size();

					if (i == hlineStart)
					{
						// highlight start is on this line
						s = txtLine->subString(0, realmbgn - startPos);
						mbegin = font->getDimension(s.c_str()).Width;

						// deal with kerning
						mbegin += font->getKerningWidth(
							&((*txtLine)[realmbgn - startPos]),
							realmbgn - startPos > 0 ? &((*txtLine)[realmbgn - startPos - 1]) : 0);

						lineStartPos = realmbgn - startPos;
					}
					if (i == hlineStart + hlineCount - 1)
					{
						// highlight end is on this line
						s2 = txtLine->subString(0, realmend - startPos);
						mend = font->getDimension(s2.c_str()).Width;
						lineEndPos = (s32)s2.size();
					}
					else
						mend = font->getDimension(txtLine->c_str()).Width;

					CurrentTextRect.UpperLeftCorner.X += mbegin;
					CurrentTextRect.LowerRightCorner.X = CurrentTextRect.UpperLeftCorner.X + mend - mbegin;

					// draw mark
					skin->draw2DRectangle(this, skin->getColor(EGDC_HIGH_LIGHT), CurrentTextRect, &localClipRect);

					// draw marked text
					s = txtLine->subString(lineStartPos, lineEndPos - lineStartPos);

					if (s.size())
						font->draw(s.c_str(), CurrentTextRect,
							OverrideColorEnabled ? OverrideColor : skin->getColor(EGDC_HIGH_LIGHT_TEXT),
							false, true, &localClipRect);

				}
			}

			// Return the override color information to its previous settings.
			OverrideColorEnabled = prevOver;
			OverrideColor = prevColor;
		}

		// draw cursor

		if (WordWrap || MultiLine)
		{
			cursorLine = getLineFromPos(CursorPos);
			txtLine = &BrokenText[cursorLine];
			startPos = BrokenTextPositions[cursorLine];
		}
		s = txtLine->subString(0,CursorPos-startPos);
		charcursorpos = font->getDimension(s.c_str()).Width ;
        // + font->getKerningWidth(L"_", CursorPos-startPos > 0 ? &((*txtLine)[CursorPos-startPos-1]) : 0);

		if (focus && (getTime() - BlinkStartTime) % 2 == 0 && !m_rtl)
		{
			//setTextRect(cursorLine);
			//CurrentTextRect.UpperLeftCorner.X += charcursorpos;

            setTextRect(0);
            
            core::rect< s32 > caret_rect = CurrentTextRect;
            caret_rect.UpperLeftCorner.X += charcursorpos - 1;
            caret_rect.LowerRightCorner.X = caret_rect.UpperLeftCorner.X + 2;
            irr_driver->getVideoDriver()->draw2DRectangle( video::SColor(255,0,0,0), caret_rect );
            
            /*
			font->draw(L"_", CurrentTextRect,
				OverrideColorEnabled ? OverrideColor : skin->getColor(EGDC_BUTTON_TEXT),
				false, true, &localClipRect);
             */
		}
	}

	// draw children
	IGUIElement::draw();
}


//! Sets the new caption of this element.
void CGUIEditBox::setText(const wchar_t* text)
{
	Text = text;
	if (u32(CursorPos) > Text.size())
		CursorPos = Text.size();
	HScrollPos = 0;
	breakText();
}


//! Enables or disables automatic scrolling with cursor position
//! \param enable: If set to true, the text will move around with the cursor position
void CGUIEditBox::setAutoScroll(bool enable)
{
	AutoScroll = enable;
}


//! Checks to see if automatic scrolling is enabled
//! \return true if automatic scrolling is enabled, false if not
bool CGUIEditBox::isAutoScrollEnabled() const
{
	_IRR_IMPLEMENT_MANAGED_MARSHALLING_BUGFIX;
	return AutoScroll;
}


//! Gets the area of the text in the edit box
//! \return Returns the size in pixels of the text
core::dimension2du CGUIEditBox::getTextDimension()
{
	core::rect<s32> ret;

	setTextRect(0);
	ret = CurrentTextRect;

	for (u32 i=1; i < BrokenText.size(); ++i)
	{
		setTextRect(i);
		ret.addInternalPoint(CurrentTextRect.UpperLeftCorner);
		ret.addInternalPoint(CurrentTextRect.LowerRightCorner);
	}

	return core::dimension2du(ret.getSize());
}


//! Sets the maximum amount of characters which may be entered in the box.
//! \param max: Maximum amount of characters. If 0, the character amount is
//! infinity.
void CGUIEditBox::setMax(u32 max)
{
	Max = max;

	if (Text.size() > Max && Max != 0)
		Text = Text.subString(0, Max);
}


//! Returns maximum amount of characters, previously set by setMax();
u32 CGUIEditBox::getMax() const
{
	return Max;
}


bool CGUIEditBox::processMouse(const SEvent& event)
{
	switch(event.MouseInput.Event)
	{
	case irr::EMIE_LMOUSE_LEFT_UP:
		if (Environment->hasFocus(this) && !m_rtl)
		{
			CursorPos = getCursorPos(event.MouseInput.X, event.MouseInput.Y);
			if (MouseMarking)
			{
			    setTextMarkers( MarkBegin, CursorPos );
			}
			MouseMarking = false;
			calculateScrollPos();
			return true;
		}
		break;
	case irr::EMIE_MOUSE_MOVED:
		{
			if (MouseMarking)
			{
				CursorPos = getCursorPos(event.MouseInput.X, event.MouseInput.Y);
				setTextMarkers( MarkBegin, CursorPos );
				calculateScrollPos();
				return true;
			}
		}
		break;
	case EMIE_LMOUSE_PRESSED_DOWN:
		if (!Environment->hasFocus(this))
		{
			BlinkStartTime = getTime();
			MouseMarking = true;
			CursorPos = getCursorPos(event.MouseInput.X, event.MouseInput.Y);
			setTextMarkers(CursorPos, CursorPos );
			calculateScrollPos();
			return true;
		}
		else if (!m_rtl)
		{
			if (!AbsoluteClippingRect.isPointInside(
				core::position2d<s32>(event.MouseInput.X, event.MouseInput.Y)))
			{
				return false;
			}
			else
			{
				// move cursor
				CursorPos = getCursorPos(event.MouseInput.X, event.MouseInput.Y);

                s32 newMarkBegin = MarkBegin;
				if (!MouseMarking)
					newMarkBegin = CursorPos;

				MouseMarking = true;
				setTextMarkers( newMarkBegin, CursorPos);
				calculateScrollPos();
				return true;
			}
		}
	default:
		break;
	}

	return false;
}


s32 CGUIEditBox::getCursorPos(s32 x, s32 y)
{
	IGUIFont* font = OverrideFont;
	IGUISkin* skin = Environment->getSkin();
	if (!OverrideFont)
		font = skin->getFont();

	const u32 lineCount = (WordWrap || MultiLine) ? BrokenText.size() : 1;

	core::stringw *txtLine=0;
	s32 startPos=0;
	x+=3;

	for (u32 i=0; i < lineCount; ++i)
	{
		setTextRect(i);
		if (i == 0 && y < CurrentTextRect.UpperLeftCorner.Y)
			y = CurrentTextRect.UpperLeftCorner.Y;
		if (i == lineCount - 1 && y > CurrentTextRect.LowerRightCorner.Y )
			y = CurrentTextRect.LowerRightCorner.Y;

		// is it inside this region?
		if (y >= CurrentTextRect.UpperLeftCorner.Y && y <= CurrentTextRect.LowerRightCorner.Y)
		{
			// we've found the clicked line
			txtLine = (WordWrap || MultiLine) ? &BrokenText[i] : &Text;
			startPos = (WordWrap || MultiLine) ? BrokenTextPositions[i] : 0;
			break;
		}
	}

	if (x < CurrentTextRect.UpperLeftCorner.X)
		x = CurrentTextRect.UpperLeftCorner.X;

	s32 idx = txtLine ? font->getCharacterFromPos(txtLine->c_str(), x - CurrentTextRect.UpperLeftCorner.X) : -1;

	// click was on or left of the line
	if (idx != -1)
		return idx + startPos;

	// click was off the right edge of the line, go to end.
	return txtLine->size() + startPos;
}


//! Breaks the single text line.
void CGUIEditBox::breakText()
{
	IGUISkin* skin = Environment->getSkin();

	if ((!WordWrap && !MultiLine) || !skin)
		return;

	BrokenText.clear(); // need to reallocate :/
	BrokenTextPositions.set_used(0);

	IGUIFont* font = OverrideFont;
	if (!OverrideFont)
		font = skin->getFont();

	if (!font)
		return;

	LastBreakFont = font;

	core::stringw line;
	core::stringw word;
	core::stringw whitespace;
	s32 lastLineStart = 0;
	s32 size = Text.size();
	s32 length = 0;
	s32 elWidth = RelativeRect.getWidth() - 6;
	wchar_t c;

	for (s32 i=0; i<size; ++i)
	{
		c = Text[i];
		bool lineBreak = false;

		if (c == L'\r') // Mac or Windows breaks
		{
			lineBreak = true;
			c = ' ';
			if (Text[i+1] == L'\n') // Windows breaks
			{
				Text.erase(i+1);
				--size;
			}
		}
		else if (c == L'\n') // Unix breaks
		{
			lineBreak = true;
			c = ' ';
		}

		// don't break if we're not a multi-line edit box
		if (!MultiLine)
			lineBreak = false;

		if (c == L' ' || c == 0 || i == (size-1))
		{
			if (word.size())
			{
				// here comes the next whitespace, look if
				// we can break the last word to the next line.
				s32 whitelgth = font->getDimension(whitespace.c_str()).Width;
				s32 worldlgth = font->getDimension(word.c_str()).Width;

				if (WordWrap && length + worldlgth + whitelgth > elWidth)
				{
					// break to next line
					length = worldlgth;
					BrokenText.push_back(line);
					BrokenTextPositions.push_back(lastLineStart);
					lastLineStart = i - (s32)word.size();
					line = word;
				}
				else
				{
					// add word to line
					line += whitespace;
					line += word;
					length += whitelgth + worldlgth;
				}

				word = L"";
				whitespace = L"";
			}

			whitespace += c;

			// compute line break
			if (lineBreak)
			{
				line += whitespace;
				line += word;
				BrokenText.push_back(line);
				BrokenTextPositions.push_back(lastLineStart);
				lastLineStart = i+1;
				line = L"";
				word = L"";
				whitespace = L"";
				length = 0;
			}
		}
		else
		{
			// yippee this is a word..
			word += c;
		}
	}

	line += whitespace;
	line += word;
	BrokenText.push_back(line);
	BrokenTextPositions.push_back(lastLineStart);
}


void CGUIEditBox::setTextRect(s32 line)
{
	core::dimension2du d;

	IGUISkin* skin = Environment->getSkin();
	if (!skin)
		return;

	IGUIFont* font = OverrideFont ? OverrideFont : skin->getFont();

	if (!font)
		return;

	// get text dimension
	const u32 lineCount = (WordWrap || MultiLine) ? BrokenText.size() : 1;
	if (WordWrap || MultiLine)
	{
		d = font->getDimension(BrokenText[line].c_str());
	}
	else
	{
		d = font->getDimension(Text.c_str());
		d.Height = AbsoluteRect.getHeight();
	}
	d.Height += font->getKerningHeight();

	// justification
	switch (HAlign)
	{
	case EGUIA_CENTER:
		// align to h centre
		CurrentTextRect.UpperLeftCorner.X = (FrameRect.getWidth()/2) - (d.Width/2);
		CurrentTextRect.LowerRightCorner.X = (FrameRect.getWidth()/2) + (d.Width/2);
		break;
	case EGUIA_LOWERRIGHT:
		// align to right edge
		CurrentTextRect.UpperLeftCorner.X = FrameRect.getWidth() - d.Width;
		CurrentTextRect.LowerRightCorner.X = FrameRect.getWidth();
		break;
	default:
		// align to left edge
		CurrentTextRect.UpperLeftCorner.X = 0;
		CurrentTextRect.LowerRightCorner.X = d.Width;

	}

	switch (VAlign)
	{
	case EGUIA_CENTER:
		// align to v centre
		CurrentTextRect.UpperLeftCorner.Y =
			(FrameRect.getHeight()/2) - (lineCount*d.Height)/2 + d.Height*line;
		break;
	case EGUIA_LOWERRIGHT:
		// align to bottom edge
		CurrentTextRect.UpperLeftCorner.Y =
			FrameRect.getHeight() - lineCount*d.Height + d.Height*line;
		break;
	default:
		// align to top edge
		CurrentTextRect.UpperLeftCorner.Y = d.Height*line;
		break;
	}

	CurrentTextRect.UpperLeftCorner.X  -= HScrollPos;
	CurrentTextRect.LowerRightCorner.X -= HScrollPos;
	CurrentTextRect.UpperLeftCorner.Y  -= VScrollPos;
	CurrentTextRect.LowerRightCorner.Y = CurrentTextRect.UpperLeftCorner.Y + d.Height;

	CurrentTextRect += FrameRect.UpperLeftCorner;

}


s32 CGUIEditBox::getLineFromPos(s32 pos)
{
	if (!WordWrap && !MultiLine)
		return 0;

	s32 i=0;
	while (i < (s32)BrokenTextPositions.size())
	{
		if (BrokenTextPositions[i] > pos)
			return i-1;
		++i;
	}
	return (s32)BrokenTextPositions.size() - 1;
}


void CGUIEditBox::inputChar(wchar_t c)
{
	if (!isEnabled())
		return;

	if (c != 0)
	{
		if (Text.size() < Max || Max == 0)
		{
			core::stringw s;

			if (MarkBegin != MarkEnd)
			{
				// replace marked text
				const s32 realmbgn = MarkBegin < MarkEnd ? MarkBegin : MarkEnd;
				const s32 realmend = MarkBegin < MarkEnd ? MarkEnd : MarkBegin;

				s = Text.subString(0, realmbgn);
				s.append(c);
				s.append( Text.subString(realmend, Text.size()-realmend) );
				Text = s;
				CursorPos = realmbgn+1;
			}
			else
			{
				// add new character
				s = Text.subString(0, CursorPos);
				s.append(c);
				s.append( Text.subString(CursorPos, Text.size()-CursorPos) );
				Text = s;
				++CursorPos;
			}

			BlinkStartTime = getTime();
			setTextMarkers(0, 0);
		}
	}
	breakText();
	sendGuiEvent(EGET_EDITBOX_CHANGED);
	calculateScrollPos();
}


void CGUIEditBox::calculateScrollPos()
{
	if (!AutoScroll)
		return;

	// calculate horizontal scroll position
	s32 cursLine = getLineFromPos(CursorPos);
	setTextRect(cursLine);

	// don't do horizontal scrolling when wordwrap is enabled.
	if (!WordWrap)
	{
		// get cursor position
		IGUISkin* skin = Environment->getSkin();
		if (!skin)
			return;
		IGUIFont* font = OverrideFont ? OverrideFont : skin->getFont();
		if (!font)
			return;

		core::stringw *txtLine = MultiLine ? &BrokenText[cursLine] : &Text;
		s32 cPos = MultiLine ? CursorPos - BrokenTextPositions[cursLine] : CursorPos;

		s32 cStart = CurrentTextRect.UpperLeftCorner.X + HScrollPos +
			font->getDimension(txtLine->subString(0, cPos).c_str()).Width;

		s32 cEnd = cStart + font->getDimension(L"_ ").Width;

		if (FrameRect.LowerRightCorner.X < cEnd)
			HScrollPos = cEnd - FrameRect.LowerRightCorner.X;
		else if (FrameRect.UpperLeftCorner.X > cStart)
			HScrollPos = cStart - FrameRect.UpperLeftCorner.X;
		else
			HScrollPos = 0;

		// todo: adjust scrollbar
	}

	// vertical scroll position
	if (FrameRect.LowerRightCorner.Y < CurrentTextRect.LowerRightCorner.Y + VScrollPos)
		VScrollPos = CurrentTextRect.LowerRightCorner.Y - FrameRect.LowerRightCorner.Y + VScrollPos;

	else if (FrameRect.UpperLeftCorner.Y > CurrentTextRect.UpperLeftCorner.Y + VScrollPos)
		VScrollPos = CurrentTextRect.UpperLeftCorner.Y - FrameRect.UpperLeftCorner.Y + VScrollPos;
	else
		VScrollPos = 0;

	// todo: adjust scrollbar
}

//! set text markers
void CGUIEditBox::setTextMarkers(s32 begin, s32 end)
{
    if ( begin != MarkBegin || end != MarkEnd )
    {
        MarkBegin = begin;
        MarkEnd = end;
        sendGuiEvent(EGET_EDITBOX_MARKING_CHANGED);
    }
}

//! send some gui event to parent
void CGUIEditBox::sendGuiEvent(EGUI_EVENT_TYPE type)
{
	if ( Parent )
	{
        SEvent e;
        e.EventType = EET_GUI_EVENT;
        e.GUIEvent.Caller = this;
        e.GUIEvent.Element = 0;
        e.GUIEvent.EventType = type;

        Parent->OnEvent(e);
	}
}

//! Writes attributes of the element.
void CGUIEditBox::serializeAttributes(io::IAttributes* out, io::SAttributeReadWriteOptions* options=0) const
{
	// IGUIEditBox::serializeAttributes(out,options);

	out->addBool  ("OverrideColorEnabled",OverrideColorEnabled );
	out->addColor ("OverrideColor",       OverrideColor);
	// out->addFont("OverrideFont",OverrideFont);
	out->addInt   ("MaxChars",            Max);
	out->addBool  ("WordWrap",            WordWrap);
	out->addBool  ("MultiLine",           MultiLine);
	out->addBool  ("AutoScroll",          AutoScroll);
	out->addBool  ("PasswordBox",         PasswordBox);
	core::stringw ch = L" ";
	ch[0] = PasswordChar;
	out->addString("PasswordChar",        ch.c_str());
	out->addEnum  ("HTextAlign",          HAlign, GUIAlignmentNames);
	out->addEnum  ("VTextAlign",          VAlign, GUIAlignmentNames);

	IGUIEditBox::serializeAttributes(out,options);
}


//! Reads attributes of the element
void CGUIEditBox::deserializeAttributes(io::IAttributes* in, io::SAttributeReadWriteOptions* options=0)
{
	IGUIEditBox::deserializeAttributes(in,options);

	setOverrideColor(in->getAttributeAsColor("OverrideColor"));
	enableOverrideColor(in->getAttributeAsBool("OverrideColorEnabled"));
	setMax(in->getAttributeAsInt("MaxChars"));
	setWordWrap(in->getAttributeAsBool("WordWrap"));
	setMultiLine(in->getAttributeAsBool("MultiLine"));
	setAutoScroll(in->getAttributeAsBool("AutoScroll"));
	core::stringw ch = in->getAttributeAsStringW("PasswordChar");

	if (!ch.size())
		setPasswordBox(in->getAttributeAsBool("PasswordBox"));
	else
		setPasswordBox(in->getAttributeAsBool("PasswordBox"), ch[0]);

	setTextAlignment( (EGUI_ALIGNMENT) in->getAttributeAsEnumeration("HTextAlign", GUIAlignmentNames),
			(EGUI_ALIGNMENT) in->getAttributeAsEnumeration("VTextAlign", GUIAlignmentNames));

	// setOverrideFont(in->getAttributeAsFont("OverrideFont"));
}

