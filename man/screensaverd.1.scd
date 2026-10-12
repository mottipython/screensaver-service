screensaverd(1)

# NAME

screensaverd - start a screen saver when a Wayland session is idle

# SYNOPSIS

*screensaverd* [*--verbose*] [*--dry-run*] [*--version*]

# DESCRIPTION

*screensaverd* runs in a Wayland session. When there has been no input for the
time set, it starts a screen saver; when the user comes back, it stops it.

Idleness is reported by the compositor through the ext-idle-notify-v1 protocol,
so a program that asks the compositor not to go idle, such as a video player,
holds the screen saver off as well. KDE Plasma, Sway, Hyprland, niri, COSMIC and
labwc provide the protocol.

The service draws nothing itself and does not lock the screen. If asked to, it
requests a lock from the session (*loginctl lock-session*) when the user
returns, and the desktop's own locker does the rest.

It is normally run as a systemd user service:

	systemctl --user enable --now screensaverd

# OPTIONS

*--verbose*
	Say what is being started and stopped, on standard error.

*--dry-run*
	Say what would be started, but start nothing.

*--version*
	Print the version and exit.

# SCREEN SAVERS

Two kinds are run.

A _standalone_ screen saver is a program that goes full screen by itself. It is
described by a desktop entry in _screensaver-service/savers/_ under any XDG data
directory. Besides *Name*, *Comment*, *Exec* and *TryExec*, these keys are read:

*X-Screensaver-Standalone=true*
	The program makes its own window. This is the default in that directory.

*X-Screensaver-Snapshot=*_command_
	A command that writes a still picture, for the settings window. *%o* is
	replaced by the file to write, *%w* and *%h* by its width and height.

*X-Screensaver-Settings=*_command_
	A command that opens the screen saver's own options.

An _X11 hack_ in the xscreensaver tradition draws into a window it is given. It
is described by a desktop entry in _applications/screensavers/_, where the
xscreensaver packages put theirs. The service makes a full-screen window on each
monitor through Xwayland and passes it in *XSCREENSAVER_WINDOW*.

# FILES

_$XDG_CONFIG_HOME/screensaver-service/config_
	The settings. See *screensaverctl*(1).

_$XDG_RUNTIME_DIR/screensaverd.pid_
	The process ID of the running service.

# SIGNALS

*SIGHUP* reads the settings again. *SIGUSR1* starts the screen saver now.
*SIGUSR2* stops it.

# SEE ALSO

*screensaverctl*(1), *screensaver-settings*(1)
