screensaverctl(1)

# NAME

screensaverctl - control the screen saver service

# SYNOPSIS

*screensaverctl* _command_ [_argument_...]

# COMMANDS

*status*
	Show the settings and whether the service is running.

*list*
	List the screen savers installed. An asterisk marks those in use.

*on*, *off*
	Turn the screen saver on or off.

*wait* _minutes_
	Start after this long without input.

*use* _name_..., *use-all*
	Use only the screen savers named, or all of them.

*mode* *random*|*cycle*|*single*
	Pick at random, go through them in turn, or always show the first.

*change* _minutes_
	Change to another screen saver this often while idle. 0 means never.

*rest* _minutes_
	Stop the screen saver after this long, to rest the screen. 0 means never.

*lock* *on*|*off*
	Ask the session to lock when the user returns.

*test* _name_
	Run one screen saver now, until a key or a click.

*start*, *stop*
	Start or stop the screen saver now.

# SEE ALSO

*screensaverd*(1), *screensaver-settings*(1)
