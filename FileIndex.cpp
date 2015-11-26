/// \file
/// Definition of the FileIndex class.
/// <p>
/// Copyright (c) 2009 Ross Tyler.
/// This file may be copied under the terms of the
/// GNU Lesser General Public License (LGPL).
/// See COPYING file for details.

#include "FileIndex.h"

FileIndex::FileIndex(struct stat const & st) throw()
: fileSystem(st.st_dev), inode(st.st_ino), time(st.st_mtime) {}

bool FileIndex::operator < (FileIndex const & that) const throw() {
	return fileSystem < that.fileSystem
	    ? true
	    : fileSystem > that.fileSystem
		? false
		: inode < that.inode
		    ? true
		    : inode > that.inode
			? false
			: time < that.time
			    ? true
			    : false;

}

typedef char dot;

std::istream & operator >>(std::istream & s, dot & d) throw() {
    if ('.' == s.get()) {
	d = '.';
    } else {
	s.setstate(std::ios::failbit);
    }
    return s;
}

std::ostream & operator <<(std::ostream & s, FileIndex & fileIndex) throw() {
    s		<< fileIndex.fileSystem
	<< '.'	<< fileIndex.inode
	<< '.'	<< fileIndex.time;
    return s;
}

std::istream & operator >>(std::istream & s, FileIndex & fileIndex) throw() {
    dot d;
    s		>> fileIndex.fileSystem
	>> d	>> fileIndex.inode
	>> d	>> fileIndex.time;
    return s;
}
