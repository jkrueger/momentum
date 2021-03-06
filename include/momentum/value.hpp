#pragma once

#include "util.hpp"

#include <iostream>
#include <memory>
#include <typeinfo>
#include <type_traits>

namespace imu {

  /**
   * A type that can hold any other value. This could be replaced with
   * std::any, once it's not experimental anymore.
   *
   */
  struct value {

    typedef std::shared_ptr<value> p;

    inline value()
      : pad(nullptr)
    {}

    template<typename T>
    inline value(const T& v)
      : pad(new value_pad<typename std::decay<const T>::type>(v))
    {}

    inline value(const value& cpy)
      : pad(cpy.pad ? cpy.pad->clone() : nullptr)
    {}

    inline value(value&& other)
      : pad(other.pad)
    {
      other.pad = nullptr;
    }

    inline ~value() {
      delete pad;
    }

    inline value& operator= (const value& cpy)
    {
      if (cpy.pad) {
        delete pad;
        pad = cpy.pad->clone();
      }
      return *this;
    }

    inline value& operator= (value&& other)
    {
      std::swap(pad, other.pad);
      delete other.pad;
      other.pad = nullptr;
      return *this;
    }

    inline bool is_set() const {
      return (bool) pad;
    }

    inline operator bool() const {
      return is_set();
    }

    template<typename T>
    inline const T& get() const {

      typedef typename std::decay<const T>::type value_type;
      typedef value_pad<value_type> pad_type;

      if (typeid(value_type) != type()) {
        throw bad_value_cast();
      }

      return dynamic_cast<const pad_type*>(pad)->value;
    }

    inline bool operator== (const value& r) const {
      return pad->equiv(r.pad);
    }

    template<typename T>
    inline bool operator== (const T& r) const {
      return get<T>() == r;
    }

    inline const std::type_info& type() const {
      return pad->type();
    }

    struct value_pad_base {

      typedef std::unique_ptr<value_pad_base> p;

      virtual ~value_pad_base() {}

      virtual value_pad_base* clone() const = 0;

      virtual bool equiv(const value_pad_base* other) const = 0;

      virtual const std::type_info& type() const = 0;
    };

    template<typename T>
    struct value_pad : public value_pad_base {

      T value;

      value_pad(const T& v)
        : value(v)
      {}

      value_pad(const T&& v)
        : value(static_cast<T&&>(v))
      {}

      value_pad_base* clone() const {
        return new value_pad(value);
      }

      bool equiv(const value_pad_base* other) const {
        return value == dynamic_cast<const value_pad*>(other)->value;
      }

      const std::type_info& type() const {
        return typeid(T);
      }
    };

    struct bad_value_cast : public std::bad_cast {

      virtual const char* what() const noexcept {
        return "Bad cast of imu::value to concrete type";
      }
    };

    const value_pad_base* pad;
  };

  template<typename T>
  inline const T& value_cast(const value& v) {
    return v.get<T>();
  }

  template<typename T>
  inline const T& value_cast(const value::p& v) {
    return v->get<T>();
  }

  template<>
  inline const value& value_cast<value>(const value& v) {
    return v;
  }

  template<
    typename T,
    typename X,
    typename = typename std::enable_if<
      !std::is_pointer<T>::value
      >::type>
  inline const T& value_cast(const X& x) {
    return static_cast<const T&>(x);
  }

  template<typename T, typename X>
  inline decltype(auto) value_cast(X* x) {
    typedef typename std::decay<
      typename std::remove_pointer<T>::type
      >::type type;
    return static_cast<type*>(x);
  }

  template<typename T, typename X>
  inline T value_cast(const std::shared_ptr<X>& x) {
    return std::static_pointer_cast<typename T::element_type>(x);
  }
}
