#pragma once

#include "library/rect.h"
#include "library/sp.h"
#include "library/strings.h"
#include "library/vec.h"
#include <list>
#include <utility>

namespace OpenApoc
{

class Image;

class EquipmentType
{
  public:
	virtual ~EquipmentType() = default;
	virtual sp<Image> getImage() const = 0;
	virtual Vec2<int> getGridSize() const = 0;
	virtual std::list<std::pair<UString, UString>> getStats() const = 0;
	virtual UString getName() const = 0;
};

class Equipment
{
  public:
	virtual ~Equipment() = default;

	virtual sp<EquipmentType> getType() const = 0;
	virtual sp<Image> getImage() const = 0;
};

class EquipmentSlot
{
  public:
	enum class AlignmentX
	{
		Left,
		Right,
		Centre,
	};
	enum class AlignmentY
	{
		Top,
		Bottom,
		Centre,
	};
	virtual ~EquipmentSlot() = default;
	virtual AlignmentX getAlignX() const = 0;
	virtual AlignmentY getAlignY() const = 0;
	virtual Rect<int> getBounds() const = 0;

	virtual bool canContainEquipment(const sp<Equipment> &) const = 0;
};

class EquippableObject
{
  public:
	virtual ~EquippableObject() = default;
	// The total number of slots in the grid
	virtual Vec2<int> getEquipGridSize() const = 0;
	// The size (in pixels) of each slot in the grid
	virtual Vec2<int> getGridSlotSize() const = 0;

	virtual bool canEquipAtPosition(Vec2<int> gridPosition, sp<Equipment> equipment) const = 0;
	virtual void equipAtPosition(Vec2<int> gridPosition, sp<Equipment> equipment) = 0;

	virtual sp<Equipment> getEquipmentAtPosition(Vec2<int> gridPosition) const = 0;
	virtual sp<Equipment> removeEquipmentAtPosition(Vec2<int> gridPosition) = 0;

	virtual const std::list<sp<EquipmentSlot>> &getEquipmentSlots() const = 0;
	virtual const std::list<std::pair<Vec2<int>, sp<Equipment>>> &getCurrentEquipment() const = 0;
};

class EquipmentStore
{
  public:
	virtual ~EquipmentStore() = default;

	virtual void returnEquipmentToStore(sp<Equipment>) = 0;

	virtual std::list<std::pair<sp<EquipmentType>, int>> getEquipment() const = 0;
};

} // namespace OpenApoc
