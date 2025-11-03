#include <exec/Result.h>
#include <exec/Unit.h>

#include <supp/CircularBuffer.h>

#include <utest/utest.h>

namespace exec {

enum Kind {
    CopyConstruct,
    CopyAssign,
    MoveConstruct,
    MoveAssign,
    Destroy,
};

struct Dummy;

struct Op {
    int tag;
    Kind op;
};

supp::CircularBuffer<Op, 10>* ops_ = nullptr;

struct Dummy {
    Dummy(int tag) : tag{tag} {}
    Dummy(const Dummy& r) : tag{r.tag} { ops_->push({tag, CopyConstruct}); }
    Dummy(Dummy&& r) : tag{std::exchange(r.tag, -1)} { ops_->push({tag, MoveConstruct}); }
    ~Dummy() { ops_->push({tag, Destroy}); }

    Dummy& operator=(const Dummy& r) {
        ops_->push({tag, CopyAssign});
        tag = r.tag;
        return *this;
    }

    Dummy& operator=(Dummy&& r) {
        ops_->push({tag, MoveAssign});
        tag = std::exchange(r.tag, -1);
        return *this;
    }

    int tag;
};

struct t_result {
    t_result() { ops_ = &ops; }
    ~t_result() { ops_ = nullptr; }

    template <std::same_as<Op>... Ops>
    void check(Ops... ops) {
        [[maybe_unused]] auto check = [&](Op op) {
            TEST_ASSERT_FALSE(this->ops.empty());
            auto actual = this->ops.pop();
            TEST_ASSERT_EQUAL(op.tag, actual.tag);
            TEST_ASSERT_EQUAL(op.op, actual.op);
        };

        (check(ops), ...);
        TEST_ASSERT_EQUAL(0, this->ops.size());
    }

    void clear() { ops.clear(); }

