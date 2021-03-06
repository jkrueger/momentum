#pragma once

#include "seq.hpp"
#include "util.hpp"
#include "value.hpp"

#include <memory>
#include <vector>

namespace imu {

  namespace ty {

    template<typename mixin = no_mixin>
    struct base_node : public mixin {

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

      inline basic_node(const p& root)
        : _arr(root->_arr)
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
          uint64_t cnt
        , uint64_t level
        , const p& parent
        , const base& tail) {

        p out(nu<basic_node>(*parent));
        base insert;

        uint64_t idx = ((cnt - 1) >> level) & 0x01f;

        if (level == 5) {
          insert = tail;
        }
        else {
          auto child = parent->_arr[idx];
          if (child) {
            auto as_base = std::dynamic_pointer_cast<basic_node>(child);
            insert = push_tail(cnt, level - 5, as_base, tail);
          }
          else {
            insert = new_path(level - 5, tail);
          }
        }

        out->_arr[idx] = insert;

        return out;
      }
    };

    template<typename Value, typename mixin = no_mixin>
    struct basic_leaf : public base_node<mixin> {

      typedef std::shared_ptr<basic_leaf> p;
      typedef base_node<mixin>  base;
      typedef basic_node<mixin> node;

      std::vector<Value> _arr;

      inline basic_leaf()
      {}

      inline basic_leaf(const basic_leaf::p& a)
        : _arr(a->_arr)
      {}

      inline basic_leaf(const Value& val)
      { _arr.push_back(val); }

      inline const Value& operator[](uint64_t n) const {
        return _arr[n];
      }

      static inline typename base::p assoc(
        const typename base::p& n, uint64_t level,
        uint64_t k, const Value& v) {

        uint64_t idx = (k >> level) & 0x01f;
        if (level == 0) {
          auto leaf = std::static_pointer_cast<basic_leaf>(n);
          leaf->_arr[idx] = v;
          return leaf;
        }
        else {
          auto bn  = std::static_pointer_cast<node>(n);
          auto ret = nu<node>(bn);

          ret->_arr[idx] = assoc(bn->_arr[idx], level - 5, k, v);

          return ret;
        }
      }
    };

    struct vector_tag {};

    template<
        typename Value = value
      , typename mixin = no_mixin
      , typename node  = basic_node<>
      , typename leaf  = basic_leaf<Value>
      >
    struct basic_vector : public mixin, vector_tag {

      typedef typename mixin::template semantics<basic_vector>::p p;

      typedef typename node::base base_node;
      typedef leaf leaf_type;

      typedef Value value_type;

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

      inline basic_vector(const p& v)
        : _cnt(v->_cnt)
        , _shift(v->_shift)
        , _root(v->_root)
        , _tail(nu<leaf>(v->_tail))
      {}

      inline basic_vector(const p& v, const Value& val)
        : _cnt(v->_cnt + 1)
        , _shift(v->_shift)
      {
        if ((v->_cnt - v->tail_off()) < 32) {
          _shift = v->_shift;
          _root  = v->_root;
          _tail  = nu<leaf>(v->_tail);
          _tail->_arr.push_back(val);
        }
        else {
          extend_root(v, val);
        }
      }

      static inline p factory() {
        return nu<basic_vector>();
      }

      template<typename Arg>
      static inline p factory(const p& v, const Arg& x) {
        return conj(v, x);
      }

      template<typename Arg, typename... Args>
      static inline p factory(const p& v, const Arg& x, Args... args) {
        return factory(conj(v, x), args...);
      }

      template<typename Arg, typename... Args>
      static inline p factory(const Arg& x, Args... args) {
        return factory(factory(), x, args...);
      }

      template<typename T>
      static inline p from_std(const T& b, const T& e) {
        auto out = nu<basic_vector>();
        for (auto i=b; i!=e; ++i) {
          out = nu<basic_vector>(out, *i);
        }
        return out;
      }

      template<typename T>
      static inline p from_std(const T& coll) {
        return from_std(std::begin(coll), std::end(coll));
      }

      inline bool is_empty() const {
        return _cnt == 0;
      }

      inline uint64_t count() const {
        return _cnt;
      }

