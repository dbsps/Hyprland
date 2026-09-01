#include <layout/algorithm/Algorithm.hpp>
#include <layout/algorithm/TiledAlgorithm.hpp>
#include <layout/algorithm/floating/FloatingAlgorithm.hpp>

#include <gtest/gtest.h>

using namespace Layout;

namespace {
    // records whether it was handed a message, and opts in only when told to
    template <typename Base>
    class CRecordingAlgorithm : public Base {
      public:
        CRecordingAlgorithm(bool optIn) : m_optIn(optIn) {}

        bool                m_optIn = false;
        bool                m_saw   = false;

        Config::ErrorResult layoutMsg(const std::string_view& sv) override {
            m_saw = true;
            return {};
        }

        bool supportsTargetedLayoutMsg(const std::string_view& sv) const override {
            return m_optIn;
        }

        void newTarget(SP<ITarget>) override {}
        void movedTarget(SP<ITarget>, std::optional<Vector2D>) override {}
        void removeTarget(SP<ITarget>) override {}
        void resizeTarget(const Vector2D&, SP<ITarget>, eRectCorner) override {}
        void recalculate(eRecalculateReason) override {}
        void swapTargets(SP<ITarget>, SP<ITarget>) override {}
        void moveTargetInDirection(SP<ITarget>, Math::eDirection, bool) override {}
    };

    class CRecordingTiled : public CRecordingAlgorithm<ITiledAlgorithm> {
      public:
        using CRecordingAlgorithm::CRecordingAlgorithm;
        SP<ITarget> getNextCandidate(SP<ITarget>) override {
            return nullptr;
        }
    };

    using CRecordingFloating = CRecordingAlgorithm<IFloatingAlgorithm>;

    struct SFixture {
        CRecordingTiled*    tiled    = nullptr;
        CRecordingFloating* floating = nullptr;
        SP<CAlgorithm>      algo;

        SFixture(bool tiledOptIn, bool floatingOptIn) {
            auto t   = makeUnique<CRecordingTiled>(tiledOptIn);
            auto f   = makeUnique<CRecordingFloating>(floatingOptIn);
            tiled    = t.get();
            floating = f.get();
            algo     = CAlgorithm::create(std::move(t), std::move(f), nullptr);
        }
    };
}

// a mode algorithm that did not opt in must not receive a targeted message
// because the other one did. this is the whole point of a per-mode gate, and it
// is only true while asking and dispatching stay a single decision.
TEST(Layout, targetedLayoutMsgOnlyReachesOptedInModes) {
    {
        SFixture f{/* tiled */ true, /* floating */ false};
        EXPECT_TRUE(f.algo->targetedLayoutMsg("whatever"));
        EXPECT_TRUE(f.tiled->m_saw);
        EXPECT_FALSE(f.floating->m_saw);
    }

    {
        SFixture f{/* tiled */ false, /* floating */ true};
        EXPECT_TRUE(f.algo->targetedLayoutMsg("whatever"));
        EXPECT_FALSE(f.tiled->m_saw);
        EXPECT_TRUE(f.floating->m_saw);
    }

    {
        SFixture f{/* tiled */ true, /* floating */ true};
        EXPECT_TRUE(f.algo->targetedLayoutMsg("whatever"));
        EXPECT_TRUE(f.tiled->m_saw);
        EXPECT_TRUE(f.floating->m_saw);
    }

    {
        SFixture f{/* tiled */ false, /* floating */ false};
        EXPECT_FALSE(f.algo->targetedLayoutMsg("whatever")); // refused
        EXPECT_FALSE(f.tiled->m_saw);
        EXPECT_FALSE(f.floating->m_saw);
    }
}
