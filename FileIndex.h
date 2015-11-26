/// \file
/// Declaration of the FileIndex class.
/// <p>
/// Copyright (c) 2009 Ross Tyler.
/// This file may be copied under the terms of the
/// GNU Lesser General Public License (LGPL).
/// See COPYING file for details.

#ifndef FileIndex_h
#define FileIndex_h

#include <iostream>

#include <sys/stat.h>

/// A FileIndex may be used by containers to index information associated
/// with a file by its fileSystem, inode and modification time.
/// This indexing information is constructed directly from a stat(2) struct.
/// This indexing information is much better than a simple path name
/// for identifying file content.
class FileIndex {
public:
    dev_t		fileSystem;	///< from stat st_dev
    ino_t		inode;		///< from stat st_ino
    time_t		time;		///< from stat st_mtime
    FileIndex() throw() : fileSystem(0), inode(0), time(0) {}
    FileIndex(struct stat const & st) throw();
    bool operator < (FileIndex const &) const throw();
};

std::ostream & operator <<(std::ostream &, FileIndex & fileIndex) throw();
std::istream & operator >>(std::istream &, FileIndex & fileIndex) throw();

#endif
