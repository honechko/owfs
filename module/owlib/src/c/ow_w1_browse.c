/*
$Id$
    OWFS -- One-Wire filesystem
    OWHTTPD -- One-Wire Web Server
    Written 2003 Paul H Alfille
	email: palfille@earthlink.net
	Released under the GPL
	See the header file: ow.h for full attribution
	1wire/iButton system from Dallas Semiconductor
*/

/* w1 browse for existing and new bus masters
 * w1 is different: it is a dynamic list of adapters
 * the scanning starts with "W1_Browse" in LibStart and continues in it's own thread 
 */

#include <config.h>
#include "owfs_config.h"

#if OW_W1

#include "ow_w1.h"
#include "ow_connection.h"

static void W1Clear( void ) ;
static void W1SysList( const char * directory ) ;

// Remove stale connections
static void W1Clear( void )
{
	struct connection_in * in ;

	CONNIN_WLOCK ;

	// check for w1 bus masters that weren't found
	for ( in = Inbound_Control.head ; in ; in = in->next ) {
		if ( in->busmode == bus_w1
			&& in->connin.w1.entry_mark  != Inbound_Control.w1_entry_mark
			) {
			LEVEL_DEBUG("w1 bus <%s> no longer found",in->name) ;
			RemoveIn( in ) ;
		}
	}

	CONNIN_WUNLOCK ;
}

static void W1SysList( const char * directory )
{
	DIR * sys_w1  = opendir(directory) ;

	if ( sys_w1 != NULL ) {
		struct dirent * dent ;

		while ( (dent = readdir( sys_w1)) != NULL ) {
			if ( strncasecmp( "w1", dent->d_name, 2 ) == 0 ) {
				int bus_master ;
				//printf("About to scan %s\n",dent->d_name);
				if ( sscanf( dent->d_name, "w1_bus_master%d", &bus_master) == 1 ) {
					AddW1Bus( bus_master ) ;
				} else {
					ERROR_DEBUG("Can't interpret bus number in sysfs entry %s/%s",directory,dent->d_name);
				}
			}
		}

		closedir( sys_w1 ) ;
	}
}

#if OW_MT

static void * W1_start_scan( void * v )
{
	(void) v ;

	pthread_detach(pthread_self());

	if ( FILE_DESCRIPTOR_NOT_VALID(Inbound_Control.w1_file_descriptor) ) {
		LEVEL_DEBUG("Cannot monitor w1 bus, No netlink connection.");
	} else {
		W1NLScan() ;
	}
	LEVEL_DEBUG("Normal exit.\n");
	return NULL ;
}

int W1_Browse( void )
{
    pthread_t thread_scan ;
    pthread_t thread_dispatch ;

    ++Inbound_Control.w1_entry_mark ;
    LEVEL_DEBUG("Calling for netlink w1 list");

    // Initial setup
    my_pthread_mutex_init(&(Inbound_Control.w1_mutex), Mutex.pmattr);
    my_pthread_mutex_init(&(Inbound_Control.w1_read_mutex), Mutex.pmattr);
    gettimeofday(&Inbound_Control.w1_last_read,NULL);
    ++Inbound_Control.w1_last_read.tv_sec ;

    if ( FILE_DESCRIPTOR_NOT_VALID(w1_bind()) ) {
        ERROR_DEBUG("Netlink problem -- are you root?");
        return -1 ;
    }

    if ( pthread_create(&thread_dispatch, NULL, W1_Dispatch, NULL) != 0 ) {
        ERROR_DEBUG("Couldn't create netlink monitoring thread");
        return -1 ;
    }

    if ( W1NLList() != nrs_complete ) {
 		LEVEL_DEBUG("Drop down to sysfs w1 list");
		W1SysList("/sys/bus/w1/devices") ;
	}

	// And clear deadwood
	W1Clear() ;

	return pthread_create(&thread_scan, NULL, W1_start_scan, NULL);
}

#else /* OW_MT */
int W1_Browse( void )
{
	LEVEL_CONNECT("Dynamic w1 support requires multithreading (a compile-time option");
	// Initial setup
	W1SysScan("/sys/bus/w1/devices") ;
}
#endif /* OW_MT */

#endif /* OW_W1 */
