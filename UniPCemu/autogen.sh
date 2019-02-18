aclocal || die "aclocal failed" # Set up an m4 environment
autoconf || die "autoconf failed" # Generate configure from configure.ac