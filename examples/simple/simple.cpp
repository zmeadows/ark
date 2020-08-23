#include "ark/ark.hpp"

using namespace ark;

struct Velocity {
    float x = 0;
    float y = 0;
    using Storage = BucketArrayStorage<Velocity, 250>;
};

struct Position {
    float x = 0;
    float y = 0;
    using Storage = BucketArrayStorage<Position, 250>;

    inline void advance(float dt, const Velocity& v)
    {
        x += dt * v.x;
        y += dt * v.y;
    }
};

struct TestSystem {
    using Subscriptions = TypeList<Position, Velocity>;

    using SystemData = std::tuple<WriteComponent<Position>, ReadComponent<Velocity> >;

    static void run(FollowedEntities followed, SystemData data)
    {
        auto [position, velocity] = data;

        followed.for_each([&](const EntityID id) -> void {
            // we now use EntityId as an index into component storage
            position[id].advance(0.016, velocity[id]);
        });
    }
};

using GameComponents = TypeList<Position, Velocity>;
using GameSystems = TypeList<TestSystem>;
using GameWorld = World<GameComponents, GameSystems>;
using EntityCreator = EntityBuilder<GameComponents>;

int main()
{
    GameWorld world;

    world.build_entities([&](EntityCreator creator) {
        for (size_t i = 0; i < 500; i++) {
            creator.new_entity()
                .attach<Position>(Position{0.f, 0.f})
                .attach<Velocity>(Velocity{1.f, 1.f});
        }
    });

    for (size_t frame = 0; frame < 60; frame++) {
        world.run_systems_sequential<TestSystem>();
    }

    return 0;
}
