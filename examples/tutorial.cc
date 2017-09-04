#include "sc2api/sc2_api.h"

#include <iostream>

using namespace sc2;

class Bot : public Agent {

Point2D marine_staging_location;

public:
    virtual void OnGameStart() final {
        std::cout << "Hello, World!" << std::endl;
        Unit cc = Observation()->GetUnits(Unit::Alliance::Self, IsUnit(UNIT_TYPEID::TERRAN_COMMANDCENTER)).front();
        if (cc.pos.x == 29.5) {
            marine_staging_location = Point2D(67.003418f, 127.894043f);
        } else {
            marine_staging_location = Point2D(77.574707f, 30.615479f);
        }
    }

    virtual void OnStep() {
        TryBuildSupplyDepot();
        TryBuildBarracks();
        TryBuildMarines();
        TryAttackWithMarines();
    }

    virtual void OnUnitIdle(const Unit& unit) final {
        switch (unit.unit_type.ToType()) {
        case UNIT_TYPEID::TERRAN_COMMANDCENTER: {
            if (unit.assigned_harvesters == 0 || unit.assigned_harvesters < unit.ideal_harvesters) {
                Actions()->UnitCommand(unit, ABILITY_ID::TRAIN_SCV);
            }
            break;
        }
        case UNIT_TYPEID::TERRAN_SCV: {
            uint64_t mineral_target;
            if (!FindNearestMineralPatch(unit.pos, mineral_target)) {
                break;
            }
            Actions()->UnitCommand(unit, ABILITY_ID::SMART, mineral_target);
            break;
        }
        default: {
            break;
        }
        }
    }

    virtual void OnUnitCreated(const Unit& unit) final {
        switch(unit.unit_type.ToType()) {
        case UNIT_TYPEID::TERRAN_BARRACKS: {
            Actions()->UnitCommand(unit, ABILITY_ID::RALLY_UNITS, marine_staging_location);
        }
        default: {
            break;
        }
        }
    }
private:

    bool TryBuildMarines(){
        int marine_limit = 100;
        Units marines = Observation()->GetUnits(Unit::Alliance::Self, IsUnit(UNIT_TYPEID::TERRAN_MARINE));
        Units barracks = Observation()->GetUnits(Unit::Alliance::Self, IsUnit(UNIT_TYPEID::TERRAN_BARRACKS));
        int marines_in_prod = 0;

        std::vector<Tag> idle_barracks;
        for (const auto &barrack : barracks) {
            for (const auto &order : barrack.orders) {
                if (order.ability_id == ABILITY_ID::TRAIN_MARINE) {
                    marines_in_prod++;
                }
            }
            if (barrack.orders.size() == 0) {
                idle_barracks.push_back(barrack.tag);
            }
        }

        if (idle_barracks.size() > 0 && marines.size() + marines_in_prod < 100) {
           Actions()->UnitCommand(idle_barracks, ABILITY_ID::TRAIN_MARINE);
        }
        return true;
    }

    bool TryBuildStructure(ABILITY_ID ability_type_for_structure, 
                           int concurrent_number = 1, 
                           UNIT_TYPEID unit_type = UNIT_TYPEID::TERRAN_SCV) {
        const ObservationInterface *observation = Observation();

        Units all_scvs = observation->GetUnits(Unit::Alliance::Self, IsUnit(unit_type));
        std::vector<Unit> builders;
        int already_building = 0;
        for (const auto& scv : all_scvs) {
            if (already_building >= concurrent_number) {
                return false;
            }
            for(const auto& order : scv.orders) {
                if (order.ability_id == ability_type_for_structure) {
                    already_building++;
                    break;
                } else {
                    builders.push_back(scv);
                }
            }
        }

        builders.resize(concurrent_number - already_building);

        for (const auto& scv : builders) {
            float rx = GetRandomScalar();
            float ry = GetRandomScalar();

            Actions()->UnitCommand(scv,
                                   ability_type_for_structure,
                                   Point2D(scv.pos.x + rx * 15.0f, scv.pos.y + ry * 15.0f));
        }
        return true;
    }

    bool TryBuildSupplyDepot() {
        const ObservationInterface* observation = Observation();

        // If we are not supply capped, don't build a supply depot.
        if (observation->GetFoodUsed() <= observation->GetFoodCap() - 2)
            return false;

        // Try and build a depot. Find a random SCV and give it the order.
        return TryBuildStructure(ABILITY_ID::BUILD_SUPPLYDEPOT);
    }

    bool TryBuildBarracks() {
        Units depots = Observation()->GetUnits(Unit::Alliance::Self, IsUnit(UNIT_TYPEID::TERRAN_SUPPLYDEPOT));
        Units barracks = Observation()->GetUnits(Unit::Alliance::Self, IsUnit(UNIT_TYPEID::TERRAN_BARRACKS));
        if (barracks.size() < 3 && depots.size() >= 1) {
            return TryBuildStructure(ABILITY_ID::BUILD_BARRACKS, 3);
        }
        return false;
    }

    bool FindNearestMineralPatch(const Point2D& start, uint64_t& target) {
        Units units = Observation()->GetUnits(Unit::Alliance::Neutral);
        float distance = std::numeric_limits<float>::max();
        for (const auto& u : units) {
            if (u.unit_type == UNIT_TYPEID::NEUTRAL_MINERALFIELD) {
                float d = DistanceSquared2D(u.pos, start);
                if (d < distance) {
                    distance = d;
                    target = u.tag;
                }
            }
        }

        if (distance == std::numeric_limits<float>::max()) {
            return false;
        }

        return true;
    }

    bool TryAttackWithMarines() {
        Units marines = Observation()->GetUnits(Unit::Alliance::Self, IsUnit(UNIT_TYPEID::TERRAN_MARINE));
        if (marines.size() >= 8 ) {
            GameInfo game_info = Observation()->GetGameInfo();
            Point2D enemy_start_location;
            enemy_start_location = game_info.enemy_start_locations.front();

            std::vector<Tag> marine_tags;
            for (const auto &marine : marines) {
                marine_tags.push_back(marine.tag);
            }
            Actions()->UnitCommand(marine_tags, ABILITY_ID::ATTACK, enemy_start_location);
            return true;
        }
        return false;
    }
};

int main(int argc, char* argv[]) {
    Coordinator coordinator;
    coordinator.LoadSettings(argc, argv);

    Bot bot;
    coordinator.SetParticipants({
        CreateParticipant(Race::Terran, &bot),
        CreateComputer(Race::Zerg)
    });

    coordinator.LaunchStarcraft();
    coordinator.StartGame(sc2::kMapBelShirVestigeLE);

    while (coordinator.Update()) {
    }

    return 0;
}