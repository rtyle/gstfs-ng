/// \file
/// Definition of the ReaderFactory class.
/// <p>
/// Copyright (c) 2009 Ross Tyler.
/// This file may be copied under the terms of the
/// GNU Lesser General Public License (LGPL).
/// See COPYING file for details.

#include "ImageReader.h"
#include "FileReader.h"
#include "ReaderFactory.h"
#include "TranscodeFileReader.h"

ReaderFactory::ReadAheadRelease::ReadAheadRelease(
    ReaderFactory & readerFactory_) throw()
:
    readerFactory(readerFactory_),
    stop(false),
    thread(boost::bind(&ReadAheadRelease::run, this))
{}

ReaderFactory::ReadAheadRelease::~ReadAheadRelease() throw() {
    {
	Synchronized synchronized(*this);
	stop = true;
	synchronized.notify();
    }
    thread.join();
}

Reader * ReaderFactory::ReadAheadRelease::pop() throw() {
    Synchronized synchronized(*this);
    for (;;) {
	if (deque.size()) {
	    Reader * reader = deque.front();
	    deque.pop_front();
	    return reader;
	}
	if (!readerFactory.readAheadCount && stop) return 0;
	synchronized.wait();
    }
}

void ReaderFactory::ReadAheadRelease::push(Reader * reader) throw() {
    Synchronized synchronized(*this);
    deque.push_back(reader);
    synchronized.notify();
}

void ReaderFactory::ReadAheadRelease::run() throw() {
    while (Reader * reader = pop()) {
	readerFactory.release(reader);
    }
}

ReaderFactory::ReaderFactory(
    int baseFd_,
    Transcode::Mapping * transcodeMapping_,
    bool trueSize_,
    size_t readAheadLimit_,
    size_t countLimit, size_t memoryLimit, time_t timeLimit, int persistFd)
throw()
:
    baseFd(baseFd_),
    transcodeMapping(transcodeMapping_),
    trueSize(trueSize_),
    readAheadLimit(readAheadLimit_),
    imageCache(countLimit, memoryLimit, timeLimit, baseFd, persistFd),
    readAheadCount(0),
    readAheadRelease(*this)
{}

ReaderFactory::~ReaderFactory() throw() {
    // we should not have to (and we don't) explicitly do anything.
    // all files explicitly opened we expect to have been explicitly released.
    // implicit readAhead readers will be released as part of
    // implicit destruction of our readAheadRelease member.
}

Reader * ReaderFactory::open(char const * path) throw() {
    boost::mutex::scoped_lock lock(*this);

    struct stat st;

    // get stat for path under base.
    // if successful and it is a directory then we are done.
    if ((-1 != fstatat(baseFd, path, &st, 0))
	    && S_ISDIR(st.st_mode))
	return 0;

    // find the source for this reader and if different stat the source
    Transcode::Element transcodeElement;
    boost::shared_ptr<char const> source(
	transcodeMapping->sourceFrom(path, &transcodeElement));
    if (source.get() != path) {
	if (-1 == fstatat(baseFd, source.get(), &st, 0))
	    return 0;
    }

    // this is how we will index the file
    FileIndex fileIndex(st);

    // if there is currently a Reader for this FileIndex, we will use it
    Map::iterator it = map.find(fileIndex);
    Reader * reader = it == map.end() ? 0 : it->second;
    if (!reader) {
	// there is no reader so create one.

	reader = imageCache.open(fileIndex);
	if (!reader) {
	    // we can not read from the imageCache

	    // if we cannot open the file, we are done.
	    int fileFd = openat(baseFd, source.get(), O_RDONLY);
	    if (-1 == fileFd) return 0;

	    if (source.get() == path) {
		reader = new FileReader(fileIndex, fileFd);
	    } else {
		if (readAheadCount < readAheadLimit) {
		    // the caller and readAheadRelease are responsible for it
		    reader = new TranscodeFileReader(fileIndex, fileFd,
			transcodeElement.pipeline,
			boost::bind(&ReaderFactory::readAheadIsDone, this, _1));
		    ++*reader;
		    ++readAheadCount;
		} else {
		    // only the caller is responsible for it
		    reader = new TranscodeFileReader(fileIndex, fileFd,
			transcodeElement.pipeline,
			boost::bind(&ReaderFactory::nonReadAheadIsDone, this, _1));
		}
	    }
	}

	// remember this new reader for others
	map.insert(Map::value_type(fileIndex, reader));
    }

    // our caller is responsible for the reader
    ++*reader;
    return reader;
}

