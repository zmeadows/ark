#include "ark/ark.hpp"
#include "ark/storage/robin_hood.hpp"
using namespace ark;

#include <array>
#include <chrono>
#include <random>
#include <vector>
using namespace std::chrono;

#include <GL/glut.h>

namespace boids {

// --------------------------------------------------------------------------------

using real_t = float;

static constexpr size_t NUM_BOIDS = 10000;
static constexpr real_t SCREEN_WIDTH_PIXELS = 1200.;
static constexpr int FINE_GRAIN_CELL_LIMIT = 2;
static constexpr int COARSE_GRAIN_CELL_LIMIT = 3;

// --------------------------------------------------------------------------------

struct V2 {
    real_t x = 0.;
    real_t y = 0.;

    inline V2& operator+=(const V2& rhs)
    {
        x += rhs.x;
        y += rhs.y;
        return *this;
    }

    inline V2& operator-=(const V2& rhs)
    {
        x -= rhs.x;
        y -= rhs.y;
        return *this;
    }

    inline V2& operator/=(real_t sf)
    {
        x /= sf;
        y /= sf;
        return *this;
    }

    inline real_t magnitude(void) const { return std::sqrt(x * x + y * y); }
};

inline V2 operator*(const V2& v, real_t sf) { return {v.x * sf, v.y * sf}; }

inline V2 operator*(real_t sf, const V2& v) { return {v.x * sf, v.y * sf}; }

inline V2 operator-(const V2& lhs, const V2& rhs) { return {lhs.x - rhs.x, lhs.y - rhs.y}; }

inline V2 operator/(const V2& v, real_t sf) { return {v.x / sf, v.y / sf}; }

inline bool operator==(const V2& lhs, const V2& rhs) { return lhs.x == rhs.x && lhs.x == lhs.y; }

inline bool operator!=(const V2& lhs, const V2& rhs) { return !(lhs == rhs); }

inline real_t distance_sq(const V2& a, const V2& b)
{
    const real_t dx = a.x - b.x;
    const real_t dy = a.y - b.y;
    return dx * dx + dy * dy;
}

std::ostream& operator<<(std::ostream& os, const V2& v)
{
    os << "(" << v.x << " , " << v.y << ")";
    return os;
}

// --------------------------------------------------------------------------------

inline double wrap_real(real_t x, real_t m) { return x - m * std::floor(x / m); }

class DeltaTime {
    real_t value;

public:
    inline real_t unwrap(void) const { return value; }
    DeltaTime(real_t dt) : value(dt) {}
};

template <typename R>
concept Rule = requires(R rule)
{
    {
        rule.delta
    }
    ->V2;
};

struct Boid {
    V2 pos;
    V2 vel;
    using Storage = BucketArrayStorage<Boid, 2000>;

    inline void advance(real_t dt)
    {
        pos += dt * vel;
        pos.x = wrap_real(pos.x, SCREEN_WIDTH_PIXELS);
        pos.y = wrap_real(pos.y, SCREEN_WIDTH_PIXELS);
    }

    inline void apply_rule(const Rule& rule) { vel += rule.delta; }
};

Boid random_boid(void)
{
    static std::mt19937 e(1213);
    static std::uniform_real_distribution<> pos_dis(300., 500.);
    static std::uniform_real_distribution<> vel_dis(-50., 50.);
    Boid b;
    b.pos.x = pos_dis(e);
    b.pos.y = pos_dis(e);
    b.vel.x = vel_dis(e);
    b.vel.y = vel_dis(e);
    return b;
}

inline bool operator==(const Boid& lhs, const Boid& rhs)
{
    return lhs.pos == rhs.pos && lhs.vel == rhs.vel;
}

inline bool operator!=(const Boid& lhs, const Boid& rhs) { return !(lhs == rhs); }

struct PseudoBoid {
    V2 pos;
    V2 vel;
    real_t weight;

