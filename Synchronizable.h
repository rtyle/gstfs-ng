/// \file
/// Declaration/definition of the Synchronizable and
/// Synchronizable::Syncrhronized classes.
/// <p>
/// Copyright (c) 2009 Ross Tyler.
/// This file may be copied under the terms of the
/// GNU Lesser General Public License (LGPL).
/// See COPYING file for details.

#ifndef Synchronizable_h_
#define Synchronizable_h_

#include <boost/thread/recursive_mutex.hpp>
#include <boost/thread/condition.hpp>

template <typename Mutex = boost::recursive_mutex> class Synchronizable {
private:
    Mutex mutex;
    boost::condition condition;
public:
    class Synchronized : public Mutex::scoped_lock {
    private:
	Synchronizable & synchronizable;
    public:
	Synchronized(Synchronizable & synchronizable_) throw()
	    : Mutex::scoped_lock(synchronizable_.mutex),
		synchronizable(synchronizable_)
	    {}
	~Synchronized() throw() {}
	void wait() throw() {synchronizable.condition.wait(*this);}
	void notify() throw() {synchronizable.condition.notify_one();}
	void notifyAll() throw() {synchronizable.condition.notify_all();}
    };
    /**
    \class Synchronized
    An instance of this class should be constructed on a Synchronizable
    in order to mimic the behavior of the synchronized modifier
    on a block of java code.

    For example, the following blocks of code are equivalent.

    Java:
    \code
	Object object;
	synchronized(object) {
	    // ...
	    object.notify();
	    object.wait();
	    // ...
	}
    \endcode

    C++:
    \code
	Synchronizable<> synchronizable;
	{
	    Synchronizable<>::Synchronized synchronized(synchronizable);
	    // ...
	    synchronized.notify();
	    synchronized.wait();
	    // ...
	}
    \endcode
    *******************************************************************/

    friend class Synchronized;
};
/// \class Synchronizable
/// This (or derivation of this) class may be used to mimic the
/// Synchronizable capabilities of java.lang.Object's.
///
/// The type of boost Mutex to be used is specified as a template argument.
/// The default type (boost::recursive_mutex) mimics Java's reentrant behavior.
///
/// While available for public construction, an instance of this class
/// is only accessible from the scope of a friendly Synchronized instance.
//**********************************************************************

#endif
