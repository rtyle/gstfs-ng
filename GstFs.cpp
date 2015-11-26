/// \file
/// Definition of the GstFs class.
/// <p>
/// Copyright (c) 2009 Ross Tyler.
/// This file may be copied under the terms of the
/// GNU Lesser General Public License (LGPL).
/// See COPYING file for details.

#include <cerrno>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <sstream>

#include <glib.h>

#include "GstFs.h"
#include "Utility.h"

static intptr_t getPhysicalMemorySize() {
   return (intptr_t) sysconf(_SC_PAGESIZE)
       *  (intptr_t) sysconf(_SC_PHYS_PAGES);
}

int GstFs::option(char const * arg, int key, fuse_args * args) throw() {
    switch (key) {
    case FUSE_OPT_KEY_OPT:
	// ignore fstab specific options
	if (0 == strcmp(arg, "user")
		|| 0 == strcmp(arg, "noauto"))
	    return 0;
	if (0 == transcodeMapping.builder.option(arg, key, args)) return 0;
	size_t length;
	if (-1 == baseFd
		&& (length = Utility::match(arg, "base=", "src=", 0))) {
	    baseFd = ::open(arg + length, O_RDONLY);
	    struct stat st;
	    fstat(baseFd, &st);
	    if (S_ISDIR(st.st_mode)) {
		base = strdup(arg + length);
	    } else {
		close(baseFd);
		baseFd = -1;
	    }
	    return 0;
	}
	if (Utility::match(arg, "trueSize", 0)) {
	    trueSize = true;
	    return 0;
	}
	if ((length = Utility::match(arg, "readAhead=", 0))) {
	    std::istringstream in(arg + length);
	    size_t readAhead;
	    if (in >> readAhead) {\
		readAheadLimit = readAhead;
		return 0;
	    }
	}
	if ((length = Utility::match(arg, "cacheCount=", 0))) {
	    std::istringstream in(arg + length);
	    size_t cacheCount;
	    if (in >> cacheCount) {
		char multiplier;
		if (in >> multiplier) {
		    switch (tolower(multiplier)) {
		    case 'k': cacheCount *= 1024; break;
		    case 'm': cacheCount *= 1024 * 1024; break;
		    case 'g': cacheCount *= 1024 * 1024 * 1024; break;
		    }
		}
		imageCacheCountLimit = cacheCount;
		return 0;
	    }
	}
	if ((length = Utility::match(arg, "cacheMemory=", 0))) {
	    std::istringstream in(arg + length);
	    unsigned long long cacheMemory;
	    if (in >> cacheMemory) {
		char multiplier;
		if (in >> multiplier) {
		    switch (tolower(multiplier)) {
		    case 'k': cacheMemory *= 1024; break;
		    case 'm': cacheMemory *= 1024 * 1024; break;
		    case 'g': cacheMemory *= 1024 * 1024 * 1024; break;
		    case '%': cacheMemory *= getPhysicalMemorySize() / 100;
		    }
		}
		imageCacheMemoryLimit = cacheMemory;
		return 0;
	    }
	}
	if ((length = Utility::match(arg, "cacheTime=", 0))) {
	    std::istringstream in(arg + length);
	    time_t cacheTime;
	    if (in >> cacheTime) {
		char multiplier;
		if (in >> multiplier) {
		    switch (tolower(multiplier)) {
		    case 's': break;
		    case 'm': cacheTime *= 60; break;
		    case 'h': cacheTime *= 60 * 60; break;
		    case 'd': cacheTime *= 60 * 60 * 24; break;
		    case 'w': cacheTime *= 60 * 60 * 24 * 7; break;
		    case 'y': cacheTime *= 60 * 60 * 24 * 7 * 52; break;
		    }
		}
		imageCacheTimeLimit = cacheTime;
		return 0;
	    }
	}
	if (-1 == imageCachePersistFd
		&& (length = Utility::match(arg, "cachePersist=", 0))) {
	    imageCachePersistFd = ::open(arg + length, O_RDONLY);
	    struct stat st;
	    fstat(imageCachePersistFd, &st);
	    if (!S_ISDIR(st.st_mode)) {
		close(imageCachePersistFd);
		imageCachePersistFd = -1;
	    }
	    return 0;
	}
	break;
    case FUSE_OPT_KEY_NONOPT:
	if (-1 == baseFd) {
	    baseFd = ::open(arg, O_RDONLY);
	    struct stat st;
	    fstat(baseFd, &st);
	    if (S_ISDIR(st.st_mode)) {
		base = strdup(arg);
	    } else {
		close(baseFd);
		baseFd = -1;
	    }
	    return 0;
	}
	break;
    }
    return 1;
}
/*static*/ int GstFs::option_(
	void * that, char const * arg, int key, fuse_args * args) throw() {
    return reinterpret_cast<GstFs *>(that)->option(arg, key, args);
}

GstFs::GstFs(int argc_, char ** argv_) throw(std::runtime_error)
:
    argc(argc_),
    argv(argv_),
    transcodeMapping(),
    base(0),
    baseFd(-1),
    loopThread(0),
    trueSize(false),
    readAheadLimit(16),
    imageCacheCountLimit(50),
    imageCacheMemoryLimit(getPhysicalMemorySize() / 4),
    imageCacheTimeLimit(60 * 60),
    imageCachePersistFd(-1),
    readerFactory(0)
{
    fuse_args args = FUSE_ARGS_INIT(argc, argv);
    fuse_opt_parse(&args, this, 0, option_);
    if (-1 == baseFd) {
	throw std::runtime_error("base option not specified or not a directory");
    }
    if (!transcodeMapping.size()) {
	throw std::runtime_error("no transcode mappings specified");
    }
    if (transcodeMapping.builder.pending()) {
	throw std::runtime_error("transcode mapping specification incomplete");
    }
    argc = args.argc;
    argv = args.argv;
}