    PseudoBoid() : pos(), vel(), weight(0.) {}
    PseudoBoid(const Boid& boid) : pos(boid.pos), vel(boid.vel), weight(1.) {}
};

// --------------------------------------------------------------------------------

// Store all the boids in a grid.
// When computing rules that rely on neighboring boids,
// only look at individual boids for neighboring grid cells.
// Otherwise, for distance cells, just take average of
// properties of all boids in the cell, weighted by the number of boids in the cell.
class Grid {
    static constexpr size_t N = 256;
    // static_assert(is_power_of_two(N));

    class Node {
        std::vector<Boid> m_contained;

        PseudoBoid m_pseudo_boid;

        inline real_t population(void) const { return (real_t)m_contained.size(); }

    public:
        Node() { m_contained.reserve(256); }

        void reset(void)
        {
            m_contained.clear();
            m_pseudo_boid.pos = {0., 0.};
            m_pseudo_boid.vel = {0., 0.};
            m_pseudo_boid.weight = 0.;
        }

        inline void insert(const Boid& boid) { m_contained.push_back(boid); }

        inline const PseudoBoid& get_pseudoboid(void) const { return m_pseudo_boid; }

        void recompute_pseudoboid(void)
        {
            for (const Boid& b : m_contained) {
                m_pseudo_boid.pos += b.pos;
                m_pseudo_boid.vel += b.vel;
            }

            const real_t pop = population();

            m_pseudo_boid.weight = pop;
            m_pseudo_boid.pos /= pop;
            m_pseudo_boid.vel /= pop;
        }

        const std::vector<Boid>& boids(void) const { return m_contained; }
    };

    std::array<Node, N * N> m_nodes;

    size_t m_population;

    static constexpr real_t NODE_DIMENSION = SCREEN_WIDTH_PIXELS / (real_t)N;
    static constexpr int NUM_CELLS = N * N;

    size_t boid_to_node_index(const Boid& boid) const
    {
        const size_t node_x = (size_t)std::floor(boid.pos.x / NODE_DIMENSION);
        const size_t node_y = (size_t)std::floor(boid.pos.y / NODE_DIMENSION);
        const size_t new_node = N * node_y + node_x;

        if (new_node >= N * N) {
            std::cout << "broken pos x: " << boid.pos.x << std::endl;
            std::cout << "broken pos y: " << boid.pos.y << std::endl;
            std::cout << "broken node x: " << node_x << std::endl;
            std::cout << "broken node y: " << node_y << std::endl;
            std::cout << "broken weird node: " << new_node << std::endl;
        }

        return new_node;
    }

    // static size_t manhattan_distance(size_t node1, size_t node2) {
    //     const int x1 = node1 & (N-1);
    //     const int x2 = node2 & (N-1);
    //     const int y1 = node1 / N;
    //     const int y2 = node2 / N;

    //     return std::abs(x1-x2) + std::abs(y1-y2);
    // }

public:
    void reset(void)
    {
        m_population = 0;
        for (auto& node : m_nodes) {
            node.reset();
        }
    }

    void recompute_pseudoboids(void)
    {
        for (auto& node : m_nodes) {
            node.recompute_pseudoboid();
        }
    }

    void insert(const Boid& boid)
    {
        const size_t node_id = boid_to_node_index(boid);
        m_nodes[node_id].insert(boid);
        m_population++;
    }

    inline real_t population(void) const { return m_population; }

