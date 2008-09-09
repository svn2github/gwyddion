# Module name, the name of the .so file, it is also defined as a preprocessor
# macro you are encouraged to use for module registering.
PACKAGE = threshold-example
# Module version, it is also defined as a preprocessor macro you are
# encouraged to use for module registering.
VERSION = 2.0
# Module type (one of process, file)
MODULE_TYPE = process
# Module source files
SOURCES = $(PACKAGE).c
# Module header files, if any
HEADERS =
# Extra files to distribute (README, ...)
EXTRA_DIST = README

