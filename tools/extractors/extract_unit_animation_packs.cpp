#include "framework/data.h"
#include "framework/framework.h"
#include "framework/palette.h"
#include "game/state/agent.h"
#include "game/state/battle/battleunitanimationpack.h"
#include "game/state/gamestate.h"
#include "library/strings_format.h"
#include "tools/extractors/common/animation.h"
#include "tools/extractors/extractors.h"

namespace OpenApoc
{
/*
// May be required in order to join together animations to create a longer one, i.e. Standing->Prone
from Standing->Kneeling and Kneeling->Prone
// But for now, not used
sp<BattleUnitAnimationPack::AnimationEntry>
combineAnimationEntries(sp<BattleUnitAnimationPack::AnimationEntry> e1,
sp<BattleUnitAnimationPack::AnimationEntry> e2)
{
    auto e = mksp<BattleUnitAnimationPack::AnimationEntry>();

    if (e1->is_overlay != e2->is_overlay)
        LogError("Incompatible entries: one is overlay, other isn't!");

    e->is_overlay = e1->is_overlay;
    e->frame_count = e1->frame_count + e2->frame_count;

    for (auto &f : e1->frames)
        e->frames.push_back(f);
    for (auto &f : e2->frames)
        e->frames.push_back(f);

    return e;
}
*/

sp<BattleUnitAnimationPack::AnimationEntry> InitialGameStateExtractor::getAnimationEntry(
    const std::vector<AnimationDataAD> &dataAD, const std::vector<AnimationDataUA> &dataUA,
    std::vector<AnimationDataUF> &dataUF, int index, Vec2<int> direction, int frames_per_100_units,
    int split_point, bool left_side, bool isOverlay, bool removeItem, Vec2<int> targetOffset,
    Vec2<int> beginOffset, bool inverse, int extraEndFrames, bool singleFrame) const
{
	static const std::map<Vec2<int>, int> offset_dir_map = {
	    {{0, -1}, 0}, {{1, -1}, 1}, {{1, 0}, 2},  {{1, 1}, 3},
	    {{0, 1}, 4},  {{-1, 1}, 5}, {{-1, 0}, 6}, {{-1, -1}, 7},
	};

	auto e = mksp<BattleUnitAnimationPack::AnimationEntry>();

	int offset_dir = offset_dir_map.at(direction);
	int offset_ua = dataAD[index * 8 + offset_dir].offset;
	int offset_uf = dataUA[offset_ua].offset;
	int from = left_side ? 0 : split_point;
	int to = singleFrame ? from + 1 : (left_side ? split_point : dataUA[offset_ua].frame_count);

	for (int i = from; i < to; i++)
	{
		int k = inverse ? to - i - 1 : i;

		int x_offset = (to == from + 1) ? (beginOffset.x + targetOffset.x) / 2
		                                : beginOffset.x * (to - from - 1 - i) / (to - from - 1) +
		                                      targetOffset.x * (i - from) / (to - from - 1);
		int y_offset = (to == from + 1) ? (beginOffset.y + targetOffset.y) / 2
		                                : beginOffset.y * (to - from - 1 - i) / (to - from - 1) +
		                                      targetOffset.y * (i - from) / (to - from - 1);

		auto data = dataUF[offset_uf + k];

		e->frames.push_back(BattleUnitAnimationPack::AnimationEntry::Frame());

		for (int j = 0; j < 7; j++)
		{
			int part_idx = data.draw_order[j];
			auto part_type = BattleUnitAnimationPack::AnimationEntry::Frame::UnitImagePart::Shadow;
			switch (part_idx)
			{
				case 0:
					part_type =
					    BattleUnitAnimationPack::AnimationEntry::Frame::UnitImagePart::Shadow;
					break;
				case 1:
					part_type = BattleUnitAnimationPack::AnimationEntry::Frame::UnitImagePart::Body;
					break;
				case 2:
					part_type = BattleUnitAnimationPack::AnimationEntry::Frame::UnitImagePart::Legs;
					break;
				case 3:
					part_type =
					    BattleUnitAnimationPack::AnimationEntry::Frame::UnitImagePart::Helmet;
					break;
				case 4:
					part_type =
					    BattleUnitAnimationPack::AnimationEntry::Frame::UnitImagePart::LeftArm;
					break;
				case 5:
					part_type =
					    BattleUnitAnimationPack::AnimationEntry::Frame::UnitImagePart::RightArm;
					break;
				case 6:
					part_type =
					    BattleUnitAnimationPack::AnimationEntry::Frame::UnitImagePart::Weapon;
					if (removeItem)
						continue;
					break;
				default:
					LogError("Impossible part index %d found in UF located at entry %d offset %d",
					         part_idx, offset_uf, j);
					break;
			}
			e->frames[i - from].unit_image_draw_order.push_back(part_type);
			e->frames[i - from].unit_image_parts[part_type] =
			    BattleUnitAnimationPack::AnimationEntry::Frame::InfoBlock(
			        data.parts[part_idx].frame_idx, data.parts[part_idx].x_offset + x_offset,
			        data.parts[part_idx].y_offset + y_offset);
		}
	}

	for (int i = 0; i < extraEndFrames; i++)
	{
		e->frames.push_back(e->frames.back());
	}

	e->is_overlay = isOverlay;
	e->frame_count = e->frames.size();
	e->frames_per_100_units = frames_per_100_units;

	return e;
}

// Get Prone offset for facing
Vec2<int> InitialGameStateExtractor::gPrOff(Vec2<int> facing) const
{
	// 48 = tile_x, 24 = tile_y
	return {(facing.x - facing.y) * 48 / 8, (facing.x + facing.y) * 24 / 8};
}
sp<BattleUnitAnimationPack>
InitialGameStateExtractor::extractAnimationPack(GameState &state, const UString &path,
                                                const UString &name) const
{
	std::ignore = state;
	UString dirName = "xcom3/tacdata/";

	auto p = mksp<BattleUnitAnimationPack>();

	std::vector<AnimationDataAD> dataAD;
	{
		auto fileName = format("%s%s%s", dirName, path, ".ad");

		auto inFile = fw().data->fs.open(fileName);
		if (inFile)
		{
			auto fileSize = inFile.size();
			auto objectCount = fileSize / sizeof(struct AnimationDataAD);

			for (unsigned i = 0; i < objectCount; i++)
			{
				AnimationDataAD data;
				inFile.read((char *)&data, sizeof(data));
				if (!inFile)
				{
					LogError("Failed to read entry in \"%s\"", fileName);
					return nullptr;
				}
				dataAD.push_back(data);
			}
		}
	}

	std::vector<AnimationDataUA> dataUA;
	{
		auto fileName = format("%s%s%s", dirName, path, ".ua");

		auto inFile = fw().data->fs.open(fileName);
		if (inFile)
		{
			auto fileSize = inFile.size();
			auto objectCount = fileSize / sizeof(struct AnimationDataUA);

			for (unsigned i = 0; i < objectCount; i++)
			{
				AnimationDataUA data;
				inFile.read((char *)&data, sizeof(data));
				if (!inFile)
				{
					LogError("Failed to read entry in \"%s\"", fileName);
					return nullptr;
				}
				dataUA.push_back(data);
			}
		}
	}

	std::vector<AnimationDataUF> dataUF;
	{
		auto fileName = format("%s%s%s", dirName, path, ".uf");

		auto inFile = fw().data->fs.open(fileName);
		if (inFile)
		{
			auto fileSize = inFile.size();
			auto objectCount = fileSize / sizeof(struct AnimationDataUF);

			for (unsigned i = 0; i < objectCount; i++)
			{
				AnimationDataUF data;
				inFile.read((char *)&data, sizeof(data));
				if (!inFile)
				{
					LogError("Failed to read entry in \"%s\"", fileName);
					return nullptr;
				}
				dataUF.push_back(data);
			}
		}
	}

	// It is not understood in what order animations come in .AD files.
	// Plus, some simplier aliens are missing those files alltogether.
	// Therefore, we have to parse them all manually here. I see no other option

	if (name == "unit")
	{
		for (int x = -1; x <= 1; x++)
		{
			for (int y = -1; y <= 1; y++)
			{
				// 0, 0 facing does not exist
				if (x == 0 && y == 0)
					continue;

				extractAnimationPackUnit(p, dataAD, dataUA, dataUF, x, y);
			}
		}
	}
	if (name == "bsk")
	{
	}
	if (name == "chrys1")
	{
	}
	if (name == "chrys2")
	{
	}
	if (name == "gun")
	{
	}
	if (name == "hypr")
	{
	}
	if (name == "mega")
	{
		// Buggy death animations, take last frame from dropping animation
	}
	if (name == "micro")
	{
	}
	if (name == "multi")
	{
	}
	if (name == "mwegg1")
	{
	}
	if (name == "mwegg2")
	{
	}
	if (name == "popper")
	{
	}
	if (name == "psi")
	{
	}
	if (name == "queen")
	{
	}
	if (name == "spitr")
	{
	}
	if (name == "civ")
	{
	}

	return p;
}
}
