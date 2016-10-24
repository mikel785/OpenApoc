		#include "game/ui/base/vequipscreen.h"
#include "forms/form.h"
#include "forms/graphic.h"
#include "forms/label.h"
#include "forms/list.h"
#include "forms/radiobutton.h"
#include "forms/ui.h"
#include "framework/apocresources/cursor.h"
#include "framework/data.h"
#include "framework/event.h"
#include "framework/font.h"
#include "framework/framework.h"
#include "framework/keycodes.h"
#include "framework/renderer.h"
#include "game/state/base/base.h"
#include "game/state/city/vehicle.h"
#include "game/state/city/vequipment.h"
#include "game/state/equipment.h"
#include "game/state/gamestate.h"
#include "game/state/rules/vehicle_type.h"
#include "library/strings_format.h"
#include <cmath>

namespace OpenApoc
{

static sp<Control> createInventoryControl(sp<EquipmentType> equipment, int count)
{
	auto font = ui().getFont("SMALFONT");
	auto control = mksp<Control>();
	auto img = equipment->getImage();
	auto graphic = control->createChild<Graphic>(img);
	graphic->Size = img->size;
	graphic->Location = {0, 0};
	// FIXME: This currently seems to be the only way of getting the size of a label upfront?
	auto textImage = font->getString(format("%d", count));

	auto label = control->createChild<Label>(format("%d", count), font);

	control->Size.x = std::max(img->size.x, textImage->size.x);
	control->Size.y = img->size.y + textImage->size.y;
	// Set label X to 'whole control' x and centre
	label->Size = {control->Size.x, textImage->size.y};
	label->Location = {0, img->size.y};
	label->TextHAlign = HorizontalAlignment::Centre;

	return control;
}

class VEquipState
{
  private:
	sp<EquippableObject> object;
	sp<EquipmentStore> inventory;
	sp<Control> paperDollControl;
	sp<ListBox> inventoryControl;

	sp<Equipment> draggedEquipment;
	VEquipmentType::Type selectedType = VEquipmentType::Type::Weapon;

  public:
	VEquipState(sp<EquippableObject> object, sp<EquipmentStore> inventory,
	            sp<Control> paperDollControl, sp<ListBox> inventoryControl)
	    : object(object), inventory(inventory), paperDollControl(paperDollControl),
	      inventoryControl(inventoryControl)
	{
		this->resetInventory();
	}
	void render()
	{
		
	}
	void eventOccurred(Event *e);
	void setSelectedType(VEquipmentType::Type);

	void resetInventory()
	{
		inventoryControl->clear();
		if (!inventory)
			return;
		for (auto &item : inventory->getEquipment())
		{
			auto itemType = item.first;
			auto count = item.second;

			auto control = createInventoryControl(itemType, count);
			control->setData(itemType);
			inventoryControl->addItem(control);
		}
	}
};

class BaseVehicleInventory : public EquipmentStore
{
  private:
	StateRef<Base> base;
	VEquipmentType::Type currentType = VEquipmentType::Type::Weapon;

  public:
	BaseVehicleInventory(StateRef<Base> base) : base(base) {}
	~BaseVehicleInventory() override = default;

