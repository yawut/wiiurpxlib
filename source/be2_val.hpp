// Copyright (C) 2020 decaf-emu contributors, Ash Logan <ash@heyquark.com>
// Licensed under the terms of the GNU GPL, version 3
// http://www.gnu.org/licenses/gpl-3.0.txt
// https://github.com/decaf-emu/decaf-emu/blob/6ef8a24da13e519e68ce3c6a6fdece44841aaac2/src/libcpu/be2_val.h

#pragma once

#include <utility>
#include <type_traits>

// Same as std::underlying_type but works for non-enum Types
template<class T, bool = std::is_enum<T>::value>
struct safe_underlying_type : std::underlying_type<T> { };

template<class T>
struct safe_underlying_type<T, false>
{
   using type = T;
};

template<typename Type>
class be2_val
{
public:
   static_assert(!std::is_array<Type>::value,
                 "be2_val invalid type: array");

   static_assert(!std::is_pointer<Type>::value,
                 "be2_val invalid type: pointer");

   static_assert(sizeof(Type) == 1 || sizeof(Type) == 2 || sizeof(Type) == 4 || sizeof(Type) == 8,
                 "be2_val invalid type size");

   using value_type = Type;

   be2_val() = default;

   template<typename OtherType,
            typename = typename std::enable_if<std::is_constructible<value_type, const OtherType &>::value ||
                                               std::is_convertible<const OtherType &, value_type>::value>::type>
   be2_val(const OtherType &other)
   {
      if constexpr (std::is_constructible<value_type, const OtherType &>::value) {
         setValue(value_type { other });
      } else {
         setValue(static_cast<value_type>(other));
      }
   }

   template<typename OtherType,
            typename = typename std::enable_if<std::is_constructible<value_type, const OtherType &>::value ||
                                               std::is_convertible<const OtherType &, value_type>::value>::type>
   be2_val(OtherType &&other)
   {
      if constexpr (std::is_constructible<value_type, const OtherType &>::value) {
         setValue(value_type { std::forward<OtherType>(other) });
      } else {
         setValue(static_cast<value_type>(std::forward<OtherType>(other)));
      }
   }

   template<typename OtherType,
            typename = typename std::enable_if<std::is_convertible<const OtherType &, value_type>::value ||
                                               std::is_constructible<value_type, const OtherType &>::value>::type>
   be2_val(const be2_val<OtherType> &other)
   {
      if constexpr (std::is_constructible<value_type, const OtherType &>::value) {
         setValue(value_type { other.value() });
      } else {
         setValue(static_cast<value_type>(other.value()));
      }
   }

   template<typename OtherType,
            typename = typename std::enable_if<std::is_convertible<const OtherType &, value_type>::value ||
                                               std::is_constructible<value_type, const OtherType &>::value>::type>
   be2_val(be2_val<OtherType> &&other)
   {
      if constexpr (std::is_constructible<value_type, const OtherType &>::value) {
         setValue(value_type { other.value() });
      } else {
         setValue(static_cast<value_type>(other.value()));
      }
   }

   value_type value() const
   {
      if constexpr (sizeof(value_type) == 1) {
         return mStorage;
      } else {
         union {
            value_type v;
            unsigned char c[sizeof(value_type)];
         } pun = { .v = mStorage };
         if constexpr (sizeof(value_type) == 2) {
            return (pun.c[0] << 8) | (pun.c[1] & 0xff);
         } else if constexpr (sizeof(value_type) == 4) {
            return (pun.c[0] << 24) | (pun.c[1] << 16) | (pun.c[2] << 8) | (pun.c[3] & 0xff);
         } else if constexpr (sizeof(value_type) == 8) {
            return (pun.c[0] << 56) | (pun.c[1] << 48) | (pun.c[2] << 40) | (pun.c[3] << 32) |
                   (pun.c[4] << 24) | (pun.c[5] << 16) | (pun.c[6] << 8) | (pun.c[7] & 0xff);
         }
      }
   }

