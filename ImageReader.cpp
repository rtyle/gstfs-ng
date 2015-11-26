/// \file
/// Definition of the ImageReader class.
/// <p>
/// Copyright (c) 2009 Ross Tyler.
/// This file may be copied under the terms of the
/// GNU Lesser General Public License (LGPL).
/// See COPYING file for details.

#include <errno.h>

#include "ImageReader.h"

ImageReader::ImageReader(
    FileIndex fileIndex_, ImageConstPointer imageConstPointer_) throw()
:
    Reader(fileIndex_),
    imageConstPointer(imageConstPointer_)
{}

/*virtual*/ ssize_t ImageReader::read(
	char * buffer, size_t size, off_t offset_) throw() {
    if (0 > offset_) return -EINVAL;
    size_t offset = offset_;
    // answer the request the best we can
    if (offset >= imageConstPointer->size()) return 0;
    size_t available = imageConstPointer->size() - offset;
    size_t copy = size < available ? size : available;
    imageConstPointer->copy(offset, copy, buffer);
    return copy;
}

/*virtual*/ size_t ImageReader::size(bool wait) throw() {
    return imageConstPointer->size();
}
