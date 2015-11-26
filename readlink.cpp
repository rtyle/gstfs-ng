/// \file
/// \brief Definition of readlink(2) wrappers
///
/// Copyright (c) 2009 Ross Tyler.
/// This file may be copied under the terms of the
/// GNU Lesser General Public License (LGPL).
/// See COPYING file for details.

#include <cerrno>
#include <sstream>

#include "Exception.h"

#include "readlink.h"

template <typename T> static T forget(T & that) {
    T remember = that;
    that = 0;
    return remember;
}

boost::shared_ptr<char const> readlink(char const * link)
	throw (Exception::Error) {
    // the trouble with the system-supplied readlink(2) is that one cannot
    // know how large the result is going to be.
    // start with a reasonable size then try successively larger ones,
    // by factors of 2,until one is large enough
    // and then return a trimmed result.
    size_t size(1024);
    char * buffer(0);
    try {
	for (;;) {
	    buffer = static_cast<char *>(Exception::throwErrorIfEqual(
		static_cast<void *>(0),
		realloc(forget(buffer), size)));
	    size_t const length(Exception::throwErrorIfEqual(
		static_cast<ssize_t>(-1),
		readlink(link, buffer, size)));
	    if (length < size) {
		buffer[length] = 0;
		char * trim = Exception::throwErrorIfEqual(
		    static_cast<char *>(0),
		    strdup(buffer));
		free(buffer);
		return boost::shared_ptr<char const>(trim, free);
	    }
	    size *= 2;
	}
    } catch (...) {
	if (buffer) free(buffer);
	throw;
    }
}

boost::shared_ptr<char const> readlink(int fd) throw (Exception::Error) {
    // links for file descriptors are found in procfs
    std::ostringstream out;
    out << "/proc/" << getpid() << "/fd/" << fd;
    return readlink(out.str().c_str());
}
