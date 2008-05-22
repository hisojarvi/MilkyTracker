/*
 *  ppui/Control.h
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
//	Basic control class
//
/////////////////////////////////////////////////////////////////
#ifndef CONTROL__H
#define CONTROL__H

// our basic types
#include "Object.h"
#include "BasicTypes.h"

// Forwards
class PPEvent;
class EventListenerInterface;
class PPGraphicsAbstract;
class PPScreen;

/////////////////////////////////////////////////////////////////
//	Basic interface for all controls
/////////////////////////////////////////////////////////////////
class PPControl : public PPObject
{
private:
	pp_int32 id;

protected:
	PPScreen*	parentScreen;
	EventListenerInterface* eventListener; // this object will receive events
	PPControl* ownerControl;

	PPPoint location;
	PPSize size;

	bool visible;
	bool enabled;

	bool hasFocus;

	PPControl(pp_int32 id, PPScreen* parentScreen, EventListenerInterface* eventListener = NULL);
	PPControl(pp_int32 id, PPScreen* parentScreen, EventListenerInterface* eventListener, const PPPoint& location, const PPSize& size);

public:
	virtual ~PPControl() {}

	void setEventListener(EventListenerInterface* eventListener) { this->eventListener = eventListener; }
	EventListenerInterface* getEventListener() { return eventListener; }

	void setOwnerControl(PPControl* ownerControl) { this->ownerControl = ownerControl; }
	PPControl* getOwnerControl() { return ownerControl; }

	virtual pp_int32 dispatchEvent(PPEvent* event);

	virtual void paint(PPGraphicsAbstract* graphics) = 0;

	virtual bool gainsFocus() { return false; }
	virtual bool gainedFocusByMouse() { return gainsFocus(); }

	virtual bool isActive() { return true; }

	virtual bool hit(const PPPoint& p); 

	virtual void show(bool visible) { this->visible = visible; }
	virtual void hide(bool hidden) { show(!hidden); }

	virtual bool isVisible();
	virtual bool isHidden() { return !isVisible(); }
	
	virtual void enable(bool b) { enabled = b; }
	bool isEnabled() { return enabled; }

	PPPoint getLocation() { return location; }
	PPSize getSize() { return size; }
	PPRect getBoundingRect() 
	{
		PPRect result;
		result.x1 = location.x;
		result.y1 = location.y;
		result.x2 = location.x + size.width;
		result.y2 = location.y + size.height;
		return result;
	}

	pp_int32 getID() { return id; }

	virtual void setSize(PPSize size) { this->size = size; }
	virtual void setLocation(PPPoint location) { this->location = location; }
	virtual bool isContainer() { return false; }
	virtual bool isListBox() { return false; }
	virtual bool receiveTimerEvent() { return false; }

	virtual bool gotFocus() { return hasFocus; }

protected:
	virtual void translateCoordinates(PPPoint& cp) 
	{ 
		cp.x-=location.x;
		cp.y-=location.y;
	}

	virtual void notifyChanges();
	
	void drawBorder(PPGraphicsAbstract* g, const PPColor& borderColor);
	void drawThickBorder(PPGraphicsAbstract* g, const PPColor& borderColor);
	
};

#endif
