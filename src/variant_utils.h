#pragma once

#include <boost/mpl/vector.hpp>
#include <boost/variant.hpp>

namespace satori {
namespace video {
namespace variantutils {

template <typename Variant, typename... ExtraTypes>
struct extend_variant {
 private:
  using variant_types = typename Variant::types;
  static_assert(boost::mpl::is_sequence<variant_types>::value,
                "probably not a boost::variant");

  using extra_types = boost::mpl::vector<ExtraTypes...>;
  using aggregated_types =
      typename boost::mpl::joint_view<variant_types, extra_types>::type;

 public:
  using type = typename boost::make_variant_over<aggregated_types>::type;
};

}  // namespace variantutils
}  // namespace video
}  // namespace satori