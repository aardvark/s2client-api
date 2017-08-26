#include "sc2api/sc2_api.h"

#include <iostream>

using namespace sc2;

class Bot : public Agent {
public:
    virtual void OnGameStart() final {
        std::cout << "Hello, World!" << std::endl;
    }

    virtual void OnStep() {
        TryBuildSupplyDepot();
        TryBuildBarracks();
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
        case UNIT_TYPEID::TERRAN_BARRACKS: {
            Units marines = Observation()->GetUnits(Unit::Alliance::Self, IsUnit(UNIT_TYPEID::TERRAN_MARINE));
            Units barracks = Observation()->GetUnits(Unit::Alliance::Self, IsUnit(UNIT_TYPEID::TERRAN_BARRACKS));
            int marines_in_prod = 0;

            for (const auto& barrack : barracks) {
                for (const auto& order : barrack.orders) {
                    if (order.ability_id == ABILITY_ID::TRAIN_MARINE) {
                        marines_in_prod++;
                    }
                }
            };

            if (marines.size() + marines_in_prod < 8) {
                Actions()->UnitCommand(unit, ABILITY_ID::TRAIN_MARINE);
            }
        }
        default: {
            break;
        }
        }
    }
private:
    bool TryBuildStructure(ABILITY_ID ability_type_for_structure, UNIT_TYPEID unit_type = UNIT_TYPEID::TERRAN_SCV) {
        const ObservationInterface* observation = Observation();

        // If a unit already is building a supply structure of this type, do nothing.
        // Also get an scv to build the structure.
        Unit unit_to_build;
        Units units = observation->GetUnits(Unit::Alliance::Self);
        for (const auto& unit : units) {
            for (const auto& order : unit.orders) {
                if (order.ability_id == ability_type_for_structure) {
                    return false;
                }
            }

            if (unit.unit_type == unit_type) {
                unit_to_build = unit;
            }
        }

        float rx = GetRandomScalar();
        float ry = GetRandomScalar();

        Actions()->UnitCommand(unit_to_build,
            ability_type_for_structure,
            Point2D(unit_to_build.pos.x + rx * 15.0f, unit_to_build.pos.y + ry * 15.0f));

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
            return TryBuildStructure(ABILITY_ID::BUILD_BARRACKS);
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