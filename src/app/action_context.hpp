#pragma once
#include "platform/input.hpp"
#include <array>
#include <cstdint>
#include <variant>
#include <vector>

namespace app {

enum class Action : uint32_t {
    None = 0,
    OrbitalRotation,
    moveForward,
    moveBackward,
    moveRight,
    moveLeft,
    moveUp,
    moveDown,
    cameraPan,
    SelectCamera,
    toggleCameraControlMode,
    toggleProjection,
    focusObject,
    count
};
enum class Trigger : uint8_t { Hold, Press };
// Binding grouping tag used by the gating stage: Walk bindings are evaluated
// only while the walk gesture is active, Default bindings only when no
// gesture is active (modal exclusivity / key swallowing).
enum class Mode    : uint8_t { Default, Walk };

enum class ModifierFlag : uint32_t {
    None  = 0,
    Shift = 1 << 0,
    Ctrl  = 1 << 1,
    Alt   = 1 << 2,
};
using ModMask = uint32_t;
constexpr ModifierFlag operator|(ModifierFlag lhs, ModifierFlag rhs) noexcept {
    return static_cast<ModifierFlag>( static_cast<ModMask>(lhs) | static_cast<ModMask>(rhs));
}
constexpr ModifierFlag operator&(ModifierFlag lhs, ModifierFlag rhs) noexcept {
    return static_cast<ModifierFlag>( static_cast<ModMask>(lhs) & static_cast<ModMask>(rhs));
}
constexpr ModifierFlag operator~(ModifierFlag flag) noexcept {
    return static_cast<ModifierFlag>( ~static_cast<ModMask>(flag));
}

class ActionContext {
    public:
        ActionContext();

        // keyboard binding interface
        void bind(platform::Key key, ModifierFlag mods, Action action, 
                    Trigger trigger, Mode mode, int param = 0);
        void bind(platform::Key key, Action action, 
                    Trigger trigger, Mode mode, int param = 0);
        // mouse binding interface
        void bind(platform::MouseButton mbutton, ModifierFlag mods, Action action, 
                    Trigger trigger, Mode mode, int param = 0);
        void bind(platform::MouseButton mbutton, Action action,
                    Trigger trigger, Mode mode, int param = 0);

        // update interface, call per frame
        void update(const platform::Input& input);

        [[nodiscard]]bool isActive(Action action) const;
        [[nodiscard]]bool onActivated(Action action) const;
        [[nodiscard]]bool onDeactivated(Action action) const;

        [[nodiscard]]int getParam(Action action) const;

    private:
        struct Binding {
            std::variant<platform::Key, platform::MouseButton> source;
            ModMask mods    = 0;
            Action  action; Trigger trigger = Trigger::Press; Mode mode    = Mode::Default;
            int     param   = 0;
        };

        std::vector<Binding>            bindings_;
        std::vector<bool>               rawMatch_, prevMatch_;
        std::array<bool, static_cast<size_t>(Action::count)>    level_{}, activated_{}, deactivated_{};
        std::array<int,  static_cast<size_t>(Action::count)>    param_{};
        std::array<bool, 3>             gestureActive_{}, prevButtonDown_{};
        std::array<Action, 3>           gestureAction_{};

        bool areModifiersDown(ModMask required, const platform::Input& input) const;
        bool anyGestureActive() const;
};

}