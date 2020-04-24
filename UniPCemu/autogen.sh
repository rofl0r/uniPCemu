git update-index --assume-unchanged Makefile
aclocal || die "aclocal failed" # Set up an m4 environment
autoconf || die "autoconf failed" # Generate configure from configure.ac
echo Configure script is generated.