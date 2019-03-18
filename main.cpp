#include "ark/ark.hpp"
#include "ark/storage/bucket_array.hpp"

using namespace ark;

struct Position {
    float x,y;
    using Storage = BucketArrayStorage<Position, 250>;
};

struct Velocity {
    float x,y;
    using Storage = BucketArrayStorage<Velocity, 250>;
};

class DeltaTime {
    const float value;
public:
    float unwrap() const { return value; }
    DeltaTime(float dt) : value(dt) {}
};

using GameComponents = TypeList<Position, Velocity>;

struct TestSystem {
    // using Subscriptions = TypeList<Position, Velocity>;

    using SystemData = std::tuple< FollowedEntities
                                 , WriteComponent<Position>
                                 , ReadComponent<Velocity>
                                 , ReadResource<DeltaTime>
                                 , EntityBuilder<GameComponents>
                                 >;

    static void run(SystemData& data) {
	   auto [ entities, position, velocity, dt, builder ] = data;

       builder.new_entity()
                .attach<Position>()
                .attach<Velocity>();

       for (const EntityID id : entities) {
           position[id].x += dt->unwrap() * velocity[id].x;
           position[id].y += dt->unwrap() * velocity[id].y;
       }
    }
};


using GameSystems    = TypeList<TestSystem>;
using GameResources  = TypeList<DeltaTime>;
using GameWorld      = World<GameComponents, GameSystems, GameResources>;

int main() {
    GameWorld* world = GameWorld::init([](auto* stash) {
        stash->template construct<DeltaTime>(0.016);
    });

    if (!world) { return 1; }

    for (auto i = 0; i < 1000; i++) {
        world->tick();
    }

    return 0;
}
