#pragma once

namespace spud {
namespace detail {
//
template <typename> struct function_traits;

template <typename Function>
struct function_traits
    : public function_traits<
          decltype(&Function::template operator()<void (*)(...)>)> {};

template <typename ClassType, typename ReturnType, typename FirstArg,
          typename... Arguments>
struct function_traits<ReturnType (ClassType::*)(FirstArg, Arguments...) const>
    : function_traits<ReturnType (*)(ReturnType (*)(Arguments...),
                                     Arguments...)> {};

/* support the non-const operator ()
 * this will work with user defined functors */
template <typename ClassType, typename ReturnType, typename FirstArg,
          typename... Arguments>
struct function_traits<ReturnType (ClassType::*)(FirstArg, Arguments...)>
    : function_traits<ReturnType (*)(ReturnType (*)(Arguments...),
                                     Arguments...)> {};

template <typename ReturnType, typename FirstArg, typename... Arguments>
struct function_traits<ReturnType (*)(FirstArg, Arguments...)> {
  using FuncType = ReturnType (*)(ReturnType (*)(Arguments...), Arguments...);
};

template <typename> struct function_traits_ptr;

template <typename ClassType, typename ReturnType, typename FirstArg,
          typename... Arguments>
struct function_traits_ptr<ReturnType (ClassType::*)(FirstArg, Arguments...)
                               const>
    : function_traits_ptr<ReturnType (*)(ReturnType (*)(Arguments...),
                                         Arguments...)> {};

/* support the non-const operator ()
 * this will work with user defined functors */
template <typename ClassType, typename ReturnType, typename FirstArg,
          typename... Arguments>
struct function_traits_ptr<ReturnType (ClassType::*)(FirstArg, Arguments...)>
    : function_traits_ptr<ReturnType (*)(ReturnType (*)(Arguments...),
                                         Arguments...)> {};

template <typename ReturnType, typename FirstArg, typename... Arguments>
struct function_traits_ptr<ReturnType (*)(FirstArg, Arguments...)> {
  using FuncType = ReturnType (*)(ReturnType (*)(Arguments...), Arguments...);
};

} // namespace detail
} // namespace spud
