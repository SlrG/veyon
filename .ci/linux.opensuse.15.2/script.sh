#!/usr/bin/env bash

set -e

$1/.ci/common/linux-build.sh $@
$1/.ci/common/finalize-rpm.sh $@ "opensuse-15.2"
