/*
 * This file is part of the OregonCore Project. See AUTHORS file for Copyright information
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef OREGON_GRIDNOTIFIERS_H
#define OREGON_GRIDNOTIFIERS_H

#include "ObjectGridLoader.h"
#include "ByteBuffer.h"
#include "UpdateData.h"
#include <iostream>

#include "Corpse.h"
#include "Object.h"
#include "DynamicObject.h"
#include "GameObject.h"
#include "Player.h"
#include "Unit.h"
#include "CreatureAI.h"

class Player;
//class Map;

namespace Oregon
{
struct VisibleNotifier
{
    Player& i_player;
    UpdateData i_data;
    std::set<Unit*> i_visibleNow;
    Player::ClientGUIDs vis_guids;

    VisibleNotifier(Player& player) : i_player(player), vis_guids(player.m_clientGUIDs) {}
    template<class T> void Visit(GridRefManager<T>& m);
    void SendToSelf(void);
};

struct VisibleChangesNotifier
{
    WorldObject& i_object;

    explicit VisibleChangesNotifier(WorldObject& object) : i_object(object) {}
    template<class T> void Visit(GridRefManager<T>&) {}
    void Visit(PlayerMapType&);
    void Visit(CreatureMapType&);
    void Visit(DynamicObjectMapType&);
};

struct PlayerRelocationNotifier : public VisibleNotifier
{
    PlayerRelocationNotifier(Player& player) : VisibleNotifier(player) { }

    template<class T> void Visit(GridRefManager<T> &m) { VisibleNotifier::Visit(m); }
    void Visit(CreatureMapType&);
    void Visit(PlayerMapType&);
};

struct CreatureRelocationNotifier
{
    Creature& i_creature;
    const float i_radius;
    CreatureRelocationNotifier(Creature& c, float radius) :
        i_creature(c), i_radius(radius) {}
    template<class T> void Visit(GridRefManager<T>&) {}
    void Visit(CreatureMapType&);
    void Visit(PlayerMapType&);
};

struct DelayedUnitRelocation
{
    Map& i_map;
    Cell& cell;
    CellCoord& p;
    const float i_radius;
    DelayedUnitRelocation(Cell& c, CellCoord& pair, Map& map, float radius) :
        i_map(map), cell(c), p(pair), i_radius(radius) {}
    template<class T> void Visit(GridRefManager<T>&) {}
    void Visit(CreatureMapType&);
    void Visit(PlayerMapType&);
};

struct AIRelocationNotifier
{
    Unit& i_unit;
    bool isCreature;
    explicit AIRelocationNotifier(Unit& unit) : i_unit(unit), isCreature(unit.GetTypeId() == TYPEID_UNIT)  {}
    template<class T> void Visit(GridRefManager<T>&) {}
    void Visit(CreatureMapType&);
};

struct GridUpdater
{
    GridType& i_grid;
    uint32 i_timeDiff;
    GridUpdater(GridType& grid, uint32 diff) : i_grid(grid), i_timeDiff(diff) {}

    template<class T> void updateObjects(GridRefManager<T>& m)
    {
        for (typename GridRefManager<T>::iterator iter = m.begin(); iter != m.end(); ++iter)
            iter->GetSource()->Update(i_timeDiff);
    }

    void Visit(PlayerMapType& m)
    {
        updateObjects<Player>(m);
    }
    void Visit(CreatureMapType& m)
    {
        updateObjects<Creature>(m);
    }
    void Visit(GameObjectMapType& m)
    {
        updateObjects<GameObject>(m);
    }
    void Visit(DynamicObjectMapType& m)
    {
        updateObjects<DynamicObject>(m);
    }
    void Visit(CorpseMapType& m)
    {
        updateObjects<Corpse>(m);
    }
};

struct MessageDistDeliverer
{
    WorldObject* i_source;
    WorldPacket* i_message;
    float i_distSq;
    uint32 team;
    MessageDistDeliverer(WorldObject* src, WorldPacket* msg, float dist, bool own_team_only = false)
        : i_source(src), i_message(msg), i_distSq(dist* dist)
        , team((own_team_only && src->GetTypeId() == TYPEID_PLAYER) ? ((Player*)src)->GetTeam() : 0)
    {
    }
    void Visit(PlayerMapType& m);
    void Visit(CreatureMapType& m);
    void Visit(DynamicObjectMapType& m);
    template<class SKIP> void Visit(GridRefManager<SKIP>&) {}

    void SendPacket(Player* plr)
    {
        // never send packet to self
        if (plr == i_source || (team && plr->GetTeam() != team))
            return;

        if (!plr->HaveAtClient(i_source))
            return;

        if (WorldSession* session = plr->GetSession())
            session->SendPacket(i_message);
    }
};

struct ObjectUpdater
{
    uint32 i_timeDiff;
    explicit ObjectUpdater(const uint32& diff) : i_timeDiff(diff) {}
    template<class T> void Visit(GridRefManager<T>& m);
    void Visit(PlayerMapType&) {}
    void Visit(CorpseMapType&) {}
    void Visit(CreatureMapType&);
};

struct DynamicObjectUpdater
{
    DynamicObject& i_dynobject;
    Unit* i_check;
    DynamicObjectUpdater(DynamicObject& dynobject, Unit* caster) : i_dynobject(dynobject)
    {
        i_check = caster;
        Unit* owner = i_check->GetOwner();
        if (owner)
            i_check = owner;
    }

    template<class T> void Visit(GridRefManager<T>&) {}
    void Visit(CreatureMapType&);
    void Visit(PlayerMapType&);

    void VisitHelper(Unit* target);
};

// SEARCHERS & LIST SEARCHERS & WORKERS

// WorldObject searchers & workers

template<class Check>
struct WorldObjectSearcher
{
    WorldObject*& i_object;
    Check& i_check;

    WorldObjectSearcher(WorldObject*& result, Check& check) : i_object(result), i_check(check) {}

    void Visit(GameObjectMapType& m);
    void Visit(PlayerMapType& m);
    void Visit(CreatureMapType& m);
    void Visit(CorpseMapType& m);
    void Visit(DynamicObjectMapType& m);

    template<class NOT_INTERESTED> void Visit(GridRefManager<NOT_INTERESTED>&) {}
};

template<class Check>
struct WorldObjectListSearcher
{
    std::list<WorldObject*>& i_objects;
    Check& i_check;

    WorldObjectListSearcher(std::list<WorldObject*>& objects, Check& check) : i_objects(objects), i_check(check) {}

    void Visit(PlayerMapType& m);
    void Visit(CreatureMapType& m);
    void Visit(CorpseMapType& m);
    void Visit(GameObjectMapType& m);
    void Visit(DynamicObjectMapType& m);

    template<class NOT_INTERESTED> void Visit(GridRefManager<NOT_INTERESTED>&) {}
};

template<class Do>
struct WorldObjectWorker
{
    Do const& i_do;

    explicit WorldObjectWorker(Do const& _do) : i_do(_do) {}

    void Visit(GameObjectMapType& m)
    {
        for (GameObjectMapType::iterator itr = m.begin(); itr != m.end(); ++itr)
            i_do(itr->GetSource());
    }

    void Visit(PlayerMapType& m)
    {
        for (PlayerMapType::iterator itr = m.begin(); itr != m.end(); ++itr)
            i_do(itr->GetSource());
    }
    void Visit(CreatureMapType& m)
    {
        for (CreatureMapType::iterator itr = m.begin(); itr != m.end(); ++itr)
            i_do(itr->GetSource());
    }

    void Visit(CorpseMapType& m)
    {
        for (CorpseMapType::iterator itr = m.begin(); itr != m.end(); ++itr)
            i_do(itr->GetSource());
    }

    void Visit(DynamicObjectMapType& m)
    {
        for (DynamicObjectMapType::iterator itr = m.begin(); itr != m.end(); ++itr)
            i_do(itr->GetSource());
    }

    template<class NOT_INTERESTED> void Visit(GridRefManager<NOT_INTERESTED>&) {}
};

// Gameobject searchers

template<class Check>
struct GameObjectSearcher
{
    GameObject*& i_object;
    Check& i_check;

    GameObjectSearcher(GameObject*& result, Check& check) : i_object(result), i_check(check) {}

    void Visit(GameObjectMapType& m);

    template<class NOT_INTERESTED> void Visit(GridRefManager<NOT_INTERESTED>&) {}
};

// Last accepted by Check GO if any (Check can change requirements at each call)
template<class Check>
struct GameObjectLastSearcher
{
    GameObject*& i_object;
    Check& i_check;

    GameObjectLastSearcher(GameObject*& result, Check& check) : i_object(result), i_check(check) {}

    void Visit(GameObjectMapType& m);

    template<class NOT_INTERESTED> void Visit(GridRefManager<NOT_INTERESTED>&) {}
};

template<class Check>
struct GameObjectListSearcher
{
    std::list<GameObject*>& i_objects;
    Check& i_check;

    GameObjectListSearcher(std::list<GameObject*>& objects, Check& check) : i_objects(objects), i_check(check) {}

    void Visit(GameObjectMapType& m);

    template<class NOT_INTERESTED> void Visit(GridRefManager<NOT_INTERESTED>&) {}
};

// Unit searchers

// First accepted by Check Unit if any
template<class Check>
struct UnitSearcher
{
    Unit*& i_object;
    Check& i_check;

    UnitSearcher(Unit*& result, Check& check) : i_object(result), i_check(check) {}

    void Visit(CreatureMapType& m);
    void Visit(PlayerMapType& m);

    template<class NOT_INTERESTED> void Visit(GridRefManager<NOT_INTERESTED>&) {}
};

// Last accepted by Check Unit if any (Check can change requirements at each call)
template<class Check>
struct UnitLastSearcher
{
    Unit*& i_object;
    Check& i_check;

    UnitLastSearcher(Unit*& result, Check& check) : i_object(result), i_check(check) {}

    void Visit(CreatureMapType& m);
    void Visit(PlayerMapType& m);

    template<class NOT_INTERESTED> void Visit(GridRefManager<NOT_INTERESTED>&) {}
};

// All accepted by Check units if any
template<class Check>
struct UnitListSearcher
{
    std::list<Unit*>& i_objects;
    Check& i_check;

    UnitListSearcher(std::list<Unit*>& objects, Check& check) : i_objects(objects), i_check(check) {}

    void Visit(PlayerMapType& m);
    void Visit(CreatureMapType& m);

    template<class NOT_INTERESTED> void Visit(GridRefManager<NOT_INTERESTED>&) {}
};

// Creature searchers

template<class Check>
struct CreatureSearcher
{
    Creature*& i_object;
    Check& i_check;

    CreatureSearcher(Creature*& result, Check& check) : i_object(result), i_check(check) {}

    void Visit(CreatureMapType& m);

    template<class NOT_INTERESTED> void Visit(GridRefManager<NOT_INTERESTED>&) {}
};

// Last accepted by Check Creature if any (Check can change requirements at each call)
template<class Check>
struct CreatureLastSearcher
{
    Creature*& i_object;
    Check& i_check;

    CreatureLastSearcher(Creature*& result, Check& check) : i_object(result), i_check(check) {}

    void Visit(CreatureMapType& m);

    template<class NOT_INTERESTED> void Visit(GridRefManager<NOT_INTERESTED>&) {}
};

template<class Check>
struct CreatureListSearcher
{
    std::list<Creature*>& i_objects;
    Check& i_check;

    CreatureListSearcher(std::list<Creature*>& objects, Check& check) : i_objects(objects), i_check(check) {}

    void Visit(CreatureMapType& m);

    template<class NOT_INTERESTED> void Visit(GridRefManager<NOT_INTERESTED>&) {}
};

template<class Do>
struct CreatureWorker
{
    Do& i_do;

    CreatureWorker(Do& _do)
        : i_do(_do) {}

    void Visit(CreatureMapType& m)
    {
        for (CreatureMapType::iterator itr = m.begin(); itr != m.end(); ++itr)
            i_do(itr->GetSource());
    }

    template<class NOT_INTERESTED> void Visit(GridRefManager<NOT_INTERESTED>&) {}
};

// Player searchers

template<class Check>
struct PlayerSearcher
{
    Player*& i_object;
    Check& i_check;

    PlayerSearcher(Player*& result, Check& check) : i_object(result), i_check(check) {}

    void Visit(PlayerMapType& m);

    template<class NOT_INTERESTED> void Visit(GridRefManager<NOT_INTERESTED>&) {}
};

template<class Check>
struct PlayerListSearcher
{
    std::list<Player*>& i_objects;
    Check& i_check;

    PlayerListSearcher(std::list<Player*>& objects, Check& check)
        : i_objects(objects), i_check(check) {}

    void Visit(PlayerMapType& m);

    template<class NOT_INTERESTED> void Visit(GridRefManager<NOT_INTERESTED>&) {}
};

template<class Do>
struct PlayerWorker
{
    Do& i_do;

    explicit PlayerWorker(Do& _do) : i_do(_do) {}

    void Visit(PlayerMapType& m)
    {
        for (PlayerMapType::iterator itr = m.begin(); itr != m.end(); ++itr)
            i_do(itr->GetSource());
    }

    template<class NOT_INTERESTED> void Visit(GridRefManager<NOT_INTERESTED>&) {}
};

template<class Do>
struct PlayerDistWorker
{
    float i_dist;
    Do& i_do;

    PlayerDistWorker(float _dist, Do& _do)
        : i_dist(_dist), i_do(_do) {}

    void Visit(PlayerMapType& m)
    {
        for (PlayerMapType::iterator itr = m.begin(); itr != m.end(); ++itr)
            i_do(itr->GetSource());
    }

    template<class NOT_INTERESTED> void Visit(GridRefManager<NOT_INTERESTED>&) {}
};

// CHECKS && DO classes

// WorldObject check classes
class CannibalizeObjectCheck
{
    public:
        CannibalizeObjectCheck(Unit* funit, float range) : i_funit(funit), i_range(range) {}
        bool operator()(Player* u)
        {
            if (i_funit->IsFriendlyTo(u) || u->IsAlive() || u->isInFlight())
                return false;

            return i_funit->IsWithinDistInMap(u, i_range);
        }
        bool operator()(Corpse* u);
        bool operator()(Creature* u)
        {
            if (i_funit->IsFriendlyTo(u) || u->IsAlive() || u->isInFlight() ||
                (u->GetCreatureTypeMask() & CREATURE_TYPEMASK_HUMANOID_OR_UNDEAD) == 0)
                return false;

            return i_funit->IsWithinDistInMap(u, i_range);
        }
        template<class NOT_INTERESTED> bool operator()(NOT_INTERESTED*)
        {
            return false;
        }
    private:
        Unit* const i_funit;
        float i_range;
};

// WorldObject do classes

class RespawnDo
{
    public:
        RespawnDo() {}
        void operator()(Creature* u) const
        {
            u->Respawn();
        }
        void operator()(GameObject* u) const
        {
            u->Respawn();
        }
        void operator()(WorldObject*) const {}
        void operator()(Corpse*) const {}
};

// GameObject checks

class GameObjectFocusCheck
{
    public:
        GameObjectFocusCheck(Unit const* unit, uint32 focusId) : i_unit(unit), i_focusId(focusId) {}
        bool operator()(GameObject* go) const
        {
            if (go->GetGOInfo()->type != GAMEOBJECT_TYPE_SPELL_FOCUS)
                return false;

            if (go->GetGOInfo()->spellFocus.focusId != i_focusId)
                return false;

            float dist = go->GetGOInfo()->spellFocus.dist / 2.f;

            return go->IsWithinDistInMap(i_unit, dist);
        }
    private:
        Unit const* i_unit;
        uint32 i_focusId;
};

// Find the nearest Fishing hole and return true only if source object is in range of hole
class NearestGameObjectFishingHole
{
    public:
        NearestGameObjectFishingHole(WorldObject const& obj, float range) : i_obj(obj), i_range(range) {}
        bool operator()(GameObject* go)
        {
            if (go->GetGOInfo()->type == GAMEOBJECT_TYPE_FISHINGHOLE && go->isSpawned() && i_obj.IsWithinDistInMap(go, i_range) && i_obj.IsWithinDistInMap(go, go->GetGOInfo()->fishinghole.radius))
            {
                i_range = i_obj.GetDistance(go);
                return true;
            }
            return false;
        }
        float GetLastRange() const
        {
            return i_range;
        }
    private:
        WorldObject const& i_obj;
        float  i_range;

        // prevent clone
        NearestGameObjectFishingHole(NearestGameObjectFishingHole const&);
};

// Success at unit in range, range update for next check (this can be use with GameobjectLastSearcher to find nearest GO with a certain type)
class NearestGameObjectTypeInObjectRangeCheck
{
    public:
        NearestGameObjectTypeInObjectRangeCheck(WorldObject const& obj, GameobjectTypes type, float range) : i_obj(obj), i_type(type), i_range(range) {}
        bool operator()(GameObject* go)
        {
            if (go->GetGoType() == i_type && i_obj.IsWithinDistInMap(go, i_range))
            {
                i_range = i_obj.GetDistance(go);        // use found GO range as new range limit for next check
                return true;
            }
            return false;
        }

    float GetLastRange() const { return i_range; }

    private:
        WorldObject const& i_obj;
        GameobjectTypes i_type;
        float  i_range;

    // prevent clone this object
    NearestGameObjectTypeInObjectRangeCheck(NearestGameObjectTypeInObjectRangeCheck const&);
};


// Success at unit in range, range update for next check (this can be use with GameobjectLastSearcher to find nearest GO)
class NearestGameObjectEntryInObjectRangeCheck
{
    public:
        NearestGameObjectEntryInObjectRangeCheck(WorldObject const& obj, uint32 entry, float range) : i_obj(obj), i_entry(entry), i_range(range) {}
        bool operator()(GameObject* go)
        {
            if (go->GetEntry() == i_entry && i_obj.IsWithinDistInMap(go, i_range))
            {
                i_range = i_obj.GetDistance(go);        // use found GO range as new range limit for next check
                return true;
            }
            return false;
        }
        float GetLastRange() const
        {
            return i_range;
        }
    private:
        WorldObject const& i_obj;
        uint32 i_entry;
        float  i_range;

        // prevent clone this object
        NearestGameObjectEntryInObjectRangeCheck(NearestGameObjectEntryInObjectRangeCheck const&);
};

class GameObjectWithDbGUIDCheck
{
    public:
        GameObjectWithDbGUIDCheck(WorldObject const&, uint32 db_guid) : i_db_guid(db_guid) {}
        bool operator()(GameObject const* go) const
        {
            return go->GetDBTableGUIDLow() == i_db_guid;
        }
    private:
        uint32 i_db_guid;
};

// Unit checks

class MostHPMissingInRange
{
    public:
        MostHPMissingInRange(Unit const* obj, float range, uint32 hp) : i_obj(obj), i_range(range), i_hp(hp) {}
        bool operator()(Unit* u)
        {
            if (u->IsAlive() && u->IsInCombat() && !i_obj->IsHostileTo(u) && i_obj->IsWithinDistInMap(u, i_range) && u->GetMaxHealth() - u->GetHealth() > i_hp)
            {
                i_hp = u->GetMaxHealth() - u->GetHealth();
                return true;
            }
            return false;
        }
    private:
        Unit const* i_obj;
        float i_range;
        uint32 i_hp;
};

class FriendlyCCedInRange
{
    public:
        FriendlyCCedInRange(Unit const* obj, float range) : i_obj(obj), i_range(range) {}
        bool operator()(Unit* u)
        {
            if (u->IsAlive() && u->IsInCombat() && !i_obj->IsHostileTo(u) && i_obj->IsWithinDistInMap(u, i_range) &&
                (u->isFeared() || u->isCharmed() || u->isFrozen() || u->HasUnitState(UNIT_STATE_STUNNED) || u->HasUnitState(UNIT_STATE_CONFUSED)))
                return true;
            return false;
        }
    private:
        Unit const* i_obj;
        float i_range;
};

///void DoFindFriendlyNPC(std::list<Creature*>& _list, uint64 guid, float range, uint64 myguid, uint32 cooldown);
class FriendlyNPCInRange
{
public:
	FriendlyNPCInRange(Creature *obj, uint64 guid, float range, uint64 myguid, uint32 cooldown) : i_obj(obj), i_range(range), i_guid(guid), i_myguid(myguid), i_cooldown(cooldown){}
	bool operator()(Creature* u)
	{
		//if (u->IsAlive() && !u->IsInCombat() && !i_obj->IsHostileTo(u) && i_obj->IsWithinDistInMap(u, i_range) && (u->GetDBTableGUIDLow() == i_myguid) && (i_obj->GetDBTableGUIDLow() == i_guid))
		if (u->IsAlive() && !u->IsInCombat() && !i_obj->IsHostileTo(u) && i_obj->IsWithinDistInMap(u, i_range) && (i_obj->GetDBTableGUIDLow() == i_myguid) && (u->GetDBTableGUIDLow() == i_guid))
			return true;
		return false;
	}
private:
	Creature* i_obj;
	float i_range;
	uint64 i_guid;
	uint64 i_myguid;
	uint32 i_cooldown;
};

class FriendlyNPCInRangeCombat
{
public:
	FriendlyNPCInRangeCombat(Creature *obj, uint64 guid, float range, uint64 myguid, uint32 cooldown) : i_obj(obj), i_range(range), i_guid(guid), i_myguid(myguid), i_cooldown(cooldown){}
	bool operator()(Creature* u)
	{
		//if (u->IsAlive() && !u->IsInCombat() && !i_obj->IsHostileTo(u) && i_obj->IsWithinDistInMap(u, i_range) && (u->GetDBTableGUIDLow() == i_myguid) && (i_obj->GetDBTableGUIDLow() == i_guid))
		if (u->IsAlive() && i_obj->IsInCombat() && !i_obj->IsHostileTo(u) && i_obj->IsWithinDistInMap(u, i_range) && (i_obj->GetDBTableGUIDLow() == i_myguid) && (u->GetDBTableGUIDLow() == i_guid))
			return true;
		return false;
	}
private:
	Creature* i_obj;
	float i_range;
	uint64 i_guid;
	uint64 i_myguid;
	uint32 i_cooldown;
};

class FriendlyNPCInRangeDeath
{
public:
	FriendlyNPCInRangeDeath(Creature *obj, uint64 targetguid, float range, uint64 myguid) : i_obj(obj), i_targetguid(targetguid), i_range(range), i_myguid(myguid){}
	bool operator()(Creature* u)
	{
		if (u->IsAlive() && !i_obj->IsHostileTo(u) && i_obj->IsWithinDistInMap(u, i_range) && (i_obj->GetDBTableGUIDLow() == i_myguid) && (u->GetDBTableGUIDLow() == i_targetguid))
			return true;	
		return false;
	}
private:
	Creature* i_obj;
	uint64 i_targetguid;
	float i_range;
	uint64 i_myguid;
};

class FriendlyMissingBuffInRange
{
    public:
        FriendlyMissingBuffInRange(Unit const* obj, float range, uint32 spellid) : i_obj(obj), i_range(range), i_spell(spellid) {}
        bool operator()(Unit* u)
        {
            if (u->IsAlive() && u->IsInCombat() && /*!i_obj->IsHostileTo(u)*/ i_obj->IsFriendlyTo(u) && i_obj->IsWithinDistInMap(u, i_range) &&
                !(u->HasAura(i_spell, 0) || u->HasAura(i_spell, 1) || u->HasAura(i_spell, 2)))
                return true;
            return false;
        }
    private:
        Unit const* i_obj;
        float i_range;
        uint32 i_spell;
};

