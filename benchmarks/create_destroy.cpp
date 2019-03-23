#include "ark/ark.hpp"
#include "ark/prelude.hpp"
#include "ark/storage/bucket_array.hpp"
#include "ark/third_party/ThreadPool.hpp"
#include "benchmark.hpp"

#include <iostream>
#include <cstdlib>

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
    builder.new_entity()
           .attach<Position>()
           .attach<Velocity>(random_velocities[rand_index])
           .attach<Angle>()
           .attach<RotationalVelocity>(RotationalVelocity{0.1});
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
        return pos.x*pos.x > 100.f || pos.y*pos.y > 100.f;
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

using GameSystems    = TypeList<TranslationSystem, RotationSystem, CreateDestroySystem>;
using GameWorld      = World<GameComponents, GameSystems>;

int main() {
    std::ios_base::sync_with_stdio(false);
    std::cin.tie(NULL);

    const size_t num_entities = 100000;
    bench::start("create/destroy, 4 components, 3 systems", num_entities);

    GameWorld* world = GameWorld::init([](auto&) {});

    world->build_entities([](EntityBuilder<GameComponents> builder) {
        for (size_t i = 0; i < num_entities; i++) {
            make_new_entity(builder);
        }
    });

    auto bench_result1 = benchmark([world]() {
        world->run_systems_sequential<TranslationSystem,RotationSystem>();
        //world->run_systems_sequential<CreateDestroySystem>();
    }, 1e2);

    bench_result1.print("time per system iteration: no create/destroy");

    auto bench_result2 = benchmark([world]() {
        world->run_systems_parallel<TranslationSystem,RotationSystem>();
        world->run_systems_sequential<CreateDestroySystem>();
    }, 1e2);

    bench_result2.print("time per system iteration: w/ create/destroy");

    auto bench_result3 = benchmark([world]() {
        world->run_systems_parallel<TranslationSystem,RotationSystem>();
        //world->run_systems_sequential<CreateDestroySystem>();
    }, 1e2);

    bench_result3.print("time per system iteration: no bucket array maintenance");

            const auto start_time = high_resolution_clock::now();
    world->run_storage_maintenance<Position>();
    world->run_storage_maintenance<Velocity>();
    world->run_storage_maintenance<Angle>();
    world->run_storage_maintenance<RotationalVelocity>();
            const auto elapsed = high_resolution_clock::now() - start_time;
            const double dur = duration_cast<duration<double>>(elapsed).count();
            std::cout << "defrag time: " << dur << std::endl;

    auto bench_result4 = benchmark([world]() {
        world->run_systems_parallel<TranslationSystem,RotationSystem>();
        //world->run_systems_sequential<CreateDestroySystem>();
    }, 1e2);

    bench_result4.print("time per system iteration: after bucket array maintanence");

    //std::cout << "total entities destroyed/re-created: "
    //          << CreateDestroySystem::entities_destroyed
    //          << " (" << CreateDestroySystem::entities_destroyed / (double) bench_result.count
    //          << " / iteration)"
    //          << std::endl;

    bench::end();

    return 0;
}
