#!/bin/sh

$XGETTEXT `find . -name \*.cpp -o -name \*.h` -o $podir/kio_snapshot.pot