class AnyUnfriendlyUnitInObjectRangeCheck
{
    public:
        AnyUnfriendlyUnitInObjectRangeCheck(WorldObject const* obj, Unit const* funit, float range) : i_obj(obj), i_funit(funit), i_range(range) {}
        bool operator()(Unit* u)
        {
            if (u->IsAlive() && i_obj->IsWithinDistInMap(u, i_range) && !i_funit->IsFriendlyTo(u))
                return true;
            else
                return false;
        }
    private:
        WorldObject const* i_obj;
        Unit const* i_funit;
        float i_range;
};

class AnyUnfriendlyNoTotemUnitInObjectRangeCheck
{
    public:
        AnyUnfriendlyNoTotemUnitInObjectRangeCheck(WorldObject const* obj, Unit const* funit, float range) : i_obj(obj), i_funit(funit), i_range(range) {}
        bool operator()(Unit* u)
        {
            if (!u->IsAlive())
                return false;

            if (u->GetCreatureType() == CREATURE_TYPE_NON_COMBAT_PET)
                return false;

            if (u->GetTypeId() == TYPEID_UNIT && u->IsTotem())
                return false;

            if (!u->isTargetableForAttack(false))
                return false;

            return i_obj->IsWithinDistInMap(u, i_range) && !i_funit->IsFriendlyTo(u);
        }
    private:
        WorldObject const* i_obj;
        Unit const* i_funit;
        float i_range;
};

