/// \file
/// Definition of the TranscodeFileReader class.
/// <p>
/// Copyright (c) 2009 Ross Tyler.
/// This file may be copied under the terms of the
/// GNU Lesser General Public License (LGPL).
/// See COPYING file for details.

#include <iostream>
#include <map>

#include <gst/gst.h>
#include <glib.h>

#include <boost/bind/bind.hpp>
#include <boost/scoped_ptr.hpp>
#include <boost/shared_ptr.hpp>

#include "readlink.h"
#include "Cwd.h"
#include "TranscodeFileReader.h"
#include "Utility.h"

TranscodeFileReader::TranscodeFileReader(
    FileIndex fileIndex_, int fd_,
    char const * pipelineDescription,
    boost::shared_ptr<void const> & doneGuarantee,
    boost::function<void (Reader *)> done) throw()
:
    FileReader(fileIndex_, fd_),
    pipeline(0),
    bus(0),
    imageBuilderThread(0)
{
    // guarantee a call to the done function object until
    // we transfer the guarantee to our imageBuilderThread
    doneGuarantee.reset(static_cast<void const *>(0), boost::bind(done, this));

    // resolve the location of/from fd
    boost::shared_ptr<char const> locationShared = readlink(fd);
    char const * location = locationShared.get();

    // resolve the location's directory
    // and make it current during this scope.
    // this allows elements in the pipeline to locate files relatively
    // but will also effect relative components of GST_PLUGIN_PATH
    char const * slash = strrchr(location, '/');
    size_t directoryLength = slash ? slash - location : strlen(location);
    char * directory = new char[directoryLength + 1];
    boost::scoped_ptr<char> directoryScoped(directory);
    memcpy(directory, location, directoryLength);
    directory[directoryLength] = 0;
    CwdSynchronized cwd(directory);

    // construct our GstPipeline from the pipelineDescription
    GError * error = 0;
    pipeline = gst_parse_launch(pipelineDescription, &error);
    if (error) {
	std::cerr << error->message << std::endl;
	g_error_free(error);
    }
    if (!pipeline) return;

    {
	boost::shared_ptr<GstElement> fdsrc(
	    gst_bin_get_by_name(GST_BIN(pipeline), "fdsrc"),
	    gst_object_unref);
	if (fdsrc) {
	    // set the fd property of the fdsrc GstElement
	    // (named fdsrc) to the source fd
	    g_object_set(G_OBJECT(fdsrc.get()), "fd", fd, NULL);
	} else {
	    boost::shared_ptr<GstElement> filesrc(
		gst_bin_get_by_name(GST_BIN(pipeline), "filesrc"),
		gst_object_unref);
	    if (filesrc) {
		// set the location property of the filesrc GstElement
		// (named filesrc) to the source location
		g_object_set(G_OBJECT(filesrc.get()),
		    "location", location, NULL);
	    } else {
		std::cerr << pipelineDescription
		    << ": no element named fdsrc or filesrc" << std::endl;
		return;
	    }
	}
    }

    int pipe[2];
    {
	// set the sync property of the fdsink GstElement (named fdsink) to false
	boost::shared_ptr<GstElement> fdsink(
	    gst_bin_get_by_name(GST_BIN(pipeline), "fdsink"),
	    gst_object_unref);
	if (!fdsink) {
	    std::cerr << pipelineDescription
		<< ": no element named fdsink" << std::endl;
	    return;
	}
	g_object_set(G_OBJECT(fdsink.get()), "sync", 0, NULL);

	// create a pipe for consuming the output of the pipeline
	if (-1 == ::pipe(pipe)) {
	    std::cerr << "pipe failed" << std::endl;
	    return;
	}

	// set the fd property of the fdsink GstElement (named fdsink)
	// to the write side of the pipe.
	g_object_set(G_OBJECT(fdsink.get()), "fd", pipe[1], NULL);
    }

    // create an ImageBuilderThread to consume what is output through the pipe,
    // transfer pipe ownership and our doneGuarantee to it
    // and responsibility to close the pipe ends when done
    imageBuilderThread = new ImageBuilderThread(pipe[0], pipe[1],
	doneGuarantee);

    // make sure that we are notified when interesting things happen
    {
	bus = gst_pipeline_get_bus(GST_PIPELINE(pipeline));
	gst_bus_add_signal_watch(bus);
	g_signal_connect(bus, "message::warning",
	    G_CALLBACK(warning_), this);
	g_signal_connect(bus, "message::error",
	    G_CALLBACK(error_), this);
	g_signal_connect(bus, "message::eos",
	    G_CALLBACK(ImageBuilderThread::eos_), this->imageBuilderThread);
    }

    // start the pipeline
    if (GST_STATE_CHANGE_ASYNC
	    == gst_element_set_state(pipeline, GST_STATE_PLAYING)) {
	// block until async state change completes
	gst_element_get_state(pipeline, 0, 0, GST_CLOCK_TIME_NONE);
    }
}

