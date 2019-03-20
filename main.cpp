#include "ark/ark.hpp"
#include "ark/storage/bucket_array.hpp"

#include <iostream>
#include <cstdlib>
#include <chrono>

using namespace ark;

struct Position {
    float x,y;
    using Storage = BucketArrayStorage<Position, 1000>;
};

struct Velocity {
    float x,y;
    using Storage = BucketArrayStorage<Velocity, 1000>;
};

class DeltaTime {
    const float value;
public:
    float unwrap() const { return value; }
    DeltaTime(float dt) : value(dt) {}
};

using GameComponents = TypeList<Position, Velocity>;

struct TestSystem {
    using Subscriptions = TypeList<Position, Velocity>;

    using SystemData = std::tuple< FollowedEntities
                                 , WriteComponent<Position>
                                 , ReadComponent<Velocity>
                                 , ReadResource<DeltaTime>
                                 >;

    static void run(SystemData data) {
	   auto [ entities, position, velocity, delta_t ] = data;

       const float dt = delta_t->unwrap();

       for (const EntityID id : entities) {
           Position& pos = position[id];
           const Velocity& vel = velocity[id];
           pos.x += dt * vel.x;
           pos.y += dt * vel.y;
       }
    }
};

using GameSystems    = TypeList<TestSystem>;
using GameResources  = TypeList<DeltaTime>;
using GameWorld      = World<GameComponents, GameSystems, GameResources>;

int main() {

    GameWorld* world = GameWorld::init([](ResourceStash<GameResources>& stash) {
        stash.construct<DeltaTime>(0.016);
    });

    if (!world) { return 1; }

    world->build_entities([](EntityBuilder<GameComponents> builder) {
        for (auto i = 0; i < 1000; i++) {
            builder.new_entity()
                   .attach<Position>()
                   .attach<Velocity>();
        }
    });


    for (auto i = 0; i < 1e6; i++) {
        world->tick();
    }
    // auto start = std::chrono::high_resolution_clock::now();
    // auto elapsed = std::chrono::high_resolution_clock::now() - start;
    // long long mu_sec = std::chrono::duration_cast<std::chrono::microseconds>(elapsed).count();
    // std::cout << 1.0 / 100.0 * (double) mu_sec / 1e6 << std::endl;

    return 0;
}
