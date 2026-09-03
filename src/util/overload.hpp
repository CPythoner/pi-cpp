#pragma once

namespace pi {

// C++17 经典 overload 访问工具（std::visit 配套，替代 C++20 的泛型 lambda）。
// 用法：std::visit(overload{[](const A&){...}, [](const B&){...}}, v);
template <class... Ts> struct overload : Ts... { using Ts::operator()...; };
template <class... Ts> overload(Ts...) -> overload<Ts...>;

} // namespace pi
