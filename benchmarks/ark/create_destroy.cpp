#include "ark/ark.hpp"
#include "ark/prelude.hpp"
#include "ark/storage/bucket_array.hpp"
#include "ark/third_party/ThreadPool.hpp"
#include "benchmark.hpp"
#include "types.hpp"

#include <iostream>
#include <cstdlib>
#include <memory>

#include <chrono>
using namespace std::chrono;

using namespace ark;

using ark::bench::Position;
using ark::bench::Velocity;
using ark::bench::Angle;
using ark::bench::RotationalVelocity;

using GameComponents = TypeList<Position, Velocity, Angle, RotationalVelocity>;

void make_new_entity(EntityBuilder<GameComponents>& builder) {
    static std::array<Velocity,10000> random_velocities = bench::build_random_velocities<10000>();
    static size_t rand_index = 0;
    rand_index = (rand_index + 1) % 10000;
    auto e = builder.new_entity();
    e.attach<Position>();
    e.attach<Velocity>(random_velocities[rand_index]);
    e.attach<Angle>();
    e.attach<RotationalVelocity>(RotationalVelocity{0.1});
}

struct TranslationSystem {
    using Subscriptions = TypeList<Position, Velocity>;

    using SystemData = std::tuple< WriteComponent<Position>
                                 , ReadComponent<Velocity>
                                 >;

    static void run(FollowedEntities followed, SystemData data) {
	   auto [ position, velocity ] = data;

       followed.for_each_par([&] (const EntityID id) -> void {
           position[id].advance(0.016, velocity[id]);
       });
    }
};

struct RotationSystem {
    using Subscriptions = TypeList<Angle, RotationalVelocity>;

    using SystemData = std::tuple< WriteComponent<Angle>
                                 , ReadComponent<RotationalVelocity>
                                 >;

    static void run(FollowedEntities followed, SystemData data) {
	   auto [ angle, rot_vel ] = data;

       followed.for_each_par([&] (const EntityID id) -> void {
           angle[id].advance(0.016, rot_vel[id]);
       });
    }
};

struct CreateDestroySystem {
    using Subscriptions = TypeList<Position>;

    using SystemData = std::tuple< ReadComponent<Position>
                                 , EntityBuilder<GameComponents>
                                 , EntityDestroyer
                                 >;

    static size_t entities_destroyed;

    static bool is_offscreen(const Position& pos) {
        return pos.x*pos.x > 500000.f || pos.y*pos.y > 500000.f;
    }

    static void run(FollowedEntities followed, SystemData data) {
	   auto [ position, builder, destroy ] = data;

       followed.for_each([&] (const EntityID id) -> void {
           if (is_offscreen(position[id])) {
               entities_destroyed++;
               destroy(id);
               make_new_entity(builder);
           }
       });
    }
};

size_t CreateDestroySystem::entities_destroyed = 0;

using GameSystems = TypeList<TranslationSystem, RotationSystem, CreateDestroySystem>;
using GameWorld   = World<GameComponents, GameSystems>;

int main() {

    auto build_world = [] (size_t num_entities) -> GameWorld* {
        GameWorld* world = GameWorld::init([](auto&){});
        world->build_entities([&](EntityBuilder<GameComponents> builder) {
            for (size_t i = 0; i < num_entities; i++) {
                make_new_entity(builder);
            }
        });
        return world;
    };

    auto bench1 = [](GameWorld* world) {
        world->run_systems_sequential<TranslationSystem,RotationSystem>();
    };

    auto bench2 = [](GameWorld* world) {
        const auto start = high_resolution_clock::now();
        world->run_systems_sequential<TranslationSystem,RotationSystem>();
        world->run_systems_sequential<CreateDestroySystem>();
        const auto end = high_resolution_clock::now();
        const double dur = duration_cast<duration<double>>(end - start).count();
    };

    for (size_t num_entities : { 1000, 10000, 50000, 100000}) {
        ecs_bench("two systems + four components + simple updates", "ark",
                  num_entities, build_world, bench1);
        ecs_bench("three systems + four components + simple updates + create/destroy", "ark",
                  num_entities, build_world, bench2);

        std::cout << "total entities destroyed/re-created: "
                  << CreateDestroySystem::entities_destroyed
                  << std::endl;
        CreateDestroySystem::entities_destroyed = 0;
    }

    return 0;
}
