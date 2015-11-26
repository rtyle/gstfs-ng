/// \file
/// Declaration of the GstFs class.
/// <p>
/// Copyright (c) 2009 Ross Tyler.
/// This file may be copied under the terms of the
/// GNU Lesser General Public License (LGPL).
/// See COPYING file for details.

#ifndef GstFs_h
#define GstFs_h

#include <map>

#include <dirent.h>

#include <fuse.h>
#include <glib/gmain.h>

#include <boost/shared_ptr.hpp>
#include <boost/thread.hpp>

#include "ReaderFactory.h"
#include "Transcode.h"

class GstFs {
private:

    /// A LoopThread runs a glib main loop for the default context.
    /// The glib main loop of this thread will watch (and dispatch)
    /// signals sent on the GstBus of a GstPipeline.
    class LoopThread {
    private:
	GMainLoop * loop;
	boost::thread thread;
	void run() throw();
    public:
	LoopThread() throw();
	~LoopThread() throw();
    };

    int argc;		///< Argument count after our options are taken out
    char ** argv;	///< Argument vector after our options are taken out
    Transcode::Mapping transcodeMapping;
    char const * base;
    int baseFd;
    LoopThread * loopThread;
    bool trueSize;
    size_t readAheadLimit;
    size_t imageCacheCountLimit;
    size_t imageCacheMemoryLimit;
    time_t imageCacheTimeLimit;
    int imageCachePersistFd;
    ReaderFactory * readerFactory;

    int option(
	char const * arg, int key, fuse_args * args) throw();
    static int option_(void * that,
	char const * arg, int key, fuse_args * args) throw();

    void * init(fuse_conn_info *) throw();
    static void * init_(fuse_conn_info *) throw();

    int getattr(char const * path, struct stat * st) throw();
    static int getattr_(char const * path, struct stat * st) throw();

    int open(char const * path, fuse_file_info * info) throw();
    static int open_(char const * path, fuse_file_info * info) throw();

    int opendir(char const * path, fuse_file_info * info) throw();
    static int opendir_(char const* path, fuse_file_info * info) throw();

    int read(
	char const * path, char * buffer, size_t size, off_t offset,
	fuse_file_info * info) throw();
    static int read_(
	char const * path, char * buffer, size_t size, off_t offset,
	fuse_file_info * info) throw();

    int readdir(
	char const * path, void * buffer, fuse_fill_dir_t filler, off_t offset,
	fuse_file_info * info) throw();
    static int readdir_(
	char const * path, void * buffer, fuse_fill_dir_t filler, off_t offset,
	fuse_file_info * info) throw();

    int release(char const * path, struct fuse_file_info *) throw();
    static int release_(char const * path, struct fuse_file_info *) throw();

    int releasedir(
	char const * path, fuse_file_info * info) throw();
    static int releasedir_(
	char const * path, fuse_file_info * info) throw();

public:
    GstFs(int argc, char ** argv) throw(std::runtime_error);
    virtual ~GstFs() throw();
    int main() throw();
};

#endif
