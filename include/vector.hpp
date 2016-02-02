#pragma once

#include "seq.hpp"
#include "util.hpp"
#include "value.hpp"

#include <memory>
#include <vector>

namespace imu {

  namespace ty {

    template<typename mixin = no_mixin>
    struct base_node {

      typedef std::shared_ptr<base_node> p;

      mixin _mixin;

      virtual ~base_node()
      {}
    };

    template<typename mixin = no_mixin>
    struct basic_node : public base_node<mixin> {

      typedef std::shared_ptr<basic_node> p;
      typedef typename base_node<mixin>::p base;

      std::vector<base> _arr;

      inline basic_node()
        : _arr(32)
      {}

      inline basic_node(const base& root, const base& node)
        : basic_node()
      {
        _arr[0] = root;
        _arr[1] = node;
      }

      inline basic_node(const base& child)
        : basic_node()
      {
        _arr[0] = child;
      }

      static inline base new_path(uint64_t level, const base& node) {

        base out(node);
        for(auto l = level; level != 0; level -= 5) {
          out = base(nu<basic_node>(out));
        }
        return out;
      }

      static inline p push_tail(
          uint64_t idx
        , uint64_t level
        , const p& parent
        , const base& tail) {

        p out(nu<basic_node>(*parent));
        base insert;

        if (level == 5) {
          insert = tail;
        }
        else {
          auto child = parent->_arr[idx];
          if (child) {
            auto as_base = std::dynamic_pointer_cast<basic_node>(child);
            insert = push_tail(idx, level - 5, as_base, tail);
          }
          else {
            insert = new_path(level - 5, tail);
          }
        }

        out->_arr[idx] = insert;

        return out;
      }
    };

    template<typename mixin = no_mixin>
    struct basic_leaf : public base_node<mixin> {

      typedef std::shared_ptr<basic_leaf> p;
      typedef base_node<mixin> base;

      std::vector<value> _arr;

      inline basic_leaf()
      {}

      inline basic_leaf(const basic_leaf::p& a)
        : _arr(a->_arr)
      {}

      inline const value& operator[](uint64_t n) const {
        return _arr[n];
      }
    };

    typedef basic_node<> node;
    typedef basic_leaf<> leaf;

    template<
        typename node  = node
      , typename leaf  = leaf
      , typename mixin = no_mixin
      >
    struct basic_vector {

      typedef std::shared_ptr<basic_vector> p;
      typedef typename node::base base_node;

      typedef value value_type;

      mixin _mixin;

      uint64_t _cnt;
      uint64_t _shift;

      typename node::p _root;
      typename leaf::p _tail;

      inline basic_vector()
        : _cnt(0)
        , _shift(5)
        , _root(nu<node>())
        , _tail(nu<leaf>())
      {}

      template<typename T>
      inline basic_vector(const p& v, const T& val)
        : _cnt(v->_cnt + 1)
        , _shift(v->_shift)
        , _tail(v->_tail)
      {
        if ((v->_cnt - v->tail_off()) < 32) {
          _shift = v->_shift;
          _root  = v->_root;
          _tail->_arr.push_back(value(val));
        }
        else {
          extend_root(v, value(val));
        }
      }

      inline bool is_empty() const {
        return _cnt == 0;
      }

      inline uint64_t count() const {
        return _cnt;
      }

      template<typename T>
      inline const T& nth(uint64_t n) const {
        return (*leaf_for(n))[n & 0x01f].template get<T>();
      }

      template<typename S>
      inline friend bool operator== (
        const p& self, const std::shared_ptr<S> x) {

        return seqs::equiv(seq(self), x);
      }

      inline uint64_t tail_off() const {
        return (_cnt < 32) ? 0 : ((_cnt - 1) >> 5) << 5;
      }

      inline const typename leaf::p leaf_for(uint64_t n) const {
        if (n >= 0 && n < _cnt) {
          if (n >= tail_off()) {
            return _tail;
          }
          typename node::base out = _root;
          for (auto level = _shift; level > 0; level -= 5) {
            auto inner = std::dynamic_pointer_cast<node>(out);
            out = inner->_arr[(n >> level) & 0x01f];
          }
          return std::dynamic_pointer_cast<leaf>(out);
        }
        throw out_of_bounds(n, _cnt);
      }

      inline void extend_root(const basic_vector::p& v, const value& val) {

        auto new_leaf = nu<leaf>(v->_tail);

        bool overflow = ((v->_cnt >> 5) > (1 << v->_shift));
        if (overflow) {
          _shift = v->_shift + 5;
          _root  = nu<node>(v->_root, node::new_path(v->_shift, new_leaf));
        }
        else {
          uint64_t idx = ((v->_cnt - 1) >> _shift) & 0x01f;
          _root = node::push_tail(idx, _shift, v->_root, new_leaf);
        }
      }
    };

    typedef basic_vector<> vector;

    template<typename mixin = no_mixin>
    struct basic_chunked_seq {

      typedef std::shared_ptr<basic_chunked_seq> p;

      typedef typename vector::value_type value_type;

      mixin _mixin;

      vector::p _vec;
      uint64_t  _idx;
      uint64_t  _off;

      leaf::p _leaf;

      inline basic_chunked_seq(const vector::p& v, uint64_t i, uint64_t o)
        : _vec(v)
        , _idx(i)
        , _off(o)
        , _leaf(v->leaf_for(_idx))
      {}

      inline bool is_empty() const {
        return (_off >= _leaf->_arr.size());
      }

      template<typename T>
      inline const T& first() const {
        return value_cast<T>((*_leaf)[_off]);
      }

      inline const value_type& first() const {
        return (*_leaf)[_off];
      };

      inline p rest() const {
        if (!is_empty()) {
          return nu<basic_chunked_seq>(_vec, _idx, _off + 1);
        }
        return p();
      }

      template<typename S>
      inline friend bool operator== (
        const p& self, const std::shared_ptr<S> x) {

        return seqs::equiv(self, x);
      }
    };

    typedef basic_chunked_seq<> chunked_seq;
  }

  inline ty::vector::p vector() {
    return nu<ty::vector>();
  }

  template<typename Arg>
  inline ty::vector::p vector(const ty::vector::p& v, const Arg& x) {
    return conj(v, x);
  }

  template<typename Arg, typename... Args>
  inline ty::vector::p vector(const ty::vector::p& v, const Arg& x, Args... args) {
    return vector(conj(v, x), args...);
  }

  template<typename Arg, typename... Args>
  inline ty::vector::p vector(const Arg& x, Args... args) {
    return vector(vector(), x, args...);
  }

  inline ty::chunked_seq::p seq(const ty::vector::p& v) {
    if (!v || v->is_empty()) {
      return ty::chunked_seq::p();
    }
    return nu<ty::chunked_seq>(v, 0, 0);
  }

  template<typename T>
  inline ty::vector::p conj(const ty::vector::p& v, const T& x) {
    return nu<ty::vector>(v, x);
  }
}
