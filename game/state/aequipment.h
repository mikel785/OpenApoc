#pragma once
#include "game/state/agent.h"
#include "game/state/battle/battle.h"
#include "game/state/gametime.h"
#include "library/sp.h"
#include "library/vec.h"

namespace OpenApoc
{
// Items recharge their recharge rate of ammo every 4 seconds (or fully recharge every turn in TB)
static const unsigned TICKS_PER_RECHARGE = TICKS_PER_TURN;

// How many shots does an exploded payload fire (max)
static const unsigned MAX_PAYLOAD_EXPLOSION_SHOTS = 10;

class BattleItem;
class BattleUnit;
class Organisation;
class Projectile;
class AEquipmentType;
enum class TriggerType;
enum class WeaponAimingMode;

class AEquipment : public std::enable_shared_from_this<AEquipment>
{
  public:
	AEquipment();
	~AEquipment() = default;

	StateRef<AEquipmentType> type;
	// Type of loaded ammunition
	StateRef<AEquipmentType> payloadType;
	// Function to simplify getting payload for weapons and grenades
	StateRef<AEquipmentType> getPayloadType() const;

	Vec2<int> equippedPosition;
	AEquipmentSlotType equippedSlotType = AEquipmentSlotType::General;
	// Agent in who's inventory this item is located
	StateRef<Agent> ownerAgent;
	// Organization which brought this item to the battle
	StateRef<Organisation> ownerOrganisation;
	// Unit that threw or dropped the item last
	StateRef<BattleUnit> ownerUnit;

	// Ammunition for weapons, protection for armor, charge for items
	int ammo = 0;

	// Explosives parameters

	// If set, will activate upon leaving agent inventory
	bool primed = false;
	// If set, will count down timer and go off when trigger condition is satisfied
	bool activated = false;
	// Delay until trigger is activated, in ticks
	unsigned int triggerDelay = 0;
	// Range for proximity, same scale as TUs (4 linear, 6 diagonal etc.)
	float triggerRange = 0.0f;
	// Type of trigger used
	TriggerType triggerType;

	void prime(bool onImpact = true, int triggerDelay = 0, float triggerRange = 0.0f);
	void explode(GameState &state);

	// Ticks until recharge rate is added to ammo for anything that recharges
	unsigned int recharge_ticks_accumulated = 0;

	// Weapon parameters

	// Weapon is ready to fire now
	bool readyToFire = false;
	// Ticks until weapon is ready to fire
	unsigned int weapon_fire_ticks_remaining = 0;
	// Aiming mode for the weapon
	WeaponAimingMode aimingMode;

	// In use, for medikit and motion scanner
	bool inUse = false;

	int getAccuracy(BodyState bodyState, MovementState movementState, WeaponAimingMode fireMode,
	                bool thrown = false);

	bool isFiring() const { return weapon_fire_ticks_remaining > 0 || readyToFire; };
	bool canFire() const;
	bool canFire(Vec3<float> to) const;
	bool needsReload() const;
	void stopFiring();
	void startFiring(WeaponAimingMode fireMode);

	// Support nullptr ammoItem for auto-reloading
	void loadAmmo(GameState &state, sp<AEquipment> ammoItem = nullptr);

	void update(GameState &state, unsigned int ticks);

	// Wether this weapon works like brainsucker launcher, throwing it's ammunition instead of
	// firing a projectile
	bool isLauncher();
	void fire(GameState &state, Vec3<float> targetPosition,
	          StateRef<BattleUnit> targetUnit = nullptr);
	void throwItem(GameState &state, Vec3<int> targetPosition, float velocityXY, float velocityZ,
	               bool launch = false);

	bool getVelocityForThrow(const TileMap &map, int strength, Vec3<float> startPos,
	                         Vec3<int> target, float &velocityXY, float &velocityZ) const;
	bool getVelocityForThrow(const BattleUnit &unit, Vec3<int> target, float &velocityXY,
	                         float &velocityZ) const;
	bool getVelocityForLaunch(const BattleUnit &unit, Vec3<int> target, float &velocityXY,
	                          float &velocityZ) const;

  private:
	static float getMaxThrowDistance(int weight, int strength, int heightDifference);
	static bool calculateNextVelocityForThrow(float distanceXY, float diffZ, float &velocityXY,
	                                          float &velocityZ);
	static bool getVelocityForThrowLaunch(const BattleUnit *unit, const TileMap &map, int strength,
	                                      int weight, Vec3<float> startPos, Vec3<int> target,
	                                      float &velocityXY, float &velocityZ);

  public:
	// Following members are not serialized, but rather are set in initBattle method

	wp<BattleItem> ownerItem;
};
} // namespace OpenApoc