/*virtual*/ GstFs::~GstFs() throw() {
    if (readerFactory) delete readerFactory;
    if (loopThread) delete loopThread;
    if (base) free(const_cast<char *>(base));
    close(baseFd);
    close(imageCachePersistFd);
}

int GstFs::main() throw() {
    fuse_operations ops = {};
    ops.getattr		= getattr_;
    ops.init		= init_;
    ops.open		= open_;
    ops.opendir		= opendir_;
    ops.readdir 	= readdir_;
    ops.release		= release_;
    ops.releasedir	= releasedir_;
    ops.read		= read_;
    return fuse_main(argc, argv, &ops, this);
}

static GstFs * that() throw() {
    return reinterpret_cast<GstFs *>(fuse_get_context()->private_data);
}

int GstFs::getattr(char const * path, struct stat * st) throw() {
    ++path;
    return readerFactory->stat(path, st);
}
/*static*/ int GstFs::getattr_(char const * path, struct stat * st) throw() {
    return that()->getattr(path, st);
}

void GstFs::LoopThread::run() throw() {
    g_main_loop_run(loop);
}

GstFs::LoopThread::LoopThread() throw()
:
    loop(g_main_loop_new(0, false)),
    thread(boost::bind(&LoopThread::run, this))
{}

GstFs::LoopThread::~LoopThread() throw() {
    g_main_loop_quit(loop);
    thread.join();
    g_main_loop_unref(loop);
}

void * GstFs::init(fuse_conn_info * conn) throw() {
    // since GstFs::main (fuse_main) might decide to daemonize the caller
    // (fork a child to complete processing and return to the parent)
    // we must delay construction of all threads until now.
    // otherwise, a thread in the parent will be aborted when the
    // short-lived parent exits and will not even exist int he long-lived child.
    readerFactory = new ReaderFactory(
	baseFd,
	&transcodeMapping,
	trueSize,
	readAheadLimit,
	imageCacheCountLimit,
	imageCacheMemoryLimit,
	imageCacheTimeLimit,
	imageCachePersistFd);
    loopThread = new LoopThread();
    return this;
}
/*static*/ void * GstFs::init_(fuse_conn_info * conn) throw() {
    return that()->init(conn);
}

int GstFs::opendir(
	char const * path, fuse_file_info * info) throw() {
    ++path;
    // if there is no path then we need to reopen the base directory
    // we cannot use a dup of baseFd because all readers would end up sharing
    // the same file offset!
    int dirFd = *path
	? openat(baseFd, path, O_RDONLY)
	: ::open(base, O_RDONLY);
    if (-1 == dirFd) return -errno;
    DIR * dir = fdopendir(dirFd);	// closedir will close dirFd
    if (!dir) {
	return -errno;
	close(dirFd);
    }
    info->fh = reinterpret_cast<intptr_t>(dir);
    return 0;
}
/*static*/ int GstFs::opendir_(
	char const* path, fuse_file_info * info) throw() {
    return that()->opendir(path, info);
}

int GstFs::open(char const * path, fuse_file_info * info) throw() {
    if (O_RDONLY != (info->flags & O_ACCMODE)) return -EACCES;
    ++path;
    if (!*path
	    || !(info->fh = reinterpret_cast<intptr_t>(
		readerFactory->open(path))))
	return -EACCES;
    return 0;
}
/*static*/ int GstFs::open_(char const * path, fuse_file_info * info) throw() {
    return that()->open(path, info);
}

int GstFs::read(
	char const * path, char * buffer, size_t size, off_t offset,
	fuse_file_info * info) throw() {
    return reinterpret_cast<Reader *>(info->fh)->read(buffer, size, offset);
}
/*static*/ int GstFs::read_(
	char const * path, char * buffer, size_t size, off_t offset,
	fuse_file_info * info) throw() {
    return that()->read(path, buffer, size, offset, info);
}

int GstFs::readdir(
	char const * path, void * buffer, fuse_fill_dir_t filler, off_t offset,
	fuse_file_info * info) throw() {
    ++path;
    DIR * dir = reinterpret_cast<DIR *>(info->fh);
    while (dirent * dirent = ::readdir(dir)) {
	boost::shared_ptr<char const> target(
	    transcodeMapping.targetFrom(dirent->d_name, 0));
	if (target.get() != dirent->d_name) {
	    std::string targetPath(path);
	    targetPath += '/';
	    targetPath += target.get();
	    readerFactory->readAhead(targetPath.c_str());
	}
	if (filler(buffer, target.get(), 0, 0)) break;
    }
    return 0;
}
/*static*/ int GstFs::readdir_(
	char const * path, void * buffer, fuse_fill_dir_t filler, off_t offset,
	fuse_file_info * info) throw() {
    return that()->readdir(path, buffer, filler, offset, info);
}

int GstFs::release(
	char const * path, fuse_file_info * info) throw() {
    readerFactory->release(reinterpret_cast<Reader *>(info->fh));
    return 0;
}
/*static*/ int GstFs::release_(
	char const * path, fuse_file_info * info) throw() {
    return that()->release(path, info);
}

int GstFs::releasedir(
	char const * path, fuse_file_info * info) throw() {
    return -1 == closedir(reinterpret_cast<DIR *>(info->fh))
	? -errno : 0;
}
/*static*/ int GstFs::releasedir_(
	char const * path, fuse_file_info * info) throw() {
    return that()->releasedir(path, info);
}
