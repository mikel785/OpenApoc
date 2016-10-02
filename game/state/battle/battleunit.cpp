#include "game/state/battle/battleunit.h"
#include "framework/framework.h"
#include "game/state/battle/battle.h"
#include "game/state/battle/battleunitanimationpack.h"
#include "game/state/gamestate.h"
#include "game/state/tileview/collision.h"
#include "game/state/tileview/tileobject_battleunit.h"
#include "game/state/tileview/tileobject_shadow.h"
#include <cmath>

namespace OpenApoc
{

void BattleUnit::removeFromSquad()
{
	auto b = battle.lock();
	if (!b)
	{
		LogError("removeFromSquad - Battle disappeared");
	}
	b->forces[owner].removeAt(squadNumber, squadPosition);
}

bool BattleUnit::assignToSquad(int squad)
{
	auto b = battle.lock();
	if (!b)
	{
		LogError("assignToSquad - Battle disappeared");
	}
	return b->forces[owner].insert(squad, shared_from_this());
}

void BattleUnit::moveToSquadPosition(int position)
{
	auto b = battle.lock();
	if (!b)
	{
		LogError("moveToSquadPosition - Battle disappeared");
	}
	b->forces[owner].insertAt(squadNumber, position, shared_from_this());
}

bool BattleUnit::isFatallyWounded()
{
	for (auto e : fatalWounds)
	{
		if (e.second > 0)
		{
			return true;
		}
	}
	return false;
}

void BattleUnit::setPosition(const Vec3<float> &pos)
{
	this->position = pos;
	if (!this->tileObject)
	{
		LogError("setPosition called on unit with no tile object");
		return;
	}
	else
	{
		this->tileObject->setPosition(pos);
	}

	if (this->shadowObject)
	{
		this->shadowObject->setPosition(this->tileObject->getCenter());
	}
}

void BattleUnit::resetGoal()
{
	goalPosition = position;
	goalFacing = facing;
}

StateRef<AEquipmentType> BattleUnit::getDisplayedItem() const
{
	if (missions.size() > 0)
	{
		for (auto &m : missions)
		{
			auto item = m->item;
			if (item)
			{
				return item->type;
			}
		}
	}
	return agent->getDominantItemInHands();
}

bool BattleUnit::canAfford(int cost) const
{ 
	auto b = battle.lock();
	if (!b)
	{
		LogError("canAfford: Battle disappeared!");
		return false;
	}
	if (b->mode == Battle::Mode::RealTime)
	{
		return true;
	}
	return agent->modified_stats.time_units >= cost;
}

bool BattleUnit::spendTU(int cost)
{
	auto b = battle.lock();
	if (!b)
	{
		LogError("spendTU: Battle disappeared!");
		return false;
	}
	if (b->mode == Battle::Mode::RealTime)
	{
		return true;
	}
	if (cost > agent->modified_stats.time_units)
	{
		return false;
	}
	agent->modified_stats.time_units -= cost;
	return true;
}

int BattleUnit::getMaxHealth() const { return this->agent->current_stats.health; }

int BattleUnit::getHealth() const { return this->agent->modified_stats.health; }

int BattleUnit::getMaxShield() const
{
	int maxShield = 0;

	for (auto &e : this->agent->equipment)
	{
		if (e->type->type != AEquipmentType::Type::DisruptorShield)
			continue;
		maxShield += e->type->max_ammo;
	}

	return maxShield;
}

int BattleUnit::getShield() const
{
	int curShield = 0;

	for (auto &e : this->agent->equipment)
	{
		if (e->type->type != AEquipmentType::Type::DisruptorShield)
			continue;
		curShield += e->ammo;
	}

	return curShield;
}

int BattleUnit::getStunDamage() const
{
	// FIXME: Figure out stun damage scale
	int SCALE = TICKS_PER_SECOND;
	return stunDamageInTicks / SCALE;
}

void BattleUnit::dealStunDamage(int damage)
{
	// FIXME: Figure out stun damage scale
	int SCALE = TICKS_PER_SECOND;
	stunDamageInTicks += damage* SCALE;
}

bool BattleUnit::isDead() const { return getHealth() == 0 || destroyed; }

bool BattleUnit::isUnconscious() const { return !isDead() && getStunDamage() >= getHealth(); }

bool BattleUnit::isConscious() const
{
	return !isDead() && getStunDamage() < getHealth() &&
	       (current_body_state != AgentType::BodyState::Downed ||
	        target_body_state != AgentType::BodyState::Downed);
}

bool BattleUnit::isStatic() const
{
	return current_movement_state == AgentType::MovementState::None && !falling &&
	       current_body_state == target_body_state;
}

bool BattleUnit::isBusy() const
{
	// FIXME: handle units busy with firing, aiming or other stuff
	return !isStatic() || false;
}

bool BattleUnit::isThrowing() const
{
	bool throwing = false;
	for (auto &m : missions)
	{
		if (m->type == BattleUnitMission::MissionType::ThrowItem)
		{
			throwing = true;
			break;
		}
	}
	return throwing;
}

bool BattleUnit::canFly() const
{
	return isConscious() && agent->isBodyStateAllowed(AgentType::BodyState::Flying);
}

bool BattleUnit::canMove() const
{
	if (!isConscious())
	{
		return false;
	}
	if (agent->isMovementStateAllowed(AgentType::MovementState::Normal) ||
	    agent->isMovementStateAllowed(AgentType::MovementState::Running))
	{
		return true;
	}
	return false;
}

bool BattleUnit::canProne(Vec3<int> pos, Vec2<int> fac) const
{
	if (isLarge())
	{
		LogError("Large unit attempting to go prone? WTF? Should large units ever acces this?");
		return false;
	}
	// Check if agent can go prone and stand in its current tile
	if (!agent->isBodyStateAllowed(AgentType::BodyState::Prone) ||
	    !tileObject->getOwningTile()->getCanStand())
		return false;
	// Check if agent can put legs in the tile behind. Conditions
	// 1) Target tile provides standing ability
	// 2) Target tile height is not too big compared to current tile
	// 3) Target tile is passable
	// 4) Target tile has no unit occupying it (other than us)
	Vec3<int> legsPos = pos - Vec3<int>{fac.x, fac.y, 0};
	if ((legsPos.x >= 0) && (legsPos.x < tileObject->map.size.x) && (legsPos.y >= 0) &&
	    (legsPos.y < tileObject->map.size.y) && (legsPos.z >= 0) &&
	    (legsPos.z < tileObject->map.size.z))
	{
		auto bodyTile = tileObject->map.getTile(pos);
		auto legsTile = tileObject->map.getTile(legsPos);
		if (legsTile->canStand && std::abs(legsTile->height - bodyTile->height) <= 0.25f &&
		    legsTile->getPassable(false,
		                          agent->type->bodyType->height.at(AgentType::BodyState::Prone))
			&& (legsPos == (Vec3<int>)position || !legsTile->getUnitIfPresent(true, true)))
		{
			return true;
		}
	}
	return false;
}

bool BattleUnit::canKneel() const
{
	if (!agent->isBodyStateAllowed(AgentType::BodyState::Kneeling) ||
	    !tileObject->getOwningTile()->getCanStand(isLarge()))
		return false;
	return true;
}

// FIXME: Apply damage to the unit
bool BattleUnit::applyDamage(GameState &state, int damage, float armour)
{
	std::ignore = state;
	std::ignore = damage;
	std::ignore = armour;
	// if (this->shield <= damage)
	//{
	//	if (this->shield > 0)
	//	{
	//		damage -= this->shield;
	//		this->shield = 0;

	//		// destroy the shield modules
	//		for (auto it = this->equipment.begin(); it != this->equipment.end();)
	//		{
	//			if ((*it)->type->type == VEquipmentType::Type::General &&
	//				(*it)->type->shielding > 0)
	//			{
	//				it = this->equipment.erase(it);
	//			}
	//			else
	//			{
	//				++it;
	//			}
	//		}
	//	}

	//	damage -= armour;
	//	if (damage > 0)
	//	{
	//		this->health -= damage;
	//		if (this->health <= 0)
	//		{
	//			this->health = 0;
	//			return true;
	//		}
	//		else if (isCrashed())
	//		{
	//			this->missions.clear();
	//			this->missions.emplace_back(VehicleMission::crashLand(*this));
	//			this->missions.front()->start(state, *this);
	//			return false;
	//		}
	//	}
	//}
	// else
	//{
	//	this->shield -= damage;
	//}
	return false;
}

// FIXME: Handle unit's collision with projectile
void BattleUnit::handleCollision(GameState &state, Collision &c)
{
	std::ignore = state;

	if (!this->tileObject)
	{
		LogError("It's possible multiple projectiles hit the same tile in the same tick (?)");
		return;
	}

	auto projectile = c.projectile.get();
	if (projectile)
	{
		// auto vehicleDir = glm::round(this->facing);
		// auto projectileDir = glm::normalize(projectile->getVelocity());
		// auto dir = vehicleDir + projectileDir;
		// dir = glm::round(dir);

		// auto armourDirection = VehicleType::ArmourDirection::Right;
		// if (dir.x == 0 && dir.y == 0 && dir.z == 0)
		//{
		//	armourDirection = VehicleType::ArmourDirection::Front;
		//}
		// else if (dir * 0.5f == vehicleDir)
		//{
		//	armourDirection = VehicleType::ArmourDirection::Rear;
		//}
		//// FIXME: vehicle Z != 0
		// else if (dir.z < 0)
		//{
		//	armourDirection = VehicleType::ArmourDirection::Top;
		//}
		// else if (dir.z > 0)
		//{
		//	armourDirection = VehicleType::ArmourDirection::Bottom;
		//}
		// else if ((vehicleDir.x == 0 && dir.x != dir.y) || (vehicleDir.y == 0 && dir.x == dir.y))
		//{
		//	armourDirection = VehicleType::ArmourDirection::Left;
		//}

		// float armourValue = 0.0f;
		// auto armour = this->type->armour.find(armourDirection);
		// if (armour != this->type->armour.end())
		//{
		//	armourValue = armour->second;
		//}

		// if (applyDamage(state, projectile->damage, armourValue))
		//{
		//	auto doodad = city->placeDoodad(StateRef<DoodadType>{&state, "DOODAD_EXPLOSION_2"},
		//		this->tileObject->getPosition());

		//	this->shadowObject->removeFromMap();
		//	this->tileObject->removeFromMap();
		//	this->shadowObject.reset();
		//	this->tileObject.reset();
		//	state.vehicles.erase(this->getId(state, this->shared_from_this()));
		//	return;
		//}
	}
}

void BattleUnit::update(GameState &state, unsigned int ticks)
{
	static const std::set<TileObject::Type> mapPartSet = {
	    TileObject::Type::Ground, TileObject::Type::LeftWall, TileObject::Type::RightWall,
	    TileObject::Type::Feature};

	if (destroyed || retreated)
	{
		return;
	}

	auto b = battle.lock();
	if (!b)
		return;
	auto &map = tileObject->map;


	if (!this->missions.empty())
		this->missions.front()->update(state, *this, ticks);

	// FIXME: Regenerate stamina

	// Stun removal
	if (stunDamageInTicks > 0)
	{
		stunDamageInTicks = std::max(0, stunDamageInTicks - (int)ticks);
	}

	// Attempt rising if laying on the ground and not unconscious
	if (!isUnconscious() && !isConscious())
	{
		tryToRiseUp(state);
	}

	// Fatal wounds / healing
	if (isFatallyWounded() && !isDead())
	{
		bool unconscious = isUnconscious();
		// FIXME: Get a proper fatal wound scale
		int TICKS_PER_FATAL_WOUND_DAMAGE = TICKS_PER_SECOND;
		woundTicksAccumulated += ticks;
		while (woundTicksAccumulated > TICKS_PER_FATAL_WOUND_DAMAGE)
		{
			woundTicksAccumulated -= TICKS_PER_FATAL_WOUND_DAMAGE;
			for (auto &w : fatalWounds)
			{
				if (w.second > 0)
				{
					if (isHealing && healingBodyPart == w.first)
					{
						w.second--;
					}
					else
					{
						// FIXME: Deal damage in a unified way so we don't have to check 
						// for unconscious and dead manually !
						agent->modified_stats.health -= w.second;
					}
				}
			}
		}
		// If fully healed
		if (!isFatallyWounded())
		{
			isHealing = false;
		}
		// If died or went unconscious
		if (isDead())
		{
			die(state, true);
		}
		if (!unconscious && isUnconscious())
		{
			fallUnconscious(state);
		}
	} // End of Fatal Wounds and Healing

	// Idling check
	if (missions.empty() && isConscious())
	{
		if (falling)
		{
			LogError("Unit falling without a mission, wtf?");
		}
		if (goalFacing != facing)
		{
			LogError("Unit turning without a mission, wtf?");
		}
		if (target_body_state != current_body_state)
		{
			LogError("Unit changing body state without a mission, wtf?");
		}
		// Try giving way if asked to
		// FIXME: Ensure we're not in a firefight before giving way!
		if (giveWayRequest.size() > 0)
		{
			// If we're given a giveWay request 0, 0 it means we're asked to kneel temporarily
			if (giveWayRequest.size() == 1 && giveWayRequest.front().x == 0 &&
			    giveWayRequest.front().y == 0 && canAfford(BattleUnitMission::getBodyStateChangeCost(target_body_state, AgentType::BodyState::Kneeling)))
			{
				// Give time for that unit to pass
				addMission(state, BattleUnitMission::snooze(*this, TICKS_PER_SECOND), false);
				// Give way
				addMission(state, BattleUnitMission::changeStance(*this, AgentType::BodyState::Kneeling));
			}
			else
			{
				auto from = tileObject->getOwningTile();
				for (auto newHeading : giveWayRequest)
				{
					for (int z = -1; z <= 1; z++)
					{
						if (z < 0 || z >= map.size.z)
						{
							continue;
						}
						// Try the new heading
						Vec3<int> pos = {position.x + newHeading.x, position.y + newHeading.y,
						                 position.z + z};
						auto to = map.getTile(pos);
						// Check if heading on our level is acceptable
						bool acceptable = BattleUnitTileHelper{ map, *this }.canEnterTile(from, to) &&
							BattleUnitTileHelper{ map, *this }.canEnterTile(to, from);
						// If not, check if we can go down one tile
						if (!acceptable && pos.z - 1 >= 0)
						{
							pos -= Vec3<int>{0, 0, 1};
							to = map.getTile(pos);
							acceptable = BattleUnitTileHelper{ map, *this }.canEnterTile(from, to) &&
								BattleUnitTileHelper{ map, *this }.canEnterTile(to, from);
						}
						// If not, check if we can go up one tile
						if (!acceptable && pos.z +2 < map.size.z)
						{
							pos += Vec3<int>{0, 0, 2};
							to = map.getTile(pos);
							acceptable = BattleUnitTileHelper{ map, *this }.canEnterTile(from, to) &&
								BattleUnitTileHelper{ map, *this }.canEnterTile(to, from);
						}
						if (acceptable)
						{
							// 05: Turn to previous facing
							addMission(state, BattleUnitMission::turn(*this, facing), false);
							// 04: Return to our position after we're done
							addMission(state, BattleUnitMission::gotoLocation(*this, position, 0, false), false);
							// 03: Give time for that unit to pass
							addMission(state, BattleUnitMission::snooze(*this, 60), false);
							// 02: Turn to previous facing
							addMission(state, BattleUnitMission::turn(*this, facing), false);
							// 01: Give way (move 1 tile away)
							addMission(state, BattleUnitMission::gotoLocation(*this, pos, 0, false));
						}
						if (!missions.empty())
						{
							break;
						}
					}
					if (!missions.empty())
					{
						break;
					}
				}
			}
			giveWayRequest.clear();
		}  
		else // if not giving way
		{
			setMovementState(AgentType::MovementState::None);
			// Kneel if not kneeling and should kneel
			if (kneeling_mode == KneelingMode::Kneeling &&
			    current_body_state != AgentType::BodyState::Kneeling && canKneel()
				&& canAfford(BattleUnitMission::getBodyStateChangeCost(target_body_state, AgentType::BodyState::Kneeling)))
			{
				addMission(state, BattleUnitMission::changeStance(*this, AgentType::BodyState::Kneeling));
			}
			// Go prone if not prone and should stay prone
			else if (movement_mode == BattleUnit::MovementMode::Prone &&
			         current_body_state != AgentType::BodyState::Prone &&
			         kneeling_mode != KneelingMode::Kneeling && canProne(position, facing)
				&& canAfford(BattleUnitMission::getBodyStateChangeCost(target_body_state, AgentType::BodyState::Prone)))
			{
				addMission(state, BattleUnitMission::changeStance(*this, AgentType::BodyState::Prone));
			}
			// Stand up if not standing up and should stand up
			else if ((movement_mode == BattleUnit::MovementMode::Walking ||
			          movement_mode == BattleUnit::MovementMode::Running) &&
			         kneeling_mode != KneelingMode::Kneeling &&
			         current_body_state != AgentType::BodyState::Standing &&
			         current_body_state != AgentType::BodyState::Flying)
			{
				if (agent->isBodyStateAllowed(AgentType::BodyState::Standing))
				{
					if (canAfford(BattleUnitMission::getBodyStateChangeCost(target_body_state, AgentType::BodyState::Standing)))
					{
						addMission(state, BattleUnitMission::changeStance(*this, AgentType::BodyState::Standing));
					}
				}
				else
				{
					if (canAfford(BattleUnitMission::getBodyStateChangeCost(target_body_state, AgentType::BodyState::Flying)))
					{
						addMission(state, BattleUnitMission::changeStance(*this, AgentType::BodyState::Flying));
					}
				}
			}
			// Stop flying if we can stand
			else if (current_body_state == AgentType::BodyState::Flying &&
			         tileObject->getOwningTile()->getCanStand(isLarge()) &&
			         agent->isBodyStateAllowed(AgentType::BodyState::Standing)
				&& canAfford(BattleUnitMission::getBodyStateChangeCost(target_body_state, AgentType::BodyState::Standing)))
			{
				addMission(state, BattleUnitMission::changeStance(*this, AgentType::BodyState::Standing));
			}
			// Stop being prone if legs are no longer supported and we haven't taken a mission yet
			if (current_body_state == AgentType::BodyState::Prone && missions.empty())
			{
				bool hasSupport = true;
				for (auto t : tileObject->occupiedTiles)
				{
					if (!map.getTile(t)->getCanStand())
					{
						hasSupport = false;
						break;
					}
				}
				if (!hasSupport && canAfford(BattleUnitMission::getBodyStateChangeCost(target_body_state, AgentType::BodyState::Kneeling)))
				{
					addMission(state, BattleUnitMission::changeStance(*this, AgentType::BodyState::Kneeling));
				}
			}
		}
	} // End of Idling

	// Movement and Body Animation
	{
		bool wasUsingLift = usingLift;
		usingLift = false;

		// Turn off Jetpacks
		if (current_body_state != AgentType::BodyState::Flying)
		{
			flyingSpeedModifier = 0;
		}

		// If not running we will consume these twice as fast
		int moveTicksRemaining = ticks * agent->modified_stats.getActualSpeedValue() * 2;
		int bodyTicksRemaining = ticks;
		int handTicksRemaining = ticks;
		int turnTicksRemaining = ticks;

		int lastMoveTicksRemaining = 0;
		int lastBodyTicksRemaining = 0;
		int lastHandTicksRemaining = 0;
		int lastTurnTicksRemaining = 0;

		while (lastMoveTicksRemaining != moveTicksRemaining ||
		       lastBodyTicksRemaining != bodyTicksRemaining ||
		       lastHandTicksRemaining != handTicksRemaining ||
		       lastTurnTicksRemaining != turnTicksRemaining)
		{
			lastMoveTicksRemaining = moveTicksRemaining;
			lastBodyTicksRemaining = bodyTicksRemaining;
			lastHandTicksRemaining = handTicksRemaining;
			lastTurnTicksRemaining = turnTicksRemaining;

			// STEP 01: Begin falling or changing stance to flying if appropriate
			if (!fallingQueued)
			{
				// Check if should fall or start flying
				if (!canFly() || current_body_state != AgentType::BodyState::Flying)
				{
					bool hasSupport = false;
					bool fullySupported = true;
					if (tileObject->getOwningTile()->getCanStand(isLarge()))
					{
						hasSupport = true;
					}
					else
					{
						fullySupported = false;
					}
					if (!atGoal)
					{
						if (map.getTile(goalPosition)->getCanStand(isLarge()))
						{
							hasSupport = true;
						}
						else
						{
							fullySupported = false;
						}
					}
					// If not flying and has no support - fall!
					if (!hasSupport && !canFly())
					{
						addMission(state, BattleUnitMission::MissionType::Fall);
					}
					// If flying and not supported both on current and goal locations - start flying
					if (!fullySupported && canFly())
					{
						if (current_body_state == target_body_state)
						{
							setBodyState(AgentType::BodyState::Flying);
						}
					}
				}
			}

			// Change body state
			if (bodyTicksRemaining > 0)
			{
				if (body_animation_ticks_remaining > bodyTicksRemaining)
				{
					body_animation_ticks_remaining -= bodyTicksRemaining;
					bodyTicksRemaining = 0;
				}
				else
				{
					if (current_body_state != target_body_state)
					{
						bodyTicksRemaining -= body_animation_ticks_remaining;
						setBodyState(target_body_state);
					}
					// Pop finished missions if present
					if (popFinishedMissions(state))
					{
						return;
					}
					// Try to get new body state change
					AgentType::BodyState nextState = AgentType::BodyState::Downed;
					int nextTicks = 0;
					if (getNextBodyState(state, nextState, nextTicks))
					{
						beginBodyStateChange(nextState, nextTicks);
					}
				}
			}

			// Try changing hand state
			if (handTicksRemaining > 0)
			{
				if (hand_animation_ticks_remaining > handTicksRemaining)
				{
					hand_animation_ticks_remaining -= handTicksRemaining;
					handTicksRemaining = 0;
				}
				else
				{
					if (current_hand_state != target_hand_state)
					{
						handTicksRemaining -= hand_animation_ticks_remaining;
						hand_animation_ticks_remaining = 0;
						setHandState(target_hand_state);
					}
				}
			}

			// Try moving
			if (moveTicksRemaining > 0)
			{
				// If falling then process falling
				if (falling)
				{
					// Falling consumes remaining move ticks
					auto fallTicksRemaining = moveTicksRemaining / (agent->modified_stats.getActualSpeedValue() * 2);
					moveTicksRemaining = 0;

					// Process falling
					auto newPosition = position;
					while (fallTicksRemaining-- > 0)
					{
						fallingSpeed += FALLING_ACCELERATION_UNIT;
						newPosition -= Vec3<float>{0.0f, 0.0f, (fallingSpeed / TICK_SCALE)} / VELOCITY_SCALE_BATTLE;
					}
					// Fell into a unit
					if (isConscious() && map.getTile(newPosition)->getUnitIfPresent(true, true, false, tileObject))
					{	
						// FIXME: Proper stun damage (ensure it is!)
						stunDamageInTicks = 0;
						dealStunDamage(agent->current_stats.health * 3 / 2);
						fallUnconscious(state);
					}
					setPosition(newPosition);

					// Falling units can always turn
					goalPosition = position;
					atGoal = true;

					// Check if reached ground
					auto restingPosition =
					    tileObject->getOwningTile()->getRestingPosition(isLarge());
					if (position.z < restingPosition.z)
					{
						// Stopped falling
						falling = false;
						fallingQueued = false;
						if (!isConscious())
						{
							// Bodies drop to the exact spot they fell upon
							setPosition({position.x, position.y, restingPosition.z});
						}
						else
						{
							setPosition(restingPosition);
						}
						resetGoal();
						// FIXME: Deal fall damage before nullifying this
						// FIXME: Play falling sound
						fallingSpeed = 0;
					}
				}

				// Not falling and moving
				else if (current_movement_state != AgentType::MovementState::None)
				{
					int speedModifier = 100;
					if (current_body_state == AgentType::BodyState::Flying)
					{
						speedModifier = std::max(1, flyingSpeedModifier);
					}

					Vec3<float> vectorToGoal = goalPosition - getPosition();
					int distanceToGoal = (int)ceilf(glm::length(vectorToGoal * VELOCITY_SCALE_BATTLE *
					                                       (float)TICKS_PER_UNIT_TRAVELLED));
					int moveTicksConsumeRate =
					    current_movement_state == AgentType::MovementState::Running ? 1 : 2;

					if (distanceToGoal > 0 && current_body_state != AgentType::BodyState::Flying &&
					    vectorToGoal.x == 0 && vectorToGoal.y == 0)
					{
						// FIXME: Actually read set option
						bool USER_OPTION_GRAVLIFT_SOUNDS = true;
						if (!wasUsingLift)
						{
							fw().soundBackend->playSample(b->common_sample_list->gravlift,
							                              getPosition(), 0.25f);
						}
						usingLift = true;
						movement_ticks_passed = 0;
					}
					int movementTicksAccumulated = 0;
					if (distanceToGoal * moveTicksConsumeRate * 100 / speedModifier >
					    moveTicksRemaining)
					{
						if (flyingSpeedModifier != 100)
						{
							flyingSpeedModifier =
								std::min(100, flyingSpeedModifier +
									moveTicksRemaining / moveTicksConsumeRate /
									FLYING_ACCELERATION_DIVISOR);
						}
						movementTicksAccumulated = moveTicksRemaining / moveTicksConsumeRate;
						auto dir = glm::normalize(vectorToGoal);
						Vec3<float> newPosition =
							(float)(moveTicksRemaining / moveTicksConsumeRate) *
							(float)(speedModifier / 100) * dir;
						newPosition /= VELOCITY_SCALE_BATTLE;
						newPosition /= (float)TICKS_PER_UNIT_TRAVELLED;
						newPosition += getPosition();
						setPosition(newPosition);
						moveTicksRemaining = moveTicksRemaining % moveTicksConsumeRate;
						atGoal = false;
					} 
					else
					{
						if (distanceToGoal > 0)
						{
							movementTicksAccumulated = distanceToGoal;
							if (flyingSpeedModifier != 100)
							{
								flyingSpeedModifier =
								    std::min(100, flyingSpeedModifier +
								                      distanceToGoal / FLYING_ACCELERATION_DIVISOR);
							}
							moveTicksRemaining -= distanceToGoal * moveTicksConsumeRate;
							setPosition(goalPosition);
							goalPosition = getPosition();
						}
						atGoal = true;
						// Pop finished missions if present
						if (popFinishedMissions(state))
						{
							return;
						}
						// Try to get new destination
						Vec3<float> nextGoal;
						if (getNextDestination(state, nextGoal))
						{
							goalPosition = nextGoal;
							atGoal = false;
						}
					}
					
					// Scale ticks so that animations look proper on isometric sceen
					// facing down or up on screen
					if (facing.x == facing.y)
					{
						movement_ticks_passed += movementTicksAccumulated * 2 / 3;
					}
					// facing left or right on screen
					else if (facing.x == -facing.y)
					{
						movement_ticks_passed += movementTicksAccumulated * 141 / 150;
					}
					else
					{
						movement_ticks_passed += movementTicksAccumulated;
					}
					// Footsteps sound
					if (shouldPlaySoundNow() && current_body_state != AgentType::BodyState::Flying)
					{
						if (agent->type->walkSfx.size() > 0)
						{
							fw().soundBackend->playSample(
							    agent->type
							        ->walkSfx[getWalkSoundIndex() % agent->type->walkSfx.size()],
							    getPosition(), 0.25f);
						}
						else
						{
							auto t = tileObject->getOwningTile();
							if (t->walkSfx && t->walkSfx->size() > 0)
							{
								fw().soundBackend->playSample(
								    t->walkSfx->at(getWalkSoundIndex() % t->walkSfx->size()),
								    getPosition(), 0.25f);
							}
						}
					}
				}
				// Not falling and not moving
				else
				{
					// Check if we should adjust our current position
					if (goalPosition == getPosition())
					{
						goalPosition = tileObject->getOwningTile()->getRestingPosition(isLarge());
					}
					atGoal = goalPosition == getPosition();
					// If not at goal - go to goal
					if (!atGoal)
					{
						addMission(state, BattleUnitMission::MissionType::ReachGoal);
					}
					// If at goal - try to request new destination
					else
					{
						// Pop finished missions if present
						if (popFinishedMissions(state))
						{
							return;
						}
						// Try to get new destination
						Vec3<float> nextGoal;
						if (getNextDestination(state, nextGoal))
						{
							goalPosition = nextGoal;
							atGoal = false;
						}
					}
				}
			}
		
			// Try turning
			if (turnTicksRemaining > 0)
			{
				if (turning_animation_ticks_remaining > turnTicksRemaining)
				{
					turning_animation_ticks_remaining -= turnTicksRemaining;
					turnTicksRemaining = 0;
				}
				else
				{
					if (turning_animation_ticks_remaining > 0)
					{
						turnTicksRemaining -= turning_animation_ticks_remaining;
						turning_animation_ticks_remaining = 0;
						facing = goalFacing;
					}
					// Pop finished missions if present
					if (popFinishedMissions(state))
					{
						return;
					}
					// Try to get new facing change
					Vec2<int> nextFacing;
					int nextTicks = 0;
					if (getNextFacing(state, nextFacing, nextTicks))
					{
						goalFacing = nextFacing;
						turning_animation_ticks_remaining = nextTicks;
					}
				}
			}
		}

	} // End of Movement and Body Animation

	// Firing

	{
		// FIXME: TODO

		// TBD later
	}

	// FIXME: Soldier "thinking" (auto-attacking, auto-turning)
}

void BattleUnit::destroy(GameState &)
{
	auto this_shared = shared_from_this();
	auto b = battle.lock();
	if (b)
		b->units.remove(this_shared);
	this->tileObject->removeFromMap();
	this->shadowObject->removeFromMap();
	this->tileObject.reset();
	this->shadowObject.reset();
}

void BattleUnit::tryToRiseUp(GameState &state)
{
	// Do not rise up if unit is standing on us
	if (tileObject->getOwningTile()->getUnitIfPresent(true, true, false, tileObject))
		return;

	// Find state we can rise into (with animation)
	auto targetState = AgentType::BodyState::Standing;
	while (targetState != AgentType::BodyState::Downed
		&& agent->getAnimationPack()->getFrameCountBody(getDisplayedItem(), current_body_state,
	                                                    targetState, current_hand_state,
	                                                    current_movement_state, facing) == 0)
	{
		switch (targetState)
		{
			case AgentType::BodyState::Standing:
				if (agent->isBodyStateAllowed(AgentType::BodyState::Flying))
				{
					targetState = AgentType::BodyState::Flying;
					continue;
				}
			// Intentional fall-through
			case AgentType::BodyState::Flying:
				if (agent->isBodyStateAllowed(AgentType::BodyState::Kneeling))
				{
					targetState = AgentType::BodyState::Kneeling;
					continue;
				}
			// Intentional fall-through
			case AgentType::BodyState::Kneeling:
				if (canProne(position, facing))
				{
					targetState = AgentType::BodyState::Prone;
					continue;
				}
			// Intentional fall-through
			case AgentType::BodyState::Prone:
				// If we arrived here then we have no animation for standing up
				targetState = AgentType::BodyState::Downed;
				continue;
		}
	}
	// Find state we can rise into (with no animation)
	if (targetState == AgentType::BodyState::Downed)
	{
		if (agent->isBodyStateAllowed(AgentType::BodyState::Standing))
		{
			targetState = AgentType::BodyState::Standing;
		}
		else if (agent->isBodyStateAllowed(AgentType::BodyState::Flying))
		{
			targetState = AgentType::BodyState::Flying;
		}
		else if (agent->isBodyStateAllowed(AgentType::BodyState::Kneeling))
		{
			targetState = AgentType::BodyState::Kneeling;
		}
		else if (canProne(position, facing))
		{
			targetState = AgentType::BodyState::Prone;
		}
		else
		{
			LogError("Unit cannot stand up???");
		}
	}

	missions.clear();
	addMission(state, BattleUnitMission::changeStance(*this, targetState));
	// Unit will automatically move to goal after rising due to logic in update()
}

void BattleUnit::dropDown(GameState &state)
{
	resetGoal();
	setMovementState(AgentType::MovementState::None);
	setHandState(AgentType::HandState::AtEase);
	setBodyState(target_body_state);
	// Check if we can drop from current state
	while (agent->getAnimationPack()->getFrameCountBody(
	           getDisplayedItem(), current_body_state, AgentType::BodyState::Downed,
	           current_hand_state, current_movement_state, facing) == 0)
	{
		switch (current_body_state)
		{
			case AgentType::BodyState::Jumping:
			case AgentType::BodyState::Throwing:
			case AgentType::BodyState::Flying:
				if (agent->isBodyStateAllowed(AgentType::BodyState::Standing))
				{
					setBodyState(AgentType::BodyState::Standing);
					continue;
				}
			// Intentional fall-through
			case AgentType::BodyState::Standing:
				if (agent->isBodyStateAllowed(AgentType::BodyState::Kneeling))
				{
					setBodyState(AgentType::BodyState::Kneeling);
					continue;
				}
			// Intentional fall-through
			case AgentType::BodyState::Kneeling:
				setBodyState(AgentType::BodyState::Prone);
				continue;
			case AgentType::BodyState::Downed:
				break;
		}
		break;
	}
	missions.clear();
	addMission(state, BattleUnitMission::changeStance(*this, AgentType::BodyState::Downed));
}

void BattleUnit::retreat(GameState &state)
{
	tileObject->removeFromMap();
	retreated = true;
	removeFromSquad();
	// FIXME: Trigger retreated event
}

void BattleUnit::die(GameState &state, bool violently)
{
	if (violently)
	{
		// FIXME: Explode if nessecary, or spawn shit
	}
	// FIXME: do what has to be done when unit dies
	// Drop equipment, make notification,...
	dropDown(state);
}

void BattleUnit::fallUnconscious(GameState &state)
{
	// FIXME: do what has to be done when unit goes unconscious
	dropDown(state);
}

void BattleUnit::beginBodyStateChange(AgentType::BodyState state, int ticks)
{
	if (ticks > 0 && current_body_state != state)
	{
		target_body_state = state;
		body_animation_ticks_remaining = ticks;
		// Updates bounds etc.
		if (tileObject)
		{
			setPosition(position);
		}
	}
	else
	{
		setBodyState(state);
	}
}

void BattleUnit::setBodyState(AgentType::BodyState state)
{
	current_body_state = state;
	target_body_state = state;
	body_animation_ticks_remaining = 0;
	// Updates bounds etc.
	if (tileObject)
	{
		setPosition(position);
	}
}

void BattleUnit::setHandState(AgentType::HandState state)
{
	current_hand_state = state;
	target_hand_state = state;
	hand_animation_ticks_remaining = 0;
}

void BattleUnit::setMovementState(AgentType::MovementState state)
{
	current_movement_state = state;
	if (state == AgentType::MovementState::None)
	{
		movement_ticks_passed = 0;
		movement_sounds_played = 0;
	}
}

int BattleUnit::getWalkSoundIndex()
{
	if (current_movement_state == AgentType::MovementState::Running)
	{
		return ((movement_sounds_played + UNITS_TRAVELLED_PER_SOUND_RUNNING_DIVISOR - 1) /
		        UNITS_TRAVELLED_PER_SOUND_RUNNING_DIVISOR) %
		       2;
	}
	else
	{
		return movement_sounds_played % 2;
	}
}

// Alexey Andronov: Istrebitel
// Made up values calculated by trying several throws in game
// This formula closely resembles results I've gotten
// But it may be completely wrong
float BattleUnit::getMaxThrowDistance(int weight, int heightDifference)
{
	float max = 30.0f;
	if (weight <= 2)
	{
		return max;
	}
	int mod = heightDifference > 0 ? heightDifference : heightDifference * 2;
	return std::max(0.0f, std::min(max, (float)agent->modified_stats.strength / ((float)weight - 1) - 2 + mod));
}

bool BattleUnit::shouldPlaySoundNow()
{
	bool play = false;
	int sounds_to_play = getDistanceTravelled() / UNITS_TRAVELLED_PER_SOUND;
	if (sounds_to_play != movement_sounds_played)
	{
		int divisor = (current_movement_state == AgentType::MovementState::Running)
		                  ? UNITS_TRAVELLED_PER_SOUND_RUNNING_DIVISOR
		                  : 1;
		play = ((sounds_to_play + divisor - 1) % divisor) == 0;
		movement_sounds_played = sounds_to_play;
	}
	return play;
}

bool BattleUnit::popFinishedMissions(GameState &state)
{
	while (missions.size() > 0 && missions.front()->isFinished(state, *this))
	{
		LogWarning("Unit mission \"%s\" finished", missions.front()->getName().cStr());
		missions.pop_front();

		// We may have retreated as a result of finished mission
		if (retreated)
			return true;

		if (!missions.empty())
		{
			missions.front()->start(state, *this);
			continue;
		}
		else
		{
			LogWarning("No next unit mission, going idle");
			break;
		}
	}
	return false;
}

bool BattleUnit::getNextDestination(GameState &state, Vec3<float> &dest)
{
	if (missions.empty())
	{
		return false;
	}
	return missions.front()->getNextDestination(state, *this, dest);
}

bool BattleUnit::getNextFacing(GameState &state, Vec2<int> &dest, int &ticks)
{
	if (missions.empty())
	{
		return false;
	}
	return missions.front()->getNextFacing(state, *this, dest, ticks);
}

bool BattleUnit::getNextBodyState(GameState &state, AgentType::BodyState &dest, int &ticks)
{
	if (missions.empty())
	{
		return false;
	}
	return missions.front()->getNextBodyState(state, *this, dest, ticks);
}

bool BattleUnit::addMission(GameState &state, BattleUnitMission::MissionType type)
{
	switch (type)
	{
		case BattleUnitMission::MissionType::Fall:
			return addMission(state, BattleUnitMission::fall(*this));
		case BattleUnitMission::MissionType::RestartNextMission:
			return addMission(state, BattleUnitMission::restartNextMission(*this));
		case BattleUnitMission::MissionType::ReachGoal:
			if (!addMission(state, BattleUnitMission::reachGoal(*this), false))
			{
				return false;
			}
			if (!addMission(state, BattleUnitMission::turn(*this, goalPosition)))
			{
				LogError("Could not add turn in front of reachGoal? WTF?");
				return false;
			}
			return true;
		case BattleUnitMission::MissionType::ThrowItem:
		case BattleUnitMission::MissionType::Snooze:
		case BattleUnitMission::MissionType::ChangeBodyState:
		case BattleUnitMission::MissionType::Turn:
		case BattleUnitMission::MissionType::AcquireTU:
		case BattleUnitMission::MissionType::GotoLocation:
		case BattleUnitMission::MissionType::Teleport:
			LogError("Cannot add mission by type if it requires parameters");
			break;
		default:
			LogError("Unimplemented");
			break;
	}
	return false;
}

bool BattleUnit::addMission(GameState &state, BattleUnitMission *mission, bool start)
{
	switch (mission->type)
	{
		// Falling:
		// - If waiting for TUs, cancel everything and fall
		// - If throwing, append after throw
		// - Otherwise, append in front
		case BattleUnitMission::MissionType::Fall:
			if (fallingQueued)
			{
				LogError("Queueing falling when already falling? WTF?");
				return false;
			}
			fallingQueued = true;
			if (missions.empty())
			{
				missions.emplace_front(mission);
				if (start) { missions.front()->start(state, *this); }
			}
			else if (missions.front()->type == BattleUnitMission::MissionType::AcquireTU)
			{
				missions.clear();
				missions.emplace_front(mission);
				if (start) { missions.front()->start(state, *this); }
			}
			else
			{
				auto it = missions.begin();
				while (it != missions.end() && (*it)->type != BattleUnitMission::MissionType::ThrowItem && ++it != missions.end()) {}
				if (it == missions.end())
				{
					missions.emplace_front(mission);
					if (start) { missions.front()->start(state, *this); }
				}
				else
				{
					missions.emplace(++it, mission);
				}
			}
			break;
		// Reach goal can only be added if it can overwrite the mission
		case BattleUnitMission::MissionType::ReachGoal:
		{
			// If current mission does not prevent moving - then move to goal
			bool shouldMoveToGoal = true;
			if (missions.size() > 0)
			{
				switch (missions.front()->type)
				{
					// Missions that prevent going to goal
				case BattleUnitMission::MissionType::Fall:
				case BattleUnitMission::MissionType::Snooze:
				case BattleUnitMission::MissionType::ThrowItem:
				case BattleUnitMission::MissionType::ChangeBodyState:
				case BattleUnitMission::MissionType::Turn:
				case BattleUnitMission::MissionType::ReachGoal:
					shouldMoveToGoal = false;
					break;
					// Missions that can be overwritten
				case BattleUnitMission::MissionType::AcquireTU:
				case BattleUnitMission::MissionType::GotoLocation:
				case BattleUnitMission::MissionType::RestartNextMission:
					shouldMoveToGoal = true;
					break;
				}
			}
			if (!shouldMoveToGoal)
			{
				return false;
			}
			missions.emplace_front(mission);
			if (start) { missions.front()->start(state, *this); }
			break;
		}
		// Turn that requires goal can only be added 
		// if it's not added in front of "reachgoal" mission
		case BattleUnitMission::MissionType::Turn:
		{
			if (mission->requireGoal)
			{
				auto it = missions.begin();
				while (it != missions.end() && (*it)->type != BattleUnitMission::MissionType::ReachGoal && ++it != missions.end()) {}
				if (it != missions.end())
				{
					return false;
				}
			}
			missions.emplace_front(mission);
			if (start) { missions.front()->start(state, *this); }
			break;
		}
		// Snooze, change body state, acquire TU, restart and teleport can be added at any time
		case BattleUnitMission::MissionType::Snooze:
		case BattleUnitMission::MissionType::ChangeBodyState:
		case BattleUnitMission::MissionType::AcquireTU:
		case BattleUnitMission::MissionType::RestartNextMission:
		case BattleUnitMission::MissionType::Teleport:
			missions.emplace_front(mission);
			if (start) { missions.front()->start(state, *this); }
			break;
			// FIXME: Implement this
		case BattleUnitMission::MissionType::ThrowItem:
		case BattleUnitMission::MissionType::GotoLocation:
			LogWarning("Adding Throw/GoTo : Ensure implemented correctly!");
			missions.emplace_front(mission);
			if (start) { missions.front()->start(state, *this); }
			break;
		default:
			LogError("Unimplemented");
			break;
	}
	return true;
}

// FIXME: When unit dies, gets destroyed, retreats or changes ownership, remove it from squad
}
