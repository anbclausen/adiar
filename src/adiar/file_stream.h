#ifndef ADIAR_FILE_STREAM_H
#define ADIAR_FILE_STREAM_H

#include <tpie/tpie.h>
#include <tpie/sort.h>

#include <adiar/data.h>
#include <adiar/file.h>

#include <adiar/internal/assert.h>
#include <adiar/internal/decision_diagram.h>
#include <adiar/internal/memory.h>

namespace adiar
{
  //////////////////////////////////////////////////////////////////////////////
  /// \brief Stream to a file with a one-way reading direction.
  ///
  /// \param T           The type of the file's elements
  ///
  /// \param REVERSE     Whether the reading direction should be reversed
  ///
  /// \param SharedPtr_T The type of the shared pointer to a file
  //////////////////////////////////////////////////////////////////////////////
  template <typename T, bool REVERSE = false, typename SharedPtr_T = file<T>>
  class file_stream
  {
  public:
    static constexpr size_t memory_usage()
    {
      return __tpie_file_stream_memory_usage<T>();
    }

  private:
    bool _has_peeked = false;
    T _peeked;
    bool _negate = false;

    typename tpie::file_stream<T> _stream;

    ////////////////////////////////////////////////////////////////////////////
    /// The file stream includes a shared pointer to hook into the reference
    /// counting and garbage collection of the file.
    ////////////////////////////////////////////////////////////////////////////
    std::shared_ptr<SharedPtr_T> _file_ptr;

  protected:
    file_stream() { }

    file_stream(file<T> &f,
                const std::shared_ptr<SharedPtr_T> &shared_ptr,
                bool negate = false)
    {
      attach(f, shared_ptr, negate);
    }

    void attach(file<T> &f,
                const std::shared_ptr<SharedPtr_T> &shared_ptr,
                bool negate = false)
    {
      if (attached()) { detach(); }

      _file_ptr = shared_ptr;
      _file_ptr -> make_read_only();

      _stream.open(f._tpie_file, ADIAR_READ_ACCESS);
      _negate = negate;

      reset();
    }

  public:
    file_stream(const std::shared_ptr<file<T>> &f, bool negate = false)
    {
      attach(*f, f, negate);
    }

    file_stream(const simple_file<T> &f, bool negate = false) : file_stream(f._file_ptr, negate) { }

    ~file_stream()
    {
      detach();
    }

    ////////////////////////////////////////////////////////////////////////////
    /// \brief Reset the read head back to the beginning (relatively to the
    /// reading direction).
    ////////////////////////////////////////////////////////////////////////////
    void reset()
    {
      if constexpr (REVERSE) {
        _stream.seek(0, tpie::file_stream_base::end);
      } else {
        _stream.seek(0);
      }
    }

    ////////////////////////////////////////////////////////////////////////////
    /// \brief Whether the stream contains more elements.
    ////////////////////////////////////////////////////////////////////////////
    bool can_pull()
    {
      return _has_peeked
        || (REVERSE ? _stream.can_read_back() : _stream.can_read());
    }

    ////////////////////////////////////////////////////////////////////////////
    /// \brief Obtain the next element (and move the read head)
    ////////////////////////////////////////////////////////////////////////////
    const T pull()
    {
      if (_has_peeked) {
        _has_peeked = false;
        return _peeked;
      }
      T t = REVERSE ? _stream.read_back() : _stream.read();
      return _negate ? !t : t;
    }

    ////////////////////////////////////////////////////////////////////////////
    /// \brief Obtain the next element (but do not move the read head)
    ////////////////////////////////////////////////////////////////////////////
    const T peek()
    {
      if (!_has_peeked) {
        _peeked = pull();
        _has_peeked = true;
      }
      return _peeked;
    }

    ////////////////////////////////////////////////////////////////////////////
    /// \brief Attach to a file
    ////////////////////////////////////////////////////////////////////////////
    void attach(const simple_file<T> &f, bool negate = false)
    {
      attach(f, f._file_ptr, negate);
    }

    ////////////////////////////////////////////////////////////////////////////
    /// \brief Whether the reader is currently attached.
    ////////////////////////////////////////////////////////////////////////////
    bool attached()
    {
      return _stream.is_open();
    }

