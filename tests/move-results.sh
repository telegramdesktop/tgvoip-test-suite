#!/bin/bash -x

RESULTS_PATH="$1"

function die
{
    local message=$1
    [ -z "$message" ] && message="Died"
    echo "$message at ${BASH_SOURCE[1]} line ${BASH_LINENO[0]}." >&2
    exit 1
}


if [ -d $RESULTS_PATH ]; then
  die "Directory $RESULTS_PATH already exists";
fi

mkdir -p $RESULTS_PATH || die "Can't create results directory $RESULTS_PATH.";

mv out preprocessed nohup.out $RESULTS_PATH


mkdir out
mkdir preprocessed

echo "Results were moved to directory:"
echo $RESULTS_PATH

