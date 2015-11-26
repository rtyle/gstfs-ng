/// \file
/// Declaration of miscellaneous utilities.
/// <p>
/// Copyright (c) 2009 Ross Tyler.
/// This file may be copied under the terms of the
/// GNU Lesser General Public License (LGPL).
/// See COPYING file for details.

#ifndef Utility_h
#define Utility_h

#include <cstdlib>

namespace Utility {

    /// Compare a to b..., considering only the length of b
    /// and return the length of the first match.
    /// \return the length of the first match
    size_t match(char const * a, char const * b, ...) throw();

    /// For use as a custom deleter for a boost::shared_ptr
    /// when we want nothing to happen when the last shared copy
    /// is destroyed
    struct NoDeleter {
	void operator()(void const *) const {}
    };

    /// Abstraction of a less than operation
    /// for use as a Compare template argument for some STL containers
    /// to compare objects of the type suggested by container's
    /// Data template argument.
    template<typename T> struct LessThan {
        bool operator()(T a, T b) const throw();
    };

    /// Template for comparing what is referenced by pointers
    /// rather than the pointers themselves.
    template<typename T> struct LessThanReferenced {
        bool operator ()(T const * a, T const * b) const throw() {
    	return *a < *b;
        }
    };

}

#endif