    void get_pseudoboid_neighbors(const Boid& boid, std::vector<PseudoBoid>& results) const
    {
        results.clear();

        //@OPTIMIZE: pre-compute this and keep in a NodeID component?
        const int focus_node = boid_to_node_index(boid);

        for (int i = -FINE_GRAIN_CELL_LIMIT; i <= FINE_GRAIN_CELL_LIMIT; i++) {
            for (int j = -FINE_GRAIN_CELL_LIMIT; j <= FINE_GRAIN_CELL_LIMIT; j++) {
                const int node_itr = focus_node + N * j + i;
                if (node_itr >= 0 && node_itr < NUM_CELLS) {
                    for (const Boid& b : m_nodes[node_itr].boids()) {
                        //@OPTIMIZE: could store pointers for cheaper comparison here
                        if (node_itr == focus_node) {
                            if (boid != b) {
                                results.emplace_back(b);  //@SLOW?
                            }
                        }
                        else {
                            results.emplace_back(b);  //@SLOW?
                        }
                    }
                }
            }
        }

        auto advance_coarse_cell_index = [](int idx) -> int {
            if (idx == -FINE_GRAIN_CELL_LIMIT - 1) {
                return FINE_GRAIN_CELL_LIMIT + 1;
            }
            else {
                return idx + 1;
            }
        };

        for (int i = -COARSE_GRAIN_CELL_LIMIT; i <= COARSE_GRAIN_CELL_LIMIT;
             i = advance_coarse_cell_index(i)) {
            for (int j = -COARSE_GRAIN_CELL_LIMIT; j <= COARSE_GRAIN_CELL_LIMIT;
                 j = advance_coarse_cell_index(j)) {
                const int node_itr = focus_node + N * j + i;
                if (node_itr >= 0 && node_itr < NUM_CELLS) {
                    const PseudoBoid& pb = m_nodes[node_itr].get_pseudoboid();
                    if (pb.weight > 0.) {
                        results.push_back(pb);
                    }
                }
            }
        }
    }
};

// --------------------------------------------------------------------------------

struct RuleAvgVel {
    V2 delta;
    using Storage = BucketArrayStorage<RuleAvgVel, 2000>;
};

struct AvgVelRuleSystem {
    using Subscriptions = TypeList<Boid, RuleAvgVel>;

    using SystemData =
        std::tuple<ReadComponent<Boid>, WriteComponent<RuleAvgVel>, ReadResource<Grid>>;

    static constexpr real_t STRENGTH = 1.5;

    static void run(FollowedEntities followed, SystemData data)
    {
        auto [boid, avgvel_rule, grid] = data;

        static std::vector<PseudoBoid> pseudoboid_buffer;
        followed.for_each([&](const EntityID id) -> void {
            const Boid& b = boid[id];
            grid->get_pseudoboid_neighbors(b, pseudoboid_buffer);

            V2 sum_vel;
            real_t sum_weights = 0.;
            for (const PseudoBoid& pb : pseudoboid_buffer) {
                sum_vel += (pb.weight) * pb.vel;
                sum_weights += pb.weight;
            }

            avgvel_rule[id].delta = AvgVelRuleSystem::STRENGTH * sum_vel / sum_weights;
        });
    }
};

// --------------------------------------------------------------------------------

struct RuleConfine {
    V2 delta;
    using Storage = BucketArrayStorage<RuleConfine, 2000>;
};

struct ConfineRuleSystem {
    using Subscriptions = TypeList<Boid, RuleConfine>;

    using SystemData = std::tuple<ReadComponent<Boid>, WriteComponent<RuleConfine>>;

    static constexpr real_t STRENGTH = 500;

    static void run(FollowedEntities followed, SystemData data)
    {
        auto [boid, confine_rule] = data;

        followed.for_each([&](const EntityID id) -> void {
            const Boid& b = boid[id];
            RuleConfine& rule = confine_rule[id];
            rule.delta = {0., 0.};
            if (b.pos.x < 20.) {
                rule.delta.x += ConfineRuleSystem::STRENGTH;
            }
            else if (b.pos.x > SCREEN_WIDTH_PIXELS - 20.) {
                rule.delta.x -= ConfineRuleSystem::STRENGTH;
            }

            if (b.pos.y > SCREEN_WIDTH_PIXELS - 20.) {
                rule.delta.y -= ConfineRuleSystem::STRENGTH;
            }
            else if (b.pos.y < 20.) {
                rule.delta.y += ConfineRuleSystem::STRENGTH;
            }
        });
    }
};

// --------------------------------------------------------------------------------

struct RuleDensity {
    V2 delta;
    using Storage = BucketArrayStorage<RuleDensity, 2000>;
};

struct DensityRuleSystem {
    using Subscriptions = TypeList<Boid, RuleDensity>;

