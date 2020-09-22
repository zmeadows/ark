#include "ark/storage/bucket_array.hpp"
#include "ark/storage/robin_hood.hpp"

#include <array>
#include <random>

namespace ark::bench {

struct Velocity {
    float x = 0;
    float y = 0;
    using Storage = RobinHoodStorage<Velocity>;
};

struct Position {
    float x = 0;
    float y = 0;
    using Storage = RobinHoodStorage<Position>;

    inline void advance(float dt, const Velocity& v)
    {
        x += dt * v.x;
        y += dt * v.y;
    }
};

struct RotationalVelocity {
    float dtheta = 0;
    using Storage = RobinHoodStorage<RotationalVelocity>;
};

struct Angle {
    float theta = 0;
    using Storage = RobinHoodStorage<Angle>;

    inline void advance(float dt, const RotationalVelocity& v) { theta += dt * v.dtheta; }
};

template <size_t N>
std::array<Velocity, N> build_random_velocities(void)
{
    std::array<Velocity, N> result;
    std::default_random_engine e;
    std::uniform_real_distribution<float> dis(-1.f, 1.f);

    for (size_t i = 0; i < N; i++) {
        Velocity v;
        v.x = dis(e);
        v.y = dis(e);
        result[i] = v;
    }

    return result;
}

} // namespace ark::bench
