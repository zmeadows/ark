ark is an entity component system focused on performance through data-oriented design, compile-time metaprogramming, and effortless parallelization.
Currently written in C++20 and requires g++ for foncepts with flag -fconcepts.

Some notable features include:

* Customizable component storage, similar to specs: https://slide-rs.github.io/specs/
  * unique self-defragmenting 'bucket array' component storage provided by default
* Easy parallelization via behind-the-scenes thread pool 
  * Entity level: ```for_each``` --> ```for_each_par``` 
  * System level: ```run_systems_sequential``` --> ```run_systems_parallel``` 
* Concrete notion of a 'System' as a struct with specific typedefs and a 'run' method
* Loose coupling.
  * Systems don't need to know about the entire 'World'.
  * System/World communication handled by a small number of 'handle' types in ark/system.hpp
* No inheritance, no exceptions, no RTTI


## Example
```C++
struct Velocity {
    float x = 0;
    float y = 0;
    using Storage = BucketArrayStorage<Velocity, 2000>;
};

struct Position {
    float x = 0;
    float y = 0;
    using Storage = BucketArrayStorage<Position, 2000>;

    inline void advance(float dt, const Velocity& v) {
        x += dt * v.x;
        y += dt * v.y;
    }
};

struct TestSystem {
    using Subscriptions = TypeList<Position, Velocity>;

    using SystemData = std::tuple< WriteComponent<Position>
                                 , ReadComponent<Velocity>
                                 >;

    static void run(FollowedEntities followed, SystemData data) {
	   auto [ position, velocity ] = data;

       followed.for_each([&] (const EntityID id) -> void {
           pos[id].update(0.016, vel[id]);
       });
    }
};

using GameComponents = TypeList<Position, Velocity>;
using GameSystems    = TypeList<TestSystem>;
using GameWorld      = World<GameComponents, GameSystems>;
using EntityCreator  = EntityBuilder<GameComponents>;

int main() {
    GameWorld* world = GameWorld::init();

    world->build_entities([&](EntityCreator creator) {
        for (size_t i = 0; i < num_entities; i++) {
            creator.new_entity()
            .attach<Position>(Position{0.f, 0.f})
            .attach<Velocity>(Velocity{1.f, 1.f});
        }
    });

    for (size_t frame = 0; frame < 60; frame++) {
        world->run_all_systems_sequential();
    }

    return 0;
}
```

### Plans for the Future
* Move from C++20 to C++17 so people can actually use this without g++.
* Check system dependencies at compile time to alert user of potential race conditions when using run_systems_parallel

### Acknowledgements/Inspiration:

Jonathan Blow for his informative streams and the bucket array idea: https://www.youtube.com/watch?v=COQKyOCAxOQ

Casey Muratori for the Handmade Hero streams and inspiration to think more deeply about these problems: https://handmadehero.org/

Mike Acton for the famous talk you've probably already heard. If not: https://www.youtube.com/watch?v=rX0ItVEVjHc

Malte Skarupke for his great work on optimizing radix sort and open addressing hash tables. Check out his blog: https://probablydance.com/

Other ECS implementations:
* specs (Rust): https://slide-rs.github.io/specs/
* entt (C++17): https://github.com/skypjack/entt
