/// \file
/// \brief Declaration and definition of classes in the FindFile namespace.
///
/// Copyright (c) 2009 Ross Tyler.
/// This file may be copied under the terms of the
/// GNU Lesser General Public License (LGPL).
/// See COPYING file for details.

#ifndef FindFile_h
#define FindFile_h

#include <boost/shared_ptr.hpp>

#include <dirent.h>
#include <fcntl.h>
#include <sys/stat.h>

#include "Exception.h"
#include "readlink.h"

namespace FindFile {

    template<
	int		ignore,		///< Mask of errors to ignore
	bool		follow>		///< True => follow symbolic links
    struct stat stat_(
	int		fd,		///< directory fd, if not -1
	char const *	name)		///< file name relative to directory
    throw(Exception::Error)
    /// \return The completed stat struct or zeros if there was an ignored error
    /// \throw Exception::Error If there was an error we could not ignore
    /// \brief This is a template function that serves as a wrapper to one of
    /// the stat(2) system call variants.
    {
	struct stat st;
	try {
	    if (-1 == fd) {
		if (follow) {
		    Exception::throwErrorIfNegative1(::stat(name, &st));
		} else {
		    Exception::throwErrorIfNegative1(lstat(name, &st));
		}
	    } else {
		Exception::throwErrorIfNegative1(
		    fstatat(fd, name, &st, follow ? 0: AT_SYMLINK_NOFOLLOW));
	    }
	} catch (Exception::Error & e) {
	    if (~ignore & (1 << e.error)) throw;
	    static struct stat const zero = {};
	    st = zero;
	}
	return st;
    }

    template<
	int		ignore>		///< Mask of errors to ignore
    int open_(
	int		fd,		///< directory fd, if not -1
	char const *	name)		///< file name relative to directory
    throw(Exception::Error)
    /// \return The opened file descriptor or -1 if there was an ignored error.
    /// \throw Exception::Error If there was an error we could not ignore
    /// \brief This is a template function that serves as a wrapper to one of
    /// the open(2) system call variants.
    {
	try {
	    return -1 == fd
		? Exception::throwErrorIfNegative1(
		    ::open(name, O_RDONLY))
		: Exception::throwErrorIfNegative1(
		    openat(fd, name, O_RDONLY));
	} catch (Exception::Error & e) {
	    if (~ignore & (1 << e.error)) throw;
	    return -1;
	}
    }

    /// \brief A Location is used to maintain the context of a node being
    /// visited in a filesystem hierarchy.
    /// This base class provides all of the context needed to support
    /// the base Visitor class traverse.
    /// This class may be extended to support additional context for
    /// Visitor specializations.
    ///
    /// This template class must be instantiated with stat
    /// and open functions to provide fd relative stat'ing and open'ing of
    /// a name.
    /// The default is to use a stat method that will not follow symbolic links,
    /// and ignore EACCES errors
    /// and to use an open method that will ignore EACCES errors.
    template<
	    struct stat stat(int, char const *) throw(std::runtime_error)
		= stat_<1 << EACCES, false>,
	    int open(int, char const *) throw(std::runtime_error)
		= open_<1 << EACCES>
	> class Location {
    public:
	char const *		name;	///< Name of file
	Location * const	parent;	///< Relative to parent directory?
	unsigned int const	depth;	///< Depth in traverse
	struct stat const	st;	///< Value returned by stat
	int const		fd;	///< Value returned by open
    protected:
	DIR * const		dir;	///< Value returned by fdopendir

    public:
	Location(
	    char const *	name_,	///< Name of file
	    Location *		parent_)///< Relative to parent directory?
	throw(std::runtime_error)
	/// \throw std::runtime_error Upon error
	/// Construct a Location for the named file relative to its parent
	:
	    name	(name_),
	    parent	(parent_),
	    depth	(parent ? 1 + parent->depth : 0),
	    st		(stat(parent ? parent->fd : -1, name)),
	    fd		(S_ISDIR(st.st_mode)
			    ? open(parent ? parent->fd : -1, name)
			    : -1),
	    dir		(-1 == fd
			    ? 0
			    : Exception::throwErrorIfEqual(
				static_cast<DIR *>(0), fdopendir(fd)))
	{}

	virtual boost::shared_ptr<char const> next()
	throw(std::runtime_error)
	/// \return The name of the next file under this one
	/// \throw std::runtime_error Upon error
	/// \brief Return the name of the next file under this one or
	/// a null pointer if there are no more.
	{
	    if (!dir) return boost::shared_ptr<char const>();
	    for (;;) {
		errno = 0;
		struct dirent * e = readdir(dir);
		if (0 == e) {
		    Exception::throwErrorIfNotEqual(0, errno);
		    return boost::shared_ptr<char const>();
		}
		if (false
			|| 0 == strcmp( ".", e->d_name)
			|| 0 == strcmp("..", e->d_name)) {
		    continue;
		}
		return boost::shared_ptr<char const>(strdup(e->d_name), free);
	    }
	}

	virtual ~Location() throw()
	/// \brief Destroy this Location
	{
	    if (dir) {
		closedir(dir);	// will close fd as well
	    } else {
		if (-1 != fd) {
		    close(fd);
		}
	    }
	}

	virtual Location const * match() const throw()
	/// \return The first parent in the location chain that is equal to this
	/// \brief Used to detect recursion induced by following symbolic links
	{
	    Location const * that;
	    for (that = parent; that && !(*this == *that); that = that->parent);
	    return that;
	}

	virtual bool operator==(Location const & that) const throw()
	/// \return True if this Location refers to the same thing as that one
	{
	    return true
		&& st.st_dev == that.st.st_dev
		&& st.st_ino == that.st.st_ino;
	}

	bool isDefined() const throw()
	/// \return True unless stat ignored an error
	/// \brief Return true unless stat ignored an error, in which case
	/// the stat struct of this Location is not defined (is all zeros).
	{
	    static struct stat const zero = {};
	    return 0 != memcmp(&zero, &st, sizeof zero);
	}
    };

