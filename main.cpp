#include "ark/ark.hpp"
#include "ark/storage/bucket_array.hpp"
#include "ark/third_party/ThreadPool.hpp"

#include <iostream>
#include <cstdlib>
#include <chrono>

using namespace ark;

struct Position {
    float x,y;
    using Storage = BucketArrayStorage<Position, 65000>;
};

struct Velocity {
    float x,y;
    using Storage = BucketArrayStorage<Velocity, 65000>;
};

struct DeltaTime {
    float value;
};

struct TestSystem {
    using Subscriptions = TypeList<Position, Velocity>;

    using SystemData = std::tuple< FollowedEntities
                                 , WriteComponent<Position>
                                 , ReadComponent<Velocity>
                                 , ReadResource<DeltaTime>
                                 >;

    static void run(SystemData data) {
	   auto [ followed, position, velocity, delta_t ] = data;

       const float dt = delta_t->value;

       followed.for_each_par([&] (const EntityID id) -> void {
           Position& pos = position[id];
           const Velocity& vel = velocity[id];
           pos.x += dt * vel.x;
           pos.y += dt * vel.y;
       });
    }
};

using GameComponents = TypeList<Position, Velocity>;
using GameSystems    = TypeList<TestSystem>;
using GameResources  = TypeList<DeltaTime>;
using GameWorld      = World<GameComponents, GameSystems, GameResources>;

int main() {

    GameWorld* world = GameWorld::init([](ResourceStash<GameResources>& stash) {
        stash.construct<DeltaTime>(DeltaTime{0.016});
    });

    if (!world) {
        std::cerr << "failed to generate world!" << std::endl;
        return 1;
    }

    const size_t num_entities = 100000;
    world->build_entities([](EntityBuilder<GameComponents> builder) {
        for (size_t i = 0; i < num_entities; i++) {
            builder.new_entity()
                   .attach<Position>()
                   .attach<Velocity>();
        }
    });

    std::cout << "finished creating " << num_entities << " entities." << std::endl;

    world->tick();
    auto start = std::chrono::high_resolution_clock::now();
    const size_t iters = 1e4;
    for (size_t i = 0; i < iters; i++) {
        if (i % (iters/10) == 0) {
            std::cout << "iteration: " << i << std::endl;
        }
        world->tick();
    }
    auto elapsed = std::chrono::high_resolution_clock::now() - start;
    long long mu_sec = std::chrono::duration_cast<std::chrono::microseconds>(elapsed).count();
    std::cout << "time per iteration: " << (1.0 / (double) iters) * ((double) mu_sec / 1e6) << std::endl;

    return 0;
}
