#ifndef LITHIUM_SERVER_COMMAND_COMPONENT_HPP
#define LITHIUM_SERVER_COMMAND_COMPONENT_HPP

#include <any>
#include <chrono>
#include <deque>
#include <functional>
#include <iostream>
#include <thread>
#include <tuple>
#include <unordered_map>
#include <vector>

namespace lithium {

/**
 * @class HooksManager
 * @brief Manages hooks for state, effects, memoization, refs, and reducers.
 */
class HooksManager {
    struct State {
        std::any value;                        ///< The state value.
        std::function<void(std::any)> setter;  ///< The state setter function.
    };

    struct Effect {
        std::function<void()> setup;  ///< The setup function for the effect.
        std::function<void()>
            cleanup;                 ///< The cleanup function for the effect.
        std::vector<std::any> deps;  ///< The dependencies for the effect.
    };

    struct Memo {
        std::any value;  ///< The memoized value.
        std::vector<std::any>
            deps;  ///< The dependencies for the memoized value.
    };

    struct Ref {
        std::any current;            ///< The current value of the ref.
        std::vector<std::any> deps;  ///< The dependencies for the ref.
    };

    struct Reducer {
        std::function<std::any(std::any, std::any)>
            reducer;     ///< The reducer function.
        std::any state;  ///< The state managed by the reducer.
    };

    struct DeferredValue {
        std::any value;  ///< The deferred value.
        bool pending;    ///< Indicates if the value is pending.
    };

    std::deque<DeferredValue> deferred_values_;  ///< Queue of deferred values.
    size_t deferred_idx_ = 0;                    ///< Index for deferred values.

    static inline std::unordered_map<int, std::any>
        contexts_;  ///< Contexts map.

    std::deque<State> states_;      ///< Queue of states.
    std::deque<Effect> effects_;    ///< Queue of effects.
    std::deque<Memo> memos_;        ///< Queue of memoized values.
    std::deque<Ref> refs_;          ///< Queue of refs.
    std::deque<Reducer> reducers_;  ///< Queue of reducers.

    size_t state_idx_ = 0;    ///< Index for states.
    size_t effect_idx_ = 0;   ///< Index for effects.
    size_t memo_idx_ = 0;     ///< Index for memoized values.
    size_t ref_idx_ = 0;      ///< Index for refs.
    size_t reducer_idx_ = 0;  ///< Index for reducers.

    bool needs_render_ = true;  ///< Indicates if a render is needed.

    /**
     * @brief Checks if dependencies have changed.
     * @tparam Deps The types of the dependencies.
     * @param old_deps The old dependencies.
     * @param new_deps The new dependencies.
     * @return True if dependencies have changed, false otherwise.
     */
    template <typename... Deps>
    static bool deps_changed(const std::vector<std::any>& old_deps,
                             const std::tuple<Deps...>& new_deps) {
        if (old_deps.size() != sizeof...(Deps))
            return true;

        bool changed = false;
        size_t i = 0;
        std::apply(
            [&](const auto&... elems) {
                ((changed = changed || try_compare_any(old_deps[i++], elems)),
                 ...);
            },
            new_deps);
        return changed;
    }

    /**
     * @brief Compares two std::any values.
     * @tparam T The type of the second value.
     * @param a The first value.
     * @param b The second value.
     * @return True if the values are different, false otherwise.
     */
    template <typename T>
    static bool try_compare_any(const std::any& a, const T& b) {
        try {
            if (a.type() != typeid(T)) {
                return true;
            }
            if constexpr (std::is_same_v<T, std::any>) {
                const std::any& a_val = std::any_cast<const std::any&>(a);
                if (a_val.type() != b.type()) {
                    return true;
                }
                // If both are of the same type, try to compare their values
                if (a_val.type() == typeid(int))
                    return std::any_cast<int>(a_val) != std::any_cast<int>(b);
                if (a_val.type() == typeid(float))
                    return std::any_cast<float>(a_val) !=
                           std::any_cast<float>(b);
                if (a_val.type() == typeid(double))
                    return std::any_cast<double>(a_val) !=
                           std::any_cast<double>(b);

                if (a_val.type() == typeid(bool))
                    return std::any_cast<bool>(a_val) != std::any_cast<bool>(b);

                if (a_val.type() == typeid(std::string))
                    return std::any_cast<std::string>(a_val) !=
                           std::any_cast<std::string>(b);

                return false;
            } else {
                const T& a_val = std::any_cast<const T&>(a);
                return a_val != b;  // 正确返回比较结果
            }
        } catch (const std::bad_any_cast&) {
            return true;
        }
    }

