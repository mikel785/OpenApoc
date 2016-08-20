#include "forms/control.h"
#include "forms/forms.h"
#include "framework/event.h"
#include "framework/framework.h"
#include "library/sp.h"
#include <tinyxml2.h>

namespace OpenApoc
{

Control::Control(bool takesFocus)
    : mouseInside(false), mouseDepressed(false), resolvedLocation(0, 0), Name("Control"),
      Location(0, 0), Size(0, 0), BackgroundColour(0, 0, 0, 0), takesFocus(takesFocus),
      showBounds(false), Visible(true), Enabled(true), canCopy(true)
{
}

Control::~Control() { unloadResources(); }

bool Control::isFocused() const
{
	// FIXME: Need to think of a method of 'sharing' focus across multiple forms/non-form active
	// areas
	return false;
}

void Control::resolveLocation()
{
	auto parentControl = this->getParent();
	if (parentControl == nullptr)
	{
		resolvedLocation.x = Location.x;
		resolvedLocation.y = Location.y;
	}
	else
	{
		if (Location.x > parentControl->Size.x || Location.y > parentControl->Size.y)
		{
			resolvedLocation.x = -99999;
			resolvedLocation.y = -99999;
		}
		else
		{
			resolvedLocation.x = parentControl->resolvedLocation.x + Location.x;
			resolvedLocation.y = parentControl->resolvedLocation.y + Location.y;
		}
	}

	for (auto ctrlidx = Controls.rbegin(); ctrlidx != Controls.rend(); ctrlidx++)
	{
		auto c = *ctrlidx;
		c->resolveLocation();
	}
}

void Control::eventOccured(Event *e)
{
	for (auto ctrlidx = Controls.rbegin(); ctrlidx != Controls.rend(); ctrlidx++)
	{
		auto c = *ctrlidx;
		if (c->Visible && c->Enabled)
		{
			c->eventOccured(e);
			if (e->Handled)
			{
				return;
			}
		}
	}

	if (e->type() == EVENT_MOUSE_MOVE || e->type() == EVENT_MOUSE_DOWN ||
	    e->type() == EVENT_MOUSE_UP)
	{
		bool newInside =
		    (e->mouse().X >= resolvedLocation.x && e->mouse().X < resolvedLocation.x + Size.x &&
		     e->mouse().Y >= resolvedLocation.y && e->mouse().Y < resolvedLocation.y + Size.y);

		if (e->type() == EVENT_MOUSE_MOVE)
		{
			if (newInside)
			{
				if (!mouseInside)
				{
					this->pushFormEvent(FormEventType::MouseEnter, e);
				}
				this->pushFormEvent(FormEventType::MouseMove, e);
				e->Handled = true;
			}
			else
			{
				if (mouseInside)
				{
					this->pushFormEvent(FormEventType::MouseLeave, e);
				}
			}
		}

		if (e->type() == EVENT_MOUSE_DOWN)
		{
			if (newInside)
			{
				this->pushFormEvent(FormEventType::MouseDown, e);
				mouseDepressed = true;
			}
		}

		if (e->type() == EVENT_MOUSE_UP)
		{
			if (newInside)
			{
				this->pushFormEvent(FormEventType::MouseUp, e);

				if (mouseDepressed)
				{
					this->pushFormEvent(FormEventType::MouseClick, e);
				}
			}
			mouseDepressed = false;
		}
		mouseInside = newInside;
	}

	if (e->type() == EVENT_FINGER_DOWN || e->type() == EVENT_FINGER_UP ||
	    e->type() == EVENT_FINGER_MOVE)
	{
		// This right now is a carbon copy of mouse event-handling. Maybe we should do something
		// else?
		// FIXME: Use something other than mouseInside? fingerInside maybe?
		bool newInside =
		    (e->finger().X >= resolvedLocation.x && e->finger().X < resolvedLocation.x + Size.x &&
		     e->finger().Y >= resolvedLocation.y && e->finger().Y < resolvedLocation.y + Size.y);

		// FIXME: Right now we'll just copy touch data to the mouse location. That's... not exactly
		// right.
		// FIXME: We're only doing event processing for the "primary" finger.
		if (e->finger().IsPrimary || 1)
		{
			FrameworkMouseEvent FakeMouseData;
			FakeMouseData.X = e->finger().X;
			FakeMouseData.Y = e->finger().Y;
			FakeMouseData.DeltaX = e->finger().DeltaX;
			FakeMouseData.DeltaY = e->finger().DeltaY;
			FakeMouseData.WheelHorizontal = 0;
			FakeMouseData.WheelVertical = 0;
			FakeMouseData.Button = 1; // Always left mouse button?

			if (e->type() == EVENT_FINGER_MOVE)
			{
				MouseEvent fakeMouseEvent(EVENT_MOUSE_MOVE);
				fakeMouseEvent.mouse() = FakeMouseData;
				if (newInside)
				{
					if (!mouseInside)
					{
						this->pushFormEvent(FormEventType::MouseEnter, &fakeMouseEvent);
					}

					this->pushFormEvent(FormEventType::MouseMove, &fakeMouseEvent);

					e->Handled = true;
				}
				else
				{
					if (mouseInside)
					{
						this->pushFormEvent(FormEventType::MouseLeave, &fakeMouseEvent);
					}
				}
			}

			if (e->type() == EVENT_FINGER_DOWN)
			{
				MouseEvent fakeMouseEvent(EVENT_MOUSE_DOWN);
				fakeMouseEvent.mouse() = FakeMouseData;
				if (newInside)
				{
					this->pushFormEvent(FormEventType::MouseDown, &fakeMouseEvent);
					mouseDepressed = true;

					// e->Handled = true;
				}
			}

			if (e->type() == EVENT_FINGER_UP)
			{
				MouseEvent fakeMouseEvent(EVENT_MOUSE_UP);
				fakeMouseEvent.mouse() = FakeMouseData;
				if (newInside)
				{
					this->pushFormEvent(FormEventType::MouseUp, &fakeMouseEvent);

					if (mouseDepressed)
					{
						this->pushFormEvent(FormEventType::MouseClick, &fakeMouseEvent);
					}
				}
				// FIXME: This will result in collisions with real mouse events.
				mouseDepressed = false;
			}
		}
		mouseInside = newInside;
	}

	if (e->type() == EVENT_KEY_DOWN || e->type() == EVENT_KEY_UP)
	{
		if (isFocused())
		{
			this->pushFormEvent(
			    e->type() == EVENT_KEY_DOWN ? FormEventType::KeyDown : FormEventType::KeyUp, e);

			e->Handled = true;
		}
	}
	if (e->type() == EVENT_KEY_PRESS)
	{
		if (isFocused())
		{
			this->pushFormEvent(FormEventType::KeyPress, e);

			e->Handled = true;
		}
	}

	if (e->type() == EVENT_TEXT_INPUT)
	{
		if (isFocused())
		{
			this->pushFormEvent(FormEventType::TextInput, e);

			e->Handled = true;
		}
	}
}

void Control::render()
{
	if (!Visible || Size.x == 0 || Size.y == 0)
	{
		return;
	}

	if (controlArea == nullptr || controlArea->size != Vec2<unsigned int>(Size))
	{
		controlArea.reset(new Surface{Vec2<unsigned int>(Size)});
	}
	{
		sp<Palette> previousPalette;
		if (this->palette)
		{
			previousPalette = fw().renderer->getPalette();
			fw().renderer->setPalette(this->palette);
		}

		RendererSurfaceBinding b(*fw().renderer, controlArea);
		preRender();
		onRender();
		postRender();
		if (this->palette)
		{
			fw().renderer->setPalette(previousPalette);
		}
	}

	if (Enabled)
	{
		fw().renderer->draw(controlArea, Location);
	}
	else
	{
		fw().renderer->drawTinted(controlArea, Location, Colour(255, 255, 255, 128));
	}
}

void Control::preRender() { fw().renderer->clear(BackgroundColour); }

void Control::onRender()
{
	// Nothing specifically for the base control
}

void Control::postRender()
{
	for (auto ctrlidx = Controls.begin(); ctrlidx != Controls.end(); ctrlidx++)
	{
		auto c = *ctrlidx;
		if (c->Visible)
		{
			c->render();
		}
	}
	if (showBounds)
	{
		fw().renderer->drawRect({0, 0}, Size, Colour{255, 0, 0, 255});
	}
}

void Control::update()
{
	for (auto ctrlidx = Controls.begin(); ctrlidx != Controls.end(); ctrlidx++)
	{
		auto c = *ctrlidx;
		c->update();
	}
}

void Control::configureFromXml(tinyxml2::XMLElement *Element)
{
	configureSelfFromXml(Element);
	configureChildrenFromXml(Element);
}

void Control::configureChildrenFromXml(tinyxml2::XMLElement *Element)
{
	UString nodename;
	UString attribvalue;
	tinyxml2::XMLElement *node;
	for (node = Element->FirstChildElement(); node != nullptr; node = node->NextSiblingElement())
	{
		nodename = node->Name();

		// Child controls
		if (nodename == "control")
		{
			auto c = this->createChild<Control>();
			c->configureFromXml(node);
		}
		else if (nodename == "label")
		{
			auto l = this->createChild<Label>();
			l->configureFromXml(node);
		}
		else if (nodename == "graphic")
		{
			auto g = this->createChild<Graphic>();
			g->configureFromXml(node);
		}
		else if (nodename == "textbutton")
		{
			auto tb = this->createChild<TextButton>();
			tb->configureFromXml(node);
		}
		else if (nodename == "graphicbutton")
		{
			auto gb = this->createChild<GraphicButton>();
			gb->configureFromXml(node);
			if (node->Attribute("scrollprev") != nullptr &&
			    UString(node->Attribute("scrollprev")) != "")
			{
				attribvalue = node->Attribute("scrollprev");
				gb->ScrollBarPrev = this->findControlTyped<ScrollBar>(attribvalue);
			}
			if (node->Attribute("scrollnext") != nullptr &&
			    UString(node->Attribute("scrollnext")) != "")
			{
				attribvalue = node->Attribute("scrollnext");
				gb->ScrollBarNext = this->findControlTyped<ScrollBar>(attribvalue);
			}
		}
		else if (nodename == "checkbox")
		{
			auto cb = this->createChild<CheckBox>();
			cb->configureFromXml(node);
		}
		else if (nodename == "radiobutton")
		{
			sp<RadioButtonGroup> group = nullptr;
			if (node->Attribute("groupid") != nullptr && UString(node->Attribute("groupid")) != "")
			{
				attribvalue = node->Attribute("groupid");
				if (radiogroups.find(attribvalue) == radiogroups.end())
				{
					radiogroups[attribvalue] = mksp<RadioButtonGroup>(attribvalue);
				}
				group = radiogroups[attribvalue];
			}
			else
			{
				LogError("Radiobutton \"%s\" has no group", node->Attribute("id"));
			}
			auto rb = this->createChild<RadioButton>(group);
			rb->configureFromXml(node);
			if (group)
			{
				group->radioButtons.push_back(rb);
			}
		}
		else if (nodename == "scroll")
		{
			auto sb = this->createChild<ScrollBar>();
			sb->configureFromXml(node);
		}

		else if (nodename == "listbox")
		{
			sp<ScrollBar> sb = nullptr;

			if (node->Attribute("scrollbarid") != nullptr &&
			    UString(node->Attribute("scrollbarid")) != "")
			{
				attribvalue = node->Attribute("scrollbarid");
				sb = this->findControlTyped<ScrollBar>(attribvalue);
			}
			auto lb = this->createChild<ListBox>(sb);
			lb->configureFromXml(node);
		}

		else if (nodename == "textedit")
		{
			auto te = this->createChild<TextEdit>();
			te->configureFromXml(node);
		}

		else if (nodename == "ticker")
		{
			auto tk = this->createChild<Ticker>();
			tk->configureFromXml(node);
		}
	}
}

void Control::configureSelfFromXml(tinyxml2::XMLElement *Element)
{
	UString nodename;
	UString attribvalue;
	UString specialpositionx = "";
	UString specialpositiony = "";

	if (Element->Attribute("id") != nullptr && UString(Element->Attribute("id")) != "")
	{
		nodename = Element->Attribute("id");
		this->Name = nodename;
	}

	if (Element->Attribute("visible") != nullptr && UString(Element->Attribute("visible")) != "")
	{
		UString vistxt = Element->Attribute("visible");
		vistxt = vistxt.substr(0, 1).toUpper();
		this->Visible = (vistxt == "Y" || vistxt == "T");
	}

	if (Element->Attribute("border") != nullptr && UString(Element->Attribute("border")) != "")
	{
		UString vistxt = Element->Attribute("border");
		vistxt = vistxt.substr(0, 1).toUpper();
		this->showBounds = (vistxt == "Y" || vistxt == "T");
	}

	auto parentControl = this->getParent();
	tinyxml2::XMLElement *node;
	for (node = Element->FirstChildElement(); node != nullptr; node = node->NextSiblingElement())
	{
		nodename = node->Name();

		if (nodename == "palette")
		{
			auto pal = fw().data->loadPalette(node->GetText());
			if (!pal)
			{
				LogError("Control referenced palette \"%s\" that cannot be loaded",
				         node->GetText());
			}
			this->palette = pal;
		}

		else if (nodename == "backcolour")
		{
			if (node->Attribute("a") != nullptr && UString(node->Attribute("a")) != "")
			{
				this->BackgroundColour = Colour{
				    Strings::toU8(node->Attribute("r")), Strings::toU8(node->Attribute("g")),
				    Strings::toU8(node->Attribute("b")), Strings::toU8(node->Attribute("a"))};
			}
			else
			{
				this->BackgroundColour =
				    Colour{Strings::toU8(node->Attribute("r")), Strings::toU8(node->Attribute("g")),
				           Strings::toU8(node->Attribute("b"))};
			}
		}
		else if (nodename == "position")
		{
			if (Strings::isInteger(node->Attribute("x")))
			{
				Location.x = Strings::toInteger(node->Attribute("x"));
			}
			else
			{
				specialpositionx = node->Attribute("x");
			}
			if (Strings::isInteger(node->Attribute("y")))
			{
				Location.y = Strings::toInteger(node->Attribute("y"));
			}
			else
			{
				specialpositiony = node->Attribute("y");
			}
		}
		else if (nodename == "size")
		{
			UString specialsizex = "";
			UString specialsizey = "";

			// if size ends with % this means that it is special (percentage) size
			UString width = node->Attribute("width");
			if (Strings::isInteger(width) && !(width.str().back() == '%'))
			{
				Size.x = Strings::toInteger(width);
			}
			else
			{
				specialsizex = width;
			}

			UString height = node->Attribute("height");
			if (Strings::isInteger(height) && !(height.str().back() == '%'))
			{
				Size.y = Strings::toInteger(height);
			}
			else
			{
				specialsizey = height;
			}

			if (specialsizex != "")
			{
				if (specialsizex.str().back() == '%')
				{
					// Skip % sign at the end
					auto sizeRatio = Strings::toFloat(specialsizex) / 100.0f;
					setRelativeWidth(sizeRatio);
				}
				else
				{
					LogWarning("Control \"%s\" has not supported size x value \"%s\"",
					           this->Name.cStr(), specialsizex.cStr());
				}
			}

			if (specialsizey != "")
			{
				if (specialsizey.str().back() == '%')
				{
					auto sizeRatio = Strings::toFloat(specialsizey) / 100.0f;
					setRelativeHeight(sizeRatio);
				}
				else if (specialsizey == "item")
				{
					// get size y for parent control
					sp<Control> parent = parentControl;
					sp<ListBox> listBox;
					while (parent != nullptr &&
					       !(listBox = std::dynamic_pointer_cast<ListBox>(parent)))
					{
						parent = parent->getParent();
					}
					if (listBox != nullptr)
					{
						Size.y = listBox->ItemSize;
					}
					else
					{
						LogWarning(
						    "Control \"%s\" with \"item\" size.y does not have ListBox parent ",
						    this->Name.cStr());
					}
				}
				else
				{
					LogWarning("Control \"%s\" has not supported size y value \"%s\"",
					           this->Name.cStr(), specialsizey.cStr());
				}
			}
		}
	}

	if (specialpositionx != "")
	{
		if (specialpositionx == "left")
		{
			align(HorizontalAlignment::Left);
		}
		else if (specialpositionx == "centre")
		{
			align(HorizontalAlignment::Centre);
		}
		else if (specialpositionx == "right")
		{
			align(HorizontalAlignment::Right);
		}
	}

	if (specialpositiony != "")
	{
		if (specialpositiony == "top")
		{
			align(VerticalAlignment::Top);
		}
		else if (specialpositiony == "centre")
		{
			align(VerticalAlignment::Centre);
		}
		else if (specialpositiony == "bottom")
		{
			align(VerticalAlignment::Bottom);
		}
	}

	LogInfo("Control \"%s\" has %zu subcontrols (%d, %d, %d, %d)", this->Name.cStr(),
	        Controls.size(), Location.x, Location.y, Size.x, Size.y);
}

void Control::unloadResources() {}

sp<Control> Control::operator[](int Index) const { return Controls.at(Index); }

sp<Control> Control::findControl(UString ID) const
{
	for (auto c = Controls.begin(); c != Controls.end(); c++)
	{
		auto ctrl = *c;
		if (ctrl->Name == ID)
		{
			return ctrl;
		}
		auto childControl = ctrl->findControl(ID);
		if (childControl)
			return childControl;
	}
	return nullptr;
}

sp<Control> Control::getParent() const { return owningControl.lock(); }

sp<Control> Control::getRootControl()
{
	auto parent = owningControl.lock();
	if (parent == nullptr)
	{
		return shared_from_this();
	}
	else
	{
		return parent->getRootControl();
	}
}

sp<Form> Control::getForm()
{
	auto c = getRootControl();
	return std::dynamic_pointer_cast<Form>(c);
}

void Control::setParent(sp<Control> Parent)
{
	if (Parent)
	{
		auto previousParent = this->owningControl.lock();
		if (previousParent)
		{
			LogError("Reparenting control");
		}
		Parent->Controls.push_back(shared_from_this());
	}
	owningControl = Parent;
}

sp<Control> Control::getAncestor(sp<Control> Parent)
{
	sp<Control> ancestor = shared_from_this();
	while (ancestor != nullptr)
	{
		if (ancestor->getParent() == Parent)
		{
			break;
		}
		else
		{
			ancestor = ancestor->getParent();
		}
	}
	return ancestor;
}

Vec2<int> Control::getLocationOnScreen() const
{
	Vec2<int> r(resolvedLocation.x, resolvedLocation.y);
	return r;
}

void Control::setRelativeWidth(float widthFactor)
{
	if (widthFactor == 0)
	{
		Size.x = 0;
	}
	else
	{
		Vec2<int> parentSize = getParentSize();
		Size.x = (int)(parentSize.x * widthFactor);
	}
}

void Control::setRelativeHeight(float heightFactor)
{
	if (heightFactor == 0)
	{
		Size.y = 0;
	}
	else
	{
		Vec2<int> parentSize = getParentSize();
		Size.y = (int)(parentSize.y * heightFactor);
	}
}

Vec2<int> Control::getParentSize() const
{
	auto parent = getParent();
	if (parent != nullptr)
	{
		return parent->Size;
	}
	else
	{
		return Vec2<int>{fw().displayGetWidth(), fw().displayGetHeight()};
	}
}

int Control::align(HorizontalAlignment HAlign, int ParentWidth, int ChildWidth)
{
	int x = 0;
	switch (HAlign)
	{
		case HorizontalAlignment::Left:
			x = 0;
			break;
		case HorizontalAlignment::Centre:
			x = (ParentWidth / 2) - (ChildWidth / 2);
			break;
		case HorizontalAlignment::Right:
			x = ParentWidth - ChildWidth;
			break;
	}
	return x;
}

int Control::align(VerticalAlignment VAlign, int ParentHeight, int ChildHeight)
{
	int y = 0;
	switch (VAlign)
	{
		case VerticalAlignment::Top:
			y = 0;
			break;
		case VerticalAlignment::Centre:
			y = (ParentHeight / 2) - (ChildHeight / 2);
			break;
		case VerticalAlignment::Bottom:
			y = ParentHeight - ChildHeight;
			break;
	}
	return y;
}

void Control::align(HorizontalAlignment HAlign)
{
	Location.x = align(HAlign, getParentSize().x, Size.x);
}

void Control::align(VerticalAlignment VAlign)
{
	Location.y = align(VAlign, getParentSize().y, Size.y);
}

void Control::align(HorizontalAlignment HAlign, VerticalAlignment VAlign)
{
	align(HAlign);
	align(VAlign);
}

sp<Control> Control::copyTo(sp<Control> CopyParent)
{
	sp<Control> copy;
	if (CopyParent)
	{
		copy = CopyParent->createChild<Control>(takesFocus);
	}
	else
	{
		copy = mksp<Control>(takesFocus);
	}
	copyControlData(copy);
	return copy;
}

void Control::copyControlData(sp<Control> CopyOf)
{
	lastCopiedTo = CopyOf;

	CopyOf->Name = this->Name;
	CopyOf->Location = this->Location;
	CopyOf->Size = this->Size;
	CopyOf->BackgroundColour = this->BackgroundColour;
	CopyOf->takesFocus = this->takesFocus;
	CopyOf->showBounds = this->showBounds;
	CopyOf->Visible = this->Visible;

	for (auto c = Controls.begin(); c != Controls.end(); c++)
	{
		auto ctrl = *c;
		if (ctrl->canCopy)
		{
			ctrl->copyTo(CopyOf);
		}
	}
}

bool Control::eventIsWithin(const Event *e) const
{
	if (e->type() == EVENT_MOUSE_MOVE || e->type() == EVENT_MOUSE_DOWN ||
	    e->type() == EVENT_MOUSE_UP)
	{
		return (e->mouse().X >= resolvedLocation.x && e->mouse().X < resolvedLocation.x + Size.x &&
		        e->mouse().Y >= resolvedLocation.y && e->mouse().Y < resolvedLocation.y + Size.y);
	}
	else if (e->type() == EVENT_FINGER_DOWN || e->type() == EVENT_FINGER_UP ||
	         e->type() == EVENT_FINGER_MOVE)
	{
		return (e->finger().X >= resolvedLocation.x &&
		        e->finger().X < resolvedLocation.x + Size.x &&
		        e->finger().Y >= resolvedLocation.y && e->finger().Y < resolvedLocation.y + Size.y);
	}
	// Only mouse & finger events have a location to be within
	return false;
}

void Control::pushFormEvent(FormEventType type, Event *parentEvent)
{
	FormsEvent *event = nullptr;
	switch (type)
	{
		// Mouse events fall-through
		case FormEventType::MouseEnter:
		case FormEventType::MouseLeave:
		case FormEventType::MouseDown:
		case FormEventType::MouseUp:
		case FormEventType::MouseMove:
		case FormEventType::MouseClick:
		{
			event = new FormsEvent();
			event->forms().RaisedBy = shared_from_this();
			event->forms().EventFlag = type;
			event->forms().MouseInfo = parentEvent->mouse();
			event->forms().MouseInfo.X -= resolvedLocation.x;
			event->forms().MouseInfo.Y -= resolvedLocation.y;
			fw().pushEvent(event);
			break;
		}
		// Keyboard events fall-through
		case FormEventType::KeyDown:
		case FormEventType::KeyUp:
		case FormEventType::KeyPress:
		{
			event = new FormsEvent();
			event->forms().RaisedBy = shared_from_this();
			event->forms().EventFlag = type;
			event->forms().KeyInfo = parentEvent->keyboard();
			fw().pushEvent(event);
			break;
		}
		// Input event special
		case FormEventType::TextInput:
		{
			event = new FormsEvent();
			event->forms().RaisedBy = shared_from_this();
			event->forms().EventFlag = type;
			event->forms().Input = parentEvent->text();
			fw().pushEvent(event);
			break;
		}
		// Forms events fall-through
		case FormEventType::ButtonClick:
		case FormEventType::CheckBoxChange:
		case FormEventType::ListBoxChangeHover:
		case FormEventType::ListBoxChangeSelected:
		case FormEventType::ScrollBarChange:
		case FormEventType::TextChanged:
		case FormEventType::CheckBoxSelected:
		case FormEventType::CheckBoxDeSelected:
		case FormEventType::TextEditFinish:
		{
			event = new FormsEvent();
			if (parentEvent)
			{
				event->forms() = parentEvent->forms();
			}
			event->forms().RaisedBy = shared_from_this();
			event->forms().EventFlag = type;
			fw().pushEvent(event);
			break;
		}
		default:
			LogError("Unexpected event type %d", (int)type);
	}
	this->triggerEventCallbacks(event);
}

void Control::triggerEventCallbacks(FormsEvent *e)
{
	for (auto &callback : this->callbacks[e->forms().EventFlag])
	{
		callback(e);
	}
}

void Control::addCallback(FormEventType event, std::function<void(FormsEvent *e)> callback)
{
	this->callbacks[event].push_back(callback);
}

}; // namespace OpenApoc
