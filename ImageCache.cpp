/// \file
/// Definitions in the ImageCache namespace of the ImageCache::Container
/// and supporting classes.
/// <p>
/// Copyright (c) 2009 Ross Tyler.
/// This file may be copied under the terms of the
/// GNU Lesser General Public License (LGPL).
/// See COPYING file for details.
#include <limits>
#include <set>

#include <ext/stdio_filebuf.h>

#include "fcntl.h"

#include "FileReader.h"
#include "FindFile.h"
#include "ImageReader.h"
#include "Utility.h"

#include "ImageCache.h"

namespace ImageCache {

    typedef std::set<FileIndex> FileIndexSet;

    /// \brief An AllFiles instance is a FileIndexSet that is built at
    /// construction time from everything under a location in a filesystem.
    /// Symbolic links are followed and files are only visited once.
    class AllFiles : public FileIndexSet,
	    private FindFile::Visitor<FindFile::LocationSymLinkFollow> {
    private:
	DirectionEnum before(FindFile::LocationSymLinkFollow * location)
		throw() {
	    if (false
		    || location->match()	// recursion
		    || !location->isDefined())	// cannot stat(2) location
		return Prune;
	    // insert a FileIndex for this location into this set
	    if (!insert(FileIndex(location->st)).second) {
		return Prune;			// location visited before
	    }
	    return Continue;
	}
    public:
	AllFiles(int fd) throw(std::runtime_error) {
	    traverse(fd);
	}
    };

    /// \brief A RemoveAllUnreferencedFiles object is created to remove every
    /// file at a depth of one under a location that has a FileIndex name
    /// whose FileIndex is not in a references FileIndexSet.
    class RemoveAllUnreferencedFiles : FindFile::Visitor<> {
    private:
	FileIndexSet & references;
	DirectionEnum before(FindFile::Location<> * location) throw () {
	    if (0 == location->depth) return Continue;	// deeper
	    if (location->isDefined() && S_ISREG(location->st.st_mode)) {
		std::istringstream in(location->name);
		FileIndex fileIndex;
		if (in >> fileIndex && in.eof()) {
		    // a FileIndex can be built from the location name
		    if (references.end() == references.find(fileIndex)) {
			// remove the file because it is not referenced
			unlinkat(location->parent->fd, location->name, 0);
		    }
		}
	    }
	    return Prune; // no deeper
	}
    public:
	RemoveAllUnreferencedFiles(int fd, FileIndexSet & references_) throw()
	:
	    references(references_)
	{
	    traverse(fd);
	}
    };

    LruIndex::LruIndex() throw() : count(0), time(::time(0)) {}

    Value::Value(
	FileIndex const & fileIndex_, ImageConst * image_) throw()
    : fileIndex(fileIndex_), lruIndex(), image(image_) {}

    bool LruIndex::operator < (LruIndex const & that) const throw() {
	return count < that.count
	    ? true
	    : count > that.count
		? false
		: time < that.time
		    ? true
		    : false;
    }

    void Container::run() throw() {
	if (std::numeric_limits<time_t>::max() == timeLimit) return;
	// we need to be interruptible.
	// this could be implemented best by using boost 1.3.5 interruptible
	// threads but we don't want to depend on this availability (yet).
	// it could be implemented better by interrupting a blocked system
	// call with pthread_kill but this would assume boost threads
	// are based on posix threads (is that bad?) and is a little ugly.
	// until there is felt to be a need/capability to do otherwise,
	// we will just poll to find out when to stop.
	time_t when = time(0) + timeLimit;
	while (!stop) {
	    time_t now = time(0);
	    if (now > when) {
		boost::mutex::scoped_lock lock(*this);
		cull();
		when = now + timeLimit;
	    }
	    sleep(5);
	}
    }

    Container::Container(
	size_t countLimit_,
	unsigned long long memoryLimit_,
	time_t timeLimit_,
	int baseFd_,
	int persistFd_) throw()
    :
	countLimit(countLimit_),
	memoryLimit(memoryLimit_),
	timeLimit(timeLimit_),
	baseFd(baseFd_),
	persistFd(persistFd_),
	count(0),
	memory(0),
	stop(0),
	thread(boost::bind(&Container::run, this))
    {
	if (-1 == persistFd) return;
	try {
	    AllFiles references(baseFd);
	    RemoveAllUnreferencedFiles(persistFd, references);
	} catch (std::runtime_error & e) {
	    std::cerr << e.what() << std::endl;
	}
    }

    Container::~Container() throw() {
	stop = true;
	thread.join();
	ByLruIndex & byLruIndex = get<LruIndex>();
	for (ByLruIndex::iterator it = byLruIndex.begin();
		it != byLruIndex.end(); ++it) {
	    persist(it->fileIndex, it->image);
	    delete it->image;
	}
    }