    /**
     * @brief Compares a std::any value with a vector of std::any values.
     * @param a The first value.
     * @param b The vector of values.
     * @return True if the values are different, false otherwise.
     */
    static bool try_compare_any(const std::any& a,
                                const std::vector<std::any>& b) {
        try {
            const auto& a_vec = std::any_cast<const std::vector<std::any>&>(a);
            if (a_vec.size() != b.size())
                return true;
            for (size_t i = 0; i < a_vec.size(); ++i) {
                if (try_compare_any<std::any>(a_vec[i], b[i]))
                    return true;
            }
            return false;
        } catch (const std::bad_any_cast&) {
            return true;
        }
    }

public:
    /**
     * @brief Destructor for HooksManager.
     * Cleans up all effects.
     */
    ~HooksManager() {
        for (auto& effect : effects_) {
            if (effect.cleanup) {
                effect.cleanup();
            }
        }
    }

    /**
     * @brief Sets whether a render is needed.
     * @param needs_render True if a render is needed, false otherwise.
     */
    void setNeedsRender(bool needs_render) { needs_render_ = needs_render; }

    /**
     * @brief Manages state in a component.
     * @tparam T The type of the state.
     * @param initial The initial state value.
     * @return A pair containing a reference to the state and a setter function.
     */
    template <typename T>
    std::pair<T&, std::function<void(T)>> useState(T initial) {
        if (state_idx_ >= states_.size()) {
            states_.push_back(
                State{std::make_any<T>(initial),
                      [this, idx = state_idx_](std::any val) {
                          if (val.type() != typeid(T)) {
                              std::cerr << "Type mismatch in setter.\n";
                              return;
                          }
                          auto& current = std::any_cast<T&>(states_[idx].value);
                          current = std::any_cast<T>(val);
                          needs_render_ = true;
                      }});
        }

        T& value = std::any_cast<T&>(states_[state_idx_].value);

        auto setter = [setter = states_[state_idx_].setter](T val) {
            setter(std::make_any<T>(val));
        };

        state_idx_++;
        return {value, setter};
    }

    /**
     * @brief Manages side effects in a component.
     * @tparam Setup The type of the setup function.
     * @param setup The setup function.
     */
    template <typename Setup>
    void useEffect(Setup&& setup) {
        if (effect_idx_ >= effects_.size()) {
            auto cleanup = setup();
            effects_.emplace_back(
                Effect{std::forward<Setup>(setup), cleanup, {}});
        }
        effect_idx_++;
    }

    /**
     * @brief Manages side effects with dependencies in a component.
     * @tparam Setup The type of the setup function.
     * @tparam Deps The types of the dependencies.
     * @param setup The setup function.
     * @param deps The dependencies.
     */
    template <typename Setup, typename... Deps>
    void useEffect(Setup&& setup, Deps&&... deps) {
        std::tuple<Deps...> new_deps(std::forward<Deps>(deps)...);

        if (effect_idx_ >= effects_.size()) {
            auto cleanup = setup();
            effects_.emplace_back(Effect{std::forward<Setup>(setup),
                                         std::move(cleanup),
                                         {std::forward<Deps>(deps)...}});
        } else {
            auto& effect = effects_[effect_idx_];
            if (deps_changed(effect.deps, new_deps)) {
                if (effect.cleanup) {
                    effect.cleanup();
                }
                effect.cleanup = setup();
                effect.deps = {std::forward<Deps>(deps)...};
            }
        }
        effect_idx_++;
    }

