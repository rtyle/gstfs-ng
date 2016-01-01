/// \file
/// Declaration of the Cwd class.
/// <p>
/// Copyright (c) 2015 Ross Tyler.
/// This file may be copied under the terms of the
/// GNU Lesser General Public License (LGPL).
/// See COPYING file for details.

#ifndef Cwd_h
#define Cwd_h

#include <string>

#include "Synchronizable.h"

/// A Cwd object changes the current working directory on construction
/// and restores it on destruction.
class Cwd {
protected:
    char * path;
public:
    Cwd(char const * path) throw();
    ~Cwd() throw();
};

/// The CwdSynchronized class has one Cwd that is shared/synchronized
/// between any/all instances in the process so/as
/// there is only one notion of a "current directory" in the whole process.
class CwdSynchronized {
protected:
    static Synchronizable<boost::mutex> synchronizable;
    static Cwd *	cwd;
    static std::string	path;
    static unsigned	count;
public:
    CwdSynchronized(char const * path) throw();
    ~CwdSynchronized() throw();
};

#endif
