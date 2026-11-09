#include <Geode/Geode.hpp>
#include <Geode/modify/GJBaseGameLayer.hpp>

using namespace geode::prelude;

// 800 ms at Geometry Dash's normal 240 physics steps per second.
static constexpr int BUFFER_DURATION_STEPS = 192;

struct InputBufferState {
    int step = 0;
    bool pending = false;
    int expiresAt = 0;
    bool releasedBeforeLanding = false;
    bool bufferedPressIsDown = false;
    bool releaseNextStep = false;
};

class $modify(InputBufferLayer, GJBaseGameLayer) {
    struct Fields {
        InputBufferState player1;
        InputBufferState player2;
    };

    void sendJump(PlayerObject* player, bool down) {
        if (!player) return;
        if (down) {
            player->pushButton(PlayerButton::Jump);
        } else {
            player->releaseButton(PlayerButton::Jump);
        }
    }

    InputBufferState& stateFor(bool isPlayer1) {
        return isPlayer1 ? m_fields->player1 : m_fields->player2;
    }

    PlayerObject* playerFor(bool isPlayer1) {
        return isPlayer1 ? m_player1 : m_player2;
    }

    void updateBuffer(PlayerObject* player, InputBufferState& state) {
        if (!player) {
            state = InputBufferState();
            return;
        }

        // A short tap is replayed as a one-step press/release once landing
        // makes the jump valid. A held press stays down until the real release.
        if (state.releaseNextStep) {
            state.releaseNextStep = false;
            state.bufferedPressIsDown = false;
            sendJump(player, false);
        }

        if (!state.pending) return;

        if (state.step > state.expiresAt) {
            state.pending = false;
            state.releasedBeforeLanding = false;
            return;
        }

        // Only ground-based forms need this buffer. Flying forms retain
        // Geometry Dash's normal input behavior.
        bool isFlying = player->m_isShip || player->m_isBird ||
                        player->m_isDart || player->m_isSwing;
        if (isFlying) {
            state.pending = false;
            state.releasedBeforeLanding = false;
            return;
        }

        if (player->m_isOnGround) {
            state.pending = false;
            sendJump(player, true);
            state.bufferedPressIsDown = true;

            if (state.releasedBeforeLanding) {
                state.releaseNextStep = true;
            }
        }
    }

    void processCommands(float dt, bool isHalfTick, bool isLastTick) {
        ++m_fields->player1.step;
        ++m_fields->player2.step;

        GJBaseGameLayer::processCommands(dt, isHalfTick, isLastTick);

        if (!PlayLayer::get()) {
            m_fields->player1 = InputBufferState();
            m_fields->player2 = InputBufferState();
            return;
        }

        updateBuffer(m_player1, m_fields->player1);
        updateBuffer(m_player2, m_fields->player2);
    }

    void handleButton(bool down, int button, bool isPlayer1) {
        // Geometry Dash button 1 is jump. Leave all other controls untouched.
        if (button != 1 || !PlayLayer::get()) {
            GJBaseGameLayer::handleButton(down, button, isPlayer1);
            return;
        }

        // In normal one-player levels, route input to player 1 even if the
        // event was marked as player 2. In two-player levels, keep them separate.
        bool twoPlayer = m_levelSettings && m_levelSettings->m_twoPlayerMode;
        bool targetPlayer1 = isPlayer1 || !twoPlayer;
        auto* player = targetPlayer1 ? m_player1 : m_player2;
        auto& state = stateFor(targetPlayer1);

        if (!player) {
            GJBaseGameLayer::handleButton(down, button, isPlayer1);
            return;
        }

        if (down) {
            // Don't replace an existing queued press with repeated touch-downs.
            if (state.pending) return;

            bool isFlying = player->m_isShip || player->m_isBird ||
                            player->m_isDart || player->m_isSwing;
            if (!isFlying && !player->m_isOnGround) {
                state.pending = true;
                state.expiresAt = state.step + BUFFER_DURATION_STEPS;
                state.releasedBeforeLanding = false;
                return; // hold the press until landing or expiry
            }

            if (state.bufferedPressIsDown) {
                sendJump(player, false);
                state.bufferedPressIsDown = false;
            }
            GJBaseGameLayer::handleButton(down, button, isPlayer1);
            return;
        }

        if (state.pending) {
            state.releasedBeforeLanding = true;
            return; // remember the tap; replay it if landing happens in time
        }

        if (state.bufferedPressIsDown) {
            state.bufferedPressIsDown = false;
            state.releaseNextStep = false;
            sendJump(player, false);
            return;
        }

        GJBaseGameLayer::handleButton(down, button, isPlayer1);
    }
};

$on_mod(Loaded) {
    log::info("Input Buffer loaded (800 ms jump buffer).");
}
