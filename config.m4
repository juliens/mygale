PHP_ARG_ENABLE(mygale, whether to enable mygale support,
[ --enable-mygale   Enable mygale support])

if test "$PHP_MYGALE" = "yes"; then
  AC_DEFINE(HAVE_MYGALE, 1, [Mygale])
  PHP_NEW_EXTENSION(mygale, mygale.c, $ext_shared)
fi