    supp::CircularBuffer<Op, 10> ops;
};

void checkClean(const auto& r) {
    TEST_ASSERT_FALSE(r.hasValue());
    TEST_ASSERT_EQUAL(ErrCode::Unknown, r.code());
}

TEST_F(t_result, result_default_constructor) {
    {
        Result<Dummy> r;
        checkClean(r);
    }

    check();
}

TEST_F(t_result, result_errcode_constructor) {
    {
        Result<Dummy> r{ErrCode::Cancelled};
        TEST_ASSERT_FALSE(r.hasValue());
        TEST_ASSERT_EQUAL(ErrCode::Cancelled, r.code());
    }

    check();
}

TEST_F(t_result, result_value_constructor) {
    {
        Result<Dummy> r{Dummy(10)};
        TEST_ASSERT_TRUE(r.hasValue());
        TEST_ASSERT_EQUAL(ErrCode::Success, r.code());
    }

    check(Op{10, MoveConstruct}, Op{-1, Destroy}, Op{10, Destroy});
}

TEST_F(t_result, result_copy_constructor) {
    SECTION("from error value") {
        {
            Result<Dummy> r{ErrCode::Cancelled};
            Result<Dummy> u = r;

            TEST_ASSERT_FALSE(r.hasValue());
            TEST_ASSERT_FALSE(u.hasValue());
            TEST_ASSERT_EQUAL(ErrCode::Cancelled, r.code());
            TEST_ASSERT_EQUAL(ErrCode::Cancelled, u.code());
        }

        check();
    }

    SECTION("from success value") {
        {
            Result<Dummy> r{Dummy(10)};

            clear();
            Result<Dummy> u = r;

            TEST_ASSERT_TRUE(r.hasValue());
            TEST_ASSERT_TRUE(u.hasValue());
            TEST_ASSERT_EQUAL(ErrCode::Success, r.code());
            TEST_ASSERT_EQUAL(ErrCode::Success, u.code());
        }

        check(Op{10, CopyConstruct}, Op{10, Destroy}, Op{10, Destroy});
    }
}

TEST_F(t_result, result_move_constructor) {
    SECTION("from error value") {
        {
            Result<Dummy> r{ErrCode::Cancelled};
            Result<Dummy> u = std::move(r);

            TEST_ASSERT_FALSE(r.hasValue());
            TEST_ASSERT_FALSE(u.hasValue());
            TEST_ASSERT_EQUAL(ErrCode::Cancelled, r.code());
            TEST_ASSERT_EQUAL(ErrCode::Cancelled, u.code());
        }

        check();
    }

    SECTION("from success value") {
        {
            Result<Dummy> r{Dummy(10)};

            clear();
            Result<Dummy> u = std::move(r);

            TEST_ASSERT_TRUE(u.hasValue());
            TEST_ASSERT_FALSE(r.hasValue());
            TEST_ASSERT_EQUAL(ErrCode::Success, u.code());
            TEST_ASSERT_EQUAL(ErrCode::Unknown, r.code());
        }

        check(Op{10, MoveConstruct}, Op{-1, Destroy}, Op{10, Destroy});
    }
}

TEST_F(t_result, result_copy_assign) {
    SECTION("error = error") {
        {
            Result<Dummy> r{ErrCode::Cancelled};
            Result<Dummy> u{ErrCode::OutOfMemory};

            clear();
            u = r;

            TEST_ASSERT_FALSE(r.hasValue());
            TEST_ASSERT_FALSE(u.hasValue());
            TEST_ASSERT_EQUAL(ErrCode::Cancelled, r.code());
            TEST_ASSERT_EQUAL(ErrCode::Cancelled, u.code());
        }

        check();
    }

    SECTION("error = value") {
        {
            Result<Dummy> r{Dummy(10)};
            Result<Dummy> u{ErrCode::OutOfMemory};

            clear();
            u = r;

            TEST_ASSERT_TRUE(r.hasValue());
            TEST_ASSERT_TRUE(u.hasValue());
        }

        check(Op{10, CopyConstruct}, Op{10, Destroy}, Op{10, Destroy});
    }

    SECTION("value = error") {
        {
            Result<Dummy> r{ErrCode::OutOfMemory};
            Result<Dummy> u{Dummy(10)};

            clear();
            u = r;

            TEST_ASSERT_FALSE(r.hasValue());
            TEST_ASSERT_FALSE(u.hasValue());
            TEST_ASSERT_EQUAL(ErrCode::OutOfMemory, r.code());
            TEST_ASSERT_EQUAL(ErrCode::OutOfMemory, u.code());
        }

        check(Op{10, Destroy});
    }

    SECTION("value = value") {
        {
            Result<Dummy> r{Dummy(10)};
            Result<Dummy> u{Dummy(20)};

            clear();
            u = r;

            TEST_ASSERT_TRUE(r.hasValue());
            TEST_ASSERT_TRUE(u.hasValue());
        }

        check(Op{20, CopyAssign}, Op{10, Destroy}, Op{10, Destroy});
    }
}

TEST_F(t_result, result_move_assign) {
    SECTION("error = error") {
        {
            Result<Dummy> r{ErrCode::Cancelled};
            Result<Dummy> u{ErrCode::OutOfMemory};

            clear();
            u = std::move(r);

            TEST_ASSERT_FALSE(r.hasValue());
            TEST_ASSERT_FALSE(u.hasValue());
            TEST_ASSERT_EQUAL(ErrCode::Unknown, r.code());
            TEST_ASSERT_EQUAL(ErrCode::Cancelled, u.code());
        }

        check();
    }

    SECTION("error = value") {
        {
            Result<Dummy> r{Dummy(10)};
            Result<Dummy> u{ErrCode::OutOfMemory};

            clear();
            u = std::move(r);

            TEST_ASSERT_FALSE(r.hasValue());
            TEST_ASSERT_EQUAL(ErrCode::Unknown, r.code());
            TEST_ASSERT_TRUE(u.hasValue());
        }

        check(Op{10, MoveConstruct}, Op{-1, Destroy}, Op{10, Destroy});
    }

    SECTION("value = error") {
        {
            Result<Dummy> r{ErrCode::OutOfMemory};
            Result<Dummy> u{Dummy(10)};

            clear();
            u = std::move(r);

            TEST_ASSERT_FALSE(r.hasValue());
            TEST_ASSERT_FALSE(u.hasValue());
            TEST_ASSERT_EQUAL(ErrCode::Unknown, r.code());
            TEST_ASSERT_EQUAL(ErrCode::OutOfMemory, u.code());
        }

        check(Op{10, Destroy});
    }

    SECTION("value = value") {
        {
            Result<Dummy> r{Dummy(10)};
            Result<Dummy> u{Dummy(20)};

            clear();
            u = std::move(r);

            TEST_ASSERT_FALSE(r.hasValue());
            TEST_ASSERT_EQUAL(ErrCode::Unknown, r.code());
            TEST_ASSERT_TRUE(u.hasValue());
        }

        check(
            Op{10, MoveConstruct},
            Op{-1, Destroy},
            Op{20, MoveAssign},
            Op{-1, Destroy},
            Op{10, Destroy});
    }
}

}  // namespace exec

TESTS_MAIN
