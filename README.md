c-macos-keylogger
=================

Simple keylogger written in C for macOS.

The objective of this is to build a corpus of my actual keystrokes for
improving my keyboard layout. I plan to analyze the resulting file once I've
logged enough of my work.

Output uses Mac-style sigils like &#x2318;. Keys are show in the same line if
not much time has passed since the last key was entered. As of now, this is
hardcoded to be 500 milliseconds.

While there are layouts optimized for English or programming/"Proglish" based
on available text or code, those are observations on the finished state.
Getting there requires Vim keybindings, editor hotkeys, command line aliases,
etc., none of which can be seen in the code itself.

Usage
-----

You should be able to build this in macOS with only system dependencies:

```bash
git clone https://github.com/brymck/c-macos-keylogger.git
cd c-macos-keylogger
make
```

Note that root privileges are required to run a keylogger. You'll also need to
go to System Settings > Privacy & Security > Accessibility and enable whichever
terminal application you'll use to run this tool.

You can then log keys with one of the following:

```
# Stdout
sudo ./macos-keylogger
# File
sudo ./macos-keylogger >> ~/corpus.txt
# Both
sudo ./macos-keylogger | tee -a ~/corpus.txt
```
