#!/bin/sh

OBJECT="iPwnder32"

ARCH=""

FLAGS=""
#FLAGS="-DHAVE_DEBUG -DHAVE_HOOKER"
#FLAGS="-DHAVE_DEBUG -DHAVE_HOOKER -DIPHONEOS_ARM"

readonly CC="clang"

readonly C_SRC="\
ipwnder32.c \
ircv.c \
checkm8.c \
payload.c \
limera1n.c \
partial.c \
boot.c\
"

readonly FRAMEWORK="\
-framework IOKit \
-framework CoreFoundation\
"

readonly INCLUDE="\
-I./include\
"

readonly DYLIB="\
-lcurl \
-lz\
"

function usage {
  cat <<EOM
Usage: $(basename "$0") [option]
  --intel       Make for Intel Mac (x86_64)
  --M1          Make for M1 Mac (arm64)
  --universal   Make for both x86_64 and arm64
  --help        Show Usage
EOM
  exit 1
}

function make {
  echo ""$CC" "$C_SRC" "$INCLUDE" "$FRAMEWORK" "$DYLIB" "$FLAGS" "$3" -o "$1" -arch "$2""
  $CC $C_SRC $INCLUDE $FRAMEWORK $DYLIB $FLAGS $3 -o $1 -arch $2
  strip $1
}

if [ $# -lt 1 ]; then
  usage
fi

case "$1" in
  '--intel')
    ARCH="x86_64"
    ;;
  '--M1')
    ARCH="arm64"
    ;;
  '--universal')
    ARCH="universal"
    ;;
  *)
    usage
    ;;
esac

if [ $ARCH != "universal" ]; then
STATICLIB="static/macosx/"$ARCH"/iBoot32patcher.a"

  make $OBJECT $ARCH $STATICLIB
  exit
fi

if [ $ARCH == "universal" ]; then
ARCH1="x86_64"
STATICLIB1="static/macosx/"$ARCH1"/iBoot32patcher.a"
OBJECT1="iPwnder32_"$ARCH1""

ARCH2="arm64"
STATICLIB2="static/macosx/"$ARCH2"/iBoot32patcher.a"
OBJECT2="iPwnder32_"$ARCH2""

  make $OBJECT1 $ARCH1 $STATICLIB1
  make $OBJECT2 $ARCH2 $STATICLIB2

  echo "lipo -create -output "$OBJECT" -arch "$ARCH1" "$OBJECT1" -arch "$ARCH2" "$OBJECT2""
  lipo -create -output $OBJECT -arch $ARCH1 $OBJECT1 -arch $ARCH2 $OBJECT2
  exit
fi

usage
exit