void ReaderFactory::release(Reader * reader) throw() {
    boost::mutex::scoped_lock lock(*this);

    if (!--*reader) {
	if (ImageConst * image = reader->getImage()) {
	    imageCache.add(reader->fileIndex, image);
	}
	map.erase(reader->fileIndex);
	delete reader;
    }
}

int ReaderFactory::stat(char const * path, struct stat * st) throw() {
    Reader * reader;
    {
	boost::mutex::scoped_lock lock(*this);

	if (!*path) {
	    return -1 == fstat(baseFd, st) ? -errno : 0;
	}

	bool exists;

	// get stat for path under base.
	// if successful and it is a directory then we are done.
	if ((exists = (-1 != fstatat(baseFd, path, st, 0)))
		&& S_ISDIR(st->st_mode))
	    return 0;

	// if there is no mapping to this target, return what we have
	Transcode::Element transcodeElement;
	boost::shared_ptr<char const> source(
	    transcodeMapping->sourceFrom(path, &transcodeElement));
	if (source.get() == path) return exists ? 0 : -errno;

	// if we cannot stat the source, return the failure
	if (-1 == fstatat(baseFd, source.get(), st, 0)) return -errno;

	// this is how we will index the file
	FileIndex fileIndex(*st);

	// if there is an image cached for this FileIndex,
	// return stat with its size.
	ssize_t size = imageCache.sizeOf(fileIndex);
	if (0 <= size) {
	    st->st_size = size;
	    return 0;
	}

	Map::iterator it = map.find(fileIndex);
	if (it != map.end()) {
	    // there is a Reader for this FileIndex
	    reader = it->second;
	    // we are responsible for it
	    ++*reader;

	} else if (trueSize
		|| readAheadCount < readAheadLimit) {
	    // there is no Reader for this FileIndex
	    // but we could use one.

	    // if we cannot open the file,
	    // return error and stat with a size of 0.
	    int fileFd = openat(baseFd, source.get(), O_RDONLY);
	    if (-1 == fileFd) {
		st->st_size = 0;
		return -errno;
	    }

	    // construct a new TranscodeFileReader
	    reader = new TranscodeFileReader(fileIndex, fileFd,
		transcodeElement.pipeline,
		boost::bind(&ReaderFactory::readAheadIsDone, this, _1));
	    map.insert(Map::value_type(fileIndex, reader));

	    // readAheadRelease is responsible for it ...
	    ++*reader;
	    ++readAheadCount;

	    // ... and so are we
	    ++*reader;

	} else {
	    // there is no Reader for this FileIndex and we don't care
	    return 0;
	}

	// if we are going to block on trueSize we need to do it without
	// holding our lock.
    }

    // read (true?) size and release our hold on the reader
    st->st_size = reader->size(trueSize);
    release(reader);

    return 0;
}

void ReaderFactory::readAhead(char const * path) throw() {
    boost::mutex::scoped_lock lock(*this);

    // if we are at the readAheadLimit then return
    if (!(readAheadCount < readAheadLimit)) return;

    struct stat st;

    // get stat for path under base.
    // if successful and it is a directory then we are done.
    if ((-1 != fstatat(baseFd, path, &st, 0))
	    && S_ISDIR(st.st_mode))
	return;

    // if there is no mapping to this target, return
    Transcode::Element transcodeElement;
    boost::shared_ptr<char const> source(
	transcodeMapping->sourceFrom(path, &transcodeElement));
    if (source.get() == path) return;

    // if we cannot stat the source, return
    if (-1 == fstatat(baseFd, source.get(), &st, 0)) return;

    // this is how we will index the file
    FileIndex fileIndex(st);

    // if there is an image cached for this FileIndex, return
    if (0 <= imageCache.sizeOf(fileIndex)) return;

    // if there is currently a Reader for this FileIndex, return
    if (map.find(fileIndex) != map.end()) return;

    // if we cannot open the file, return
    int fileFd = ::openat(baseFd, source.get(), O_RDONLY);
    if (-1 == fileFd) return;

    // construct a new TranscodeFileReader
    Reader * reader = new TranscodeFileReader(fileIndex, fileFd,
	transcodeElement.pipeline,
	boost::bind(&ReaderFactory::readAheadIsDone, this, _1));
    map.insert(Map::value_type(fileIndex, reader));

    // readAheadRelease is responsible for it
    ++*reader;
    ++readAheadCount;
}

void ReaderFactory::readAheadIsDone(Reader * reader) throw() {
    boost::mutex::scoped_lock lock(*this);
    --readAheadCount;
    readAheadRelease.push(reader);
}

void ReaderFactory::nonReadAheadIsDone(Reader * reader) throw() {}