gboolean TranscodeFileReader::warning(GstBus * bus, GstMessage * message) throw() {
    GError * error;
    gchar * debug;
    gst_message_parse_error(message, &error, &debug);
    std::cerr << "warning=" << error->message << ", debug=" << debug << std::endl;
    g_error_free(error);
    g_free(debug);
    return TRUE;	// call us again
}
/*static*/ gboolean TranscodeFileReader::warning_(
	GstBus * bus, GstMessage * message, TranscodeFileReader * that) throw() {
    return that->warning(bus, message);
}

gboolean TranscodeFileReader::error(GstBus * bus, GstMessage * message) throw() {
    GError * error;
    gchar * debug;
    gst_message_parse_error(message, &error, &debug);
    std::cerr << "error=" << error->message << ", debug=" << debug << std::endl;
    g_error_free(error);
    g_free(debug);
    return TRUE;	// call us again
}
/*static*/ gboolean TranscodeFileReader::error_(
	GstBus * bus, GstMessage * message, TranscodeFileReader * that) throw() {
    return that->error(bus, message);
}

/*virtual*/ TranscodeFileReader::~TranscodeFileReader() throw() {
    if (pipeline) {
	if (GST_STATE_CHANGE_ASYNC
		== gst_element_set_state(pipeline, GST_STATE_NULL)) {
	    // block until async state change completes
	    gst_element_get_state(pipeline, 0, 0, GST_CLOCK_TIME_NONE);
	}
	if (bus) {
	    gst_bus_remove_signal_watch(bus);
	    gst_object_unref(bus);
	}
	gst_object_unref(pipeline);
    }
    if (imageBuilderThread) {
	imageBuilderThread->stopRunning();
	delete imageBuilderThread;
    }
}

/*virtual*/ ssize_t TranscodeFileReader::read(
	char * buffer, size_t size, off_t offset_) throw() {
    if (0 > offset_) return -EINVAL;
    size_t offset = offset_;
    if (!imageBuilderThread) return -EIO;
    return imageBuilderThread->read(buffer, size, offset);
}

/*virtual*/ size_t TranscodeFileReader::size(bool wait) throw() {
    return imageBuilderThread ? imageBuilderThread->size(wait) : 0;
}

/* virtual*/ ImageConst * TranscodeFileReader::getImage() throw() {
    return imageBuilderThread->getImage();
}

void TranscodeFileReader::ImageBuilderThread::stopRunning() throw() {
    Synchronized synchronized(*this);
    if (-1 != out) {
	close(out);
	out = -1;
    }
}

gboolean TranscodeFileReader::ImageBuilderThread::eos(
	GstBus * bus, GstMessage * message) throw() {
    streaming = false;
    stopRunning();
    return TRUE;	// call us again
}
/*static*/ gboolean TranscodeFileReader::ImageBuilderThread::eos_(
	GstBus * bus, GstMessage * message, ImageBuilderThread * that) throw() {
    return that->eos(bus, message);
}

ssize_t TranscodeFileReader::ImageBuilderThread::read(
	char * buffer, size_t size, size_t offset) throw() {
    // wait until we can answer the request
    Synchronized synchronized(*this);
    while (running && offset + size > image->size()) synchronized.wait();
    // answer the request the best we can
    if (offset >= image->size()) return 0;
    size_t available = image->size() - offset;
    size_t copy = size < available ? size : available;
    image->copy(offset, copy, buffer);
    return copy;
}

size_t TranscodeFileReader::ImageBuilderThread::size(bool wait) throw() {
    // wait until we can answer the request
    Synchronized synchronized(*this);
    if (!wait) return image->size();
    while (running) synchronized.wait();
    return image->size();
}

ImageConst * TranscodeFileReader::ImageBuilderThread::getImage() throw() {
    Synchronized synchronized(*this);
    if (streaming) {
	// image is not complete
	return 0;
    } else {
	// image is complete.
	// transfer ownership to the caller.
	ImageConst * image = this->image;
	this->image = 0;
	return image;
    }
}

void TranscodeFileReader::ImageBuilderThread::run() throw() {
    char tile[8192];
    ssize_t length;
    while (0 < (length = ::read(in, tile, sizeof tile))) {
	// append the tile to the image that has already been transcoded
	// and notifyAll that might be waiting for this in read().
	Synchronized synchronized(*this);
	image->append(tile, length);
	synchronized.notifyAll();
    }
    // there is nothing more to be transcoded so close our input
    // and notifyAll that might be waiting for this in read().
    Synchronized synchronized(*this);
    close(in);
    running = false;
    synchronized.notifyAll();
    // fulfill our doneGuarantee now
    doneGuarantee.reset();
}

TranscodeFileReader::ImageBuilderThread::ImageBuilderThread(
    int in_, int out_, boost::shared_ptr<void const> doneGuarantee_) throw()
:
    in(in_),
    out(out_),
    doneGuarantee(doneGuarantee_),
    running(true),
    streaming(true),
    image(new Image()),
    thread(boost::bind(&ImageBuilderThread::run, this))
{}

TranscodeFileReader::ImageBuilderThread::~ImageBuilderThread() throw() {
    thread.join();
    if (image) delete image;
}
