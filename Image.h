/// \file
/// Declarations of the Image type and relations.
/// <p>
/// Copyright (c) 2009 Ross Tyler.
/// This file may be copied under the terms of the
/// GNU Lesser General Public License (LGPL).
/// See COPYING file for details.

#ifndef Image_h
#define Image_h

#include <ext/rope>
#include <boost/shared_ptr.hpp>

/// An Image is an STL rope
/// which is a heavyweight string involving the use of a concatenation tree
/// representation.
/// This is perfect for representing images that are built as a sequence
/// of many pieces.
/// For this purpose, a rope performs _much_ better than a std::string.
typedef __gnu_cxx::crope Image;

typedef Image const ImageConst;

typedef boost::shared_ptr<ImageConst> ImageConstPointer;

#endif