    /**
     * @brief Manages state with a reducer function in a component.
     * @tparam StateType The type of the state.
     * @tparam Action The type of the action.
     * @param reducer The reducer function.
     * @param initialState The initial state value.
     * @return A pair containing a reference to the state and a dispatch
     * function.
     */
    template <typename StateType, typename Action>
    std::pair<std::reference_wrapper<StateType>, std::function<void(Action)>>
    useReducer(std::function<StateType(StateType, Action)> reducer,
               StateType initialState) {
        if (reducer_idx_ >= reducers_.size()) {
            auto wrapped_reducer = [reducer](std::any state,
                                             std::any action) -> std::any {
                return reducer(std::any_cast<StateType>(state),
                               std::any_cast<Action>(action));
            };
            reducers_.emplace_back(
                Reducer{wrapped_reducer, std::any(initialState)});
        }

        auto& reducer_obj = reducers_[reducer_idx_];
        StateType& state = std::any_cast<StateType&>(reducer_obj.state);

        std::function<void(Action)> dispatch =
            [this, reducer_idx = reducer_idx_](Action action) {
                auto& r = reducers_[reducer_idx];
                r.state = r.reducer(r.state, std::any(action));
                needs_render_ = true;
            };

        reducer_idx_++;
        return {std::ref(state), dispatch};
    }

    /**
     * @brief Memoizes a value in a component.
     * @tparam T The type of the value.
     * @tparam Factory The type of the factory function.
     * @tparam Deps The types of the dependencies.
     * @param factory The factory function.
     * @param deps The dependencies.
     * @return A reference to the memoized value.
     */
    template <typename T, typename Factory, typename... Deps>
    T& useMemo(Factory&& factory, Deps... deps) {
        if (memo_idx_ >= memos_.size()) {
            memos_.emplace_back(Memo{std::make_any<T>(factory()), {deps...}});
        } else {
            auto& memo = memos_[memo_idx_];
            if (deps_changed(memo.deps, std::tuple<Deps...>(deps...))) {
                memo.value = factory();
            }
        }
        return std::any_cast<T&>(memos_[memo_idx_++].value);
    }

    /**
     * @brief Memoizes a callback function in a component.
     * @tparam F The type of the callback function.
     * @tparam Deps The types of the dependencies.
     * @param fn The callback function.
     * @param deps The dependencies.
     * @return The memoized callback function.
     */
    template <typename F, typename... Deps>
    auto useCallback(F&& fn, Deps&&... deps) {
        using Func = std::decay_t<F>;
        return useMemo<Func>(
            [fn = std::forward<F>(fn)]() mutable {
                return std::forward<F>(fn);
            },
            std::forward<Deps>(deps)...);
    }

    /**
     * @brief Manages layout effects in a component.
     * @tparam Setup The type of the setup function.
     * @param setup The setup function.
     */
    template <typename Setup>
    void useLayoutEffect(Setup&& setup) {
        if (effect_idx_ >= effects_.size()) {
            auto cleanup = setup();
            effects_.emplace_back(
                Effect{std::forward<Setup>(setup), cleanup, {}});
        }
        effect_idx_++;
    }

    /**
     * @brief Manages imperative handles in a component.
     * @tparam T The type of the ref.
     * @tparam Handle The type of the handle.
     * @param ref The ref object.
     * @param handle The handle function.
     * @param deps The dependencies.
     */
    template <typename T, typename Handle>
    void useImperativeHandle(T& ref, Handle&& handle,
                             std::vector<std::any> deps) {
        if (ref_idx_ >= refs_.size()) {
            refs_.emplace_back(Ref{std::any(handle), deps});
            ref.current = handle;
        } else if (deps_changed(refs_[ref_idx_].deps, std::make_tuple(deps))) {
            refs_[ref_idx_].current = std::any(handle);
            refs_[ref_idx_].deps = deps;
            ref.current = handle;
        }
        ref_idx_++;
    }

    /**
     * @brief Manages refs in a component.
     * @tparam T The type of the ref.
     * @param initial The initial value of the ref.
     * @return A reference to the ref object.
     */
    template <typename T>
    struct RefObject {
        T current;
    };

    template <typename T>
    RefObject<T>& useRef(T initial = T{}) {
        if (ref_idx_ >= refs_.size()) {
            refs_.emplace_back(
                Ref{std::make_any<RefObject<T>>(RefObject<T>{initial}),
                    std::vector<std::any>{}});
        }
        return std::any_cast<RefObject<T>&>(refs_[ref_idx_++].current);
    }

    /**
     * @brief Manages context in a component.
     * @tparam T The type of the context.
     */
    template <typename T>
    class Context {
    public:
        /**
         * @brief Gets the context ID.
         * @return The context ID.
         */
        static int id() {
            static int context_id = 0;
            return context_id;
        }

