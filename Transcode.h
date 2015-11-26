/// \file
/// Declarations in the Transcode namespace of the Transcode::Mapping class
/// and supporting classes.
/// <p>
/// Copyright (c) 2009 Ross Tyler.
/// This file may be copied under the terms of the
/// GNU Lesser General Public License (LGPL).
/// See COPYING file for details.

#ifndef Transcode_h
#define Transcode_h

#include <boost/multi_index_container.hpp>
#include <boost/multi_index/ordered_index.hpp>
#include <boost/multi_index/member.hpp>
#include <boost/multi_index/tag.hpp>

#include <boost/shared_ptr.hpp>

#include <fuse.h>

#include "Utility.h"

namespace Transcode {

    /// A Transcode::Element associates a source, target and pipeline with
    /// one another.
    class Element {
    public:
	char const *	source;
	char const *	target;
	char const *	pipeline;
	Element() throw();
	Element(
	    char const * source, char const * target, char const * pipeline)
	    throw();
    };

    struct SourceIndex {};	///< Used only for multi_index::tag
    struct TargetIndex {};	///< Used only for multi_index::tag

    /// Transcode::Mapping uses both
    /// &Element::source and &Element::target Element members
    /// to order its Element objects.
    /// Elements (added) to the mapping
    /// will be deleted upon mapping destruction.
    class Mapping : private boost::multi_index_container<
	    Element,
	    boost::multi_index::indexed_by<
		boost::multi_index::ordered_unique<
		    boost::multi_index::tag<SourceIndex>,
		    boost::multi_index::member<
			Element, char const *, &Element::source>,
		    Utility::LessThan<char const *>
		>,
		boost::multi_index::ordered_unique<
		    boost::multi_index::tag<TargetIndex>,
		    boost::multi_index::member<
			Element, char const *, &Element::target>,
		    Utility::LessThan<char const *>
		>
	    >
	>
    {

    private:

	typedef index<SourceIndex>::type BySourceIndex;
	typedef index<TargetIndex>::type ByTargetIndex;

	void add(
	    char const * source, char const * target, char const * pipeline)
	    throw();

    public:

	/// The option method of a Transcode::Mapping::Builder can be
	/// called while parsing fuse_args to collect source, target and
	/// pipeline associations and add them to its mapping.
	/// Each Mapping has a public Builder that should be so-used to
	/// build the Transcode mapping.
	class Builder {
	private:
	    Mapping &		mapping;
	    char const *	source;
	    char const *	target;
	    char const *	pipeline;
	    void build() throw();
	public:
	    Builder(Mapping & mapping) throw();
	    int option(
		char const * arg, int key, fuse_args * args) throw();
	    bool pending() throw();
	};

	Builder builder;
	Mapping() throw();
	~Mapping() throw();
	size_t size() throw();

	/// Return argument if no mapping from target path to source;
	/// otherwise, new char[] with mapped result
	/// and, if requested, the Element of the mapping.
	/// Result wrapped in smart pointer to destroy allocated memory if/when
	/// needed.
	boost::shared_ptr<char const> sourceFrom(
	    char const * path, Element * element = 0) throw();

	/// Return argument if no mapping from source path to target;
	/// otherwise, new char[] with mapped result
	/// and, if requested, the Element of the mapping.
	/// Result wrapped in smart pointer to destroy allocated memory if/when
	/// needed.
	boost::shared_ptr<char const> targetFrom(
	    char const * path, Element * element = 0) throw();
    };
}

#endif