    void Container::add(FileIndex fileIndex, ImageConst * image) throw() {
	boost::mutex::scoped_lock lock(*this);
	bool inserted = get<LruIndex>().insert(Value(fileIndex, image)).second;
	assert(inserted);
	++count;
	memory += image->size();
	cull();
    }

    void Container::release(FileIndex fileIndex) throw() {
	boost::mutex::scoped_lock lock(*this);
	ByFileIndex & byFileIndex = get<FileIndex>();
	ByFileIndex::iterator it = byFileIndex.find(fileIndex);
	if (it == byFileIndex.end()) return;
	// decrement use count on Value and replace it (reorder LruIndex)
	// set the time if last use.
	Value value = *it;
	if (!--value.lruIndex.count) value.lruIndex.time = time(0);
	byFileIndex.replace(it, value);
	if (!value.lruIndex.count) cull();
    }

    /// Acquire the Image associated with the FileIndex.
    /// A copy of a ImageConstPointer is returned and when the
    /// last copy of this smart pointer is destroyed the image will
    /// be automatically released by calling our private release method.
    /// \return A smart pointer that references the image or 0 if none.
    ImageConstPointer Container::acquire(FileIndex fileIndex) throw() {
	// callers should have already obtained a lock on *this!
	ByFileIndex & byFileIndex = get<FileIndex>();
	ByFileIndex::iterator it = byFileIndex.find(fileIndex);
	if (it == byFileIndex.end()) {
	    return ImageConstPointer(
		static_cast<ImageConst *>(0), Utility::NoDeleter());
	}
	// increment use count on Value and replace it (reorder LruIndex)
	Value value = *it;
	++value.lruIndex.count;
	byFileIndex.replace(it, value);
	return ImageConstPointer(value.image,
	    boost::bind(&Container::release, this, fileIndex));
    }

    static std::string persistName(FileIndex fileIndex) throw() {
	std::ostringstream name;
	name << fileIndex;
	return name.str();
    }

    Reader * Container::open(FileIndex fileIndex) throw() {
	boost::mutex::scoped_lock lock(*this);
	// if there is an image cached for this FileIndex,
	// return a new ImageReader constructed with it.
	ImageConstPointer imageConstPointer = acquire(fileIndex);
	if (imageConstPointer)
	    return new ImageReader(fileIndex, imageConstPointer);
	if (-1 == persistFd) return 0;
	int fd = openat(persistFd, persistName(fileIndex).c_str(), O_RDONLY);
	if (-1 == fd) return 0;
	return new FileReader(fileIndex, fd);
    }

    ssize_t Container::sizeOf(FileIndex fileIndex) throw() {
	boost::mutex::scoped_lock lock(*this);
	ByFileIndex & byFileIndex = get<FileIndex>();
	ByFileIndex::iterator it = byFileIndex.find(fileIndex);
	if (byFileIndex.end() != it) return it->image->size();
	if (-1 == persistFd) return -1;
	struct stat st;
	if (-1 != fstatat(persistFd, persistName(fileIndex).c_str(), &st, 0))
	    return st.st_size;
	return -1;
    }

    void Container::cull() throw() {
	// callers should have already obtained a lock on *this!
	time_t now = time(0);
	time_t latest = now < timeLimit ? 0 : now - timeLimit;
	ByLruIndex & byLruIndex = get<LruIndex>();
	ByLruIndex::iterator it = byLruIndex.begin();
	while (it != byLruIndex.end()
		&& 0 == it->lruIndex.count
		&& (countLimit < count
		    || memoryLimit < memory
		    || (it->lruIndex.time < latest))) {
	    --count;
	    memory -= it->image->size();
	    persist(it->fileIndex, it->image);
	    delete it->image;
	    it = byLruIndex.erase(it);
	}
    }

    void Container::persist(FileIndex fileIndex, ImageConst * image) throw() {
	// callers should have already obtained a lock on *this!
	if (-1 == persistFd) return;
	std::string name = persistName(fileIndex);
	std::string temp = name + ".tmp";
	int fd = openat(persistFd, temp.c_str(), O_CREAT | O_WRONLY, 0666);
	if (-1 == fd) return;
	bool success;
	{
	    __gnu_cxx::stdio_filebuf<char> buffer(fd, std::ios::out);
	    std::ostream out(&buffer);
	    success = (out << *image);
	    // stdio_filebuf destructor will close fd
	}
	if (success) {
	    renameat(persistFd, temp.c_str(), persistFd, name.c_str());
	} else {
	    unlinkat(persistFd, temp.c_str(), 0);
	}
    }
}
