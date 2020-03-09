import os
import math
import sys
import re
import subprocess

prediction = 0.0


def run_subprocess(command):
    pipe = subprocess.Popen(command, stdout=subprocess.PIPE,
                            stderr=subprocess.STDOUT, shell=True, universal_newlines=True)
    while True:
        line = pipe.stdout.readline().strip()
        if line == '' and pipe.poll() is not None:
            break


def convert_prediction(pred):
    pred = pred / 4.5
    pred = pred * 3.4 + 1.3
    return pred
    # abuse_values = (1.3, 1.7, 2.3, 2.7, 3.3, 3.7, 4.3, 4.7)
    if pred > 3.0:
        if pred > 4.0:
            if pred > 4.5:
                return 4.7
            else:
                return 4.3
        else:
            if pred > 3.5:
                return 3.7
            else:
                return 3.3
    else:
        if pred > 2.0:
            if pred > 2.5:
                return 2.7
            else:
                return 2.3
        else:
            if pred > 1.5:
                return 1.7
            else:
                return 1.3


def run_session(command_run):
    global prediction
    re_prediction = re.compile(r'Prediction : PESQ_MOS = (\d{1}\.\d{1,3})', re.U)
    pipe = subprocess.Popen(command_run, shell=True,
                            stdout=subprocess.PIPE,
                            stderr=subprocess.STDOUT,
                            universal_newlines=True)
    while True:
        line = pipe.stdout.readline().strip()
        if line == '' and pipe.poll() is not None:
            break
        prediction_match = re_prediction.match(line)
        if prediction_match:
            prediction = float(prediction_match.group(1))
            prediction = max(0.0, prediction)
            prediction = min(4.5, prediction)
    print(convert_prediction(prediction))


if __name__ == '__main__':
    import argparse

    parser = argparse.ArgumentParser(description='tgvoiprate runner')
    parser.add_argument('orig', type=str)
    parser.add_argument('degr', type=str)

    args = parser.parse_args()
    ORIGINAL_FILE_PATH = args.orig
    DEGRADED_FILE_PATH = args.degr

    run_session("{0}/../environment/pesq +16000 {1} {2}".format(
      os.path.dirname(__file__),
      ORIGINAL_FILE_PATH,
      DEGRADED_FILE_PATH
    ))
    sys.exit()

    ORIGINAL_FILE_NAME = os.path.basename(ORIGINAL_FILE_PATH)
    DEGRADED_FILE_NAME = os.path.basename(DEGRADED_FILE_PATH)

    ORIGINAL_FILE_DIR = os.path.dirname(ORIGINAL_FILE_PATH)
    ORIGINAL_FILE_DIR = os.path.abspath('.') if ORIGINAL_FILE_DIR == '' else os.path.abspath(ORIGINAL_FILE_DIR)

    DEGRADED_FILE_DIR = os.path.dirname(DEGRADED_FILE_PATH)
    DEGRADED_FILE_DIR = os.path.abspath('.') if DEGRADED_FILE_DIR == '' else os.path.abspath(DEGRADED_FILE_DIR)

    ORIGINAL_FILE_PATH_LOCAL = "/mnt/orig/{0}".format(ORIGINAL_FILE_NAME)
    DEGRADED_FILE_PATH_LOCAL = "/mnt/degr/{0}".format(DEGRADED_FILE_NAME)

    # print(ADDRESS, TAG, KEY, INPUT_FILE_PATH, OUTPUT_FILE_PATH, CONFIG_FILE_PATH, ROLE)

    docker_run_cmd = "docker run --rm -v {0}:/mnt/orig: -v {1}:/mnt/degr: " \
                     "--name tgvoiprate slavikmipt/tgvoip_env:debian ".format(ORIGINAL_FILE_DIR, DEGRADED_FILE_DIR)
    start_script_param = "/bin/bash /root/rate.sh {0} {1}".format(ORIGINAL_FILE_PATH_LOCAL, DEGRADED_FILE_PATH_LOCAL)

    command_run = docker_run_cmd + start_script_param

    run_subprocess("docker pull slavikmipt/tgvoip_env:debian")
    run_subprocess("docker stop tgvoiprate")

    run_session(command_run)
