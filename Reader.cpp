/// \file
/// Definition of the Reader class.
/// <p>
/// Copyright (c) 2009 Ross Tyler.
/// This file may be copied under the terms of the
/// GNU Lesser General Public License (LGPL).
/// See COPYING file for details.

#include "Reader.h"

Reader::Reader(FileIndex fileIndex_) throw()
: fileIndex(fileIndex_), count(0) {}

Reader::~Reader() throw() {}

ImageConst * Reader::getImage() throw() {return 0;}

Reader::operator unsigned() throw() {return count;}

Reader & Reader::operator ++() throw() {++count; return *this;}

Reader & Reader::operator --() throw() {--count; return *this;}

bool Reader::operator < (Reader const & that) const throw() {
    return fileIndex < that.fileIndex;
}

