/// \file
/// Declaration of miscellaneous utilities.
/// <p>
/// Copyright (c) 2009 Ross Tyler.
/// This file may be copied under the terms of the
/// GNU Lesser General Public License (LGPL).
/// See COPYING file for details.

#include <cstdarg>
#include <cstring>

#include "Utility.h"

namespace Utility {

    size_t match(char const * a, char const * b, ...) throw() {
	va_list ap;
	va_start(ap, b);
	while (b) {
	    size_t l = strlen(b);
	    if (0 == strncmp(a, b, l)) {
		va_end(ap);
		return l;
	    }
	    b = va_arg(ap, char const *);
	}
	va_end(ap);
	return 0;
    }

    /// LessThan<T>::operator(T a, T b) specialization
    /// for the char const * type T.
    /// Unless this is used for the Compare template argument of an STL
    /// container with Data of type char const *, the objects in the container
    /// will be compared by address - not content.
    template<> bool LessThan<char const *>::operator()(
	char const * a, char const * b) const throw() {
	return strcmp(a, b) < 0;
    }

}