   void setValue(value_type value)
   {
      if constexpr (sizeof(value_type) == 1) {
         mStorage = value;
      } else {
         typedef union {
            value_type v;
            unsigned char c[sizeof(value_type)];
         } pun;
         if constexpr (sizeof(value_type) == 2) {
            mStorage = (pun) { .c = {
               [0] = (unsigned char)(value >> 8),
               [1] = (unsigned char)(value >> 0),
            }}.v;
         } else if constexpr (sizeof(value_type) == 4) {
            mStorage = (pun) { .c = {
               [0] = (unsigned char)(value >> 24),
               [1] = (unsigned char)(value >> 16),
               [2] = (unsigned char)(value >>  8),
               [3] = (unsigned char)(value >>  0),
            }}.v;
         } else if constexpr (sizeof(value_type) == 8) {
            mStorage = (pun) { .c = {
               [0] = (unsigned char)(value >> 56),
               [1] = (unsigned char)(value >> 48),
               [2] = (unsigned char)(value >> 40),
               [3] = (unsigned char)(value >> 32),
               [4] = (unsigned char)(value >> 24),
               [5] = (unsigned char)(value >> 16),
               [6] = (unsigned char)(value >>  8),
               [7] = (unsigned char)(value >>  0),
            }}.v;
         }
      }
   }

   operator value_type() const
   {
      return value();
   }

   template<typename T = Type,
            typename = typename std::enable_if<std::is_convertible<T, bool>::value ||
                                               std::is_constructible<bool, T>::value
                                              >::type>
   explicit operator bool() const
   {
      return static_cast<bool>(value());
   }

   template<typename OtherType,
            typename = typename std::enable_if<std::is_convertible<Type, OtherType>::value ||
                                               std::is_constructible<OtherType, Type>::value ||
                                               std::is_convertible<Type, typename safe_underlying_type<OtherType>::type>::value
                                              >::type>
   explicit operator OtherType() const
   {
      return static_cast<OtherType>(value());
   }

   template<typename OtherType,
            typename = typename std::enable_if<std::is_constructible<value_type, const OtherType &>::value ||
                                               std::is_convertible<const OtherType &, value_type>::value>::type>
   be2_val & operator =(const OtherType &other)
   {
      if constexpr (std::is_constructible<value_type, const OtherType &>::value) {
         setValue(value_type { other });
      } else {
         setValue(static_cast<value_type>(other));
      }

      return *this;
   }

   template<typename OtherType,
            typename = typename std::enable_if<std::is_constructible<value_type, const OtherType &>::value ||
                                               std::is_convertible<const OtherType &, value_type>::value>::type>
   be2_val & operator =(OtherType &&other)
   {
      if constexpr (std::is_constructible<value_type, const OtherType &>::value) {
         setValue(value_type { std::forward<OtherType>(other) });
      } else {
         setValue(static_cast<value_type>(std::forward<OtherType>(other)));
      }

      return *this;
   }

   template<typename OtherType,
            typename = typename std::enable_if<std::is_convertible<const OtherType &, value_type>::value ||
                                               std::is_constructible<value_type, const OtherType &>::value>::type>
   be2_val & operator =(const be2_val<OtherType> &other)
   {
      if constexpr (std::is_constructible<value_type, const OtherType &>::value) {
         setValue(value_type { other.value() });
      } else {
         setValue(static_cast<value_type>(other.value()));
      }

      return *this;
   }

   template<typename OtherType,
            typename = typename std::enable_if<std::is_convertible<const OtherType &, value_type>::value ||
                                               std::is_constructible<value_type, const OtherType &>::value>::type>
   be2_val & operator =(be2_val<OtherType> &&other)
   {
      if constexpr (std::is_constructible<value_type, const OtherType &>::value) {
         setValue(value_type { other.value() });
      } else {
         setValue(static_cast<value_type>(other.value()));
      }

      return *this;
   }

   template<typename OtherType, typename K = value_type>
   auto operator ==(const OtherType &other) const
      -> decltype(std::declval<const K>().operator ==(std::declval<const OtherType>()))
   {
      return value() == other;
   }

   template<typename OtherType, typename K = value_type>
   auto operator !=(const OtherType &other) const
      -> decltype(std::declval<const K>().operator !=(std::declval<const OtherType>()))
   {
      return value() != other;
   }

