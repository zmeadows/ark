// template <Component T>
// float defragment_component_storage(double time_left) {
//     if constexpr(storage::is_bucket_array<typename T::Storage>::value) {
//         typename T::Storage* store = m_component_stash.template get<T>();
//         const std::optional<double> predicted_time = store->estimate_maintenance_time();
//         if (predicted_time && *predicted_time < time_left) {
//             const auto start = high_resolution_clock::now();
//             store->maintenance();
//             const auto end = high_resolution_clock::now();
//             const double dur = duration_cast<duration<double>>(end - start).count();
//             return time_left - dur;
//         } else {
//             return time_left;
//         }
//     } else {
//         return time_left;
//     }
// }

// void run_maintanence(float allowed_time) {
//     run_maintanence(allowed_time, AllComponents());
// }

// template <typename T, typename... Ts>
// void run_maintanence(float allowed_time, const TypeList<T,Ts...>&) {
//     allowed_time = defragment_component_storage<T>(allowed_time);
//     run_maintanence(allowed_time, TypeList<Ts...>());
// }

// void run_maintanence(float, const TypeList<>&) {
//     return;
// }

// ------------------------------------------------------------------------------------
// Profiling

/*
static constexpr size_t PROFILE_FRAME_GROUP = 30;

class TimingProfiler {
    std::array<size_t, PROFILE_FRAME_GROUP> m_recent_ns;
    size_t m_index;

    std::vector<double> m_history_ms;

public:
    void start_timer(void);
    void stop_timer(void);

    const std::vector<double>& history_ms(void) const { return m_history_ms; }
    const double last_iteration_time_ms(void) const;
};

class MemoryProfiler {
    std::array<size_t, PROFILE_FRAME_GROUP> m_used_kb;
    std::array<size_t, PROFILE_FRAME_GROUP> m_allocated_kb;
    std::vector<size_t> m_history_kb;

public:
    void profile(void);
};

std::unordered_map<size_t, SystemProfiler> m_system_profilers;
std::unordered_map<size_t, ComponentProfiler> m_component_profilers;
*/
