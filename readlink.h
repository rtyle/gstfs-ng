/// \file
/// \brief Declaration of readlink(2) wrappers
///
/// Copyright (c) 2009 Ross Tyler.
/// This file may be copied under the terms of the
/// GNU Lesser General Public License (LGPL).
/// See COPYING file for details.

#ifndef readlink_h
#define readlink_h

#include <boost/shared_ptr.hpp>

#include "Exception.h"

boost::shared_ptr<char const> readlink(
    char const *	link)	///< The link to read
throw (Exception::Error)
/// \return The value of the read link
/// \throw Exception::Error If the link cannot be read
/// \brief Like readlink(2) except the result is returned in a smart pointer
/// or an exception is thrown.
;

boost::shared_ptr<char const> readlink(
    int			fd)	///< The file descriptor whose path to return
throw (Exception::Error)
/// \return The path to the object referenced by the file descriptor
/// \throw Exception::Error If the path cannot be determined
/// \brief Return the path to the object referenced by the file descriptor
/// or throw an exception.
;

#endif
