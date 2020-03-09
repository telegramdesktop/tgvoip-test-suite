#!/bin/bash -e
#  Daniil Gentili's submission to the VoIP contest.
#  Copyright (C) 2019 Daniil Gentili <daniil@daniil.it>
#
#  This program is free software: you can redistribute it and/or modify
#  it under the terms of the GNU General Public License as published by
#  the Free Software Foundation, either version 3 of the License, or
#  (at your option) any later version.
#
#  This program is distributed in the hope that it will be useful,
#  but WITHOUT ANY WARRANTY; without even the implied warranty of
#  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#  GNU General Public License for more details.
#
#  You should have received a copy of the GNU General Public License
#  along with this program.  If not, see <https://www.gnu.org/licenses/>.
#

cd $(dirname $0)/../tests-output

# Generate automatic ratings for all files in background
{
    for mod in ./{1,2}mod*.ogg; do
        [ -f $mod.rateX ] && [ "$(cat $mod.rateX)" != "" ] || {
            orig=${mod/mod/orig}
            ../../tgvoiprate $orig $mod $mod.log > $mod.rateX
        }
    done
} &
pid=$!

# Generate manual ratings
rate=y
which play &>/dev/null || {
    echo "Could not find play binary, writing default rating of 6 for all files!"
    rate=n
}

for mod in ./{1,2}mod*.ogg; do
    [ -f $mod.rateY ] && [ "$(cat $mod.rateY)" != "" ] || {
        [ "$rate" == "n" ] && { echo 6 > $mod.rateY; continue; }
        orig=${mod/mod/orig}
        echo -en "\r\033[KListening to original $orig...";
        play $orig
        echo -en "\r\033[KListening to modified $mod...";
        play $mod
        echo -en "\r\033[K"
        read -rp "Please give a rating for the modified recording (0-6): " rateY
        echo "$rateY" > $mod.rateY
    }
done

wait $pid || echo "Finished!"

php ../tests/mean.php
