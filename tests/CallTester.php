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

class CallTester {
  /**
   * API Token.
   *
   * @var string
   */
  private $token = '';
  /**
   * Input audio files.
   *
   * @var array
   */
  private $files = [];
  /**
   * Whether we're currently preparing a testing session.
   *
   * @var boolean
   */
  private $testing = false;

  /**
   * Test directory.
   *
   * @var string
   */
  private $testdir = '';

  /**
   * File to send by caller
   *
   * @var string
   */
  private $fileCaller = '';

  /**
   * File to send by callee
   *
   * @var string
   */
  private $fileCallee = '';

  /**
   * Queued netem commands.
   *
   * @var array
   */
  private $commands = [];

  /**
   * Extra command.
   *
   * @var string
   */
  private $extraCmd = '';
  /**
   * Extra command (undo).
   *
   * @var string
   */
  private $extraCmdUndo = '';
  /**
   * Batches of commands separated by sleeps
   *
   * @var array
   */
  private $netemSequence = [];
  /**
   * Name of network interface.
   *
   * @var string
   */
  private $dev;
  /**
   * Network type
   *
   * @var int
   */
  private $net;

  /**
   * CSV file write descriptor
   */
  private $csvFD = false;

  /**
   * Name of testing session.
   *
   * @var string
   */
  private $name = '';
  /**
   * Constructor.
   *
   * @param string $me    $argv[0], path of current script
   * @param bool $verbose true for verbose output
   * @param string $dev   Name of network interface
   */
  public function __construct(string $me, bool $verbose = false, string $dev = 'v-peer1') {
    if ($me[0] !== '/') {
      $me = \getcwd()."/".$me;
    }
    $me = \dirname($me);

    $this->verbose = $verbose;

    if (!$dev) {
      preg_match("/default via .* dev (\w+)/", $this->execSync('ip route') , $matches);
      $dev = $matches[1];
    }
    $this->token   = require 'token.php'; // <?php return 'token';
    $this->files   = \glob($me.'/../samples/*');
    $this->testdir = $me.'/../';
    $this->dev     = $dev;
  }

  public function __destruct() {
    if ($this->csvFD !== false) {
      fclose($this->csvFD);
      $this->csvFD = false;

      $this->printCsvScores();
    }
  }
  /**
   * Fetch VoIP params from API.
   *
   * @return array
   */
  private function fetchParams(): array {
    $random = \bin2hex(\random_bytes(64));
    $result = \json_decode(\file_get_contents("https://api.contest.com/voip{$this->token}/getConnection?call={$random}") , true);

    if (!($result['ok']??false)) {
      throw new \Exception("Error while retrieving API result");
    }

    return $result['result'];
  }
  /**
   * Start testing session.
   *
   * @return self
   */
  public function start(): self {
    if ($this->testing) {
      throw new \Exception('Currently creating testing session, cannot create another');
    }
    $this->testing = true;

    $this->net           = null;
    $this->name          = '';
    $this->netemSequence = [];
    $this->commands      = [];
    $this->extraCmd      = '';
    $this->extraCmdUndo  = '';

    return $this;
  }
  /**
   * Set network type.
   *
   * @return self
   */
  public function networkType($net_str = 'wifi'): self {
    static $nets = [
      'gprs'             => 1,
      'edge'             => 2,
      '3g'               => 3,
      'hspa'             => 4,
      'lte'              => 5,
      'wifi'             => 6,
      'ethernet'         => 7,
      'other_high_speed' => 8,
      'other_low_speed'  => 9,
      'dialup'           => 10,
      'other_mobile'     => 11,
    ];
    if (!isset($nets[$net_str])) {
      throw new \Exception("Unknown network type: ".\json_encode($net_str));
    }
    $this->name .= "net{$net_str},";
    $this->net = $nets[$net_str];
    return $this;
  }
  /**
   * Set rate control for connection.
   *
   * @param string $rate Rate
   *
   * @return self
   */
  public function rateControl($rate = '10kbit'): self {
    $this->name .= "rate{$rate},";
    //$this->commands []= "handle 1:0";
    $commands = \implode(' ', $this->commands);
    $this->commands = [];

    $this->extraCmd = "tc qdisc add dev {$this->dev} root handle 1:0 netem $commands && ".
                      "tc qdisc add dev {$this->dev} parent 1:1 handle 10: tbf rate $rate buffer 1600 limit 3000";
    $this->extraCmdUndo = "tc qdisc del dev {$this->dev} root";
    return $this;
  }
  /**
   * Emulate packet loss.
   *
   * @param float  $loss  Packet loss percentage
   * @param string $correlation Probability for loss bursts
   *
   * @return self
   */
  public function loss(float $loss = 30, float $correlation = 0): self {
    $this->name .= "loss{$loss}-{$correlation},";
    $this->commands[] = "loss $loss% $correlation%";
    return $this;
  }
  /**
   * Emulate packet delay $delay ms with $jitter ms and $correlation %.
   *
   * @param int   $delay
   * @param int   $jitter
   * @param int   $correlation
   *
   * @return self
   */
  public function delay(int $delay = 300, int $jitter = 10, $correlation = 5): self {
    $this->name .= "delay{$delay}-{$jitter}-{$correlation},";
    $this->commands[] = "delay {$delay}ms {$jitter}ms {$correlation}% distribution normal";
    return $this;
  }

