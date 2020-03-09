#!/bin/bash
#  -x

SAMPLE_PATH_PCM=$1
PREPROCESSED_PATH_PCM=$2
DISTORTED_PATH_PCM=$3

sample_name=$(basename $SAMPLE_PATH_PCM);
sample_ogg=$(echo $SAMPLE_PATH_PCM | sed "s/\.pcm$/.ogg/")
sample_wav=$(echo $SAMPLE_PATH_PCM | sed "s/\.pcm$/.wav/")
if [ ! -f $sample_ogg ]; then
  ffmpeg -hide_banner -loglevel panic -y -f s16le -ac 1 -ar 48k -i $SAMPLE_PATH_PCM -f opus $sample_ogg
fi

duration=$(echo $sample_name | grep -oP '^sample0*\K(\d+)')
duration=$(($duration + 1))

distorted_ogg=$(echo $DISTORTED_PATH_PCM | sed "s/\.pcm$/.ogg/")
if [ ! -f $distorted_ogg ]; then
  ffmpeg -hide_banner -loglevel panic -y -f s16le -ac 1 -ar 48k -i $DISTORTED_PATH_PCM -t $duration -f opus $distorted_ogg
fi
bin/tgvoiprate $SAMPLE_PATH_PCM $DISTORTED_PATH_PCM > $DISTORTED_PATH_PCM.Rate.Short &
bin/tgvoiprate $SAMPLE_PATH_PCM $PREPROCESSED_PATH_PCM $DISTORTED_PATH_PCM > $DISTORTED_PATH_PCM.Rate.Full &

bin/other_raters/entry997/tgvoiprate $sample_ogg $distorted_ogg > $DISTORTED_PATH_PCM.Rate.997 &
bin/other_raters/entry1010/tgvoiprate $sample_ogg $distorted_ogg > $DISTORTED_PATH_PCM.Rate.1010 & 
bin/other_raters/entry1012/tgvoiprate $sample_ogg $distorted_ogg > $DISTORTED_PATH_PCM.Rate.1012 &
bin/other_raters/entry1002/tgvoiprate $sample_ogg $distorted_ogg > $DISTORTED_PATH_PCM.Rate.1002 & 

if [ -f bin/other_raters/entry1007/src/environment/pesq ]; then
  if [ ! -f $sample_wav ]; then
    ffmpeg -hide_banner -loglevel panic -y -f s16le -ac 1 -ar 48k -i $SAMPLE_PATH_PCM -ar 16000 $sample_wav
  fi
  distorted_wav=$(echo $DISTORTED_PATH_PCM | sed "s/\.pcm$/.wav/")
  if [ ! -f $distorted_wav ]; then
    ffmpeg -hide_banner -loglevel panic -y -f s16le -ac 1 -ar 48k -i $DISTORTED_PATH_PCM -ar 16000 $distorted_wav
  fi

  python bin/other_raters/entry1007/src/python/tgvoiprate.py $sample_wav $distorted_wav > $DISTORTED_PATH_PCM.Rate.1007 &
fi

wait;

short_score=$(tail -1 $DISTORTED_PATH_PCM.Rate.Short | grep -oP '\K(\d{1}(\.\d{1,})?)')
full_scores=$(tail -1 $DISTORTED_PATH_PCM.Rate.Full | grep -oP '\K(\d{1}(\.\d{1,})? \d{1}(\.\d{1,})?)' | sed "s/ /,/")

score997=$(tail -1 $DISTORTED_PATH_PCM.Rate.997 | grep -oP '\K(\d{1}(\.\d{1,})?)')
score1010=$(tail -1 $DISTORTED_PATH_PCM.Rate.1010 | grep -oP '\K(\d{1}(\.\d{1,})?)')
score1012=$(tail -1 $DISTORTED_PATH_PCM.Rate.1012 | grep -oP '\K(\d{1}(\.\d{1,})?)')
score1002=$(tail -1 $DISTORTED_PATH_PCM.Rate.1002 | grep -oP '\K(\d{1}(\.\d{1,})?)')

if [ -f bin/other_raters/entry1007/src/environment/pesq ]; then
  score1007=$(tail -1 $DISTORTED_PATH_PCM.Rate.1007 | grep -oP '\K(\d{1}(\.\d{1,})?)')
else 
  score1007="0"
fi

echo "$short_score,$full_scores,$score997,$score1010,$score1012,$score1002,$score1007"
