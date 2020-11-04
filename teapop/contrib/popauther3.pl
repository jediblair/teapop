#!/usr/bin/perl
# - Makes a Fifo called /var/adm/popauther
# - reads from that Fifo all POP sessions from Syslog.
# - Puts the IPs in /var/spool/popauth/
# - Removes "expired" IPs from /var/sopol/popauth
# - Automatically rebuilds POP-authorized IP list
#
# with version 1.3-3 of syslog you can add an entry in your /etc/syslog.conf
# local0.info                                     |/var/adm/popauther
# which will cause all local0 syslog messages of priority info or greater to 
# be sent thru the fifo.
#
# Original code from: William R. Thomas <wthomas@poweruser.com>
# This version is from: Harlan Stenn <harlan@pfcs.com>
# Added support for teapop: Ivan F. Martinez <ivanfm@os2brasil.com.br>
# 
$fifo = "/var/spool/popauther";
$popauthspool = "/var/spool/popauth/";
$watcherlog = "/var/log/popauth.watcher.log";
$popwatcherpidfile = "/var/run/popauth.watcher.pid";
$secondstoallow = 15 * 60;	# 30 minutes before permission expires
$minwakeupin = 5 * 60;		# 5 minute minimum wakeup time
$maptype = "hash";

{
  system("mkdir -p $popauthspool");
  while(1) {
    unless( -p $fifo)
      {
	unlink $fifo;
	system("mkfifo $fifo") && die "Can't mkfifo $fifo: $!";
	chmod 0600, $fifo;
      }
    open(FIFO, "< $fifo") || die "Can't open $fifo: $!";

    open(LOG,">>$watcherlog") || die("Can't open $watcherlog");
    select(LOG);
    $| = 1;

    print LOG "\n";
    print LOG &tstamp." Starting log for popauth.watcher at pid $$\n";

    select(STDOUT);
    $| = 1;

    $SIG{'INT'} = 'exithandler';
    $SIG{'QUIT'} = 'exithandler';
    $SIG{'KILL'} = 'exithandler';

    open(PID,">$popwatcherpidfile");
    print PID "$$\n";
    close(PID);

    $nextwakeup = time;
    $rebuild = 1;
    while(1)
      {
	$rin = "";
	vec($rin, fileno(FIFO), 1) = 1;
	my $now = time;

	my $wakeupat = ($nextwakeup <= $now) ? $now : $nextwakeup;
	my $wakeupin = $wakeupat - $now;
	# warn "nextwakeup: $nextwakeup\n";
	# warn "now:        $now\n";
	# warn "wakeupat:   $wakeupat\n";
	# warn "wakeupin:   $wakeupin\n";
	$nfound = select($rout=$rin, undef, undef, $wakeupin);
	# warn "Select found $nfound ready descriptors\n";

	if ($nfound)
	  {
	    $rebuild += add_new();
	  }

	if ($nextwakeup <= time)
	  {
	    # warn "Somebody expired. nextwakeup is $nextwakeup\n";
	    ($nextwakeup, $changed) = scan_old();
	    # warn "After scan_old, nextwakeup is $nextwakeup\n";
	    $rebuild += $changed;
	  }

	rebuild() if $rebuild;
	$rebuild = 0;
      }
    close(LOG);
  }
  exit(1);
}

