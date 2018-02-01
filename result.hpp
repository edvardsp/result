#pragma once

#include <exception>
#include <functional>
#include <optional>
#include <string_view>
#include <type_traits>
#include <variant>

namespace details {

template<typename T, typename U>
constexpr bool is_equiv_v = std::is_same_v< 
    std::decay_t< T >, 
    std::decay_t< U >
>;

template<typename R, typename Fn, typename A>
constexpr bool is_op_v = std::is_invocable_r_v< R, Fn, A >;

struct OkBase {};
struct ErrBase {};
struct ResultBase {};

template<typename OkT>
constexpr bool is_ok_v = std::is_base_of_v<
    OkBase,
    std::decay_t< OkT >
>;

template<typename ErrT>
constexpr bool is_err_v = std::is_base_of_v<
    ErrBase,
    std::decay_t< ErrT >
>;

template<typename ResT>
constexpr bool is_result_v = std::is_base_of_v<
    ResultBase,
    std::decay_t< ResT >
>;

template<typename T>
struct Ok : details::OkBase
{
    T t_;

    constexpr Ok(T t) : t_(std::move(t)) {}
    template<typename U>
    constexpr Ok(Ok< U > ok) : t_(std::move(ok.t_)) {}
};


template<typename E>
struct Err : details::ErrBase
{
    E e_;

    constexpr Err(E e) : e_(std::move(e)) {}
    template<typename F>
    constexpr Err(Err< F > err) : e_(std::move(err.e_)) {}
};

} /* namespace details */


template<typename T>
constexpr auto Ok(T&& t)
{
    using CleanT = std::decay_t< T >;
    return details::Ok< CleanT >(std::forward< T >(t));
}

template<typename E>
constexpr auto Err(E&& e)
{
    using CleanE = std::decay_t< E >;
    return details::Err< CleanE >(std::forward< E >(e));
}


template<typename T, typename E>
class Result : details::ResultBase
{
    using ResT = Result< T, E >;
    using OkT  = details::Ok< T >;
    using ErrE = details::Err< E >;

    std::variant< OkT, ErrE > variant_;

public:
    template<typename U>
    constexpr Result(details::Ok< U > ok) : variant_{ OkT( std::move(ok.t_) ) } {}

    template<typename F>
    constexpr Result(details::Err< F > err) : variant_{ ErrE( std::move(err.e_) ) } {}

    // Make movable
    constexpr Result(Result&&) = default;
    constexpr Result& operator=(Result&&) = default;

    // Make non-default constructable
    Result() = delete;
    // Make non-copyable
    Result(Result const&) = delete;
    Result& operator=(Result const&) = delete;

    // API
    constexpr bool is_ok() const { return std::holds_alternative< OkT >(variant_); }
    constexpr bool is_err() const { return std::holds_alternative< ErrE >(variant_); }

    constexpr std::optional< T > ok()
    {
        if (is_ok())
            return move_t_();
        else
            return {};
    }

    constexpr std::optional< E > err()
    {
        if (is_err())
            return move_e_();
        else
            return {};
    }

    constexpr T unwrap()
    {
        if (is_ok())
            return move_t_();
        else
            throw std::logic_error{ "Result::unwrap panicked" };
    }

    constexpr E unwrap_err()
    {
        if (is_err())
            return move_e_();
        else
            throw std::logic_error{ "Result::unwrawp_err panicked" };
    }

    template<typename U>
    constexpr T unwrap_or(U&& default_value)
    {
        static_assert(details::is_equiv_v< T, U >, 
            "Default value type is not equivalent to Ok type");
        if (is_ok())
            return move_t_();
        else
            return std::forward< U >(default_value);
    }

    template<typename Fn>
    constexpr T unwrap_or_else(Fn& fn)
    {
        static_assert(details::is_op_v< T, Fn, E >,
            "Function is not of signature Fn(E) -> T");
        if (is_ok())
            return move_t_();
        else
            return fn(move_e_());
    }

    constexpr T unwrap_or_default()
    {
        if (is_ok())
            return move_t_();
        else
            return T();
    }

    constexpr T expect(std::string_view msg)
    {
        if (is_ok())
            return move_t_();
        else
            throw std::logic_error( msg );
    }

    constexpr E expect_err(std::string_view msg)
    {
        if (is_err())
            return move_e_();
        else
            throw std::logic_error( msg );
    }

