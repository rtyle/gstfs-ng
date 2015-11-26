/// \file
/// Declarations in the ImageCache namespace of the ImageCache::Container
/// and supporting classes.
/// <p>
/// Each object in an ImageCache::Container is an Image, is
/// indexed by a FileIndex and
/// indexed by a ImageCache::LruIndex (which reflects its current and last use).
/// As objects are accessed in an ImageCache::Container the least
/// recently used ones are culled to maintain a cache with limited
/// entries and/or memory footprint and/or lifetime.
/// <p>
/// Copyright (c) 2009 Ross Tyler.
/// This file may be copied under the terms of the
/// GNU Lesser General Public License (LGPL).
/// See COPYING file for details.

#ifndef ImageCache_h_
#define ImageCache_h_

#include <sys/stat.h>

#include <boost/multi_index_container.hpp>
#include <boost/multi_index/sequenced_index.hpp>
#include <boost/multi_index/ordered_index.hpp>
#include <boost/multi_index/member.hpp>
#include <boost/multi_index/tag.hpp>

#include <boost/thread.hpp>
#include <boost/thread/mutex.hpp>

#include "FileIndex.h"
#include "Image.h"
#include "Reader.h"

namespace ImageCache {

    /// ImageCache::Container uses LruIndex
    /// to order images, Least Recently Used (LRU) first
    class LruIndex {
    public:
	unsigned int	count;		///< use counter
	time_t		time;		///< time of last use
	LruIndex() throw();
	bool operator < (LruIndex const &) const throw();
    };

    /// ImageCache::Value has a FileIndex, an LruIndex and an image
    class Value {
    public:
	FileIndex 	fileIndex;
	LruIndex	lruIndex;
	ImageConst *	image;
	Value(FileIndex const &, ImageConst *) throw();
    };

    /// ImageCache::Container uses both
    /// &Value::fileIndex and &Value::lruIndex Value members
    /// to order its Value objects.
    /// Images transferred (added) to the container
    /// will be deleted upon container destruction.
    class Container
	: private boost::mutex, private boost::multi_index_container<
	    Value,
	    boost::multi_index::indexed_by<
		boost::multi_index::ordered_unique<
		    boost::multi_index::tag<FileIndex>,
		    boost::multi_index::member<
			Value, FileIndex const, &Value::fileIndex> >,
		boost::multi_index::ordered_non_unique<
		    boost::multi_index::tag<LruIndex>,
		    boost::multi_index::member<
			Value, LruIndex const, &Value::lruIndex> >
	    >
	>
    {
    public:

	/// Construct an ImageCache::Container with its limits
	Container(
	    size_t countLimit,
	    unsigned long long memoryLimit,
	    time_t timeLimit,
	    int baseFd,
	    int persistFd) throw();

	~Container() throw();

	/// Add the image indexed by FileIndex to this container.
	/// This container assumes ownership of the image.
	/// \return True if the image was added successfully.
	void add(FileIndex const, ImageConst * image) throw();

	/// Open a Reader to the Image associated with the FileIndex.
	/// The caller is responsible for releasing the reader when done.
	/// \return The Reader if the image is cached; otherwise 0
	Reader * open(FileIndex) throw();

	/// Return the size of the Image associated with FileIndex or
	/// -1 if none.
	ssize_t sizeOf(FileIndex) throw();

    private:
	typedef index<FileIndex>::type	ByFileIndex;
	typedef index<LruIndex >::type	ByLruIndex;
	size_t 	countLimit;		///< Cull LRU when count  > countLimit
	size_t 	memoryLimit;		///< Cull LRU when memory > memoryLimit
	time_t 	timeLimit;		///< Cull LRU while now - then > timeLimit
	int	baseFd;			///<
	int	persistFd;			///< Cull to this directory
	size_t			count;	///< Number of Values in Container
	unsigned long long	memory;	///< Memory used by all images
	void cull() throw();
	bool volatile stop;
	boost::thread thread;		///< Periodic cull thread
	void run() throw();
	ImageConstPointer acquire(FileIndex) throw();
	void release(FileIndex) throw();
	void persist(FileIndex const, ImageConst * image) throw();
    };
}

#endif