      inline const value_type& nth(uint64_t n) const {
        return (*leaf_for(n))[n & 0x01f];
      }

      template<typename T>
      inline const T& nth(uint64_t n) const {
        return value_cast<T>(nth(n));
      }

      inline const value_type& operator[](uint64_t n) const {
        return nth(n);
      }

      inline const value_type& operator()(uint64_t n) const {
        return nth(n);
      }

      template<typename S>
      inline friend bool operator== (const p& self, const S& x) {
        return seqs::equiv(seq(self), x);
      }

      template<typename S>
      inline friend bool operator== (const p& self, const std::shared_ptr<S>& x) {
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

      inline void extend_root(const p& v, const value_type& val) {

        auto new_leaf = nu<leaf>(v->_tail);
        _tail = nu<leaf>(val);

        bool overflow = ((v->_cnt >> 5) > (1 << v->_shift));
        if (overflow) {
          _shift = v->_shift + 5;
          _root  = nu<node>(v->_root, node::new_path(v->_shift, new_leaf));
        }
        else {
          _root = node::push_tail(v->_cnt, _shift, v->_root, new_leaf);
        }
      }

      static inline p assoc(const p& v, uint64_t idx, const value_type& val) {
        if (0 <= idx && idx < v->_cnt) {

          auto ret = imu::nu<basic_vector>(v);

          if (v->tail_off() <= idx) {
            ret->_tail->_arr[(idx & 0x01f)] = val;
          }
          else {
            auto root = leaf::assoc(ret->_root, ret->_shift, idx, val);
            ret->_root = std::static_pointer_cast<node>(root);
          }
          return ret;
        }
        else {
          return nu<basic_vector>(v, val);
        }
      }
    };

    typedef basic_vector<> vector;

    template<typename V = vector, typename mixin = no_mixin>
    struct basic_chunked_seq : public no_mixin {

      typedef std::shared_ptr<basic_chunked_seq> p;

      typedef typename V::value_type value_type;
      typedef typename V::leaf_type  leaf_type;

      mixin _mixin;

      typename V::p _vec;
      uint64_t      _idx;
      uint64_t      _off;

      typename leaf_type::p _leaf;

      inline basic_chunked_seq(const typename V::p& v, uint64_t i, uint64_t o)
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
        if ((_off + 1) < _leaf->_arr.size()) {
          return nu<basic_chunked_seq>(_vec, _idx, _off + 1);
        }
        return p();
      }

      template<typename S>
      inline friend bool operator== (const p& self, const S& x) {
        return seqs::equiv(self, x);
      }

      template<typename S>
      inline friend bool operator== (const p& self, const std::shared_ptr<S>& x) {
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

  template<typename T>
  inline auto vector(const T& coll)
    -> decltype(std::begin(coll), std::end(coll), ty::list::p()) {
    return ty::vector::from_std(coll);
  }

  template<typename T>
  inline ty::vector::p vector(std::initializer_list<T> l) {
    return ty::vector::from_std(l);
  }

  // @cond HIDE
  template<typename... TS>
  inline decltype(auto) seq(
    const std::shared_ptr<ty::basic_vector<TS...>>& v) {

    typedef typename ty::basic_vector<TS...> V;
    typedef typename ty::basic_chunked_seq<> S;

    if (!v || v->is_empty()) {
      return typename S::p();
    }
    return nu<S>(v, 0, 0);
  }

  template<typename... TS>
  inline decltype(auto) seq(ty::basic_vector<TS...>* const & v) {

    typedef typename ty::basic_vector<TS...>  V;
    typedef typename ty::basic_chunked_seq<V> S;

    if (!v || v->is_empty()) {
      return typename S::p();
    }
    return nu<S>(v, 0, 0);
  }

  template<typename V, typename T>
  inline typename std::enable_if<
    std::is_base_of<
      ty::vector_tag
      , typename semantics::real_type<V>::type
      >::value,
    typename semantics::real_type<V>::type::p
    >::type
  conj(const V& v, const T& x) {
    typedef typename semantics::real_type<V>::type type;
    return nu<type>(v, x);
  }
  // @endcond
}