    template<typename Fn, typename U = std::invoke_result_t< Fn, T > >
    constexpr Result< U, E > map(Fn&& fn)
    {
        static_assert(details::is_op_v< U, Fn, T >,
            "Function is not of signature Fn(T) -> U");

        if (is_ok())
            return Ok( fn(move_t_()) );
        else
            return move_err_();
    }

    template<typename Fn, typename F = std::invoke_result_t< Fn, E > >
    constexpr Result< T, F > map_err(Fn& fn)
    {
        static_assert(details::is_op_v< F, Fn, T >,
            "Function is not of signature Fn(E) -> F");

        if (is_ok())
            return move_ok_();
        else
            return Err( fn(move_e_()) );
    }

    template<typename Res>
    constexpr Res and_(Res&& res)
    {
        static_assert(details::is_result_v< Res >,
            "Argument is not a Result type");
        static_assert(details::is_equiv_v< ErrE, typename Res::ErrE >,
            "Err type of argument and object is not equivalent");

        if (is_ok())
            return std::forward< Res >(res);
        else
            return move_err_();
    }

    template<typename Fn, typename Res = std::invoke_result_t< Fn, T > >
    constexpr Res and_then(Fn& fn)
    {
        static_assert(details::is_op_v< Res, Fn, T >,
            "Function is not of signature Fn(T) -> Res<U, E>");
        static_assert(details::is_result_v< Res >,
            "Return value is not a Result type");
        static_assert(details::is_equiv_v< ErrE, typename Res::ErrE >,
            "Err type of return value and object is not equivalent");

        if (is_ok())
            return fn(move_t_());
        else
            return move_err_();
    }

    template<typename Res>
    constexpr Res or_(Res&& res)
    {
        static_assert(details::is_result_v< Res >,
            "Argument is not a Result type");
        static_assert(details::is_equiv_v< OkT, typename Res::OkT >,
            "Ok type of argument and object is not equivalent");

        if (is_ok())
            return move_ok_();
        else
            return std::forward< Res >(res);
    }

    template<typename Fn, typename Res = std::invoke_result_t< Fn, E > >
    constexpr Res or_else(Fn& fn)
    {
        static_assert(details::is_op_v< Res, Fn, E >,
            "Function is not of signature Fn(E) -> Res<T, F>");
        static_assert(details::is_result_v< Res >,
            "Return value is not a Result type");
        static_assert(details::is_equiv_v< OkT, typename Res::OkT >,
            "Ok type of return value and object is not equivalent");

        if (is_ok())
            return move_ok_();
        else
            return fn(move_e_());
    }

    constexpr bool operator<(ResT const & other) { return rel_op_(other, std::less<>()); }
    constexpr bool operator>(ResT const & other) { return rel_op_(other, std::greater<>()); }
    constexpr bool operator<=(ResT const & other) { return rel_op_(other, std::less_equal<>()); }
    constexpr bool operator>=(ResT const & other) { return rel_op_(other, std::greater_equal<>()); }
    constexpr bool operator==(ResT const & other) { return rel_op_(other, std::equal_to<>()); }

private:
    template<typename Op>
    constexpr bool rel_op_(ResT const& other, Op & op)
    {
        if (is_ok() && other.is_ok())
            return op(get_t_(), other.get_t_());
        else if (is_err() && other.is_err())
            return op(get_e_(), other.get_e_());
        else
            return is_ok();
    }

    constexpr OkT & get_ok_() { return std::get< OkT >(variant_); }
    constexpr ErrE & get_err_() { return std::get< ErrE >(variant_); }

    constexpr OkT move_ok_() { return std::get< OkT >(std::move(variant_)); }
    constexpr ErrE move_err_() { return std::get< ErrE >(std::move(variant_)); }

    constexpr T & get_t_() { return get_ok_().t_; }
    constexpr E & get_e_() { return get_err_().e_; }

    constexpr T move_t_() { return std::move(get_t_()); }
    constexpr E move_e_() { return std::move(get_e_()); }

    template<typename U, typename F>
    friend class Result;
};

#define TRY(...) \
    ({ \
        auto res = (__VA_ARGS__); \
        if (res.is_err()) { \
            return res.unwrap_err(); \
        } \
        res.unwrap(); \
    })
