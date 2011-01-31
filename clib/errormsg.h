/*
errormsg.h:

Copyright (c) 1999 Marcel R. van der Goot
All rights reserved.

This file is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This file is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this file.  If not, see <http://www.gnu.org/licenses/>.

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
