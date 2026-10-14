// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include <windows.h>
#include <array>
#include "music_hotkeys.h"

namespace rwr {
// UI producers never call the audio engine or wait for the music thread. A
// full/contended queue rejects the click so its caller can report that outcome.
// The music thread consumes at most one command per scheduler status poll.
class MusicCommandQueue {
public:
    static constexpr size_t capacity = 16;
    MusicCommandQueue() = default;
    MusicCommandQueue(const MusicCommandQueue&) = delete;
    MusicCommandQueue& operator=(const MusicCommandQueue&) = delete;
    bool tryPush(MusicAction action) {
        if (action < MusicAction::Next || action > MusicAction::PreviousTrack ||
            !TryAcquireSRWLockExclusive(&lock_)) return false;
        const bool accepted = count_ < commands_.size();
        if (accepted) {
            commands_[(head_ + count_) % commands_.size()] = action;
            ++count_;
        }
        ReleaseSRWLockExclusive(&lock_);
        return accepted;
    }
    MusicAction tryPop() {
        if (!TryAcquireSRWLockExclusive(&lock_)) return MusicAction::None;
        MusicAction action = MusicAction::None;
        if (count_) {
            action = commands_[head_];
            head_ = (head_ + 1) % commands_.size();
            --count_;
        }
        ReleaseSRWLockExclusive(&lock_);
        return action;
    }
    bool tryClear() {
        if (!TryAcquireSRWLockExclusive(&lock_)) return false;
        head_ = count_ = 0;
        ReleaseSRWLockExclusive(&lock_);
        return true;
    }
private:
    SRWLOCK lock_ = SRWLOCK_INIT;
    std::array<MusicAction, capacity> commands_{};
    size_t head_ = 0;
    size_t count_ = 0;
};

inline MusicCommandQueue uiMusicCommands;
inline bool queueUiMusicAction(MusicAction action) { return uiMusicCommands.tryPush(action); }
inline MusicAction popUiMusicAction() { return uiMusicCommands.tryPop(); }
} // namespace rwr
