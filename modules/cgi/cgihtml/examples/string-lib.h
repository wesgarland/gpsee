/* string-lib.h - headers for string-lib.c
   $Id: string-lib.h,v 1.1 2009/03/31 12:04:38 wes Exp $
*/

char *newstr(char *str);
char *substr(char *str, int offset, int len);
char *replace_ltgt(char *str);
char *lower_case(char *buffer);