        /**
         * @brief Provides a context value.
         */
        class Provider {
            int id_;
            T value_;

        public:
            /**
             * @brief Constructs a Provider.
             * @param value The context value.
             */
            Provider(T value) : id_(Context<T>::id()), value_(value) {
                HooksManager::contexts_[id_] = value_;
            }

            /**
             * @brief Destroys the Provider.
             */
            ~Provider() { HooksManager::contexts_.erase(id_); }
        };

        /**
         * @brief Gets the current context value.
         * @return The current context value.
         */
        static T& current() {
            return std::any_cast<T&>(HooksManager::contexts_[id()]);
        }
    };

    /**
     * @brief Uses a context value in a component.
     * @tparam T The type of the context.
     * @return The current context value.
     */
    template <typename T>
    static T& useContext() {
        return Context<T>::current();
    }

    /**
     * @brief Uses a deferred value in a component.
     * @tparam T The type of the value.
     * @param value The value.
     * @return The deferred value.
     */
    template <typename T>
    T useDeferredValue(const T& value) {
        if (deferred_idx_ >= deferred_values_.size()) {
            deferred_values_.push_back(DeferredValue{value, false});
        }

        auto& deferred = deferred_values_[deferred_idx_++];
        if (!deferred.pending) {
            deferred.value = value;
            deferred.pending = true;
            needs_render_ = true;
        }

        return std::any_cast<T>(deferred.value);
    }

    /**
     * @brief Uses a throttled callback function in a component.
     * @tparam F The type of the callback function.
     * @param fn The callback function.
     * @param wait_ms The wait time in milliseconds.
     * @return The throttled callback function.
     */
    template <typename F>
    auto useThrottleCallback(F&& fn, int wait_ms) {
        auto& ref = useRef<std::chrono::steady_clock::time_point>(
            std::chrono::steady_clock::time_point{}  // 明确初始化为时间起点
        );

        return useCallback(
            [fn = std::forward<F>(fn), wait_ms, &ref]() mutable {
                auto now = std::chrono::steady_clock::now();
                auto elapsed =
                    std::chrono::duration_cast<std::chrono::milliseconds>(
                        now - ref.current)
                        .count();

                if (elapsed >= wait_ms) {
                    ref.current = now;
                    fn();
                }
            },
            // 依赖项包括 ref，确保回调更新
            ref.current);
    }

    /**
     * @brief Uses a debounced callback function in a component.
     * @tparam F The type of the callback function.
     * @param fn The callback function.
     * @param wait_ms The wait time in milliseconds.
     * @return The debounced callback function.
     */
    template <typename F>
    auto useDebounceCallback(F&& fn, int wait_ms) {
        auto timer_ref =
            useRef<std::optional<std::chrono::steady_clock::time_point>>();

        return useCallback(
            [fn = std::forward<F>(fn), wait_ms, &timer_ref]() mutable {
                auto now = std::chrono::steady_clock::now();
                timer_ref.current = now;

                std::this_thread::sleep_for(std::chrono::milliseconds(wait_ms));

                if (timer_ref.current && *timer_ref.current == now) {
                    return fn();
                }
            });
    }

    /**
     * @brief Uses a try-catch block in a component.
     * @tparam T The return type of the function.
     * @param fn The function to try.
     * @param handler The exception handler function.
     * @return The result of the function or the handler.
     */
    template <typename T>
    T useTry(std::function<T()> fn, std::function<T(std::exception&)> handler) {
        try {
            return fn();
        } catch (std::exception& e) {
            return handler(e);
        }
    }

    void reset() {
        for (auto& effect : effects_) {
            if (effect.cleanup) {
                effect.cleanup();
            }
        }

        state_idx_ = 0;
        effect_idx_ = 0;
        memo_idx_ = 0;
        ref_idx_ = 0;
        reducer_idx_ = 0;
        deferred_idx_ = 0;  // 重置 deferred 索引

        // 清除所有 DeferredValue 的 pending 状态
        for (auto& dv : deferred_values_) {
            dv.pending = false;
        }
    }

    bool needsRender() const { return needs_render_; }

#ifdef TESTING
    friend class HooksManagerTest;
#endif

    template <typename T>
    void setStateValue(size_t index, const T& value) {
        if (index < states_.size()) {
            states_[index].setter(std::any(value));
        }
    }
};
}  // namespace lithium

#endif