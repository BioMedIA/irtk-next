/* The Image Registration Toolkit (IRTK)
 *
 * Copyright 2008-2015 Imperial College London
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License. */

#ifndef _IRTKPARALLEL_H
#define _IRTKPARALLEL_H

#include <iostream>


// =============================================================================
// Global parallelization options
// =============================================================================

/// Enable/disable GPU acceleration
extern bool use_gpu;

/// Debugging level of GPU code
extern int debug_gpu;

/// Debugging level of TBB code
extern int tbb_debug;

// =============================================================================
// Command help
// =============================================================================

/// Check if given option is a parallelization option
bool IsParallelOption(const char *);

/// Parse parallelization option
void ParseParallelOption(int &, int &, char *[]);

/// Print parallelization command-line options
void PrintParallelOptions(std::ostream &);

// =============================================================================
// Multi-threading support using Intel's TBB
// =============================================================================

// If TBB is available and BUILD_TBB_EXE is set to ON, use TBB to execute
// any parallelizable code concurrently
//
// Attention: DO NOT define TBB_DEPRECATED by default or before including the
//            other TBB header files, in particular parallel_for. The deprecated
//            behavior of parallel_for is to not choose the chunk size (grainsize)
//            automatically!
//
// http://software.intel.com/sites/products/documentation/doclib/tbb_sa/help/tbb_userguide/Automatic_Chunking.htm
#ifdef HAS_TBB

#include <tbb/task_scheduler_init.h>
#include <tbb/blocked_range.h>
#include <tbb/blocked_range2d.h>
#include <tbb/blocked_range3d.h>
#include <tbb/parallel_for.h>
#include <tbb/parallel_reduce.h>
#include <tbb/concurrent_queue.h>
#include <tbb/mutex.h>

using namespace tbb;

// A task scheduler is created/terminated automatically by TBB since
// version 2.2. It is recommended by Intel not to instantiate any task
// scheduler manually. However, in order to support the -threads option
// which can be used to limit the number of threads, a global task scheduler
// instance is created and the -threads argument passed on to its initialize
// method by ParseParallelOption. There should be no task scheduler created/
// terminated in any of the IRTK libraries functions and classes.
extern std::auto_ptr<task_scheduler_init> tbb_scheduler;

// Otherwise, use dummy implementations of TBB classes/functions which allows
// developers to write parallelizable code as if TBB was available and yet
// executes the code serially due to the lack of TBB (or BUILD_TBB_EXE set to OFF).
// This avoids code duplication and unnecessary conditional code compilation.
#else // HAS_TBB

  /// Dummy type used to distinguish split constructor from copy constructor
  struct split {};

  /// Helper for initialization of task scheduler
  class task_scheduler_init
  {
  public:
    task_scheduler_init(int) {}
    void terminate() {}
  };

  /// One-dimensional range
  template <typename T>
  class blocked_range
  {
    int _lbound;
    int _ubound;
  public:
    blocked_range(int l, int u, int = 1) : _lbound(l), _ubound(u) {}
    int begin() const { return _lbound; }
    int end()   const { return _ubound; }
  };

  /// Two-dimensional range
  template <typename T>
  class blocked_range2d
  {
    blocked_range<int> _rows;
    blocked_range<int> _cols;

  public:

    blocked_range2d(int rl, int ru,
                    int cl, int cu)
    :
      _rows (rl, ru),
      _cols (cl, cu)
    {
    }

    blocked_range2d(int rl, int ru, int,
                    int cl, int cu, int)
    :
      _rows (rl, ru),
      _cols (cl, cu)
    {
    }

    const blocked_range<int> &rows() const { return _rows; }
    const blocked_range<int> &cols() const { return _cols; }
  };

  /// Three-dimensional range
  template <typename T>
  class blocked_range3d
  {
    blocked_range<int> _pages;
    blocked_range<int> _rows;
    blocked_range<int> _cols;

  public:

    blocked_range3d(int pl, int pu,
                    int rl, int ru,
                    int cl, int cu)
    :
      _pages(pl, pu),
      _rows (rl, ru),
      _cols (cl, cu)
    {
    }

    blocked_range3d(int pl, int pu, int,
                    int rl, int ru, int,
                    int cl, int cu, int)
    :
      _pages(pl, pu),
      _rows (rl, ru),
      _cols (cl, cu)
    {
    }

    const blocked_range<int> &pages() const { return _pages; }
    const blocked_range<int> &rows() const { return _rows; }
    const blocked_range<int> &cols() const { return _cols; }
  };

  /// parallel_for dummy template function which executes the body serially
  template <class Range, class Body>
  void parallel_for(const Range &range, const Body &body) {
    body(range);
  }

  /// parallel_reduce dummy template function which executes the body serially
  template <class Range, class Body>
  void parallel_reduce(const Range &range, Body &body) {
    body(range);
  }

#endif // HAS_TBB


#endif
