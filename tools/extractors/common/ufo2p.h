#pragma once

#include "game/state/organisation.h"
#include "game/state/rules/aequipment_type.h"
#include "game/state/rules/facility_type.h"
#include "game/state/rules/vammo_type.h"
#include "game/state/rules/vehicle_type.h"
#include "game/state/rules/vequipment_type.h"
#include "tools/extractors/common/audio.h"
#include "tools/extractors/common/baselayout.h"
#include "tools/extractors/common/building.h"
#include "tools/extractors/common/bulletsprite.h"
#include "tools/extractors/common/canonstring.h"
#include "tools/extractors/common/datachunk.h"
#include "tools/extractors/common/facilities.h"
#include "tools/extractors/common/organisations.h"
#include "tools/extractors/common/research.h"
#include "tools/extractors/common/scenerytile.h"
#include "tools/extractors/common/strtab.h"
#include "tools/extractors/common/ufopaedia.h"
#include "tools/extractors/common/vehicle.h"
#include "tools/extractors/common/vequipment.h"
#include <algorithm>
#include <memory>
#include <string>

namespace OpenApoc
{

class UFO2P
{
  public:
	UFO2P(std::string fileName = "XCOM3/UFOEXE/UFO2P.EXE");
	std::unique_ptr<StrTab> research_names;
	std::unique_ptr<StrTab> research_descriptions;
	std::unique_ptr<DataChunk<ResearchData>> research_data;

	std::unique_ptr<StrTab> vehicle_names;
	std::unique_ptr<StrTab> organisation_names;
	std::unique_ptr<StrTab> building_names;
	std::unique_ptr<StrTab> building_functions;
	std::unique_ptr<StrTab> alien_building_names;

	std::unique_ptr<DataChunk<VehicleData>> vehicle_data;

	std::unique_ptr<StrTab> ufopaedia_group;

	std::unique_ptr<DataChunk<RawSoundData>> rawsound;
	std::unique_ptr<DataChunk<BaseLayoutData>> baselayouts;

	std::unique_ptr<StrTab> vehicle_equipment_names;

	std::unique_ptr<DataChunk<VehicleEquipmentData>> vehicle_equipment;
	std::unique_ptr<DataChunk<VehicleWeaponData>> vehicle_weapons;
	std::unique_ptr<DataChunk<VehicleEngineData>> vehicle_engines;
	std::unique_ptr<DataChunk<VehicleGeneralEquipmentData>> vehicle_general_equipment;

	std::unique_ptr<DataChunk<VehicleEquipmentLayout>> vehicle_equipment_layouts;

	std::unique_ptr<StrTab> facility_names;
	std::unique_ptr<DataChunk<FacilityData>> facility_data;

	std::unique_ptr<DataChunk<SceneryMinimapColour>> scenery_minimap_colour;

	std::unique_ptr<DataChunk<BulletSprite>> bullet_sprites;
	std::unique_ptr<DataChunk<ProjectileSprites>> projectile_sprites;

	UString getOrgId(int idx)
	{
		return Organisation::getPrefix() + canon_string(this->organisation_names->get(idx));
	}
	UString getFacilityId(int idx)
	{
		return FacilityType::getPrefix() + canon_string(this->facility_names->get(idx));
	}
	UString getVequipmentId(int idx)
	{
		return VEquipmentType::getPrefix() + canon_string(this->vehicle_equipment_names->get(idx));
	}
	UString getVehicleId(int idx)
	{
		return VehicleType::getPrefix() + canon_string(this->vehicle_names->get(idx));
	}
};

UFO2P &getUFO2PData();

static inline std::string toLower(std::string input)
{
	std::transform(input.begin(), input.end(), input.begin(), ::tolower);
	return input;
}
} // namespace OpenApoc