  /**
   * Emulate packet reordering.
   *
   * @param float $reorder
   * @param float $correlation
   *
   * @return self
   */
  public function reordering(float $reorder = 30, float $correlation = 25): self {
    if (strpos($this->name, 'delay') === false) {
      throw new \Exception("Reordering without delay specified");
    }
    $this->name .= "reorder{$reorder}-{$correlation},";
    $this->commands[] = "reorder $reorder% $correlation%";
    return $this;
  }
  /**
   * Emulate packet duplication.
   *
   * @param float  $duplication  Packet duplication percentage
   * @param string $correlation Probability for duplication bursts
   *
   * @return self
   */
  public function duplication(float $duplication = 30, float $correlation = 0): self {
    $this->name .= "dup{$duplication}-{$correlation},";
    $this->commands[] = "duplicate $duplication% $correlation%";
    return $this;
  }
  /**
   * Reset network after some time.
   *
   * @param float $delay seconds of delay
   *
   * @return self
   */
  public function after(float $delay): self {
    $this->name .= "after{$delay},";
    $this->netemSequence[] = [
      'commands'     => $this->commands,
      'extraCmd'     => $this->extraCmd,
      'extraCmdUndo' => $this->extraCmdUndo,
      'sleepAfter'   => $delay,
    ];

    $this->commands     = [];
    $this->extraCmd     = '';
    $this->extraCmdUndo = '';
    return $this;
  }

