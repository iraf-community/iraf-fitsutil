#!/bin/sh

set -e

patch -b -s -p1 <<EOF
--- b./wcsutil.c
+++ a./wcsutil.c
@@ -81,8 +81,9 @@ int ffwldp(double xpix, double ypix, double xref, double yref,
       dect = dec0 + m;

     } else if (*cptr == 'T') {  /* -TAN */
-      if (*(cptr + 1) != 'A' ||  *(cptr + 2) != 'N') {
-         return(*status = 504);
+      if ( !(*(cptr + 1) == 'A' && *(cptr + 2) == 'N') &&
+           !(*(cptr + 1) == 'P' && *(cptr + 2) == 'V') ) {
+               return(*status = 504);
       }
       x = cos0*cos(ra0) - l*sin(ra0) - m*cos(ra0)*sin0;
       y = cos0*sin(ra0) + l*cos(ra0) - m*sin(ra0)*sin0;

EOF