    ////////////////////////////////////////////////////////////////////////////
    /// \brief Detach from the file, i.e. close the stream.
    ////////////////////////////////////////////////////////////////////////////
    void detach()
    {
      _stream.close();
      // if (_file_ptr) { _file_ptr.reset(); }
    }
  };

  //////////////////////////////////////////////////////////////////////////////
  /// \brief File streams for assignments (label, value).
  ///
  /// \param REVERSE Whether the reading direction should be reversed
  //////////////////////////////////////////////////////////////////////////////
  template<bool REVERSE = false>
  using assignment_stream = file_stream<assignment_t, REVERSE>;

  //////////////////////////////////////////////////////////////////////////////
  /// \brief File streams for variable labels.
  ///
  /// \param REVERSE Whether the reading direction should be reversed
  //////////////////////////////////////////////////////////////////////////////
  template<bool REVERSE = false>
  using label_stream = file_stream<label_t, REVERSE>;

  //////////////////////////////////////////////////////////////////////////////
  /// \brief         File stream of files with meta information.
  ///
  /// \param T       The type of the file(s)'s elements
  ///
  /// \param File    Index for the file to read from
  ///
  /// \param REVERSE Whether the reading direction should be reversed
  ///
  /// \remark Since all files are written to in reverse of the desired reading
  ///         order, then 'reversing' the reversed input is equivalent to not
  ///         reversing the underlying stream. Hence, we do hide a a negation of
  ///         the \c REVERSE parameter.
  //////////////////////////////////////////////////////////////////////////////
  template <typename T, size_t File, bool REVERSE = false>
  class meta_file_stream : public file_stream<T, REVERSE, __meta_file<T>>
  {
    static_assert(File < FILE_CONSTANTS<T>::files, "The file to pick must be a valid index");

  public:
    meta_file_stream(const meta_file<T> &file, bool negate = false)
      : file_stream<T, REVERSE, __meta_file<T>>(file._file_ptr -> _files[File], file._file_ptr, negate)
    { }

    // TODO: 'attach', 'attached', and 'detach'
  };

  //////////////////////////////////////////////////////////////////////////////
  /// \brief         Stream of nodes from a node file.
  ///
  /// \param REVERSE Whether the reading direction should be reversed
  ///                (relatively to the ordering of nodes within the file).
  ///
  /// \sa node_file
  //////////////////////////////////////////////////////////////////////////////
  template<bool REVERSE = false>
  class node_stream : public meta_file_stream<node_t, 0, !REVERSE>
  {
  public:
    node_stream(const node_file &file, bool negate = false)
      : meta_file_stream<node_t, 0, !REVERSE>(file, negate)
    { }

    node_stream(const decision_diagram &dd)
      : meta_file_stream<node_t, 0, !REVERSE>(dd.file, dd.negate)
    { }
  };

  //////////////////////////////////////////////////////////////////////////////
  /// \brief Stream for internal (reversed) arcs of an arc file.
  ///
  /// \sa arc_file
  //////////////////////////////////////////////////////////////////////////////
  template<bool REVERSE = false>
  using node_arc_stream = meta_file_stream<arc_t, 0, !REVERSE>;

  // TODO: Move inside of sink_arc_stream below ?
  template<bool REVERSE = false>
  using in_order_arc_stream = meta_file_stream<arc_t, 1, !REVERSE>;

  // TODO: Move inside of sink_arc_stream below ?
  template<bool REVERSE = false>
  using out_of_order_arc_stream = meta_file_stream<arc_t, 2, !REVERSE>;

