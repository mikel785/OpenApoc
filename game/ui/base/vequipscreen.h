#pragma once

#include "framework/stage.h"
#include "game/state/rules/vequipment_type.h"
#include "library/colour.h"
#include "library/rect.h"
#include "library/sp.h"
#include <map>

namespace OpenApoc
{

class Vehicle;
class Form;
class Palette;
class VEquipment;
class BitmapFont;
class GameState;
class Control;
class VEquipmentType;

class VEquipState;

class VEquipScreen : public Stage
{
  private:
	sp<Form> form;
	sp<Palette> pal;
	sp<BitmapFont> labelFont;

	up<VEquipState> equipState;
	sp<GameState> state;

	sp<Vehicle> selected;
	VEquipmentType::Type selectionType;

	float glowCounter;

  public:
	VEquipScreen(sp<GameState> state);
	~VEquipScreen() override;

	void begin() override;
	void pause() override;
	void resume() override;
	void finish() override;
	void eventOccurred(Event *e) override;
	void update() override;
	void render() override;
	bool isTransition() override;

	void setSelectedVehicle(sp<Vehicle> vehicle);
};

} // namespace OpenApoc