	void setEquipmentType(VEquipmentType::Type type) { this->currentType = type; }
	void returnEquipmentToStore(sp<Equipment>) override
	{
		// TODO:
		// De-instanticate equipment (remove ammo)
		// Return to base->stores
	}
	std::list<std::pair<sp<EquipmentType>, int>> getEquipment() const override
	{
		std::list<std::pair<sp<EquipmentType>, int>> list;
		// TODO:
		// for (equipment : base->storedEquipment)
		// {
		//   if (equipment->Type == currentType)
		//   {
		//     list.push_back(equipment, equipment->storedCount);
		//   }
		// }
		return list;
	}
};

VEquipScreen::VEquipScreen(sp<GameState> state)
    : Stage(), form(ui().getForm("vequipscreen")), selectionType(VEquipmentType::Type::Weapon),
      pal(fw().data->loadPalette("xcom3/ufodata/vroadwar.pcx")),
      labelFont(ui().getFont("smalfont")), state(state), glowCounter(0)

{
	form->findControlTyped<RadioButton>("BUTTON_SHOW_WEAPONS")->setChecked(true);

	for (auto &v : state->vehicles)
	{
		auto vehicle = v.second;
		if (vehicle->owner != state->getPlayer())
			continue;
		this->setSelectedVehicle(vehicle);
		break;
	}
	if (!this->selected)
	{
		LogError("No vehicles found - the original apoc didn't open the equip screen in this case");
	}
}

VEquipScreen::~VEquipScreen() = default;

void VEquipScreen::begin()
{
	form->findControlTyped<Label>("TEXT_FUNDS")->setText(state->getPlayerBalance());
}

void VEquipScreen::pause() {}

void VEquipScreen::resume() {}

void VEquipScreen::finish() {}

void VEquipScreen::eventOccurred(Event *e)
{
	form->eventOccured(e);
	equipState->eventOccurred(e);

	if (e->type() == EVENT_KEY_DOWN)
	{
		if (e->keyboard().KeyCode == SDLK_ESCAPE)
		{
			fw().stageQueueCommand({StageCmd::Command::POP});
			return;
		}
	}
	if (e->type() == EVENT_FORM_INTERACTION && e->forms().EventFlag == FormEventType::ButtonClick)
	{

		if (e->forms().RaisedBy->Name == "BUTTON_OK")
		{
			fw().stageQueueCommand({StageCmd::Command::POP});
			return;
		}
	}
	else if (e->type() == EVENT_FORM_INTERACTION &&
	         e->forms().EventFlag == FormEventType::CheckBoxChange)
	{
		if (form->findControlTyped<RadioButton>("BUTTON_SHOW_WEAPONS")->isChecked())
		{
			this->selectionType = VEquipmentType::Type::Weapon;
			return;
		}
		else if (form->findControlTyped<RadioButton>("BUTTON_SHOW_ENGINES")->isChecked())
		{
			this->selectionType = VEquipmentType::Type::Engine;
			return;
		}
		else if (form->findControlTyped<RadioButton>("BUTTON_SHOW_GENERAL")->isChecked())
		{
			this->selectionType = VEquipmentType::Type::General;
			return;
		}
	}
}

void VEquipScreen::update()
{
	static const float GLOW_COUNTER_INCREMENT = M_PI / 15.0f;
	this->glowCounter += GLOW_COUNTER_INCREMENT;
	// Loop the increment over the period, otherwise we could start getting lower precision etc. if
	// people leave the screen going for a few centuries
	while (this->glowCounter > 2.0f * M_PI)
	{
		this->glowCounter -= 2.0f * M_PI;
	}
	form->update();
}

void VEquipScreen::render()
{
	fw().stageGetPrevious(this->shared_from_this())->render();

	fw().renderer->setPalette(this->pal);

	// The labels/values in the stats column are used for lots of different things, so keep them
	// around clear them and keep them around in a vector so we don't have 5 copies of the same
	// "reset unused entries" code around
	std::vector<sp<Label>> statsLabels;
	std::vector<sp<Label>> statsValues;
	for (int i = 0; i < 9; i++)
	{
		auto labelName = format("LABEL_%d", i + 1);
		auto label = form->findControlTyped<Label>(labelName);
		if (!label)
		{
			LogError("Failed to find UI control matching \"%s\"", labelName.cStr());
		}
		label->setText("");
		statsLabels.push_back(label);

		auto valueName = format("VALUE_%d", i + 1);
		auto value = form->findControlTyped<Label>(valueName);
		if (!value)
		{
			LogError("Failed to find UI control matching \"%s\"", valueName.cStr());
		}
		value->setText("");
		statsValues.push_back(value);
	}
	auto nameLabel = form->findControlTyped<Label>("NAME");
	auto iconGraphic = form->findControlTyped<Graphic>("SELECTED_ICON");

	form->render();
	equipState->render();
}

bool VEquipScreen::isTransition() { return false; }

void VEquipScreen::setSelectedVehicle(sp<Vehicle> vehicle)
{
	if (!vehicle)
	{
		LogError("Trying to set invalid selected vehicle");
		return;
	}
	LogInfo("Selecting vehicle \"%s\"", vehicle->name.cStr());
	this->selected = vehicle;
	auto backgroundImage = vehicle->type->equipment_screen;
	if (!backgroundImage)
	{
		LogError("Trying to view equipment screen of vehicle \"%s\" which has no equipment screen "
		         "background",
		         vehicle->type->name.cStr());
	}

	auto backgroundControl = form->findControlTyped<Graphic>("BACKGROUND");
	backgroundControl->setImage(backgroundImage);
	StateRef<Base> base;
	for (auto &b : state->player_bases)
	{
		if (b.second->building == selected->currentlyLandedBuilding)
			base = {state.get(), b.first};
	}
	sp<BaseVehicleInventory> inventory;
	if (base)
	{
		inventory = mksp<BaseVehicleInventory>(base);
	}

	this->equipState.reset(new VEquipState(this->selected, inventory,
	                                       form->findControl("PAPER_DOLL"),
	                                       form->findControlTyped<ListBox>("INVENTORY")));
}

} // namespace OpenApoc
