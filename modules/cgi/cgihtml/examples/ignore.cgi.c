/* ignore.cgi.c - sends a status of 204; use it in imagemaps to ignore
     clicks in default areas.

   Eugene Kim, eekim@fas.harvard.edu
   $Id: ignore.cgi.c,v 1.1 2009/03/31 12:04:38 wes Exp $

   Copyright (C) 1996 Eugene Eric Kim
   All Rights Reserved
*/

#include "html-lib.h"

int main() {
  status("204 nada");
  html_header();
  exit(0);
}

