#
# Regular cron jobs for the skdownloader package
#
0 4	* * *	root	[ -x /usr/bin/skdownloader_maintenance ] && /usr/bin/skdownloader_maintenance
