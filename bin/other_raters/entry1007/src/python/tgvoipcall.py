import os
import math
import re
import subprocess

call_state = None
debug_log = ''


def run_subprocess(command):
    pipe = subprocess.Popen(command, stdout=subprocess.PIPE,
                            stderr=subprocess.STDOUT, shell=True, universal_newlines=True)
    while True:
        line = pipe.stdout.readline().strip()
        if line == '' and pipe.poll() is not None:
            break


def run_session(command_run, command_play, command_convert, status_handler=None):
    global debug_log
    global call_state
    re_call_state = re.compile(r'V/tgvoip: Call state changed to (\d)', re.U)
    re_debug_log = re.compile(r'D/tgvoipcall: statistics: (.+)$', re.U | re.I)
    pipe = subprocess.Popen(command_run, shell=True,
                            stdout=subprocess.PIPE,
                            stderr=subprocess.STDOUT,
                            universal_newlines=True)
    while True:
        line = pipe.stdout.readline().strip()
        if line == '' and pipe.poll() is not None:
            break
        call_debug_log_match = re_debug_log.match(line)
        if call_debug_log_match:
            debug_log = str(call_debug_log_match.group(1))
        call_state_match = re_call_state.match(line)
        if call_state_match:
            call_state = int(call_state_match.group(1))
            # print(call_state)
            if call_state == 3:
                run_subprocess(command_play)
                run_subprocess(command_convert)
    print(debug_log)


if __name__ == '__main__':
    import argparse

    parser = argparse.ArgumentParser(description='tgvoipcall runner')
    parser.add_argument('address', type=str)
    parser.add_argument('tag', type=str)
    parser.add_argument('-k', '--key', type=str)
    parser.add_argument('-i', '--input', type=str)
    parser.add_argument('-o', '--output', type=str)
    parser.add_argument('-c', '--config', type=str)
    parser.add_argument('-r', '--role', type=str)
    args = parser.parse_args()
    ADDRESS = args.address
    TAG = args.tag
    KEY = args.key
    INPUT_FILE_PATH = args.input
    OUTPUT_FILE_PATH = args.output
    CONFIG_FILE_PATH = args.config
    ROLE = args.role
    INPUT_FILE_NAME = os.path.basename(INPUT_FILE_PATH)
    OUTPUT_FILE_NAME = os.path.basename(OUTPUT_FILE_PATH)
    CONFIG_FILE_NAME = os.path.basename(CONFIG_FILE_PATH)
    INPUT_FILE_DIR = os.path.dirname(INPUT_FILE_PATH)
    INPUT_FILE_DIR = os.path.abspath('.') if INPUT_FILE_DIR == '' else os.path.abspath(INPUT_FILE_DIR)
    OUTPUT_FILE_DIR = os.path.dirname(OUTPUT_FILE_PATH)
    OUTPUT_FILE_DIR = os.path.abspath('.') if OUTPUT_FILE_DIR == '' else os.path.abspath(OUTPUT_FILE_DIR)
    CONFIG_FILE_DIR = os.path.dirname(CONFIG_FILE_PATH)
    CONFIG_FILE_DIR = os.path.abspath('.') if CONFIG_FILE_DIR == '' else os.path.abspath(CONFIG_FILE_DIR)
    INPUT_FILE_PATH_LOCAL = "/mnt/input/{0}".format(INPUT_FILE_NAME)
    OUTPUT_FILE_PATH_LOCAL = "/mnt/output/{0}".format(OUTPUT_FILE_NAME)
    CONFIG_FILE_PATH_LOCAL = "/mnt/config/{0}".format(CONFIG_FILE_NAME)
    # print(ADDRESS, TAG, KEY, INPUT_FILE_PATH, OUTPUT_FILE_PATH, CONFIG_FILE_PATH, ROLE)

    docker_run_cmd = "docker run --rm -v {0}:/mnt/input: -v {1}:/mnt/output: -v {2}:/mnt/config: " \
                     "--name tgvoipcall{3} slavikmipt/tgvoip_env:debian " \
        .format(INPUT_FILE_DIR, OUTPUT_FILE_DIR, CONFIG_FILE_DIR, ROLE)
    start_script_param = "/bin/bash /root/start.sh {0} {1} {2} {3} {4} {5} {6}".format(
        ADDRESS, TAG, KEY, INPUT_FILE_PATH_LOCAL, OUTPUT_FILE_PATH_LOCAL, CONFIG_FILE_PATH_LOCAL, ROLE)

    command_run = docker_run_cmd + start_script_param

    command_play = "docker exec tgvoipcall{0} ffmpeg -y -re -i \"{1}\" " \
                   "-f s16le -ac 1 -ar 48000 -acodec pcm_s16le /home/callmic.pipe".format(ROLE, INPUT_FILE_PATH_LOCAL)

    command_convert = "docker exec tgvoipcall{0} ffmpeg -y -i /home/callout.wav " \
                      "-af \"silenceremove=start_periods=1:start_duration=1:start_threshold=0:detection=peak, " \
                      "aformat=dblp,areverse," \
                      "silenceremove=start_periods=1:start_duration=1:start_threshold=0:detection=peak," \
                      "aformat=dblp,areverse\" " \
                      "-acodec libopus -b:a 64000 -vbr off " \
                      "-compression_level 0 -frame_duration 60 -ac 1 -ar 48000 -vn {1}" \
        .format(ROLE, OUTPUT_FILE_PATH_LOCAL)

    run_subprocess("docker pull slavikmipt/tgvoip_env:debian")
    run_subprocess("docker stop tgvoipcall{0}".format(ROLE))

    run_session(command_run, command_play, command_convert)
