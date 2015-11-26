/// \file
/// Declaration of the ImageReader class.
/// <p>
/// Copyright (c) 2009 Ross Tyler.
/// This file may be copied under the terms of the
/// GNU Lesser General Public License (LGPL).
/// See COPYING file for details.

#ifndef ImageReader_h
#define ImageReader_h

#include "Image.h"
#include "Reader.h"

/// An ImageReader provides the #read method for
/// accessing the image it was constructed with.
class ImageReader : public Reader{
private:
    ImageConstPointer imageConstPointer;	///< Image to read
public:
    ImageReader(FileIndex, ImageConstPointer) throw();

    virtual ssize_t read(char * buffer, size_t size, off_t offset) throw();

    virtual size_t size(bool wait) throw();
};

#endif
