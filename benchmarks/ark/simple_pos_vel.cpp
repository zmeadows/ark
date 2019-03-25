#include "ark/ark.hpp"
#include "types.hpp"
#include "benchmark.hpp"

#include <iostream>
#include <cstdlib>

using namespace ark;
using ark::bench::Position;
using ark::bench::Velocity;

struct TestSystem {
    using Subscriptions = TypeList<Position, Velocity>;

    using SystemData = std::tuple< WriteComponent<Position>
                                 , ReadComponent<Velocity>
                                 >;

    static void run(FollowedEntities followed, SystemData data) {
	   auto [ position, velocity ] = data;

       followed.for_each([&] (const EntityID id) -> void {
           Position& pos = position[id];
           const Velocity& vel = velocity[id];
       });
    }
};

using GameComponents = TypeList<Position, Velocity>;
using GameSystems    = TypeList<TestSystem>;
using GameWorld      = World<GameComponents, GameSystems>;

int main() {
    auto build_world = [] (size_t num_entities) {
        GameWorld* world = GameWorld::init([](auto&){});
        world->build_entities([&](EntityBuilder<GameComponents> builder) {
            for (size_t i = 0; i < num_entities; i++) {
                builder.new_entity()
                    .attach<Position>(Position{0.f, 0.f})
                    .attach<Velocity>(Velocity{1.f, 1.f});
            }
        });
        return world;
    };

    auto iterate_world = [] (GameWorld* world) {
        world->run_all_systems_sequential();
    };

    for (size_t num_entities : {1000000}) {
        ecs_bench("one system + two components + empty update", "ark",
                  num_entities, build_world, iterate_world);
    }

    return 0;
}