    using SystemData =
        std::tuple<ReadComponent<Boid>, WriteComponent<RuleDensity>, ReadResource<Grid>>;

    static constexpr real_t STRENGTH = 100;

    static void run(FollowedEntities followed, SystemData data)
    {
        auto [boid, density_rule, grid] = data;

        static std::vector<PseudoBoid> pseudoboid_buffer;
        followed.for_each([&](const EntityID id) -> void {
            const Boid& b = boid[id];
            //@OPTIMIZE: do for_each_neighbor to avoid copies?
            grid->get_pseudoboid_neighbors(b, pseudoboid_buffer);

            V2 c;
            for (const PseudoBoid& pb : pseudoboid_buffer) {
                const real_t d = std::max(distance_sq(pb.pos, b.pos), 0.1f);
                const V2 dv = b.pos - pb.pos;
                c += (pb.weight / d) * dv;
                // std::cout << "pseudoboid weight: " << pb.weight << std::endl;
                // std::cout << "pseudoboid pos: " << pb.pos << std::endl;
                // std::cout << "pseudoboid vel: " << pb.vel << std::endl;
            }

            // std::cout << "Density Rule Calculation for " << id << ": " <<
            // DensityRuleSystem::STRENGTH * c << std::endl;
            density_rule[id].delta = DensityRuleSystem::STRENGTH * c;
            // std::cout << "Density Rule Actual for " << id << ": " <<
            // density_rule[id].delta << std::endl;
        });
    }
};

// --------------------------------------------------------------------------------

struct RuleCOM {
    V2 delta;
    using Storage = BucketArrayStorage<RuleCOM, 2000>;
};

struct CenterOfMassRuleSystem {
    using Subscriptions = TypeList<Boid, RuleCOM>;

    using SystemData = std::tuple<ReadComponent<Boid>, WriteComponent<RuleCOM>, ReadResource<Grid>>;

    static constexpr real_t STRENGTH = 18.5;

    static std::vector<PseudoBoid> pseudoboid_buffer;
    static void run(FollowedEntities followed, SystemData data)
    {
        auto [boid, com_rule, grid] = data;

        static std::vector<PseudoBoid> pseudoboid_buffer;
        followed.for_each([&](const EntityID id) -> void {
            const Boid& b = boid[id];
            grid->get_pseudoboid_neighbors(b, pseudoboid_buffer);

            V2 c;
            real_t sum_weights = 0.;
            for (const PseudoBoid& pb : pseudoboid_buffer) {
                c += pb.weight * pb.pos;
                sum_weights += pb.weight;
            }

            // std::cout << "COM Rule Calculation for " <<
            // CenterOfMassRuleSystem::STRENGTH * c / (grid->population() - 1);
            if (sum_weights > 0.) {
                c /= sum_weights;
                com_rule[id].delta = CenterOfMassRuleSystem::STRENGTH * (c - b.pos);
            }
            // std::cout << "COM Rule Actual for " << id << ": " << com_rule[id].delta <<
            // std::endl;
        });
    }
};

struct DrawSystem {
    using Subscriptions = TypeList<Boid>;

    using SystemData = std::tuple<ReadComponent<Boid>>;

    static void run(FollowedEntities followed, SystemData data)
    {
        auto [boid] = data;

        followed.for_each([&](const EntityID id) -> void {
            const Boid& b = boid[id];
            glColor3f(b.vel.magnitude() / 300., std::fabs(b.vel.x) / 200.,
                      std::fabs(b.vel.y) / 200.);
            glVertex2f(b.pos.x, b.pos.y);
        });
    }
};

struct PositionUpdateSystem {
    using Subscriptions = TypeList<Boid, RuleCOM, RuleDensity, RuleAvgVel, RuleConfine>;

