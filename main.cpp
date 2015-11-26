/// \file
/// Definition of the main function.
/// <p>
/// Copyright (c) 2009 Ross Tyler.
/// This file may be copied under the terms of the
/// GNU Lesser General Public License (LGPL).
/// See COPYING file for details.

#include <iostream>
#include <stdexcept>

#include <gst/gst.h>

#include "GstFs.h"

/// The main function.
int main(int argc, char ** argv, char ** envv) throw() {
    try {
	gst_init(0, 0);
	GstFs gstFs(argc, argv);
	return gstFs.main();
    } catch (std::exception & e) {
	std::cerr << e.what() << std::endl;
	return -1;
    }
}
