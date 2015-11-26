/// \file
/// \brief Declaration of classes in the Exception namespace
///
/// Copyright (c) 2009 Ross Tyler.
/// This file may be copied under the terms of the
/// GNU Lesser General Public License (LGPL).
/// See COPYING file for details.

#ifndef Exception_h
#define Exception_h

#include <cerrno>
#include <cstring>
#include <stdexcept>

/// Namespace for names dealing with common exceptions
namespace Exception {

    //******************************************************************
    class Error : public std::runtime_error {

	typedef std::runtime_error Inherited;	///< We are a runtime_error

    public:

	Error(
	    int	error_ = errno)	///< OS error number (errno)
	throw()
	/// Construct an Error using global errno and its associated string.
	:
	    Inherited(strerror(error_)),
	    error(error_)
	{}

	Error(
	    std::string const &	message,	///< Explanatory message
	    int			error_ = errno)	///< OS error number (errno)
	throw()
	/// \brief Construct an Error using message and error provided.
	/// The message is appended to the string associated with error.
	:
	    Inherited(std::string(strerror(error_)) + message),
	    error(error_)
	{}

	int const error;	///< OS error number (errno)
    };
    /// \class Error
    /// \brief Base class for all exceptions thrown as the result of a system
    /// error (errno).
    ///
    /// Our Inherited class keeps a copy of the message string
    /// and we keep a copy of the error.
    //******************************************************************

    #define STRINGIFY_(x) #x
    #define TOSTRING(x) STRINGIFY_(x)

    //******************************************************************
    #define here(operation)\
	there(__FILE__, __func__, TOSTRING(__LINE__), operation)
    /// \def here
    /// \return The constructed string.
    ///
    /// \brief The here macro constructs a string that reflects the location
    /// of its expansion using standard cpp macros __FILE__ and __LINE__
    /// along with the C99 compiler's expansion of __func__
    /// and the operation argument.
    /// The here macro relies on the #there function for formatting the
    /// the location there.

    static inline std::string there(
	char const *	file,		///< __FILE__
	char const *	function,	///< __func__
	char const *	line,		///< __LINE__
	char const *	operation)	///< operation
    throw()
    /// \brief Return a formatted string that reflects the location specified
    /// by its arguments (there).
    /// \return A formattted string for the location there
    //******************************************************************
    {
	return std::string(" @")
	    + file
	    + ';'
	    + function
	    + ';'
	    + line
	    + ';'
	    + operation;
    }

    //********************************************************************
    #define throwErrorIfEqual(value, result) throwErrorIfEqual_(\
	value, result, __FILE__, __func__, TOSTRING(__LINE__), STRINGIFY_(value == result))
    /**
    \def throwErrorIfEqual(value, result)
    \param value	Unexpected value
    \param result	Result to test against value and, if not, return
    \return The result if not value; otherwise throw Error
    \throw Error If result is value

    \brief Throws an Error if result is value;
    otherwise, the result is returned as the function value.
    */

    template<class T> static inline T throwErrorIfEqual_(
	T		value,		///< Unexpected value
	T               result,         ///< Function result if no error
	char const *    file,           ///< __FILE__
	char const *    function,       ///< __func__
	char const *    line,           ///< __LINE__
	char const *    operation)      ///< operation
    throw(Error)
    /// \brief Throw an Error (constructed with Error() and
    /// a formatted concatenation of file, function, line and operation)
    /// if result is value;
    /// otherwise, result is returned as the function value.
    /// \return result if not value; otherwise throw Error
    /// \throw Error if result is value
    //********************************************************************
    {
	if (value == result)
	    throw Error(there(file, function, line, operation));
	return result;
    }

    //********************************************************************
    #define throwErrorIfNotEqual(value, result) throwErrorIfNotEqual_(\
	value, result, __FILE__, __func__, TOSTRING(__LINE__), STRINGIFY_(value != result))
    /**
    \def throwErrorIfNotEqual(value, result)
    \param value	Expected value
    \param result	Result to test against value and, if equal, return
    \return The result if value; otherwise throw Error
    \throw Error If result is not value

    \brief Throws an Error if result is not value;
    otherwise, the result is returned as the function value.
    */

    template<class T> static inline T throwErrorIfNotEqual_(
	T		value,		///< Expected value
	T               result,         ///< Function result if no error
	char const *    file,           ///< __FILE__
	char const *    function,       ///< __func__
	char const *    line,           ///< __LINE__
	char const *    operation)      ///< operation
    throw(Error)
    /// \brief Throw an Error (constructed with Error() and
    /// a formatted concatenation of file, function, line and operation)
    /// if result is not value;
    /// otherwise, result is returned as the function value.
    /// \return result if value; otherwise throw Error
    /// \throw Error if result is not value
    //********************************************************************
    {
	if (value != result)
	    throw Error(there(file, function, line, operation));
	return result;
    }

    //******************************************************************
    #define throwErrorIfNegative1(result) throwErrorIfEqual(-1, result)
    /**
    \def throwErrorIfNegative1(result)
    \param result	result to test against -1 and, if not equal, return
    \return The result if not -1; otherwise throw Exception::Error
    \throw Error If result is -1

    \brief Throws an Exception::Error if result is -1;
    otherwise, result is returned as the function value.

    Consider main.cc:
    \code
	#include <iostream>

	#include <unistd.h>

	#include "Exception.h"

	int main(int argc, char ** argv, char ** envv) {
	    try {
		char buffer[64];
		ssize_t count = Exception::throwErrorIfNegative1(
		    read(-1, buffer, sizeof buffer));
	    }
	    catch (std::exception & e) {
		std::cerr << e.what() << std::endl;
	    }
	    return 0;
	}
    \endcode
    When run, this example will print:
    \code
	Bad file descriptor @main.cc:main:11:read(-1, buffer, sizeof(buffer)
    \endcode
    */

}

#endif
