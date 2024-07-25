// Copyright Valve Corporation, All rights reserved.
//
// COM interface smart pointer.

#ifndef SE_COMMON_COM_COM_PTR_H_
#define SE_COMMON_COM_COM_PTR_H_

#include <objbase.h>
#include <comip.h>

#include <type_traits>

namespace se::win::com {

/**
 * @brief COM interface concept.
 * @tparam TInterface Interface which should be COM one.
 */
template <typename TInterface>
using com_interface_concept =
    std::enable_if_t<std::is_abstract_v<TInterface> &&
                     std::is_base_of_v<IUnknown, TInterface>>;

/**
 * @brief COM smart pointer with automatic IID deducing from TInterface.
 */
template <typename TInterface, const IID *TIid = &__uuidof(TInterface),
          typename = com_interface_concept<TInterface>>
class com_ptr : public _com_ptr_t<_com_IIID<TInterface, TIid>> {};

}  // namespace se::win::com

#endif  // !SE_COMMON_COM_COM_PTR_H_
