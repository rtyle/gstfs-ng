/// \file
/// Declaration of the abstract Reader class.
/// <p>
/// Copyright (c) 2009 Ross Tyler.
/// This file may be copied under the terms of the
/// GNU Lesser General Public License (LGPL).
/// See COPYING file for details.

#ifndef Reader_h
#define Reader_h

#include <unistd.h>

#include "FileIndex.h"
#include "Image.h"

/// A Reader implementation provides #read, #stat and #getImage methods.
/// This abstract base class maintains a use counter that can be incremented,
/// decremented and tested.
/// It also has members that make it friendly for management in containers.
class Reader {
public:
    FileIndex const	fileIndex;	///< Used for indexing containers

    Reader(FileIndex fileIndex) throw();
    virtual ~Reader() throw();

    /// Fill a buffer with the specified portion of the read target,
    /// waiting as necessary.
    virtual ssize_t read(char * buffer, size_t size, off_t offset) throw() = 0;

    /// Return the image size the reflects fileIndex,
    /// waiting on the target as necessary/requested.
    virtual size_t size(bool wait) throw() = 0;

    /// Return complete target image or 0 if none.
    virtual ImageConst * getImage() throw();

    operator unsigned() throw();
    Reader & operator ++() throw();
    Reader & operator --() throw();

    bool operator < (Reader const &) const throw();

private:
    unsigned	count;		///< Used for aging in containers
};

#endif
