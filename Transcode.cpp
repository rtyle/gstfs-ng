/// \file
/// Definitions in the Transcode namespace of the Transcode::Mapping class
/// and supporting classes.
/// <p>
/// Copyright (c) 2009 Ross Tyler.
/// This file may be copied under the terms of the
/// GNU Lesser General Public License (LGPL).
/// See COPYING file for details.

#include <cstring>
#include <iostream>

#include "Transcode.h"
#include "Utility.h"

namespace Transcode {

    Element::Element() throw() : source(0), target(0), pipeline(0) {}

    Element::Element(
	char const * source_, char const * target_, char const * pipeline_)
	throw()
    :
	source(source_),
	target(target_),
	pipeline(pipeline_)
    {}

    Mapping::Builder::Builder(Mapping & mapping_) throw()
    : mapping(mapping_), source(0), target(0), pipeline(0)
    {}

    Mapping::Builder::~Builder() throw(){
	if (source)	free(const_cast<char *>(source));
	if (target)	free(const_cast<char *>(target));
	if (pipeline)	free(const_cast<char *>(pipeline));
    }

    bool Mapping::Builder::pending() throw() {
	return source || target || pipeline;
    }

    void Mapping::Builder::build() throw() {
	if (source && target && pipeline) {
	    mapping.add(source, target, pipeline);
	    free(const_cast<char *>(source));
	    free(const_cast<char *>(target));
	    free(const_cast<char *>(pipeline));
	    source = target = pipeline = 0;
	}
    }

    int Mapping::Builder::option(
	    char const * arg, int key, fuse_args * args) throw() {
	size_t length;
	if ((length = Utility::match(arg, "source=", "src_ext=", 0))) {
	    source = strdup(arg + length);
	    build();
	    return 0;
	}
	if ((length = Utility::match(arg, "target=", "dst_ext=", 0))) {
	    target = strdup(arg + length);
	    build();
	    return 0;
	}
	if ((length = Utility::match(arg, "pipeline=", 0))) {
	    pipeline = strdup(arg + length);
	    build();
	    return 0;
	}
	return 1;
    }

    Mapping::Mapping() throw() : builder(*this) {}

    Mapping::~Mapping() throw() {
	BySourceIndex & bySourceIndex = get<SourceIndex>();
	for (BySourceIndex::iterator it = bySourceIndex.begin();
		it != bySourceIndex.end(); ++it) {
	    free(const_cast<char *>(it->source));
	    free(const_cast<char *>(it->target));
	    free(const_cast<char *>(it->pipeline));
	}
    }

    size_t Mapping::size() throw () {
	return get<SourceIndex>().size();
    }

    void Mapping::add(
	    char const * source_, char const * target_, char const * pipeline_)
	    throw() {
	char * source	= strdup(source_);
	char * target	= strdup(target_);
	// put pipeline_ in a fdsrc/fdsink sandwich
	char * pipeline	= strdup(
	    (std::string(
		    #if 0
			// this is the more elegant solution
			// which will allow us to use an fd directly
			// but will cause FLAC metatada to be lost
			"fdsrc name=fdsrc ! "
		    #else
			// this is a kludgy solution
			// which will cause us to derive a location from an fd
			// but will not cause FLAC metadata to be lost
			"filesrc name=filesrc ! "
		    #endif
		    )
		+ pipeline_
		+ (*pipeline_ ? " ! " : "")
		+ "fdsink name=fdsink"
	    ).c_str());
	if (!get<SourceIndex>().insert(Element(source, target, pipeline))
		.second) {
	    std::cerr
		<< "mapping from source extension \""
		<< source
		<< "\" or to target extension \""
		<< target
		<< "\" already specified - ignoring"
		<< std::endl;
	    free(source);
	    free(target);
	    free(pipeline);
	}
    }

    static char const * newConcatenation(
    	char const * base, char const * end, char const * extension) throw() {
        size_t baseLength = end - base;
        size_t extensionLength = strlen(extension) + 1;
        char * result = new char[baseLength + extensionLength];
        memcpy(baseLength + static_cast<char *>(memcpy(result,
	    base, baseLength)),
	    extension, extensionLength);
        return result;
    }

    /// For use as a custom deleter for a boost::shared_ptr
    /// when we want nothing to happen.
    struct Noop {
        void operator()(void const *) const {}
    };

    boost::shared_ptr<char const> Mapping::sourceFrom(
	    char const * path, Element * elementReturn) throw() {
	ByTargetIndex & byTargetIndex = get<TargetIndex>();
        char const * extension = path;
        while ((extension = strchr(extension, '.'))) {
            ByTargetIndex::iterator it = byTargetIndex.find(++extension);
	    if (it != byTargetIndex.end()) {
		Element const element = *it;
		if (elementReturn) *elementReturn = element;
		return boost::shared_ptr<char const>(
		    newConcatenation(path, extension, element.source));
	    }
        }
        return boost::shared_ptr<char const>(path, Noop());
    }

    boost::shared_ptr<char const> Mapping::targetFrom(
	    char const * path, Element * elementReturn) throw() {
	BySourceIndex & bySourceIndex = get<SourceIndex>();
        char const * extension = path;
        while ((extension = strchr(extension, '.'))) {
            BySourceIndex::iterator it = bySourceIndex.find(++extension);
	    if (it != bySourceIndex.end()) {
		Element const element = *it;
		if (elementReturn) *elementReturn = element;
		return boost::shared_ptr<char const>(
		    newConcatenation(path, extension, element.target));
	    }
        }
        return boost::shared_ptr<char const>(path, Noop());
    }
}
