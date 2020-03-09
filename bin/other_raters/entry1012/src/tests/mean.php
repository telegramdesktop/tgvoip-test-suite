<?php
/*
 *  Daniil Gentili's submission to the VoIP contest.
 *  Copyright (C) 2019 Daniil Gentili <daniil@daniil.it>
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

$files = \glob("*rateX");
$running = 0;

foreach ($files as $fileX) {
    $fileY = \str_replace('rateX', 'rateY', $fileX);
    $X = trim(file_get_contents($fileX));
    $Y = trim(file_get_contents($fileY));
    $running += $err = max(abs($X - $Y) - 0.3, 0.0)**2;

    $min = !$err ? "(Minimum error)\t" : str_repeat(' ', 15);
    $Y = ($Y/6)*5; // Correct scale
    $fileX = str_pad($fileX, 27);
    echo "$fileX:\t error6 $err $min\t$X vs $Y".PHP_EOL;
}

$n = count($files);
$runningN = $running/$n;

echo "Total error: $running".PHP_EOL;
echo "Total error/n: {$runningN}".PHP_EOL;
