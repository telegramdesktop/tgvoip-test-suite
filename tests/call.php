<?php

require_once('tests/CallTester.php');
// To use between two hosts, copy the folder and use CallRemoteTester class.
// SSH connection with ControlMaster is highly recommended
// $tester = new CallRemoteTester('10.0.0.2', 'tgvoip-test-suite', $argv[0], true);
$tester = new CallTester($argv[0], true);



$libraries = [
  'stable'            => 'lib/libtgvoip-stable.so',
  'unstable-2.5'      => 'lib/libtgvoip-unstable.so',
  'unstable-2.6'      => 'lib/libtgvoip-unstable-2.6.so',
];
$files = 2;
for ($i = 0; $i < $files; $i++) {
  $tester->chooseCouple(false, ($i % 2) == 0);
  foreach ($libraries as $version => $path) {
    $tester
      ->library($version, $path)
      ->start()
      ->networkType('wifi')->networkAlias('WiFi')
      ->end();
    // continue;
    $tester
      ->start()
      ->loss(9, 20)->rateControl('44kbit')->networkType('hspa')->networkAlias('3G1')
      ->end()
      ->start()
      ->loss(17)->rateControl('29kbit')->networkType('3g')->networkAlias('3G2')
      ->end()
      ->start()
      ->loss(12, 3)->rateControl('32kbit')->networkType('3g')->networkAlias('3G3')
      ->end()
      ->start()
      ->loss(18)->rateControl('32kbit')->networkType('3g')->networkAlias('3G4')
      ->end()
      ->start()
      ->loss(17, 5)->delay(500, 50)->networkType('3g')->networkAlias('3GDelay')
      ->end()
      ->start()
      ->loss(3, 10)->rateControl('64kbit')->after(3)->loss(20)->rateControl('8kbit')->after(3)->rateControl('64kbit')->networkType('3g')->networkAlias('3GOutage')
      ->end()
      ->start()
      ->loss(11)->rateControl('24kbit')->networkType('3g')->networkAlias('EDGE1')
      ->end()
      ->start()
      ->loss(15, 5)->rateControl('19kbit')->networkType('edge')->networkAlias('EDGE2')
      ->end()
      ->start()
      ->loss(20, 5)->rateControl('17kbit')->networkType('gprs')->networkAlias('GPRS1')
      ->end()
      ->start()
      ->loss(19, 5)->rateControl('14kbit')->networkType('gprs')->networkAlias('GPRS2')
      ->end()
      ->start()
      ->loss(40, 5)->delay(500, 50)->rateControl('8kbit')->networkType('gprs')->networkAlias('GPRS3')
      ->end();

  }
}

