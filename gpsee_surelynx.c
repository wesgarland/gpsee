/*
 * @file	gpsee_surelynx.c	Code specific to the target GPSEE environment. Specifically
 *					created to allow open and closed
 *					gpsee projects to be identical, by abstracting configuration, 
 *					logging, and output routines.
 * @author	Wes Garland
 * @date	August 2008
 * @version	$Id: gpsee_surelynx.c,v 1.4 2010/09/01 18:12:35 wes Exp $
 * @note	This version is for SureLynx GPSEE
 *
 * $Log: gpsee_surelynx.c,v $
 * Revision 1.4  2010/09/01 18:12:35  wes
 * Sync with hg
 *
 * Revision 1.3  2010/04/28 12:44:48  wes
 * SureLynx compatibility fixes; gffi build system & hookable I/O
 *
 * Revision 1.2  2010/02/08 16:56:47  wes
 * Added gpsee_closelog()
 *
 * Revision 1.1  2009/03/30 23:55:43  wes
 * Initial Revision for GPSEE. Merges now-defunct JSEng and Open JSEng projects.
 *
 */

#define NO_SURELYNX_INT_TYPEDEFS
#define NO_APR_SURELYNX_NAMESPACE_POISONING

#include "gpsee.h"

cfgHnd cfg;

static const char __attribute__((unused)) rcsid[]="$Id: gpsee_surelynx.c,v 1.4 2010/09/01 18:12:35 wes Exp $";

#if 0  /* deprecated with new hookable I/O code */
/** 
 *  @note Source: APR 1.2.7 file_io/unix/readwrite.c/ 
 */
struct apr_file_printf_data {
    apr_vformatter_buff_t vbuff;
    apr_file_t *fptr;  
    char *buf;
};

static int file_printf_flush(apr_vformatter_buff_t *buff)
{
    struct apr_file_printf_data *data = (struct apr_file_printf_data *)buff;
 
    if (apr_file_write_full(data->fptr, data->buf,
                            data->vbuff.curpos - data->buf, NULL)) {
        return -1;
    }
 
    data->vbuff.curpos = data->buf;
    return 0;
}

int gpsee_printf(const char *format, /* args */ ...)
{
  apr_file_t	*fptr = apr_stdout;

  struct apr_file_printf_data data;
  va_list ap;
  int count;

  if (!fptr)
    return 0;

#define _GPSEE_SURELYNX_HUGE_STRING_LEN 10240
  /* don't really need a HUGE_STRING_LEN anymore */
  data.buf = malloc(_GPSEE_SURELYNX_HUGE_STRING_LEN);
  if (data.buf == NULL) {
    return -1;
  }
  data.vbuff.curpos = data.buf;
  data.vbuff.endpos = data.buf + _GPSEE_SURELYNX_HUGE_STRING_LEN;
#undef _GPSEE_SURELYNX_HUGE_STRING_LEN
  data.fptr = fptr;
  va_start(ap, format);
  count = apr_vformatter(file_printf_flush,
			 (apr_vformatter_buff_t *)&data, format, ap);
  /* apr_vformatter does not call flush for the last bits */
  if (count >= 0) file_printf_flush((apr_vformatter_buff_t *)&data);
    
  va_end(ap);
 
  free(data.buf);
     
  return count;
}
#endif