  /**
   * End sequence, launch test.
   *
   * @return self
   */
  public function end(): self {
    $this->testing = false;
    $result = $this->fetchParams();
    $config = \json_encode($result['config'], JSON_PRETTY_PRINT);
    $key = $result['encryption_key'];
    $endpoint = $result['endpoints'][0];
    $ipPort = "{$endpoint['ip']}:{$endpoint['port']}";
    $callerTag = $endpoint['peer_tags']['caller'];
    $calleeTag = $endpoint['peer_tags']['callee'];
    $netOption = $this->net ? " -n {$this->net}" : '';
    $ldPreload = "LD_PRELOAD={$this->libraryPath} ";

    $name = \trim($this->name, ',');
    if (!strlen($this->fileCaller) || !strlen($this->fileCallee)) {
      $this->chooseCouple();
    }
    $fileCaller = $this->fileCaller;
    $fileCallee = $this->fileCallee;

    $fileNameCaller = basename($fileCaller);
    $fileNameCallee = basename($fileCallee);

    $netnsPrefix = $this->netnsPrefix(true);

    $iteration = mt_rand(1000000, 9999999);

    $outDir = $this->testdir.'out/';
    $preprocessedDir = $this->testdir.'preprocessed/';
    $configDir = $this->testdir.'out/';

    $configPath = $configDir.'config.json';

    $callerPreprocessedPath = "{$preprocessedDir}{$this->libraryVersion}_{$fileNameCaller}_{$name}_{$iteration}.pcm";
    $callerOutPath = "{$outDir}{$this->libraryVersion}_{$fileNameCallee}_{$name}_{$iteration}.pcm";
    $callerCommand = "{$netnsPrefix} {$ldPreload} bin/tgvoipcall {$ipPort} {$callerTag} -k {$key} -i {$fileCaller} -p {$callerPreprocessedPath} -o {$callerOutPath} -c {$configPath} -r caller {$netOption} > {$callerOutPath}.log 2>&1";

    $calleePreprocessedPath = "{$preprocessedDir}{$this->libraryVersion}_{$fileNameCallee}_{$name}_{$iteration}.pcm";
    $calleeOutPath = "{$outDir}{$this->libraryVersion}_{$fileNameCaller}_{$name}_{$iteration}.pcm";
    $calleeCommand = "{$ldPreload} bin/tgvoipcall {$ipPort} {$calleeTag} -k {$key} -i {$fileCallee} -p {$calleePreprocessedPath} -o {$calleeOutPath} -c {$configPath} -r callee {$netOption} > {$calleeOutPath}.log 2>&1";

    \file_put_contents($configPath, $config);

    $this->netemSequence[] = ['commands' => $this->commands, 'extraCmd' => $this->extraCmd, 'extraCmdUndo' => $this->extraCmdUndo, ];

    $curNetem = array_shift($this->netemSequence);

    $this->applyNetem($curNetem);

    $caller_pid = $this->execBackground($callerCommand);
    $callee_pid = $this->execBackground($calleeCommand);

    while ($nextNetem = array_shift($this->netemSequence)) {
      usleep($curNetem['sleepAfter'] * 1000000);
      $this->undoNetem($curNetem);
      $curNetem = $nextNetem;
      $this->applyNetem($curNetem);
    }

    $this->waitBackground($caller_pid);
    $this->waitBackground($callee_pid);

    $this->undoNetem($curNetem);

    $csvFD = $this->getCsvWriteFD();
    $commandShort = "bin/tgvoiprate {$fileCaller} {$calleeOutPath} 2>> {$outDir}rate_errors.log";
    $shortRating = $this->execSync($commandShort);
    $shortRating = trim($shortRating);
    $commandFull = "bin/tgvoiprate {$fileCaller} {$callerPreprocessedPath} {$calleeOutPath} 2>> {$outDir}rate_errors.log";
    $fullRating = $this->execSync($commandFull);
    list($fullRatingPrep, $fullRatingOut) = explode(' ', trim($fullRating), 2);

    fputcsv($csvFD, [
      $this->libraryVersion,
      $fileNameCaller,
      $name,
      basename($calleeOutPath),
      $fullRatingPrep,
      $fullRatingOut,
      $shortRating
    ]);

    return $this;
  }

  /**
   * Choose 2 files for caller and callee
   *
   * @param bool $short if true, prefer short audios up to 8 seconds instead of up to 18 seconds
   * @param bool $oneway if true, callee always replies with silence instead of speach
   *
   * @return self
   */
  public function chooseCouple($short = false, $oneway = false): self {
    do {
      $fileCaller = $this->files[\array_rand($this->files) ];
      \preg_match("|sample(\d+)_|", $fileCaller, $matchA);
      $durationA = $matchA[1];
      if ($short != ($durationA <= 7)) {
        $durationB = 1000;
        continue;
      }
      if ($oneway) {
        $durationB = $short ? '8' : '18';
        $fileCallee = "silence/silence{$durationB}.pcm";
        break;
      }

      $fileCallee = $this->files[\array_rand($this->files) ];
      \preg_match("|sample(\d+)_|", $fileCallee, $matchB);
      $durationB = $matchB[1];
    } while (\abs($durationA - $durationB) > 3);

    $this->fileCaller = $fileCaller;
    $this->fileCallee = $fileCallee;

    return $this;
  }

  /**
   * Select libtgvoip dynamic library file to load.
   *
   * @param string $version version name to be used in stats later
   * @param string $path path to the .so file
   *
   * @return self
   */
  public function library(string $version, string $path): self {
    $this->libraryVersion = $version;
    $this->libraryPath = $path;

    return $this;
  }

  private function execBackground(string $cmd) {
    $this->log('> '.$cmd);
    return \proc_open($cmd, [0 => STDIN, 1 => STDOUT, 2 => STDERR], $pipes);
  }

  private function waitBackground($proc) {
    return \proc_close($proc);
  }

  private function execSync(string $cmd) {
    $this->log('> '.$cmd);
    return shell_exec($cmd);
  }

  private function execBridge(string $cmd) {
    return $this->execSync($cmd);
  }