   template<typename OtherType, typename K = value_type>
   auto operator >=(const OtherType &other) const
      -> decltype(std::declval<const K>().operator >=(std::declval<const OtherType>()))
   {
      return value() >= other;
   }

   template<typename OtherType, typename K = value_type>
   auto operator <=(const OtherType &other) const
      -> decltype(std::declval<const K>().operator <=(std::declval<const OtherType>()))
   {
      return value() <= other;
   }

   template<typename OtherType, typename K = value_type>
   auto operator >(const OtherType &other) const
      -> decltype(std::declval<const K>().operator >(std::declval<const OtherType>()))
   {
      return value() > other;
   }

   template<typename OtherType, typename K = value_type>
   auto operator <(const OtherType &other) const
      -> decltype(std::declval<const K>().operator <(std::declval<const OtherType>()))
   {
      return value() < other;
   }

   template<typename K = value_type>
   auto operator +() const
      ->  decltype(std::declval<const K>(). operator+())
   {
      return +value();
   }

   template<typename K = value_type>
   auto operator -() const
      -> decltype(std::declval<const K>(). operator-())
   {
      return -value();
   }

   template<typename OtherType, typename K = value_type>
   auto operator +(const OtherType &other) const
      -> decltype(std::declval<const K>().operator +(std::declval<const OtherType>()))
   {
      return value() + other;
   }

   template<typename OtherType, typename K = value_type>
   auto operator -(const OtherType &other)const
      -> decltype(std::declval<const K>().operator -(std::declval<const OtherType>()))
   {
      return value() - other;
   }

   template<typename OtherType, typename K = value_type>
   auto operator *(const OtherType &other) const
      -> decltype(std::declval<const K>().operator *(std::declval<const OtherType>()))
   {
      return value() * other;
   }

   template<typename OtherType, typename K = value_type>
   auto operator /(const OtherType &other) const
      -> decltype(std::declval<const K>().operator /(std::declval<const OtherType>()))
   {
      return value() / other;
   }

   template<typename OtherType, typename K = value_type>
   auto operator %(const OtherType &other) const
      -> decltype(std::declval<const K>().operator %(std::declval<const OtherType>()))
   {
      return value() % other;
   }

   template<typename OtherType, typename K = value_type>
   auto operator |(const OtherType &other) const
      -> decltype(std::declval<const K>().operator |(std::declval<const OtherType>()))
   {
      return value() | other;
   }

   template<typename OtherType, typename K = value_type>
   auto operator &(const OtherType &other) const
      -> decltype(std::declval<const K>().operator &(std::declval<const OtherType>()))
   {
      return value() & other;
   }

   template<typename OtherType, typename K = value_type>
   auto operator ^(const OtherType &other) const
      -> decltype(std::declval<const K>().operator ^(std::declval<const OtherType>()))
   {
      return value() ^ other;
   }

   template<typename OtherType, typename K = value_type>
   auto operator <<(const OtherType &other) const
      -> decltype(std::declval<const K>().operator <<(std::declval<const OtherType>()))
   {
      return value() << other;
   }

   template<typename OtherType, typename K = value_type>
   auto operator >>(const OtherType &other) const
      -> decltype(std::declval<const K>().operator >>(std::declval<const OtherType>()))
   {
      return value() >> other;
   }

   template<typename OtherType,
            typename = decltype(std::declval<const value_type>() + std::declval<const OtherType>())>
   be2_val &operator +=(const OtherType &other)
   {
      *this = static_cast<value_type>(value() + other);
      return *this;
   }

   template<typename OtherType,
            typename = decltype(std::declval<const value_type>() - std::declval<const OtherType>())>
   be2_val &operator -=(const OtherType &other)
   {
      *this = static_cast<value_type>(value() - other);
      return *this;
   }

   template<typename OtherType,
            typename = decltype(std::declval<const value_type>() * std::declval<const OtherType>())>
   be2_val &operator *=(const OtherType &other)
   {
      *this = static_cast<value_type>(value() * other);
      return *this;
   }

