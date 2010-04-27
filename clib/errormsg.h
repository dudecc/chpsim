/*
errormsg.h:

Copyright (c) 1999 Marcel R. van der Goot
All rights reserved.

This file is only for use as part of software written by
Marcel van der Goot. Any other use requires a separate license.
This file contains confidential information, and should not be
redistributed to third parties.

*/

#ifndef ERRORMSG_H
#define ERRORMSG_H

extern const char *application_nm;
  /* name used in error/warning messages. */

extern void error(const char *fmt, ...);
 /* Error message. "Error ...\n". Abort. */

extern void warning(const char *fmt, ...);
 /* Warning message: "Warning ...\n". */


#endif /* ERRORMSG_H */