class CreatureWithDbGUIDCheck
{
    public:
        CreatureWithDbGUIDCheck(WorldObject const*, uint32 lowguid) : i_lowguid(lowguid) {}
        bool operator()(Creature* u)
        {
            return u->GetDBTableGUIDLow() == i_lowguid;
        }
    private:
        uint32 i_lowguid;
};

class AnyFriendlyUnitInObjectRangeCheck
{
    public:
        AnyFriendlyUnitInObjectRangeCheck(WorldObject const* obj, Unit const* funit, float range, bool playerOnly = false) : i_obj(obj), i_funit(funit), i_range(range), i_playerOnly(playerOnly) {}
        bool operator()(Unit* u)
        {
            if (u->IsAlive() && i_obj->IsWithinDistInMap(u, i_range) && i_funit->IsFriendlyTo(u) && (!i_playerOnly || u->GetTypeId() == TYPEID_PLAYER))
                return true;
            else
                return false;
        }
    private:
        WorldObject const* i_obj;
        Unit const* i_funit;
        float i_range;
        bool i_playerOnly;
};

class AnyUnitInObjectRangeCheck
{
    public:
        AnyUnitInObjectRangeCheck(WorldObject const* obj, float range) : i_obj(obj), i_range(range) {}
        bool operator()(Unit* u)
        {
            if (u->IsAlive() && i_obj->IsWithinDistInMap(u, i_range))
                return true;

            return false;
        }
    private:
        WorldObject const* i_obj;
        float i_range;
};

