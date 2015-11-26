/// \file
/// Definition of the FileReader class.
/// <p>
/// Copyright (c) 2009 Ross Tyler.
/// This file may be copied under the terms of the
/// GNU Lesser General Public License (LGPL).
/// See COPYING file for details.

#include <cstring>

#include <errno.h>
#include <stdlib.h>
#include <sys/stat.h>

#include "FileReader.h"

FileReader::FileReader(FileIndex fileIndex_, int fd_) throw()
:
    Reader(fileIndex_),
    fd(fd_)
{}

/*virtual*/ FileReader::~FileReader() throw() {
    close(fd);
}

/*virtual*/ ssize_t FileReader::read(
	char * buffer, size_t size, off_t offset) throw() {
    ssize_t result = pread(fd, buffer, size, offset);
    return -1 == result ? -errno : result;
}

/*virtual*/ size_t FileReader::size(bool wait) throw() {
    struct stat st;
    if (-1 == fstat(fd, &st)) return 0;
    return st.st_size;
}
