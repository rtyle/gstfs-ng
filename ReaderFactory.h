/// \file
/// Declaration of the ReaderFactory class.
/// <p>
/// Copyright (c) 2009 Ross Tyler.
/// This file may be copied under the terms of the
/// GNU Lesser General Public License (LGPL).
/// See COPYING file for details.

#ifndef ReaderFactory_h_
#define ReaderFactory_h_

#include <deque>
#include <map>
#include <set>

#include <boost/shared_ptr.hpp>
#include <boost/thread.hpp>
#include <boost/thread/mutex.hpp>


#include "ImageCache.h"
#include "Reader.h"
#include "Synchronizable.h"
#include "Transcode.h"

/// A ReaderFactory class manufactures Reader objects as they are needed.
/// It keeps track of active readers in a Map that is accessed with a FileIndex.
/// A reader is acquired when needed and released when done.
/// There is at most one Reader object for a FileIndex but this one Reader
/// may be used many times.
/// When the first need acquires the Reader it is created and when the last
/// releases it it is destroyed.
/// Before a Reader is destroyed it is asked for any image that it might have.
/// If it offers one, the ReaderFactory caches the image in its imageCache.
/// The imageCache is also indexed by FileIndex.
/// The next time a Reader is acquired for this FileIndex, if the image
/// is still cached, an ImageReader is created for it instead of a
/// FileReader.
class ReaderFactory : public boost::mutex {
private:

    class ReadAheadRelease : private Synchronizable<boost::mutex> {
    private:
	ReaderFactory & readerFactory;
	std::deque<Reader *> deque;
	bool stop;
	Reader * pop() throw();
	void run() throw();
	boost::thread thread;
    public:
	ReadAheadRelease(ReaderFactory &) throw();
	~ReadAheadRelease() throw();
	void push(Reader *) throw();
    };

    typedef std::map<FileIndex const, Reader *> Map;
    Map map;
    int baseFd;
    Transcode::Mapping * transcodeMapping;
    bool trueSize;
    size_t readAheadLimit;
    ImageCache::Container imageCache;
    size_t volatile readAheadCount;
    ReadAheadRelease readAheadRelease;

    void readAheadIsDone(Reader *) throw();
    void nonReadAheadIsDone(Reader *) throw();

public:

    ReaderFactory(
	int baseFd,
	Transcode::Mapping * transcodeMapping,
	bool trueSize,
	size_t readAheadLimit,
	size_t imageCacheCountLimit,
	size_t imageCacheMemoryLimit,
	time_t imageCacheTimeLimit,
	int imageCachePersistFd)
	throw();

    ~ReaderFactory() throw();

    /// Open a Reader for the file suggested by the target path.
    /// The caller is responsible for releasing the reader when done.
    /// If room for readAhead, readAheadRelease will share this responsibility
    /// but won't do the release until readAheadIsDone.
    /// This way the image might be cached even if the opener releases
    /// a TranscodeFileReader before readAheadIsDone.
    /// \return The Reader
    Reader * open(char const * path) throw();

    /// Every Reader that is opened must be released.
    /// When the last user of the Reader releases it, the Reader is destroyed.
    void release(Reader * reader) throw();

    /// Get stat for the file suggested by the target path.
    /// Depending on trueSize, accuracy on the true size will be
    /// guaranteed or not.
    int stat(char const * path, struct stat * st) throw();

    /// Subject to our readAheadLimit and if appropriate,
    /// construct a new TranscodeFileReader to start transcoding the
    /// file and assign ownership to readAheadRelease.
    void readAhead(char const * path) throw();

};

#endif