// Success at unit in range, range update for next check (this can be use with UnitLastSearcher to find nearest unit)
class NearestAttackableUnitInObjectRangeCheck
{
    public:
        NearestAttackableUnitInObjectRangeCheck(WorldObject const* obj, Unit const* funit, float range) : i_obj(obj), i_funit(funit), i_range(range) {}
        bool operator()(Unit* u)
        {
            if (u->isTargetableForAttack() && i_obj->IsWithinDistInMap(u, i_range) &&
                !i_funit->IsFriendlyTo(u) && u->CanSeeOrDetect(i_funit))
            {
                i_range = i_obj->GetDistance(u);        // use found unit range as new range limit for next check
                return true;
            }

            return false;
        }
    private:
        WorldObject const* i_obj;
        Unit const* i_funit;
        float i_range;

        // prevent clone this object
        NearestAttackableUnitInObjectRangeCheck(NearestAttackableUnitInObjectRangeCheck const&);
};

class AnyAoETargetUnitInObjectRangeCheck
{
    public:
        AnyAoETargetUnitInObjectRangeCheck(WorldObject const* obj, Unit const* funit, float range)
            : i_obj(obj), i_funit(funit), i_range(range)
        {
            Unit const* check = i_funit;
            Unit const* owner = i_funit->GetOwner();
            if (owner)
                check = owner;
            i_targetForPlayer = (check->GetTypeId() == TYPEID_PLAYER);
        }
        bool operator()(Unit* u)
        {
            // Check contains checks for: live, non-selectable, non-attackable flags, flight check and GM check, ignore totems
            if (!u->isTargetableForAttack())
                return false;
            if (u->GetTypeId() == TYPEID_UNIT && u->IsTotem())
                return false;

            if ((i_targetForPlayer ? !i_funit->IsFriendlyTo(u) : i_funit->IsHostileTo(u)) && i_obj->IsWithinDistInMap(u, i_range))
                return true;

            return false;
        }
    private:
        bool i_targetForPlayer;
        WorldObject const* i_obj;
        Unit const* i_funit;
        float i_range;
};

