#include "app/action_context.hpp"
#include "platform/input.hpp"
#include <algorithm>
#include <cstddef>
#include <variant>

namespace app {

ActionContext::ActionContext() {
    level_.fill(false);
}

// ---------- binding interface ----------
void ActionContext::bind(platform::Key key, ModifierFlag mods, Action action,
                            Trigger trigger, Mode mode, int param) {
    bindings_.emplace_back(Binding{key, static_cast<ModMask>(mods), action,
                                     trigger, mode, param});
}

void ActionContext::bind(platform::Key key, Action action,
                            Trigger trigger, Mode mode, int param) {
    bind(key, ModifierFlag::None, action, trigger, mode, param);
}

void ActionContext::bind(platform::MouseButton mbutton, ModifierFlag mods, Action action,
                            Trigger trigger, Mode mode, int param) {
    bindings_.emplace_back(Binding{mbutton, static_cast<ModMask>(mods), action,
                                     trigger, mode, param});
}

void ActionContext::bind(platform::MouseButton mbutton, Action action, 
                            Trigger trigger, Mode mode, int param) {
    bind(mbutton, ModifierFlag::None, action, trigger, mode, param);
}
// ------------------------------------

// ---------- check key down ----------
bool ActionContext::areModifiersDown(ModMask required, const platform::Input& input) const {
    if((required & static_cast<ModMask>(ModifierFlag::Shift)) &&
        !input.isDown(platform::Key::LeftShift)) return false;
    if((required & static_cast<ModMask>(ModifierFlag::Ctrl)) &&
        !input.isDown(platform::Key::LeftCtrl)) return false;
    if((required & static_cast<ModMask>(ModifierFlag::Alt)) &&
        !input.isDown(platform::Key::LeftAlt)) return false;
    
    return true;
}

bool ActionContext::isActive(Action action) const {
    return level_[static_cast<size_t>(action)];
}
bool ActionContext::onActivated(Action action) const {
    return activated_[static_cast<size_t>(action)];
}
bool ActionContext::onDeactivated(Action action) const {
    return deactivated_[static_cast<size_t>(action)];
}

int ActionContext::getParam(Action action) const {
    return param_[static_cast<size_t>(action)];
}
// ------------------------------------

bool ActionContext::anyGestureActive() const {
    return std::ranges::any_of(gestureActive_, [](bool v) { return v; });
}

void ActionContext::update(const platform::Input& input) {
    if(rawMatch_.size() != bindings_.size()) {
        rawMatch_.assign(bindings_.size(), false);
        prevMatch_.assign(bindings_.size(), false);
    }
    activated_.fill(false);
    deactivated_.fill(false);

    for(size_t i = 0; i < bindings_.size(); ++i) {
        const auto& b = bindings_[i];
        const bool isDown = std::visit([&input](const auto& src) {
            return input.isDown(src);
        }, b.source);
        rawMatch_[i] = isDown && areModifiersDown(b.mods, input);
    }

    // --- gesture state machine ---
    for(size_t b = 0 ; b < 3 ; ++b) {
        const auto button = static_cast<platform::MouseButton>(b);
        const bool down = input.isDown(button);
        if(gestureActive_[b]) {
            if(!down) {
                deactivated_[static_cast<size_t>(gestureAction_[b])] = true;
                level_[static_cast<size_t>(gestureAction_[b])] = false;
                gestureActive_[b] = false;
                gestureAction_[b] = Action::None;
            }
        }
        else if(down && !prevButtonDown_[b] && !anyGestureActive()) {
            for(size_t i = 0; i < bindings_.size(); ++i) {
                const auto src = std::get_if<platform::MouseButton>(&bindings_[i].source);
                if(src && *src == button && rawMatch_[i]) {
                    auto idx = static_cast<size_t>(bindings_[i].action);
                    gestureActive_[b] = true;
                    gestureAction_[b] = bindings_[i].action;
                    activated_[idx] = true;
                    level_[idx] = true;
                    break;
                }
            }
        }
        prevButtonDown_[b] = down;
    }
    const bool gestureActive = anyGestureActive();
    // --- key binding ---
   for (size_t i = 0; i < bindings_.size(); ++i) {
        const Binding& b = bindings_[i];
        if (std::holds_alternative<platform::MouseButton>(b.source)) continue;

        bool gated = rawMatch_[i];
        gated = (b.mode == Mode::Walk) ? (gated && level_[static_cast<size_t>(Action::toggleCameraControlMode)])
                                       : (gated && !gestureActive);

        auto idx = static_cast<size_t>(b.action);
        if (gated && !prevMatch_[i]) {                   
            activated_[idx] = true;
            param_[idx]     = b.param;   
        }
        if (!gated && prevMatch_[i]) deactivated_[idx] = true;
        if (b.trigger == Trigger::Hold) level_[idx] = gated;
        prevMatch_[i] = gated;                         
    }
}

}