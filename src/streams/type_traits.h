#pragma once

template <typename T>
struct function_traits : function_traits<decltype(&T::operator())> {};

template <typename ClassType, typename ReturnType, typename... Args>
struct function_traits<ReturnType (ClassType::*)(Args...) const>
    : public function_traits<ReturnType (*)(Args...)> {};

template <typename ClassType, typename ReturnType, typename... Args>
struct function_traits<ReturnType (ClassType::*)(Args...)>
    : public function_traits<ReturnType (*)(Args...)> {};

template <typename ReturnType, typename... Args>
struct function_traits<ReturnType (*)(Args...)> {
  enum { ARITY = sizeof...(Args) };
  using result_type = ReturnType;

  template <size_t I>
  struct arg {
    using type = typename std::tuple_element<I, std::tuple<Args...>>::type;
  };
};
