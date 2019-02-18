aclocal || die "aclocal failed" # Set up an m4 environment
autoconf || die "autoreconf failed" # Generate configure from configure.ac