<?php

require_once('CallTester.php');

class CallRemoteTester extends CallTester {

  public function __construct(string $calleeHost, string $calleePath, string $me, bool $verbose = false, string $dev = 'v-peer1') {
    parent::__construct($me, $verbose, $dev);
    $this->calleeHost = $calleeHost;
    $this->calleePath = $calleePath;
  }

  protected function execSyncCallee(string $cmd) {
    $cmd = 'ssh '.$this->calleeHost.' "cd '.$this->calleePath.' && '.$cmd.'"';
    return parent::execSyncCallee($cmd);
  }

  protected function execBackgroundCallee(string $cmd) {
    $cmd = 'ssh '.$this->calleeHost.' "cd '.$this->calleePath.' && '.$cmd.'"';
    return parent::execBackgroundCallee($cmd);
  }

  protected function copyFileFromCallee(string $path) {
    $cmd = "scp {$this->calleeHost}:{$path} {$path}";
    parent::execSync($cmd);
  }
}