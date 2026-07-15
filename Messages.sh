#!/bin/sh

# SPDX-FileCopyrightText: 2026 Bharadwaj Raju <bharadwaj.raju@machinesoul.in>
# SPDX-License-Identifier: LGPL-2.0-or-later

$XGETTEXT `find . -name \*.cpp -o -name \*.h` -o $podir/kio_snapshot.pot
