#include "ark/ark.hpp"
#include "benchmark.hpp"
#include "types.hpp"

#include <cstdlib>
#include <iostream>

using namespace ark;
using ark::bench::Position;
using ark::bench::Velocity;

struct TestSystem {
    using Subscriptions = TypeList<Position, Velocity>;

    static void run(FollowedEntities followed, WriteComponent<Position> position,
                    ReadComponent<Velocity> velocity)
    {
        followed.for_each([&](const EntityID id) -> void {
            (void)position[id];
            (void)velocity[id];
        });
    }
};

using GameComponents = TypeList<Position, Velocity>;
using GameSystems = TypeList<TestSystem>;
using GameWorld = World<GameComponents, GameSystems>;
using Builder = EntityBuilder<GameComponents>;

int main()
{
    auto build_world = [](size_t num_entities) {
        GameWorld* world = GameWorld::init([](auto&) {});

        world->build_entities([&](Builder builder) {
            for (size_t i = 0; i < num_entities; i++) {
                builder.new_entity()
                    .attach<Position>(Position{0.f, 0.f})
                    .attach<Velocity>(Velocity{1.f, 1.f});
            }
        });

        return world;
    };

    auto iterate_world = [](GameWorld* world) { world->run_systems_sequential<TestSystem>(); };

    for (double num_entities : {1e3, 1e4, 1e5, 1e6}) {
        ecs_bench("one system + two components + empty update + single threaded", "ark",
                  (size_t)num_entities, build_world, iterate_world);
    }

    return 0;
}
