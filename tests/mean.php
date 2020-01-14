<?php

// Calculates average scores for each library version
// Usage:
// php tests/mean.php [path/to/file.csv]

require_once('tests/CallTester.php');
$tester = new CallTester($argv[0], true);

$tester->printCsvScores($argv[1] ?? '');