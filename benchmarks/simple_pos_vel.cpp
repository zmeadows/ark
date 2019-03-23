#include "ark/ark.hpp"
#include "ark/storage/bucket_array.hpp"
#include "benchmark.hpp"

#include <iostream>
#include <cstdlib>

#include <chrono>
using namespace std::chrono;

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

       followed.for_each_par([&] (const EntityID id) -> void {
           Position& pos = position[id];
           const Velocity& vel = velocity[id];
           // pos.x += dt * vel.x;
           // pos.y += dt * vel.y;
       });
    }
};

using GameComponents = TypeList<Position, Velocity>;
using GameSystems    = TypeList<TestSystem>;
using GameWorld      = World<GameComponents, GameSystems>;

int main() {
    const size_t num_entities = 100000;
    bench::start("simple one-system (position + velocity)", num_entities);

    GameWorld* world = GameWorld::init([](auto&) {});

    world->build_entities([](EntityBuilder<GameComponents> builder) {
        for (size_t i = 0; i < num_entities; i++) {
            builder.new_entity()
                .attach<Position>(Position{0.f, 0.f})
                .attach<Velocity>(Velocity{0.1f, 0.1f});
        }
    });


    auto bench_result = benchmark([world]() {
        world->run_all_systems_sequential();
    }, 5e3);

    bench_result.print("time per system iteration");
    bench::end();

    return 0;
}
