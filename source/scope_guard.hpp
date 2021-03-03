#pragma once

#define ALWAYS_INLINE inline __attribute__((always_inline))

template<class F>
class ScopeGuard {
    F f;
    bool active;

  public:
    constexpr ALWAYS_INLINE ScopeGuard(F f) : f(f), active(true) { }
    ALWAYS_INLINE ~ScopeGuard() { this->Invoke(); }
    ALWAYS_INLINE void Cancel() { active = false; }
    ALWAYS_INLINE void Invoke() {
        if (active) f();
        active = false;
    }
};