// do attack at call of help to friendly crearture
class CallOfHelpCreatureInRangeDo
{
    public:
        CallOfHelpCreatureInRangeDo(Unit* funit, Unit* enemy, float range)
            : i_funit(funit), i_enemy(enemy), i_range(range)
        {}
        void operator()(Creature* u)
        {
            if (u == i_funit)
                return;

            if (!u->CanAssistTo(i_funit, i_enemy, false))
                return;

            // too far
            if (!u->IsWithinDistInMap(i_funit, i_range))
                return;

            // only if see assisted creature
            if (!u->IsWithinLOSInMap(i_enemy))
                return;

            if (u->AI())
                u->AI()->AttackStart(i_enemy);
        }
    private:
        Unit* const i_funit;
        Unit* const i_enemy;
        float i_range;
};

struct AnyDeadUnitCheck
{
    bool operator()(Unit* u)
    {
        return !u->IsAlive();
    }
};

// Creature checks

    class NearestHostileUnitCheck
    {
        public:
            explicit NearestHostileUnitCheck(Creature const* creature, float dist = 0, bool playerOnly = false) : me(creature), i_playerOnly(playerOnly)
            {
                m_range = (dist == 0 ? 9999 : dist);
            }
            bool operator()(Unit* u)
            {
                if (!me->IsWithinDistInMap(u, m_range))
                    return false;

                if (!me->canAttack(u))
                    return false;

                if (i_playerOnly && u->GetTypeId() != TYPEID_PLAYER)
                    return false;

                m_range = me->GetDistance(u);   // use found unit range as new range limit for next check
                return true;
            }

    private:
            Creature const *me;
            float m_range;
            bool i_playerOnly;
            NearestHostileUnitCheck(NearestHostileUnitCheck const&);
    };

