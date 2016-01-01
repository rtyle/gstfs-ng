/// \file
/// Definition of the Cwd class.
/// <p>
/// Copyright (c) 2015 Ross Tyler.
/// This file may be copied under the terms of the
/// GNU Lesser General Public License (LGPL).
/// See COPYING file for details.

#include <stdlib.h>
#include <unistd.h>

#include "Cwd.h"

Cwd::Cwd(char const * path_) throw()
:
    path(getcwd(0, 0))
{
    chdir(path_);
}

Cwd::~Cwd() throw() {
    chdir(path);
    free(path);
}

// CwdSynchronized class variables
/*static*/ Synchronizable<boost::mutex> CwdSynchronized::synchronizable;
/*static*/ Cwd *	CwdSynchronized::cwd	(0);
/*static*/ std::string	CwdSynchronized::path;
/*static*/ unsigned	CwdSynchronized::count	(0);

CwdSynchronized::CwdSynchronized(char const * path_) throw() {
    Synchronizable<boost::mutex>::Synchronized synchronized(synchronizable);
    // block until we can create a new cwd or we agree with the existing one
    while (cwd && 0 != path.compare( path_)) {
	synchronized.wait();
    }
    if (!cwd) {
	cwd = new Cwd(path_);
	path = path_;
    }
    ++count;
}

CwdSynchronized::~CwdSynchronized() throw() {
    Synchronizable<boost::mutex>::Synchronized synchronized(synchronizable);
    if (!--count) {
	delete cwd;
	cwd = 0;
	synchronized.notifyAll();
    }
}