    /// Like Location<> but symbolic links are followed and broken links
    /// (stat ENODEV errors) are ignored.
    /// Following symbolic links can create many paths to the same object
    /// and the base Visitor class only prunes the recursive ones.
    /// One might require a specialized Visitor to do more.
    typedef Location<
	stat_<1 << EACCES | 1 << ENOENT, true>,
	open_<1 << EACCES>
    > LocationSymLinkFollow;

    /// \brief A FindFile::Visitor object serves the context of a
    /// traverse while the class serves the mechanism.
    /// Derivations of this class should define what to do before and/or after
    /// children are visited at a location as well as what the LocationType is.
    ///
    /// The FindFile::Visitor template class be instantiated with a
    /// LocationType, which is, by default, a Location<>.
    template<class LocationType = Location<> > class Visitor {
    public:
	enum DirectionEnum {
	    Continue = 0,	///< Continue with traverse
	    Stop,		///< Stop traverse
	    Prune,		///< Prune traverse
	    Return		///< Return to root of traverse
	};

	virtual DirectionEnum traverse(
	    char const *	name,		///< Name of file
	    LocationType *	parent = 0)	///< Relative to directory?
	throw(std::runtime_error)
	/// \return Direction to go after this
	/// \throw std::runtime_error Upon error
	/// \brief Continue the traverse at the location named under the parent.
	/// If the parent is not specified then start the traverse at the
	/// absolute path name.
	{
	    LocationType location(name, parent);
	    return traverseLocation(&location);
	}

	virtual DirectionEnum traverse(
	    int			fd)		///< Descriptor to file
	throw(std::runtime_error)
	/// \return Direction to go after this
	/// \throw std::runtime_error Upon error
	/// \brief Start the traverse at the absolute path name associated
	/// with the file descriptor.
	{
	    boost::shared_ptr<char const> name(readlink(fd));
	    LocationType location(name.get(), 0);
	    return traverseLocation(&location);
	}

    protected:
	virtual DirectionEnum before(
	    LocationType *	location)	///< Location in traverse
	throw(std::runtime_error)
	/// \return Direction to go after this
	/// \throw std::runtime_error Upon error
	/// \brief Visit the location before visiting anything under it.
	/// The base implementation does nothing more than to prune the traverse
	/// if it detects recursion (induced, say, by following recursive
	/// symbolic links or filesystem mounts).
	{
	    return location->match()
		? Prune
		: Continue;
	}

	virtual DirectionEnum after(
	    LocationType *	location)	///< Location in traverse
	throw(std::runtime_error)
	/// \return Direction to go after this
	/// \throw std::runtime_error Upon error
	/// \brief Visit the location after visiting everything under it.
	/// The base implementation does nothing.
	{
	    return Continue;
	}

    private:
	DirectionEnum traverseLocation(
	    LocationType *	location)	///< Location in traverse
	throw(std::runtime_error)
	/// \return Direction to go after this
	/// \throw std::runtime_error Upon error
	/// \brief Continue the traverse at the location
	{
	    DirectionEnum direction;
	    direction = before(location);
	    if (Continue == direction) {
		do {
		    boost::shared_ptr<char const> store(location->next());
		    char const * name = store.get();
		    if (!name) break;
		    direction = traverse(name, location);
		} while (false
		    || Continue == direction
		    || Prune == direction);
	    }
	    if (Stop != direction)
		direction = after(location);
	    return direction;
	}
    };
}

#endif
