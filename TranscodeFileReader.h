/// \file
/// Declaration of the TranscodeFileReader class.
/// <p>
/// Copyright (c) 2009 Ross Tyler.
/// This file may be copied under the terms of the
/// GNU Lesser General Public License (LGPL).
/// See COPYING file for details.

#ifndef TranscodeFileReader_h
#define TranscodeFileReader_h

#include <boost/thread.hpp>
#include <boost/multi_index_container.hpp>
#include <boost/multi_index/member.hpp>

#include <gst/gst.h>

#include "Image.h"
#include "Synchronizable.h"

#include "FileReader.h"

/// A TranscodeFileReader is a specialized FileReader that is constructed with
/// a gstreamer pipeline description that reflects the transcoding
/// instructions.
///
/// The overidden #read method reads the transcoded image as it is being
/// created.
/// The overidden #stat method returns the size of the transcoded image,
/// potentially blocking until it is complete.
class TranscodeFileReader : public FileReader {
private:

    /// An ImageBuilderThread to build an Image from the output of a
    /// gstreamer pipeline.
    class ImageBuilderThread : public Synchronizable<boost::mutex> {
    private:
	int in;			///< Input from gstreamer pipeline
	int out;		///< Output from gstreamer pipeline
	boost::shared_ptr<void const> doneGuarantee;	///< reset when done
	bool running;		///< This thread is still running
	bool streaming;		///< GstPipeline is still streaming
	Image * image;		///< Built image
	boost::thread thread;	///< This thread
	void run() throw();	///< What this thread runs
    public:
	ImageBuilderThread(int in, int out,
	    boost::shared_ptr<void const>) throw();
	~ImageBuilderThread() throw();
	ssize_t read(char * buffer, size_t size, size_t offset) throw();
	size_t size(bool wait) throw();
	ImageConst * getImage() throw();
	void stopRunning() throw();
	gboolean eos(GstBus *, GstMessage *) throw();
	static gboolean eos_(GstBus *, GstMessage *, ImageBuilderThread *) throw();
    };

    GstElement * pipeline;	///< Gstreamer pipeline to build image
    GstBus * bus;		///< Gstreamer pipeline bus
    ImageBuilderThread * imageBuilderThread;	///< Thread to build image

    gboolean warning(GstBus *, GstMessage *) throw();
    static gboolean warning_(GstBus *, GstMessage *, TranscodeFileReader *) throw();
    gboolean error(GstBus *, GstMessage *) throw();
    static gboolean error_(GstBus *, GstMessage *, TranscodeFileReader *) throw();

public:

    /// Construct a TranscodeFileReader on the file identified by fileIndex
    /// and fd, using the parseable pipeline description and notify the
    /// done function object when done successfully or otherwise.
    TranscodeFileReader(
	FileIndex fileIndex, int fd,
	char const * pipeline,
	boost::shared_ptr<void const> & doneGuarantee,
	boost::function<void (Reader *)> done)
	throw();

    /// Destroy the TranscodeFileReader by aborting any transcoding in process
    ~TranscodeFileReader() throw();

    virtual ssize_t read(char * buffer, size_t size, off_t offset) throw();

    virtual size_t size(bool wait) throw();

    virtual ImageConst * getImage() throw();
};

#endif