sub add_new
  {
    my $rebuild = 0;
    my $good = 0;

    $_ = <FIFO>;
    chomp;

    # warn "add_new: Checking <$_>\n";

    # imap-4.1.BETA, with HMS's ip number logging hacks
    if(!$good && /^([A-Za-z]+\s+\d+\s\d+\:\d+\:\d+)\s\w+\sipop[23]d\[\d+\]\: Login user=([a-z0-9]{2,8}) host=\[(\d+\.\d+\.\d+\.\d+)\].+$/)
      {
	# warn "add_new: imap-4.1.BETA\n";
	$tstamp = $1;
	$user = $2;
	$ip = $3;
	++$good;
      }

    # cucipop
    if(!$good && /^([A-Za-z]+\s+\d+\s\d+\:\d+\:\d+)\s\w+\scucipop\[\d+\]\: ([a-z0-9]{2,8}) (\d+\.\d+\.\d+\.\d+),\s+.+$/)
      {
	# warn "add_new: cucipop\n";
	$tstamp = $1;
	$user = $2;
	$ip = $3;
	# These lines probably skip "authentication faulure" and "lost (user)"
	# messages from CuciPopper.
	goto add_new_exit if ($user =~ /authenti/);
	goto add_new_exit if ($user =~ /lost/);
	++$good;
      }

    # teapop
    # Jun 15 13:16:47 nbifm teapop[17844]: Successful login for user@domain [127.0.0.1] from localhost [127.0.0.1]
    if(!$good && /^([A-Za-z]+\s+\d+\s\d+\:\d+\:\d+)\s\w+\steapop\[\d+\]\: Successful login for ([a-z0-9@\.]+) \[(\d+\.\d+\.\d+\.\d+)\] from ([a-z0-9\.]+) \[(\d+\.\d+\.\d+\.\d+).$*/)
      {
	# warn "add_new: teapop \n";
	$tstamp = $1;
	$user = $2;
	$ip = $5;
	++$good;
      }

    # popper-1.831 + HMS's log "Authenticated" patch.
    # Apr 23 16:26:11 rs6a popper: (tbarney,192.156.252.66) Authenticated.
    if(!$good && /^([A-Za-z]+\s+\d+\s\d+\:\d+\:\d+)\s\w+\spopper: \(([a-z0-9]{2,8}),(\d+\.\d+\.\d+\.\d+)\) Authenticated\..*$/)
      {
	# warn "add_new: Whatever...\n";
	$tstamp = $1;
	$user = $2;
	$ip = $3;
	++$good;
      }

    if ($good)
      {
	++$rebuild;
	print LOG "$tstamp $user authenticating relaying for $ip\n" ;
	my $file = ">".$popauthspool.$ip;
	open(TEMP,$file);
	print TEMP "$user\n";
	close(TEMP);
      }
    add_new_exit:
      # warn "add_new: returning $rebuild\n";
      return $rebuild;
  }

sub scan_old
  {
    my $now = time;
    my $next = $now + $minwakeupin;
    my $changed = 0;

    opendir(DIR2,$popauthspool);
    my @dir2 = grep !/^\.\.?$/, readdir(DIR2);
    closedir(DIR2);
    foreach $file (@dir2)
      {
	my $mtime = (stat($popauthspool.$file))[8];
	my $exptime = $mtime + $secondstoallow;
	if( $exptime <= $now )
	  {
	    print LOG &tstamp." removing authentication for relay from $file\n";
	    unlink($popauthspool.$file);
	    ++$changed;
	  }
	else
	  {
	    $next = $exptime if ($exptime < $next);
	  }
      }
    return ( $next, $changed );
  }

sub rebuild
  {
    system("mv /etc/mail/popauth /etc/mail/popauth-old");
    opendir(DIR, $popauthspool);
    my @dir = grep !/^\.\.?$/, readdir(DIR);
    closedir(DIR);

    open(POPAUTH, ">/etc/mail/popauth");
    foreach $_ (@dir)
      {
	if(/^\d+\.\d+\.\d+\.\d+$/)
	  {
	    print POPAUTH "$_\tOK\n";
	  }
	else
	  {
	    print LOG &tstamp." rebuild: Unrecognized file: <$popauthspool$_>\n";
	  }
      }
    close POPAUTH;

    sleep 2;			# HMS: Why is this needed?  (Was 5 seconds)
    system("makemap $maptype /etc/mail/popauth < /etc/mail/popauth");
  }

sub tstamp
  {
    use POSIX qw(strftime);

    return POSIX::strftime("%b %d %H:%M:%S", localtime(time));
  }

sub exithandler
  {
    local($sig) = @_;
    close(POPPER);
    close(LOG);
    exit(0);
  }