    using SystemData =
        std::tuple<WriteComponent<Boid>, ReadComponent<RuleCOM>, ReadComponent<RuleDensity>,
                   ReadComponent<RuleAvgVel>, ReadComponent<RuleConfine>, ReadResource<DeltaTime>,
                   WriteResource<Grid>>;

    static void run(FollowedEntities followed, SystemData data)
    {
        auto [boid, com_rule, density_rule, avg_vel_rule, confine_rule, delta_t, grid] = data;

        const real_t dt = delta_t->unwrap();

        followed.for_each_par([&](const EntityID id) -> void {
            Boid& b = boid[id];
            b.apply_rule(com_rule[id]);
            b.apply_rule(density_rule[id]);
            b.apply_rule(avg_vel_rule[id]);
            b.apply_rule(confine_rule[id]);
            b.advance(dt);
            // b.vel += {0., 15.};
            const real_t speed = b.vel.magnitude();
            if (speed > 200.) {
                b.vel = (200. / speed) * b.vel;
            }
            // std::cout << "new position after advance: " << id << " with pos: " <<
            // b.pos.x << " " << b.pos.y << std::endl;
        });

        grid->reset();
        for (const EntityID id : followed) {
            grid->insert(boid[id]);
        }
        grid->recompute_pseudoboids();
    }
};

// --------------------------------------------------------------------------------

using BoidComponents = TypeList<Boid, RuleCOM, RuleDensity, RuleAvgVel, RuleConfine>;

using BoidSystems = TypeList<CenterOfMassRuleSystem, DensityRuleSystem, AvgVelRuleSystem,
                             PositionUpdateSystem, ConfineRuleSystem, DrawSystem>;

using BoidResources = TypeList<DeltaTime, Grid>;

using BoidWorld = World<BoidComponents, BoidSystems, BoidResources>;

static BoidWorld* g_world;

static void tick(void)
{
    const auto build_start_time = high_resolution_clock::now();
    g_world->run_systems_sequential<PositionUpdateSystem>();
    g_world->run_systems_parallel<DensityRuleSystem, AvgVelRuleSystem, CenterOfMassRuleSystem,
                                  ConfineRuleSystem>();
    const auto build_end_time = high_resolution_clock::now();
    const double build_duration =
        duration_cast<duration<double>>(build_end_time - build_start_time).count();
    std::cout << "iter time: " << build_duration << std::endl;

    glClearColor(0.05f, 0.05f, 0.05f, 1.0f);  // Set background color to black and opaque
    glClear(GL_COLOR_BUFFER_BIT);
    glBegin(GL_POINTS);
    g_world->run_systems_sequential<DrawSystem>();
    glEnd();
    glutPostRedisplay();
    glutSwapBuffers();
}

int boids_main(int argc, char** argv)
{
    g_world = BoidWorld::init([](ResourceStash<BoidResources>& resources) {
        resources.construct_and_own<DeltaTime>(0.004);
        resources.construct_and_own<Grid>();
    });

    g_world->build_entities([&](EntityBuilder<BoidComponents> builder) {
        for (size_t i = 0; i < NUM_BOIDS; i++) {
            builder.new_entity()
                .attach<Boid>(random_boid())
                .attach<RuleCOM>()
                .attach<RuleDensity>()
                .attach<RuleAvgVel>()
                .attach<RuleConfine>();
        }
    });

    glutInit(&argc, argv);
    glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGB);

    glutInitWindowPosition(80, 80);
    glutInitWindowSize((size_t)SCREEN_WIDTH_PIXELS, (size_t)SCREEN_WIDTH_PIXELS);

    glutCreateWindow("Ark Boids");

    glClear(GL_COLOR_BUFFER_BIT);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    gluOrtho2D(0.0, SCREEN_WIDTH_PIXELS, SCREEN_WIDTH_PIXELS, 0.0);
    glutDisplayFunc(tick);

    glutMainLoop();

    delete g_world;
    return 0;
}

}  // namespace boids

int main(int argc, char** argv) { return boids::boids_main(argc, argv); }