  //////////////////////////////////////////////////////////////////////////////
  /// \brief Stream for sink arcs of an arc file.
  ///
  /// \sa arc_file
  //////////////////////////////////////////////////////////////////////////////
  template<bool REVERSE = false>
  class sink_arc_stream
    : private in_order_arc_stream<REVERSE>, private out_of_order_arc_stream<REVERSE>
  {
  public:
    static constexpr size_t memory_usage()
    {
      return in_order_arc_stream<REVERSE>::memory_usage()
        + out_of_order_arc_stream<REVERSE>::memory_usage();
    }

  public:
    sink_arc_stream(const arc_file &file, bool negate = false)
      : in_order_arc_stream<REVERSE>(file, negate),
        out_of_order_arc_stream<REVERSE>(file, negate)
    { }

  private:
    bool pull_in_order()
    {
      bool in_order_pull = in_order_arc_stream<REVERSE>::can_pull();
      bool out_of_order_pull = out_of_order_arc_stream<REVERSE>::can_pull();

      if (in_order_pull != out_of_order_pull) {
        return in_order_pull;
      }

      ptr_t in_order_source = in_order_arc_stream<REVERSE>::peek().source;
      ptr_t out_of_order_source = out_of_order_arc_stream<REVERSE>::peek().source;

      return (REVERSE && in_order_source < out_of_order_source)
        || (!REVERSE && in_order_source > out_of_order_source);
    }

  public:
    ////////////////////////////////////////////////////////////////////////////
    /// \brief Reset the read head back to the beginning (relatively to the
    /// reading direction).
    ////////////////////////////////////////////////////////////////////////////
    void reset()
    {
      in_order_arc_stream<REVERSE>::reset();
      out_of_order_arc_stream<REVERSE>::reset();
    }

    ////////////////////////////////////////////////////////////////////////////
    /// \brief Whether the stream contains more arcs.
    ////////////////////////////////////////////////////////////////////////////
    bool can_pull()
    {
      return in_order_arc_stream<REVERSE>::can_pull()
        || out_of_order_arc_stream<REVERSE>::can_pull();
    }

    ////////////////////////////////////////////////////////////////////////////
    /// \brief Obtain the next arc (and move the read head)
    ////////////////////////////////////////////////////////////////////////////
    const arc_t pull()
    {
      return pull_in_order()
        ? in_order_arc_stream<REVERSE>::pull()
        : out_of_order_arc_stream<REVERSE>::pull();
    }

    ////////////////////////////////////////////////////////////////////////////
    /// \brief Obtain the next arc (but do not move the read head)
    ////////////////////////////////////////////////////////////////////////////
    const arc_t peek()
    {
      return pull_in_order()
        ? in_order_arc_stream<REVERSE>::peek()
        : out_of_order_arc_stream<REVERSE>::peek();
    }
  };

  //////////////////////////////////////////////////////////////////////////////
  /// \brief Stream for the levelized meta information.
  //////////////////////////////////////////////////////////////////////////////
  template <typename T, bool REVERSE = false>
  class level_info_stream : public file_stream<level_info_t, !REVERSE, __meta_file<T>>
  {
  public:
    //////////////////////////////////////////////////////////////////////////////
    /// Access the level information of a file with meta information.
    //////////////////////////////////////////////////////////////////////////////
    level_info_stream(const meta_file<T> &f)
      : file_stream<level_info_t, !REVERSE, __meta_file<T>>(f._file_ptr -> _level_info_file, f._file_ptr)
    { }

    //////////////////////////////////////////////////////////////////////////////
    /// Access the level information stream of a decision diagram.
    //////////////////////////////////////////////////////////////////////////////
    level_info_stream(const decision_diagram &dd)
      : level_info_stream(dd.file)
    { }

  private:
    //////////////////////////////////////////////////////////////////////////////
    /// For unit testing only!
    //////////////////////////////////////////////////////////////////////////////
    meta_file<T> __obtain_file(const __decision_diagram &dd)
    {
      if constexpr (std::is_same<node_t, T>::value) {
        return dd.get<node_file>();
      } else {
        return dd.get<arc_file>();
      }
    }

  public:
    //////////////////////////////////////////////////////////////////////////////
    /// For unit testing only!
    ///
    /// Access to level information of an unreduced decision diagram.
    //////////////////////////////////////////////////////////////////////////////
    level_info_stream(const __decision_diagram &dd)
      : level_info_stream(__obtain_file(dd))
    { }

    // TODO: 'attach', 'attached', and 'detach'
  };
}

#endif // ADIAR_FILE_STREAM_H
