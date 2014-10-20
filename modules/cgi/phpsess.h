/** @file	phpsess.h		Prototypes and types for phpsess API users. 
 *  @author	Wes Garland
 *  @see	phpsess.c
 *
 *  $Id: phpsess.h,v 1.1 2007/01/25 15:47:43 wes Exp $
 */

typedef enum
{
  php_string 	= 's',
  php_integer 	= 'i',
  php_date 	= 'd',
  php_null 	= 'N',
} php_data_t;

typedef struct
{
  php_data_t	type;			/**< Type of data */
  union
  {
    int		integer;		/**< Data as integer if type is integer */
    const char	*string;		/**< Data as ASCIZ if type is string */
    struct
    {
      const char	*string;	/**< Data as ASCIZ if type is date */
      apr_time_t	aprTime;	/**< Data as microseconds since 1970 if type is date */
    } date;
  } data;
} php_value_t;

const char 	*phpSession_getString(apr_pool_t *pool, apr_hash_t *phpSession, const char *stringName);
apr_status_t	 phpSession_load(apr_pool_t *pool, apr_pool_t *tempPool, 
				 const char *sessionID, apr_hash_t **phpSession_p);


