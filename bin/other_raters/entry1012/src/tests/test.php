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

class test
{
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
     * Name of network interface.
     *
     * @var string
     */
    private $dev;

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
     * @param string $dev   Name of network interface
     */
    public function __construct(string $me, string $dev = 'wlo1')
    {
        if ($me[0] !== '/') {
            $me = \getcwd()."/".$me;
        }
        $me = \dirname($me);
        $this->token = require 'token.php'; // <?php return 'token';
        $this->files = \glob($me.'/../assets/samples/*');
        $this->testdir = \dirname($me.'/../tests-output/test');
        $this->dev = $dev;
    }
    /**
     * Fetch VoIP params from API.
     *
     * @return array
     */
    private function fetchParams(): array
    {
        $random = \bin2hex(\random_bytes(64));
        $result = \json_decode(\file_get_contents("https://api.contest.com/voip{$this->token}/getConnection?call=$random"), true);

        if (!($result['ok'] ?? false)) {
            throw new \Exception("Error while retrieving API result");
        }

        return $result['result'];
    }
    /**
     * Start testing session.
     *
     * @return self
     */
    public function start(): self
    {
        if ($this->testing) {
            throw new \Exception('Currently creating testing session, cannot create another');
        }
        $this->testing = true;
        return $this;
    }
    /**
     * Set rate control for connection.
     *
     * @param string $rate Rate
     *
     * @return self
     */
    public function rateControl($rate = '10kbit'): self
    {
        $this->name .= 'rate,';
        //$this->commands []= "handle 1:0";
        $commands = \implode(' ', $this->commands);
        $this->commands = [];

        $this->extraCmd = "sudo tc qdisc add dev {$this->dev} root handle 1:0 netem $commands &&"
                        ."sudo tc qdisc add dev {$this->dev} parent 1:1 handle 10: tbf rate $rate buffer 1600 limit 3000";
        $this->extraCmdUndo = "sudo tc qdisc del dev {$this->dev} root";
        return $this;
    }
    /**
     * Emulate packet loss.
     *
     * @param float  $loss  Packet loss percentage
     * @param string $burst Probability for loss bursts
     *
     * @return self
     */
    public function loss(float $loss = 30, float $burst = 0): self
    {
        $this->name .= 'loss,';
        $this->commands []= "loss $loss% $burst%";
        return $this;
    }
    /**
     * Emulate packet delay between $A and $B milliseconds.
     *
     * @param int   $A
     * @param int   $B
     *
     * @return self
     */
    public function delay(int $A = 300, int $B = 10): self
    {
        $this->name .= 'delay,';
        $this->commands []= "delay {$A}ms {$B}ms distribution normal";
        return $this;
    }

    /**
     * Emulate packet reordering.
     *
     * @param float $reorder
     * @param float $burst
     *
     * @return self
     */
    public function reordering(float $reorder = 30, float $burst = 25): self
    {
        $this->name .= 'reorder,';
        $this->commands []= "reorder $reorder% $burst%";
        return $this;
    }
    /**
     * Emulate packet duplication.
     *
     * @param float  $duplication  Packet duplication percentage
     * @param string $burst Probability for duplication bursts
     *
     * @return self
     */
    public function duplication(float $duplication = 30, float $burst = 0): self
    {
        $this->name .= 'dup,';
        $this->commands []= "duplicate $duplication% $burst%";
        return $this;
    }
    public function end(): self
    {
        $this->testing = false;
        $result = $this->fetchParams();
        $config = \json_encode($result['config'], JSON_PRETTY_PRINT);
        $key = $result['encryption_key'];
        $endpoint = $result['endpoints'][0];
        $ipPort = "{$endpoint['ip']}:{$endpoint['port']}";
        $callerTag = $endpoint['peer_tags']['caller'];
        $calleeTag = $endpoint['peer_tags']['callee'];

        $name = \trim($this->name, ',');
        list($fileA, $fileB) = $this->chooseCouple();

        \chdir($this->testdir);

        @\unlink("caller$name.log");
        @\unlink("callee$name.log");
        foreach (['ogg', 'png'] as $ext) {
            @\unlink("1orig$name.$ext");
            @\unlink("1mod$name.$ext");
            @\unlink("2orig$name.$ext");
            @\unlink("2mod$name.$ext");
        }

        \file_put_contents("config.json", $config);
        if ($this->commands) {
            $this->exec("sudo tc qdisc add dev {$this->dev} root netem ".\implode(' ', $this->commands));
        }
        if ($this->extraCmd) {
            $this->exec($this->extraCmd);
        }

        //tc qdisc change dev {$this->dev} root netem
        $first = $this->execBackground("../../tgvoipcall $ipPort $callerTag -k $key -i $fileA -o 1mod$name.ogg -c config.json -r caller caller$name.log");
        $this->exec("../../tgvoipcall $ipPort $calleeTag -k $key -i $fileB -o 2mod$name.ogg -c config.json -r callee callee$name.log");

        \proc_close($first);

        \copy($fileA, "2orig$name.ogg");
        \copy($fileB, "1orig$name.ogg");

        $this->exec("sox 1orig$name.ogg -n spectrogram -o 1orig$name.png");
        $this->exec("sox 1mod$name.ogg -n spectrogram -o 1mod$name.png");
        $this->exec("sox 2orig$name.ogg -n spectrogram -o 2orig$name.png");
        $this->exec("sox 2mod$name.ogg -n spectrogram -o 2mod$name.png");

        if ($this->commands) {
            $this->exec("sudo tc qdisc del dev {$this->dev} root");
            $this->commands = [];
        }
        if ($this->extraCmd) {
            $this->exec($this->extraCmdUndo);
        }
        $this->extraCmd = '';
        $this->name = '';
        return $this;
    }

    private function chooseCouple(): array
    {
        do {
            $fileA = $this->files[\array_rand($this->files)];
            $fileB = $this->files[\array_rand($this->files)];
            \preg_match("|sample(\d+)_|", $fileA, $matchA);
            \preg_match("|sample(\d+)_|", $fileB, $matchB);
            $durationA = $matchA[1];
            $durationB = $matchB[1];
        } while (\abs($durationA - $durationB) > 3);
        return [$fileA, $fileB];
    }

    private function execBackground(string $cmd)
    {
        echo($cmd.PHP_EOL);
        return \proc_open($cmd, [0 => STDIN, 1 => STDOUT, 2 => STDERR], $pipes);
    }

    private function exec(string $cmd)
    {
        echo($cmd.PHP_EOL);
        echo(\exec($cmd).PHP_EOL);
    }
}

preg_match("/default via .* dev (\w+)/", shell_exec("ip route"), $matches);

$tester = new Test($argv[0], $argv[1] ?? $matches[1]);
$tester
    ->start()->loss()->end()
    ->start()->delay()->end()
    ->start()->delay()->reordering(80)->end()
    ->start()->duplication(90)->end()
    ->start()->delay()->rateControl()->end()
    ->start()->delay(300, 200)->loss(10)->end();
