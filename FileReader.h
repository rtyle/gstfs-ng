/// \file
/// Declaration of the FileReader class.
/// <p>
/// Copyright (c) 2009 Ross Tyler.
/// This file may be copied under the terms of the
/// GNU Lesser General Public License (LGPL).
/// See COPYING file for details.

#ifndef FileReader_h
#define FileReader_h

#include "Reader.h"

/// A FileReader provides the base file #read methods for
/// directly accessing the file descriptor it was constructed with.
class FileReader : public Reader{
protected:
    int	fd;	///< Our fd to file (will close on destruction)
public:
    FileReader(FileIndex, int fd) throw();

    virtual ~FileReader() throw();

    virtual ssize_t read(char * buffer, size_t size, off_t offset) throw();

    virtual size_t size(bool wait) throw();
};

#endif
