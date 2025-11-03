#pragma once

#include "exec/Runnable.h"
#include "exec/sm/Initiator.h"

#include <supp/Coro.h>
#include <supp/IntrusiveList.h>
#include <supp/Pinned.h>
#include <supp/tuple.h>
#include <supp/verify.h>

#include <array>
#include <cstdint>
#include <tuple>

namespace exec::detail {

enum class ParGroupType : uint8_t {
    All,
    Any,
};

template <size_t Children, ParGroupType Type>
class ParGroup : Runnable, CancellationHandler {
    static_assert(Children >= 2, "One child does not make sense for ParGroup");

 public:
    ParGroup() {
        for (auto& child : children_) {
            child.parent = this;
        }
    }

    // NOTE: Equivalent to the default constructor.
    ParGroup(ParGroup&&) : ParGroup() {}

    template <Initiator... Init>
    [[nodiscard]] Initiator auto operator()(Init&&... init) {
        static_assert(sizeof...(Init) > 1, "One child does not make sense for ParGroup");
        static_assert(sizeof...(Init) <= Children, "Too many children");

        return [this, inits = std::tuple<Init...>{std::forward<Init>(init)...}](
                   Runnable* cb, CancellationSlot slot = {}) mutable {
            return doWait(cb, slot, std::move(inits));
        };
    }

 private:
    struct Child : Runnable {
        CancellationSignal sig;
        ParGroup* parent;

        // Runnable. Called by child tasks.
        Runnable* run() override {
            sig.clear();
            return parent->childDone();
        }
    };

    template <typename... Init>
    Runnable* doWait(Runnable* cb, CancellationSlot slot, std::tuple<Init...> inits) {
        cb_ = cb;
        slot_ = slot;
        completed_ = 0;
        total_ = sizeof...(Init);
        slot.installIfConnected(this);

        // Clear all cancellation signals
        supp::constexprFor<0, Children, 1>([&](auto ic) { children_[ic].sig.clear(); });

        // Connect first |Init...| signals to child tasks and initiate them
        supp::constexprFor<0, sizeof...(Init), 1>([&](auto ic) {
            Initiator auto&& init = std::get<ic()>(inits);

            // Initiator cannot call the final callback directly
            auto* child = &children_[ic];
            if (auto* cont = init(child, child->sig.slot())) {
                queue_.pushBack(cont);
            }
        });

        // We do not run child initiation continuations here. See run().
        return queue_.empty() ? noop : this;
    }

    Runnable* childDone() {
        DASSERT(completed_ < total_);
        auto new_completed = ++completed_;

        if constexpr (Type == ParGroupType::Any) {
            if (new_completed == 1) {
                // The first task has been completed
                // cancel() always returns noop
                return cancel();
            }
        }

        if (new_completed < total_) {
            // Not all hasks have completed
            return noop;
        }

        if constexpr (Type == ParGroupType::All) {
            slot_.clearIfConnected();
        }

        // All tasks have completed
        return cb_;
    }

    // CancellationSignal
    Runnable* cancel() override {
        // Clear the cancellation slot, acknowledging the cancellation
        slot_.clearIfConnected();

        // Cancel all child tasks, without running their callbacks
        // because the completion may then be called before
        // cancellation of one of the later tasks
        for (size_t i = 0; i < total_; ++i) {
            if (auto* task = children_[i].sig.emitRaw()) {
                queue_.pushBack(task);
            }
        }

        // See run() implementation
        return queue_.empty() ? noop : this;
    }

    // Runnable. Runs all available continuations
    Runnable* run() override {
        while (!queue_.empty()) {
            queue_.popFront()->runAll();
        }

        return noop;
    }

    // This queue holds both initiation and cancellation continuations.
    supp::IntrusiveForwardList<Runnable> queue_;

    // Children
    std::array<Child, Children> children_;

    CancellationSlot slot_;
    Runnable* cb_ = noop;
    uint8_t completed_ = 0;
    uint8_t total_ = 0;
};

}  // namespace exec::detail
