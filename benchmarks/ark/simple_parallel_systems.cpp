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

struct R {
    float x;
    using Storage = BucketArrayStorage<R, 15000>;
};

struct W1 {
    float x;
    using Storage = BucketArrayStorage<W1, 15000>;
};

struct W2 {
    float x;
    using Storage = BucketArrayStorage<W2, 15000>;
};

struct W1System {
    using Subscriptions = TypeList<R, W1>;

    using SystemData = std::tuple< ReadComponent<R>
                                 , WriteComponent<W1>
                                 >;

    static void run(FollowedEntities followed, SystemData data) {
	   auto [ r, w1 ] = data;

       followed.for_each([&] (const EntityID id) -> void {
           w1[id].x += r[id].x;
       });
    }
};

struct W2System {
    using Subscriptions = TypeList<R, W2>;

    using SystemData = std::tuple< ReadComponent<R>
                                 , WriteComponent<W2>
                                 >;

    static void run(FollowedEntities followed, SystemData data) {
	   auto [ r, w2 ] = data;
       followed.for_each([&] (const EntityID id) -> void {
           w2[id].x += r[id].x;
       });
    }
};

using GameComponents = TypeList<R, W1, W2>;
using GameSystems    = TypeList<W1System, W2System>;
using GameWorld      = World<GameComponents, GameSystems>;

int main() {
    auto build_world = [] (size_t num_entities) {
        GameWorld* world = GameWorld::init([](auto&) {});

        world->build_entities([&](EntityBuilder<GameComponents> builder) {
            for (size_t i = 0; i < num_entities; i++) {
                builder.new_entity()
                    .attach<R>()
                    .attach<W1>()
                    .attach<W2>();
            }
        });

        return world;
    };

    auto iterate_world = [] (GameWorld* world) {
        world->run_systems_parallel<W1System, W2System>();
    };

    for (size_t num_entities : {1e6}) {
        ecs_bench("two parallel systems, three components", "ark",
                  num_entities, build_world, iterate_world, .5);
    }

    return 0;
}