   template<typename OtherType,
            typename = decltype(std::declval<const value_type>() / std::declval<const OtherType>())>
   be2_val &operator /=(const OtherType &other)
   {
      *this = static_cast<value_type>(value() / other);
      return *this;
   }

   template<typename OtherType,
            typename = decltype(std::declval<const value_type>() % std::declval<const OtherType>())>
   be2_val &operator %=(const OtherType &other)
   {
      *this = static_cast<value_type>(value() % other);
      return *this;
   }

   template<typename OtherType,
            typename = decltype(std::declval<const value_type>() | std::declval<const OtherType>())>
   be2_val &operator |=(const OtherType &other)
   {
      *this = static_cast<value_type>(value() | other);
      return *this;
   }

   template<typename OtherType,
            typename = decltype(std::declval<const value_type>() & std::declval<const OtherType>())>
   be2_val &operator &=(const OtherType &other)
   {
      *this = static_cast<value_type>(value() & other);
      return *this;
   }

   template<typename OtherType,
            typename = decltype(std::declval<const value_type>() ^ std::declval<const OtherType>())>
   be2_val &operator ^=(const OtherType &other)
   {
      *this = static_cast<value_type>(value() ^ other);
      return *this;
   }

   template<typename OtherType,
            typename = decltype(std::declval<const value_type>() << std::declval<const OtherType>())>
   be2_val &operator <<=(const OtherType &other)
   {
      *this = value() << other;
      return *this;
   }

   template<typename OtherType,
            typename = decltype(std::declval<const value_type>() >> std::declval<const OtherType>())>
   be2_val &operator >>=(const OtherType &other)
   {
      *this = value() >> other;
      return *this;
   }

   template<typename T = Type,
            typename = decltype(std::declval<const T>() + 1)>
   be2_val &operator ++()
   {
      setValue(value() + 1);
      return *this;
   }

   template<typename T = Type,
            typename = decltype(std::declval<const T>() + 1)>
   be2_val operator ++(int)
   {
      auto before = *this;
      setValue(value() + 1);
      return before;
   }

   template<typename T = Type,
            typename = decltype(std::declval<const T>() - 1)>
   be2_val &operator --()
   {
      setValue(value() - 1);
      return *this;
   }

   template<typename T = Type,
            typename = decltype(std::declval<const T>() - 1)>
   be2_val operator --(int)
   {
      auto before = *this;
      setValue(value() - 1);
      return before;
   }

   template<typename IndexType,
            typename K = value_type>
   auto operator [](const IndexType &index)
      -> decltype(std::declval<K>().operator [](std::declval<IndexType>()))
   {
      return value().operator [](index);
   }

   template<typename IndexType,
            typename K = value_type>
   auto operator [](const IndexType &index) const
      -> decltype(std::declval<const K>().operator [](std::declval<IndexType>()))
   {
      return value().operator [](index);
   }

   template<typename K = value_type>
   auto operator ->()
      -> decltype(std::declval<K>().operator ->())
   {
      return value().operator ->();
   }

   template<typename K = value_type>
   auto operator ->() const
      -> decltype(std::declval<const K>().operator ->())
   {
      return value().operator ->();
   }

   template<typename K = value_type>
   auto operator *()
      -> decltype(std::declval<K>().operator *())
   {
      return value().operator *();
   }

   template<typename K = value_type>
   auto operator *() const
      -> decltype(std::declval<const K>().operator *())
   {
      return value().operator ->();
   }

   // Helper to access FunctionPointer::getAddress
   template<typename K = value_type>
   auto getAddress() const
      -> decltype(std::declval<const K>().getAddress())
   {
      return value().getAddress();
   }

   // Helper to access Pointer::get
   template<typename K = value_type>
   auto get() const
      -> decltype(std::declval<const K>().get())
   {
      return value().get();
   }

   // Helper to access Pointer::getRawPointer
   template<typename K = value_type>
   auto getRawPointer() const
      -> decltype(std::declval<const K>().getRawPointer())
   {
      return value().getRawPointer();
   }

   //we don't have those types, EXJAM
   // Please use virt_addrof or phys_addrof instead
   //auto operator &() = delete;

private:
   value_type mStorage;
};