class NearestHostileUnitInAttackDistanceCheck
{
    public:
        explicit NearestHostileUnitInAttackDistanceCheck(Creature const* creature, float dist = 0) : me(creature)
        {
            m_range = (dist == 0 ? 9999 : dist);
            m_force = (dist == 0 ? false : true);
        }
        bool operator()(Unit* u)
        {
            if (!me->IsWithinDistInMap(u, m_range))
                return false;

            if (!me->CanSeeOrDetect(u))
                return false;

            if (m_force)
            {
                if (!me->canAttack(u))
                    return false;
            }
            else
            {
                if (!me->canStartAttack(u))
                    return false;
            }

            m_range = me->GetDistance(u);   // use found unit range as new range limit for next check
            return true;
        }
        float GetLastRange() const
        {
            return m_range;
        }
    private:
        Creature const* me;
        float m_range;
        bool m_force;
        NearestHostileUnitInAttackDistanceCheck(NearestHostileUnitInAttackDistanceCheck const&);
};

class AnyAssistCreatureInRangeCheck
{
    public:
        AnyAssistCreatureInRangeCheck(Unit* funit, Unit* enemy, float range)
            : i_funit(funit), i_enemy(enemy), i_range(range)
        {
        }
        bool operator()(Creature* u)
        {
            if (u == i_funit)
                return false;

            if (!u->CanAssistTo(i_funit, i_enemy))
                return false;

            // too far
            if (!i_funit->IsWithinDistInMap(u, i_range))
                return false;

            // only if see assisted creature
            if (!i_funit->IsWithinLOSInMap(u))
                return false;

            return true;
        }
    private:
        Unit* const i_funit;
        Unit* const i_enemy;
        float i_range;
};