  private function calcStddevMean(array $arr) {
    $cnt = count($arr);
    if (!$cnt) {
      return [];
    }
    if ($cnt == 1) {
      return [0, $arr[0]];
    }
    $cnt = floatval($cnt);

    $sum = 0.0;
    foreach ($arr as $value) {
      $sum += floatval($value);
    }
    $mean = $sum / floatval($cnt);

    $sum2 = 0.0;
    foreach ($arr as $value) {
      $f = floatval($value) - $mean;
      $sum2 += $f * $f;
    }
    $stddev = sqrt($sum2 / ($cnt - 1.0));

    return [$stddev, $mean];
  }

  private function readCsvFile(string $fileName) {
    if (!file_exists($fileName) || !is_readable($fileName)) {
      throw new \Exception("CSV file not available: ".$fileName);
    }

    $header = false;
    $rows = [];
    if (($fp = fopen($fileName, 'r')) !== false) {
      while (($row = fgetcsv($fp)) !== false) {
        if ($header === false) {
          $header = $row;
        } else {
          $rows[] = array_combine($header, $row);
        }
      }
      fclose($fp);
    }
    return $rows;
  }

  public function printCsvScores(string $fileName = '') {
    if ($fileName === '') {
      $fileName = $this->testdir.'out/ratings.csv';
    }
    $rows = $this->readCsvFile($fileName);

    $scoresByVersion = [];
    $scoreTypes = ['ScoreCombined', 'ScorePreprocess', 'ScoreOutput'];
    $counts = [];
    foreach ($rows as $row) {
      foreach ($scoreTypes as $scoreType) {
        $score = floatval($row[$scoreType]);
        $scoresByVersion[$row['LibVersion']][$scoreType][] = $score;
        @$counts[$row['LibVersion']]++;
      }
    }

    foreach ($scoresByVersion as $version => $scoreTypes) {
      echo "Version {$version} ({$counts[$version]} ratings)".PHP_EOL;
      echo str_repeat('=', 45).PHP_EOL;
      foreach ($scoreTypes as $scoreType => $scores) {
        list($stddev, $mean) = $this->calcStddevMean($scores);
        echo str_pad($scoreType.':', 20, ' ')."mean ".round($mean, 3).", stddev: ".round($stddev, 3).PHP_EOL;
      }
      echo PHP_EOL;
    }
  }

  private function getCsvWriteFD() {
    if ($this->csvFD === false) {
      $csvFilePath = $this->testdir.'out/ratings.csv';
      if (!is_file($csvFilePath)) {
        $this->csvFD = fopen($csvFilePath, 'w');
        fputcsv($this->csvFD, [
          'LibVersion',
          'Sample',
          'Network',
          'Distorted',
          'ScorePreprocess',
          'ScoreOutput',
          'ScoreCombined'
        ]);
      } else {
        $this->csvFD = fopen($csvFilePath, 'a');
      }
    }
    return $this->csvFD;
  }

  private function netnsPrefix($as_nobody = false) {
    static $myuser = false;
    $netns = 'sudo ip netns exec client1 ';
    if ($as_nobody) {
      if ($myuser === false) {
        $myuser = trim(shell_exec('whoami'));
      }
      $netns .= "sudo -u {$myuser} ";
    }
    return $netns;
  }

  private function applyNetem($netem) {
    static $first = false;
    $netnsPrefix = $this->netnsPrefix();
    if (!$first) {
      $first = true;
      $this->execBridge($netnsPrefix."tc qdisc del dev {$this->dev} root");
    }
    if ($commands = $netem['commands']) {
      $this->execBridge($netnsPrefix."tc qdisc add dev {$this->dev} root netem ".\implode(' ', $commands));
    }
    if ($extraCmd = $netem['extraCmd']) {
      $extraCmd = implode('&& '.$netnsPrefix, explode('&&', $extraCmd));
      $this->execBridge($netnsPrefix.$extraCmd);
    }
  }

  private function undoNetem($netem) {
    $netnsPrefix = $this->netnsPrefix();
    if ($netem['commands']) {
      $this->execBridge($netnsPrefix."tc qdisc del dev {$this->dev} root");
    }
    if ($extraCmdUndo = $netem['extraCmdUndo']) {
      $this->execBridge($netnsPrefix.$extraCmdUndo);
    }
  }

  private function log($str) {
    static $startTime = false;
    if (!$this->verbose) {
      return;
    }
    if ($startTime === false) {
      $startTime = microtime(true);
    }
    echo '[+'.round(microtime(true) - $startTime, 3).'s] '.$str.PHP_EOL;
  }
}