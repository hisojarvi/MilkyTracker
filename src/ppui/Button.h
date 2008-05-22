/*
 *  ppui/Button.h
 *
 *  Copyright 2008 Peter Barth
 *
 *  This file is part of Milkytracker.
 *
 *  Milkytracker is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  Milkytracker is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with Milkytracker.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

/////////////////////////////////////////////////////////////////
//
//	PPButton control class
//
/////////////////////////////////////////////////////////////////
#ifndef BUTTON__H
#define BUTTON__H

#include "BasicTypes.h"
#include "Control.h"
#include "Event.h"

// Forwards
class PPGraphicsAbstract;
class PPFont;

class PPButton : public PPControl
{
private:
	const PPColor* color;
	const PPColor* textColor;
	bool border;
	bool clickable;
	bool update;
	bool verticalText;
	bool flat;
	bool autoSizeFont;

	PPPoint offset;
	
	bool pressed;

	bool invertShading;

	bool lMouseDown;
	bool rMouseDown;

	SimpleBitmap* bitmap;

	PPFont* font;
	PPString text;

public:
	PPButton(pp_int32 id, PPScreen* parentScreen, EventListenerInterface* eventListener, PPPoint location, PPSize size, bool border = true, bool clickable = true, bool update = true);
	virtual ~PPButton();

	void setSize(PPSize size) 
	{ 
		this->size = size; 
		if (this->size.width < 8)
			this->size.width = 8;	
		if (this->size.height < 8)
			this->size.height = 8;
	}
	void setLocation(PPPoint location) { this->location = location; }

	void setColor(const PPColor& color) { this->color = &color; }
	const PPColor* getColor() { return color; }

	void setXOffset(pp_int32 offset) { this->offset.x = offset; }
	void setYOffset(pp_int32 offset) { this->offset.y = offset; }

	SimpleBitmap& getBitmap(); 

	void setText(const PPString& text);
	const PPString& getText() { return text; }

	void setTextColor(const PPColor& color) { this->textColor = &color; }
	const PPColor* getTextColor() { return textColor; }

	void setVerticalText(bool b) { verticalText = b; }

	void setFont(PPFont* font) { this->font = font; }
	PPFont* getFont() { return font; }

	void setPressed(bool pressed) { this->pressed = pressed; }
	bool isPressed() { return pressed; }

	void setUpdateable(bool b) { update = b; }

	void setClickable(bool b) { clickable = b; }
	bool isClickable() { return clickable; }
	
	void setInvertShading(bool b) { invertShading = b; }

	void setFlat(bool b) { flat = b; }
	
	void setAutoSizeFont(bool autoSizeFont) { this->autoSizeFont = autoSizeFont; }

	virtual void paint(PPGraphicsAbstract* graphics);
	virtual pp_int32 dispatchEvent(PPEvent* event);
	virtual bool gainsFocus() { return false; }
	virtual bool isActive() { return true; }
	
private:
	void handleButtonPress(bool& lMouseDown, bool& rMouseDown);
	void handleButtonRelease(bool& lMouseDown, bool& rMouseDown, PPEvent* event, EEventDescriptor postEvent);
};

#endif