class NearestAssistCreatureInCreatureRangeCheck
{
    public:
        NearestAssistCreatureInCreatureRangeCheck(Creature* obj, Unit* enemy, float range)
            : i_obj(obj), i_enemy(enemy), i_range(range) {}

        bool operator()(Creature* u)
        {
            if (u->getFaction() == i_obj->getFaction() && !u->IsInCombat() && !u->GetCharmerOrOwnerGUID() && u->IsHostileTo(i_enemy) && u->IsAlive() && i_obj->IsWithinDistInMap(u, i_range) && i_obj->IsWithinLOSInMap(u))
            {
                i_range = i_obj->GetDistance(u);         // use found unit range as new range limit for next check
                return true;
            }
            return false;
        }
        float GetLastRange() const
        {
            return i_range;
        }
    private:
        Creature* const i_obj;
        Unit* const i_enemy;
        float  i_range;

        // prevent clone this object
        NearestAssistCreatureInCreatureRangeCheck(NearestAssistCreatureInCreatureRangeCheck const&);
};

// Success at unit in range, range update for next check (this can be use with CreatureLastSearcher to find nearest creature)
class NearestCreatureEntryWithLiveStateInObjectRangeCheck
{
    public:
        NearestCreatureEntryWithLiveStateInObjectRangeCheck(WorldObject const& obj, uint32 entry, bool alive, float range)
            : i_obj(obj), i_entry(entry), i_alive(alive), i_range(range) {}

        bool operator()(Creature* u)
        {
            if (u->GetEntry() == i_entry && u->IsAlive() == i_alive && i_obj.IsWithinDistInMap(u, i_range))
            {
                i_range = i_obj.GetDistance(u);         // use found unit range as new range limit for next check
                return true;
            }
            return false;
        }
        float GetLastRange() const
        {
            return i_range;
        }
    private:
        WorldObject const& i_obj;
        uint32 i_entry;
        bool   i_alive;
        float  i_range;

        // prevent clone this object
        NearestCreatureEntryWithLiveStateInObjectRangeCheck(NearestCreatureEntryWithLiveStateInObjectRangeCheck const&);
};

class AnyPlayerInObjectRangeCheck
{
    public:
        AnyPlayerInObjectRangeCheck(WorldObject const* obj, float range, bool reqAlive = true) : i_obj(obj), i_range(range), _reqAlive(reqAlive) {}
        bool operator()(Player* u)
        {
            if (_reqAlive && !u->IsAlive())
                return false;

            if (!i_obj->IsWithinDistInMap(u, i_range))
                return false;

            return true;
        }
    private:
        WorldObject const* i_obj;
        float i_range;
        bool _reqAlive;
};

