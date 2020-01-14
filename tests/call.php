<?php

require_once('tests/CallTester.php');
$tester = new CallTester($argv[0], true);



$libraries = [
  'stable'   => 'lib/libtgvoip-stable.so',
  'unstable' => 'lib/libtgvoip-unstable.so',
];
$files = 2;
for ($i = 0; $i < $files; $i++) {
  $tester->chooseCouple(false, true);
  foreach ($libraries as $version => $path) {
    $tester
      ->library($version, $path)
      ->start()
      ->networkType('wifi')
      ->end()
      ->start()
      ->loss(25)->delay(500, 50)->networkType('3g')
      ->end()
      ->start()
      ->loss(13)->rateControl('44kbit')->networkType('hspa')
      ->end()
      ->start()
      ->loss(18)->rateControl('32kbit')->networkType('3g')
      ->end()
      ->start()
      ->loss(5)->rateControl('64kbit')->after(3)->loss(30)->rateControl('8kbit')->after(3)->rateControl('64kbit')->networkType('3g')
      ->end()
      ->start()
      ->loss(25)->rateControl('29kbit')->networkType('3g')
      ->end()
      ->start()
      ->loss(17)->rateControl('24kbit')->networkType('3g')
      ->end()
      ->start()
      ->loss(26)->rateControl('22kbit')->networkType('edge')
      ->end()
      ->start()
      ->loss(22)->rateControl('19kbit')->networkType('edge')
      ->end()
      ->start()
      ->loss(30)->rateControl('17kbit')->networkType('gprs')
      ->end()
      ->start()
      ->loss(28)->rateControl('14kbit')->networkType('gprs')
      ->end();

  }
}

