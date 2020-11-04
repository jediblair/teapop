#!/bin/sh
#
# teapop        Starts teapop POP Server
#
#
# chkconfig: 345 85 15
# description: Teapop is a pop3 server

# Source function library.
. /etc/init.d/functions

RETVAL=0

start() {
 	echo -n "Starting teapop server: "
	# we don't want the MARK ticks
	daemon teapop -s
	RETVAL=$?
	echo
	[ $RETVAL = 0 ] && touch /var/lock/subsys/teapop
	return $RETVAL
}	
stop() {
	echo -n "Shutting down teapop server: "
	killproc teapop
	echo
	[ $RETVAL -eq 0 ] && rm -f /var/lock/subsys/teapop
	return $RETVAL
}

case "$1" in
  start)
  	start
	;;
  stop)
  	stop
	;;
  status)
  	status teapop
	;;
  restart|reload)
	stop
  	start
	;;
  *)
	echo "Usage: teapop {start|stop|status|restart}"
	exit 1
esac

exit $?