class NearestPlayerInObjectRangeCheck
{
public:
    NearestPlayerInObjectRangeCheck(WorldObject const& obj, float range)
        : i_obj(obj), i_range(range) {}

    bool operator()(Player* u)
    {
        if (i_obj.IsWithinDistInMap(u, i_range))
        {
            i_range = i_obj.GetDistance(u);         // use found unit range as new range limit for next check
            return true;
        }
        return false;
    }
    float GetLastRange() const { return i_range; }
private:
    WorldObject const& i_obj;
    float  i_range;

    // prevent clone this object
    NearestPlayerInObjectRangeCheck(NearestPlayerInObjectRangeCheck const&);
};

class AllWorldObjectsInRange
{
public:
    AllWorldObjectsInRange(const WorldObject* pObject, float fMaxRange) : m_pObject(pObject), m_fRange(fMaxRange) {}
    bool operator() (WorldObject* pGo)
    {
        return m_pObject->IsWithinDistInMap(pGo, m_fRange, false);
    }
private:
    const WorldObject* m_pObject;
    float m_fRange;
};

class AllFriendlyCreaturesInGrid
{
    public:
        AllFriendlyCreaturesInGrid(Unit const* obj) : pUnit(obj) {}
        bool operator() (Unit* u)
        {
            if (u->IsAlive() && u->IsVisible() && u->IsFriendlyTo(pUnit))
                return true;

            return false;
        }
    private:
        Unit const* pUnit;
};

class AllGameObjectsWithEntryInRange
{
    public:
        AllGameObjectsWithEntryInRange(const WorldObject* pObject, uint32 uiEntry, float fMaxRange) : m_pObject(pObject), m_uiEntry(uiEntry), m_fRange(fMaxRange) {}
        bool operator() (GameObject* pGo)
        {
            if ((!m_uiEntry || pGo->GetEntry() == m_uiEntry) && m_pObject->IsWithinDist(pGo, m_fRange, false))
                return true;

            return false;
        }
    private:
        const WorldObject* m_pObject;
        uint32 m_uiEntry;
        float m_fRange;
};

class AllCreaturesOfEntryInRange
{
    public:
        AllCreaturesOfEntryInRange(const WorldObject* pObject, uint32 uiEntry, float fMaxRange) : m_pObject(pObject), m_uiEntry(uiEntry), m_fRange(fMaxRange) {}
        bool operator() (Unit* pUnit)
        {
            if ((!m_uiEntry || pUnit->GetEntry() == m_uiEntry) && m_pObject->IsWithinDist(pUnit, m_fRange, false))
                return true;

            return false;
        }

    private:
        const WorldObject* m_pObject;
        uint32 m_uiEntry;
        float m_fRange;
};

class PlayerAtMinimumRangeAway
{
    public:
        PlayerAtMinimumRangeAway(Unit const* unit, float fMinRange) : pUnit(unit), fRange(fMinRange) {}
        bool operator() (Player* pPlayer)
        {
            //No threat list check, must be done explicit if expected to be in combat with creature
            if (!pPlayer->IsGameMaster() && pPlayer->IsAlive() && !pUnit->IsWithinDist(pPlayer, fRange, false))
                return true;

            return false;
        }

    private:
        Unit const* pUnit;
        float fRange;
};

class ObjectTypeIdCheck
{
    public:
        ObjectTypeIdCheck(TypeID typeId, bool equals) : _typeId(typeId), _equals(equals) {}
        bool operator()(WorldObject* object)
        {
            return (object->GetTypeId() == _typeId) == _equals;
        }

    private:
        TypeID _typeId;
        bool _equals;
};

// Player checks and do

// Prepare using Builder localized packets with caching and send to player
template<class Builder>
class LocalizedPacketDo
{
    public:
        explicit LocalizedPacketDo(Builder& builder) : i_builder(builder) {}

        ~LocalizedPacketDo()
        {
            for (uint32 i = 0; i < i_data_cache.size(); ++i)
                delete i_data_cache[i];
        }
        void operator()(Player* p);

    private:
        Builder& i_builder;
        std::vector<WorldPacket*> i_data_cache;         // 0 = default, i => i-1 locale index
};

// Prepare using Builder localized packets with caching and send to player
template<class Builder>
class LocalizedPacketListDo
{
    public:
        typedef std::vector<WorldPacket*> WorldPacketList;
        explicit LocalizedPacketListDo(Builder& builder) : i_builder(builder) {}

        ~LocalizedPacketListDo()
        {
            for (size_t i = 0; i < i_data_cache.size(); ++i)
                for (int j = 0; j < i_data_cache[i].size(); ++j)
                    delete i_data_cache[i][j];
        }
        void operator()(Player* p);

    private:
        Builder& i_builder;
        std::vector<WorldPacketList> i_data_cache;
        // 0 = default, i => i-1 locale index
};
}
#endif

